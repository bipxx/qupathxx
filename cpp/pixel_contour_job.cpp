#include "qupathxx/pixel_contours.hpp"
#include <chrono>
#include <future>
#include <limits>
#include <stdexcept>
namespace qpx {
struct PixelContourJob::Impl {
    struct Completion {std::shared_ptr<const PixelContours> result;std::string error;double ms=0;};
    std::optional<PixelPredictionRequest> request;
    std::future<Completion> future;
    std::atomic<bool> cancelled{false};
    bool stale=false;
    PixelTrainingStats stats;
    std::shared_ptr<const PixelContours> result;
};
PixelContourJob::PixelContourJob():impl_(std::make_unique<Impl>()) {}
PixelContourJob::~PixelContourJob() {cancel();if(busy()) impl_->future.wait();}
bool PixelContourJob::busy() const {return impl_->future.valid();}
void PixelContourJob::cancel() {
    auto& s=*impl_;s.cancelled.store(true);s.result.reset();
    if(s.request) s.stats.state=busy()?"cancelling":"cancelled";
    if(!busy()) s.request.reset();
}
bool PixelContourJob::request(const PixelPredictionRequest& request,std::shared_ptr<const PixelPredictionResult> result,TissueContourOptions options) {
    auto& s=*impl_;if(busy()) return false;
    if(!result || !result->matches(request)) throw std::invalid_argument("pixel contour prediction does not match the captured request");
    options.validate();
    if(s.stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("pixel contour generation exhausted");
    s.request=request;s.result.reset();s.stale=false;s.cancelled.store(false);++s.stats.generation;
    s.stats.state="running";s.stats.phase="tracing";s.stats.error.clear();s.stats.elapsed_ms=0;s.stats.completed_tiles=0;s.stats.total_tiles=0;
    try {
        s.future=std::async(std::launch::async,[state=&s,result=std::move(result),options] {
            Impl::Completion out;const auto began=std::chrono::steady_clock::now();
            try {out.result=trace_pixel_prediction(result,options,&state->cancelled);}catch(const std::exception& e) {out.error=e.what();}
            out.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return out;
        });
    }catch(...) {s.request.reset();s.stats.state="failed";s.stats.phase="finished";s.stats.error="could not start pixel contour worker";throw;}
    return true;
}
PixelTrainingStats PixelContourJob::poll(const Document& doc) {
    auto& s=*impl_;
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(busy() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto out=s.future.get();s.stats.elapsed_ms=out.ms;
            if(s.stale) s.stats.state="stale";
            else if(s.cancelled.load()) s.stats.state="cancelled";
            else if(!out.error.empty()) {s.stats.state="failed";s.stats.error=std::move(out.error);}
            else {s.result=std::move(out.result);s.stats.state="ready";}
        }catch(const std::exception& e) {s.stats.state="failed";s.stats.error=e.what();}
        s.stats.phase="finished";
    }
    if(!busy() && !s.result) s.request.reset();return s.stats;
}
void PixelContourJob::await_completion() {if(impl_->future.valid()) impl_->future.wait();}
PixelTrainingStats PixelContourJob::wait(const Document& doc) {poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const PixelContours> PixelContourJob::result() const {return impl_->result;}
}
