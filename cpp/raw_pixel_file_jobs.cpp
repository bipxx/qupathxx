#include "qupathxx/raw_pixel_file_jobs.hpp"
#include "raw_pixel_file_job_internal.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"

namespace qpx {
namespace {
void same_summary(const RawPixelStreamFileSummary& a,const RawPixelStreamFileSummary& b) {
    if(a.bytes!=b.bytes || a.total_windows!=b.total_windows || a.completed_windows!=b.completed_windows || a.valid_samples!=b.valid_samples || a.classified_samples!=b.classified_samples || a.class_counts!=b.class_counts || a.complete!=b.complete || a.input_sha256!=b.input_sha256 || a.sha256!=b.sha256 || a.content_sha256!=b.content_sha256)
        throw std::runtime_error("raw prediction output changed after save");
}
struct CheckpointOutput {
    RawPixelTrainingCheckpointResult result;std::string path;std::shared_ptr<const FileVersion> version;
    void check() const {
        detail::ReaderFileGuard guard(path);const auto& a=*version;const auto& b=*guard.version();
        if(a.bytes!=b.bytes || a.identity_scheme!=b.identity_scheme || a.identity!=b.identity || a.stamp_scheme!=b.stamp_scheme || a.modified_token!=b.modified_token || a.change_token!=b.change_token) throw std::runtime_error("raw training checkpoint output changed before delivery");guard.verify();
    }
};
struct StreamOutput {RawPixelStreamFileSummary summary;std::shared_ptr<const RawPixelStreamFile> file;};
struct ReadOutput {std::shared_ptr<const RawPixelStreamFile> file;std::shared_ptr<const RawPixelWindowClassification> block;std::shared_ptr<const RawPixelFileContours> contours;};
}
struct RawPixelStreamSaveJob::Impl {
    std::optional<RawPixelTiledPredictionRequest> capture;
    detail::RawPixelFileJobState<StreamOutput> state;
};
RawPixelStreamSaveJob::RawPixelStreamSaveJob():impl_(std::make_unique<Impl>()) {}
RawPixelStreamSaveJob::~RawPixelStreamSaveJob()=default;
bool RawPixelStreamSaveJob::busy() const {return impl_->state.busy();}
void RawPixelStreamSaveJob::cancel() {impl_->state.cancelled.store(true);}
void RawPixelStreamSaveJob::await_completion() {impl_->state.join();}
bool RawPixelStreamSaveJob::request(const RawPixelTiledPredictionRequest& request,const std::string& input,RawPixelStreamSaveOptions options) {
    auto& s=*impl_;s.state.on_owner();if(s.state.future.valid() || s.state.pending) return false;options.limits.validate();const auto path=detail::export_path(input);
    if(!s.state.start([request,path,options](const std::atomic<bool>* c) {StreamOutput out;out.summary=save_raw_pixel_stream(request,path,options,c);detail::export_checkpoint(c);out.file=std::make_shared<const RawPixelStreamFile>(path,options.limits);out.file->verify(c);same_summary(out.summary,out.file->declared_summary());out.file->check_unchanged();detail::export_checkpoint(c);return out;},"saving prediction file",request.total_windows())) return false;
    s.capture=request;return true;
}
RawPixelFileJobStats RawPixelStreamSaveJob::poll(const Document& doc) {
    auto& s=*impl_;s.state.poll([&] {return s.capture && s.capture->matches(doc);},[&](const StreamOutput& out) {if(!s.capture->publication_matches(doc) || !out.file->verified()) return false;out.file->check_unchanged();return true;});
    if(s.state.pending) s.state.stats.completed_windows=s.state.pending->summary.completed_windows;
    if(!s.state.future.valid() && !s.state.pending) s.capture.reset();return s.state.stats;
}
RawPixelFileJobStats RawPixelStreamSaveJob::wait(const Document& doc) {poll(doc);await_completion();return poll(doc);}
std::optional<RawPixelStreamFileSummary> RawPixelStreamSaveJob::take(const Document& doc) {
    if(poll(doc).state!="ready") return {};auto out=impl_->state.take();if(!out) return {};impl_->capture.reset();return std::move(out->summary);
}
std::shared_ptr<const RawPixelStreamLink> RawPixelStreamSaveJob::take_link(const Document& doc) {
    if(poll(doc).state!="ready") return {};auto& s=impl_->state;auto link=s.pending->file->link();auto out=s.take();if(!out) return {};impl_->capture.reset();return link;
}
struct RawPixelTrainingCheckpointJob::Impl {
    std::optional<RawPixelTiledTrainingRequest> capture;
    detail::RawPixelFileJobState<CheckpointOutput> state;
};
RawPixelTrainingCheckpointJob::RawPixelTrainingCheckpointJob():impl_(std::make_unique<Impl>()) {}
RawPixelTrainingCheckpointJob::~RawPixelTrainingCheckpointJob()=default;
bool RawPixelTrainingCheckpointJob::busy() const {return impl_->state.busy();}
void RawPixelTrainingCheckpointJob::cancel() {impl_->state.cancelled.store(true);}
void RawPixelTrainingCheckpointJob::await_completion() {impl_->state.join();}
bool RawPixelTrainingCheckpointJob::request(const RawPixelTiledTrainingRequest& request,const std::string& input,RawPixelTrainingCheckpointOptions options) {
    auto& s=*impl_;s.state.on_owner();if(s.state.future.valid() || s.state.pending) return false;options.limits.validate();const auto path=detail::export_path(input);
    if(!s.state.start([request,path,options](const std::atomic<bool>* c) {CheckpointOutput out;out.result=save_raw_pixel_training_checkpoint(request,path,options,c);out.path=path;detail::ReaderFileGuard guard(path);out.version=guard.version();const auto saved=load_raw_pixel_training_checkpoint(path,options.limits,c);if(saved->bytes()!=out.result.checkpoint->bytes()) throw std::runtime_error("raw training checkpoint output changed after save");guard.verify();detail::export_checkpoint(c);return out;},"saving training checkpoint",request.window_count())) return false;
    s.capture=request;return true;
}
RawPixelFileJobStats RawPixelTrainingCheckpointJob::poll(const Document& doc) {
    auto& s=*impl_;s.state.poll([&] {return s.capture && s.capture->matches(doc);},[&](const CheckpointOutput& out) {if(!s.capture->publication_matches(doc)) return false;out.check();return true;});
    if(s.state.pending) s.state.stats.completed_windows=s.state.pending->result.checkpoint->completed_windows();
    if(!s.state.future.valid() && !s.state.pending) s.capture.reset();return s.state.stats;
}
RawPixelFileJobStats RawPixelTrainingCheckpointJob::wait(const Document& doc) {poll(doc);await_completion();return poll(doc);}
std::optional<RawPixelTrainingCheckpointResult> RawPixelTrainingCheckpointJob::take(const Document& doc) {
    if(poll(doc).state!="ready") return {};auto out=impl_->state.take();if(!out) return {};impl_->capture.reset();return std::move(out->result);
}
struct RawPixelStreamFileReadJob::Impl {
    bool opening=false,tracing=false;detail::RawPixelFileJobState<ReadOutput> state;
};
RawPixelStreamFileReadJob::RawPixelStreamFileReadJob():impl_(std::make_unique<Impl>()) {}
RawPixelStreamFileReadJob::~RawPixelStreamFileReadJob()=default;
bool RawPixelStreamFileReadJob::busy() const {return impl_->state.busy();}
void RawPixelStreamFileReadJob::cancel() {impl_->state.cancelled.store(true);}
void RawPixelStreamFileReadJob::await_completion() {impl_->state.join();}
bool RawPixelStreamFileReadJob::open(const std::string& input,RawPixelStreamFileLimits limits) {
    auto& s=*impl_;s.state.on_owner();if(s.state.future.valid() || s.state.pending) return false;limits.validate();const auto path=detail::export_path(input);
    if(!s.state.start([path,limits](const std::atomic<bool>* c) {detail::export_checkpoint(c);auto file=std::make_shared<const RawPixelStreamFile>(path,limits);file->verify(c);detail::export_checkpoint(c);return ReadOutput{std::move(file),{}};},"verifying prediction file")) return false;s.opening=true;s.tracing=false;return true;
}
bool RawPixelStreamFileReadJob::open_link(std::shared_ptr<const RawPixelStreamLink> link,RawPixelStreamFileLimits limits) {
    auto& s=*impl_;s.state.on_owner();if(s.state.future.valid() || s.state.pending) return false;if(!link) throw std::invalid_argument("raw stream link is required");limits.validate();
    if(!s.state.start([link=std::move(link),limits](const std::atomic<bool>* c) {detail::export_checkpoint(c);auto file=std::make_shared<const RawPixelStreamFile>(link->path(),limits);file->verify(c);if(!link->same_content(*file->link())) throw std::invalid_argument("raw stream file does not match saved link");detail::export_checkpoint(c);return ReadOutput{std::move(file),{}};},"verifying linked prediction file")) return false;s.opening=true;s.tracing=false;return true;
}
bool RawPixelStreamFileReadJob::read(std::shared_ptr<const RawPixelStreamFile> file,std::uint64_t index) {
    auto& s=*impl_;s.state.on_owner();if(s.state.future.valid() || s.state.pending) return false;if(!file || index>=file->declared_summary().completed_windows) throw std::invalid_argument("raw historical block request requires an existing prefix block");
    if(!s.state.start([file=std::move(file),index](const std::atomic<bool>* c) {auto block=file->read_block(index,c);detail::export_checkpoint(c);return ReadOutput{file,std::move(block)};},"reading prediction block",1)) return false;s.opening=false;s.tracing=false;return true;
}
bool RawPixelStreamFileReadJob::trace(std::shared_ptr<const RawPixelStreamFile> file,RawPixelStreamContourOptions options) {
    auto& s=*impl_;s.state.on_owner();if(s.state.future.valid() || s.state.pending) return false;
    if(!file) throw std::invalid_argument("historical contours require a file");options.validate();if(!has_geos()) throw std::runtime_error("raw historical contours require native geometry");
    const auto windows=file->declared_summary().total_windows;
    if(!s.state.start([file=std::move(file),options](const std::atomic<bool>* c) {ReadOutput out;out.file=file;out.contours=trace_raw_pixel_stream_file(file,options,c);return out;},"tracing verified historical contours",windows)) return false;
    s.opening=false;s.tracing=true;return true;
}
bool RawPixelStreamFileReadJob::trace_path(const std::string& input,RawPixelStreamFileLimits limits,RawPixelStreamContourOptions options,std::shared_ptr<const RawPixelStreamLink> expected) {
    auto& s=*impl_;s.state.on_owner();if(s.state.future.valid() || s.state.pending) return false;limits.validate();options.validate();if(!has_geos()) throw std::runtime_error("raw historical contours require native geometry");const auto path=detail::export_path(input);
    if(!s.state.start([path,limits,options,expected=std::move(expected)](const std::atomic<bool>* c) {ReadOutput out;out.file=std::make_shared<const RawPixelStreamFile>(path,limits);out.contours=trace_raw_pixel_stream_file(out.file,options,c);if(expected && !expected->same_content(*out.contours->link())) throw std::invalid_argument("raw history differs from retained link");return out;},"tracing verified historical contours")) return false;
    s.opening=false;s.tracing=true;return true;
}
RawPixelFileJobStats RawPixelStreamFileReadJob::poll() {
    auto& s=*impl_;s.state.poll([] {return true;},[](const ReadOutput& out) {out.file->check_unchanged();return true;});
    if(s.state.pending) {s.state.stats.total_windows=(s.opening || s.tracing)?s.state.pending->file->declared_summary().total_windows:1;s.state.stats.completed_windows=(s.opening || s.tracing)?s.state.pending->file->declared_summary().completed_windows:1;}return s.state.stats;
}
RawPixelFileJobStats RawPixelStreamFileReadJob::wait() {poll();await_completion();return poll();}
std::shared_ptr<const RawPixelStreamFile> RawPixelStreamFileReadJob::take_file() {
    if(poll().state!="ready" || !impl_->opening) return {};auto out=impl_->state.take();return out?std::move(out->file):nullptr;
}
std::shared_ptr<const RawPixelStreamLink> RawPixelStreamFileReadJob::take_link() {
    if(poll().state!="ready" || !impl_->opening) return {};auto& s=impl_->state;auto link=s.pending->file->link();auto out=s.take();return out?std::move(link):nullptr;
}
std::shared_ptr<const RawPixelFileContours> RawPixelStreamFileReadJob::take_contours() {
    if(poll().state!="ready" || !impl_->tracing) return {};auto out=impl_->state.take();return out?std::move(out->contours):nullptr;
}
std::shared_ptr<const RawPixelWindowClassification> RawPixelStreamFileReadJob::take_block() {
    if(poll().state!="ready" || impl_->opening || impl_->tracing) return {};auto out=impl_->state.take();return out?std::move(out->block):nullptr;
}
}
