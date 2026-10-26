#include "qupathxx/raw_pixel_stream_contours.hpp"
#include "raw_stream_boundaries.hpp"
#include "raw_pixel_stream_internal.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
namespace qpx {
namespace {
void stopped(const std::atomic<bool>* flag) {if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("raw streamed contours cancelled");}
void owner(std::thread::id id) {if(std::this_thread::get_id()!=id) throw std::runtime_error("raw streamed contour controller must run on its owner thread");}
}
void RawPixelStreamContourOptions::validate() const {
    contours.validate();
    if(!max_boundary_runs || max_boundary_runs>1000000 || !boundary_work_bytes || boundary_work_bytes>512u*1024*1024 || !work_bytes || work_bytes>1024u*1024*1024)
        throw std::invalid_argument("invalid raw streamed contour limits");
}
const std::string& RawPixelStreamContours::request_sha256() const {return snapshot_->sha;}
const ScientificRawLattice& RawPixelStreamContours::lattice() const {return snapshot_->lattice;}
const RawPixelStreamRegion& RawPixelStreamContours::region() const {return snapshot_->region;}
std::size_t RawPixelStreamContours::payload_bytes() const {
    const auto& p=*snapshot_->prototype;
    return sizeof(*this)+p.fixed+4ull*p.model.bytes().size()+64ull*p.model.forest().node_count()+
        contours_->retained_bytes()+(replay_file_?replay_file_->payload_bytes():0)+input_.capacity()+prediction_.capacity()+counts_.capacity()*sizeof(std::uint64_t)+65536;
}
struct RawPixelStreamContourKernel {
    static std::size_t admit(const RawPixelTiledPredictionRequest& request,RawPixelStreamContourOptions options) {
        options.validate();if(!has_geos()) throw std::runtime_error("raw streamed contours require QUPATHXX_BUILD_GEOS=ON");
        const auto work=131072ull+request.estimated_work_bytes()+std::max(options.boundary_work_bytes,options.contours.work_bytes);
        if(work>options.work_bytes) throw std::length_error("raw streamed contour producer/boundary/geometry work budget exceeded");
        if(options.boundary_work_bytes<65536) throw std::length_error("raw streamed boundary bookkeeping exceeds work budget");
        return std::size_t(work);
    }
    static void replay_admit(const RawPixelTiledPredictionRequest& request,const RawPixelStreamFile& file,RawPixelStreamContourOptions options) {
        const auto work=admit(request,options)+static_cast<std::uint64_t>(file.limits().work_bytes);
        if(work>options.work_bytes) throw std::length_error("replay file/producer/boundary/geometry work budget exceeded");
        if(!file.verified() || !file.declared_summary().complete) throw std::invalid_argument("replay requires a complete verified QWS1 file");
        const auto link=file.link();if(link->request_sha256()!=request.sha256() || link->model_sha256()!=request.model().sha256()) throw std::invalid_argument("raw replay request differs from verified history");
    }
    static bool current(const Document& doc,const RawPixelTiledPredictionRequest& request,const RawPixelStreamContours& result) {
        if(request.data_!=result.snapshot_) throw std::invalid_argument("raw streamed contours require the matching live stream capture");
        if(result.replay_file_) {if(!result.replay_file_->verified()) throw std::runtime_error("raw replay history is no longer verified");result.replay_file_->check_unchanged();}
        return request.publication_matches(doc);
    }
    static std::shared_ptr<const RawPixelStreamContours> run(const RawPixelTiledPredictionRequest& request,
        RawPixelStreamContourOptions options,const std::atomic<bool>* cancelled,
        std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr,std::shared_ptr<const RawPixelStreamFile> history={}) {
        stopped(cancelled);const auto work=admit(request,options);if(history) replay_admit(request,*history,options);const auto& data=*request.data_;
        detail::RawStreamBoundaryBuilder builder(data,data.prototype->model.forest().classes().size(),
            {options.max_boundary_runs,options.boundary_work_bytes});
        RawPixelPredictionStream stream(request);
        for(;;) {
            stopped(cancelled);const auto block=stream.next_impl(cancelled);if(!block) break;
            builder.append(block->window(),block->labels(),cancelled);
            if(completed) completed->store(stream.completed_windows(),std::memory_order_relaxed);
            // block is released here before the next read; no complete raster is retained.
        }
        stopped(cancelled);auto network=builder.finish(cancelled);
        if(network.class_counts!=stream.class_counts() || network.completed_samples!=stream.completed_samples())
            throw std::runtime_error("raw streamed contour coverage differs from completed prediction");
        const auto input=stream.input_sha256(),prediction=stream.sha256();
        if(phase) phase->store(1,std::memory_order_relaxed);
        auto geometry=detail::trace_raw_boundary_geometry(network,options.contours,
            data.prototype->mask?&*data.prototype->mask:nullptr,cancelled);
        stopped(cancelled);
        if(!raw_pixel_publication_window_source_matches(*data.source,data.prototype->source,data.prototype->window))
            throw std::runtime_error("raw streamed contour source changed before completion");
        auto result=std::shared_ptr<RawPixelStreamContours>(new RawPixelStreamContours());
        result->snapshot_=request.data_;result->contours_=std::move(geometry);result->input_=input;result->prediction_=prediction;
        result->counts_=std::move(network.class_counts);result->windows_=network.completed_windows;
        result->samples_=network.completed_samples;result->valid_=stream.valid_samples();result->work_=work+(history?history->limits().work_bytes:0);
        if(history) {
            const auto& saved=history->declared_summary();
            if(result->input_!=saved.input_sha256 || result->prediction_!=saved.sha256 || result->counts_!=saved.class_counts || result->windows_!=saved.completed_windows || result->valid_!=saved.valid_samples)
                throw std::runtime_error("fresh raw replay differs from saved input/prediction commitments");
            stopped(cancelled);history->check_unchanged();if(!history->verified()) throw std::runtime_error("raw replay history verification was retired");result->replay_file_=std::move(history);
        }
        if(result->payload_bytes()>options.work_bytes) throw std::length_error("raw streamed contour retained result exceeds work budget");
        stopped(cancelled);return result;
    }
    static std::vector<std::uint64_t> publish(Document& doc,const RawPixelTiledPredictionRequest& request,
        const RawPixelStreamContours& result,std::string prefix,std::size_t work) {
        detail::bounded_utf8(prefix,256);if(prefix.empty()) throw std::invalid_argument("raw streamed contour name prefix is empty");
        if(!work || work>512u*1024*1024) throw std::invalid_argument("invalid raw streamed contour publication budget");
        if(!current(doc,request,result)) throw std::invalid_argument("raw streamed contours are stale");
        const auto& components=result.contours_->components();const auto& classes=result.snapshot_->prototype->model.forest().classes();
        // Admit the entire retained result, edit/object copies and anticipated IDs
        // before building the one atomic document command. execute() retains its
        // existing transactional allocation, ID-exhaustion and undo/redo contract.
        std::size_t bytes=result.payload_bytes();
        for(const auto& c:components) {
            const auto& name=classes.at(c.label-1);
            const auto charge=1024ull+2*sizeof(std::uint64_t)+sizeof(ObjectEdit)+2*c.geometry.payload_bytes()+4*(prefix.size()+name.size()+1);
            if(bytes>work || charge>work-bytes) throw std::length_error("raw streamed contour result/edit/ID publication budget exceeded");
            bytes+=std::size_t(charge);
        }
        if(bytes>work) throw std::length_error("raw streamed contour retained publication budget exceeded");
        std::vector<ObjectEdit> edits;edits.reserve(components.size());
        const auto parent=result.contours_->options().clip_to_roi?result.snapshot_->prototype->source.provenance->object_id:0;
        for(const auto& c:components) edits.push_back(ObjectEdit::create_geometry(c.geometry,prefix+" "+classes.at(c.label-1),parent,classes.at(c.label-1)));
        return doc.execute(edits,"Add raw streamed classifier annotations");
    }
};
std::shared_ptr<const RawPixelStreamContours> trace_raw_pixel_stream(const RawPixelTiledPredictionRequest& request,
    RawPixelStreamContourOptions options,const std::atomic<bool>* cancelled) {return RawPixelStreamContourKernel::run(request,options,cancelled);}
std::shared_ptr<const RawPixelStreamContours> trace_raw_pixel_stream_replay(const RawPixelTiledPredictionRequest& request,
    std::shared_ptr<const RawPixelStreamFile> file,RawPixelStreamContourOptions options,const std::atomic<bool>* cancelled) {
    if(!file) throw std::invalid_argument("raw replay requires a verified file");
    return RawPixelStreamContourKernel::run(request,options,cancelled,nullptr,nullptr,std::move(file));
}
std::vector<std::uint64_t> publish_raw_pixel_stream_contours(Document& doc,const RawPixelTiledPredictionRequest& request,
    const RawPixelStreamContours& result,std::string prefix,std::size_t work) {return RawPixelStreamContourKernel::publish(doc,request,result,std::move(prefix),work);}
struct RawPixelStreamContourJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::optional<RawPixelTiledPredictionRequest> request;
    std::future<std::shared_ptr<const RawPixelStreamContours>> future;
    std::shared_ptr<const RawPixelStreamContours> result;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    bool stale=false;RawPixelStreamJobStats stats;std::chrono::steady_clock::time_point started;
};
RawPixelStreamContourJob::RawPixelStreamContourJob():impl_(std::make_unique<Impl>()) {}
RawPixelStreamContourJob::~RawPixelStreamContourJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelStreamContourJob::busy() const {owner(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawPixelStreamContourJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawPixelStreamContourJob::await_completion() {owner(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelStreamContourJob::request(const RawPixelTiledPredictionRequest& request,RawPixelStreamContourOptions options) {return start(request,options,{});}
bool RawPixelStreamContourJob::request_replay(const RawPixelTiledPredictionRequest& request,std::shared_ptr<const RawPixelStreamFile> file,RawPixelStreamContourOptions options) {
    if(!file) throw std::invalid_argument("raw replay requires a verified file");return start(request,options,std::move(file));
}
bool RawPixelStreamContourJob::start(const RawPixelTiledPredictionRequest& request,RawPixelStreamContourOptions options,std::shared_ptr<const RawPixelStreamFile> file) {
    owner(impl_->owner);auto& s=*impl_;if(busy()) return false;
    RawPixelStreamContourKernel::admit(request,options);if(file) RawPixelStreamContourKernel::replay_admit(request,*file,options);
    if(s.stats.generation==UINT64_MAX) throw std::overflow_error("raw streamed contour generation exhausted");
    RawPixelStreamJobStats stats;stats.generation=s.stats.generation+1;stats.state="running";stats.phase="predicting";
    stats.total_windows=request.total_windows();stats.total_samples=request.total_samples();stats.class_counts.resize(request.model().forest().classes().size());
    const auto old_cancel=s.cancelled.load();const auto old_completed=s.completed.load();const auto old_phase=s.phase.load();s.cancelled.store(false);s.completed.store(0);s.phase.store(0);
    std::future<std::shared_ptr<const RawPixelStreamContours>> future;
    try {future=std::async(std::launch::async,[state=&s,request,options,file=std::move(file)] {
        return RawPixelStreamContourKernel::run(request,options,&state->cancelled,&state->completed,&state->phase,file);
    });}catch(...) {s.cancelled.store(old_cancel);s.completed.store(old_completed);s.phase.store(old_phase);throw;}
    s.future=std::move(future);s.request=request;s.result.reset();s.stale=false;s.stats=std::move(stats);s.started=std::chrono::steady_clock::now();return true;
}
RawPixelStreamJobStats RawPixelStreamContourJob::poll(const Document& doc) {
    owner(impl_->owner);auto& s=*impl_;
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto result=s.future.get();
            if(s.stale || (!s.cancelled.load() && !RawPixelStreamContourKernel::current(doc,*s.request,*result))) {s.stale=true;s.stats.state="stale";}
            else if(s.cancelled.load()) s.stats.state="cancelled";
            else {s.result=std::move(result);s.stats.state="ready";s.stats.delivered_windows=s.result->completed_windows();s.stats.delivered_samples=s.result->completed_samples();s.stats.valid_samples=s.result->valid_samples();s.stats.class_counts=s.result->class_counts();for(auto n:s.stats.class_counts) s.stats.classified_samples+=n;}
        }catch(const std::exception& error) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";s.stats.error=error.what();s.result.reset();}
    }
    if(s.result && s.stats.state=="ready") {try {if(!RawPixelStreamContourKernel::current(doc,*s.request,*s.result)) {s.result.reset();s.stale=true;s.stats.state="stale";}}catch(const std::exception& e) {s.result.reset();s.stats.state="failed";s.stats.error=e.what();}}
    s.stats.produced_windows=s.completed.load();
    if(s.cancelled.load() && !s.stale) {s.result.reset();s.stats.state=busy()?"cancelling":"cancelled";}
    if(!s.future.valid() && s.stats.state!="ready") {s.result.reset();s.request.reset();}
    if(s.stats.generation) s.stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s.started).count();
    s.stats.phase=s.future.valid()?(s.phase.load()?"tracing":"predicting"):"finished";return s.stats;
}
RawPixelStreamJobStats RawPixelStreamContourJob::wait(const Document& doc) {owner(impl_->owner);poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const RawPixelStreamContours> RawPixelStreamContourJob::result() const {owner(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->result;}
}
