#include "qupathxx/cell_tables.hpp"
#include "cell_policy_names.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include "object_snapshot.hpp"
#include "cell_object_status.hpp"
#include "qupathxx/provenance.hpp"
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
    struct Image {
        std::uint64_t id=0,revision=0,width=0,height=0;std::string name;std::optional<double> x,y;
        bool filtered=false;
        std::map<std::uint64_t,std::size_t> runs;
        std::map<std::string,std::uint32_t> colors;
    };
    struct Entry {std::shared_ptr<const CellRecord> record;std::string status;std::size_t image=0;};
    struct Block {std::shared_ptr<const Document::ObjectBlock> value;std::size_t image=0;};
    std::vector<Entry> entries;std::vector<Image> images;std::vector<Block> blocks;CellTableOptions options;
    std::string identity;bool project=false,detector_parameters=false,grid_parameters=false,nominal_parameters=false,processing_parameters=false,sampling_parameters=false;std::uint64_t revision=0,generation=0,rows=0;std::size_t bytes=0;double capture_ms=0;
    void charge(std::size_t value) {if(value>options.snapshot_budget-bytes) throw std::length_error("cell table snapshot exceeds memory budget");bytes+=value;}
    void append(const Document& doc,std::uint64_t id,std::string name,const std::vector<std::uint64_t>& record_ids={}) {
        if(options.published_objects) {append_objects(doc,id,std::move(name),record_ids);return;}
        std::vector<std::shared_ptr<const CellRecord>> records;
        if(record_ids.empty()) records=doc.cell_records();else for(auto record:record_ids) records.push_back(doc.cell_record(record));
        charge(sizeof(Image)+name.capacity());const auto image=images.size();
        images.push_back({id,doc.revision(),std::uint64_t(doc.metadata().width),std::uint64_t(doc.metadata().height),std::move(name)});
        for(const auto& record:records) {
            auto status=doc.cell_status(record->id);if(options.require_current && status!="current") throw std::invalid_argument("cell table requires current verified records");
            charge(sizeof(Entry)+record->payload_bytes()+status.capacity());rows+=record->result->features().size();entries.push_back({record,std::move(status),image});
        }
    }
    void append_objects(const Document& doc,std::uint64_t id,std::string name,const std::vector<std::uint64_t>& record_ids) {
        for(auto record:record_ids) doc.cell_record(record);
        charge(sizeof(Image)+name.capacity());const auto image=images.size();
        images.push_back({id,doc.revision(),std::uint64_t(doc.metadata().width),std::uint64_t(doc.metadata().height),std::move(name),doc.pixel_width_um(),doc.pixel_height_um()});
        const auto missing=std::numeric_limits<std::size_t>::max();auto& captured=images.back();captured.filtered=!record_ids.empty();
        const std::string* last_class=nullptr;std::uint64_t last_run=0;std::size_t last_entry=missing;
        auto selected=[&](const Annotation& a) {return a.kind==ObjectKind::Cell && (record_ids.empty() || (a.cell_feature_link && std::binary_search(record_ids.begin(),record_ids.end(),a.cell_feature_link->record_id)));};
        auto append=[&](const Annotation& a) {
            if(!a.class_name.empty() && (!last_class || *last_class!=a.class_name)) {
                auto color=captured.colors.find(a.class_name);
                if(color==captured.colors.end()) {
                    charge(64+sizeof(std::pair<const std::string,std::uint32_t>)+a.class_name.size());
                    color=captured.colors.emplace(a.class_name,doc.classes().at(a.class_name).color).first;
                }
                last_class=&color->first;
            }
            if(a.cell_feature_link) {
                const auto run=a.cell_feature_link->record_id;
                if(run!=last_run) {
                    auto found=captured.runs.find(run);
                    if(found==captured.runs.end()) {
                        auto index=missing;const auto live=doc.cells_.find(run);
                        if(live!=doc.cells_.end()) {
                            const auto record=live->second.value;auto state=doc.cell_status(run);
                            charge(sizeof(Entry)+record->payload_bytes()+state.capacity());index=entries.size();entries.push_back({record,std::move(state),image});
                        }
                        charge(64+sizeof(std::pair<const std::uint64_t,std::size_t>));found=captured.runs.emplace(run,index).first;
                    }
                    last_run=run;last_entry=found->second;
                }
                if(last_entry!=missing) {
                    const auto& entry=entries[last_entry];const auto& result=*entry.record->result;
                    if(!a.cell_feature_link->label || a.cell_feature_link->label>result.features().size()) throw std::invalid_argument("invalid cell object feature label");
                    if(options.require_current && detail::cell_geometry_status(a,result,entry.status)!="current") throw std::invalid_argument("cell object table requires current verified measurements and unchanged geometry");
                } else if(options.require_current) throw std::invalid_argument("cell object table requires current verified measurements and unchanged geometry");
            } else if(options.require_current) throw std::invalid_argument("cell object table requires current verified measurements and unchanged geometry");
            ++rows;
        };
#ifdef QUPATHXX_HAS_PROJECT
        // Immutable canonical blocks already carry owned names/geometry and cached
        // payload sizes. Retain only blocks containing included cells.
        for(const auto& item:doc.object_blocks_) {
            const auto& block=item.second;bool kept=false;
            for(const auto& a:block->values) if(selected(a)) {
                if(!kept) {charge(sizeof(Block)+block->bytes);blocks.push_back({block,image});kept=true;}
                append(a);
            }
        }
#else
        // Storage-disabled builds have no published block index. Build a bounded
        // private block of selected values, never retain pointers to live nodes.
        std::size_t count=0;for(const auto& item:doc.objects()) if(selected(item.second)) ++count;
        if(count) {
            charge(sizeof(Document::ObjectBlock)+64+count*sizeof(Annotation)+sizeof(Block));
            auto block=std::make_shared<Document::ObjectBlock>();block->values.reserve(count);
            for(const auto& item:doc.objects()) if(selected(item.second)) {
                const auto& a=item.second;charge(64+a.name.size()+a.class_name.size()+a.geometry.payload_bytes()+(a.nucleus?a.nucleus->payload_bytes():0));block->values.push_back(a);
            }
            for(const auto& a:block->values) append(a);blocks.push_back({std::move(block),image});
        }
#endif
    }
    void finish(std::chrono::steady_clock::time_point start) {
        for(const auto& entry:entries) {
            const bool grid=entry.record->result->grid_parameters()!=nullptr;grid_parameters=grid_parameters || grid;
            nominal_parameters=nominal_parameters || bool(entry.record->result->nuclei()->nominal_sampling());
            const auto* nominal=entry.record->result->nuclei()->nominal_sampling();
            sampling_parameters=sampling_parameters || (nominal && nominal->source_request().sampling!=RgbNominalSampling::IntegerOrigin);
            processing_parameters=processing_parameters || sampling_parameters || (grid && entry.record->result->grid_parameters()->processing!=CellProcessingPolicy::Quantitative);
            detector_parameters=detector_parameters || grid || entry.record->result->parameters().algorithm!=CellAlgorithm::DistanceWatershed;
        }
        charge((entries.capacity()-entries.size())*sizeof(Entry)+(images.capacity()-images.size())*sizeof(Image)+(blocks.capacity()-blocks.size())*sizeof(Block));
        capture_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    }
};
void CellTableOptions::validate() const {
    if(!byte_limit || byte_limit>256*megabyte) throw std::invalid_argument("cell table byte limit must be 1 byte..256 MiB");
    if(!snapshot_budget || snapshot_budget>512*megabyte) throw std::invalid_argument("cell table snapshot budget must be 1 byte..512 MiB");
}
CellTableRequest::CellTableRequest(const Document& doc,const std::vector<std::uint64_t>& ids,CellTableOptions options) {
    const auto start=std::chrono::steady_clock::now();options.validate();if(ids.size()>10000) throw std::length_error("cell table selection exceeds 10000 record IDs");
    auto data=std::make_shared<Data>();data->options=options;data->revision=doc.revision();data->charge(sizeof(Data)+65536);
    auto chosen=ids;std::sort(chosen.begin(),chosen.end());chosen.erase(std::unique(chosen.begin(),chosen.end()),chosen.end());
    data->append(doc,0,{},chosen);data->finish(start);data_=std::move(data);
}
CellTableRequest::CellTableRequest(const Project& project,const std::vector<std::uint64_t>& ids,CellTableOptions options) {
    const auto start=std::chrono::steady_clock::now();options.validate();if(ids.size()>10000) throw std::length_error("cell table selection exceeds 10000 image IDs");
    if(ids.empty() && project.entries_.size()>10000) throw std::length_error("cell table selection exceeds 10000 images");
    auto chosen=ids;if(chosen.empty()) for(const auto& entry:project.entries_) chosen.push_back(entry.first);
    std::sort(chosen.begin(),chosen.end());chosen.erase(std::unique(chosen.begin(),chosen.end()),chosen.end());
    for(auto id:chosen) project.entry(id); // Validate the complete selection first.
    auto data=std::make_shared<Data>();data->options=options;data->project=true;data->identity=project.identity_;data->generation=project.generation_;data->revision=project.revision_;
    data->charge(sizeof(Data)+65536+data->identity.capacity());
    for(auto id:chosen) {const auto& entry=project.entry(id);data->append(*entry.document,id,entry.info.name);}
    data->finish(start);data_=std::move(data);
}
std::size_t CellTableRequest::record_count() const {return data_->entries.size();}
std::uint64_t CellTableRequest::row_count() const {return data_->rows;}
std::size_t CellTableRequest::snapshot_bytes() const {return data_->bytes;}
double CellTableRequest::capture_ms() const {return data_->capture_ms;}
std::uint64_t CellTableRequest::document_revision() const {return data_->project?0:data_->revision;}
bool CellTableRequest::project_scope() const {return data_->project;}
std::size_t CellTableRequest::image_count() const {return data_->images.size();}
std::string CellTableRequest::project_identity() const {return data_->identity;}
std::uint64_t CellTableRequest::project_generation() const {return data_->generation;}
std::uint64_t CellTableRequest::project_revision() const {return data_->project?data_->revision:0;}
CellTableOptions CellTableRequest::options() const {return data_->options;}
struct CellTableWriter {
    static std::string schema(const CellTableRequest& request) {
        const auto version=std::to_string(request.data_->sampling_parameters?(request.data_->options.include_capture_calibration?12:11):request.data_->processing_parameters?(request.data_->options.include_capture_calibration?10:9):request.data_->nominal_parameters?(request.data_->options.include_capture_calibration?8:7):request.data_->grid_parameters?(request.data_->options.include_capture_calibration?6:5):
            (request.data_->options.include_capture_calibration?2:1)+(request.data_->detector_parameters?2:0));
        if(request.data_->options.published_objects) return std::string(request.data_->project?"qpx.project-cell-object-table/":"qpx.cell-object-table/")+version;
        return std::string(request.data_->project?"qpx.project-cell-table/":"qpx.cell-table/")+version;
    }
    static void detector(Row& row,const CellTableRequest& request,const CellResult* result) {
        if(!request.data_->detector_parameters) return;
        if(!result) {row.separator();row.separator();row.separator();return;}
        if(result->grid_parameters()) {row.string(result->nuclei()->nominal_sampling()?"nominal-log-bodies":"grid-log-bodies");row.separator();row.separator();return;}
        const auto& p=result->parameters();row.string(p.algorithm==CellAlgorithm::SampledLogWatershedBodies?"log-sampled":(p.algorithm==CellAlgorithm::LogWatershedBodies?"log-bodies":(p.algorithm==CellAlgorithm::LogWatershed?"log":"distance")));
        if(p.algorithm!=CellAlgorithm::DistanceWatershed) {row.real(p.log_prominence);row.integer(p.context_padding);}
        else {row.separator();row.separator();}
    }
    static void context(Row& row,const CellTableRequest& request,const CellTableRequest::Data::Image& image) {
        if(request.data_->project) {row.string(request.data_->identity);row.integer(request.data_->generation);row.integer(request.data_->revision);row.integer(image.id);row.string(image.name);}
    }
    static Row prefix(const CellTableRequest& request,const CellTableRequest::Data::Entry& entry) {
        const auto& image=request.data_->images.at(entry.image);
        const auto& record=*entry.record;const auto& result=*record.result;const auto& n=*result.nuclei();
        Row row;context(row,request,image);
        row.string(schema(request));row.integer(record.id);row.integer(n.object_id());row.integer(n.object_revision());row.integer(n.document_revision());row.integer(image.revision);
        row.string(entry.status);row.string(record.producer);row.string(result.algorithm());row.string(n.input_sha256());row.integer(image.width);row.integer(image.height);row.integer(n.level());
        if(result.grid_parameters()) {
            const auto& p=*result.grid_parameters();const auto& q=*result.resolved_grid_parameters();row.separator();
            row.string(feature_name(p.feature));row.string(p.feature==TissueFeature::OpticalDensitySum?"optical_density_rgb":"stain_concentrations");row.real(p.threshold);
            for(unsigned i=0;i<4;++i) row.separator(); // Legacy sampled distance fields have no grid-unit meaning.
            row.integer(q.min_nucleus_samples);row.integer(q.max_nucleus_samples);
            for(unsigned i=0;i<3;++i) row.separator(); // Legacy connectivity/requested native level/tile size.
            for(auto v:{std::uint64_t(p.max_cells),std::uint64_t(p.work_bytes),std::uint64_t(p.contours.clip_to_roi),std::uint64_t(p.contours.max_components),std::uint64_t(p.contours.max_edges),std::uint64_t(p.contours.max_vertices),std::uint64_t(p.contours.work_bytes)}) row.integer(v);
            row.vector(n.white());for(const auto& v:n.stains()) row.vector(v);return row;
        }
        const auto& p=result.parameters();row.real(n.downsample());
        row.string(feature_name(p.feature));row.string(p.feature==TissueFeature::OpticalDensitySum?"optical_density_rgb":"stain_concentrations");
        for(auto v:{p.threshold,p.sigma,p.min_peak_radius,p.min_seed_distance,p.cell_expansion}) row.real(v);
        row.integer(p.min_nucleus_pixels);row.integer(p.max_nucleus_pixels);row.integer(p.connectivity);
        row.separator();row.text+=std::to_string(p.level);
        for(auto v:{std::uint64_t(p.tile_size),std::uint64_t(p.max_cells),std::uint64_t(p.work_bytes),std::uint64_t(p.contours.clip_to_roi),std::uint64_t(p.contours.max_components),std::uint64_t(p.contours.max_edges),std::uint64_t(p.contours.max_vertices),std::uint64_t(p.contours.work_bytes)}) row.integer(v);
        row.vector(n.white());for(const auto& v:n.stains()) row.vector(v);return row;
    }
    static const std::vector<const char*>& grid_columns() {
        static const std::vector<const char*> names={"sampling_grid","sample_step_x_px","sample_step_y_px","grid_image_width_px","grid_image_height_px",
            "grid_origin_x_px","grid_origin_y_px","grid_extent_width_px","grid_extent_height_px","grid_samples_x","grid_samples_y","grid_context_x","grid_context_y",
            "grid_parameter_units","grid_sigma","grid_peak_prominence","grid_min_peak_radius","grid_min_seed_distance","grid_cell_expansion","grid_min_nucleus_area","grid_max_nucleus_area",
            "grid_resolved_step_x","grid_resolved_step_y","grid_sigma_x_samples","grid_sigma_y_samples","grid_sampling_policy","grid_capture_pixel_width_um","grid_capture_pixel_height_um","grid_input_format"};
        return names;
    }
    static void grid(Row& row,const CellTableRequest& request,const CellResult* result) {
        if(!request.data_->grid_parameters) return;
        if(!result) {for(std::size_t i=0;i<grid_columns().size()+std::size_t(request.data_->processing_parameters)+std::size_t(request.data_->sampling_parameters);++i) row.separator();return;}
        const auto& n=*result->nuclei();row.string(n.nominal_sampling()?"nominal_rgb":result->grid_parameters()?"explicit":"natural");row.real(n.downsample_x());row.real(n.downsample_y());
        if(!result->grid_parameters()) {for(std::size_t i=3;i<grid_columns().size()+std::size_t(request.data_->processing_parameters)+std::size_t(request.data_->sampling_parameters);++i) row.separator();return;}
        const auto& p=*result->grid_parameters();const auto& q=*result->resolved_grid_parameters();
        auto signed_integer=[&](std::int64_t value){row.separator();row.text+=std::to_string(value);};
        if(const auto* nominal=n.nominal_sampling()) {
            const auto& r=nominal->source_request();const auto& source=n.provenance()->source;
            for(auto v:{source.width,source.height,r.x_px,r.y_px,r.width_px,r.height_px,nominal->core_width(),nominal->core_height(),std::int64_t(nominal->context_x()),std::int64_t(nominal->context_y())}) signed_integer(v);
        } else {
            const auto& g=*n.scientific_grid();
            for(auto v:{g.image_width(),g.image_height(),g.x().origin(),g.y().origin(),g.x().extent(),g.y().extent(),g.x().samples(),g.y().samples(),g.x().context(),g.y().context()}) signed_integer(v);
        }
        row.string(p.units==GridCellUnits::Micrometres?"um":"level_zero_px");
        for(auto v:{p.sigma,p.log_prominence,p.min_peak_radius,p.min_seed_distance,p.cell_expansion,p.min_nucleus_area}) row.real(v);
        optional(row,p.max_nucleus_area);for(auto v:{q.step_x,q.step_y,q.sigma_x,q.sigma_y}) row.real(v);
        row.string(p.sampling_policy==RoiSamplingPolicy::GeometryCoverage?"geometry_coverage":"polygon_winding");
        const auto& provenance=n.provenance();optional(row,provenance?provenance->effective_pixel_width_um:std::optional<double>{});optional(row,provenance?provenance->effective_pixel_height_um:std::optional<double>{});
        row.string(n.input_format());
        if(request.data_->processing_parameters) row.string(cell_detail::processing_name(p.processing));
        if(request.data_->sampling_parameters) row.string(n.nominal_sampling()?n.nominal_sampling()->source_request().algorithm():n.input_format());
    }
    static void feature(Row& row,const CellResult& result,std::uint32_t label,bool calibrated) {
        const auto& f=result.features().at(label-1);const auto& a=result.nuclei()->components().at(label-1);const auto& b=result.cells()->components().at(label-1);
        row.integer(f.label);row.integer(f.nucleus_pixels);row.integer(f.cell_pixels);row.integer(f.cell_pixels-f.nucleus_pixels);
        for(auto v:{a.centroid.x,a.centroid.y,b.centroid.x,b.centroid.y}) row.real(v);
        row.vector(f.nucleus_rgb);row.vector(f.cell_rgb);row.optional(f.cytoplasm_rgb);row.vector(f.nucleus_channels);row.vector(f.cell_channels);row.optional(f.cytoplasm_channels);
        if(calibrated) {
            const auto value=result.calibrated_feature(label);
            optional(row,value.pixel_width_um);optional(row,value.pixel_height_um);
            optional(row,value.nucleus_sample_area_um2);optional(row,value.cell_sample_area_um2);optional(row,value.cytoplasm_sample_area_um2);
            for(const auto& point:{value.nucleus_centroid_um,value.cell_centroid_um}) {
                if(point) {row.real(point->x);row.real(point->y);} else {row.separator();row.separator();}
            }
        }
    }
    static void optional(Row& row,std::optional<double> value) {if(value) row.real(*value);else row.separator();}
    static double area(double value,double x,double y) {
        auto result=value*x*y;
        if(!std::isfinite(result) || (result==0 && value!=0)) {
            int a=0,b=0,c=0;const auto mantissa=std::frexp(value,&a)*std::frexp(x,&b)*std::frexp(y,&c);result=std::scalbn(mantissa,a+b+c);
        }
        return result; // Row::real rejects an unrepresentable final result.
    }
    static void object(Row& row,const Annotation& a,const std::string& status,const CellTableRequest::Data::Image& image) {
        row.integer(a.id);row.integer(a.revision);row.string(a.name);row.integer(a.parent_id);row.string(a.class_name);
        if(!a.class_name.empty()) row.integer(image.colors.at(a.class_name));else row.separator();row.string(status);
        const auto b=a.geometry.bounds();for(auto v:{b.x,b.y,b.width,b.height,a.geometry.area(),a.geometry.perimeter()}) row.real(v);
        if(a.nucleus) {row.real(a.nucleus->area());row.real(a.nucleus->perimeter());}else {row.separator();row.separator();}
        optional(row,image.x);optional(row,image.y);
        if(image.x && image.y) {
            row.real(area(a.geometry.area(),*image.x,*image.y));row.real(*image.x==*image.y?a.geometry.perimeter()* *image.x:a.geometry.scaled_length(*image.x,*image.y));
            if(a.nucleus) {row.real(area(a.nucleus->area(),*image.x,*image.y));row.real(*image.x==*image.y?a.nucleus->perimeter()* *image.x:a.nucleus->scaled_length(*image.x,*image.y));}else {row.separator();row.separator();}
        } else for(int i=0;i<4;++i) row.separator();
    }
    template<class Sink> static void run(const CellTableRequest& request,Sink&& sink,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* rows=nullptr) {
        export_checkpoint(cancelled);
        std::string columns=request.data_->project?std::string("project_identity,project_generation,project_revision,image_id,image_name,")+header():header();
        if(request.data_->options.include_capture_calibration) {
            columns.resize(columns.size()-2);
            columns+=",capture_pixel_width_um,capture_pixel_height_um,nucleus_sample_area_um2,cell_sample_area_um2,cytoplasm_sample_area_um2,nucleus_centroid_x_um,nucleus_centroid_y_um,cell_centroid_x_um,cell_centroid_y_um\r\n";
        }
        if(request.data_->options.published_objects) {
            columns.resize(columns.size()-2);
            columns+=",object_id,object_revision,object_name,object_parent_id,object_class,object_class_color,object_status_at_capture,object_min_x_px,object_min_y_px,object_width_px,object_height_px,object_area_px2,object_length_px,object_nucleus_area_px2,object_nucleus_length_px,export_pixel_width_um,export_pixel_height_um,object_area_um2,object_length_um,object_nucleus_area_um2,object_nucleus_length_um\r\n";
        }
        if(request.data_->detector_parameters) {columns.resize(columns.size()-2);columns+=",detector_mode,log_peak_prominence,context_padding_sample_px\r\n";}
        if(request.data_->grid_parameters) {columns.resize(columns.size()-2);for(const auto* name:grid_columns()) {columns+=',';columns+=name;}if(request.data_->processing_parameters) columns+=",processing_policy";if(request.data_->sampling_parameters) columns+=",sampling_algorithm";columns+="\r\n";}
        sink(columns);std::uint64_t done=0;
        if(request.data_->options.published_objects) {
            const auto missing=std::numeric_limits<std::size_t>::max();std::size_t cached=missing;Row saved_prefix;
            for(const auto& block:request.data_->blocks) {
                export_checkpoint(cancelled);const auto& image=request.data_->images.at(block.image);
                std::uint64_t last_run=0;std::size_t entry=missing;
                for(const auto& a:block.value->values) {
                    export_checkpoint(cancelled);if(a.kind!=ObjectKind::Cell) continue;
                    if(image.filtered && (!a.cell_feature_link || !image.runs.count(a.cell_feature_link->record_id))) continue;
                    std::string status="no_features";Row row;
                    if(a.cell_feature_link) {
                        if(last_run!=a.cell_feature_link->record_id) {last_run=a.cell_feature_link->record_id;entry=image.runs.at(last_run);}
                        status=entry==missing?"archive_missing":detail::cell_geometry_status(a,*request.data_->entries[entry].record->result,request.data_->entries[entry].status);
                    }
                    if(a.cell_feature_link && entry!=missing) {
                        if(cached!=entry) {saved_prefix=prefix(request,request.data_->entries.at(entry));cached=entry;}
                        row=saved_prefix;feature(row,*request.data_->entries[entry].record->result,a.cell_feature_link->label,request.data_->options.include_capture_calibration);
                    } else {
                        context(row,request,image);
                        // Explicit blanks preserve unresolved/manual objects without invented features.
                        for(int column=0;column<71;++column) {
                            if(column==0) row.string(schema(request));
                            else if(column==1 && a.cell_feature_link) row.integer(a.cell_feature_link->record_id);
                            else if(column==5) row.integer(image.revision);
                            else if(column==6) row.string(status);
                            else if(column==10) row.integer(image.width);
                            else if(column==11) row.integer(image.height);
                            else if(column==45 && a.cell_feature_link) row.integer(a.cell_feature_link->label);
                            else row.separator();
                        }
                        if(request.data_->options.include_capture_calibration) for(int column=0;column<9;++column) row.separator();
                    }
                    const auto result=a.cell_feature_link && entry!=missing?request.data_->entries[entry].record->result.get():nullptr;
                    object(row,a,status,image);detector(row,request,result);grid(row,request,result);sink(row.finish());++done;if(rows) rows->store(done,std::memory_order_relaxed);
                }
            }
        } else for(const auto& entry:request.data_->entries) {
            export_checkpoint(cancelled);if(entry.record->result->features().empty()) continue;
            const auto saved_prefix=prefix(request,entry);
            for(const auto& f:entry.record->result->features()) {
                export_checkpoint(cancelled);Row row;row.text=saved_prefix.text;row.first=false;feature(row,*entry.record->result,f.label,request.data_->options.include_capture_calibration);
                detector(row,request,entry.record->result.get());grid(row,request,entry.record->result.get());sink(row.finish());++done;if(rows) rows->store(done,std::memory_order_relaxed);
            }
        }
        if(done!=request.row_count()) throw std::logic_error("cell table row count mismatch");
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
void CellTableExportJob::await_completion() {if(busy()) impl_->future.wait();}
CellTableExportStats CellTableExportJob::wait() {await_completion();return poll();}
}
