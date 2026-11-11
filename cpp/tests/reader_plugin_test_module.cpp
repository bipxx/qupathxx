#ifndef QPX_READER_PLUGIN_EXPORTS
#define QPX_READER_PLUGIN_EXPORTS
#endif
#include "qupathxx/reader_plugin.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>

#ifdef QPX_READER_TEST_WITH_DEPENDENCY
#ifdef _WIN32
extern "C" __declspec(dllimport) std::uint64_t qpx_reader_test_dependency();
#else
extern "C" std::uint64_t qpx_reader_test_dependency();
#endif
#endif
namespace {
struct Handle {std::filesystem::path events,primary;std::string mode,options;std::atomic<unsigned> active{0};std::mutex events_mutex;};
qpx_reader_status_v1 text(qpx_reader_text_v1* out,const char* value) {
    if(!out)return QPX_READER_INVALID_REQUEST;out->size=std::strlen(value);
    if(out->capacity<out->size)return QPX_READER_BUFFER_TOO_SMALL;
    if(out->size && !out->data)return QPX_READER_INVALID_REQUEST;
    std::memcpy(out->data,value,std::size_t(out->size));return QPX_READER_OK;
}
qpx_reader_status_v1 error(qpx_reader_text_v1* out,qpx_reader_status_v1 status,const char* message) {text(out,message);return status;}
void event(Handle& h,const char* value) {std::lock_guard<std::mutex> lock(h.events_mutex);std::ofstream file(h.events,std::ios::binary|std::ios::app);file<<value<<'\n';file.flush();}
bool cancelled(qpx_reader_cancel_v1 c) {return c.is_cancelled && c.is_cancelled(c.context);}
std::uint64_t companion_value(const Handle& h) {
#ifdef QPX_READER_TEST_COMPOUND_SOURCE
    std::ifstream input(std::filesystem::u8path(h.primary.u8string()+".data"),std::ios::binary);std::uint64_t value=0;if(!(input>>value))throw std::runtime_error("test companion missing");return value;
#else
    (void)h;return 0;
#endif
}
struct Active {Handle& h;Active(Handle& x):h(x){if(h.active.fetch_add(1))event(h,"overlap");event(h,"read-start");}~Active(){event(h,"read-end");h.active.fetch_sub(1);}};
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out,qpx_reader_text_v1* e) {
    // Deliberate ABI violations are isolated in fresh loader-test processes.
    if(const auto* mode=std::getenv("QPX_READER_TEST_ENTRY_MODE");mode && std::strcmp(mode,"describe-throw")==0)throw 23;
    try {
    if(!out || out->struct_size!=sizeof(*out))return error(e,QPX_READER_INVALID_REQUEST,"describe shape");
    out->abi_version=1;out->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW;
#ifdef QPX_READER_TEST_COMPOUND_SOURCE
    out->capabilities|=QPX_READER_CAP_SOURCE_FILES;
#endif
    qpx_reader_status_v1 status=QPX_READER_OK;
    for(const auto result:{text(&out->provider_id,"org.qupathxx.test"),text(&out->display_name,"Reader boundary test"),text(&out->version,"1.0.0"),text(&out->contract,"test-raw/1")})if(result!=QPX_READER_OK)status=result;
    return status;
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"describe exception");}}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** out,qpx_reader_text_v1* e) {try {
    if(!request || !out || request->struct_size!=sizeof(*request))return error(e,QPX_READER_INVALID_REQUEST,"open shape");
    *out=nullptr;if(cancelled(request->cancel))return error(e,QPX_READER_CANCELLED,"test cancelled");
    const std::string name(request->path.data,std::size_t(request->path.size));std::ifstream file(std::filesystem::u8path(name),std::ios::binary);std::string mode;std::getline(file,mode);
    if(!file && mode.empty())return error(e,QPX_READER_IO_ERROR,"test source missing");
    auto h=std::make_unique<Handle>();h->mode=mode;h->primary=std::filesystem::u8path(name);h->options.assign(request->options.data?request->options.data:"",std::size_t(request->options.size));h->events=std::filesystem::u8path(name+".events");event(*h,"open");event(*h,("options:"+h->options).c_str());(void)companion_value(*h);*out=h.release();return QPX_READER_OK;
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"open exception");}}
void QPX_READER_CALL destroy(void* handle) {try {auto* h=static_cast<Handle*>(handle);if(h->active.load())event(*h,"destroy-during-read");event(*h,"destroy");delete h;}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* handle,qpx_reader_metadata_v1* out,qpx_reader_cancel_v1 c,qpx_reader_text_v1* e) {try {
    auto& h=*static_cast<Handle*>(handle);if(cancelled(c))return error(e,QPX_READER_CANCELLED,"test cancelled");
    out->width=h.mode=="bad-width"?0:2;out->height=2;out->level_count=h.mode=="bad-level-count"?0:1;
    out->has_raw=1;out->channels=2;out->sample_type=h.mode=="float64"?QPX_READER_FLOAT64:QPX_READER_UINT64;
    if(h.mode=="bad-type")out->sample_type=999;
    out->z_planes=2;out->timepoints=2;out->series=0;out->series_count=1;
    out->calibration_flags=QPX_READER_CALIBRATION_X|QPX_READER_CALIBRATION_Y|QPX_READER_CALIBRATION_Z|QPX_READER_CALIBRATION_TIME;
    out->pixel_width_um=h.mode=="bad-calibration"?-1:.25;out->pixel_height_um=.5;out->z_spacing_um=2;out->time_interval_seconds=.125;
    const auto a=text(&out->vendor,"independent native test");const auto b=text(&out->dimension_order,"XYCZT");
    if(h.mode=="bad-text")out->vendor.size=UINT64_C(1)<<40;
    return a!=QPX_READER_OK?a:b;
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"metadata exception");}}
qpx_reader_status_v1 QPX_READER_CALL level(void* handle,uint32_t index,qpx_reader_level_v1* out,qpx_reader_text_v1* e) {try {
    auto& h=*static_cast<Handle*>(handle);if(index)return error(e,QPX_READER_INVALID_REQUEST,"test level");
    out->width=2;out->height=2;out->downsample=h.mode=="bad-level"?0:1;return QPX_READER_OK;
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"level exception");}}
qpx_reader_status_v1 QPX_READER_CALL channel(void*,uint32_t index,qpx_reader_text_v1* out,qpx_reader_text_v1* e) {try {
    if(index>1)return error(e,QPX_READER_INVALID_REQUEST,"test channel");return text(out,index?"second":"first");
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"channel exception");}}
qpx_reader_status_v1 status(Handle& h,qpx_reader_text_v1* e) {
    if(h.mode=="error-short-buffer"){e->size=e->capacity+1;return QPX_READER_BUFFER_TOO_SMALL;}
    if(h.mode=="success-oversize-error"){e->size=e->capacity+1;return QPX_READER_OK;}
    if(h.mode=="changed-error-pointer"){e->data=nullptr;e->size=e->capacity+1;return QPX_READER_BUFFER_TOO_SMALL;}
    if(h.mode=="changed-error-capacity"){++e->capacity;e->size=e->capacity+1;return QPX_READER_BUFFER_TOO_SMALL;}
    if(h.mode=="unsupported")return error(e,QPX_READER_UNSUPPORTED,"test unsupported");
    if(h.mode=="invalid-request")return error(e,QPX_READER_INVALID_REQUEST,"test invalid request");
    if(h.mode=="io-error")return error(e,QPX_READER_IO_ERROR,"test io error");
    if(h.mode=="internal-error")return error(e,QPX_READER_INTERNAL_ERROR,"test internal error");
    if(h.mode=="cancelled-status")return error(e,QPX_READER_CANCELLED,"test cancelled");
    if(h.mode=="unknown-status")return error(e,99,"test unknown status");
    if(h.mode=="buffer-status")return error(e,QPX_READER_BUFFER_TOO_SMALL,"test short buffer");
    return QPX_READER_OK;
}
qpx_reader_status_v1 QPX_READER_CALL rgba(void* handle,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* pixels,qpx_reader_text_v1* e) {try {
    auto& h=*static_cast<Handle*>(handle);Active active(h);const auto result=status(h,e);if(result)return result;
    if(h.mode=="slow")std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto offset=companion_value(h);std::memset(pixels->data,0,std::size_t(pixels->capacity));
    for(std::int64_t y=0;y<request->height;++y)for(std::int64_t x=0;x<request->width;++x) {
        const auto gx=request->x+x,gy=request->y+y;if(gx<0 || gy<0 || gx>=2 || gy>=2)continue;
        auto* p=pixels->data+std::size_t(y*request->width+x)*4;p[0]=std::uint8_t(10+gx+gy*2+offset);p[1]=20;p[2]=30;p[3]=255;
    }
    pixels->written=pixels->capacity-(h.mode=="short-display"?1:0);
    if(h.mode=="mutate-display")pixels->row_stride++;
    if(h.mode=="bad-oob-display")pixels->data[0]=1;
    return QPX_READER_OK;
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"display exception");}}
qpx_reader_status_v1 QPX_READER_CALL raw(void* handle,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* pixels,qpx_reader_cancel_v1 c,qpx_reader_text_v1* e) {try {
    auto& h=*static_cast<Handle*>(handle);Active active(h);const auto result=status(h,e);if(result)return result;
    if(h.mode=="wait-cancel") {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!cancelled(c) && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(cancelled(c)){event(h,"cancel-observed");return error(e,QPX_READER_CANCELLED,"test cancelled");}
        return error(e,QPX_READER_INTERNAL_ERROR,"cancellation callback missing");
    }
    if(cancelled(c))return error(e,QPX_READER_CANCELLED,"test cancelled");
    const auto offset=companion_value(h);std::memset(pixels->data,0,std::size_t(pixels->capacity));std::memset(pixels->validity,0,std::size_t(pixels->validity_capacity));
    for(std::int64_t y=0;y<request->height;++y)for(std::int64_t x=0;x<request->width;++x) {
        const auto gx=request->x+x,gy=request->y+y;const auto p=std::size_t(y*request->width+x);if(gx<0 || gy<0 || gx>=2 || gy>=2)continue;
        pixels->validity[p]=1;
        for(std::uint32_t channel=0;channel<request->channels;++channel) {
            std::uint64_t bits;
            if(h.mode=="float64") {
                const std::uint64_t values[]={UINT64_C(0x8000000000000000),UINT64_C(0x7ff8000000000042),UINT64_C(0x7ff0000000000000),UINT64_C(0x3ff0000000000000)};
                bits=values[std::size_t(gy*2+gx)];
            }else {
#ifdef QPX_READER_TEST_WITH_DEPENDENCY
                const auto base=qpx_reader_test_dependency();
#else
                const auto base=UINT64_C(9007199254740995);
#endif
                bits=request->first_channel+channel?UINT64_MAX-std::uint64_t(gy*2+gx):base+std::uint64_t(gy*2+gx)+offset;
                bits+=std::uint64_t(request->z)*10+std::uint64_t(request->t)*100;
            }
            std::memcpy(pixels->data+(p*request->channels+channel)*8,&bits,8);
        }
    }
    pixels->written=pixels->capacity-(h.mode=="short-raw"?1:0);pixels->validity_written=pixels->validity_capacity-(h.mode=="short-validity"?1:0);
    if(h.mode=="bad-validity")pixels->validity[0]=2;
    if(h.mode=="bad-oob-raw")pixels->data[0]=1;
    if(h.mode=="mutate-raw")pixels->sample_type=QPX_READER_UINT8;
    return QPX_READER_OK;
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"raw exception");}}
}
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(uint32_t abi,qpx_reader_api_v1* out,qpx_reader_text_v1* e) {
    if(const auto* mode=std::getenv("QPX_READER_TEST_ENTRY_MODE");mode && std::strcmp(mode,"throw")==0)throw 17;
    try {
    if(abi!=1 || !out || out->struct_size<sizeof(*out))return error(e,QPX_READER_UNSUPPORTED,"test ABI refusal");
    *out={sizeof(*out),1,describe,open,destroy,metadata,level,channel,rgba,raw};
    if(const auto* mode=std::getenv("QPX_READER_TEST_ENTRY_MODE")) {
        if(std::strcmp(mode,"abi")==0)out->abi_version=99;
        if(std::strcmp(mode,"size")==0)out->struct_size=1;
        if(std::strcmp(mode,"null")==0)out->read_raw=nullptr;
    }
    return QPX_READER_OK;
}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"entry exception");}}
#ifdef QPX_READER_TEST_COMPOUND_SOURCE
namespace {
std::string source_mode(const qpx_reader_open_v1* request){std::ifstream input(std::filesystem::u8path(std::string(request->path.data,std::size_t(request->path.size))),std::ios::binary);std::string mode;std::getline(input,mode);return mode;}
qpx_reader_status_v1 QPX_READER_CALL source_count(const qpx_reader_open_v1* request,uint32_t* count,qpx_reader_text_v1* e){try{const auto mode=source_mode(request);*count=mode=="too-many-files"?64:mode=="duplicate-files"?2:1;return QPX_READER_OK;}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"file discovery exception");}}
qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request,uint32_t,qpx_reader_text_v1* out,qpx_reader_text_v1* e){try{auto path=std::string(request->path.data,std::size_t(request->path.size));if(source_mode(request)!="primary-as-companion")path+=".data";return text(out,path.c_str());}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"file path exception");}}
}
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(uint32_t abi,qpx_reader_files_api_v1* out,qpx_reader_text_v1* e){try{if(abi!=1 || !out || out->struct_size<sizeof(*out))return error(e,QPX_READER_UNSUPPORTED,"test file ABI refusal");*out={sizeof(*out),1,source_count,source_path};return QPX_READER_OK;}catch(...){return error(e,QPX_READER_INTERNAL_ERROR,"file entry exception");}}
#endif
