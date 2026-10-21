#include "qupathxx/tissue.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
void check_cancelled(const std::atomic<bool>* flag) {if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("tissue detection cancelled");}
bool stain_feature(TissueFeature feature) {return feature!=TissueFeature::OpticalDensitySum;}
}
void TissueParameters::validate() const {
    if(feature!=TissueFeature::OpticalDensitySum && feature!=TissueFeature::Stain1 && feature!=TissueFeature::Stain2 && feature!=TissueFeature::Stain3)
        throw std::invalid_argument("invalid tissue feature");
    if(!std::isfinite(threshold)) throw std::invalid_argument("tissue threshold must be finite");
    if(connectivity!=4 && connectivity!=8) throw std::invalid_argument("tissue connectivity must be 4 or 8");
    if(!min_pixels || (max_pixels && max_pixels<min_pixels)) throw std::invalid_argument("invalid tissue component size range");
    if(tile_size<16 || tile_size>1024) throw std::invalid_argument("tissue tile size must be 16..1024");
    if(!max_components || max_components>1000000) throw std::invalid_argument("tissue component limit must be 1..1000000");
    if(!work_bytes || work_bytes>512u*1024u*1024u) throw std::invalid_argument("tissue work budget must be 1 byte..512 MiB");
}
TissueRequest::TissueRequest(const Document& document,std::uint64_t id,const StainTransform& transform,TissueParameters parameters,bool nucleus):
    source_(document,id,transform,stain_feature(parameters.feature)?AnalysisKind::StainMeans:AnalysisKind::OpticalDensityMeans,nucleus,parameters.tile_size),parameters_(parameters) {
    parameters_.validate();
    const auto w=static_cast<std::uint64_t>(source_.x1_-source_.x0_),h=static_cast<std::uint64_t>(source_.y1_-source_.y0_);
    const auto count=w*h;
    if(count>std::numeric_limits<std::uint32_t>::max()) throw std::length_error("tissue raster exceeds uint32 indexing range");
    component_capacity_=static_cast<std::size_t>(std::min<std::uint64_t>(count,parameters_.max_components));
    const auto estimate=count*8+component_capacity_*sizeof(TissueComponent)+16ull*parameters_.tile_size*parameters_.tile_size+65536;
    if(estimate>parameters_.work_bytes) throw std::length_error("tissue raster exceeds work budget; select a smaller ROI");
    estimated_work_bytes_=static_cast<std::size_t>(estimate);
}
struct TissueKernel {
    static std::shared_ptr<const TissueResult> run(const TissueRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr) {
        const auto started=std::chrono::steady_clock::now();check_cancelled(cancelled);
        const auto& source=request.source_;const auto& p=request.parameters_;
        auto output=std::shared_ptr<TissueResult>(new TissueResult());
        const auto width=source.x1_-source.x0_,height=source.y1_-source.y0_;
        const auto count=static_cast<std::size_t>(width*height);
        output->bounds_={static_cast<double>(source.x0_),static_cast<double>(source.y0_),static_cast<double>(width),static_cast<double>(height)};
        output->parameters_=p;output->estimated_work_bytes_=request.estimated_work_bytes_;
        output->object_id_=source.object_id_;output->object_revision_=source.object_revision_;output->document_revision_=source.document_revision_;
        output->nucleus_=source.nucleus_;output->white_=source.transform_.white();output->stains_=source.transform_.stains();
        output->labels_.resize(count);output->components_.reserve(request.component_capacity_);
        std::vector<std::uint32_t> queue;queue.reserve(count);
        auto& labels=output->labels_;constexpr auto foreground=std::numeric_limits<std::uint32_t>::max();
        Sha256 digest;constexpr char format[]="qpx.rgba-tiles/1";digest.update(format,sizeof(format)-1);
        digest.integer(static_cast<std::uint64_t>(source.source_->metadata().width));digest.integer(static_cast<std::uint64_t>(source.source_->metadata().height));
        for(auto value:{source.x0_,source.y0_,source.x1_,source.y1_}) digest.integer(static_cast<std::uint64_t>(value));digest.integer(p.tile_size);
        if(phase) phase->store(1);
        std::uint64_t tiles=0;
        for(auto y=source.y0_;y<source.y1_;) {
            const auto h=std::min<std::int64_t>(p.tile_size,source.y1_-y);
            for(auto x=source.x0_;x<source.x1_;) {
                check_cancelled(cancelled);const auto w=std::min<std::int64_t>(p.tile_size,source.x1_-x);
                const auto pixels=source.source_->read_region({x,y,0,w,h});
                if(pixels.width()!=w || pixels.height()!=h) throw std::runtime_error("reader returned incorrect tissue tile dimensions");
                for(auto value:{x,y,w,h}) digest.integer(static_cast<std::uint64_t>(value));digest.update(pixels.bytes().data(),pixels.bytes().size());
                const auto values=stain_feature(p.feature)?source.transform_.deconvolve(pixels,cancelled):source.transform_.optical_densities(pixels,cancelled);
                const auto& data=values.values();
                for(std::int64_t row=0;row<h;++row) {
                    check_cancelled(cancelled);
                    for(std::int64_t col=0;col<w;++col) {
                        const auto i=static_cast<std::size_t>((row*w+col)*3);
                        if(!std::isfinite(data[i])) continue;
                        if(source.roi_.kind()!=RoiKind::Rectangle && !source.roi_.contains({static_cast<double>(x+col)+.5,static_cast<double>(y+row)+.5})) continue;
                        ++output->samples_;
                        const double score=stain_feature(p.feature)?data[i+static_cast<int>(p.feature)-1]:double(data[i])+double(data[i+1])+double(data[i+2]);
                        if(score>=p.threshold) {labels[static_cast<std::size_t>((y-source.y0_+row)*width+x-source.x0_+col)]=foreground;++output->threshold_pixels_;}
                    }
                }
                ++tiles;if(completed) completed->store(tiles,std::memory_order_relaxed);x+=w;
            }
            y+=h;
        }
        if(!output->samples_) throw std::invalid_argument("tissue ROI contains no opaque pixel centers");
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
            output->components_.push_back({label,n,{double(source.x0_+min_x),double(source.y0_+min_y),double(max_x-min_x+1),double(max_y-min_y+1)},
                {double(source.x0_)+double(sum_x)/double(n)+.5,double(source.y0_)+double(sum_y)/double(n)+.5}});
        }
        check_cancelled(cancelled);output->input_sha256_=digest.finish();
        output->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();if(phase) phase->store(3);
        return output;
    }
};
std::shared_ptr<const TissueResult> detect_tissue(const TissueRequest& request,const std::atomic<bool>* cancelled) {return TissueKernel::run(request,cancelled);}
struct TissueJob::Impl {
    struct Completion {std::shared_ptr<const TissueResult> result;std::string error;double elapsed_ms=0;};
    std::optional<TissueRequest> request;
    std::future<Completion> future;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    bool stale=false;TissueStats stats;std::shared_ptr<const TissueResult> result;
};
TissueJob::TissueJob():impl_(std::make_unique<Impl>()) {}
TissueJob::~TissueJob() {cancel();if(busy()) impl_->future.wait();}
bool TissueJob::busy() const {return impl_->future.valid();}
std::shared_ptr<const TissueResult> TissueJob::result() const {return impl_->result;}
void TissueJob::cancel() {impl_->cancelled.store(true);impl_->result.reset();if(impl_->request) impl_->stats.state=busy()?"cancelling":"cancelled";if(!busy()) impl_->request.reset();}
bool TissueJob::request(const TissueRequest& request) {
    if(busy()) return false;
    if(impl_->stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("tissue generation exhausted");
    impl_->request=request;impl_->result.reset();impl_->stale=false;impl_->cancelled.store(false);impl_->completed.store(0);impl_->phase.store(0);
    ++impl_->stats.generation;impl_->stats.state="running";impl_->stats.phase="thresholding";impl_->stats.error.clear();impl_->stats.elapsed_ms=0;impl_->stats.completed_tiles=0;impl_->stats.total_tiles=request.total_tiles();
    try {impl_->future=std::async(std::launch::async,[state=impl_.get()] {
        Impl::Completion completion;const auto started=std::chrono::steady_clock::now();
        try {completion.result=TissueKernel::run(*state->request,&state->cancelled,&state->completed,&state->phase);} catch(const std::exception& e) {completion.error=e.what();}
        completion.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return completion;
    });} catch(...) {impl_->request.reset();impl_->stats.state="failed";impl_->stats.error="could not start tissue worker";throw;}
    return true;
}
TissueStats TissueJob::poll(const Document& document,std::uint64_t id) {
    if(impl_->request && !impl_->request->matches(document,id)) {impl_->stale=true;impl_->cancelled.store(true);impl_->result.reset();impl_->stats.state=busy()?"cancelling":"stale";}
    impl_->stats.completed_tiles=impl_->completed.load(std::memory_order_relaxed);
    const auto phase=impl_->phase.load();impl_->stats.phase=phase==2?"components":phase==3?"complete":phase==0 && !busy()?"idle":"thresholding";
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto completed=impl_->future.get();impl_->stats.elapsed_ms=completed.elapsed_ms;impl_->stats.completed_tiles=impl_->completed.load();
            if(impl_->stale) impl_->stats.state="stale";
            else if(impl_->cancelled.load()) impl_->stats.state="cancelled";
            else if(!completed.result) {impl_->stats.state="failed";impl_->stats.error=std::move(completed.error);}
            else {impl_->result=std::move(completed.result);impl_->stats.state="ready";impl_->stats.phase="complete";}
        } catch(const std::exception& e) {impl_->stats.state="failed";impl_->stats.error=e.what();}
    }
    if(!busy() && !impl_->result) impl_->request.reset();return impl_->stats;
}
TissueStats TissueJob::wait(const Document& document,std::uint64_t id) {poll(document,id);if(busy()) impl_->future.wait();return poll(document,id);}
}
