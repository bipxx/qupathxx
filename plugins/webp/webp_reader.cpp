#include "reader_plugin_profile.hpp"
#include "webp_memory_backend.h"
#include <qupathxx/reader_plugin.h>
#include <webp/decode.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

namespace {
constexpr std::uint64_t work_limit=UINT64_C(256)*1024*1024;
struct Failure {qpx_reader_status_v1 status;const char* message;};
[[noreturn]] void fail(qpx_reader_status_v1 status,const char* message) {throw Failure{status,message};}
void require(bool condition,const char* message) {if(!condition)fail(QPX_READER_INVALID_REQUEST,message);}
void valid(bool condition,const char* message) {if(!condition)fail(QPX_READER_IO_ERROR,message);}
void checkpoint(qpx_reader_cancel_v1 cancel) {
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))fail(QPX_READER_CANCELLED,"WebP operation cancelled");
}
bool text(qpx_reader_text_v1& output,const char* value) noexcept {
    const auto length=std::strlen(value);output.size=length;
    if(output.capacity<length || (length && !output.data))return false;
    if(length)std::memcpy(output.data,value,length);
    return true;
}
qpx_reader_status_v1 failure(qpx_reader_text_v1* error,qpx_reader_status_v1 status,const char* message) noexcept {
    if(error && !text(*error,message))return QPX_READER_BUFFER_TOO_SMALL;
    return status;
}
template<class Function>qpx_reader_status_v1 boundary(qpx_reader_text_v1* error,Function&& function) noexcept {
    if(error)error->size=0;
    try{return function();}
    catch(const Failure& e){return failure(error,e.status,e.message);}
    catch(const std::invalid_argument& e){return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::out_of_range& e){return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::bad_alloc&){return failure(error,QPX_READER_INTERNAL_ERROR,"WebP plugin allocation failed");}
    catch(const std::filesystem::filesystem_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::runtime_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::exception& e){return failure(error,QPX_READER_INTERNAL_ERROR,e.what());}
    catch(...){return failure(error,QPX_READER_INTERNAL_ERROR,"WebP plugin internal failure");}
}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {
    require(!a || b<=std::numeric_limits<std::uint64_t>::max()/a,"WebP decoded size overflow");
    return a*b;
}
struct Budget {
    std::uint64_t remaining;
    std::unique_ptr<std::uint8_t[]> array(std::uint64_t bytes) {
        // Payload sizes are exact new[] counts; reserve a fixed bookkeeping
        // allowance per owned array in addition to the wrapper/context budget.
        constexpr std::uint64_t bookkeeping=32;
        require(bytes<=remaining && bookkeeping<=remaining-bytes,"WebP decoded data exceeds reader work budget");
        remaining-=bytes+bookkeeping;
        return std::unique_ptr<std::uint8_t[]>(new std::uint8_t[static_cast<std::size_t>(bytes)]);
    }
};
std::uint32_t u32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1])<<8) |
        (static_cast<std::uint32_t>(bytes[2])<<16) |
        (static_cast<std::uint32_t>(bytes[3])<<24);
}
struct Raster {
    std::int64_t width=0,height=0;
    static constexpr std::uint32_t channels=4,sample_bytes=1;
    unsigned orientation=1;
    // Raw and display samples share the same decoded straight RGBA layout.
    std::unique_ptr<std::uint8_t[]> raw;

