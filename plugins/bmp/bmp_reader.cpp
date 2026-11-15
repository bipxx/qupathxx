#include "reader_plugin_profile.hpp"
#include <qupathxx/reader_plugin.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
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
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))fail(QPX_READER_CANCELLED,"BMP operation cancelled");
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
    catch(const std::bad_alloc&){return failure(error,QPX_READER_INTERNAL_ERROR,"BMP plugin allocation failed");}
    catch(const std::filesystem::filesystem_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::runtime_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::exception& e){return failure(error,QPX_READER_INTERNAL_ERROR,e.what());}
    catch(...){return failure(error,QPX_READER_INTERNAL_ERROR,"BMP plugin internal failure");}
}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {
    require(!a || b<=std::numeric_limits<std::uint64_t>::max()/a,"BMP decoded size overflow");
    return a*b;
}
struct Budget {
    std::uint64_t remaining;
    std::unique_ptr<std::uint8_t[]> array(std::uint64_t bytes) {
        // Payload sizes are exact new[] counts; reserve a fixed bookkeeping
        // allowance per owned array in addition to the wrapper/context budget.
        constexpr std::uint64_t bookkeeping=32;
        require(bytes<=remaining && bookkeeping<=remaining-bytes,"BMP decoded data exceeds reader work budget");
        remaining-=bytes+bookkeeping;
        return std::unique_ptr<std::uint8_t[]>(new std::uint8_t[static_cast<std::size_t>(bytes)]);
    }
};
// Independently authored byte parser using Microsoft's published BMP fields.
// No packed C structs, platform image APIs, color transforms or codec calls.
std::uint16_t u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1])<<8));
}
std::uint32_t u32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1])<<8) |
        (static_cast<std::uint32_t>(bytes[2])<<16) |
        (static_cast<std::uint32_t>(bytes[3])<<24);
}
std::int64_t i32(const std::uint8_t* bytes) {
    const auto value=u32(bytes);
    return value<=UINT32_C(2147483647)?static_cast<std::int64_t>(value):
        static_cast<std::int64_t>(value)-INT64_C(4294967296);
}
struct Raster {
    std::int64_t width=0,height=0;
    std::uint32_t channels=0,calibration_flags=0;
    static constexpr std::uint32_t sample_bytes=1;
    double pixel_width_um=0,pixel_height_um=0;
    bool indexed=false;
    std::unique_ptr<std::uint8_t[]> raw,rgba;
};
struct Handle {Raster image;};
Raster decode(const std::uint8_t* input,std::size_t input_size,Budget& budget,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    if(input_size<2 || input[0]!='B' || input[1]!='M')
        fail(QPX_READER_UNSUPPORTED,"BMP signature is not supported");
    valid(input_size>=18,"BMP file or DIB header is truncated");
    if(u32(input+14)!=40)fail(QPX_READER_UNSUPPORTED,"BMP reader requires exactly a 40-byte BITMAPINFOHEADER");
    valid(input_size>=54,"BMP BITMAPINFOHEADER is truncated");
    valid(u32(input+2)==input_size,"BMP file size does not match the exact encoded input");
    valid(!u16(input+6) && !u16(input+8),"BMP reserved file-header fields must be zero");
    const auto width=i32(input+18),signed_height=i32(input+22);
    valid(width>0 && signed_height!=0,"BMP width must be positive and height must be nonzero");
    valid(u16(input+26)==1,"BMP color planes must equal one");
    const auto bits=u16(input+28);
    if(bits!=1 && bits!=4 && bits!=8 && bits!=24 && bits!=32)
        fail(QPX_READER_UNSUPPORTED,"BMP bit depth is not supported");
    if(u32(input+30)!=0)fail(QPX_READER_UNSUPPORTED,"BMP compression or color masks are not supported");
    const auto x_ppm=i32(input+38),y_ppm=i32(input+42);
    valid(x_ppm>=0 && y_ppm>=0,"BMP pixels-per-meter values must be nonnegative");
    const bool indexed=bits<=8;
    const auto used=u32(input+46),important=u32(input+50);
    std::uint32_t palette_count=0;
    if(indexed) {
        const auto maximum=std::uint32_t(1)<<bits;
        palette_count=used?used:maximum;
        valid(palette_count<=maximum,"BMP palette count exceeds the indexed bit depth");
        valid(!important || important<=palette_count,"BMP important-color count exceeds the palette");
    } else if(used)fail(QPX_READER_UNSUPPORTED,"BMP truecolor optimization palettes are not supported");
    const std::uint64_t offset=UINT64_C(54)+4*palette_count;
    valid(u32(input+10)==offset,"BMP pixel offset contains a gap, overlap or unsupported data");
    valid(offset<=input_size,"BMP color palette is truncated");
    const auto height=static_cast<std::uint64_t>(signed_height<0?-signed_height:signed_height);
    const auto columns=static_cast<std::uint64_t>(width);
    const auto pixels=multiply(columns,height);
    const auto channels=indexed?1u:3u;
    const auto raw_bytes=multiply(pixels,channels),rgba_bytes=multiply(pixels,4);
    const auto row_bits=multiply(columns,bits);
    require(row_bits<=std::numeric_limits<std::uint64_t>::max()-31,"BMP row stride overflow");
    const auto row_bytes=multiply((row_bits+31)/32,4);
    const auto raster_bytes=multiply(row_bytes,height);
    require(raw_bytes<=work_limit && rgba_bytes<=work_limit && raster_bytes<=work_limit,
        "BMP decoded data exceeds reader work budget");
    const auto declared_image_size=u32(input+34);
    valid(!declared_image_size || declared_image_size==raster_bytes,"BMP declared raster size is inconsistent");
    valid(raster_bytes==input_size-offset,"BMP raster is truncated or has trailing image data");
    for(std::uint32_t index=0;index<palette_count;++index)
        valid(input[54+4*index+3]==0,"BMP palette reserved bytes must be zero");
    Raster result;
    result.width=width;result.height=static_cast<std::int64_t>(height);
    result.channels=channels;result.indexed=indexed;
    if(x_ppm) {result.calibration_flags|=QPX_READER_CALIBRATION_X;result.pixel_width_um=1000000.0/static_cast<double>(x_ppm);}
    if(y_ppm) {result.calibration_flags|=QPX_READER_CALIBRATION_Y;result.pixel_height_um=1000000.0/static_cast<double>(y_ppm);}
    result.raw=budget.array(raw_bytes);result.rgba=budget.array(rgba_bytes);
    for(std::uint64_t y=0;y<height;++y) {
        checkpoint(cancel);
        const auto source_y=signed_height<0?y:height-1-y;
        const auto* row=input+offset+source_y*row_bytes;
        for(std::uint64_t x=0;x<columns;++x) {
            const auto pixel=y*columns+x;
            if((pixel&4095)==0)checkpoint(cancel);
            auto* raw=result.raw.get()+pixel*channels;
            auto* rgba=result.rgba.get()+pixel*4;
            if(indexed) {
                std::uint8_t index=0;
                if(bits==1)index=static_cast<std::uint8_t>((row[x/8]>>(7-x%8))&1);
                else if(bits==4)index=static_cast<std::uint8_t>((row[x/2]>>(x%2?0:4))&15);
                else index=row[x];
                valid(index<palette_count,"BMP raster contains an out-of-range palette index");
                raw[0]=index;
                const auto* color=input+54+4*index;
                rgba[0]=color[2];rgba[1]=color[1];rgba[2]=color[0];
            } else {
                const auto* color=row+x*(bits/8);
                raw[0]=rgba[0]=color[2];raw[1]=rgba[1]=color[1];raw[2]=rgba[2]=color[0];
            }
            // BI_RGB 32-bit's fourth byte is unused X, never an alpha channel.
            rgba[3]=255;
        }
    }
    checkpoint(cancel);return result;
}
const Raster& image(void* value) {require(value!=nullptr,"BMP reader handle is null");return static_cast<Handle*>(value)->image;}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"BMP image path is invalid");
    std::string path(value.data,static_cast<std::size_t>(value.size));
    require(path.find('\0')==std::string::npos,"BMP image path contains NUL");
    require(std::filesystem::u8path(path).is_absolute(),"BMP image path must be absolute");
    return path;
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output && output->struct_size>=sizeof(*output),"BMP reader info structure is too small");
        output->abi_version=QPX_READER_ABI_V1;output->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW;
        bool complete=text(output->provider_id,"org.qupathxx.bmp");
        complete=text(output->display_name,"BMP") && complete;
        complete=text(output->version,"1.0.0") && complete;
        complete=text(output->contract,"bmp-infoheader-uint8-samples/1") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"BMP descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"BMP reader output handle is null");*output=nullptr;
        require(request && request->struct_size>=sizeof(*request),"BMP open structure is too small");
        require(request->reserved==0,"BMP open reserved field must be zero");
        require(request->options.size==0,"BMP options must be empty");
        require(request->work_bytes && request->work_bytes<=work_limit,"BMP work budget must be between 1 and 256 MiB");
        require(request->path.data && request->path.size && request->path.size<=32768,"BMP image path is invalid");
        // Reserve the path/wrapper allowance before constructing owned paths.
        const auto overhead=UINT64_C(4096)+8*request->path.size+sizeof(Handle);
        require(request->work_bytes>overhead,"BMP reader work budget is too small for opening overhead");
        checkpoint(request->cancel);
        const auto path=path_string(request->path);const auto native=std::filesystem::u8path(path);
        const auto encoded=std::filesystem::file_size(native);
        require(encoded<=request->work_bytes,"BMP encoded input exceeds reader work budget");
        // Bounded UTF-8/UTF-16 path copies, owner and unbuffered FILE state;
        // input/output array payloads plus bookkeeping are charged below.
        Budget budget{request->work_bytes-overhead};
        auto bytes=budget.array(encoded);
        std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(native.c_str(),L"rb"),std::fclose);
        if(!file)fail(QPX_READER_IO_ERROR,"BMP image cannot be opened");
        if(std::setvbuf(file.get(),nullptr,_IONBF,0))fail(QPX_READER_IO_ERROR,"BMP unbuffered input could not be configured");
        for(std::uint64_t position=0;position<encoded;) {
            checkpoint(request->cancel);
            const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(65536,encoded-position));
            valid(std::fread(bytes.get()+position,1,count,file.get())==count,"BMP input is truncated or changed");position+=count;
        }
        valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"BMP input changed during reading");
        auto result=std::make_unique<Handle>();
        result->image=decode(bytes.get(),static_cast<std::size_t>(encoded),budget,request->cancel);
        *output=result.release();return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"BMP metadata structure is too small");checkpoint(cancel);
        output->level_count=1;output->width=source.width;output->height=source.height;
        output->has_raw=1;output->channels=source.channels;output->sample_type=QPX_READER_UINT8;
        output->z_planes=1;output->timepoints=1;output->series=0;output->series_count=1;
        output->calibration_flags=source.calibration_flags;output->pixel_width_um=source.pixel_width_um;output->pixel_height_um=source.pixel_height_um;output->z_spacing_um=0;output->time_interval_seconds=0;
        bool complete=text(output->vendor,"BMP");complete=text(output->dimension_order,"XYCZT") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"BMP metadata text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"BMP level structure is too small");require(index==0,"BMP reader has only level zero");
        output->reserved=0;output->width=source.width;output->height=source.height;output->downsample=1;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value,std::uint32_t channel,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output!=nullptr,"BMP channel-name output is null");require(channel<source.channels,"BMP channel index is out of range");
        const char* name="Palette index";
        if(!source.indexed) {const char* names[]={"Red","Green","Blue"};name=names[channel];}
        return text(*output,name)?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"BMP channel-name buffer is too small");
    });
}
struct Region {std::int64_t width,height;std::uint64_t pixels;};
Region region(const qpx_reader_region_v1* request,std::uint64_t bytes_per_pixel) {
    require(request && request->struct_size>=sizeof(*request),"BMP region structure is too small");
    require(request->level==0 && !request->z && !request->t,"BMP reader has only level zero, Z=0 and T=0");
    const auto width=request->width,height=request->height;
    require(width>0 && height>0 && static_cast<std::uint64_t>(width)<=work_limit/bytes_per_pixel &&
        static_cast<std::uint64_t>(height)<=work_limit/(bytes_per_pixel*static_cast<std::uint64_t>(width)),"BMP output must have positive dimensions and fit 256 MiB");
    const auto limit=std::numeric_limits<std::int64_t>::max();
    require(request->x<=limit-width && request->y<=limit-height,"BMP region coordinate overflow");
    return {width,height,static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)};
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"BMP pixel structure is too small");output->written=0;output->validity_written=0;
        const auto area=region(request,4);require(!request->first_channel && !request->channels,"BMP RGBA request has quantitative coordinates");
        const auto row_stride=static_cast<std::uint64_t>(area.width)*4,count=area.pixels*4;
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==4 && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==4 && output->channel_stride==1,"BMP RGBA output must use packed UInt8x4 layout");
        require(output->data && output->capacity>=count && !output->validity && !output->validity_capacity,"BMP RGBA output buffer is invalid");
        std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row)for(std::int64_t col=0;col<area.width;++col) {
            const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            std::memcpy(output->data+(static_cast<std::uint64_t>(row)*area.width+col)*4,source.rgba.get()+(static_cast<std::uint64_t>(y)*source.width+x)*4,4);
        }
        output->written=count;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"BMP raw pixel structure is too small");output->written=0;output->validity_written=0;
        require(request && request->struct_size>=sizeof(*request),"BMP raw region structure is too small");
        require(request->channels>0 && request->first_channel<source.channels && request->channels<=source.channels-request->first_channel,"BMP raw channel range is invalid");
        const auto pixel_stride=static_cast<std::uint64_t>(request->channels)*source.sample_bytes;
        const auto area=region(request,pixel_stride);const auto count=area.pixels*pixel_stride,row_stride=static_cast<std::uint64_t>(area.width)*pixel_stride;
        require(area.pixels<=work_limit-count,"BMP raw output and validity exceed 256 MiB");
        const auto sample_type=QPX_READER_UINT8;
        require(output->sample_type==sample_type && output->width==area.width && output->height==area.height && output->channels==request->channels && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==pixel_stride && output->channel_stride==source.sample_bytes,"BMP raw output must use packed native-endian HWC layout");
        require(output->data && output->capacity>=count && output->validity && output->validity_capacity>=area.pixels,"BMP raw output or validity buffer is invalid");
        checkpoint(cancel);std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));std::fill_n(output->validity,static_cast<std::size_t>(area.pixels),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row) {
            checkpoint(cancel);
            for(std::int64_t col=0;col<area.width;++col) {
                if((col&4095)==0)checkpoint(cancel);
                const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
                const auto to=static_cast<std::uint64_t>(row)*area.width+col;
                const auto from=(static_cast<std::uint64_t>(y)*source.width+x)*source.channels+request->first_channel;
                std::memcpy(output->data+to*pixel_stride,source.raw.get()+from*source.sample_bytes,static_cast<std::size_t>(pixel_stride));output->validity[to]=1;
            }
        }
        checkpoint(cancel);output->written=count;output->validity_written=area.pixels;return QPX_READER_OK;
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi,qpx_reader_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"BMP reader does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"BMP reader API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,describe,open,destroy,metadata,level,channel_name,read_rgba,read_raw};return QPX_READER_OK;
    });
}
