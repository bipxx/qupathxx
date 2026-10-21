#include "qupathxx/cell_tables.hpp"
#include "atomic_export.hpp"
#include <charconv>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
namespace qpx {
namespace {
using detail::export_checkpoint;
constexpr std::size_t megabyte=1024*1024;
struct Row {
    std::string text;bool first=true;
    Row() {text.reserve(4096);}
    void separator() {if(!first) text.push_back(',');first=false;}
    void string(const std::string& value) {separator();text.push_back('"');for(char c:value) {if(c=='"') text.push_back('"');text.push_back(c);}text.push_back('"');}
    void integer(std::uint64_t value) {separator();char buffer[32];auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);if(result.ec!=std::errc{}) throw std::overflow_error("cell table integer formatting failed");text.append(buffer,result.ptr);}
    void real(double value) {if(!std::isfinite(value)) throw std::invalid_argument("cell table contains nonfinite features");separator();char buffer[128];auto result=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::general,std::numeric_limits<double>::max_digits10);if(result.ec!=std::errc{}) throw std::overflow_error("cell table real formatting failed");text.append(buffer,result.ptr);}
    void vector(const ColorVector& value) {for(auto v:value) real(v);}
    void optional(const std::optional<ColorVector>& value) {if(value) vector(*value);else for(int i=0;i<3;++i) separator();}
    const std::string& finish() {text+="\r\n";return text;}
};
const char* feature_name(TissueFeature feature) {
    switch(feature) {case TissueFeature::OpticalDensitySum:return "optical_density_sum";case TissueFeature::Stain1:return "stain_1";case TissueFeature::Stain2:return "stain_2";case TissueFeature::Stain3:return "stain_3";}throw std::invalid_argument("invalid cell table feature");
}
const std::string& header() {
    static const std::string value="table_schema,record_id,source_object_id,source_object_revision,captured_document_revision,export_document_revision,status_at_capture,producer,algorithm,input_sha256,image_width_px,image_height_px,level,downsample,detection_feature,channel_space,threshold,sigma_sample_px,min_peak_radius_sample_px,min_seed_distance_sample_px,expansion_sample_px,min_nucleus_samples,max_nucleus_samples,connectivity,requested_level,tile_size,max_cells,work_bytes,contour_clip_to_roi,contour_max_components,contour_max_edges,contour_max_vertices,contour_work_bytes,white_r,white_g,white_b,stain_1_r,stain_1_g,stain_1_b,stain_2_r,stain_2_g,stain_2_b,stain_3_r,stain_3_g,stain_3_b,cell_label,nucleus_samples,cell_samples,cytoplasm_samples,nucleus_centroid_x_px,nucleus_centroid_y_px,cell_centroid_x_px,cell_centroid_y_px,nucleus_mean_r,nucleus_mean_g,nucleus_mean_b,cell_mean_r,cell_mean_g,cell_mean_b,cytoplasm_mean_r,cytoplasm_mean_g,cytoplasm_mean_b,nucleus_mean_channel_1,nucleus_mean_channel_2,nucleus_mean_channel_3,cell_mean_channel_1,cell_mean_channel_2,cell_mean_channel_3,cytoplasm_mean_channel_1,cytoplasm_mean_channel_2,cytoplasm_mean_channel_3\r\n";
    return value;
}
}
struct CellTableRequest::Data {
    struct Entry {std::shared_ptr<const CellRecord> record;std::string status;};
    std::vector<Entry> entries;CellTableOptions options;std::uint64_t revision=0,rows=0,width=0,height=0;std::size_t bytes=0;double capture_ms=0;
};
void CellTableOptions::validate() const {
    if(!byte_limit || byte_limit>256*megabyte) throw std::invalid_argument("cell table byte limit must be 1 byte..256 MiB");
    if(!snapshot_budget || snapshot_budget>512*megabyte) throw std::invalid_argument("cell table snapshot budget must be 1 byte..512 MiB");
}
CellTableRequest::CellTableRequest(const Document& doc,const std::vector<std::uint64_t>& ids,CellTableOptions options) {
    const auto start=std::chrono::steady_clock::now();options.validate();if(ids.size()>10000) throw std::length_error("cell table selection exceeds 10000 record IDs");
    auto data=std::make_shared<Data>();data->options=options;data->revision=doc.revision();data->width=doc.metadata().width;data->height=doc.metadata().height;
    std::vector<std::shared_ptr<const CellRecord>> records;
    if(ids.empty()) records=doc.cell_records();
    else {auto chosen=ids;std::sort(chosen.begin(),chosen.end());chosen.erase(std::unique(chosen.begin(),chosen.end()),chosen.end());for(auto id:chosen) records.push_back(doc.cell_record(id));}
    data->entries.reserve(records.size());data->bytes=sizeof(Data)+65536+data->entries.capacity()*sizeof(Data::Entry);
    if(data->bytes>options.snapshot_budget) throw std::length_error("cell table snapshot exceeds memory budget");
    for(const auto& record:records) {
        auto status=doc.cell_status(record->id);if(options.require_current && status!="current") throw std::invalid_argument("cell table requires current verified records");
        const auto bytes=record->payload_bytes()+status.capacity();if(bytes>options.snapshot_budget-data->bytes) throw std::length_error("cell table snapshot exceeds memory budget");
        data->bytes+=bytes;data->rows+=record->result->features().size();data->entries.push_back({record,std::move(status)});
    }
    data->capture_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();data_=std::move(data);
}
std::size_t CellTableRequest::record_count() const {return data_->entries.size();}
std::uint64_t CellTableRequest::row_count() const {return data_->rows;}
std::size_t CellTableRequest::snapshot_bytes() const {return data_->bytes;}
double CellTableRequest::capture_ms() const {return data_->capture_ms;}
std::uint64_t CellTableRequest::document_revision() const {return data_->revision;}
CellTableOptions CellTableRequest::options() const {return data_->options;}
struct CellTableWriter {
    template<class Sink> static void run(const CellTableRequest& request,Sink&& sink,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* rows=nullptr) {
        export_checkpoint(cancelled);sink(header());std::uint64_t done=0;
        for(const auto& entry:request.data_->entries) {
            const auto& record=*entry.record;const auto& result=*record.result;const auto& n=*result.nuclei();const auto& p=result.parameters();
            const auto& nuclei=n.components();const auto& bodies=result.cells()->components();
            export_checkpoint(cancelled);if(result.features().empty()) continue;
            // Run metadata is identical for every cell; format it once.
            Row prefix;prefix.string("qpx.cell-table/1");prefix.integer(record.id);prefix.integer(n.object_id());prefix.integer(n.object_revision());prefix.integer(n.document_revision());prefix.integer(request.data_->revision);
            prefix.string(entry.status);prefix.string(record.producer);prefix.string(result.algorithm());prefix.string(n.input_sha256());prefix.integer(request.data_->width);prefix.integer(request.data_->height);prefix.integer(n.level());prefix.real(n.downsample());
            prefix.string(feature_name(p.feature));prefix.string(p.feature==TissueFeature::OpticalDensitySum?"optical_density_rgb":"stain_concentrations");
            for(auto v:{p.threshold,p.sigma,p.min_peak_radius,p.min_seed_distance,p.cell_expansion}) prefix.real(v);
            prefix.integer(p.min_nucleus_pixels);prefix.integer(p.max_nucleus_pixels);prefix.integer(p.connectivity);
            // Requested auto level is signed; the actual sampled level above is nonnegative.
            prefix.separator();prefix.text+=std::to_string(p.level);
            for(auto v:{std::uint64_t(p.tile_size),std::uint64_t(p.max_cells),std::uint64_t(p.work_bytes),std::uint64_t(p.contours.clip_to_roi),std::uint64_t(p.contours.max_components),std::uint64_t(p.contours.max_edges),std::uint64_t(p.contours.max_vertices),std::uint64_t(p.contours.work_bytes)}) prefix.integer(v);
            prefix.vector(n.white());for(const auto& v:n.stains()) prefix.vector(v);
            for(const auto& f:result.features()) {
                export_checkpoint(cancelled);Row row;row.text=prefix.text;row.first=false;
                row.integer(f.label);row.integer(f.nucleus_pixels);row.integer(f.cell_pixels);row.integer(f.cell_pixels-f.nucleus_pixels);
                const auto& a=nuclei.at(f.label-1);const auto& b=bodies.at(f.label-1);
                for(auto v:{a.centroid.x,a.centroid.y,b.centroid.x,b.centroid.y}) row.real(v);
                row.vector(f.nucleus_rgb);row.vector(f.cell_rgb);row.optional(f.cytoplasm_rgb);row.vector(f.nucleus_channels);row.vector(f.cell_channels);row.optional(f.cytoplasm_channels);
                sink(row.finish());++done;if(rows) rows->store(done,std::memory_order_relaxed);
            }
        }
        export_checkpoint(cancelled);
    }
    static void save(const CellTableRequest& request,const std::string& path,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* rows=nullptr,std::atomic<std::size_t>* bytes=nullptr) {
        detail::AtomicExportFile file(path,request.options().byte_limit,cancelled);
        // Buffer a bounded group of rows instead of issuing a write per cell.
        std::string buffer;buffer.reserve(32768);std::size_t produced=0;
        auto flush=[&] {if(!buffer.empty()) {file.write(buffer.data(),buffer.size());buffer.clear();}};
        run(request,[&](const std::string& row) {
            if(row.size()>request.options().byte_limit-produced) throw std::length_error("cell table exceeds byte limit");
            produced+=row.size();if(buffer.size()+row.size()>32768) flush();buffer+=row;if(bytes) bytes->store(produced,std::memory_order_relaxed);
        },cancelled,rows);
        flush();file.commit();
    }
};
std::string cell_measurements_csv(const CellTableRequest& request,const std::atomic<bool>* cancelled) {
    std::string result;CellTableWriter::run(request,[&](const std::string& row) {if(row.size()>request.options().byte_limit-result.size()) throw std::length_error("cell table exceeds byte limit");result+=row;},cancelled);return result;
}
void save_cell_measurements_csv(const CellTableRequest& request,const std::string& path,const std::atomic<bool>* cancelled) {CellTableWriter::save(request,path,cancelled);}
struct CellTableExportJob::Impl {
    struct Completion {bool committed=false,cancelled=false;std::string error;double write_ms=0;};
    std::future<Completion> future;std::shared_ptr<std::atomic<bool>> cancelled;
    std::atomic<std::uint64_t> rows{0};std::atomic<std::size_t> bytes{0};CellTableExportStats stats;
};
CellTableExportJob::CellTableExportJob():impl_(std::make_unique<Impl>()) {}
CellTableExportJob::~CellTableExportJob() {try {wait();} catch(...) {}}
bool CellTableExportJob::busy() const {return impl_->future.valid();}
void CellTableExportJob::cancel() {if(impl_->cancelled) impl_->cancelled->store(true);}
bool CellTableExportJob::request(const CellTableRequest& request,const std::string& path) {
    poll();if(busy()) return false;if(impl_->stats.completed==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("cell table export counter exhausted");
    auto destination=detail::export_path(path);auto cancelled=std::make_shared<std::atomic<bool>>(false);
    impl_->rows.store(0);impl_->bytes.store(0);
    auto future=std::async(std::launch::async,[request,destination,cancelled,state=impl_.get()] {
        Impl::Completion result;const auto start=std::chrono::steady_clock::now();
        try {CellTableWriter::save(request,destination,cancelled.get(),&state->rows,&state->bytes);result.committed=true;}
        catch(const std::exception& e) {result.error=e.what();result.cancelled=cancelled->load();}
        result.write_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return result;
    });
    const auto completed=impl_->stats.completed;impl_->stats={};impl_->stats.completed=completed;impl_->stats.state="writing";impl_->stats.path=std::move(destination);
    impl_->stats.total_rows=request.row_count();impl_->stats.document_revision=request.document_revision();impl_->stats.snapshot_bytes=request.snapshot_bytes();impl_->stats.capture_ms=request.capture_ms();
    impl_->cancelled=std::move(cancelled);impl_->future=std::move(future);return true;
}
CellTableExportStats CellTableExportJob::poll() {
    impl_->stats.rows=impl_->rows.load(std::memory_order_relaxed);impl_->stats.bytes=impl_->bytes.load(std::memory_order_relaxed);
    if(busy() && impl_->cancelled->load()) impl_->stats.state="cancelling";
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=impl_->future.get();impl_->stats.write_ms=result.write_ms;impl_->stats.error=std::move(result.error);
            if(result.committed) {impl_->stats.state="succeeded";++impl_->stats.completed;}
            else impl_->stats.state=result.cancelled?"cancelled":"failed";
        } catch(const std::exception& e) {impl_->stats.state="failed";impl_->stats.error=e.what();}
        impl_->stats.rows=impl_->rows.load();impl_->stats.bytes=impl_->bytes.load();
    }
    return impl_->stats;
}
CellTableExportStats CellTableExportJob::wait() {if(busy()) impl_->future.wait();return poll();}
}
