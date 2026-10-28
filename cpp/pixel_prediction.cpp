#include "pixel_workflow_snapshot.hpp"
#include "pixel_prediction_grid.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
void stop(const std::atomic<bool>* c) {if(c && c->load()) throw std::runtime_error("pixel prediction cancelled");}
double elapsed(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
std::int64_t edge(double x,double scale,std::int64_t limit) {const auto v=std::ceil(static_cast<long double>(x)/scale-.5L);return v<=0?0:v>=limit?limit:static_cast<std::int64_t>(v);}
std::int64_t origin(std::int64_t x,double scale) {const auto v=std::round(static_cast<long double>(x)*scale);if(!std::isfinite(v) || v<0 || v>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("pixel prediction read origin overflow");return static_cast<std::int64_t>(v);}
void real(Sha256& h,double x) {std::uint64_t b;std::memcpy(&b,&x,8);h.integer(b);}
}

bool PixelPredictionResult::matches(const PixelPredictionRequest& request) const {return snapshot_==request.data_;}
void PixelPredictionOptions::validate() const {if(tile_size<16 || tile_size>512 || work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024 || !std::isfinite(min_vote_fraction) || min_vote_fraction<0 || min_vote_fraction>1) throw std::invalid_argument("invalid source pixel prediction options");}
PixelPredictionRequest::PixelPredictionRequest(const Document& doc,std::uint64_t id,const PixelClassifier& model,PixelPredictionOptions options,std::optional<Rect> window):source_(doc.server()) {
    options.validate();if(options.min_vote_fraction==0) options.min_vote_fraction=0;auto data=std::make_shared<Data>(model,options);data->level=pixel_classifier_level(doc,model);data->metadata=source_->metadata();const auto object=doc.annotation(id);data->roi=object.geometry;data->id=id;data->revision=doc.revision();data->kind=object.kind;data->class_name=object.class_name;
    ObjectFeatureOptions guard;guard.calibrated=false;guard.snapshot_bytes=std::min<std::size_t>(options.work_bytes,256u*1024*1024);authority_.emplace(doc,std::vector<std::uint64_t>{id},guard);data->provenance=authority_->source_anchor();
    const auto original=data->roi.bounds();if(original.x<0 || original.y<0 || static_cast<long double>(original.x)+original.width>data->metadata.width || static_cast<long double>(original.y)+original.height>data->metadata.height) throw std::invalid_argument("pixel prediction ROI must lie inside the image");
    const auto b=detail::pixel_prediction_window(original,window);data->windowed=window.has_value();
    const auto& level=data->metadata.levels[data->level];data->x=edge(b.x,level.downsample,level.width);data->y=edge(b.y,level.downsample,level.height);data->w=edge(b.x+b.width,level.downsample,level.width)-data->x;data->h=edge(b.y+b.height,level.downsample,level.height)-data->y;
    if(data->w<=0 || data->h<=0) throw std::invalid_argument("pixel prediction ROI contains no sampled pixel centers");
    const auto snapshot=sizeof(Data)+data->class_name.size()+authority_->snapshot_bytes()+data->roi.payload_bytes()+data->provenance->payload_bytes()+262144;
    if(snapshot>=options.work_bytes || std::uint64_t(data->w)>(options.work_bytes-snapshot)/9/std::uint64_t(data->h)) throw std::length_error("pixel prediction output exceeds memory budget; select a smaller ROI");
    data->fixed=snapshot+static_cast<std::size_t>(data->w)*data->h*9;
    const auto halo=model.options().halo(),tw=std::min<std::int64_t>(data->w,options.tile_size),th=std::min<std::int64_t>(data->h,options.tile_size),cw=std::min(level.width,tw+2*halo),ch=std::min(level.height,th+2*halo);
    // Include caller-owned decoder scratch and the per-tile prediction masks;
    // the low-level estimate includes feature extraction and bounded votes.
    const auto external=static_cast<std::size_t>(cw)*static_cast<std::size_t>(ch)*8+
                        static_cast<std::size_t>(tw)*static_cast<std::size_t>(th)*9;
    if(external>options.work_bytes-data->fixed || options.work_bytes-data->fixed-external<1024*1024) throw std::length_error("pixel prediction tile scratch exceeds memory budget");
    data->feature_budget=std::min(model.options().work_bytes,options.work_bytes-data->fixed-external);auto feature_options=model.options();feature_options.work_bytes=data->feature_budget;
    const auto feature_work=pixel_feature_work_bytes(cw,ch,feature_options,PixelFeatureWindow{cw,ch,halo<cw-tw?halo:0,halo<ch-th?halo:0,tw,th});
    const auto batch=std::min<std::size_t>(512,static_cast<std::size_t>(tw)*th),columns=model.features().size(),classes=model.classes().size();
    const auto predict_work=262144+static_cast<std::size_t>(tw)*static_cast<std::size_t>(th)*(columns*4+10)+batch*(sizeof(ClassifierPrediction)+sizeof(std::size_t)+columns*4+classes*4);
    if(predict_work>data->feature_budget) throw std::length_error("pixel prediction votes exceed memory budget");data->work=data->fixed+external+std::max(feature_work,predict_work);
    data->tiles=((static_cast<std::uint64_t>(data->w)+options.tile_size-1)/options.tile_size)*((static_cast<std::uint64_t>(data->h)+options.tile_size-1)/options.tile_size);
    validate_region(data->metadata,{origin(data->x+data->w-1,level.downsample),origin(data->y+data->h-1,level.downsample),data->level,1,1});if(!authority_->matches(doc)) throw std::runtime_error("document changed during pixel prediction capture");data_=std::move(data);
}
bool PixelPredictionRequest::matches(const Document& doc) const {return authority_ && authority_->matches(doc);}
std::uint64_t PixelPredictionRequest::object_id() const {return data_->id;}
std::uint64_t PixelPredictionRequest::document_revision() const {return data_->revision;}
std::int32_t PixelPredictionRequest::level() const {return data_->level;}
double PixelPredictionRequest::downsample() const {return data_->metadata.levels[data_->level].downsample;}
Rect PixelPredictionRequest::level_bounds() const {return {double(data_->x),double(data_->y),double(data_->w),double(data_->h)};}
std::size_t PixelPredictionRequest::estimated_work_bytes() const {return data_->work;}
std::uint64_t PixelPredictionRequest::total_tiles() const {return data_->tiles;}
bool PixelPredictionRequest::windowed() const {return data_->windowed;}
bool PixelPredictionResult::windowed() const {return snapshot_->windowed;}
std::int64_t PixelPredictionResult::width() const {return snapshot_->w;}
std::int64_t PixelPredictionResult::height() const {return snapshot_->h;}
std::uint64_t PixelPredictionResult::object_id() const {return snapshot_->id;}
std::int32_t PixelPredictionResult::level() const {return snapshot_->level;}
double PixelPredictionResult::downsample() const {return snapshot_->metadata.levels[snapshot_->level].downsample;}
Rect PixelPredictionResult::bounds() const {const auto scale=downsample();return {snapshot_->x*scale,snapshot_->y*scale,snapshot_->w*scale,snapshot_->h*scale};}
Rect PixelPredictionResult::level_bounds() const {return {double(snapshot_->x),double(snapshot_->y),double(snapshot_->w),double(snapshot_->h)};}
const Roi& PixelPredictionResult::roi() const {return snapshot_->roi;}
const std::shared_ptr<const AnalysisProvenance>& PixelPredictionResult::provenance() const {return snapshot_->provenance;}
PixelPredictionOptions PixelPredictionResult::options() const {return snapshot_->options;}
const PixelClassifier& PixelPredictionResult::model() const {return snapshot_->model;}
std::size_t PixelPredictionResult::estimated_work_bytes() const {return snapshot_->work;}
struct PixelPredictionKernel {
    static void input_header(Sha256& input,const PixelPredictionRequest::Data& d) {const std::string tag="qpx.source-pixel-input/1";input.update(tag.data(),tag.size());input.integer(d.metadata.width);input.integer(d.metadata.height);input.integer(d.level);real(input,d.metadata.levels[d.level].downsample);input.integer(d.options.tile_size);}
    static std::string input(const PixelPredictionRequest& request,const std::atomic<bool>* cancelled) {
        stop(cancelled);const auto& d=*request.data_;const auto& p=d.options;const auto& level=d.metadata.levels[d.level];const auto halo=d.model.options().halo();Sha256 hash;input_header(hash,d);
        auto options=d.model.options();options.work_bytes=d.feature_budget;
        for(auto y=d.y;y<d.y+d.h;) {const auto h=std::min<std::int64_t>(p.tile_size,d.y+d.h-y);for(auto x=d.x;x<d.x+d.w;) {stop(cancelled);const auto w=std::min<std::int64_t>(p.tile_size,d.x+d.w-x),cx=std::max<std::int64_t>(0,x-halo),cy=std::max<std::int64_t>(0,y-halo),right=x+w+std::min(halo,level.width-x-w),bottom=y+h+std::min(halo,level.height-y-h),cw=right-cx,ch=bottom-cy;
            const auto pixels=request.source_->read_region({origin(cx,level.downsample),origin(cy,level.downsample),d.level,cw,ch});stop(cancelled);if(pixels.width()!=cw || pixels.height()!=ch || pixels.bytes().size()!=region_byte_count(cw,ch)) throw std::runtime_error("pixel prediction reader returned incorrect dimensions or storage");
            const auto digest=pixel_feature_input_sha256(pixels,d.model.transform(),options,PixelFeatureWindow{level.width,level.height,x,y,w,h,cx,cy},cancelled);hash.update(digest.data(),digest.size());
            for(std::int64_t yy=0;yy<h;++yy) {stop(cancelled);for(std::int64_t xx=0;xx<w;++xx) hash.integer(d.roi.contains({(double(x+xx)+.5)*level.downsample,(double(y+yy)+.5)*level.downsample}));}x+=w;}y+=h;
        }stop(cancelled);return hash.finish();
    }
    static std::shared_ptr<const PixelPredictionResult> run(const PixelPredictionRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr) {
        const auto start=Clock::now();stop(cancelled);const auto& d=*request.data_;const auto& p=d.options;const auto& level=d.metadata.levels[d.level];const auto halo=d.model.options().halo();auto out=std::shared_ptr<PixelPredictionResult>(new PixelPredictionResult());out->snapshot_=request.data_;const auto count=static_cast<std::size_t>(d.w)*d.h;
        out->labels_.resize(count);out->valid_.resize(count);out->fractions_.assign(count,std::numeric_limits<float>::quiet_NaN());out->counts_.resize(d.model.classes().size());Sha256 input;input_header(input,d);std::uint64_t tiles=0;
        for(auto y=d.y;y<d.y+d.h;) {const auto h=std::min<std::int64_t>(p.tile_size,d.y+d.h-y);for(auto x=d.x;x<d.x+d.w;) {stop(cancelled);const auto w=std::min<std::int64_t>(p.tile_size,d.x+d.w-x),cx=std::max<std::int64_t>(0,x-halo),cy=std::max<std::int64_t>(0,y-halo),right=x+w+std::min(halo,level.width-x-w),bottom=y+h+std::min(halo,level.height-y-h),cw=right-cx,ch=bottom-cy;
            const auto pixels=request.source_->read_region({origin(cx,level.downsample),origin(cy,level.downsample),d.level,cw,ch});stop(cancelled);if(pixels.width()!=cw || pixels.height()!=ch || pixels.bytes().size()!=region_byte_count(cw,ch)) throw std::runtime_error("pixel prediction reader returned incorrect dimensions or storage");
            const auto result=classify_pixels(pixels,d.model,p.min_vote_fraction,PixelFeatureWindow{level.width,level.height,x,y,w,h,cx,cy},d.feature_budget,cancelled);input.update(result->input_sha256().data(),result->input_sha256().size());
            for(std::int64_t yy=0;yy<h;++yy) {stop(cancelled);for(std::int64_t xx=0;xx<w;++xx) {const auto from=static_cast<std::size_t>(yy*w+xx),to=static_cast<std::size_t>((y+yy-d.y)*d.w+x+xx-d.x);const bool inside=d.roi.contains({(double(x+xx)+.5)*level.downsample,(double(y+yy)+.5)*level.downsample});input.integer(inside);if(!inside || !result->valid()[from]) continue;
                out->valid_[to]=1;++out->valid_pixels_;out->labels_[to]=result->labels()[from];out->fractions_[to]=result->vote_fractions()[from];if(out->labels_[to]) {++out->counts_[out->labels_[to]-1];++out->classified_pixels_;}
            }}if(completed) completed->store(++tiles);x+=w;}y+=h;
        }
        out->input_sha_=input.finish();Sha256 hash;const std::string domain="qpx.source-pixel-classification/1";hash.update(domain.data(),domain.size());hash.update(d.model.sha256().data(),d.model.sha256().size());real(hash,p.min_vote_fraction);hash.integer(d.metadata.width);hash.integer(d.metadata.height);hash.integer(level.width);hash.integer(level.height);hash.integer(d.level);real(hash,level.downsample);for(auto v:{d.x,d.y,d.w,d.h}) hash.integer(v);
        for(std::size_t i=0;i<count;++i) {if((i&4095)==0) stop(cancelled);hash.integer(out->valid_[i]);hash.integer(out->labels_[i]);std::uint32_t bits=0x7fc00000;if(std::isfinite(out->fractions_[i])) std::memcpy(&bits,&out->fractions_[i],4);hash.integer(bits);}out->sha_=hash.finish();out->ms_=elapsed(start);stop(cancelled);return out;
    }
};
std::string input_sha256(const PixelPredictionRequest& request,const std::atomic<bool>* cancelled) {return PixelPredictionKernel::input(request,cancelled);}
std::shared_ptr<const PixelPredictionResult> predict_pixel_region(const PixelPredictionRequest& request,const std::atomic<bool>* cancelled) {return PixelPredictionKernel::run(request,cancelled);}
struct PixelPredictionJob::Impl {
    struct Completion {std::shared_ptr<const PixelPredictionResult> result;std::string error;double ms=0;};
    std::optional<PixelPredictionRequest> request;std::future<Completion> future;std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};bool stale=false;PixelTrainingStats stats;std::shared_ptr<const PixelPredictionResult> result;
};
PixelPredictionJob::PixelPredictionJob():impl_(std::make_unique<Impl>()) {}
PixelPredictionJob::~PixelPredictionJob() {cancel();if(busy()) impl_->future.wait();}
bool PixelPredictionJob::busy() const {return impl_->future.valid();}
void PixelPredictionJob::cancel() {auto& s=*impl_;s.cancelled.store(true);s.result.reset();if(s.request) s.stats.state=busy()?"cancelling":"cancelled";if(!busy()) s.request.reset();}
bool PixelPredictionJob::request(const PixelPredictionRequest& request) {auto& s=*impl_;if(busy()) return false;if(s.stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("pixel prediction generation exhausted");s.request=request;s.result.reset();s.stale=false;s.cancelled.store(false);s.completed.store(0);++s.stats.generation;s.stats.state="running";s.stats.phase="predicting";s.stats.error.clear();s.stats.elapsed_ms=0;s.stats.completed_tiles=0;s.stats.total_tiles=request.total_tiles();
    try {s.future=std::async(std::launch::async,[state=&s] {Impl::Completion out;const auto start=Clock::now();try {out.result=PixelPredictionKernel::run(*state->request,&state->cancelled,&state->completed);}catch(const std::exception& e) {out.error=e.what();}out.ms=elapsed(start);return out;});}catch(...) {s.request.reset();s.stats.state="failed";s.stats.phase="finished";s.stats.error="could not start pixel prediction worker";throw;}return true;
}
PixelTrainingStats PixelPredictionJob::poll(const Document& doc) {auto& s=*impl_;if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=busy()?"cancelling":"stale";}s.stats.completed_tiles=s.completed.load();if(busy() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto out=s.future.get();s.stats.elapsed_ms=out.ms;s.stats.completed_tiles=s.completed.load();if(s.stale) s.stats.state="stale";else if(s.cancelled.load()) s.stats.state="cancelled";else if(!out.error.empty()) {s.stats.state="failed";s.stats.error=std::move(out.error);}else {s.result=std::move(out.result);s.stats.state="ready";}}catch(const std::exception& e) {s.stats.state="failed";s.stats.error=e.what();}s.stats.phase="finished";}if(!busy() && !s.result) s.request.reset();return s.stats;}
void PixelPredictionJob::await_completion() {if(impl_->future.valid()) impl_->future.wait();}
PixelTrainingStats PixelPredictionJob::wait(const Document& doc) {poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const PixelPredictionResult> PixelPredictionJob::result() const {return impl_->result;}
}
