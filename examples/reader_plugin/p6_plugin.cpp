#include "reader_plugin_profile.hpp"
#include <qupathxx/reader_plugin.h>
#include "ppm_decoder.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr std::uint64_t display_limit=UINT64_C(256)*1024*1024;
struct Handle {
    qpx_ppm::DecodedImage image;
};
struct Failure {
    qpx_reader_status_v1 status;
    const char* message;
};
bool text(qpx_reader_text_v1& out,const char* value) noexcept {
    const auto length=std::strlen(value);
    out.size=static_cast<std::uint64_t>(length);
    if(out.capacity<length || (length && !out.data))return false;
    if(length)std::memcpy(out.data,value,length);
    return true;
}
qpx_reader_status_v1 failure(qpx_reader_text_v1* error,qpx_reader_status_v1 status,const char* message) noexcept {
    if(error && !text(*error,message))return QPX_READER_BUFFER_TOO_SMALL;
    return status;
}
template<class Function>
qpx_reader_status_v1 boundary(qpx_reader_text_v1* error,Function&& function) noexcept {
    if(error)error->size=0;
    try {return function();}
    catch(const Failure& e) {return failure(error,e.status,e.message);}
    catch(const std::invalid_argument& e) {return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::out_of_range& e) {return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::bad_alloc&) {return failure(error,QPX_READER_INTERNAL_ERROR,"P6 plugin allocation failed");}
    catch(const std::runtime_error& e) {return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::exception& e) {return failure(error,QPX_READER_INTERNAL_ERROR,e.what());}
    catch(...) {return failure(error,QPX_READER_INTERNAL_ERROR,"P6 plugin internal failure");}
}
void require(bool condition,const char* message) {
    if(!condition)throw Failure{QPX_READER_INVALID_REQUEST,message};
}
void checkpoint(qpx_reader_cancel_v1 cancel) {
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))
        throw Failure{QPX_READER_CANCELLED,"P6 image opening cancelled"};
}
Handle& handle(void* value) {
    require(value!=nullptr,"P6 reader handle is null");
    return *static_cast<Handle*>(value);
}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"P6 image path is invalid");
    std::string result(value.data,static_cast<std::size_t>(value.size));
    require(result.find('\0')==std::string::npos,"P6 image path contains NUL");
    require(std::filesystem::u8path(result).is_absolute(),"P6 image path must be absolute");
    return result;
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size>=sizeof(*out),"P6 reader info structure is too small");
        out->abi_version=QPX_READER_ABI_V1;
        out->capabilities=QPX_READER_CAP_RGBA;
        bool complete=text(out->provider_id,"org.qupathxx.p6");
        complete=text(out->display_name,"P6 display example") && complete;
        complete=text(out->version,"1.0.0") && complete;
        complete=text(out->contract,"p6-max255-display/1") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"P6 reader descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        require(out!=nullptr,"P6 reader output handle is null");
        *out=nullptr;
        require(request && request->struct_size>=sizeof(*request),"P6 open structure is too small");
        require(request->reserved==0,"P6 open reserved field must be zero");
        require(request->options.size==0,"P6 display reader accepts no options");
        require(request->work_bytes && request->work_bytes<=display_limit,"P6 reader work budget must be between 1 and 256 MiB");
        checkpoint(request->cancel);
        const auto path=path_string(request->path);
        // This conservative admission bounds the unchanged shared parser before
        // decoding. Large encoded files are refused even with a small raster.
        require(std::filesystem::file_size(std::filesystem::u8path(path))<=request->work_bytes,
            "P6 encoded input exceeds reader work budget");
        auto image=qpx_ppm::read_ppm(path);
        require(image.rgb.size()<=request->work_bytes,"P6 decoded image exceeds reader work budget");
        checkpoint(request->cancel);
        auto result=std::make_unique<Handle>(Handle{std::move(image)});
        *out=result.release();
        return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {
    try {delete static_cast<Handle*>(value);}catch(...) {}
}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* out,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        const auto& source=handle(value).image;
        require(out && out->struct_size>=sizeof(*out),"P6 metadata structure is too small");
        checkpoint(cancel);
        out->level_count=1;out->width=source.width;out->height=source.height;
        out->has_raw=0;out->channels=0;out->sample_type=0;
        out->z_planes=1;out->timepoints=1;out->series=0;out->series_count=1;
        out->calibration_flags=0;
        out->pixel_width_um=0;out->pixel_height_um=0;out->z_spacing_um=0;out->time_interval_seconds=0;
        bool complete=text(out->vendor,"P6 display example");
        complete=text(out->dimension_order,"") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"P6 metadata text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        const auto& source=handle(value).image;
        require(out && out->struct_size>=sizeof(*out),"P6 level structure is too small");
        require(index==0,"P6 reader has only level zero");
        out->reserved=0;out->width=source.width;out->height=source.height;out->downsample=1;
        return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void*,std::uint32_t,qpx_reader_text_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        if(out)out->size=0;
        return failure(error,QPX_READER_UNSUPPORTED,"P6 display reader has no quantitative channels");
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        const auto& source=handle(value).image;
        require(request && request->struct_size>=sizeof(*request),"P6 region structure is too small");
        require(out && out->struct_size>=sizeof(*out),"P6 pixel structure is too small");
        out->written=0;out->validity_written=0;
        require(request->level==0,"P6 reader has only level zero");
        require(!request->first_channel && !request->channels && !request->z && !request->t,"P6 RGBA request has quantitative coordinates");
        const auto width=request->width,height=request->height;
        require(width>0 && height>0 && static_cast<std::uint64_t>(width)<=display_limit/4 &&
            static_cast<std::uint64_t>(height)<=display_limit/(4*static_cast<std::uint64_t>(width)),
            "P6 region must have positive dimensions and fit the 256 MiB output limit");
        const auto limit=std::numeric_limits<std::int64_t>::max();
        require(request->x<=limit-width && request->y<=limit-height,"P6 region coordinate overflow");
        const auto row_stride=static_cast<std::uint64_t>(width)*4;
        const auto count=row_stride*static_cast<std::uint64_t>(height);
        require(out->sample_type==QPX_READER_UINT8 && out->width==width && out->height==height &&
            out->channels==4 && !out->reserved && out->row_stride==row_stride && out->pixel_stride==4 && out->channel_stride==1,
            "P6 RGBA output must use packed UInt8x4 layout");
        require(out->data && out->capacity>=count && !out->validity && !out->validity_capacity,"P6 RGBA output buffer is invalid");
        std::fill_n(out->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<height;++row)for(std::int64_t col=0;col<width;++col) {
            const auto x=request->x+col,y=request->y+row;
            if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            const auto from=static_cast<std::size_t>((y*source.width+x)*3);
            const auto to=static_cast<std::size_t>((row*width+col)*4);
            std::copy_n(source.rgb.data()+from,3,out->data+to);out->data[to+3]=255;
        }
        out->written=count;
        return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void*,const qpx_reader_region_v1*,qpx_reader_pixels_v1* out,qpx_reader_cancel_v1,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        if(out && out->struct_size>=sizeof(*out)) {out->written=0;out->validity_written=0;}
        return failure(error,QPX_READER_UNSUPPORTED,"P6 display reader does not provide quantitative samples");
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi,qpx_reader_api_v1* out,qpx_reader_text_v1* error) {
    return boundary(error,[&]() -> qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"P6 reader does not support the requested ABI");
        require(out && out->struct_size>=sizeof(*out),"P6 reader API structure is too small");
        *out={sizeof(*out),QPX_READER_ABI_V1,describe,open,destroy,metadata,level,channel_name,read_rgba,read_raw};
        return QPX_READER_OK;
    });
}
