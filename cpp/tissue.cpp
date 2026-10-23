#include "qupathxx/tissue.hpp"
#include "sha256.hpp"
#include "tissue_cleanup.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
void check_cancelled(const std::atomic<bool>* flag) {if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("tissue detection cancelled");}
bool stain_feature(TissueFeature feature) {return feature!=TissueFeature::OpticalDensitySum;}
std::int64_t grid_edge(double edge,double scale,std::int64_t limit) {
    const auto value=std::ceil(static_cast<long double>(edge)/scale-.5L);
    if(value<=0) return 0;
    if(value>=static_cast<long double>(limit)) return limit;
    return static_cast<std::int64_t>(value);
}
std::int64_t read_origin(std::int64_t index,double scale) {
    const auto value=std::round(static_cast<long double>(index)*scale);
    if(!std::isfinite(value) || value<0 || value>=static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
        throw std::invalid_argument("tissue read origin overflow");
    return static_cast<std::int64_t>(value);
}
}
void TissueParameters::validate() const {
    validate_sampling_policy(sampling_policy);
    if(feature!=TissueFeature::OpticalDensitySum && feature!=TissueFeature::Stain1 && feature!=TissueFeature::Stain2 && feature!=TissueFeature::Stain3)
        throw std::invalid_argument("invalid tissue feature");
    if(!std::isfinite(threshold)) throw std::invalid_argument("tissue threshold must be finite");
    if(connectivity!=4 && connectivity!=8) throw std::invalid_argument("tissue connectivity must be 4 or 8");
    if(level< -1) throw std::invalid_argument("tissue level must be -1 (auto) or nonnegative");
    if(open_radius<0 || open_radius>256 || close_radius<0 || close_radius>256) throw std::invalid_argument("tissue morphology radii must be 0..256 sampled pixels");
    if(!min_pixels || (max_pixels && max_pixels<min_pixels)) throw std::invalid_argument("invalid tissue component size range");
    if(tile_size<16 || tile_size>1024) throw std::invalid_argument("tissue tile size must be 16..1024");
    if(!max_components || max_components>1000000) throw std::invalid_argument("tissue component limit must be 1..1000000");
    if(!work_bytes || work_bytes>512u*1024u*1024u) throw std::invalid_argument("tissue work budget must be 1 byte..512 MiB");
}
TissueRequest::TissueRequest(const Document& document,std::uint64_t id,const StainTransform& transform,TissueParameters parameters,bool nucleus):
    source_(document,id,transform,stain_feature(parameters.feature)?AnalysisKind::StainMeans:AnalysisKind::OpticalDensityMeans,nucleus,parameters.tile_size,true,parameters.sampling_policy),parameters_(parameters) {
    parameters_.validate();
    const auto& metadata=source_.source_->metadata();const auto& levels=metadata.levels;const auto b=source_.roi_.bounds();
    if(levels.empty() || levels.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::invalid_argument("invalid tissue pyramid metadata");
    if(parameters.level>=0 && static_cast<std::size_t>(parameters.level)>=levels.size()) throw std::out_of_range("invalid tissue pyramid level");
    bool found=false,has_centers=false;
    for(std::size_t i=0;i<levels.size();++i) {
        const auto& l=levels[i];
        if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1 || (i==0 && l.downsample!=1))
            throw std::invalid_argument("invalid tissue pyramid metadata");
        if(parameters.level>=0 && i!=static_cast<std::size_t>(parameters.level)) continue;
        const auto x0=grid_edge(b.x,l.downsample,l.width),y0=grid_edge(b.y,l.downsample,l.height);
        const auto x1=grid_edge(b.x+b.width,l.downsample,l.width),y1=grid_edge(b.y+b.height,l.downsample,l.height);
        if(x0>=x1 || y0>=y1) continue;
        has_centers=true;
        const auto w=static_cast<std::uint64_t>(x1-x0),h=static_cast<std::uint64_t>(y1-y0);
        if(w>std::numeric_limits<std::uint32_t>::max()/h) continue;
        const auto count=w*h,capacity=std::min<std::uint64_t>(count,parameters.max_components);
        const auto estimate=count*(tissue_detail::enabled(parameters)?9:8)+capacity*sizeof(TissueComponent)+16ull*parameters.tile_size*parameters.tile_size+65536;
        if(estimate>parameters.work_bytes || (found && l.downsample>=downsample_)) continue;
        // Validate the furthest nominal tile before the worker allocates/decodes.
        const auto step=static_cast<std::int64_t>(parameters.tile_size);
        const auto last_x=x0+((x1-x0-1)/step)*step;
        const auto last_y=y0+((y1-y0-1)/step)*step;
        validate_region(metadata,{read_origin(last_x,l.downsample),read_origin(last_y,l.downsample),static_cast<std::int32_t>(i),x1-last_x,y1-last_y});
        const auto bounds=Rect{double(x0)*l.downsample,double(y0)*l.downsample,double(w)*l.downsample,double(h)*l.downsample};
        if(!std::isfinite(bounds.x+bounds.width) || !std::isfinite(bounds.y+bounds.height)) throw std::invalid_argument("tissue grid extent overflow");
        found=true;level_=static_cast<std::int32_t>(i);downsample_=l.downsample;x0_=x0;y0_=y0;x1_=x1;y1_=y1;
        component_capacity_=static_cast<std::size_t>(capacity);estimated_work_bytes_=static_cast<std::size_t>(estimate);
        total_tiles_=((w+parameters.tile_size-1)/parameters.tile_size)*((h+parameters.tile_size-1)/parameters.tile_size);
    }
    if(!has_centers) throw std::invalid_argument("ROI contains no pixel centers at the requested pyramid level");
    if(!found) throw std::length_error("tissue raster exceeds work budget or uint32 indexing; select a coarser level or smaller ROI");
}
struct TissueKernel {
    static std::shared_ptr<const TissueResult> run(const TissueRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr) {
        const auto started=std::chrono::steady_clock::now();check_cancelled(cancelled);
        const auto& source=request.source_;const auto& p=request.parameters_;
        auto output=std::shared_ptr<TissueResult>(new TissueResult());output->provenance_=request.provenance();
        output->ticket_=request.ticket_;
        const auto width=request.x1_-request.x0_,height=request.y1_-request.y0_;const auto scale=request.downsample_;
        const auto count=static_cast<std::size_t>(width*height);
        output->bounds_=request.bounds();output->level_bounds_=request.level_bounds();output->level_=request.level_;output->downsample_=scale;
        output->roi_=source.roi_;output->image_bounds_={0,0,double(source.source_->metadata().width),double(source.source_->metadata().height)};
        output->algorithm_=tissue_detail::algorithm(p,request.level_);
        const bool cleanup=tissue_detail::enabled(p);
        output->parameters_=p;output->estimated_work_bytes_=request.estimated_work_bytes_;
        output->object_id_=source.object_id_;output->object_revision_=source.object_revision_;output->document_revision_=source.document_revision_;
        output->nucleus_=source.nucleus_;output->white_=source.transform_.white();output->stains_=source.transform_.stains();
        output->labels_.resize(count);output->components_.reserve(request.component_capacity_);
        std::vector<std::uint32_t> queue;queue.reserve(count);
        std::vector<std::uint8_t> domain;if(cleanup) domain.resize(count);
        auto& labels=output->labels_;constexpr auto foreground=std::numeric_limits<std::uint32_t>::max();
        Sha256 digest;const std::string format=output->input_format();digest.update(format.data(),format.size());
        digest.integer(static_cast<std::uint64_t>(source.source_->metadata().width));digest.integer(static_cast<std::uint64_t>(source.source_->metadata().height));
        if(request.level_!=0) {
            const auto& l=source.source_->metadata().levels[request.level_];std::uint64_t bits=0;static_assert(sizeof(bits)==sizeof(scale));std::memcpy(&bits,&scale,sizeof(bits));
            digest.integer(request.level_);digest.integer(l.width);digest.integer(l.height);digest.integer(bits);
        }
        for(auto value:{request.x0_,request.y0_,request.x1_,request.y1_}) digest.integer(static_cast<std::uint64_t>(value));digest.integer(p.tile_size);
        if(phase) phase->store(1);
        std::uint64_t tiles=0;
        for(auto y=request.y0_;y<request.y1_;) {
            const auto h=std::min<std::int64_t>(p.tile_size,request.y1_-y);
            for(auto x=request.x0_;x<request.x1_;) {
                check_cancelled(cancelled);const auto w=std::min<std::int64_t>(p.tile_size,request.x1_-x);
                const auto rx=read_origin(x,scale),ry=read_origin(y,scale);
                const auto pixels=source.source_->read_region({rx,ry,request.level_,w,h});
                if(pixels.width()!=w || pixels.height()!=h) throw std::runtime_error("reader returned incorrect tissue tile dimensions");
                if(request.level_!=0) {digest.integer(x);digest.integer(y);}
                for(auto value:{rx,ry,w,h}) digest.integer(static_cast<std::uint64_t>(value));digest.update(pixels.bytes().data(),pixels.bytes().size());
                const auto values=stain_feature(p.feature)?source.transform_.deconvolve(pixels,cancelled):source.transform_.optical_densities(pixels,cancelled);
                const auto& data=values.values();
                for(std::int64_t row=0;row<h;++row) {
                    check_cancelled(cancelled);
                    for(std::int64_t col=0;col<w;++col) {
                        const auto i=static_cast<std::size_t>((row*w+col)*3);
                        if(!std::isfinite(data[i])) continue;
                        if(source.roi_.kind()!=RoiKind::Rectangle) {
                            const Point center{(static_cast<double>(x+col)+.5)*scale,(static_cast<double>(y+row)+.5)*scale};
                            if(!(p.sampling_policy==RoiSamplingPolicy::GeometryCoverage?source.roi_.contains(center):source.roi_.contains_sample(center,p.sampling_policy))) continue;
                        }
                        ++output->samples_;
                        const auto target=static_cast<std::size_t>((y-request.y0_+row)*width+x-request.x0_+col);
                        if(cleanup) domain[target]=1;
                        const double score=stain_feature(p.feature)?data[i+static_cast<int>(p.feature)-1]:double(data[i])+double(data[i+1])+double(data[i+2]);
                        if(score>=p.threshold) {labels[target]=foreground;++output->threshold_pixels_;}
                    }
                }
                ++tiles;if(completed) completed->store(tiles,std::memory_order_relaxed);x+=w;
            }
            y+=h;
        }
        if(!output->samples_) throw std::invalid_argument("tissue ROI contains no opaque pixel centers");
        if(cleanup) {
            if(phase) phase->store(4);const auto began=std::chrono::steady_clock::now();
            if(p.open_radius) {
                tissue_detail::square(labels,queue,domain,width,height,p.open_radius,false,cancelled);
                tissue_detail::square(labels,queue,domain,width,height,p.open_radius,true,cancelled);
            }
            if(p.close_radius) {
                tissue_detail::square(labels,queue,domain,width,height,p.close_radius,true,cancelled);
                tissue_detail::square(labels,queue,domain,width,height,p.close_radius,false,cancelled);
            }
            if(p.fill_holes) output->filled_pixels_=tissue_detail::fill(labels,queue,domain,width,height,p.connectivity,p.max_hole_pixels,cancelled);
            for(std::size_t i=0;i<count;++i) {if((i&4095)==0) check_cancelled(cancelled);output->cleaned_pixels_+=labels[i]!=0;}
            output->cleanup_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
            queue.clear();
        } else output->cleaned_pixels_=output->threshold_pixels_;
        if(phase) phase->store(2);
        for(std::size_t seed=0;seed<count;++seed) {
            if((seed&4095)==0) check_cancelled(cancelled);
            if(labels[seed]!=foreground) continue;
            if(++output->raw_components_>p.max_components) throw std::length_error("tissue component limit exceeded");
            queue.clear();queue.push_back(static_cast<std::uint32_t>(seed));labels[seed]=0;
            std::int64_t min_x=width,min_y=height,max_x=0,max_y=0;std::uint64_t sum_x=0,sum_y=0;
            for(std::size_t cursor=0;cursor<queue.size();++cursor) {
                if((cursor&4095)==0) check_cancelled(cancelled);
                const auto i=queue[cursor];const auto row=static_cast<std::int64_t>(i)/width,col=static_cast<std::int64_t>(i)%width;
                min_x=std::min(min_x,col);max_x=std::max(max_x,col);min_y=std::min(min_y,row);max_y=std::max(max_y,row);
                sum_x+=static_cast<std::uint64_t>(col);sum_y+=static_cast<std::uint64_t>(row);
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    if((dx==0 && dy==0) || (p.connectivity==4 && dx!=0 && dy!=0)) continue;
                    const auto xx=col+dx,yy=row+dy;if(xx<0 || yy<0 || xx>=width || yy>=height) continue;
                    const auto next=static_cast<std::size_t>(yy*width+xx);
                    if(labels[next]==foreground) {labels[next]=0;queue.push_back(static_cast<std::uint32_t>(next));}
                }
            }
            if(queue.size()<p.min_pixels || (p.max_pixels && queue.size()>p.max_pixels)) continue;
            const auto label=static_cast<std::uint32_t>(output->components_.size()+1);
            for(std::size_t i=0;i<queue.size();++i) {if((i&4095)==0) check_cancelled(cancelled);labels[queue[i]]=label;}
            const auto n=static_cast<std::uint64_t>(queue.size());output->foreground_pixels_+=n;
            output->components_.push_back({label,n,{double(request.x0_+min_x)*scale,double(request.y0_+min_y)*scale,double(max_x-min_x+1)*scale,double(max_y-min_y+1)*scale},
                {(double(request.x0_)+double(sum_x)/double(n)+.5)*scale,(double(request.y0_)+double(sum_y)/double(n)+.5)*scale}});
        }
        check_cancelled(cancelled);output->input_sha256_=digest.finish();
        output->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();if(phase) phase->store(3);
        return output;
    }
};
std::shared_ptr<const TissueResult> detect_tissue(const TissueRequest& request,const std::atomic<bool>* cancelled) {return TissueKernel::run(request,cancelled);}
struct TissueJob::Impl {
    struct Completion {std::shared_ptr<const TissueResult> result;std::shared_ptr<const TissueContours> contours;std::string error;double elapsed_ms=0;};
    std::optional<TissueRequest> request;
    std::future<Completion> future;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    bool stale=false;TissueStats stats;std::shared_ptr<const TissueResult> result;
    bool contouring=false,published=false;std::shared_ptr<const TissueContours> contours;
};
TissueJob::TissueJob():impl_(std::make_unique<Impl>()) {}
TissueJob::~TissueJob() {cancel();if(busy()) impl_->future.wait();}
bool TissueJob::busy() const {return impl_->future.valid();}
std::shared_ptr<const TissueResult> TissueJob::result() const {return impl_->result;}
std::shared_ptr<const TissueContours> TissueJob::contours() const {return impl_->contours;}
bool TissueJob::contours_published() const {return impl_->published;}
void TissueJob::cancel() {impl_->cancelled.store(true);impl_->result.reset();impl_->contours.reset();impl_->published=false;if(impl_->request) impl_->stats.state=busy()?"cancelling":"cancelled";if(!busy()) impl_->request.reset();}
bool TissueJob::request(const TissueRequest& request) {
    if(busy()) return false;
    if(impl_->stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("tissue generation exhausted");
    impl_->request=request;impl_->result.reset();impl_->stale=false;impl_->cancelled.store(false);impl_->completed.store(0);impl_->phase.store(0);
    impl_->contouring=false;impl_->published=false;impl_->contours.reset();
    ++impl_->stats.generation;impl_->stats.state="running";impl_->stats.phase="thresholding";impl_->stats.error.clear();impl_->stats.elapsed_ms=0;impl_->stats.completed_tiles=0;impl_->stats.total_tiles=request.total_tiles();
    try {impl_->future=std::async(std::launch::async,[state=impl_.get()] {
        Impl::Completion completion;const auto started=std::chrono::steady_clock::now();
        try {completion.result=TissueKernel::run(*state->request,&state->cancelled,&state->completed,&state->phase);} catch(const std::exception& e) {completion.error=e.what();}
        completion.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return completion;
    });} catch(...) {impl_->request.reset();impl_->stats.state="failed";impl_->stats.error="could not start tissue worker";throw;}
    return true;
}
TissueStats TissueJob::poll(const Document& document,std::uint64_t id) {
    if(impl_->request && !impl_->request->matches(document,id)) {impl_->stale=true;impl_->cancelled.store(true);impl_->result.reset();impl_->contours.reset();impl_->published=false;impl_->stats.state=busy()?"cancelling":"stale";}
    impl_->stats.completed_tiles=impl_->completed.load(std::memory_order_relaxed);
    const auto phase=impl_->phase.load();impl_->stats.phase=phase==5?"contours":phase==4?"cleanup":phase==2?"components":phase==3?"complete":phase==0 && !busy()?"idle":"thresholding";
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto completed=impl_->future.get();impl_->stats.elapsed_ms=completed.elapsed_ms;impl_->stats.completed_tiles=impl_->completed.load();
            if(impl_->stale) impl_->stats.state="stale";
            else if(impl_->cancelled.load()) impl_->stats.state="cancelled";
            else if(!completed.result || (impl_->contouring && !completed.contours)) {impl_->stats.state="failed";impl_->stats.error=std::move(completed.error);}
            else {impl_->result=std::move(completed.result);impl_->contours=std::move(completed.contours);impl_->stats.state="ready";impl_->stats.phase="complete";impl_->phase.store(3);}
        } catch(const std::exception& e) {impl_->stats.state="failed";impl_->stats.error=e.what();}
    }
    if(!busy() && !impl_->result) impl_->request.reset();return impl_->stats;
}
TissueStats TissueJob::wait(const Document& document,std::uint64_t id) {poll(document,id);if(busy()) impl_->future.wait();return poll(document,id);}
bool TissueJob::request_contours(TissueContourOptions options) {
    if(busy()) return false;
    options.validate();if(!has_geos()) throw std::runtime_error("tissue contours require QUPATHXX_BUILD_GEOS=ON");
    if(!impl_->request || !impl_->result || impl_->stale) throw std::logic_error("no current tissue mask to trace");
    if(impl_->stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("tissue generation exhausted");
    impl_->contouring=true;impl_->contours.reset();impl_->published=false;impl_->cancelled.store(false);impl_->completed.store(0);impl_->phase.store(5);
    ++impl_->stats.generation;impl_->stats.state="running";impl_->stats.phase="contours";impl_->stats.error.clear();impl_->stats.elapsed_ms=0;impl_->stats.completed_tiles=impl_->stats.total_tiles=0;
    try {impl_->future=std::async(std::launch::async,[state=impl_.get(),mask=impl_->result,options] {
        Impl::Completion completion;completion.result=mask;const auto began=std::chrono::steady_clock::now();
        try {completion.contours=trace_tissue(*mask,options,&state->cancelled);} catch(const std::exception& e) {completion.error=e.what();}
        completion.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return completion;
    });} catch(...) {impl_->stats.state="failed";impl_->stats.error="could not start contour worker";throw;}
    return true;
}
std::vector<std::uint64_t> TissueJob::keep_contours(Document& document,std::uint64_t id,std::string prefix,std::string classification) {
    if(poll(document,id).state!="ready" || !impl_->request || !impl_->contours) throw std::logic_error("no current tissue contours to publish");
    if(impl_->published) throw std::logic_error("tissue contours already published; use Undo/Redo for this batch");
    std::vector<ObjectEdit> edits;edits.reserve(impl_->contours->components().size());
    const auto parent=impl_->contours->options().clip_to_roi?id:0;
    for(const auto& component:impl_->contours->components()) edits.push_back(ObjectEdit::create_geometry(component.geometry,prefix+" "+std::to_string(component.label),parent,classification));
    auto ids=document.execute(edits,"Add tissue annotations");impl_->published=true;return ids;
}
std::uint64_t TissueJob::keep_result(Document& document,std::uint64_t id) {
    if(poll(document,id).state!="ready" || !impl_->request || !impl_->result) throw std::logic_error("no current tissue mask to keep");
    return document.keep_tissue(*impl_->request,impl_->result);
}
bool TissueJob::verify_result(Document& document,std::uint64_t record_id) {
    if(!impl_->request || poll(document,impl_->request->object_id()).state!="ready" || !impl_->result) throw std::logic_error("no current tissue mask to verify");
    return document.verify_tissue(record_id,*impl_->request,*impl_->result);
}
}
