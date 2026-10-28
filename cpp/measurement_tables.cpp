#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <type_traits>
namespace qpx {
namespace {
using detail::export_checkpoint;
constexpr std::size_t megabyte=1024*1024;
struct Row {
    std::string text;bool first=true;
    Row() {text.reserve(4096);}
    void separator() {if(!first) text.push_back(',');first=false;}
    void string(const std::string& value) {separator();text.push_back('"');for(char c:value) {if(c=='"') text.push_back('"');text.push_back(c);}text.push_back('"');}
    void integer(std::uint64_t value) {separator();char buffer[32];auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);if(result.ec!=std::errc{}) throw std::overflow_error("measurement table integer formatting failed");text.append(buffer,result.ptr);}
    void real(double value) {if(!std::isfinite(value)) throw std::invalid_argument("measurement table contains nonfinite features");separator();char buffer[128];auto result=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::general,std::numeric_limits<double>::max_digits10);if(result.ec!=std::errc{}) throw std::overflow_error("measurement table real formatting failed");text.append(buffer,result.ptr);}
    void vector(const ColorVector& value) {for(auto v:value) real(v);}
    void optional(const std::optional<ColorVector>& value) {if(value) vector(*value);else for(int i=0;i<3;++i) separator();}
    const std::string& finish() {text+="\r\n";return text;}
};

