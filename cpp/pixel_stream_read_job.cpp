#include "qupathxx/pixel_stream_file.hpp"
#include "atomic_export.hpp"
#include <chrono>
#include <future>
#include <limits>
namespace qpx {
struct PixelStreamReadJob::Impl {
    struct Completion {std::shared_ptr<PixelStreamFile> file;std::shared_ptr<const PixelPredictionResult> block;std::shared_ptr<const PixelStreamFileSummary> summary;std::shared_ptr<const PixelStreamLink> link;std::string error;double ms=0;};
    std::shared_ptr<PixelStreamFile> file;
    std::future<Completion> future;std::atomic<bool> cancelled{false};
    PixelStreamReadStats stats;
    void begin(const char* state) {
        if(stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("pixel stream read generation exhausted");
        ++stats.generation;stats.state=state;stats.error.clear();stats.elapsed_ms=0;cancelled.store(false);
    }
    template<class Work> void start(Work work) {
        try {future=std::async(std::launch::async,[this,work=std::move(work)] {
            Completion out;const auto began=std::chrono::steady_clock::now();
            try {detail::export_checkpoint(&cancelled);work(out);detail::export_checkpoint(&cancelled);}catch(const std::exception& e) {out.error=e.what();}
            out.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return out;
        });}catch(...) {stats.state="failed";stats.error="could not start pixel stream read worker";throw;}
    }
};
PixelStreamReadJob::PixelStreamReadJob():impl_(std::make_unique<Impl>()) {}
PixelStreamReadJob::~PixelStreamReadJob() {cancel();if(busy()) impl_->future.wait();}
bool PixelStreamReadJob::busy() const {return impl_->future.valid();}
void PixelStreamReadJob::cancel() {
    auto& s=*impl_;if(!busy() && !s.file) return;s.cancelled.store(true);if(s.file) s.file->cancel();s.stats.block.reset();s.stats.verified=false;s.stats.link.reset();s.stats.error.clear();s.stats.state=busy()?"cancelling":"cancelled";
}
bool PixelStreamReadJob::open(const std::string& path,std::uint64_t index,std::uint64_t max_bytes) {
    return open_impl(path,index,max_bytes,{});
}
bool PixelStreamReadJob::open_link(std::shared_ptr<const PixelStreamLink> expected,std::uint64_t index,std::uint64_t max_bytes) {
    if(busy()) return false;if(!expected) throw std::invalid_argument("pixel stream link is required");const auto path=expected->path();return open_impl(path,index,max_bytes,std::move(expected));
}
bool PixelStreamReadJob::open_impl(const std::string& path,std::uint64_t index,std::uint64_t max_bytes,std::shared_ptr<const PixelStreamLink> expected) {
    if(busy()) return false;if(max_bytes<128 || max_bytes>(1ULL<<46)) throw std::invalid_argument("invalid pixel stream file byte limit");
    auto& s=*impl_;const auto generation=s.stats.generation;s.stats={};s.stats.generation=generation;s.begin("opening");s.stats.block_index=index;s.file.reset();
    s.start([state=&s,path,index,max_bytes,expected=std::move(expected)](Impl::Completion& out) {
        out.file=std::make_shared<PixelStreamFile>(path,max_bytes);detail::export_checkpoint(&state->cancelled);
        if(expected) {out.file->verify(&state->cancelled);out.link=out.file->link();if(!expected->same_content(*out.link)) throw std::invalid_argument("saved pixel stream content does not match project link");}
        out.summary=std::make_shared<const PixelStreamFileSummary>(out.file->declared_summary());out.block=out.file->read_block(index);
    });return true;
}
bool PixelStreamReadJob::read(std::uint64_t index) {
    auto& s=*impl_;if(busy()) return false;if(!s.file || s.file->cancelled()) throw std::logic_error("open a pixel stream file before reading");
    s.begin("reading");s.stats.block_index=index;s.stats.block.reset();
    s.start([state=&s,index](Impl::Completion& out) {out.block=state->file->read_block(index);});return true;
}
bool PixelStreamReadJob::verify() {
    auto& s=*impl_;if(busy()) return false;if(!s.file || s.file->cancelled()) throw std::logic_error("open a pixel stream file before verification");
    s.begin("verifying");s.stats.verified=false;
    s.start([state=&s](Impl::Completion& out) {state->file->verify(&state->cancelled);out.link=state->file->link();});return true;
}
bool PixelStreamReadJob::close() {
    auto& s=*impl_;if(busy()) return false;s.file.reset();const auto generation=s.stats.generation;s.stats={};s.stats.generation=generation;return true;
}
PixelStreamReadStats PixelStreamReadJob::poll() {
    auto& s=*impl_;
    if(busy() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto out=s.future.get();s.stats.elapsed_ms=out.ms;
            if(s.cancelled.load()) {s.stats.state="cancelled";s.stats.block.reset();s.stats.verified=false;s.stats.link.reset();}
            else if(!out.error.empty()) {s.stats.state="failed";s.stats.error=std::move(out.error);s.stats.block.reset();s.stats.verified=false;s.stats.link.reset();}
            else {
                if(out.file) {s.file=std::move(out.file);s.stats.declared_summary=std::move(out.summary);s.stats.level_bounds=s.file->level_bounds();s.stats.block_size=s.file->block_size();s.stats.windowed=s.file->windowed();}
                if(out.link) s.stats.link=std::move(out.link);if(out.block) s.stats.block=std::move(out.block);s.stats.verified=s.file->verified();s.stats.state="ready";
            }
        }catch(const std::exception& e) {s.stats.state="failed";s.stats.error=e.what();s.stats.block.reset();s.stats.verified=false;s.stats.link.reset();}
    }
    return s.stats;
}
PixelStreamReadStats PixelStreamReadJob::wait() {if(busy()) impl_->future.wait();return poll();}
}
