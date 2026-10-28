#include "qupathxx/pixel_stream_file.hpp"
#include <chrono>
#include <future>
#include <limits>
namespace qpx {
struct PixelStreamExportJob::Impl {
    struct Completion {std::shared_ptr<const PixelStreamFileSummary> summary;std::string error;double ms=0;};
    std::unique_ptr<PixelPredictionStream> producer;
    std::future<Completion> future;
    std::atomic<bool> cancelled{false};
    PixelStreamExportStats stats;
};
PixelStreamExportJob::PixelStreamExportJob():impl_(std::make_unique<Impl>()) {}
PixelStreamExportJob::~PixelStreamExportJob() {cancel();if(busy()) impl_->future.wait();}
bool PixelStreamExportJob::busy() const {return impl_->future.valid();}
void PixelStreamExportJob::cancel() {
    auto& s=*impl_;if(!busy()) return;s.cancelled.store(true);if(s.producer) s.producer->cancel();s.stats.state="cancelling";
}
bool PixelStreamExportJob::request(const Document& doc,std::uint64_t object,const PixelClassifier& model,const std::string& path,PixelStreamOptions options,std::optional<Rect> window,bool replace,std::uint64_t max_bytes) {
    auto& s=*impl_;if(busy()) return false;
    if(s.stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("pixel export generation exhausted");
    if(max_bytes<128 || max_bytes>(1ULL<<46)) throw std::invalid_argument("invalid pixel stream file byte limit");
    auto producer=std::make_unique<PixelPredictionStream>(doc,object,model,options,window);
    const auto generation=s.stats.generation+1;s.producer=std::move(producer);s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.current=true;
    s.stats.total_blocks=s.producer->total_blocks();s.stats.estimated_work_bytes=s.producer->estimated_work_bytes();s.cancelled.store(false);
    try {
        s.future=std::async(std::launch::async,[state=&s,path,replace,max_bytes] {
            Impl::Completion out;const auto began=std::chrono::steady_clock::now();
            try {auto summary=std::make_shared<PixelStreamFileSummary>();*summary=save_pixel_stream(*state->producer,path,replace,max_bytes,&state->cancelled);out.summary=std::move(summary);}
            catch(const std::exception& e) {out.error=e.what();}
            out.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return out;
        });
    }catch(...) {s.producer.reset();s.stats.state="failed";s.stats.error="could not start pixel export worker";throw;}
    return true;
}
PixelStreamExportStats PixelStreamExportJob::poll(const Document& doc) {
    auto& s=*impl_;
    if(s.producer) {s.stats.current=s.producer->matches(doc);s.stats.completed_blocks=s.producer->completed_blocks();s.stats.valid_pixels=s.producer->valid_pixels();s.stats.classified_pixels=s.producer->classified_pixels();}
    if(busy() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto out=s.future.get();s.stats.elapsed_ms=out.ms;
            // A completed commit wins over a late cancellation request.
            if(out.summary) {s.stats.summary=std::move(out.summary);s.stats.state="saved";}
            else if(s.cancelled.load()) s.stats.state="cancelled";
            else {s.stats.state="failed";s.stats.error=std::move(out.error);}
        }catch(const std::exception& e) {s.stats.state="failed";s.stats.error=e.what();}
        if(s.producer) {s.stats.completed_blocks=s.producer->completed_blocks();s.stats.valid_pixels=s.producer->valid_pixels();s.stats.classified_pixels=s.producer->classified_pixels();}
    }
    return s.stats;
}
void PixelStreamExportJob::await_completion() {if(impl_->future.valid()) impl_->future.wait();}
PixelStreamExportStats PixelStreamExportJob::wait(const Document& doc) {poll(doc);await_completion();return poll(doc);}
}
