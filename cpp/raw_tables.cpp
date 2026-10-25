#include "qupathxx/raw_tables.hpp"
#include "qupathxx/source_identity.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <future>
#include <set>
#include <thread>
#include <stdexcept>

namespace qpx {
struct RawTableRequest::Data {
    struct Entry {std::shared_ptr<const RawAnalysisRecord> record;std::string status;};
    std::vector<Entry> records;
    std::vector<std::string> protected_sources;
    bool grid_schema=false;
    std::uint64_t rows=0;
    std::size_t bytes=0;
};
RawTableRequest::RawTableRequest(const Document& document,const std::vector<std::uint64_t>& ids,bool require_current,std::size_t maximum,const std::vector<std::string>& protected_paths) {
    if(maximum<4096 || maximum>512u*1024*1024 || ids.size()>10000 || protected_paths.size()>1024) throw std::invalid_argument("invalid raw table snapshot budget/selection");
    const auto count=ids.empty()?document.raw_analysis_record_count():ids.size();
    if(count>(maximum-sizeof(Data))/(sizeof(Data::Entry)+sizeof(std::shared_ptr<const RawAnalysisRecord>)+64)) throw std::length_error("raw table snapshot exceeds byte limit");
    auto data=std::make_shared<Data>();data->bytes=sizeof(Data);data->records.reserve(count);data->bytes+=data->records.capacity()*sizeof(Data::Entry);
    auto add=[&](std::size_t bytes) {if(bytes>maximum-data->bytes) throw std::length_error("raw table snapshot exceeds byte limit");data->bytes+=bytes;};
    std::set<std::string> paths;for(const auto& path:protected_paths) paths.insert(detail::export_path(path));const auto current=document.server()->source_locator();if(!current.empty()) paths.insert(current);
    auto capture=[&](const std::shared_ptr<const RawAnalysisRecord>& record) {
        auto status=document.raw_analysis_status(record->id);if(require_current && status!="current") throw std::invalid_argument("raw table contains unverified or stale records");
        add(record->payload_bytes()+status.capacity()+1);data->grid_schema=data->grid_schema || record->result->grid().has_value();data->rows+=record->result->grid()?record->result->grid()->options.channels:record->result->region().channels;paths.insert(record->result->source()->locator);
        data->records.push_back({record,std::move(status)});
    };
    if(ids.empty()) for(const auto& record:document.raw_analysis_records()) capture(record);
    else {std::set<std::uint64_t> selected(ids.begin(),ids.end());for(auto id:selected) capture(document.raw_analysis_record(id));}
    data->protected_sources.reserve(paths.size());add(data->protected_sources.capacity()*sizeof(std::string));
    while(!paths.empty()) {auto item=paths.extract(paths.begin());add(item.value().capacity()+1);data->protected_sources.push_back(std::move(item.value()));}
    data_=std::move(data);
}
std::uint64_t RawTableRequest::rows() const {return data_->rows;}
std::size_t RawTableRequest::records() const {return data_->records.size();}
std::size_t RawTableRequest::payload_bytes() const {return data_->bytes;}
namespace {
void bytes_limit(std::size_t value) {if(value<1024 || value>std::size_t(1024u)*1024*1024) throw std::invalid_argument("raw CSV byte limit must be 1 KiB..1 GiB");}
std::string quoted(const std::string& value) {
    if(value.find_first_of(",\r\n\"")==std::string::npos) return value;std::string out="\"";for(char c:value) {if(c=='\"') out+='\"';out+=c;}return out+'\"';
}
std::string real(std::optional<double> value) {
    if(!value) return {};char buffer[64];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),*value,std::chars_format::general,17);
    if(result.ec!=std::errc{}) throw std::runtime_error("cannot format raw measurement");return {buffer,result.ptr};
}
std::string line(const std::vector<std::string>& fields) {std::string out;for(const auto& field:fields) {if(!out.empty()) out+=',';out+=quoted(field);}return out+"\r\n";}
constexpr char header[]="schema,record_id,object_id,object_revision,document_revision,object_name,object_class,parent_id,object_kind,tma_core_id,tma_core_revision,tma_grid_name,tma_row,tma_column,tma_row_label,tma_column_label,tma_core_name,tma_case_id,tma_missing,status_at_capture,algorithm,input_sha256,series,z,time,channel,channel_name,sample_type,level,downsample,level_zero_x,level_zero_y,output_width,output_height,pixels,valid_pixels,finite_samples,nonfinite_samples,mean,minimum,maximum,pixel_width_um,pixel_height_um,z_spacing_um,time_interval_seconds,nucleus,roi_wkt,source_path,source_bytes,source_identity_scheme,source_identity,source_stamp_scheme,source_modified_token,source_change_token,source_observed_unix_ms\r\n";
}
struct RawTableKernel {
    template<class Sink> static void write(const RawTableRequest& request,Sink&& sink,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr) {
        detail::export_checkpoint(cancelled);
        if(request.data_->grid_schema) {
            std::string typed_header(header,sizeof(header)-3);
            typed_header+=",sampling_kind,derived_sample_type,grid_origin_x,grid_origin_y,grid_extent_x,grid_extent_y,grid_samples_x,grid_samples_y,grid_context_x,grid_context_y,decoded_source_sha256,sampling_input_sha256\r\n";
            sink(typed_header.data(),typed_header.size());
        } else sink(header,sizeof(header)-1);
        for(const auto& entry:request.data_->records) {
            const auto& record=*entry.record;const auto& result=*record.result;const auto region=result.grid()?RawRegionRequest{{result.grid()->grid.x().origin(),result.grid()->grid.y().origin(),0,result.grid()->grid.width(),result.grid()->grid.height()},result.grid()->options.first_channel,result.grid()->options.channels,result.grid()->options.z,result.grid()->options.t}:result.region();const auto& metadata=result.metadata();const auto& stats=result.statistics();const auto& file=*result.source()->primary;const auto& context=result.object_context();const auto& tma=context.tma;
            const auto roi=record.mask?record.mask->wkt():std::string{};
            for(std::uint32_t c=0;c<region.channels;++c) {
                detail::export_checkpoint(cancelled);
                std::vector<std::string> fields={request.data_->grid_schema?"qpx.raw-analysis-csv/2":"qpx.raw-analysis-csv/1",std::to_string(record.id),std::to_string(result.object_id()),std::to_string(result.object_revision()),std::to_string(result.document_revision()),context.name,context.class_name,std::to_string(context.parent_id),context.kind==ObjectKind::Cell?"cell":context.kind==ObjectKind::Detection?"detection":"annotation",
                    tma?std::to_string(tma->core_id):std::string{},tma?std::to_string(tma->core_revision):std::string{},tma?tma->grid_name:std::string{},tma?std::to_string(tma->row):std::string{},tma?std::to_string(tma->column):std::string{},
                    tma?tma->row_label:std::string{},tma?tma->column_label:std::string{},tma?tma->core_name:std::string{},tma?tma->case_id:std::string{},tma?(tma->missing?"1":"0"):std::string{},entry.status,result.algorithm(),result.input_sha256(),
                    std::to_string(metadata.series),std::to_string(region.z),std::to_string(region.t),std::to_string(region.first_channel+c),metadata.channel_names[c],sample_name(metadata.sample_type),result.grid()?std::string{}:std::to_string(region.region.level),result.grid()?std::string{}:real(metadata.downsample),
                    result.grid()?std::string{}:std::to_string(region.region.x),result.grid()?std::string{}:std::to_string(region.region.y),std::to_string(region.region.width),std::to_string(region.region.height),std::to_string(stats.pixels),std::to_string(stats.valid_pixels),std::to_string(stats.finite_samples[c]),std::to_string(stats.nonfinite_samples[c]),
                    real(stats.means[c]),real(stats.minima[c]),real(stats.maxima[c]),real(result.pixel_width_um()),real(result.pixel_height_um()),real(metadata.z_spacing_um),real(metadata.time_interval_seconds),result.nucleus()?"1":"0",roi,
                    file.path,std::to_string(file.bytes),file.identity_scheme,file.identity,file.stamp_scheme,file.modified_token,file.change_token,std::to_string(file.observed_unix_ms)};
                if(request.data_->grid_schema) {
                    if(result.grid()) {
                        const auto& capture=*result.grid();const auto& g=capture.grid;
                        for(const auto& value:std::vector<std::string>{"explicit_grid","float64",std::to_string(g.x().origin()),std::to_string(g.y().origin()),std::to_string(g.x().extent()),std::to_string(g.y().extent()),std::to_string(g.x().samples()),std::to_string(g.y().samples()),std::to_string(g.x().context()),std::to_string(g.y().context()),capture.decoded_source_sha256,capture.sampling_input_sha256}) fields.push_back(value);
                    } else {
                        fields.push_back("natural_level");fields.push_back("float64");fields.resize(fields.size()+10);
                    }
                }
                const auto row=line(fields);
                sink(row.data(),row.size());if(completed) completed->fetch_add(1,std::memory_order_relaxed);
            }
        }detail::export_checkpoint(cancelled);
    }
    static std::size_t save(const RawTableRequest& request,const std::string& path,bool replace,std::size_t maximum,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr) {
        bytes_limit(maximum);const auto output=detail::export_path(path);for(const auto& source:request.data_->protected_sources) detail::guard_file_output(source,"",output);
        detail::AtomicExportFile file(output,maximum,cancelled);write(request,[&](const char* data,std::size_t count){file.write(data,count);},cancelled,completed);
        for(const auto& source:request.data_->protected_sources) detail::guard_file_output(source,"",output);file.commit(replace);return file.bytes();
    }
};
std::string raw_analysis_csv(const RawTableRequest& request,std::size_t maximum,const std::atomic<bool>* cancelled) {
    bytes_limit(maximum);std::string output;RawTableKernel::write(request,[&](const char* data,std::size_t count) {if(count>maximum-output.size()) throw std::length_error("raw CSV exceeds byte limit");output.append(data,count);},cancelled);return output;
}
std::size_t save_raw_analysis_csv(const RawTableRequest& request,const std::string& path,bool replace,std::size_t maximum,const std::atomic<bool>* cancelled) {return RawTableKernel::save(request,path,replace,maximum,cancelled);}
struct RawTableJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();std::future<std::size_t> future;std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};RawTableStats stats;std::chrono::steady_clock::time_point started;
    void check_owner() const {if(std::this_thread::get_id()!=owner) throw std::runtime_error("raw table controller must run on its owner thread");}
};
RawTableJob::RawTableJob():impl_(std::make_unique<Impl>()) {}
RawTableJob::~RawTableJob() {impl_->cancelled.store(true);if(impl_->future.valid()) impl_->future.wait();}
bool RawTableJob::busy() const {impl_->check_owner();return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawTableJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawTableJob::await_completion() {impl_->check_owner();if(impl_->future.valid()) impl_->future.wait();}
bool RawTableJob::request(const RawTableRequest& request,const std::string& path,bool replace,std::size_t maximum) {
    impl_->check_owner();if(busy()) return false;bytes_limit(maximum);if(impl_->stats.generation==UINT64_MAX) throw std::overflow_error("raw table generation exhausted");const auto output=detail::export_path(path);
    impl_->cancelled.store(false);impl_->completed.store(0);auto future=std::async(std::launch::async,[state=impl_.get(),request,output,replace,maximum]{return RawTableKernel::save(request,output,replace,maximum,&state->cancelled,&state->completed);});
    impl_->future=std::move(future);const auto generation=impl_->stats.generation+1;impl_->stats={};impl_->stats.generation=generation;impl_->stats.path=output;impl_->stats.total_rows=request.rows();impl_->stats.state="running";impl_->started=std::chrono::steady_clock::now();return true;
}
RawTableStats RawTableJob::poll() {
    impl_->check_owner();auto& stats=impl_->stats;
    if(impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {stats.bytes=impl_->future.get();stats.state="saved";}catch(const std::exception& error) {stats.state=impl_->cancelled.load()?"cancelled":"failed";try {stats.error=error.what();detail::bounded_utf8(stats.error,8192);}catch(...) {stats.error="Raw CSV export failed";}}
        stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-impl_->started).count();
    }stats.completed_rows=impl_->completed.load(std::memory_order_relaxed);return stats;
}
RawTableStats RawTableJob::wait() {await_completion();return poll();}
}
