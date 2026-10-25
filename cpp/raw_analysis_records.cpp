#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/source_identity.hpp"
#include "raw_statistics_internal.hpp"
#include "raw_analysis_internal.hpp"
#include "scientific_raster.hpp"
#include "scientific_raw_sampling_internal.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
constexpr std::size_t budget=64u*1024*1024;
bool same_file(const FileVersion& a,const FileVersion& b) {
    return a.identity_scheme==b.identity_scheme && a.identity==b.identity && a.bytes==b.bytes && a.stamp_scheme==b.stamp_scheme && a.modified_token==b.modified_token && a.change_token==b.change_token;
}
bool same_region(const RawRegionRequest& a,const RawRegionRequest& b) {
    return a.region.x==b.region.x && a.region.y==b.region.y && a.region.width==b.region.width && a.region.height==b.region.height && a.region.level==b.region.level &&
        a.first_channel==b.first_channel && a.channels==b.channels && a.z==b.z && a.t==b.t;
}
bool same_mask(const std::optional<Roi>& a,const std::optional<Roi>& b) {return a.has_value()==b.has_value() && (!a || a->equals(*b));}
void advance(std::uint64_t value) {if(value==UINT64_MAX) throw std::overflow_error("document revision exhausted");}
void calibration(std::optional<double> value) {if(value && (!std::isfinite(*value) || *value<=0)) throw std::invalid_argument("invalid raw analysis calibration");}
}
bool same_raw_grid(const std::optional<RawAnalysisGrid>& captured,const std::optional<ScientificGrid>& requested,const ScientificRawOptions& options) {
    if(captured.has_value()!=requested.has_value()) return false;
    if(!captured) return true;
    const auto& a=captured->grid;const auto& b=*requested;
    if(a.image_width()!=b.image_width() || a.image_height()!=b.image_height()) return false;
    for(const auto& pair:{std::make_pair(&a.x(),&b.x()),std::make_pair(&a.y(),&b.y())}) {
        if(pair.first->origin()!=pair.second->origin() || pair.first->extent()!=pair.second->extent() ||
           pair.first->samples()!=pair.second->samples() || pair.first->context()!=pair.second->context()) return false;
    }
    const auto& o=captured->options;
    return o.first_channel==options.first_channel && o.channels==options.channels && o.z==options.z && o.t==options.t && o.work_bytes==options.work_bytes;
}
std::size_t RawAnalysisRecord::payload_bytes() const {return sizeof(*this)+(result?result->payload_bytes():0)+(mask?mask->payload_bytes():0)+256;}
void RawAnalysisRecord::validate(const ImageMetadata& image) const {
    if(!id || !result || result->algorithm()!=(result->grid()?"qpx.raw-grid-statistics/1":"qpx.raw-roi-statistics/1") || result->document_revision()>=id || result->object_revision()>result->document_revision() ||
       (!result->object_id() && (result->object_revision() || result->nucleus() || mask)) || (result->object_id() && (!mask || !mask->is_area())) ||
       (result->grid()?(requested_tile_size!=0 || result->tile_size()!=0):(requested_tile_size<16 || requested_tile_size>1024 || result->tile_size()<16 || result->tile_size()>requested_tile_size)) || !max_pixels || max_pixels>1000000000000ULL ||
       !std::isfinite(result->elapsed_ms()) || result->elapsed_ms()<0) throw std::invalid_argument("invalid raw analysis record metadata");
    const auto& context=result->object_context();detail::bounded_utf8(context.name,4096);detail::bounded_utf8(context.class_name,4096);
    if(int(context.kind)<0 || int(context.kind)>2 || (result->object_id() && context.parent_id==result->object_id()) || (!result->object_id() && (!context.name.empty() || !context.class_name.empty() || context.parent_id || context.tma || context.kind!=ObjectKind::Annotation))) throw std::invalid_argument("invalid raw object context");
    if(context.tma) {
        const auto& t=*context.tma;if(!result->object_id() || !t.core_id || t.core_revision>result->document_revision() || t.row>=10000 || t.column>=10000) throw std::invalid_argument("invalid captured raw TMA selectors");
        detail::bounded_utf8(t.grid_name,256);detail::bounded_utf8(t.row_label,128);detail::bounded_utf8(t.column_label,128);detail::bounded_utf8(t.core_name,4096);detail::bounded_utf8(t.case_id,4096);
        if(t.grid_name.empty() || t.row_label.empty() || t.column_label.empty()) throw std::invalid_argument("invalid captured raw TMA labels");
    }
    const auto& metadata=result->metadata();const auto& stats=result->statistics();
    const auto region=result->grid()?RawRegionRequest{{0,0,0,1,1},result->grid()->options.first_channel,result->grid()->options.channels,result->grid()->options.z,result->grid()->options.t}:result->region();
    if(metadata.width!=image.width || metadata.height!=image.height || metadata.channels<1 || metadata.channels>65536 || !metadata.z_planes || metadata.z_planes>65536 ||
       !metadata.timepoints || metadata.timepoints>65536 || metadata.series>=65536 || std::uint64_t(metadata.channels)*metadata.z_planes*metadata.timepoints>1048576 || metadata.channel_names.size()!=region.channels || region.region.level<0 || std::size_t(region.region.level)>=image.levels.size()) throw std::invalid_argument("raw analysis record axes mismatch");
    if(result->grid()) {
        const auto& captured=*result->grid();const auto& g=captured.grid;const auto& raw=captured.source_metadata;
        scientific_raw_work_bytes(raw,g,raw_grid_analysis_sampling_options(raw,g,captured.options,true));validate_scientific_raster_geometry(g);
        if(g.image_width()!=image.width || g.image_height()!=image.height || metadata.width!=raw.width || metadata.height!=raw.height ||
           metadata.level_width!=g.width() || metadata.level_height!=g.height() || !std::isnan(metadata.downsample) ||
           metadata.channels!=raw.channels || metadata.z_planes!=raw.z_planes || metadata.timepoints!=raw.timepoints || metadata.series!=raw.series ||
           metadata.sample_type!=raw.sample_type || metadata.z_spacing_um!=raw.z_spacing_um || metadata.time_interval_seconds!=raw.time_interval_seconds ||
           raw.channel_names.size()!=raw.channels || std::uint64_t(g.width())>max_pixels/std::uint64_t(g.height()))
            throw std::invalid_argument("raw grid record metadata mismatch");
        std::size_t labels=0;
        for(const auto& name:raw.channel_names) {detail::bounded_utf8(name,4096);labels+=name.size();}
        if(labels>4u*1024*1024 || raw.levels.size()>65536) throw std::length_error("raw grid source metadata exceeds limit");
        for(std::size_t i=0;i<metadata.channel_names.size();++i)
            if(metadata.channel_names[i]!=raw.channel_names[captured.options.first_channel+i]) throw std::invalid_argument("raw grid selected channel name mismatch");
        calibration(raw.pixel_width_um);calibration(raw.pixel_height_um);calibration(raw.z_spacing_um);calibration(raw.time_interval_seconds);
        detail::bounded_utf8(raw.dimension_order,65536);detail::bounded_utf8(captured.reader,65536);
        if(captured.reader.empty()) throw std::invalid_argument("raw grid reader identity is empty");
        for(const auto* hash:{&captured.decoded_source_sha256,&captured.sampling_input_sha256})
            if(hash->size()!=64 || hash->find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid raw grid provenance digest");
        if(detail::scientific_raw_input_sha256(raw,g,captured.reader,captured.decoded_source_sha256)!=captured.sampling_input_sha256)
            throw std::invalid_argument("raw grid sampling digest differs from metadata");
        for(const auto& level:raw.levels)
            if(level.width<1 || level.height<1 || !std::isfinite(level.downsample) || level.downsample<1) throw std::invalid_argument("invalid raw grid source level metadata");
        if(raw_grid_analysis_hash(captured.sampling_input_sha256,result->pixel_width_um(),result->pixel_height_um(),mask)!=result->input_sha256())
            throw std::invalid_argument("raw grid analysis digest differs from capture");
        std::uint64_t eligible=0;
        if(!mask) eligible=std::uint64_t(g.x().samples())*g.y().samples();
        else for(std::int64_t y=0;y<g.height();++y) for(std::int64_t x=0;x<g.width();++x)
            if(scientific_core_roi_contains(*mask,g,RoiSamplingPolicy::GeometryCoverage,std::size_t(x),std::size_t(y))) ++eligible;
        const auto b=g.clipped_read_bounds();
        if(stats.pixels!=eligible || stats.valid_pixels>stats.pixels || result->tiles()!=std::uint64_t(bool(b.width && b.height)))
            throw std::invalid_argument("invalid raw grid support counts");
        if(stats.finite_samples.size()!=region.channels || stats.nonfinite_samples.size()!=region.channels || stats.means.size()!=region.channels ||
           stats.minima.size()!=region.channels || stats.maxima.size()!=region.channels) throw std::invalid_argument("raw grid statistic channel counts differ");
    } else {
    const auto& level=image.levels[region.region.level];if(!std::isfinite(metadata.downsample) || metadata.downsample<1 || metadata.downsample!=std::floor(metadata.downsample) || level.width!=metadata.level_width || level.height!=metadata.level_height || level.downsample!=metadata.downsample) throw std::invalid_argument("raw analysis record pyramid mismatch");
    RawImageMetadata raw;raw.width=metadata.width;raw.height=metadata.height;raw.channels=metadata.channels;raw.z_planes=metadata.z_planes;raw.timepoints=metadata.timepoints;raw.levels=image.levels;
    RawStatisticsKernel::validate_coordinates(raw,region);const auto unit=std::uint64_t(region.channels)*sample_bytes(metadata.sample_type)+1;
    if(std::uint64_t(result->tile_size())*result->tile_size()>256*1024*1024/unit) throw std::invalid_argument("raw analysis tile exceeds working buffer limit");
    if(std::uint64_t(region.region.width)>max_pixels/std::uint64_t(region.region.height) || stats.pixels!=std::uint64_t(region.region.width)*region.region.height || stats.valid_pixels>stats.pixels ||
       stats.finite_samples.size()!=region.channels || stats.nonfinite_samples.size()!=region.channels || stats.means.size()!=region.channels || stats.minima.size()!=region.channels || stats.maxima.size()!=region.channels)
        throw std::invalid_argument("invalid raw analysis sample counts");
    const auto nx=(std::uint64_t(region.region.width)+result->tile_size()-1)/result->tile_size(),ny=(std::uint64_t(region.region.height)+result->tile_size()-1)/result->tile_size();
    if(result->tiles()!=nx*ny) throw std::invalid_argument("invalid raw analysis tile count");
    }
    std::size_t text_bytes=0;
    for(std::size_t i=0;i<region.channels;++i) {
        const auto n=stats.finite_samples[i];if(n>stats.valid_pixels || stats.nonfinite_samples[i]!=stats.valid_pixels-n) throw std::invalid_argument("raw analysis finite/nonfinite counts differ");
        if(stats.means[i].has_value()!=bool(n) || stats.minima[i].has_value()!=bool(n) || stats.maxima[i].has_value()!=bool(n)) throw std::invalid_argument("raw analysis missing/extra numeric result");
        if(n && (!std::isfinite(*stats.means[i]) || !std::isfinite(*stats.minima[i]) || !std::isfinite(*stats.maxima[i]) || *stats.minima[i]>*stats.maxima[i] || *stats.means[i]<*stats.minima[i] || *stats.means[i]>*stats.maxima[i])) throw std::invalid_argument("invalid raw analysis numeric result");
        detail::bounded_utf8(metadata.channel_names[i],4096);text_bytes+=metadata.channel_names[i].size();
    }
    if(text_bytes>4u*1024*1024) throw std::length_error("raw analysis channel labels exceed limit");
    calibration(result->pixel_width_um());calibration(result->pixel_height_um());calibration(metadata.z_spacing_um);calibration(metadata.time_interval_seconds);
    const auto& hash=result->input_sha256();if(hash.size()!=64 || hash.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid raw input digest");
    if(!result->source() || !result->source()->primary || result->source()->reader.rfind("tiff:",0)!=0 || result->source()->scope!="single_file") throw std::invalid_argument("raw analysis requires observed TIFF provenance");result->source()->validate();
    if(payload_bytes()>16u*1024*1024) throw std::length_error("raw analysis record exceeds 16 MiB limit");
}
void Document::restore_raw_analysis(std::shared_ptr<const RawAnalysisRecord> record) {
    if(!record || !record->id || record->id>revision_) throw std::invalid_argument("raw analysis record revision mismatch");record->validate(metadata());const auto projection=server_->tiff_view();if(!projection || projection->series!=record->result->metadata().series) throw std::invalid_argument("raw analysis record requires the captured TIFF series");
    const auto id=record->id;if(raw_analysis_.count(id) || analysis_.count(id) || tissue_.count(id) || cells_.count(id) || classifiers_.count(id) || pixels_.count(id) || streams_.count(id)) throw std::invalid_argument("duplicate raw analysis record ID");
    const auto bytes=record->payload_bytes();if(raw_analysis_.size()>=10000 || bytes>budget-raw_analysis_bytes_) throw std::length_error("raw analysis records exceed 10000-record/64 MiB limit");
    const auto slot=raw_analysis_free_slots_.empty()?raw_analysis_next_slot_:raw_analysis_free_slots_.back(),block_index=slot/256;
    auto& blocks=raw_analysis_blocks_.values;const bool added=block_index==blocks.size();std::shared_ptr<RawAnalysisBlock> block;
    if(added) {block=std::make_shared<RawAnalysisBlock>();if(blocks.size()==blocks.capacity()) blocks.reserve(std::max<std::size_t>(4,blocks.capacity()*2));}
    else if(!blocks[block_index]->sealed) block=blocks[block_index];else {block=std::make_shared<RawAnalysisBlock>(*blocks[block_index]);block->sealed=false;}
    if(block->records[slot%256]) throw std::logic_error("raw analysis record slot is already occupied");
    const auto key=std::make_pair(record->result->object_id(),id);raw_analysis_by_object_.emplace(key,true);
    try {raw_analysis_.emplace(id,StoredRawAnalysis{record,{},{},false,slot});}catch(...) {raw_analysis_by_object_.erase(key);throw;}
    if(added) blocks.push_back(block);else blocks[block_index]=block;block->records[slot%256]=std::move(record);
    if(raw_analysis_free_slots_.empty()) ++raw_analysis_next_slot_;else raw_analysis_free_slots_.pop_back();raw_analysis_bytes_+=bytes;
}
std::uint64_t Document::keep_raw_analysis(const RawAnalysisRequest& request,std::shared_ptr<const RawAnalysisResult> result) {
    if(!result || !request.matches(*this) || result->capture_!=request.capture_) throw std::invalid_argument("raw analysis result is stale or belongs to another request");advance(revision_);
    auto record=std::make_shared<RawAnalysisRecord>();record->id=revision_+1;record->result=std::move(result);record->mask=request.mask_;record->max_pixels=request.max_pixels_;record->requested_tile_size=request.tile_size_;
    ++revision_;try {restore_raw_analysis(record);}catch(...) {--revision_;throw;}
    auto& stored=raw_analysis_.at(record->id);stored.identity=analysis_identity_.token;stored.observed_source=record->result->source();stored.verified=true;return record->id;
}
bool Document::verify_raw_analysis(std::uint64_t id,const RawAnalysisRequest& request,std::shared_ptr<const RawAnalysisResult> result) {
    if(!result || !request.matches(*this) || result->capture_!=request.capture_) throw std::invalid_argument("raw verification result is stale or belongs to another request");
    auto found=raw_analysis_.find(id);if(found==raw_analysis_.end()) throw std::out_of_range("raw analysis record does not exist");auto& stored=found->second;const auto& record=*stored.value;const auto& before=*record.result;
    if(before.object_id()!=request.object_id_ || before.object_revision()!=request.object_revision_ || before.nucleus()!=request.nucleus_ ||
       !same_raw_grid(before.grid(),request.grid_,request.grid_options_) || (!request.grid_ && !same_region(before.region(),request.region_)) || !same_mask(record.mask,request.mask_) || record.requested_tile_size!=request.tile_size_ || record.max_pixels!=request.max_pixels_ ||
       before.pixel_width_um()!=result->pixel_width_um() || before.pixel_height_um()!=result->pixel_height_um() || before.metadata().series!=result->metadata().series || !before.object_context().same_metadata(request.context_))
        throw std::invalid_argument("raw verification parameters differ from the archived capture");
    const auto& a=before.statistics();const auto& b=result->statistics();stored.identity=analysis_identity_.token;stored.observed_source=result->source();
    stored.verified=before.algorithm()==result->algorithm() && before.input_sha256()==result->input_sha256() && before.tile_size()==result->tile_size() && before.tiles()==result->tiles() &&
        a.pixels==b.pixels && a.valid_pixels==b.valid_pixels && a.finite_samples==b.finite_samples && a.nonfinite_samples==b.nonfinite_samples && a.means==b.means && a.minima==b.minima && a.maxima==b.maxima;
    return stored.verified;
}
std::vector<std::shared_ptr<const RawAnalysisRecord>> Document::raw_analysis_records() const {
    std::vector<std::shared_ptr<const RawAnalysisRecord>> result;result.reserve(raw_analysis_record_count());for(const auto& entry:raw_analysis_) result.push_back(entry.second.value);
    for(const auto& block:raw_analysis_snapshot_) for(const auto& record:block->records) if(record) result.push_back(record);
    if(!raw_analysis_snapshot_.empty()) std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a->id<b->id;});return result;
}
std::vector<std::shared_ptr<const RawAnalysisRecord>> Document::raw_analysis_page(std::uint64_t after,std::size_t limit) const {
    if(!limit || limit>512) throw std::invalid_argument("raw analysis page limit must be 1..512");std::vector<std::shared_ptr<const RawAnalysisRecord>> result;result.reserve(std::min(limit,raw_analysis_record_count()));
    if(!raw_analysis_snapshot_.empty()) {for(const auto& record:raw_analysis_records()) if(record->id>after) {result.push_back(record);if(result.size()==limit) break;}return result;}
    for(auto it=raw_analysis_.upper_bound(after);it!=raw_analysis_.end() && result.size()<limit;++it) result.push_back(it->second.value);return result;
}
std::shared_ptr<const RawAnalysisRecord> Document::raw_analysis_record(std::uint64_t id) const {
    const auto found=raw_analysis_.find(id);if(found!=raw_analysis_.end()) return found->second.value;
    for(const auto& block:raw_analysis_snapshot_) for(const auto& record:block->records) if(record && record->id==id) return record;throw std::out_of_range("raw analysis record does not exist");
}
std::shared_ptr<const RawAnalysisRecord> Document::latest_raw_analysis(std::uint64_t object) const {
    auto it=raw_analysis_by_object_.upper_bound({object,UINT64_MAX});if(it!=raw_analysis_by_object_.begin() && (--it)->first.first==object) return raw_analysis_.at(it->first.second).value;
    std::shared_ptr<const RawAnalysisRecord> result;for(const auto& block:raw_analysis_snapshot_) for(const auto& record:block->records) if(record && record->result->object_id()==object && (!result || record->id>result->id)) result=record;return result;
}
std::string Document::raw_analysis_status(std::uint64_t id) const {
    const auto found=raw_analysis_.find(id);if(found==raw_analysis_.end()) {raw_analysis_record(id);return "unverified";}const auto& stored=found->second;const auto& result=*stored.value->result;
    if(result.object_id()) {const auto object=objects_.find(result.object_id());if(object==objects_.end() || object->second.revision!=result.object_revision()) return "object_changed";
        const auto roi=result.nucleus()?object->second.nucleus:std::optional<Roi>(object->second.geometry);if(!same_mask(stored.value->mask,roi)) return "object_changed";}
    if(!RawAnalysisRequest::context_matches(*this,result.object_id(),result.object_context())) return "context_changed";
    if(pixel_width_um_!=result.pixel_width_um() || pixel_height_um_!=result.pixel_height_um()) return "calibration_changed";
    if(!stored.identity || stored.identity!=analysis_identity_.token) return "unverified";
    if(server_->closed()) return "source_closed";const auto view=server_->tiff_view();if(!view || view->series!=result.metadata().series) return "source_changed";
    const auto source=server_->reader_source(),proof=stored.observed_source;
    if(!source || !proof || !source->primary || !proof->primary) return "unverified";
    if(source->locator!=proof->locator || !same_file(*source->primary,*proof->primary)) return "source_changed";
    return stored.verified?"current":"verification_failed";
}
bool Document::remove_raw_analysis(std::uint64_t id) {
    const auto found=raw_analysis_.find(id);if(found==raw_analysis_.end()) return false;advance(revision_);
    if(raw_analysis_.size()==1) {raw_analysis_.clear();raw_analysis_by_object_.clear();raw_analysis_blocks_.values.clear();std::vector<std::size_t>().swap(raw_analysis_free_slots_);raw_analysis_next_slot_=0;raw_analysis_bytes_=0;++revision_;return true;}
    const auto slot=found->second.slot,index=slot/256;auto block=raw_analysis_blocks_.values[index];if(block->sealed) {block=std::make_shared<RawAnalysisBlock>(*block);block->sealed=false;}
    if(raw_analysis_free_slots_.size()==raw_analysis_free_slots_.capacity()) raw_analysis_free_slots_.reserve(std::max<std::size_t>(64,raw_analysis_free_slots_.capacity()*2));
    raw_analysis_blocks_.values[index]=block;block->records[slot%256].reset();raw_analysis_free_slots_.push_back(slot);
    raw_analysis_by_object_.erase({found->second.value->result->object_id(),id});raw_analysis_bytes_-=found->second.value->payload_bytes();raw_analysis_.erase(found);++revision_;return true;
}
RawAnalysisRequest RawAnalysisRequest::replay(const Document& document,std::uint64_t id) {
    const auto record=document.raw_analysis_record(id);const auto& result=*record->result;
    const auto view=document.server()->tiff_view();if(!view || view->series!=result.metadata().series) throw std::invalid_argument("raw replay requires the captured TIFF series");
    RawAnalysisRequest request=result.grid()?RawAnalysisRequest(document,result.grid()->grid,result.grid()->options,result.object_id(),result.nucleus(),record->max_pixels):
        RawAnalysisRequest(document,result.region(),result.object_id(),result.nucleus(),record->requested_tile_size,record->max_pixels);
    if(!same_raw_grid(result.grid(),request.grid_,request.grid_options_) || (!request.grid_ && !same_region(request.region_,result.region())) || !same_mask(request.mask_,record->mask) || request.object_revision_!=result.object_revision() || request.pixel_width_um_!=result.pixel_width_um() || request.pixel_height_um_!=result.pixel_height_um() || !request.context_.same_metadata(result.object_context())) throw std::invalid_argument("raw replay object/calibration no longer matches capture");
    return request;
}
}
