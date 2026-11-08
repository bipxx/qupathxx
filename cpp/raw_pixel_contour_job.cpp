#include "raw_pixel_workflow_snapshot.hpp"
#include <chrono>
#include <future>
#include <thread>
namespace qpx {
namespace {
void owner(std::thread::id id) {if(std::this_thread::get_id()!=id) throw std::runtime_error("raw pixel contour controller must run on its owner thread");}
}
struct RawPixelContourJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::optional<RawPixelPredictionRequest> request;
    std::shared_ptr<const RawPixelClassification> prediction;
    std::future<std::shared_ptr<const RawPixelContours>> future;
    std::shared_ptr<const RawPixelContours> result;
    std::atomic<bool> cancelled{false};bool stale=false;RawPixelJobStats stats;
    std::chrono::steady_clock::time_point started;
};
RawPixelContourJob::RawPixelContourJob():impl_(std::make_unique<Impl>()) {}
RawPixelContourJob::~RawPixelContourJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelContourJob::busy() const {owner(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawPixelContourJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawPixelContourJob::await_completion() {owner(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelContourJob::request(const RawPixelPredictionRequest& request,std::shared_ptr<const RawPixelClassification> prediction,TissueContourOptions options) {
    owner(impl_->owner);auto& s=*impl_;if(busy()) return false;options.validate();if(!has_geos()) throw std::runtime_error("raw contours require QUPATHXX_BUILD_GEOS=ON");
    auto snapshot=RawPixelContourKernel::capture(request,prediction);if(s.stats.generation==UINT64_MAX) throw std::overflow_error("raw contour generation exhausted");
    const auto old_cancel=s.cancelled.load();s.cancelled.store(false);std::future<std::shared_ptr<const RawPixelContours>> future;
    try {future=std::async(std::launch::async,[state=&s,snapshot,prediction,options] {return RawPixelContourKernel::run(snapshot,prediction,options,&state->cancelled);});}catch(...) {s.cancelled.store(old_cancel);throw;}
    s.future=std::move(future);s.request=request;s.prediction=std::move(prediction);s.result.reset();s.stale=false;s.started=std::chrono::steady_clock::now();const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.phase="tracing";return true;
}
RawPixelJobStats RawPixelContourJob::poll(const Document& doc) {
    owner(impl_->owner);auto& s=*impl_;
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=s.future.get();if(s.stale || (!s.cancelled.load() && !RawPixelContourKernel::current(doc,*s.request,s.prediction))) {s.stale=true;s.stats.state="stale";}else if(s.cancelled.load()) s.stats.state="cancelled";else {s.result=std::move(result);s.stats.state="ready";}}
        catch(const std::exception& e) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";s.stats.error=e.what();s.result.reset();}
        s.stats.phase="finished";s.stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s.started).count();
    }
    if(!s.future.valid() && s.cancelled.load() && !s.stale && s.stats.state=="ready") s.stats.state="cancelled";
    if(!s.future.valid() && s.stats.state!="ready") {s.result.reset();s.request.reset();s.prediction.reset();}return s.stats;
}
RawPixelJobStats RawPixelContourJob::wait(const Document& doc) {owner(impl_->owner);poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const RawPixelContours> RawPixelContourJob::result() const {owner(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->result;}
}
