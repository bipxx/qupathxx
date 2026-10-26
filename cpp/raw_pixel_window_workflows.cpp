#include "raw_pixel_window_prediction_internal.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
#include "scientific_raw_sampling_internal.hpp"
#include "raw_analysis_internal.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <thread>
namespace qpx {
namespace {
void stopped(const std::atomic<bool>* flag) {if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("raw window prediction cancelled");}
void owner(std::thread::id id) {if(std::this_thread::get_id()!=id) throw std::runtime_error("raw window prediction controller must run on its owner thread");}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
std::string result_hash(const RawPixelWindowClassification& r,double minimum,const std::atomic<bool>* cancelled) {
    Sha256 hash;text(hash,"qpx.raw-pixel-window-region/1");text(hash,r.model_sha256());text(hash,r.feature_sha256());text(hash,r.input_sha256());std::uint64_t bits;std::memcpy(&bits,&minimum,8);hash.integer(bits);
    const auto& w=r.window();for(auto v:{w.image_width(),w.image_height(),w.x().origin(),w.x().extent(),w.x().samples(),w.x().context(),w.y().origin(),w.y().extent(),w.y().samples(),w.y().context(),w.x().first_core_index(),w.y().first_core_index(),w.core_width(),w.core_height()}) hash.integer(v);
    for(std::size_t i=0;i<r.labels().size();++i) {if((i&4095)==0) stopped(cancelled);hash.integer(r.valid()[i]);hash.integer(r.labels()[i]);std::uint32_t value=0x7fc00000;if(r.valid()[i]) std::memcpy(&value,&r.vote_fractions()[i],4);hash.integer(value);}return hash.finish();
}
}
std::string RawPixelWindowPredictionKernel::result_sha256(const RawPixelWindowClassification& result,double minimum,const std::atomic<bool>* cancelled) {return result_hash(result,minimum,cancelled);}
RawPixelWindowPredictionRequest::RawPixelWindowPredictionRequest(const Document& doc,const RawPixelClassifier& model,ScientificRawWindow window,std::uint64_t object,double minimum,std::size_t budget):source_(doc.server()),identity_(doc.analysis_identity_.token),revision_(doc.revision()) {
    if(!std::isfinite(minimum) || minimum<0 || minimum>1) throw std::invalid_argument("raw window minimum vote fraction must be 0..1");if(minimum==0) minimum=0;
    if(!budget) budget=model.recipe().features().work_bytes;if(budget<1024*1024 || budget>1024ULL*1024*1024) throw std::invalid_argument("raw window prediction budget must be 1..1024 MiB");
    auto source=raw_pixel_capture_window_source(doc,window,object,budget);auto data=std::make_shared<Data>(std::move(window),model,std::move(source));data->minimum=minimum;data->budget=budget;
    if(object) {const auto& a=doc.annotation(object);if(!a.geometry.is_area()) throw std::invalid_argument("raw window prediction requires an area ROI");data->mask=a.geometry;}
    const auto count=std::uint64_t(data->window.core_width())*std::uint64_t(data->window.core_height());
    data->fixed=sizeof(Data)+raw_pixel_source_snapshot_bytes(data->source)+2*model.bytes().size()+2*(data->mask?data->mask->payload_bytes():0)+9*count+262144;
    if(data->fixed>=budget || budget-data->fixed<1024*1024) throw std::length_error("raw window capture/output leaves insufficient feature workspace");
    data->work=data->fixed+RawPixelClassifierKernel::plan(data->source.metadata,data->window,model.recipe().selection(),model,budget-data->fixed,data->source.calibration).work;
    data_=std::move(data);if(!matches(doc)) throw std::runtime_error("document changed during raw window capture");
}
bool RawPixelWindowPredictionRequest::matches(const Document& doc) const {return identity_==doc.analysis_identity_.token && revision_==doc.revision() && source_==doc.server() && raw_pixel_source_matches(*source_,data_->source);}
bool RawPixelWindowPredictionRequest::publication_matches(const Document& doc) const {return matches(doc) && raw_pixel_publication_window_source_matches(*source_,data_->source,data_->window);}
const ScientificRawWindow& RawPixelWindowPredictionRequest::window() const {return data_->window;}
const RawPixelClassifier& RawPixelWindowPredictionRequest::model() const {return data_->model;}
std::size_t RawPixelWindowPredictionRequest::estimated_work_bytes() const {return data_->work;}
std::shared_ptr<const RawPixelWindowClassification> RawPixelWindowPredictionKernel::run_window(const RawPixelWindowPredictionRequest::Data& d,const ScientificRawWindow& window,ImageServer& source,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed,std::atomic<int>* phase) {
        stopped(cancelled);if(!raw_pixel_source_matches(source,d.source)) throw std::runtime_error("raw window source changed before prediction");const auto plan=RawPixelClassifierKernel::plan(d.source.metadata,window,d.model.recipe().selection(),d.model,d.budget-d.fixed,d.source.calibration);if(phase) phase->store(1);
        auto selection=d.model.recipe().selection();selection.work_bytes=std::min(selection.work_bytes,d.budget-d.fixed);detail::ScientificRawReserve reserve;reserve.expected_series=d.model.recipe().series();
        const auto input=detail::read_scientific_raw_window_reserved(source,window,selection,reserve,cancelled);raw_pixel_verify_window_sample(source,d.source,d.window,input);if(phase) phase->store(2);
        const auto classified=RawPixelClassifierKernel::run(input,d.model,d.minimum,d.budget-d.fixed,cancelled,d.source.calibration);auto output=std::make_shared<RawPixelWindowClassification>(*classified);output->work_=d.fixed+plan.work;std::fill(output->counts_.begin(),output->counts_.end(),0);
        const auto width=std::size_t(window.core_width()),height=std::size_t(window.core_height());
        for(std::size_t y=0;y<height;++y) {stopped(cancelled);for(std::size_t x=0;x<width;++x) {if((x&4095)==0) stopped(cancelled);const auto i=y*width+x;const bool inside=!d.mask || d.mask->contains({window.x().center(window.x().first_core_index()+std::int64_t(x)).binary64(),window.y().center(window.y().first_core_index()+std::int64_t(y)).binary64()});if(!inside) {output->valid_[i]=0;output->labels_[i]=0;output->fractions_[i]=std::numeric_limits<float>::quiet_NaN();}if(output->labels_[i]) ++output->counts_[output->labels_[i]-1];if(completed) completed->store(i+1,std::memory_order_relaxed);}}
        output->input_sha_=raw_grid_analysis_hash(classified->input_sha256(),d.source.calibration.x,d.source.calibration.y,d.mask);output->sha_=result_hash(*output,d.minimum,cancelled);output->capture_=d.capture;raw_pixel_verify_window_sample(source,d.source,d.window,input);stopped(cancelled);return output;
}
std::shared_ptr<const RawPixelWindowClassification> RawPixelWindowPredictionKernel::run(const RawPixelWindowPredictionRequest::Data& d,ImageServer& source,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed,std::atomic<int>* phase) {return run_window(d,d.window,source,cancelled,completed,phase);}
std::shared_ptr<const RawPixelWindowClassification> RawPixelWindowPredictionKernel::run(const RawPixelWindowPredictionRequest& request,const std::atomic<bool>* cancelled) {return run(*request.data_,*request.source_,cancelled);}
std::shared_ptr<const RawPixelWindowClassification> predict_raw_pixel_window(const RawPixelWindowPredictionRequest& request,const std::atomic<bool>* cancelled) {return RawPixelWindowPredictionKernel::run(request,cancelled);}
struct RawPixelWindowPredictionJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();std::optional<RawPixelWindowPredictionRequest> request;
    std::future<std::shared_ptr<const RawPixelWindowClassification>> future;std::shared_ptr<const RawPixelWindowClassification> result;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    bool stale=false;RawPixelJobStats stats;std::chrono::steady_clock::time_point started;
};
RawPixelWindowPredictionJob::RawPixelWindowPredictionJob():impl_(std::make_unique<Impl>()) {}
RawPixelWindowPredictionJob::~RawPixelWindowPredictionJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelWindowPredictionJob::busy() const {owner(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawPixelWindowPredictionJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawPixelWindowPredictionJob::await_completion() {owner(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelWindowPredictionJob::request(const RawPixelWindowPredictionRequest& request) {
    owner(impl_->owner);auto& s=*impl_;if(busy()) return false;if(s.stats.generation==UINT64_MAX) throw std::overflow_error("raw window prediction generation exhausted");
    auto captured=request;auto data=captured.data_;auto* source=captured.source_.get();const auto old_cancel=s.cancelled.load();const auto old_completed=s.completed.load();const auto old_phase=s.phase.load();s.cancelled.store(false);s.completed.store(0);s.phase.store(0);std::future<std::shared_ptr<const RawPixelWindowClassification>> future;
    try {future=std::async(std::launch::async,[state=&s,data,source] {return RawPixelWindowPredictionKernel::run(*data,*source,&state->cancelled,&state->completed,&state->phase);});}catch(...) {s.cancelled.store(old_cancel);s.completed.store(old_completed);s.phase.store(old_phase);throw;}
    s.future=std::move(future);s.request=std::move(captured);s.result.reset();s.stale=false;s.started=std::chrono::steady_clock::now();const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.total_samples=std::uint64_t(request.window().core_width())*std::uint64_t(request.window().core_height());return true;
}
RawPixelJobStats RawPixelWindowPredictionJob::poll(const Document& doc) {
    owner(impl_->owner);auto& s=*impl_;if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=s.future.get();if(s.stale || (!s.cancelled.load() && !s.request->publication_matches(doc))) {s.stale=true;s.stats.state="stale";}else if(s.cancelled.load()) s.stats.state="cancelled";else {s.result=std::move(result);s.stats.state="ready";}}
        catch(const std::exception& e) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";try {s.stats.error=e.what();detail::bounded_utf8(s.stats.error,8192);}catch(...) {s.stats.error="Raw window prediction failed";}s.result.reset();}
        s.stats.phase="finished";s.stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s.started).count();
    }
    if(!s.future.valid() && s.cancelled.load() && !s.stale && s.stats.state=="ready") s.stats.state="cancelled";
    if(!s.future.valid() && s.stats.state!="ready") {s.result.reset();s.request.reset();}
    s.stats.completed_samples=s.completed.load();if(s.future.valid()) s.stats.phase=s.phase.load()==1?"reading":s.phase.load()==2?"predicting":"starting";return s.stats;
}
RawPixelJobStats RawPixelWindowPredictionJob::wait(const Document& doc) {owner(impl_->owner);poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const RawPixelWindowClassification> RawPixelWindowPredictionJob::result() const {owner(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->result;}
}
