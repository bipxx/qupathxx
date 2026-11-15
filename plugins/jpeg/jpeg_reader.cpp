#include "reader_plugin_profile.hpp"
#include <qupathxx/reader_plugin.h>
#include "jpeg_decoder.h"
#include <algorithm>
#include <cstdio>
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
using Image=std::unique_ptr<qpx_jpeg_image,decltype(&qpx_jpeg_destroy)>;
struct Handle {Image image{nullptr,qpx_jpeg_destroy};};

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
    catch(const std::bad_alloc&){return failure(error,QPX_READER_INTERNAL_ERROR,"JPEG plugin allocation failed");}
    catch(const std::runtime_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::exception& e){return failure(error,QPX_READER_INTERNAL_ERROR,e.what());}
    catch(...){return failure(error,QPX_READER_INTERNAL_ERROR,"JPEG plugin internal failure");}
}
void require(bool condition,const char* message) {
    if(!condition)throw Failure{QPX_READER_INVALID_REQUEST,message};
}
void checkpoint(qpx_reader_cancel_v1 cancel) {
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))throw Failure{QPX_READER_CANCELLED,"JPEG operation cancelled"};
}
const qpx_jpeg_image& image(void* value) {
    require(value!=nullptr,"JPEG reader handle is null");
    return *static_cast<Handle*>(value)->image;
}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"JPEG image path is invalid");
    std::string path(value.data,static_cast<std::size_t>(value.size));
    require(path.find('\0')==std::string::npos,"JPEG image path contains NUL");
    require(std::filesystem::u8path(path).is_absolute(),"JPEG image path must be absolute");
    return path;
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output && output->struct_size>=sizeof(*output),"JPEG reader info structure is too small");
        output->abi_version=QPX_READER_ABI_V1;output->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW;
        bool complete=text(output->provider_id,"org.qupathxx.jpeg");
        complete=text(output->display_name,"JPEG") && complete;
        complete=text(output->version,"1.0.0") && complete;
        complete=text(output->contract,"jpeg8-oriented-decoded-samples/1") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"JPEG descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"JPEG reader output handle is null");*output=nullptr;
        require(request && request->struct_size>=sizeof(*request),"JPEG open structure is too small");
        require(request->reserved==0,"JPEG open reserved field must be zero");
        require(request->options.size==0,"JPEG reader accepts no options");
        require(request->work_bytes && request->work_bytes<=work_limit,"JPEG work budget must be between 1 and 256 MiB");
        checkpoint(request->cancel);
        const auto path=path_string(request->path);const auto native=std::filesystem::u8path(path);
        const auto encoded=std::filesystem::file_size(native);
        require(encoded<=request->work_bytes,"JPEG encoded input exceeds reader work budget");
        // Account conservatively for bounded UTF-8/UTF-16 path copies, the FILE
        // buffer and the small C++ owner alongside the exact decoder allocator.
        const auto overhead=UINT64_C(16384)+8*path.size()+sizeof(Handle);
        require(request->work_bytes>overhead,"JPEG reader work budget is too small for opening overhead");
        std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(native.c_str(),L"rb"),std::fclose);
        if(!file)throw Failure{QPX_READER_IO_ERROR,"JPEG image cannot be opened"};
        qpx_jpeg_image* decoded=nullptr;char message[256]{};
        const auto status=qpx_jpeg_decode(file.get(),encoded,request->work_bytes-overhead,request->cancel,&decoded,message,sizeof(message));
        Image owner(decoded,qpx_jpeg_destroy);
        if(status!=QPX_READER_OK)return failure(error,status,message[0]?message:"JPEG decoding failed");
        checkpoint(request->cancel);
        auto result=std::make_unique<Handle>();result->image=std::move(owner);
        *output=result.release();return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"JPEG metadata structure is too small");checkpoint(cancel);
        output->level_count=1;output->width=source.width;output->height=source.height;
        output->has_raw=1;output->channels=source.channels;output->sample_type=QPX_READER_UINT8;
        output->z_planes=1;output->timepoints=1;output->series=0;output->series_count=1;
        output->calibration_flags=source.calibration_flags;output->pixel_width_um=source.pixel_width_um;output->pixel_height_um=source.pixel_height_um;
        output->z_spacing_um=0;output->time_interval_seconds=0;
        bool complete=text(output->vendor,"JPEG");complete=text(output->dimension_order,"XYCZT") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"JPEG metadata text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"JPEG level structure is too small");require(index==0,"JPEG reader has only level zero");
        output->reserved=0;output->width=source.width;output->height=source.height;output->downsample=1;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value,std::uint32_t channel,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output!=nullptr,"JPEG channel-name output is null");require(channel<source.channels,"JPEG channel index is out of range");
        const char* name="";
        if(source.channels==1)name="Gray";
        else {const char* names[]={"Red","Green","Blue"};name=names[channel];}
        return text(*output,name)?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"JPEG channel-name buffer is too small");
    });
}
struct Region {std::int64_t width,height;std::uint64_t pixels;};
Region region(const qpx_reader_region_v1* request,std::uint64_t bytes_per_pixel) {
    require(request && request->struct_size>=sizeof(*request),"JPEG region structure is too small");
    require(request->level==0 && !request->z && !request->t,"JPEG reader has only level zero, Z=0 and T=0");
    const auto width=request->width,height=request->height;
    require(width>0 && height>0 && static_cast<std::uint64_t>(width)<=work_limit/bytes_per_pixel &&
        static_cast<std::uint64_t>(height)<=work_limit/(bytes_per_pixel*static_cast<std::uint64_t>(width)),"JPEG output must have positive dimensions and fit 256 MiB");
    const auto limit=std::numeric_limits<std::int64_t>::max();
    require(request->x<=limit-width && request->y<=limit-height,"JPEG region coordinate overflow");
    return {width,height,static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)};
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"JPEG pixel structure is too small");output->written=0;output->validity_written=0;
        const auto area=region(request,4);require(!request->first_channel && !request->channels,"JPEG RGBA request has quantitative coordinates");
        const auto row_stride=static_cast<std::uint64_t>(area.width)*4,count=area.pixels*4;
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==4 && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==4 && output->channel_stride==1,"JPEG RGBA output must use packed UInt8x4 layout");
        require(output->data && output->capacity>=count && !output->validity && !output->validity_capacity,"JPEG RGBA output buffer is invalid");
        std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row)for(std::int64_t col=0;col<area.width;++col) {
            const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            std::memcpy(output->data+(static_cast<std::uint64_t>(row)*area.width+col)*4,source.rgba+(static_cast<std::uint64_t>(y)*source.width+x)*4,4);
        }
        output->written=count;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"JPEG raw pixel structure is too small");output->written=0;output->validity_written=0;
        require(request && request->struct_size>=sizeof(*request),"JPEG raw region structure is too small");
        require(request->channels>0 && request->first_channel<source.channels && request->channels<=source.channels-request->first_channel,"JPEG raw channel range is invalid");
        const auto pixel_stride=static_cast<std::uint64_t>(request->channels);
        const auto area=region(request,pixel_stride);const auto count=area.pixels*pixel_stride,row_stride=static_cast<std::uint64_t>(area.width)*pixel_stride;
        require(area.pixels<=work_limit-count,"JPEG raw output and validity exceed 256 MiB");
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==request->channels && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==pixel_stride && output->channel_stride==1,"JPEG raw output must use packed UInt8 HWC layout");
        require(output->data && output->capacity>=count && output->validity && output->validity_capacity>=area.pixels,"JPEG raw output or validity buffer is invalid");
        checkpoint(cancel);std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));std::fill_n(output->validity,static_cast<std::size_t>(area.pixels),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row) {
            checkpoint(cancel);
            for(std::int64_t col=0;col<area.width;++col) {
                const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
                const auto to=static_cast<std::uint64_t>(row)*area.width+col;
                const auto from=(static_cast<std::uint64_t>(y)*source.width+x)*source.channels+request->first_channel;
                std::memcpy(output->data+to*pixel_stride,source.raw+from,static_cast<std::size_t>(pixel_stride));output->validity[to]=1;
            }
        }
        checkpoint(cancel);output->written=count;output->validity_written=area.pixels;return QPX_READER_OK;
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi,qpx_reader_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"JPEG reader does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"JPEG reader API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,describe,open,destroy,metadata,level,channel_name,read_rgba,read_raw};return QPX_READER_OK;
    });
}
