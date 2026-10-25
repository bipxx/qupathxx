#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/tma.hpp"
#include "raw_statistics_internal.hpp"
#include "scientific_raster.hpp"
#include "exact_weighted_mean.hpp"
#include "geometry_archive.hpp"
#include "atomic_export.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <future>
#include <thread>
#include "utf8_text.hpp"
#include <stdexcept>

namespace qpx {
namespace {
bool same_file(const FileVersion& a,const FileVersion& b) {
    return a.identity_scheme==b.identity_scheme && a.identity==b.identity && a.bytes==b.bytes &&
        a.stamp_scheme==b.stamp_scheme && a.modified_token==b.modified_token && a.change_token==b.change_token;
}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
void number(Sha256& hash,std::optional<double> value) {
    hash.integer(value.has_value());if(value) {std::uint64_t bits;static_assert(sizeof(bits)==sizeof(*value));std::memcpy(&bits,&*value,sizeof(bits));hash.integer(bits);}
}
std::int64_t coordinate(long double value) {
    if(!std::isfinite(value) || value<0 || value>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("raw ROI coordinate overflow");
    return static_cast<std::int64_t>(value);
}
}
RawAnalysisRequest::RawAnalysisRequest(const Document& doc,RawRegionRequest region,std::uint64_t object,bool nucleus,std::size_t tile,std::uint64_t maximum):
    RawAnalysisRequest(doc,region,object,nucleus,tile,maximum,std::nullopt,{}) {}
RawAnalysisRequest::RawAnalysisRequest(const Document& doc,ScientificGrid grid,ScientificRawOptions options,std::uint64_t object,bool nucleus,std::uint64_t maximum):
    RawAnalysisRequest(doc,{{0,0,0,1,1},options.first_channel,options.channels,options.z,options.t},object,nucleus,0,maximum,std::move(grid),options) {}
RawAnalysisRequest::RawAnalysisRequest(const Document& doc,RawRegionRequest region,std::uint64_t object,bool nucleus,std::size_t tile,std::uint64_t maximum,std::optional<ScientificGrid> grid,ScientificRawOptions options):
    grid_(std::move(grid)),grid_options_(options),identity_(doc.analysis_identity_.token),source_(doc.server()),region_(region),object_id_(object),document_revision_(doc.revision()),max_pixels_(maximum),nucleus_(nucleus),tile_size_(tile),
    pixel_width_um_(doc.pixel_width_um()),pixel_height_um_(doc.pixel_height_um()) {
    if(!source_ || source_->closed()) throw std::invalid_argument("raw analysis requires an open source");
    const auto view=source_->tiff_view();if(!view) throw std::invalid_argument("raw analysis requires a quantitative TIFF source");series_=view->series;
    if((!grid_ && (tile<16 || tile>1024)) || !maximum || maximum>1000000000000ULL) throw std::invalid_argument("invalid raw analysis tile/pixel limit");
    if(!region_.channels || region_.channels>65536 || region_.first_channel>=65536 || region_.channels>65536-region_.first_channel || region_.z>=65536 || region_.t>=65536)
        throw std::out_of_range("raw analysis channel/Z/time selection exceeds limits");
    const auto& metadata=doc.metadata();const auto level=region_.region.level;
    if(level<0 || std::size_t(level)>=metadata.levels.size()) throw std::out_of_range("invalid raw analysis level");
    const auto& selected=metadata.levels[level];const auto scale=selected.downsample;
    if(!std::isfinite(scale) || scale<1 || scale!=std::floor(scale) || static_cast<long double>(scale)>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("raw analysis requires an integer natural pyramid scale");
    if(grid_) {
        grid_options_.validate();
        if(grid_->image_width()!=metadata.width || grid_->image_height()!=metadata.height)
            throw std::invalid_argument("raw analysis grid/image dimensions differ");
        validate_scientific_raster_geometry(*grid_);
        if(std::uint64_t(grid_->width())>maximum/std::uint64_t(grid_->height()))
            throw std::length_error("raw grid analysis exceeds pixel limit");
    }
    if(object) {
        const auto annotation=doc.annotation(object);object_revision_=annotation.revision;
        context_.name=annotation.name;context_.class_name=annotation.class_name;context_.parent_id=annotation.parent_id;context_.kind=annotation.kind;
        auto ancestor=annotation;std::size_t depth=0;
        for(;;) {
            if(ancestor.tma_core) {const auto& core=*ancestor.tma_core;context_.tma=RawTmaContext{ancestor.id,ancestor.revision,std::uint32_t(core.row()),std::uint32_t(core.column()),core.missing(),core.grid_name(),core.row_label(),core.column_label(),ancestor.name,core.case_id()};break;}
            if(!ancestor.parent_id) break;if(++depth>4096) throw std::length_error("raw context hierarchy exceeds 4096 ancestors");ancestor=doc.annotation(ancestor.parent_id);
        }
        if(nucleus && !annotation.nucleus) throw std::invalid_argument("selected raw analysis object has no nucleus");
        mask_=nucleus?*annotation.nucleus:annotation.geometry;if(!mask_->is_area()) throw std::invalid_argument("raw analysis requires an area ROI");
        if(!grid_) {
        const auto bounds=mask_->bounds();const auto d=static_cast<long double>(scale);
        const auto x0=std::max<long double>(0,std::floor(bounds.x/scale));
        const auto y0=std::max<long double>(0,std::floor(bounds.y/scale));
        const auto x1=std::min<long double>(static_cast<long double>(selected.width),std::ceil((bounds.x+bounds.width)/scale));
        const auto y1=std::min<long double>(static_cast<long double>(selected.height),std::ceil((bounds.y+bounds.height)/scale));
        if(x1<=x0 || y1<=y0) throw std::invalid_argument("raw analysis ROI does not intersect the image");
        region_.region=RegionRequest(coordinate(x0*d),coordinate(y0*d),level,coordinate(x1-x0),coordinate(y1-y0));
        }
    }else if(nucleus) throw std::invalid_argument("nucleus analysis requires an object");
    if(region_.region.width<=0 || region_.region.height<=0 || std::uint64_t(region_.region.width)>maximum/std::uint64_t(region_.region.height)) throw std::length_error("raw analysis exceeds pixel limit");
}
bool RawAnalysisRequest::context_matches(const Document& doc,std::uint64_t id,const RawObjectContext& context) {
    if(!id) return !context.tma;std::size_t depth=0;
    // Parent edits do not change descendant revisions. Resolve the nearest core
    // through native map references, without copying labels or opening a reader.
    for(;;) {
        const auto found=doc.objects_.find(id);if(found==doc.objects_.end()) return false;const auto& object=found->second;
        if(object.tma_core) return context.tma && context.tma->core_id==object.id && context.tma->core_revision==object.revision;
        if(!object.parent_id) return !context.tma;if(++depth>4096) return false;id=object.parent_id;
    }
}
bool RawAnalysisRequest::matches(const Document& doc) const {
    if(identity_!=doc.analysis_identity_.token || doc.pixel_width_um()!=pixel_width_um_ || doc.pixel_height_um()!=pixel_height_um_) return false;
    const auto current=doc.server();if(!current || current->closed()) return false;const auto view=current->tiff_view();
    if(!view || view->series!=series_ || current->source_locator()!=(source_?source_->source_locator():observed_?observed_->locator:std::string{})) return false;
    if(current!=source_) {
        const auto before=observed_?observed_:source_?source_->reader_source():nullptr,after=current->reader_source();
        if(!before || !after || !before->primary || !after->primary || !same_file(*before->primary,*after->primary)) return false;
    }
    if(object_id_) {
        try {const auto object=doc.annotation(object_id_);if(object.revision!=object_revision_) return false;
            if(nucleus_ && !object.nucleus) return false;const auto& roi=nucleus_?*object.nucleus:object.geometry;
            if(!mask_ || !roi.equals(*mask_)) return false;
        }catch(const std::out_of_range&) {return false;}
    }
    return context_matches(doc,object_id_,context_);
}
std::size_t RawObjectContext::payload_bytes() const {
    auto bytes=sizeof(*this)+name.capacity()+class_name.capacity()+2;if(tma) for(const auto* text:{&tma->grid_name,&tma->row_label,&tma->column_label,&tma->core_name,&tma->case_id}) bytes+=text->capacity()+1;return bytes;
}
bool RawObjectContext::same_metadata(const RawObjectContext& other) const {
    if(name!=other.name || class_name!=other.class_name || parent_id!=other.parent_id || kind!=other.kind || tma.has_value()!=other.tma.has_value()) return false;
    if(!tma) return true;const auto& a=*tma;const auto& b=*other.tma;
    return a.core_id==b.core_id && a.core_revision==b.core_revision && a.row==b.row && a.column==b.column && a.missing==b.missing &&
        a.grid_name==b.grid_name && a.row_label==b.row_label && a.column_label==b.column_label && a.core_name==b.core_name && a.case_id==b.case_id;
}
std::size_t RawAnalysisMetadata::payload_bytes() const {
    auto bytes=sizeof(*this)+channel_names.capacity()*sizeof(std::string);for(const auto& name:channel_names) bytes+=name.capacity()+1;return bytes;
}
std::size_t RawAnalysisResult::payload_bytes() const {
    return (grid_?grid_->source_metadata.payload_bytes()+grid_->decoded_source_sha256.capacity()+grid_->sampling_input_sha256.capacity():0)+sizeof(*this)+metadata_.payload_bytes()+context_.payload_bytes()+statistics_.finite_samples.capacity()*sizeof(std::uint64_t)+statistics_.nonfinite_samples.capacity()*sizeof(std::uint64_t)+
        (statistics_.means.capacity()+statistics_.minima.capacity()+statistics_.maxima.capacity())*sizeof(std::optional<double>)+input_sha256_.capacity()+algorithm_.capacity()+(source_?source_->payload_bytes():0)+256;
}
struct RawAnalysisKernel {
    static std::shared_ptr<const RawAnalysisResult> run_grid(const RawAnalysisRequest& request,const std::atomic<bool>* cancelled,
        std::atomic<std::uint64_t>* completed,std::atomic<std::uint64_t>* total,bool collect) {
        const auto start=std::chrono::steady_clock::now();detail::export_checkpoint(cancelled);
        const auto& grid=*request.grid_;validate_scientific_raster_geometry(grid,cancelled);
        const auto observed=request.source_->raw_metadata(cancelled);
        if(!observed || observed->series!=request.series_) throw std::invalid_argument("raw grid source axes changed");
        auto output=std::shared_ptr<RawAnalysisResult>(new RawAnalysisResult());
        output->source_=request.source_->reader_source();
        if(!output->source_ || !output->source_->primary) throw std::invalid_argument("raw analysis source has no observed file identity");
        const auto channels=request.grid_options_.channels;
        const auto extra=8192ULL+2*observed->payload_bytes()+64ULL*channels+(collect?std::uint64_t(grid.width())*grid.height():0);
        auto options=request.grid_options_;
        if(extra>=options.work_bytes) throw std::length_error("raw grid statistics exceed work budget");
        options.work_bytes-=std::size_t(extra);
        const auto bounds=grid.clipped_read_bounds();const std::uint64_t tiles=bounds.width && bounds.height?1:0;
        if(total) total->store(tiles,std::memory_order_relaxed);
        std::optional<ScientificRawGrid> samples;ScientificRawDigest digest;
        if(collect) {
            samples.emplace(read_scientific_raw(*request.source_,grid,options,cancelled));
            digest.metadata=samples->metadata();digest.reader=samples->reader();
            digest.decoded_source_sha256=samples->decoded_source_sha256();digest.input_sha256=samples->input_sha256();
        } else digest=digest_scientific_raw(*request.source_,grid,options,cancelled);
        if(digest.metadata.series!=request.series_) throw std::invalid_argument("raw grid source series changed");
        if(completed) completed->store(tiles,std::memory_order_relaxed);
        const auto& metadata=digest.metadata;
        if(metadata.channel_names.size()!=metadata.channels) throw std::invalid_argument("raw analysis channel metadata is incomplete");
        output->grid_.emplace(RawAnalysisGrid{grid,request.grid_options_,metadata,digest.decoded_source_sha256,digest.input_sha256});
        output->capture_=request.capture_;output->object_id_=request.object_id_;output->object_revision_=request.object_revision_;
        output->document_revision_=request.document_revision_;output->nucleus_=request.nucleus_;output->context_=request.context_;
        output->pixel_width_um_=request.pixel_width_um_;output->pixel_height_um_=request.pixel_height_um_;
        output->region_=request.region_;output->algorithm_="qpx.raw-grid-statistics/1";output->tile_size_=0;output->tiles_=tiles;
        auto& m=output->metadata_;m.width=metadata.width;m.height=metadata.height;m.level_width=grid.width();m.level_height=grid.height();
        m.channels=metadata.channels;m.z_planes=metadata.z_planes;m.timepoints=metadata.timepoints;m.series=metadata.series;m.sample_type=metadata.sample_type;
        m.downsample=std::numeric_limits<double>::quiet_NaN();m.z_spacing_um=metadata.z_spacing_um;m.time_interval_seconds=metadata.time_interval_seconds;
        m.channel_names.assign(metadata.channel_names.begin()+options.first_channel,metadata.channel_names.begin()+options.first_channel+channels);
        Sha256 hash;text(hash,"qpx.raw-grid-statistics-input/1");text(hash,digest.input_sha256);
        number(hash,request.pixel_width_um_);number(hash,request.pixel_height_um_);
        hash.integer(request.mask_.has_value());
        if(request.mask_) {const auto bytes=GeometryArchiveCodec::encode(*request.mask_);hash.integer(bytes.size());hash.update(bytes.data(),bytes.size());}
        // Fixed core-center geometry coverage; missing source support is not a zero or NaN.
        output->input_sha256_=hash.finish();
        if(collect) {
            std::vector<std::uint8_t> eligible(std::size_t(grid.width()*grid.height()),0);
            auto& stats=output->statistics_;stats.finite_samples.resize(channels);stats.nonfinite_samples.resize(channels);
            stats.means.resize(channels);stats.minima.resize(channels);stats.maxima.resize(channels);
            for(std::int64_t y=0;y<grid.height();++y) for(std::int64_t x=0;x<grid.width();++x) {
                const auto pixel=std::size_t(y*grid.width()+x);if((pixel&4095)==0) detail::export_checkpoint(cancelled);
                const auto i=x+grid.x().first_index(),j=y+grid.y().first_index();
                if(i<0 || j<0 || i>=grid.x().samples() || j>=grid.y().samples()) continue;
                if(request.mask_ && !scientific_core_roi_contains(*request.mask_,grid,RoiSamplingPolicy::GeometryCoverage,std::size_t(x),std::size_t(y))) continue;
                eligible[pixel]=1;++stats.pixels;
                const auto reason=ScientificRawReason(samples->reasons()[pixel*channels]);
                if(reason==ScientificRawReason::Valid || reason==ScientificRawReason::NonfiniteChannel) ++stats.valid_pixels;
            }
            // One bounded accumulator at a time; exact reduction of derived binary64 samples.
            for(std::uint32_t c=0;c<channels;++c) {
                detail::ExactWeightedMean mean;
                for(std::size_t pixel=0;pixel<eligible.size();++pixel) {
                    if((pixel&4095)==0) detail::export_checkpoint(cancelled);if(!eligible[pixel]) continue;
                    const auto at=pixel*channels+c;const auto reason=ScientificRawReason(samples->reasons()[at]);
                    if(reason==ScientificRawReason::NonfiniteChannel) {++stats.nonfinite_samples[c];continue;}
                    if(reason!=ScientificRawReason::Valid) continue;
                    const auto value=samples->values()[at];mean.add(value,1);++stats.finite_samples[c];
                    if(!stats.minima[c] || value<*stats.minima[c]) stats.minima[c]=value;
                    if(!stats.maxima[c] || value>*stats.maxima[c]) stats.maxima[c]=value;
                }
                if(stats.finite_samples[c]) stats.means[c]=mean.mean();
            }
        }
        detail::export_checkpoint(cancelled);
        output->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return output;
    }
    static std::shared_ptr<const RawAnalysisResult> run(const RawAnalysisRequest& request,const std::atomic<bool>* cancelled,
        std::atomic<std::uint64_t>* completed=nullptr,std::atomic<std::uint64_t>* total=nullptr,bool collect_statistics=true) {
        if(request.grid_) return run_grid(request,cancelled,completed,total,collect_statistics);
        const auto start=std::chrono::steady_clock::now();detail::export_checkpoint(cancelled);
        const auto metadata=request.source_->raw_metadata(cancelled);if(!metadata || metadata->series!=request.series_) throw std::invalid_argument("raw analysis source axes changed");
        RawStatisticsKernel::validate_coordinates(*metadata,request.region_);
        const auto& region=request.region_;const auto& level=metadata->levels[region.region.level];
        auto output=std::shared_ptr<RawAnalysisResult>(new RawAnalysisResult());output->capture_=request.capture_;output->region_=region;
        output->object_id_=request.object_id_;output->object_revision_=request.object_revision_;output->document_revision_=request.document_revision_;output->nucleus_=request.nucleus_;
        output->context_=request.context_;output->pixel_width_um_=request.pixel_width_um_;output->pixel_height_um_=request.pixel_height_um_;output->source_=request.source_->reader_source();
        if(!output->source_ || !output->source_->primary) throw std::invalid_argument("raw analysis source has no observed file identity");
        auto& capture=output->metadata_;capture.width=metadata->width;capture.height=metadata->height;capture.level_width=level.width;capture.level_height=level.height;
        capture.channels=metadata->channels;capture.z_planes=metadata->z_planes;capture.timepoints=metadata->timepoints;capture.series=metadata->series;
        capture.sample_type=metadata->sample_type;capture.downsample=level.downsample;capture.z_spacing_um=metadata->z_spacing_um;capture.time_interval_seconds=metadata->time_interval_seconds;
        if(metadata->channel_names.size()!=metadata->channels) throw std::invalid_argument("raw analysis channel metadata is incomplete");
        capture.channel_names.assign(metadata->channel_names.begin()+region.first_channel,metadata->channel_names.begin()+region.first_channel+region.channels);
        const auto unit=std::uint64_t(region.channels)*sample_bytes(metadata->sample_type)+1;
        // Keep the working output within the ordinary raw-buffer budget even
        // for a very large selected channel count; no dense whole-ROI buffer.
        auto tile=request.tile_size_;while(std::uint64_t(tile)*tile>256*1024*1024/unit && tile>16) --tile;
        output->tile_size_=tile;const auto nx=(std::uint64_t(region.region.width)+tile-1)/tile,ny=(std::uint64_t(region.region.height)+tile-1)/tile;
        if(total) total->store(nx*ny,std::memory_order_relaxed);
        Sha256 digest;text(digest,"qpx.raw-roi-input/1");
        for(auto value:{std::uint64_t(capture.width),std::uint64_t(capture.height),std::uint64_t(capture.channels),std::uint64_t(capture.z_planes),std::uint64_t(capture.timepoints),std::uint64_t(capture.series),std::uint64_t(capture.sample_type),
            std::uint64_t(region.region.x),std::uint64_t(region.region.y),std::uint64_t(region.region.width),std::uint64_t(region.region.height),std::uint64_t(region.region.level),std::uint64_t(region.first_channel),std::uint64_t(region.channels),std::uint64_t(region.z),std::uint64_t(region.t),std::uint64_t(tile)}) digest.integer(value);
        number(digest,capture.downsample);number(digest,request.pixel_width_um_);number(digest,request.pixel_height_um_);number(digest,capture.z_spacing_um);number(digest,capture.time_interval_seconds);
        for(const auto& name:capture.channel_names) text(digest,name);
        digest.integer(request.mask_.has_value());if(request.mask_) {const auto bytes=GeometryArchiveCodec::encode(*request.mask_);digest.integer(bytes.size());digest.update(bytes.data(),bytes.size());}
        output->statistics_=RawStatisticsKernel::run(*request.source_,region,tile,request.mask_?&*request.mask_:nullptr,request.max_pixels_,cancelled,&digest,completed,collect_statistics);
        detail::export_checkpoint(cancelled);output->input_sha256_=digest.finish();output->tiles_=nx*ny;
        output->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return output;
    }
};
std::shared_ptr<const RawAnalysisResult> analyze_raw(const RawAnalysisRequest& request,const std::atomic<bool>* cancelled) {return RawAnalysisKernel::run(request,cancelled);}
RawInputDigest raw_analysis_input_digest(const RawAnalysisRequest& request,const std::atomic<bool>* cancelled) {
    const auto result=RawAnalysisKernel::run(request,cancelled,nullptr,nullptr,false);
    if(result->grid()) {
        const auto& g=*result->grid();const auto bounds=g.grid.clipped_read_bounds();
        return {result->input_sha256(),result->tiles(),std::uint64_t(bounds.width)*bounds.height*(std::uint64_t(g.options.channels)*sample_bytes(g.source_metadata.sample_type)+1)};
    }
    const auto& region=result->region();
    return {result->input_sha256(),result->tiles(),std::uint64_t(region.region.width)*region.region.height*(std::uint64_t(region.channels)*sample_bytes(result->metadata().sample_type)+1)};
}

struct RawAnalysisJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::future<std::shared_ptr<const RawAnalysisResult>> future;
    std::optional<RawAnalysisRequest> request;
    std::shared_ptr<const RawAnalysisResult> result;
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> completed{0},total{0};
    RawAnalysisStats stats;
    std::chrono::steady_clock::time_point started;
    void check_owner() const {if(std::this_thread::get_id()!=owner) throw std::runtime_error("raw analysis controller must run on its owner thread");}
};
RawAnalysisJob::RawAnalysisJob():impl_(std::make_unique<Impl>()) {}
RawAnalysisJob::~RawAnalysisJob() {impl_->cancelled.store(true);if(impl_->future.valid()) impl_->future.wait();}
bool RawAnalysisJob::busy() const {impl_->check_owner();return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawAnalysisJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawAnalysisJob::await_completion() {impl_->check_owner();if(impl_->future.valid()) impl_->future.wait();}
bool RawAnalysisJob::request(const RawAnalysisRequest& request) {
    impl_->check_owner();if(busy()) return false;if(impl_->stats.generation==UINT64_MAX) throw std::overflow_error("raw analysis generation exhausted");
    auto captured=request;const auto old_completed=impl_->completed.load(),old_total=impl_->total.load();const auto old_cancelled=impl_->cancelled.load();
    impl_->cancelled.store(false);impl_->completed.store(0);impl_->total.store(0);
    std::future<std::shared_ptr<const RawAnalysisResult>> future;
    try {future=std::async(std::launch::async,[state=impl_.get(),captured] {return RawAnalysisKernel::run(captured,&state->cancelled,&state->completed,&state->total);});}
    catch(...) {impl_->completed.store(old_completed);impl_->total.store(old_total);impl_->cancelled.store(old_cancelled);throw;}
    impl_->future=std::move(future);impl_->request=std::move(captured);impl_->result.reset();impl_->started=std::chrono::steady_clock::now();
    const auto generation=impl_->stats.generation+1;impl_->stats={};impl_->stats.generation=generation;impl_->stats.state="running";return true;
}
RawAnalysisStats RawAnalysisJob::poll(const Document& document) {
    impl_->check_owner();auto& stats=impl_->stats;
    if(impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=impl_->future.get();
            if(impl_->cancelled.load()) {stats.state="cancelled";impl_->result.reset();impl_->request.reset();}
            else if(!impl_->request->matches(document)) {stats.state="stale";impl_->result.reset();impl_->request.reset();}
            else {stats.state="ready";impl_->request->observed_=result->source();impl_->request->source_.reset();impl_->result=std::move(result);}
        }catch(const std::exception& error) {stats.state=impl_->cancelled.load()?"cancelled":"failed";impl_->result.reset();impl_->request.reset();try {stats.error=error.what();detail::bounded_utf8(stats.error,8192);}catch(...) {stats.error="Raw analysis failed";}}
        stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-impl_->started).count();
    }
    if(stats.state=="ready" && impl_->request && !impl_->request->matches(document)) {stats.state="stale";impl_->result.reset();impl_->request.reset();}
    stats.completed_tiles=impl_->completed.load(std::memory_order_relaxed);stats.total_tiles=impl_->total.load(std::memory_order_relaxed);return stats;
}
RawAnalysisStats RawAnalysisJob::wait(const Document& document) {await_completion();return poll(document);}
std::shared_ptr<const RawAnalysisResult> RawAnalysisJob::result() const {impl_->check_owner();return impl_->result;}
std::uint64_t RawAnalysisJob::keep_result(Document& document) {if(poll(document).state!="ready" || !impl_->result) throw std::runtime_error("raw analysis result is not ready/current");return document.keep_raw_analysis(*impl_->request,impl_->result);}
bool RawAnalysisJob::verify_result(Document& document,std::uint64_t id) {if(poll(document).state!="ready" || !impl_->result) throw std::runtime_error("raw verification result is not ready/current");return document.verify_raw_analysis(id,*impl_->request,impl_->result);}

}
