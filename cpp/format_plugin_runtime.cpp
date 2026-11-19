#include "format_plugin_runtime.hpp"
#include "format_plugin_loader.hpp"
#include "reader_plugin_link.hpp"
#include "workflow_input.hpp"
#include "reader_file_guard.hpp"
#include "directory_inventory.hpp"
#include "utf8_text.hpp"
#include "geometry_archive.hpp"
#include "annotation_geometry_value.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <type_traits>
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
    std::uint64_t work=0,file_limit=0,total_limit=0,retained=0;
    std::unique_ptr<DirectoryStoreSnapshot> store;
    std::shared_ptr<const ReaderSource> store_reader;
    std::unique_ptr<ReaderFileGuard> primary_guard;
    std::optional<PluginMatrixStoreSource> store_info;
    void* handle=nullptr;
    Session(FormatPluginDomain domain,const std::string& id,const std::string& path,
            const std::optional<std::string>& options,std::uint64_t allowance,const std::atomic<bool>* c,
            std::uint64_t file_allowance=0):work(allowance),retained(allowance) {
        workflow_checkpoint(c);
        if(work<65536 || work>max_work)throw std::invalid_argument("Format plugin work allowance must be 64 KiB through 256 MiB");
        for(const auto& item:format_plugin_catalog().providers)if(item->info.provider_id==id){provider=item;break;}
        if(!provider || provider->info.domain!=domain)throw std::invalid_argument("Format plugin provider unavailable: "+id);
        if(domain==FormatPluginDomain::Matrix && matrix_directory_abi(provider->info.abi))
            throw std::invalid_argument("Matrix directory-store provider requires explicit store options");
        file_limit=work;
        if(domain==FormatPluginDomain::Matrix && provider->info.abi>=2) {
            file_limit=file_allowance?file_allowance:work;
            if(file_limit>16ull*1024*1024*1024)throw std::invalid_argument("Typed matrix file allowance exceeds 16 GiB");
        }
        source=std::make_unique<WorkflowInput>(path,file_limit,false,c);
        receipt.provider=provider->info;
        const auto& selected_options=options?*options:provider->info.default_options;
        receipt.options=domain==FormatPluginDomain::Matrix && provider->info.abi>=2?
            normalize_typed_matrix_options(selected_options):normalize_reader_plugin_options(selected_options);
        receipt.provider.default_options.clear();receipt.source_sha256=source->sha256();
        (void)format_plugin_receipt_json(receipt);
    }
    Session(const std::string& id,const std::string& path,const std::string& primary_key,
            const std::optional<std::string>& options,std::uint64_t allowance,
            const DirectoryStoreLimits& inventory,const DirectoryStoreFingerprintLimits& content,
            std::uint64_t host_work,const std::atomic<bool>* c):work(allowance),file_limit(content.file_bytes),total_limit(content.total_bytes) {
        workflow_checkpoint(c);inventory.validate();content.validate();
        if(work<65536 || work>max_work)throw std::invalid_argument("Format plugin work allowance must be 64 KiB through 256 MiB");
        if(!host_work || host_work>1024ull*1024*1024)throw std::invalid_argument("Matrix store host work allowance must be positive and at most 1 GiB");
        // Both inventories coexist during currentness checks. Four MiB also
        // covers filesystem/path work, bounded primary/source wrappers and the
        // streaming fingerprint buffer; no per-file retained lease is assumed.
        std::size_t capture=std::size_t(work);
        workflow_capture_charge(capture,inventory.inventory_bytes,std::size_t(host_work));
        workflow_capture_charge(capture,inventory.inventory_bytes,std::size_t(host_work));
        workflow_capture_charge(capture,4u*1024*1024,std::size_t(host_work));
        for(const auto& item:format_plugin_catalog().providers)if(item->info.provider_id==id){provider=item;break;}
        if(!provider || provider->info.domain!=FormatPluginDomain::Matrix || !matrix_directory_abi(provider->info.abi))
            throw std::invalid_argument("Matrix directory-store provider unavailable: "+id);
        store=std::make_unique<DirectoryStoreSnapshot>(path,inventory,c);
        store_reader=store->reader_source("matrix-store:"+id,primary_key,c);
        primary_guard=std::make_unique<ReaderFileGuard>(store_reader->primary->path);
        require_directory_file_version(*store_reader->primary,*primary_guard->version());
        const auto fingerprint=store->content_fingerprint(content,c);
        store_info=PluginMatrixStoreSource{primary_key,store->observed_sha256(),fingerprint.content_sha256,fingerprint.files,fingerprint.bytes};
        capture=std::size_t(work);workflow_capture_charge(capture,store->payload_bytes(),std::size_t(host_work));
        workflow_capture_charge(capture,inventory.inventory_bytes,std::size_t(host_work));
        workflow_capture_charge(capture,4u*1024*1024,std::size_t(host_work));retained=capture;
        receipt.provider=provider->info;
        receipt.options=normalize_typed_matrix_options(options?*options:provider->info.default_options);
        receipt.provider.default_options.clear();receipt.source_sha256=fingerprint.content_sha256;
        (void)format_plugin_receipt_json(receipt);verify(false,c);
    }
    ~Session(){destroy();}
    void destroy() noexcept {
        if(!handle)return;
        try {std::lock_guard<std::mutex> lock(provider->calls);
            if(provider->info.domain==FormatPluginDomain::Matrix) {
                if(matrix_directory_abi(provider->info.abi))provider->matrix_store.destroy(handle);
                else if(provider->info.abi>=2)provider->matrix_typed.destroy(handle);else provider->matrix.destroy(handle);
            }
            else if(provider->info.abi>=3)provider->metadata.destroy(handle);
            else if(provider->info.abi==2)provider->geometry.destroy(handle);
            else provider->annotation.destroy(handle);
        }catch(...){} // Destroy is infallible by contract; never let it escape RAII.
        handle=nullptr;
    }
    void verify(bool rehash,const std::atomic<bool>* c) const {
        workflow_checkpoint(c);
        if(source){source->verify(rehash,c);return;}
        // The complete content digest was captured under member guards. Its
        // separate native membership/file-version observation must remain current
        // throughout every callback and final publication; no atomic FS claim.
        try {primary_guard->verify();store->require_current(c);primary_guard->verify();}
        catch(const WorkflowCancelled&){throw;}
        catch(const WorkflowInputChanged&){throw;}
        catch(const std::exception& e){workflow_checkpoint(c);throw WorkflowInputChanged(std::string("Matrix directory source changed or unavailable: ")+e.what());}
        workflow_checkpoint(c);
    }
    template<class Call> auto call(const std::atomic<bool>* c,Call&& operation) {
        workflow_checkpoint(c);verify(false,c);
        std::lock_guard<std::mutex> lock(provider->calls);
        const auto run=[&]{try{return operation();}catch(...){if(store)verify(false,c);throw;}};
        auto result=run();verify(false,c);workflow_checkpoint(c);return result;
    }
    void open(const std::atomic<bool>* c) {
        const auto& path=source?source->path():store->directory();const auto& options=receipt.options;
        if(provider->info.domain==FormatPluginDomain::Matrix && matrix_directory_abi(provider->info.abi)) {
            const auto& key=store_info->primary_key;
            qpx_matrix_open_v3 request{sizeof(request),0,{path.data(),path.size()},{key.data(),key.size()},
                {options.data(),options.size()},work,file_limit,total_limit,cancellation(c)};
            const auto original=request;
            call(c,[&]{return invoke("store matrix open",[&](auto* error){return provider->matrix_store.open(&request,&handle,error);});});
            if(request.struct_size!=original.struct_size || request.reserved || request.path.data!=original.path.data ||
                request.path.size!=original.path.size || request.primary_key.data!=original.primary_key.data ||
                request.primary_key.size!=original.primary_key.size || request.options.data!=original.options.data ||
                request.options.size!=original.options.size || request.work_bytes!=work || request.file_bytes!=file_limit ||
                request.total_bytes!=total_limit || request.cancel.is_cancelled!=original.cancel.is_cancelled ||
                request.cancel.context!=original.cancel.context || !handle)malformed("store matrix open request/handle");
            return;
        }
        if(provider->info.domain==FormatPluginDomain::Matrix && provider->info.abi>=2) {
            qpx_matrix_open_v2 request{sizeof(request),0,{path.data(),path.size()},{options.data(),options.size()},work,file_limit,cancellation(c)};
            const auto original=request;
            call(c,[&]{return invoke("typed matrix open",[&](auto* error){return provider->matrix_typed.open(&request,&handle,error);});});
            if(request.struct_size!=original.struct_size || request.reserved || request.path.data!=original.path.data ||
                request.path.size!=original.path.size || request.options.data!=original.options.data || request.options.size!=original.options.size ||
                request.work_bytes!=work || request.file_bytes!=file_limit || request.cancel.is_cancelled!=original.cancel.is_cancelled ||
                request.cancel.context!=original.cancel.context || !handle)malformed("typed matrix open request/handle");
            return;
        }
        qpx_reader_open_v1 request{sizeof(request),0,{path.data(),path.size()},{options.data(),options.size()},work,cancellation(c)};
        const auto original=request;
        call(c,[&]{return invoke("open",[&](auto* error){return provider->info.domain==FormatPluginDomain::Matrix?
            provider->matrix.open(&request,&handle,error):(provider->info.abi>=3?provider->metadata.open(&request,&handle,error):(provider->info.abi==2?provider->geometry.open(&request,&handle,error):provider->annotation.open(&request,&handle,error)));});});
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
    const auto callback=matrix_directory_abi(session.provider->info.abi)?session.provider->matrix_store.axis:(session.provider->info.abi>=2?session.provider->matrix_typed.axis:session.provider->matrix.axis);
    const auto status=session.call(c,[&]{return invoke("axis capacity",[&](auto* error){return callback(session.handle,&request,&out,cancellation(c),error);},true);});
    if(status!=(n?QPX_READER_BUFFER_TOO_SMALL:QPX_READER_OK))malformed("axis capacity status");
    check(nullptr,0,nullptr,0);
    if(out.text.size>n*text_field*2)malformed("axis text capacity");
    const auto needed=out.text.size;
    std::vector<qpx_matrix_axis_row_v1> rows(static_cast<std::size_t>(n));
    std::vector<char> text(static_cast<std::size_t>(needed));
    out.rows=rows.data();out.capacity=n;out.written=0;out.text={text.data(),needed,0};
    session.call(c,[&]{return invoke("axis",[&](auto* error){return callback(session.handle,&request,&out,cancellation(c),error);});});
    check(rows.data(),n,text.data(),needed);if(out.text.size!=needed)malformed("axis text fill length");
    std::vector<PluginAxisRow> result;result.reserve(static_cast<std::size_t>(n));
    for(std::size_t i=0;i<rows.size();++i){workflow_checkpoint(c);const auto& row=rows[i];if(row.index!=first+i)malformed("axis index");
        result.push_back({row.index,field(text,row.id_offset,row.id_size),field(text,row.name_offset,row.name_size,true)});}
    return result;
}
template<class Value> std::vector<Value> typed_values(Session& session,const PluginMatrixMetadata& metadata,
        std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c) {
    const auto n=range(first,count,metadata.features*metadata.observations,page_values);
    qpx_matrix_values_request_v1 request{sizeof(request),0,first,count};
    std::vector<Value> result(static_cast<std::size_t>(n));
    qpx_matrix_values_buffer_v2 out{};out.struct_size=sizeof(out);out.capacity=n;
    if constexpr(std::is_same_v<Value,std::uint64_t>) {
        out.value_type=QPX_FORMAT_COUNTS_UINT64;out.counts=result.data();
    } else {
        out.value_type=QPX_FORMAT_CONTINUOUS_FLOAT64;out.continuous=result.data();
    }
    const auto original=out;
    const auto callback=matrix_directory_abi(session.provider->info.abi)?session.provider->matrix_store.values:session.provider->matrix_typed.values;
    session.call(c,[&]{return invoke("typed matrix values",[&](auto* error){return callback(session.handle,&request,&out,cancellation(c),error);});});
    if(request.struct_size!=sizeof(request) || request.reserved || request.first!=first || request.count!=count ||
        out.struct_size!=sizeof(out) || out.value_type!=original.value_type || out.counts!=original.counts ||
        out.continuous!=original.continuous || out.capacity!=n || out.written!=n || out.reserved)
        malformed("typed values page descriptor");
    if constexpr(std::is_same_v<Value,double>) {
        for(const auto value:result)if(!std::isfinite(value))malformed("nonfinite continuous matrix value");
    }
    return result;
}
}
struct MatrixPluginSession::Impl {Session session;PluginMatrixMetadata metadata;
    Impl(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,const std::atomic<bool>* c,std::uint64_t file_bytes=0):
        session(FormatPluginDomain::Matrix,id,path,options,work,c,file_bytes) {read_metadata(c);}
    Impl(const std::string& id,const std::string& path,const std::string& primary_key,
            const std::optional<std::string>& options,std::uint64_t work,const DirectoryStoreLimits& inventory,
            const DirectoryStoreFingerprintLimits& content,std::uint64_t host_work,const std::atomic<bool>* c):
        session(id,path,primary_key,options,work,inventory,content,host_work,c) {read_metadata(c);}
    void read_metadata(const std::atomic<bool>* c) {
        session.open(c);
        if(session.provider->info.abi>=2) {
            std::array<char,128> label{};
            qpx_matrix_metadata_v2 out{};out.struct_size=sizeof(out);out.value_domain={label.data(),label.size(),0};
            const auto callback=matrix_directory_abi(session.provider->info.abi)?session.provider->matrix_store.metadata:session.provider->matrix_typed.metadata;
            session.call(c,[&]{return invoke("typed matrix metadata",[&](auto* error){return callback(session.handle,&out,cancellation(c),error);});});
            if(out.struct_size!=sizeof(out) || (out.value_type!=QPX_FORMAT_COUNTS_UINT64 && out.value_type!=QPX_FORMAT_CONTINUOUS_FLOAT64) ||
                out.reserved[0] || out.reserved[1] || out.value_domain.data!=label.data() || out.value_domain.capacity!=label.size() ||
                !out.value_domain.size || out.value_domain.size>label.size() || !out.features || !out.observations ||
                out.features>std::numeric_limits<std::uint64_t>::max()/out.observations)malformed("typed matrix metadata");
            metadata.features=out.features;metadata.observations=out.observations;
            metadata.domain=out.value_type==QPX_FORMAT_COUNTS_UINT64?ExpressionValueDomain::Counts:ExpressionValueDomain::Continuous;
            metadata.value_domain.assign(label.data(),static_cast<std::size_t>(out.value_domain.size));
            bounded_utf8(metadata.value_domain,128);
            if(metadata.value_domain.find_first_of(std::string("\0\t\r\n",4))!=std::string::npos ||
                (metadata.domain==ExpressionValueDomain::Counts?metadata.value_domain!="raw-counts":metadata.value_domain=="raw-counts"))
                malformed("typed matrix domain label");
        } else {
            qpx_matrix_metadata_v1 out{};out.struct_size=sizeof(out);
            session.call(c,[&]{return invoke("matrix metadata",[&](auto* error){return session.provider->matrix.metadata(session.handle,&out,cancellation(c),error);});});
            if(out.struct_size!=sizeof(out) || out.value_type!=QPX_FORMAT_COUNTS_UINT64 || out.reserved[0] || out.reserved[1] ||
                !out.features || !out.observations || out.features>std::numeric_limits<std::uint64_t>::max()/out.observations)malformed("matrix metadata");
            metadata.features=out.features;metadata.observations=out.observations;
        }
        const auto sparse_metadata=session.provider->info.abi==QPX_MATRIX_ABI_V5?
            session.provider->matrix_sparse_file.sparse_metadata:session.provider->matrix_sparse.sparse_metadata;
        if(sparse_metadata) {
            qpx_matrix_sparse_metadata_v1 out{sizeof(out),0,0,0};
            session.call(c,[&]{return invoke("sparse matrix metadata",[&](auto* error){
                return sparse_metadata(session.handle,&out,cancellation(c),error);
            });});
            if(out.struct_size!=sizeof(out) || out.reserved || out.sparse>1 ||
                (!out.sparse && out.entries) || out.entries>metadata.features*metadata.observations)
                malformed("sparse matrix metadata");
            if(out.sparse)metadata.stored_entries=out.entries;
        }
        session.verify(true,c);
    }
};
MatrixPluginSession::MatrixPluginSession(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
MatrixPluginSession::~MatrixPluginSession()=default;
std::unique_ptr<MatrixPluginSession> MatrixPluginSession::open(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,const std::atomic<bool>* c) {
    return std::unique_ptr<MatrixPluginSession>(new MatrixPluginSession(std::make_unique<Impl>(id,path,options,work,c)));
}
std::unique_ptr<MatrixPluginSession> MatrixPluginSession::open_with_file_limit(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,std::uint64_t file_bytes,const std::atomic<bool>* c) {
    if(!file_bytes || file_bytes>16ull*1024*1024*1024)throw std::invalid_argument("Typed matrix file allowance must be positive and at most 16 GiB");
    return std::unique_ptr<MatrixPluginSession>(new MatrixPluginSession(std::make_unique<Impl>(id,path,options,work,c,file_bytes)));
}
std::unique_ptr<MatrixPluginSession> MatrixPluginSession::open_store(const std::string& id,
        const std::string& path,const std::string& primary_key,const std::optional<std::string>& options,
        std::uint64_t work,const DirectoryStoreLimits& inventory,const DirectoryStoreFingerprintLimits& content,
        std::uint64_t host_work,const std::atomic<bool>* c) {
    return std::unique_ptr<MatrixPluginSession>(new MatrixPluginSession(std::make_unique<Impl>(
        id,path,primary_key,options,work,inventory,content,host_work,c)));
}
const PluginMatrixMetadata& MatrixPluginSession::metadata() const noexcept{return impl_->metadata;}
const FormatPluginReceipt& MatrixPluginSession::receipt() const noexcept{return impl_->session.receipt;}
const std::optional<PluginMatrixStoreSource>& MatrixPluginSession::store_source() const noexcept{return impl_->session.store_info;}
std::uint64_t MatrixPluginSession::retained_work_bytes() const noexcept{return impl_->session.retained;}
std::vector<PluginAxisRow> MatrixPluginSession::features(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c){return axis(impl_->session,impl_->metadata,QPX_FORMAT_FEATURE_AXIS,first,count,c);}
std::vector<PluginAxisRow> MatrixPluginSession::observations(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c){return axis(impl_->session,impl_->metadata,QPX_FORMAT_OBSERVATION_AXIS,first,count,c);}
std::vector<std::uint64_t> MatrixPluginSession::values(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c) {
    if(impl_->metadata.domain!=ExpressionValueDomain::Counts)throw std::invalid_argument("Count page requested from a continuous matrix");
    if(impl_->session.provider->info.abi>=2)return typed_values<std::uint64_t>(impl_->session,impl_->metadata,first,count,c);
    auto& session=impl_->session;const auto n=range(first,count,impl_->metadata.features*impl_->metadata.observations,page_values);
    qpx_matrix_values_request_v1 request{sizeof(request),0,first,count};std::vector<std::uint64_t> result(static_cast<std::size_t>(n));
    qpx_matrix_values_buffer_v1 out{sizeof(out),0,result.data(),n,0};
    session.call(c,[&]{return invoke("matrix values",[&](auto* error){return session.provider->matrix.values(session.handle,&request,&out,cancellation(c),error);});});
    if(request.struct_size!=sizeof(request) || request.reserved || request.first!=first || request.count!=count ||
        out.struct_size!=sizeof(out) || out.reserved || out.values!=result.data() || out.capacity!=n || out.written!=n)malformed("values page descriptor");
    return result;
}
std::vector<double> MatrixPluginSession::continuous_values(std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c) {
    if(impl_->metadata.domain!=ExpressionValueDomain::Continuous || impl_->session.provider->info.abi<2)
        throw std::invalid_argument("Continuous page requested from a count matrix");
    return typed_values<double>(impl_->session,impl_->metadata,first,count,c);
}
std::vector<qpx_matrix_sparse_entry_v1> MatrixPluginSession::sparse_values(
        std::uint64_t first,std::uint64_t count,const std::atomic<bool>* c) {
    if(!impl_->metadata.stored_entries)throw std::invalid_argument("Sparse page requested from a dense matrix");
    auto& session=impl_->session;
    const auto size=range(first,count,*impl_->metadata.stored_entries,page_values);
    std::vector<qpx_matrix_sparse_entry_v1> entries(static_cast<std::size_t>(size));
    qpx_matrix_values_request_v1 request{sizeof(request),0,first,count};
    qpx_matrix_sparse_buffer_v1 out{sizeof(out),0,entries.data(),size,0};
    const auto callback=session.provider->info.abi==QPX_MATRIX_ABI_V5?
        session.provider->matrix_sparse_file.sparse_values:session.provider->matrix_sparse.sparse_values;
    session.call(c,[&]{return invoke("sparse matrix values",[&](auto* error){
        return callback(session.handle,&request,&out,cancellation(c),error);
    });});
    if(request.struct_size!=sizeof(request) || request.reserved || request.first!=first || request.count!=count ||
        out.struct_size!=sizeof(out) || out.reserved || out.entries!=entries.data() || out.capacity!=size ||
        out.written!=size)malformed("sparse values page descriptor");
    return entries;
}
void MatrixPluginSession::verify_source() const{impl_->session.verify(true,nullptr);}
void MatrixPluginSession::verify_source(const std::atomic<bool>* c) const{impl_->session.verify(true,c);}

struct AnnotationPluginSession::Impl {Session session;PluginAnnotationMetadata metadata;
    Impl(const std::string& id,const std::string& path,const std::optional<std::string>& options,std::uint64_t work,const std::atomic<bool>* c):
        session(FormatPluginDomain::Annotation,id,path,options,work,c) {
        session.open(c);
        if(session.provider->info.abi>=3) {
            qpx_annotation_metadata_v3 out{};out.struct_size=sizeof(out);
            session.call(c,[&]{return invoke("annotation source metadata",[&](auto* error){return session.provider->metadata.metadata(session.handle,&out,cancellation(c),error);});});
            if(out.struct_size!=sizeof(out) || out.coordinate_frame!=QPX_FORMAT_IMAGE_PIXELS || out.reserved[0] || out.reserved[1] ||
                !out.objects || !out.geometry_bytes || out.objects>out.geometry_bytes/5 || !out.geometry_encodings || (out.geometry_encodings&~(session.provider->info.abi>=5?UINT64_C(7):UINT64_C(3))) ||
                out.objects>UINT64_MAX/256 || out.properties>out.objects*256)malformed("annotation source metadata");
            metadata={out.objects,out.geometry_bytes,out.source_bytes,out.properties,out.geometry_encodings};
            if(session.provider->info.abi==QPX_ANNOTATION_ABI_V6) {
                qpx_annotation_canvas_v1 canvas{sizeof(canvas),0,0,0};
                session.call(c,[&]{return invoke("annotation canvas",[&](auto* error){
                    return session.provider->annotation_canvas.canvas(session.handle,&canvas,cancellation(c),error);
                });});
                constexpr auto maximum_axis=UINT64_C(9007199254740992);
                if(canvas.struct_size!=sizeof(canvas) || canvas.reserved || !canvas.width || !canvas.height ||
                   canvas.width>maximum_axis || canvas.height>maximum_axis)malformed("annotation canvas dimensions");
                metadata.canvas_width=canvas.width;
                metadata.canvas_height=canvas.height;
            }
        } else if(session.provider->info.abi==2) {
            qpx_annotation_metadata_v2 out{};out.struct_size=sizeof(out);
            session.call(c,[&]{return invoke("annotation geometry metadata",[&](auto* error){return session.provider->geometry.metadata(session.handle,&out,cancellation(c),error);});});
            if(out.struct_size!=sizeof(out) || out.coordinate_frame!=QPX_FORMAT_IMAGE_PIXELS ||
                out.geometry_encoding!=QPX_FORMAT_GEOMETRY_QRO1 || out.reserved || out.reserved64[0] || out.reserved64[1] ||
                !out.objects || !out.geometry_bytes || out.objects>out.geometry_bytes/5)malformed("annotation geometry metadata");
            metadata={out.objects,out.geometry_bytes};
        } else {
            qpx_annotation_metadata_v1 out{};out.struct_size=sizeof(out);
            session.call(c,[&]{return invoke("annotation metadata",[&](auto* error){return session.provider->annotation.metadata(session.handle,&out,cancellation(c),error);});});
            if(out.struct_size!=sizeof(out) || out.coordinate_frame!=QPX_FORMAT_IMAGE_PIXELS || !out.points || out.reserved[0] || out.reserved[1])malformed("annotation metadata");
            metadata={out.points,0};
        }
        session.source->verify(true,c);
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
    auto& session=impl_->session;
    if(session.provider->info.abi!=1)throw std::invalid_argument("Annotation geometry provider does not expose ABI1 points");
    const auto n=range(first,count,impl_->metadata.points,page_rows);
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
static std::vector<PluginAnnotationObject> source_objects(Session& session,
        const PluginAnnotationMetadata& metadata,std::uint64_t first,std::uint64_t count,
        std::size_t available,const std::atomic<bool>* c) {
    const auto n=range(first,count,metadata.points,1);
    qpx_annotation_objects_request_v2 request{sizeof(request),0,first,count};
    qpx_annotation_objects_buffer_v3 out{};out.struct_size=sizeof(out);
    auto check=[&](qpx_annotation_object_v3* rows,std::uint64_t capacity,char* text,std::uint64_t text_capacity,
                   std::uint8_t* geometry,std::uint64_t geometry_capacity,
                   qpx_annotation_property_v3* properties,std::uint64_t property_capacity) {
        if(request.struct_size!=sizeof(request) || request.reserved || request.first!=first || request.count!=count ||
            out.struct_size!=sizeof(out) || out.reserved || out.objects!=rows || out.capacity!=capacity || out.written!=n ||
            out.text.data!=text || out.text.capacity!=text_capacity || out.geometry.data!=geometry || out.geometry.capacity!=geometry_capacity ||
            out.properties!=properties || out.property_capacity!=property_capacity)malformed("source facts page descriptor");
    };
    const auto status=session.call(c,[&]{return invoke("source facts capacity",[&](auto* error){
        return session.provider->metadata.objects(session.handle,&request,&out,cancellation(c),error);
    },true);});
    if(status!=(n?QPX_READER_BUFFER_TOO_SMALL:QPX_READER_OK))malformed("source facts capacity status");
    check(nullptr,0,nullptr,0,nullptr,0,nullptr,0);
    if(out.text.size>n*(1024u*1024+8192) || out.geometry.size>1024u*1024 ||
        out.properties_written>n*256 || out.properties_written>metadata.properties ||
        (n?out.geometry.size<5:out.geometry.size!=0))malformed("source facts page capacity");
    const auto text_size=out.text.size,geometry_size=out.geometry.size,property_count=out.properties_written;
    std::size_t used=65536;
    workflow_capture_charge(used,std::size_t(geometry_size)*32,available);
    workflow_capture_charge(used,std::size_t(text_size)*4,available);
    // Text spans can alias. Reserve independent owned map/name copies using
    // logical field bounds, not only the physical wire-buffer size.
    workflow_capture_charge(used,std::size_t(property_count)*(256+4096+256)*4+4096*4,available);
    std::vector<qpx_annotation_object_v3> rows(static_cast<std::size_t>(n));
    std::vector<qpx_annotation_property_v3> properties(static_cast<std::size_t>(property_count));
    std::vector<char> text(static_cast<std::size_t>(text_size));
    std::vector<std::uint8_t> geometry(static_cast<std::size_t>(geometry_size));
    out.objects=rows.data();out.capacity=n;out.written=0;out.text={text.data(),text_size,0};
    out.geometry={geometry.data(),geometry_size,0};out.properties=properties.data();out.property_capacity=property_count;out.properties_written=0;
    session.call(c,[&]{return invoke("source facts objects",[&](auto* error){
        return session.provider->metadata.objects(session.handle,&request,&out,cancellation(c),error);
    });});
    check(rows.data(),n,text.data(),text_size,geometry.data(),geometry_size,properties.data(),property_count);
    if(out.text.size!=text_size || out.geometry.size!=geometry_size || out.properties_written!=property_count)malformed("source facts fill length");
    auto source_text=[&](std::uint64_t offset,std::uint64_t size) {
        if(size>4096 || offset>text.size() || size>text.size()-offset)malformed("source text span");
        std::string value;if(size)value.assign(text.data()+std::size_t(offset),std::size_t(size));
        bounded_utf8(value,4096);return value;
    };
    std::uint64_t geometry_at=0,property_at=0;
    std::vector<PluginAnnotationObject> result;result.reserve(rows.size());
    for(const auto& row:rows) {
        workflow_checkpoint(c);const auto& v=row.value;
        if(v.reserved || row.reserved || v.c< -1 || v.z< -1 || v.t< -1 || v.geometry_offset!=geometry_at ||
            v.geometry_size<5 || v.geometry_size>geometry_size-geometry_at || row.first_property!=property_at ||
            row.properties>property_count-property_at || row.geometry_encoding<1 || row.geometry_encoding>(session.provider->info.abi>=5?3u:2u) ||
            !(metadata.geometry_encodings&(UINT64_C(1)<<(row.geometry_encoding-1))) ||
            (row.flags&~(QPX_ANNOTATION_OBJECT_COLOR_RGB | (session.provider->info.abi>=4?QPX_ANNOTATION_OBJECT_LOCKED:0u))) || row.color_rgb>0xffffff ||
            (!(row.flags&QPX_ANNOTATION_OBJECT_COLOR_RGB) && row.color_rgb))malformed("source facts row");
        if(row.plane_kind==QPX_ANNOTATION_PLANE_CZT) {
            if(row.linear_position)malformed("unexpected linear plane position");
        } else if(row.plane_kind!=QPX_ANNOTATION_PLANE_LINEAR_POSITION || !row.linear_position ||
                  v.c!=-1 || v.z!=-1 || v.t!=-1)malformed("source plane descriptor");
        PluginAnnotationObject value;
        if(row.geometry_encoding==QPX_FORMAT_GEOMETRY_BINARY_MASK1) {
            const auto decoded=decode_annotation_pixel_mask(geometry.data()+geometry_at,std::size_t(v.geometry_size),available-used,c);
            value.geometry=decoded.geometry;value.canvas_width=decoded.width;value.canvas_height=decoded.height;
        } else value.geometry=decode_annotation_geometry_value(row.geometry_encoding,geometry.data()+geometry_at,std::size_t(v.geometry_size),c);
        const auto bounds=value.geometry.bounds();if(bounds.x<0 || bounds.y<0)malformed("negative source geometry");
        value.c=v.c;value.z=v.z;value.t=v.t;value.source_id=field(text,v.source_id_offset,v.source_id_size);
        value.class_label=field(text,v.class_offset,v.class_size,true);value.display_name=source_text(row.name_offset,row.name_size);
        value.source_bytes=value.display_name.size();value.geometry_bytes=v.geometry_size;
        value.geometry_encoding=row.geometry_encoding;value.plane_kind=row.plane_kind;value.linear_position=row.linear_position;
        if(row.flags&QPX_ANNOTATION_OBJECT_COLOR_RGB)value.color_rgb=row.color_rgb;
        value.locked=(row.flags&QPX_ANNOTATION_OBJECT_LOCKED)!=0;
        for(std::uint64_t i=0;i<row.properties;++i) {
            workflow_checkpoint(c);const auto& property=properties[std::size_t(property_at+i)];
            auto key=field(text,property.key_offset,property.key_size),data=source_text(property.value_offset,property.value_size);
            if(key.size()>256)malformed("source fact key length");
            value.source_bytes+=key.size()+data.size();
            if(!value.source_properties.emplace(std::move(key),std::move(data)).second)malformed("duplicate source fact key");
        }
        if(value.source_bytes>1024u*1024)malformed("source fact bytes per object");
        Sha256 source;source.integer(row.geometry_encoding);source.update(geometry.data()+geometry_at,std::size_t(v.geometry_size));
        value.geometry_sha256=source.finish();
        const auto canonical=GeometryArchiveCodec::encode(value.geometry);Sha256 canonical_hash;canonical_hash.update(canonical.data(),canonical.size());
        value.canonical_geometry_sha256=canonical_hash.finish();
        if(row.geometry_encoding!=QPX_FORMAT_GEOMETRY_BINARY_MASK1 && value.geometry.payload_bytes()>used)malformed("source geometry payload bound");
        geometry_at+=v.geometry_size;property_at+=row.properties;result.push_back(std::move(value));
    }
    if(geometry_at!=geometry_size || property_at!=property_count)malformed("unused source facts/geometry");
    workflow_checkpoint(c);return result;
}
std::vector<PluginAnnotationObject> AnnotationPluginSession::objects(std::uint64_t first,
        std::uint64_t count,std::size_t available,const std::atomic<bool>* c) {
    if(impl_->session.provider->info.abi>=3)return source_objects(impl_->session,impl_->metadata,first,count,available,c);
    if(impl_->session.provider->info.abi==1) {
        auto legacy=points(first,count,c);
        std::vector<PluginAnnotationObject> result;result.reserve(legacy.size());
        for(auto& p:legacy) {
            workflow_checkpoint(c);
            result.push_back({Roi::points(Ring{{p.x,p.y}}),p.x,p.y,p.c,p.z,p.t,
                std::move(p.source_id),std::move(p.class_label),{},0});
        }
        return result;
    }
    // One bounded object per page keeps large geometry independent of the
    // number of neighboring objects. The native import owns the aggregate quota.
    auto& session=impl_->session;
    const auto n=range(first,count,impl_->metadata.points,1);
    qpx_annotation_objects_request_v2 request{sizeof(request),0,first,count};
    qpx_annotation_objects_buffer_v2 out{};out.struct_size=sizeof(out);
    auto check=[&](qpx_annotation_object_v2* rows,std::uint64_t capacity,
                   char* text,std::uint64_t text_capacity,
                   std::uint8_t* geometry,std::uint64_t geometry_capacity) {
        if(request.struct_size!=sizeof(request) || request.reserved || request.first!=first || request.count!=count ||
            out.struct_size!=sizeof(out) || out.reserved || out.objects!=rows || out.capacity!=capacity || out.written!=n ||
            out.text.data!=text || out.text.capacity!=text_capacity ||
            out.geometry.data!=geometry || out.geometry.capacity!=geometry_capacity)
            malformed("geometry page descriptor");
    };
    const auto status=session.call(c,[&]{return invoke("geometry capacity",[&](auto* error){
        return session.provider->geometry.objects(session.handle,&request,&out,cancellation(c),error);
    },true);});
    if(status!=(n?QPX_READER_BUFFER_TOO_SMALL:QPX_READER_OK))malformed("geometry capacity status");
    check(nullptr,0,nullptr,0,nullptr,0);
    if(out.text.size>n*text_field*2 || out.geometry.size>1024u*1024 ||
        (n?out.geometry.size<5:out.geometry.size!=0))malformed("geometry page byte count");
    const auto needed_text=out.text.size,needed_geometry=out.geometry.size;
    // Includes wire buffers, bounded native decode/geometry copies, exact-wire
    // digest/row and library bookkeeping. This is the native logical work quota,
    // not a claim about total process RSS or a topology runtime bound.
    std::size_t used=65536;
    workflow_capture_charge(used,std::size_t(needed_geometry)*32,available);
    workflow_capture_charge(used,std::size_t(needed_text)*4,available);
    std::vector<qpx_annotation_object_v2> rows(static_cast<std::size_t>(n));
    std::vector<char> text(static_cast<std::size_t>(needed_text));
    std::vector<std::uint8_t> geometry(static_cast<std::size_t>(needed_geometry));
    out.objects=rows.data();out.capacity=n;out.written=0;
    out.text={text.data(),needed_text,0};out.geometry={geometry.data(),needed_geometry,0};
    session.call(c,[&]{return invoke("geometry objects",[&](auto* error){
        return session.provider->geometry.objects(session.handle,&request,&out,cancellation(c),error);
    });});
    check(rows.data(),n,text.data(),needed_text,geometry.data(),needed_geometry);
    if(out.text.size!=needed_text || out.geometry.size!=needed_geometry)malformed("geometry fill byte count");
    std::vector<PluginAnnotationObject> result;result.reserve(rows.size());
    std::uint64_t offset=0;
    for(const auto& row:rows) {
        workflow_checkpoint(c);
        if(row.reserved || row.c< -1 || row.z<0 || row.t<0 || row.geometry_offset!=offset ||
            row.geometry_size<5 || row.geometry_size>needed_geometry-offset)malformed("geometry span/plane");
        auto roi=GeometryArchiveCodec::decode(geometry.data()+offset,std::size_t(row.geometry_size));
        const auto bounds=roi.bounds();
        if(bounds.x<0 || bounds.y<0)malformed("negative geometry coordinates");
        Sha256 digest;digest.update(geometry.data()+offset,std::size_t(row.geometry_size));
        result.push_back({std::move(roi),0,0,row.c,row.z,row.t,
            field(text,row.source_id_offset,row.source_id_size),field(text,row.class_offset,row.class_size,true),
            digest.finish(),row.geometry_size});
        if(result.back().geometry.payload_bytes()>used)malformed("geometry native payload bound");
        offset+=row.geometry_size;
    }
    if(offset!=needed_geometry)malformed("unused geometry bytes");
    workflow_checkpoint(c);return result;
}
void AnnotationPluginSession::verify_source() const{impl_->session.source->verify(true);}
} // namespace qpx::detail