    const std::uint8_t* pixel(std::int64_t x,std::int64_t y) const {
        const auto encoded_width=orientation>=5?height:width;
        const auto encoded_height=orientation>=5?width:height;
        if(orientation>=5)std::swap(x,y);
        if(orientation==2 || orientation==3 || orientation==7 || orientation==8)x=encoded_width-1-x;
        if(orientation==3 || orientation==4 || orientation==6 || orientation==7)y=encoded_height-1-y;
        return raw.get()+(static_cast<std::uint64_t>(y)*encoded_width+x)*channels;
    }
};
struct Handle {Raster image;};
// Generic upstream DSP initialization has static state without its optional
// threading feature. Serialize all codec work; no host pointer is stored here.
std::mutex codec_gate;
struct Chunk {
    const std::uint8_t* data=nullptr;
    std::size_t size=0;
};
unsigned exif_orientation(Chunk exif) {
    if(exif.size>65536)fail(QPX_READER_UNSUPPORTED,"WebP EXIF exceeds the orientation profile limit");
    if(exif.size>=6 && !std::memcmp(exif.data,"Exif\0\0",6)) {
        exif.data+=6;exif.size-=6;
    }
    valid(exif.size>=8,"WebP EXIF TIFF header is truncated");
    const auto* data=exif.data;
    const bool big=!std::memcmp(data,"MM",2);
    valid(big || !std::memcmp(data,"II",2),"WebP EXIF byte order is malformed");
    const auto short_at=[big,data](std::size_t offset) {
        const auto* value=data+offset;
        return big?(unsigned(value[0])<<8)|value[1]:(unsigned(value[1])<<8)|value[0];
    };
    const auto long_at=[&](std::size_t offset) {
        return big?(short_at(offset)<<16)|short_at(offset+2):u32(data+offset);
    };
    valid(short_at(2)==42,"WebP EXIF requires a classic TIFF header");
    const auto ifd=static_cast<std::size_t>(long_at(4));
    valid(ifd>=8 && !(ifd&1) && ifd<=exif.size && exif.size-ifd>=6,
        "WebP EXIF primary IFD points outside the chunk");
    const auto count=short_at(ifd);
    valid(count<=(exif.size-ifd-6)/12,"WebP EXIF primary IFD entries are truncated");
    unsigned orientation=0;
    for(unsigned index=0;index<count;++index) {
        const auto offset=ifd+2+12*index;
        if(short_at(offset)!=274)fail(QPX_READER_UNSUPPORTED,"WebP EXIF supports only the primary orientation tag");
        valid(!orientation && short_at(offset+2)==3 && long_at(offset+4)==1,
            "WebP EXIF orientation is duplicate or malformed");
        orientation=short_at(offset+8);
        valid(orientation>=1 && orientation<=8,"WebP EXIF orientation is outside 1 through 8");
    }
    if(!orientation || long_at(ifd+2+12*count))
        fail(QPX_READER_UNSUPPORTED,"WebP EXIF requires one primary orientation and no secondary IFD");
    return orientation;
}
std::uint32_t u24(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1])<<8) |
        (static_cast<std::uint32_t>(bytes[2])<<16);
}
Raster decode(const std::uint8_t* input,std::size_t input_size,Budget& budget,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    if(input_size<4 || std::memcmp(input,"RIFF",4))
        fail(QPX_READER_UNSUPPORTED,"WebP RIFF signature is not supported");
    valid(input_size>=12,"WebP RIFF header is truncated");
    if(std::memcmp(input+8,"WEBP",4))fail(QPX_READER_UNSUPPORTED,"WebP form signature is not supported");
    valid(static_cast<std::uint64_t>(u32(input+4))+8==input_size,"WebP RIFF size does not match the exact encoded input");
    valid(input_size>=20,"WebP image chunk is missing or truncated");
    constexpr unsigned alpha_flag=0x10,exif_flag=0x08;
    Chunk bitstream,alpha,exif;
    bool lossless=false;
    unsigned features=0;
    std::uint32_t canvas_width=0,canvas_height=0;
    for(std::size_t position=12;position<input_size;) {
        checkpoint(cancel);
        valid(input_size-position>=8,"WebP chunk header is truncated");
        const auto* chunk=input+position;
        const auto length=static_cast<std::size_t>(u32(chunk+4));
        const auto remaining=input_size-position-8;
        valid(length<=remaining && (length&1)<=remaining-length,"WebP chunk payload or padding is truncated");
        if(length&1)valid(chunk[8+length]==0,"WebP odd chunk padding must be zero");
        const Chunk body{chunk+8,length};
        if(!std::memcmp(chunk,"VP8X",4)) {
            valid(position==12 && length==10,"WebP extended header has invalid order or size");
            valid(!(body.data[0]&0xc1) && u24(body.data+1)==0,"WebP extended header has nonzero reserved bits");
            features=body.data[0];
            if(!features || (features&~(alpha_flag|exif_flag)))
                fail(QPX_READER_UNSUPPORTED,"WebP extended profile supports only alpha and EXIF orientation");
            canvas_width=u24(body.data+4)+1;
            canvas_height=u24(body.data+7)+1;
        } else if(!std::memcmp(chunk,"ALPH",4)) {
            if(!canvas_width)fail(QPX_READER_UNSUPPORTED,"WebP alpha chunk requires the extended alpha profile");
            valid(features&alpha_flag && !alpha.data && !bitstream.data && length>1,"WebP alpha chunk has invalid flags, order or size");
            const auto flags=body.data[0];
            valid(!(flags&0xc0) && (flags&3)<=1 && ((flags>>4)&3)<=1,"WebP alpha header is malformed");
            alpha=body;
        } else if(!std::memcmp(chunk,"EXIF",4)) {
            if(!canvas_width)fail(QPX_READER_UNSUPPORTED,"WebP EXIF chunk requires the extended orientation profile");
            valid(features&exif_flag && !exif.data,"WebP EXIF chunk has inconsistent flags or is duplicated");
            exif=body;
        } else {
            const auto vp8l=!std::memcmp(chunk,"VP8L",4);
            const auto vp8=!std::memcmp(chunk,"VP8 ",4);
            if((!vp8l && !vp8) || (vp8l && canvas_width && !(features&exif_flag)))
                fail(QPX_READER_UNSUPPORTED,"WebP image chunk or extended profile is unsupported");
            valid(!bitstream.data,"WebP contains more than one image chunk");
            bitstream=body;
            lossless=vp8l;
        }
        position+=8+length+(length&1);
    }
    valid(bitstream.data!=nullptr,"WebP image frame is missing");
    valid(bool(exif.data)==bool(features&exif_flag),"WebP extended image is missing its EXIF chunk");
    Raster result;
    if(exif.data)result.orientation=exif_orientation(exif);
    if(lossless) {
        valid(!alpha.data,"WebP lossless frame cannot use a separate alpha chunk");
        valid(bitstream.size>=5,"WebP lossless frame header is truncated");
        valid(bitstream.data[0]==0x2f,"WebP lossless frame signature is invalid");
        const auto frame=u32(bitstream.data+1);
        if(frame>>29)fail(QPX_READER_UNSUPPORTED,"WebP lossless bitstream version is not supported");
        result.width=1+(frame&0x3fff);result.height=1+((frame>>14)&0x3fff);
        // The VP8L alpha_is_used bit is a hint, never a sample/validity decision.
    } else {
        valid(bool(alpha.data)==bool(features&alpha_flag),"WebP extended lossy image is missing its leading alpha chunk");
        // Feature probing reads the VP8 frame header without allocating. Probe
        // the frame alone so the dimensions are independent of the RIFF canvas.
        WebPBitstreamFeatures frame{};
        valid(WebPGetFeatures(bitstream.data,bitstream.size,&frame)==VP8_STATUS_OK && frame.format==1,
            "WebP lossy frame header is malformed or truncated");
        result.width=frame.width;
        result.height=frame.height;
        if(alpha.data && !(alpha.data[0]&3))
            valid(alpha.size-1==static_cast<std::uint64_t>(result.width)*result.height,"WebP raw alpha extent differs from the frame");
    }
    if(canvas_width)valid(result.width==canvas_width && result.height==canvas_height,"WebP canvas does not match the frame");
    const auto bytes=multiply(multiply(static_cast<std::uint64_t>(result.width),static_cast<std::uint64_t>(result.height)),4);
    require(bytes<=work_limit,"WebP decoded data exceeds reader work budget");
    result.raw=budget.array(bytes);
    checkpoint(cancel);
    qpx_reader_status_v1 status;
    {
        std::lock_guard<std::mutex> lock(codec_gate);
        checkpoint(cancel);
        status=qpx_webp_decode_rgba(input,input_size,result.raw.get(),static_cast<std::size_t>(bytes),
            static_cast<int>(result.width),static_cast<int>(result.height),budget.remaining,cancel);
    }
    if(status==QPX_READER_CANCELLED)fail(status,"WebP operation cancelled");
    if(status==QPX_READER_INVALID_REQUEST)fail(status,"WebP decoded data exceeds reader work budget");
    if(status==QPX_READER_INTERNAL_ERROR)fail(status,"WebP decoder allocation or internal failure");
    if(status!=QPX_READER_OK)fail(QPX_READER_IO_ERROR,"WebP bitstream is malformed or truncated");
    if(result.orientation>=5)std::swap(result.width,result.height);
    checkpoint(cancel);return result;
}
const Raster& image(void* value) {require(value!=nullptr,"WebP reader handle is null");return static_cast<Handle*>(value)->image;}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"WebP image path is invalid");
    std::string path(value.data,static_cast<std::size_t>(value.size));
    require(path.find('\0')==std::string::npos,"WebP image path contains NUL");
    require(std::filesystem::u8path(path).is_absolute(),"WebP image path must be absolute");
    return path;
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output && output->struct_size>=sizeof(*output),"WebP reader info structure is too small");
        output->abi_version=QPX_READER_ABI_V1;output->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW;
        bool complete=text(output->provider_id,"org.qupathxx.webp");
        complete=text(output->display_name,"WebP") && complete;
        complete=text(output->version,"1.2.0") && complete;
        complete=text(output->contract,"webp-static-uint8-samples/3") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"WebP descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"WebP reader output handle is null");*output=nullptr;
        require(request && request->struct_size>=sizeof(*request),"WebP open structure is too small");
        require(request->reserved==0,"WebP open reserved field must be zero");
        require(request->options.size==0,"WebP options must be empty");
        require(request->work_bytes && request->work_bytes<=work_limit,"WebP work budget must be between 1 and 256 MiB");
        require(request->path.data && request->path.size && request->path.size<=32768,"WebP image path is invalid");
        // Reserve the path/wrapper allowance before constructing owned paths.
        const auto overhead=UINT64_C(4096)+8*request->path.size+sizeof(Handle);
        require(request->work_bytes>overhead,"WebP reader work budget is too small for opening overhead");
        checkpoint(request->cancel);
        const auto path=path_string(request->path);const auto native=std::filesystem::u8path(path);
        const auto encoded=std::filesystem::file_size(native);
        require(encoded<=request->work_bytes,"WebP encoded input exceeds reader work budget");
        // Bounded UTF-8/UTF-16 path copies, owner and unbuffered FILE state;
        // input/output array payloads plus bookkeeping are charged below.
        Budget budget{request->work_bytes-overhead};
        auto bytes=budget.array(encoded);
        std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(native.c_str(),L"rb"),std::fclose);
        if(!file)fail(QPX_READER_IO_ERROR,"WebP image cannot be opened");
        if(std::setvbuf(file.get(),nullptr,_IONBF,0))fail(QPX_READER_IO_ERROR,"WebP unbuffered input could not be configured");
        for(std::uint64_t position=0;position<encoded;) {
            checkpoint(request->cancel);
            const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(65536,encoded-position));
            valid(std::fread(bytes.get()+position,1,count,file.get())==count,"WebP input is truncated or changed");position+=count;
        }
        valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"WebP input changed during reading");
        auto result=std::make_unique<Handle>();
        result->image=decode(bytes.get(),static_cast<std::size_t>(encoded),budget,request->cancel);
        *output=result.release();return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"WebP metadata structure is too small");checkpoint(cancel);
        output->level_count=1;output->width=source.width;output->height=source.height;
        output->has_raw=1;output->channels=source.channels;output->sample_type=QPX_READER_UINT8;
        output->z_planes=1;output->timepoints=1;output->series=0;output->series_count=1;
        output->calibration_flags=0;output->pixel_width_um=0;output->pixel_height_um=0;output->z_spacing_um=0;output->time_interval_seconds=0;
        bool complete=text(output->vendor,"WebP");complete=text(output->dimension_order,"XYCZT") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"WebP metadata text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"WebP level structure is too small");require(index==0,"WebP reader has only level zero");
        output->reserved=0;output->width=source.width;output->height=source.height;output->downsample=1;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value,std::uint32_t channel,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output!=nullptr,"WebP channel-name output is null");require(channel<source.channels,"WebP channel index is out of range");
        const char* names[]={"Red","Green","Blue","Alpha"};
        const char* name=names[channel];
        return text(*output,name)?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"WebP channel-name buffer is too small");
    });
}
struct Region {std::int64_t width,height;std::uint64_t pixels;};
Region region(const qpx_reader_region_v1* request,std::uint64_t bytes_per_pixel) {
    require(request && request->struct_size>=sizeof(*request),"WebP region structure is too small");
    require(request->level==0 && !request->z && !request->t,"WebP reader has only level zero, Z=0 and T=0");
    const auto width=request->width,height=request->height;
    require(width>0 && height>0 && static_cast<std::uint64_t>(width)<=work_limit/bytes_per_pixel &&
        static_cast<std::uint64_t>(height)<=work_limit/(bytes_per_pixel*static_cast<std::uint64_t>(width)),"WebP output must have positive dimensions and fit 256 MiB");
    const auto limit=std::numeric_limits<std::int64_t>::max();
    require(request->x<=limit-width && request->y<=limit-height,"WebP region coordinate overflow");
    return {width,height,static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)};
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"WebP pixel structure is too small");output->written=0;output->validity_written=0;
        const auto area=region(request,4);require(!request->first_channel && !request->channels,"WebP RGBA request has quantitative coordinates");
        const auto row_stride=static_cast<std::uint64_t>(area.width)*4,count=area.pixels*4;
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==4 && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==4 && output->channel_stride==1,"WebP RGBA output must use packed UInt8x4 layout");
        require(output->data && output->capacity>=count && !output->validity && !output->validity_capacity,"WebP RGBA output buffer is invalid");
        std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row)for(std::int64_t col=0;col<area.width;++col) {
            const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            std::memcpy(output->data+(static_cast<std::uint64_t>(row)*area.width+col)*4,source.pixel(x,y),4);
        }
        output->written=count;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"WebP raw pixel structure is too small");output->written=0;output->validity_written=0;
        require(request && request->struct_size>=sizeof(*request),"WebP raw region structure is too small");
        require(request->channels>0 && request->first_channel<source.channels && request->channels<=source.channels-request->first_channel,"WebP raw channel range is invalid");
        const auto pixel_stride=static_cast<std::uint64_t>(request->channels)*source.sample_bytes;
        const auto area=region(request,pixel_stride);const auto count=area.pixels*pixel_stride,row_stride=static_cast<std::uint64_t>(area.width)*pixel_stride;
        require(area.pixels<=work_limit-count,"WebP raw output and validity exceed 256 MiB");
        const auto sample_type=QPX_READER_UINT8;
        require(output->sample_type==sample_type && output->width==area.width && output->height==area.height && output->channels==request->channels && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==pixel_stride && output->channel_stride==source.sample_bytes,"WebP raw output must use packed native-endian HWC layout");
        require(output->data && output->capacity>=count && output->validity && output->validity_capacity>=area.pixels,"WebP raw output or validity buffer is invalid");
        checkpoint(cancel);std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));std::fill_n(output->validity,static_cast<std::size_t>(area.pixels),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row) {
            checkpoint(cancel);
            for(std::int64_t col=0;col<area.width;++col) {
                if((col&4095)==0)checkpoint(cancel);
                const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
                const auto to=static_cast<std::uint64_t>(row)*area.width+col;
                std::memcpy(output->data+to*pixel_stride,source.pixel(x,y)+request->first_channel,static_cast<std::size_t>(pixel_stride));output->validity[to]=1;
            }
        }
        checkpoint(cancel);output->written=count;output->validity_written=area.pixels;return QPX_READER_OK;
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi,qpx_reader_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"WebP reader does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"WebP reader API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,describe,open,destroy,metadata,level,channel_name,read_rgba,read_raw};return QPX_READER_OK;
    });
}
