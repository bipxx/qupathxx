#include "format_plugin_runtime.hpp"
#include "format_plugin_loader.hpp"
#include "reader_plugin_link.hpp"
#include "workflow_input.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace qpx::detail {
namespace {
constexpr std::uint64_t max_work=256u*1024*1024;
constexpr std::uint64_t page_rows=64,page_values=4096,text_field=4096;
[[noreturn]] void malformed(const char* what) {
    throw std::invalid_argument(std::string("Format plugin returned invalid ")+what);
}
std::int32_t QPX_FORMAT_CALL cancelled_callback(void* context) {
    return context && static_cast<const std::atomic<bool>*>(context)->load(std::memory_order_relaxed)?1:0;
}
qpx_reader_cancel_v1 cancellation(const std::atomic<bool>* c) {
    return {cancelled_callback,const_cast<std::atomic<bool>*>(c)};
}
template<class Call> qpx_reader_status_v1 invoke(const char* operation,Call&& call,bool probe=false) {
    std::array<char,4096> bytes{};qpx_reader_text_v1 error{bytes.data(),bytes.size(),0};
    qpx_reader_status_v1 status;
    try {status=call(&error);}catch(...){throw std::runtime_error(std::string("Format plugin threw across C ABI during ")+operation);}
    if(error.data!=bytes.data() || error.capacity!=bytes.size() || error.size>bytes.size())malformed("error buffer");
    std::string message(bytes.data(),static_cast<std::size_t>(error.size));bounded_utf8(message,bytes.size());
    if(status==QPX_READER_OK || (probe && status==QPX_READER_BUFFER_TOO_SMALL))return status;
    message=std::string("Format plugin ")+operation+": "+(message.empty()?"operation failed":message);
    switch(status) {
    case QPX_READER_CANCELLED:throw WorkflowCancelled(message);
    case QPX_READER_INVALID_REQUEST:throw std::invalid_argument(message);
    case QPX_READER_BUFFER_TOO_SMALL:throw std::length_error(message);
    case QPX_READER_UNSUPPORTED:case QPX_READER_IO_ERROR:case QPX_READER_INTERNAL_ERROR:throw std::runtime_error(message);
    default:malformed("status");
    }
}
std::uint64_t range(std::uint64_t first,std::uint64_t count,std::uint64_t total,std::uint64_t limit) {
    if(!count || count>limit || first>total)throw std::invalid_argument("Format plugin page range exceeds host limits");
    return std::min(count,total-first);
}
std::string field(const std::vector<char>& text,std::uint64_t offset,std::uint64_t size,bool empty=false) {
    if(offset>text.size() || size>text.size()-offset || size>text_field || (!empty && !size))malformed("text offset/length");
    std::string result;
    if(size)result.assign(text.data()+static_cast<std::size_t>(offset),static_cast<std::size_t>(size));
    bounded_utf8(result,text_field);
    for(std::size_t i=0;i<result.size();++i) {
        const auto ch=static_cast<unsigned char>(result[i]);
        if(ch<32 || ch==127 || (ch==0xc2 && i+1<result.size() &&
            static_cast<unsigned char>(result[i+1])>=0x80 && static_cast<unsigned char>(result[i+1])<=0x9f))malformed("text control character");
    }
    return result;
}
struct Session {
    std::shared_ptr<AdmittedFormatPlugin> provider;
    std::unique_ptr<WorkflowInput> source;
    FormatPluginReceipt receipt;
    std::uint64_t work=0;
    void* handle=nullptr;
    Session(FormatPluginDomain domain,const std::string& id,const std::string& path,
            const std::optional<std::string>& options,std::uint64_t allowance,const std::atomic<bool>* c):work(allowance) {
        workflow_checkpoint(c);
        if(work<65536 || work>max_work)throw std::invalid_argument("Format plugin work allowance must be 64 KiB through 256 MiB");
        for(const auto& item:format_plugin_catalog().providers)if(item->info.provider_id==id){provider=item;break;}
        if(!provider || provider->info.domain!=domain)throw std::invalid_argument("Format plugin provider unavailable: "+id);
        source=std::make_unique<WorkflowInput>(path,work,false,c);
        receipt.provider=provider->info;
        receipt.options=normalize_reader_plugin_options(options?*options:provider->info.default_options);
        receipt.provider.default_options.clear();receipt.source_sha256=source->sha256();
        (void)format_plugin_receipt_json(receipt);
    }
    ~Session(){destroy();}
    void destroy() noexcept {
        if(!handle)return;
        try {std::lock_guard<std::mutex> lock(provider->calls);
            if(provider->info.domain==FormatPluginDomain::Matrix)provider->matrix.destroy(handle);
            else provider->annotation.destroy(handle);
        }catch(...){} // Destroy is infallible by contract; never let it escape RAII.
        handle=nullptr;
    }
    template<class Call> auto call(const std::atomic<bool>* c,Call&& operation) {
        workflow_checkpoint(c);source->verify(false,c);
        std::lock_guard<std::mutex> lock(provider->calls);
        auto result=operation();source->verify(false,c);workflow_checkpoint(c);return result;
    }
    void open(const std::atomic<bool>* c) {
        const auto& path=source->path();const auto& options=receipt.options;
        qpx_reader_open_v1 request{sizeof(request),0,{path.data(),path.size()},{options.data(),options.size()},work,cancellation(c)};
        const auto original=request;
        call(c,[&]{return invoke("open",[&](auto* error){return provider->info.domain==FormatPluginDomain::Matrix?
            provider->matrix.open(&request,&handle,error):provider->annotation.open(&request,&handle,error);});});
        if(request.struct_size!=original.struct_size || request.reserved || request.path.data!=original.path.data ||
            request.path.size!=original.path.size || request.options.data!=original.options.data || request.options.size!=original.options.size ||
            request.work_bytes!=work || request.cancel.is_cancelled!=original.cancel.is_cancelled || request.cancel.context!=original.cancel.context || !handle)
            malformed("open request/handle");
    }
};
std::vector<PluginAxisRow> axis(Session& session,const PluginMatrixMetadata& metadata,std::uint32_t kind,
        std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c) {
    const auto total=kind==QPX_FORMAT_FEATURE_AXIS?metadata.features:metadata.observations;
    const auto n=range(first,count,total,page_rows);
    qpx_matrix_axis_request_v1 request{sizeof(request),kind,first,count};
    qpx_matrix_axis_buffer_v1 out{};out.struct_size=sizeof(out);
    auto check=[&](qpx_matrix_axis_row_v1* rows,std::uint64_t capacity,char* text,std::uint64_t text_capacity){
        if(request.struct_size!=sizeof(request) || request.axis!=kind || request.first!=first || request.count!=count ||
            out.struct_size!=sizeof(out) || out.reserved || out.rows!=rows || out.capacity!=capacity ||
            out.text.data!=text || out.text.capacity!=text_capacity || out.written!=n)malformed("axis page descriptor");
    };
    const auto status=session.call(c,[&]{return invoke("axis capacity",[&](auto* error){return session.provider->matrix.axis(session.handle,&request,&out,cancellation(c),error);},true);});
    if(status!=(n?QPX_READER_BUFFER_TOO_SMALL:QPX_READER_OK))malformed("axis capacity status");
    check(nullptr,0,nullptr,0);
    if(out.text.size>n*text_field*2)malformed("axis text capacity");
    const auto needed=out.text.size;
    std::vector<qpx_matrix_axis_row_v1> rows(static_cast<std::size_t>(n));
    std::vector<char> text(static_cast<std::size_t>(needed));
    out.rows=rows.data();out.capacity=n;out.written=0;out.text={text.data(),needed,0};
    session.call(c,[&]{return invoke("axis",[&](auto* error){return session.provider->matrix.axis(session.handle,&request,&out,cancellation(c),error);});});
    check(rows.data(),n,text.data(),needed);if(out.text.size!=needed)malformed("axis text fill length");
    std::vector<PluginAxisRow> result;result.reserve(static_cast<std::size_t>(n));
    for(std::size_t i=0;i<rows.size();++i){workflow_checkpoint(c);const auto& row=rows[i];if(row.index!=first+i)malformed("axis index");
        result.push_back({row.index,field(text,row.id_offset,row.id_size),field(text,row.name_offset,row.name_size,true)});}
    return result;
}
}
struct MatrixPluginSession::Impl {Session session;PluginMatrixMetadata metadata;
    Impl(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,const std::atomic<bool>* c):
        session(FormatPluginDomain::Matrix,id,path,options,work,c) {
        session.open(c);qpx_matrix_metadata_v1 out{};out.struct_size=sizeof(out);
        session.call(c,[&]{return invoke("matrix metadata",[&](auto* error){return session.provider->matrix.metadata(session.handle,&out,cancellation(c),error);});});
        if(out.struct_size!=sizeof(out) || out.value_type!=QPX_FORMAT_COUNTS_UINT64 || out.reserved[0] || out.reserved[1] ||
            !out.features || !out.observations || out.features>std::numeric_limits<std::uint64_t>::max()/out.observations)malformed("matrix metadata");
        metadata={out.features,out.observations};session.source->verify(true,c);
    }
};
MatrixPluginSession::MatrixPluginSession(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
MatrixPluginSession::~MatrixPluginSession()=default;
std::unique_ptr<MatrixPluginSession> MatrixPluginSession::open(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,const std::atomic<bool>* c) {
    return std::unique_ptr<MatrixPluginSession>(new MatrixPluginSession(std::make_unique<Impl>(id,path,options,work,c)));
}
const PluginMatrixMetadata& MatrixPluginSession::metadata() const noexcept{return impl_->metadata;}
const FormatPluginReceipt& MatrixPluginSession::receipt() const noexcept{return impl_->session.receipt;}
std::uint64_t MatrixPluginSession::retained_work_bytes() const noexcept{return impl_->session.work;}
std::vector<PluginAxisRow> MatrixPluginSession::features(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c){return axis(impl_->session,impl_->metadata,QPX_FORMAT_FEATURE_AXIS,first,count,c);}
std::vector<PluginAxisRow> MatrixPluginSession::observations(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c){return axis(impl_->session,impl_->metadata,QPX_FORMAT_OBSERVATION_AXIS,first,count,c);}
std::vector<std::uint64_t> MatrixPluginSession::values(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c) {
    auto& session=impl_->session;const auto n=range(first,count,impl_->metadata.features*impl_->metadata.observations,page_values);
    qpx_matrix_values_request_v1 request{sizeof(request),0,first,count};std::vector<std::uint64_t> result(static_cast<std::size_t>(n));
    qpx_matrix_values_buffer_v1 out{sizeof(out),0,result.data(),n,0};
    session.call(c,[&]{return invoke("matrix values",[&](auto* error){return session.provider->matrix.values(session.handle,&request,&out,cancellation(c),error);});});
    if(request.struct_size!=sizeof(request) || request.reserved || request.first!=first || request.count!=count ||
        out.struct_size!=sizeof(out) || out.reserved || out.values!=result.data() || out.capacity!=n || out.written!=n)malformed("values page descriptor");
    return result;
}
void MatrixPluginSession::verify_source() const{impl_->session.source->verify(true);}

struct AnnotationPluginSession::Impl {Session session;PluginAnnotationMetadata metadata;
    Impl(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,const std::atomic<bool>* c):
        session(FormatPluginDomain::Annotation,id,path,options,work,c) {
        session.open(c);qpx_annotation_metadata_v1 out{};out.struct_size=sizeof(out);
        session.call(c,[&]{return invoke("annotation metadata",[&](auto* error){return session.provider->annotation.metadata(session.handle,&out,cancellation(c),error);});});
        if(out.struct_size!=sizeof(out) || out.coordinate_frame!=QPX_FORMAT_IMAGE_PIXELS || !out.points || out.reserved[0] || out.reserved[1])malformed("annotation metadata");
        metadata={out.points};session.source->verify(true,c);
    }
};
AnnotationPluginSession::AnnotationPluginSession(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
AnnotationPluginSession::~AnnotationPluginSession()=default;
std::unique_ptr<AnnotationPluginSession> AnnotationPluginSession::open(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,const std::atomic<bool>* c) {
    return std::unique_ptr<AnnotationPluginSession>(new AnnotationPluginSession(std::make_unique<Impl>(id,path,options,work,c)));
}
const PluginAnnotationMetadata& AnnotationPluginSession::metadata() const noexcept{return impl_->metadata;}
const FormatPluginReceipt& AnnotationPluginSession::receipt() const noexcept{return impl_->session.receipt;}
std::uint64_t AnnotationPluginSession::retained_work_bytes() const noexcept{return impl_->session.work;}
std::vector<PluginPoint> AnnotationPluginSession::points(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c) {
    auto& session=impl_->session;const auto n=range(first,count,impl_->metadata.points,page_rows);
    qpx_annotation_points_request_v1 request{sizeof(request),0,first,count};qpx_annotation_points_buffer_v1 out{};out.struct_size=sizeof(out);
    auto check=[&](qpx_annotation_point_v1* points,std::uint64_t capacity,char* text,std::uint64_t text_capacity){
        if(request.struct_size!=sizeof(request) || request.reserved || request.first!=first || request.count!=count ||
            out.struct_size!=sizeof(out) || out.reserved || out.points!=points || out.capacity!=capacity ||
            out.text.data!=text || out.text.capacity!=text_capacity || out.written!=n)malformed("points page descriptor");
    };
    const auto status=session.call(c,[&]{return invoke("point capacity",[&](auto* error){return session.provider->annotation.points(session.handle,&request,&out,cancellation(c),error);},true);});
    if(status!=(n?QPX_READER_BUFFER_TOO_SMALL:QPX_READER_OK))malformed("point capacity status");
    check(nullptr,0,nullptr,0);if(out.text.size>n*text_field*2)malformed("point text capacity");
    const auto needed=out.text.size;std::vector<qpx_annotation_point_v1> points(static_cast<std::size_t>(n));std::vector<char> text(static_cast<std::size_t>(needed));
    out.points=points.data();out.capacity=n;out.written=0;out.text={text.data(),needed,0};
    session.call(c,[&]{return invoke("points",[&](auto* error){return session.provider->annotation.points(session.handle,&request,&out,cancellation(c),error);});});
    check(points.data(),n,text.data(),needed);if(out.text.size!=needed)malformed("point text fill length");
    std::vector<PluginPoint> result;result.reserve(static_cast<std::size_t>(n));
    for(const auto& p:points){workflow_checkpoint(c);
        if(!std::isfinite(p.x) || !std::isfinite(p.y) || p.x<0 || p.y<0 || p.c< -1 || p.z<0 || p.t<0 || p.reserved)malformed("point coordinates/plane");
        result.push_back({p.x,p.y,p.c,p.z,p.t,field(text,p.source_id_offset,p.source_id_size),field(text,p.class_offset,p.class_size,true)});}
    return result;
}
void AnnotationPluginSession::verify_source() const{impl_->session.source->verify(true);}
} // namespace qpx::detail