const char* status_name(std::uint8_t value) {
    static const char* names[]={"current","object_changed","unverified","source_changed","source_closed","verification_failed"};
    if(value>=6) throw std::logic_error("invalid captured measurement status");return names[value];
}
std::uint8_t status_code(const std::string& value) {
    for(std::uint8_t i=0;i<6;++i) if(value==status_name(i)) return i;
    throw std::logic_error("unknown measurement archive status");
}
const char* feature_name(TissueFeature feature) {
    switch(feature) {case TissueFeature::OpticalDensitySum:return "optical_density_sum";case TissueFeature::Stain1:return "stain_1";case TissueFeature::Stain2:return "stain_2";case TissueFeature::Stain3:return "stain_3";}throw std::invalid_argument("invalid tissue feature");
}
std::vector<std::uint64_t> selection(const std::vector<std::uint64_t>& ids,std::size_t limit) {
    if(ids.size()>limit) throw std::length_error("measurement table selection exceeds ID limit");
    auto chosen=ids;std::sort(chosen.begin(),chosen.end());chosen.erase(std::unique(chosen.begin(),chosen.end()),chosen.end());return chosen;
}
}
struct MeasurementTableRequest::Data {
    struct Image {std::uint64_t id=0,revision=0;std::string name;};
    struct Entry {
        std::shared_ptr<const AnalysisRecord> roi;
        std::shared_ptr<const TissueRecord> tissue;
        std::size_t image=0;std::uint8_t status=0;
    };
    MeasurementTableKind kind;MeasurementTableOptions options;
    std::vector<Image> images;std::vector<Entry> entries;
    bool project=false;std::string identity;std::uint64_t revision=0,generation=0,rows=0;
    std::size_t bytes=0;double capture_ms=0;
    void charge(std::size_t value) {if(value>options.snapshot_budget-bytes) throw std::length_error("measurement table snapshot exceeds memory budget");bytes+=value;}
    void append(const Document& doc,std::uint64_t id,std::string name,const std::vector<std::uint64_t>& ids={}) {
        charge(sizeof(Image)+name.capacity());const auto index=images.size();images.push_back({id,doc.revision(),std::move(name)});
        auto keep=[&](auto record,const std::string& status) {
            if(options.require_current && status!="current") throw std::invalid_argument("measurement table requires current verified records");
            charge(sizeof(Entry)+record->payload_bytes());Entry e;e.image=index;e.status=status_code(status);
            if constexpr(std::is_same_v<decltype(record),std::shared_ptr<const AnalysisRecord>>) {e.roi=std::move(record);++rows;}
            else {e.tissue=std::move(record);const auto count=std::max<std::size_t>(1,e.tissue->result->components().size());if(count>std::numeric_limits<std::uint64_t>::max()-rows) throw std::overflow_error("measurement row count overflow");rows+=count;}
            entries.push_back(std::move(e));
        };
        if(kind==MeasurementTableKind::Roi) {
            if(ids.empty()) for(const auto& [key,stored]:doc.analysis_) keep(stored.value,doc.analysis_status(key));
            else for(auto key:ids) {auto found=doc.analysis_.find(key);if(found==doc.analysis_.end()) throw std::out_of_range("analysis record does not exist");keep(found->second.value,doc.analysis_status(key));}
        } else {
            if(ids.empty()) for(const auto& [key,stored]:doc.tissue_) keep(stored.value,doc.tissue_status(key));
            else for(auto key:ids) keep(doc.tissue_record(key),doc.tissue_status(key));
        }
    }
    void finish(std::chrono::steady_clock::time_point start) {
        charge((entries.capacity()-entries.size())*sizeof(Entry)+(images.capacity()-images.size())*sizeof(Image));
        capture_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    }
};
void MeasurementTableOptions::validate() const {
    if(!byte_limit || byte_limit>256*megabyte) throw std::invalid_argument("measurement table byte limit must be 1 byte..256 MiB");
    if(!snapshot_budget || snapshot_budget>512*megabyte) throw std::invalid_argument("measurement table snapshot budget must be 1 byte..512 MiB");
}
MeasurementTableRequest::MeasurementTableRequest(const Document& doc,MeasurementTableKind kind,const std::vector<std::uint64_t>& ids,MeasurementTableOptions options) {
    const auto start=std::chrono::steady_clock::now();options.validate();if(kind!=MeasurementTableKind::Roi && kind!=MeasurementTableKind::Tissue) throw std::invalid_argument("invalid measurement table kind");
    auto chosen=selection(ids,100000);auto data=std::make_shared<Data>();data->kind=kind;data->options=options;data->revision=doc.revision();data->charge(sizeof(Data)+65536);
    data->append(doc,0,{},chosen);data->finish(start);data_=std::move(data);
}
MeasurementTableRequest::MeasurementTableRequest(const Project& project,MeasurementTableKind kind,const std::vector<std::uint64_t>& ids,MeasurementTableOptions options) {
    const auto start=std::chrono::steady_clock::now();options.validate();if(kind!=MeasurementTableKind::Roi && kind!=MeasurementTableKind::Tissue) throw std::invalid_argument("invalid measurement table kind");
    if(ids.empty() && project.entries_.size()>10000) throw std::length_error("measurement table exceeds 10000 images");
    auto chosen=selection(ids,10000);if(ids.empty()) for(const auto& [key,value]:project.entries_) chosen.push_back(key);
    for(auto key:chosen) project.entry(key);
    auto data=std::make_shared<Data>();data->kind=kind;data->options=options;data->project=true;data->identity=project.identity_;data->generation=project.generation_;data->revision=project.revision_;
    data->charge(sizeof(Data)+65536+data->identity.capacity());
    for(auto key:chosen) {const auto& entry=project.entry(key);data->append(*entry.document,key,entry.info.name);}
    data->finish(start);data_=std::move(data);
}
MeasurementTableKind MeasurementTableRequest::kind() const {return data_->kind;}
std::size_t MeasurementTableRequest::record_count() const {return data_->entries.size();}
std::uint64_t MeasurementTableRequest::row_count() const {return data_->rows;}
std::size_t MeasurementTableRequest::snapshot_bytes() const {return data_->bytes;}
double MeasurementTableRequest::capture_ms() const {return data_->capture_ms;}
std::uint64_t MeasurementTableRequest::document_revision() const {return data_->project?0:data_->revision;}
bool MeasurementTableRequest::project_scope() const {return data_->project;}
std::size_t MeasurementTableRequest::image_count() const {return data_->images.size();}
std::string MeasurementTableRequest::project_identity() const {return data_->identity;}
std::uint64_t MeasurementTableRequest::project_generation() const {return data_->generation;}
std::uint64_t MeasurementTableRequest::project_revision() const {return data_->project?data_->revision:0;}
MeasurementTableOptions MeasurementTableRequest::options() const {return data_->options;}
struct MeasurementTableWriter {
    static std::string header(const MeasurementTableRequest& request) {
        std::string h=request.project_scope()?"project_identity,project_generation,project_revision,image_id,image_name,":"";
        h+="table_schema,record_id,source_object_id,source_object_revision,captured_document_revision,export_document_revision,status_at_capture,algorithm,input_sha256,nucleus,roi_kind,roi_min_x_px,roi_min_y_px,roi_width_px,roi_height_px,roi_area_px2,roi_length_px,captured_pixel_width_um,captured_pixel_height_um,roi_area_um2,roi_length_um,captured_unix_ms,source_tree_sha256,white_r,white_g,white_b,stain_1_r,stain_1_g,stain_1_b,stain_2_r,stain_2_g,stain_2_b,stain_3_r,stain_3_g,stain_3_b,";
        if(request.kind()==MeasurementTableKind::Roi) h+="channel_space,samples,tiles,tile_size,mean_channel_1,mean_channel_2,mean_channel_3,elapsed_ms\r\n";
        else h+="producer,level,downsample,detection_feature,threshold,connectivity,requested_level,open_radius_sample_px,close_radius_sample_px,fill_holes,max_hole_samples,min_component_samples,max_component_samples,tile_size,max_components,work_bytes,samples,threshold_samples,cleaned_samples,filled_samples,foreground_samples,raw_components,kept_components,component_label,component_samples,component_min_x_px,component_min_y_px,component_width_px,component_height_px,component_centroid_x_px,component_centroid_y_px,component_area_px2,component_area_um2,cleanup_ms,elapsed_ms\r\n";
        return h;
    }
    static Row prefix(const MeasurementTableRequest& request,const MeasurementTableRequest::Data::Entry& entry) {
        const auto& d=*request.data_;const auto& image=d.images.at(entry.image);const auto* t=entry.tissue?entry.tissue->result.get():nullptr;const auto* a=entry.roi?&entry.roi->result:nullptr;
        const auto& roi=a?entry.roi->roi:t->roi();const auto& provenance=a?a->provenance:t->provenance();Row row;
        if(d.project) {row.string(d.identity);row.integer(d.generation);row.integer(d.revision);row.integer(image.id);row.string(image.name);}
        row.string(std::string(d.project?"qpx.project-":"qpx.")+(a?"roi-table/1":"tissue-table/1"));
        row.integer(a?entry.roi->id:entry.tissue->id);row.integer(a?a->object_id:t->object_id());row.integer(a?a->object_revision:t->object_revision());
        row.integer(a?a->document_revision:t->document_revision());row.integer(image.revision);row.string(status_name(entry.status));
        row.string(a?a->algorithm:t->algorithm());row.string(a?a->input_sha256:t->input_sha256());row.integer(a?a->nucleus:t->nucleus());row.integer(static_cast<unsigned>(roi.kind()));
        const auto b=roi.bounds();for(auto value:{b.x,b.y,b.width,b.height,roi.area(),roi.perimeter()}) row.real(value);
        auto x=provenance?provenance->effective_pixel_width_um:std::optional<double>{};auto y=provenance?provenance->effective_pixel_height_um:std::optional<double>{};
        if(x) row.real(*x);else row.separator();if(y) row.real(*y);else row.separator();
        if(x && y) {row.real(roi.area()* *x* *y);row.real(roi.scaled_length(*x,*y));}else {row.separator();row.separator();}
        if(provenance) {row.integer(provenance->captured_unix_ms);row.string(provenance->environment->source_tree_sha256);}else {row.separator();row.separator();}
        row.vector(a?a->white:t->white());for(const auto& stain:(a?a->stains:t->stains())) row.vector(stain);return row;
    }
    template<class Sink> static void run(const MeasurementTableRequest& request,Sink&& sink,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* rows=nullptr) {
        export_checkpoint(cancelled);sink(header(request));std::uint64_t done=0;
        auto emit=[&](Row& row) {sink(row.finish());++done;if(rows) rows->store(done,std::memory_order_relaxed);};
        for(const auto& entry:request.data_->entries) {
            export_checkpoint(cancelled);auto row=prefix(request,entry);
            if(entry.roi) {
                const auto& a=entry.roi->result;row.string(a.kind==AnalysisKind::OpticalDensityMeans?"optical_density_rgb":"stain_concentrations");row.integer(a.samples);row.integer(a.tiles);row.integer(entry.roi->tile_size);row.vector(a.means);row.real(a.elapsed_ms);emit(row);
            } else {
                const auto& r=*entry.tissue->result;const auto& p=r.parameters();row.string(entry.tissue->producer);row.integer(r.level());row.real(r.downsample());row.string(feature_name(p.feature));row.real(p.threshold);row.integer(p.connectivity);
                row.separator();row.text+=std::to_string(p.level);
                for(auto value:{std::uint64_t(p.open_radius),std::uint64_t(p.close_radius),std::uint64_t(p.fill_holes),p.max_hole_pixels,p.min_pixels,p.max_pixels,std::uint64_t(p.tile_size),std::uint64_t(p.max_components),std::uint64_t(p.work_bytes),r.samples(),r.threshold_pixels(),r.cleaned_pixels(),r.filled_pixels(),r.foreground_pixels(),r.raw_components(),std::uint64_t(r.components().size())}) row.integer(value);
                const auto& provenance=r.provenance();auto x=provenance?provenance->effective_pixel_width_um:std::optional<double>{};auto y=provenance?provenance->effective_pixel_height_um:std::optional<double>{};
                const auto saved=row;
                if(r.components().empty()) {for(int i=0;i<10;++i) row.separator();row.real(r.cleanup_ms());row.real(r.elapsed_ms());emit(row);}
                for(const auto& component:r.components()) {
                    export_checkpoint(cancelled);row=saved;row.integer(component.label);row.integer(component.pixels);
                    for(auto value:{component.bounds.x,component.bounds.y,component.bounds.width,component.bounds.height,component.centroid.x,component.centroid.y}) row.real(value);
                    const double area=double(component.pixels)*r.downsample()*r.downsample();row.real(area);if(x && y) row.real(area* *x* *y);else row.separator();
                    row.real(r.cleanup_ms());row.real(r.elapsed_ms());emit(row);
                }
            }
        }
        if(done!=request.row_count()) throw std::logic_error("measurement table row count mismatch");export_checkpoint(cancelled);
    }
    static void save(const MeasurementTableRequest& request,const std::string& path,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* rows=nullptr,std::atomic<std::size_t>* bytes=nullptr) {
        detail::AtomicExportFile file(path,request.options().byte_limit,cancelled);std::string buffer;buffer.reserve(32768);std::size_t produced=0;
        auto flush=[&] {if(!buffer.empty()) {file.write(buffer.data(),buffer.size());buffer.clear();}};
        run(request,[&](const std::string& row) {if(row.size()>request.options().byte_limit-produced) throw std::length_error("measurement table exceeds byte limit");produced+=row.size();if(buffer.size()+row.size()>32768) flush();buffer+=row;if(bytes) bytes->store(produced,std::memory_order_relaxed);},cancelled,rows);
        flush();file.commit();
    }
};
std::string measurements_csv(const MeasurementTableRequest& request,const std::atomic<bool>* cancelled) {
    std::string result;MeasurementTableWriter::run(request,[&](const std::string& row) {if(row.size()>request.options().byte_limit-result.size()) throw std::length_error("measurement table exceeds byte limit");result+=row;},cancelled);return result;
}
void save_measurements_csv(const MeasurementTableRequest& request,const std::string& path,const std::atomic<bool>* cancelled) {MeasurementTableWriter::save(request,path,cancelled);}
struct MeasurementTableExportJob::Impl {
    struct Completion {bool committed=false,cancelled=false;std::string error;double write_ms=0;};
    std::future<Completion> future;std::shared_ptr<std::atomic<bool>> cancelled;
    std::atomic<std::uint64_t> rows{0};std::atomic<std::size_t> bytes{0};MeasurementTableExportStats stats;
};
MeasurementTableExportJob::MeasurementTableExportJob():impl_(std::make_unique<Impl>()) {}
MeasurementTableExportJob::~MeasurementTableExportJob() {try {wait();} catch(...) {}}
bool MeasurementTableExportJob::busy() const {return impl_->future.valid();}
void MeasurementTableExportJob::cancel() {if(impl_->cancelled) impl_->cancelled->store(true);}
bool MeasurementTableExportJob::request(const MeasurementTableRequest& request,const std::string& path) {
    poll();if(busy()) return false;if(impl_->stats.completed==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("measurement table export counter exhausted");
    auto destination=detail::export_path(path);auto cancelled=std::make_shared<std::atomic<bool>>(false);
    impl_->rows.store(0);impl_->bytes.store(0);
    auto future=std::async(std::launch::async,[request,destination,cancelled,state=impl_.get()] {
        Impl::Completion result;const auto start=std::chrono::steady_clock::now();
        try {MeasurementTableWriter::save(request,destination,cancelled.get(),&state->rows,&state->bytes);result.committed=true;}
        catch(const std::exception& e) {result.error=e.what();result.cancelled=cancelled->load();}
        result.write_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return result;
    });
    const auto completed=impl_->stats.completed;impl_->stats={};impl_->stats.completed=completed;impl_->stats.state="writing";impl_->stats.path=std::move(destination);
    impl_->stats.total_rows=request.row_count();impl_->stats.document_revision=request.document_revision();impl_->stats.snapshot_bytes=request.snapshot_bytes();impl_->stats.capture_ms=request.capture_ms();
    impl_->cancelled=std::move(cancelled);impl_->future=std::move(future);return true;
}
MeasurementTableExportStats MeasurementTableExportJob::poll() {
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
void MeasurementTableExportJob::await_completion() {if(busy()) impl_->future.wait();}
MeasurementTableExportStats MeasurementTableExportJob::wait() {await_completion();return poll();}
}
