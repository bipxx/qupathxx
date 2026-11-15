// Repository-authored adversarial test provider; never shipped. Reports invalid
// outputs without writing beyond caller allocations or retaining caller buffers.
#include "qupathxx/format_plugin.h"
#include "format_plugin_profile.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace fs=std::filesystem;
namespace {
struct Context{std::string mode,path;};
qpx_reader_status_v1 error(qpx_reader_text_v1* out,qpx_reader_status_v1 code,const std::string& message){if(out){out->size=message.size();if(out->data&&out->capacity>=message.size())std::memcpy(out->data,message.data(),message.size());}return code;}
template<class F>qpx_reader_status_v1 guarded(qpx_reader_text_v1* out,F&& fn)noexcept{try{return fn();}catch(const std::bad_alloc&){return error(out,QPX_READER_INTERNAL_ERROR,"test provider allocation failed");}catch(...){return error(out,QPX_READER_INTERNAL_ERROR,"test provider exception contained");}}
bool cancelled(qpx_reader_cancel_v1 value){return value.is_cancelled&&value.is_cancelled(value.context);}
void event(const Context& c,const char* text){std::ofstream out(fs::u8path(c.path+".events"),std::ios::binary|std::ios::app);out<<text<<'\n';}
qpx_reader_status_v1 describe(qpx_format_info_v1* out,qpx_reader_text_v1* err,bool matrix){return guarded(err,[&]{
    if(!out||out->struct_size<sizeof(*out)||out->reserved)return error(err,QPX_READER_INVALID_REQUEST,"invalid descriptor output");
    const std::array<std::string,6> values={matrix?"org.qupathxx.test.bad-matrix":"org.qupathxx.test.bad-annotation",matrix?"bad-matrix":"bad-annotation",matrix?"Boundary matrix":"Boundary annotations","1.0.0",matrix?"test-matrix/1":"test-annotation/1",QPX_FORMAT_PLUGIN_PROFILE_ID};
    std::array<qpx_reader_text_v1*,6> texts={&out->provider_id,&out->format_id,&out->display_name,&out->version,&out->contract,&out->profile};bool fit=true;
    for(std::size_t i=0;i<texts.size();++i){texts[i]->size=values[i].size();fit=fit&&texts[i]->data&&texts[i]->capacity>=values[i].size();}
    out->abi_version=1;out->domain=matrix?QPX_FORMAT_MATRIX:QPX_FORMAT_ANNOTATION;
    if(!fit)return error(err,QPX_READER_BUFFER_TOO_SMALL,"descriptor text capacity");for(std::size_t i=0;i<texts.size();++i)std::memcpy(texts[i]->data,values[i].data(),values[i].size());return QPX_READER_OK;
});}
qpx_reader_status_v1 QPX_FORMAT_CALL matrix_describe(qpx_format_info_v1* o,qpx_reader_text_v1* e){return describe(o,e,true);}
qpx_reader_status_v1 QPX_FORMAT_CALL annotation_describe(qpx_format_info_v1* o,qpx_reader_text_v1* e){return describe(o,e,false);}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_reader_open_v1* r,void** handle,qpx_reader_text_v1* err){return guarded(err,[&]{
    if(handle)*handle=nullptr;if(!r||!handle||r->struct_size<sizeof(*r)||r->reserved||!r->path.data||!r->path.size||!r->work_bytes||r->work_bytes>256ull*1024*1024)return error(err,QPX_READER_INVALID_REQUEST,"invalid open request");
    if(cancelled(r->cancel))return error(err,QPX_READER_CANCELLED,"open cancelled");const std::string path(r->path.data,static_cast<std::size_t>(r->path.size));std::ifstream in(fs::u8path(path),std::ios::binary);if(!in)return error(err,QPX_READER_IO_ERROR,"control source unavailable");std::string mode;std::getline(in,mode);auto* c=new Context{mode,path};event(*c,"open");*handle=c;return QPX_READER_OK;
});}
void QPX_FORMAT_CALL destroy(void* p){try{if(p){std::unique_ptr<Context> c(static_cast<Context*>(p));event(*c,"destroy");}}catch(...) {}}
qpx_reader_status_v1 QPX_FORMAT_CALL matrix_metadata(void* p,qpx_matrix_metadata_v1* out,qpx_reader_cancel_v1 c,qpx_reader_text_v1* err){return guarded(err,[&]{
    if(!p||!out||out->struct_size<sizeof(*out))return error(err,QPX_READER_INVALID_REQUEST,"invalid matrix metadata request");if(cancelled(c))return error(err,QPX_READER_CANCELLED,"metadata cancelled");const auto& mode=static_cast<Context*>(p)->mode;out->value_type=QPX_FORMAT_COUNTS_UINT64;out->features=3;out->observations=3;
    if(mode=="matrix-type")out->value_type=99;if(mode=="matrix-zero")out->features=0;if(mode=="matrix-huge")out->features=UINT64_MAX;if(mode=="matrix-reserved")out->reserved[0]=1;if(mode=="matrix-mutate")out->struct_size=4;return QPX_READER_OK;
});}
const std::array<std::string,3> feature_ids={"0002","2","zero-only"},observation_ids={"0001","1","18446744073709551615"};
qpx_reader_status_v1 QPX_FORMAT_CALL axis(void* p,const qpx_matrix_axis_request_v1* r,qpx_matrix_axis_buffer_v1* out,qpx_reader_cancel_v1 c,qpx_reader_text_v1* err){return guarded(err,[&]{
    if(!p||!r||!out||r->struct_size<sizeof(*r)||out->struct_size<sizeof(*out)||out->reserved||(r->axis!=1&&r->axis!=2)||!r->count||r->first>3)return error(err,QPX_READER_INVALID_REQUEST,"invalid axis request");if(cancelled(c))return error(err,QPX_READER_CANCELLED,"axis cancelled");const auto& mode=static_cast<Context*>(p)->mode;const auto& ids=r->axis==1?feature_ids:observation_ids;const auto n=std::min<std::uint64_t>(r->count,3-r->first);std::uint64_t text_size=0;for(std::uint64_t i=0;i<n;++i)text_size+=ids[static_cast<std::size_t>(r->first+i)].size();out->written=n;out->text.size=text_size;
    if(out->capacity<n||out->text.capacity<text_size||(n&&!out->rows)||(text_size&&!out->text.data))return error(err,QPX_READER_BUFFER_TOO_SMALL,"axis buffer capacity");std::uint64_t at=0;for(std::uint64_t i=0;i<n;++i){const auto& id=ids[static_cast<std::size_t>(r->first+i)];out->rows[i]={r->first+i,at,id.size(),0,0};std::memcpy(out->text.data+at,id.data(),id.size());at+=id.size();}
    if(n){if(mode=="axis-short")out->written=n-1;if(mode=="axis-long")out->written=n+1;if(mode=="axis-index")out->rows[0].index=99;if(mode=="axis-offset")out->rows[0].id_offset=UINT64_MAX;if(mode=="axis-empty-id")out->rows[0].id_size=0;if(mode=="axis-utf8"){out->text.data[0]=char(0xc0);out->text.data[1]=char(0xaf);out->rows[0].id_offset=0;out->rows[0].id_size=2;}if(mode=="axis-c1"){out->text.data[0]=char(0xc2);out->text.data[1]=char(0x85);out->rows[0].id_offset=0;out->rows[0].id_size=2;}if(mode=="observation-name"&&r->axis==2){out->rows[0].name_offset=out->rows[0].id_offset;out->rows[0].name_size=out->rows[0].id_size;}if(mode=="axis-nul")out->text.data[0]=0;if(mode=="axis-text-long")out->text.size=out->text.capacity+1;if(mode=="axis-mutate")out->capacity++;if(mode=="axis-duplicate"&&n>1){out->rows[1].id_offset=out->rows[0].id_offset;out->rows[1].id_size=out->rows[0].id_size;}}
    return QPX_READER_OK;
});}
qpx_reader_status_v1 wait_or_mutate(Context& c,const char* stage,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* err){
    if(c.mode==std::string("wait-")+stage){event(c,"entered");const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!fs::exists(fs::u8path(c.path+".release"))){if(cancelled(cancel))return error(err,QPX_READER_CANCELLED,"controlled call cancelled");if(std::chrono::steady_clock::now()>end)return error(err,QPX_READER_IO_ERROR,"control release timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
    if(c.mode=="mutate-source"){std::ofstream out(fs::u8path(c.path),std::ios::binary|std::ios::app);out<<"changed after open\n";}
    return QPX_READER_OK;
}
qpx_reader_status_v1 QPX_FORMAT_CALL values(void* p,const qpx_matrix_values_request_v1* r,qpx_matrix_values_buffer_v1* out,qpx_reader_cancel_v1 c,qpx_reader_text_v1* err){return guarded(err,[&]{
    if(!p||!r||!out||r->struct_size<sizeof(*r)||out->struct_size<sizeof(*out)||r->reserved||out->reserved||!r->count||r->first>9)return error(err,QPX_READER_INVALID_REQUEST,"invalid values request");if(cancelled(c))return error(err,QPX_READER_CANCELLED,"values cancelled");auto& ctx=*static_cast<Context*>(p);auto code=wait_or_mutate(ctx,"values",c,err);if(code)return code;const auto n=std::min<std::uint64_t>(r->count,9-r->first);out->written=n;if(out->capacity<n||(n&&!out->values))return error(err,QPX_READER_BUFFER_TOO_SMALL,"value buffer capacity");const std::array<std::uint64_t,9> data={UINT64_MAX,UINT64_C(9007199254740993),0,0,7,0,11,0,0};for(std::uint64_t i=0;i<n;++i)out->values[i]=data[static_cast<std::size_t>(r->first+i)];
    if(ctx.mode=="values-short"&&n)out->written=n-1;if(ctx.mode=="values-long")out->written=n+1;if(ctx.mode=="values-mutate")out->capacity++;if(ctx.mode=="values-io")return error(err,QPX_READER_IO_ERROR,"controlled values failure after partial output");return QPX_READER_OK;
});}
qpx_reader_status_v1 QPX_FORMAT_CALL annotation_metadata(void* p,qpx_annotation_metadata_v1* out,qpx_reader_cancel_v1 c,qpx_reader_text_v1* err){return guarded(err,[&]{
    if(!p||!out||out->struct_size<sizeof(*out))return error(err,QPX_READER_INVALID_REQUEST,"invalid point metadata request");if(cancelled(c))return error(err,QPX_READER_CANCELLED,"point metadata cancelled");const auto& mode=static_cast<Context*>(p)->mode;out->coordinate_frame=QPX_FORMAT_IMAGE_PIXELS;out->points=3;if(mode=="point-frame")out->coordinate_frame=99;if(mode=="point-huge")out->points=UINT64_MAX;if(mode=="point-meta-reserved")out->reserved[0]=1;if(mode=="point-meta-mutate")out->struct_size=4;return QPX_READER_OK;
});}
qpx_reader_status_v1 QPX_FORMAT_CALL points(void* p,const qpx_annotation_points_request_v1* r,qpx_annotation_points_buffer_v1* out,qpx_reader_cancel_v1 c,qpx_reader_text_v1* err){return guarded(err,[&]{
    if(!p||!r||!out||r->struct_size<sizeof(*r)||out->struct_size<sizeof(*out)||r->reserved||out->reserved||!r->count||r->first>3)return error(err,QPX_READER_INVALID_REQUEST,"invalid point page request");if(cancelled(c))return error(err,QPX_READER_CANCELLED,"points cancelled");auto& ctx=*static_cast<Context*>(p);auto code=wait_or_mutate(ctx,"points",c,err);if(code)return code;const auto n=std::min<std::uint64_t>(r->count,3-r->first);const std::array<std::string,3> classes={"T","","S"};std::uint64_t needed=0;for(std::uint64_t i=0;i<n;++i)needed+=observation_ids[static_cast<std::size_t>(r->first+i)].size()+classes[static_cast<std::size_t>(r->first+i)].size();out->written=n;out->text.size=needed;if(out->capacity<n||out->text.capacity<needed||(n&&!out->points)||(needed&&!out->text.data))return error(err,QPX_READER_BUFFER_TOO_SMALL,"point buffer capacity");
    std::uint64_t at=0;for(std::uint64_t i=0;i<n;++i){const auto index=static_cast<std::size_t>(r->first+i);const auto& id=observation_ids[index];const auto& label=classes[index];auto& point=out->points[i];point={1.25+index,2.5+index,index==1?0:-1,0,0,0,at,id.size(),at+id.size(),label.size()};std::memcpy(out->text.data+at,id.data(),id.size());at+=id.size();std::memcpy(out->text.data+at,label.data(),label.size());at+=label.size();}
    if(n){auto& point=out->points[0];if(ctx.mode=="points-short")out->written=n-1;if(ctx.mode=="points-long")out->written=n+1;if(ctx.mode=="points-mutate")out->capacity++;if(ctx.mode=="points-text-long")out->text.size=out->text.capacity+1;if(ctx.mode=="points-offset")point.source_id_offset=UINT64_MAX;if(ctx.mode=="points-empty-id")point.source_id_size=0;if(ctx.mode=="points-utf8"){out->text.data[0]=char(0xc0);out->text.data[1]=char(0xaf);point.source_id_size=2;}if(ctx.mode=="points-c1"){out->text.data[0]=char(0xc2);out->text.data[1]=char(0x85);point.source_id_size=2;}if(ctx.mode=="points-nul")out->text.data[0]=0;if(ctx.mode=="points-nan")point.x=std::numeric_limits<double>::quiet_NaN();if(ctx.mode=="points-inf")point.y=std::numeric_limits<double>::infinity();if(ctx.mode=="points-c")point.c=-2;if(ctx.mode=="points-z")point.z=-1;if(ctx.mode=="points-t")point.t=-1;if(ctx.mode=="points-reserved")point.reserved=1;if(ctx.mode=="points-duplicate"&&n>1){out->points[1].source_id_offset=point.source_id_offset;out->points[1].source_id_size=point.source_id_size;}if(ctx.mode=="outside-point"){point.x=1000.25;point.y=2000.5;}}
    if(ctx.mode=="points-io")return error(err,QPX_READER_IO_ERROR,"controlled point failure after partial output");return QPX_READER_OK;
});}
}
extern "C" QPX_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v1(std::uint32_t abi,qpx_matrix_format_api_v1* out,qpx_reader_text_v1* err){return guarded(err,[&]{if(abi!=1||!out||out->struct_size<sizeof(*out))return error(err,QPX_READER_INVALID_REQUEST,"invalid matrix entry");*out={sizeof(*out),1,matrix_describe,open,destroy,matrix_metadata,axis,values};return QPX_READER_OK;});}
extern "C" QPX_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v1(std::uint32_t abi,qpx_annotation_format_api_v1* out,qpx_reader_text_v1* err){return guarded(err,[&]{if(abi!=1||!out||out->struct_size<sizeof(*out))return error(err,QPX_READER_INVALID_REQUEST,"invalid annotation entry");*out={sizeof(*out),1,annotation_describe,open,destroy,annotation_metadata,points};return QPX_READER_OK;});}
