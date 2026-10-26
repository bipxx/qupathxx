#include "raw_pixel_stream_internal.hpp"
#include "raw_pixel_features.hpp"
#include "raw_analysis_internal.hpp"
#include "scientific_raw_sampling_internal.hpp"
#include "raw_pixel_window_prediction_internal.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <mutex>
#include <thread>

namespace qpx {
namespace {
void stopped(const std::atomic<bool>& flag) {if(flag.load(std::memory_order_relaxed)) throw std::runtime_error("raw prediction stream cancelled");}
void owner(std::thread::id id) {if(std::this_thread::get_id()!=id) throw std::runtime_error("raw stream controller must run on its owner thread");}
void text(Sha256& h,const std::string& value) {h.integer(value.size());h.update(value.data(),value.size());}
void number(Sha256& h,double value) {std::uint64_t bits;std::memcpy(&bits,&value,8);h.integer(bits);}
void calibration(Sha256& h,const std::optional<double>& value) {h.integer(bool(value));if(value) number(h,*value);}
std::string chain(const char* domain,const std::string& previous,std::uint64_t index,const std::string& block) {
    Sha256 h;text(h,domain);text(h,previous);h.integer(index);text(h,block);return h.finish();
}
std::string failure(const std::exception& e) {try {std::string message=e.what();detail::bounded_utf8(message,8192);return message;}catch(const std::invalid_argument&) {return "Raw prediction stream failed";}}
enum class State {Ready,Producing,Complete,Failed};
}
void RawPixelStreamOptions::validate() const {
    if(tile_width<1 || tile_width>4096 || tile_height<1 || tile_height>4096 || !max_samples || max_samples>1000000000000ULL || !max_windows || max_windows>1000000000000ULL || work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024 || !std::isfinite(min_vote_fraction) || min_vote_fraction<0 || min_vote_fraction>1) throw std::invalid_argument("invalid raw prediction stream options");
}
std::string raw_pixel_stream_chain(const char* domain,const std::string& previous,std::uint64_t index,const std::string& block) {return chain(domain,previous,index,block);}
RawPixelStreamGeometry::RawPixelStreamGeometry(ScientificRawLattice l,RawPixelStreamRegion r,RawPixelStreamOptions o):lattice(std::move(l)),region(r),options(o) {
    options.validate();if(options.min_vote_fraction==0) options.min_vote_fraction=0;
    if(region.first_x<0 || region.first_y<0 || region.width<1 || region.height<1 || region.first_x>lattice.x().samples() || region.width>lattice.x().samples()-region.first_x || region.first_y>lattice.y().samples() || region.height>lattice.y().samples()-region.first_y) throw std::invalid_argument("raw prediction region lies outside global core lattice");
    samples=std::uint64_t(region.width)*std::uint64_t(region.height);if(samples>options.max_samples) throw std::length_error("raw prediction stream exceeds sample limit");
    columns=(std::uint64_t(region.width)+options.tile_width-1)/options.tile_width;const auto rows=(std::uint64_t(region.height)+options.tile_height-1)/options.tile_height;
    if(columns>options.max_windows/rows) throw std::length_error("raw prediction stream exceeds window limit");windows=columns*rows;
}
ScientificRawWindow RawPixelStreamGeometry::window(std::uint64_t index) const {
    if(index>=windows) throw std::out_of_range("raw prediction stream window index");
    const auto x=region.first_x+std::int64_t(index%columns)*options.tile_width,y=region.first_y+std::int64_t(index/columns)*options.tile_height;
    return lattice.window(x,y,std::min(options.tile_width,region.first_x+region.width-x),std::min(options.tile_height,region.first_y+region.height-y));
}
std::uint64_t RawPixelStreamGeometry::samples_before(std::uint64_t index) const {
    if(index>windows) throw std::out_of_range("raw stream prefix window count");if(index==windows) return samples;
    const auto row=index/columns,col=index%columns;
    return row*std::uint64_t(options.tile_height)*std::uint64_t(region.width)+col*std::uint64_t(options.tile_width)*std::min<std::uint64_t>(options.tile_height,std::uint64_t(region.height)-row*options.tile_height);
}
std::string raw_pixel_stream_request_sha256(const RawPixelStreamGeometry& g,const RawPixelClassifier& model,const std::string& reader,const std::string& epoch,RawClassifierCalibration c,const std::optional<Roi>& mask) {
    Sha256 h;text(h,"qpx.raw-pixel-stream-request/1");text(h,model.sha256());text(h,reader);text(h,epoch);const auto& bounds=g.region;const auto& options=g.options;
    for(auto v:{g.lattice.image_width(),g.lattice.image_height(),g.lattice.x().origin(),g.lattice.x().extent(),g.lattice.x().samples(),g.lattice.x().context(),g.lattice.y().origin(),g.lattice.y().extent(),g.lattice.y().samples(),g.lattice.y().context(),bounds.first_x,bounds.first_y,bounds.width,bounds.height,options.tile_width,options.tile_height}) h.integer(v);
    h.integer(options.max_samples);h.integer(options.max_windows);h.integer(options.work_bytes);number(h,options.min_vote_fraction);calibration(h,c.x);calibration(h,c.y);
    h.integer(bool(mask));if(mask) {const auto bytes=GeometryArchiveCodec::encode(*mask);h.integer(bytes.size());h.update(bytes.data(),bytes.size());}return h.finish();
}
RawPixelTiledPredictionRequest::RawPixelTiledPredictionRequest(const Document& doc,const RawPixelClassifier& model,ScientificRawLattice lattice,RawPixelStreamOptions options,std::optional<RawPixelStreamRegion> region,std::uint64_t object) {
    options.validate();if(options.min_vote_fraction==0) options.min_vote_fraction=0;
    const auto bounds=region.value_or(RawPixelStreamRegion{0,0,lattice.x().samples(),lattice.y().samples()});
    auto data=std::make_shared<Data>(std::move(lattice),bounds,options);
    // Covers producer/controller bookkeeping, progress arrays, digests and one
    // future slot. The window authority accounts for source/model/output copies.
    constexpr std::size_t overhead=131072;
    if(options.work_bytes<=overhead+1024*1024) throw std::length_error("raw prediction stream bookkeeping exceeds budget");
    auto authority=std::make_shared<RawPixelWindowPredictionRequest>(doc,model,data->window(0),object,options.min_vote_fraction,options.work_bytes-overhead);
    data->prototype=authority->data_;data->source=authority->source_.get();
    data->sha=raw_pixel_stream_request_sha256(*data,model,data->prototype->source.reader,data->prototype->source.epoch,data->prototype->source.calibration,data->prototype->mask);authority_=std::move(authority);data_=std::move(data);if(!matches(doc)) throw std::runtime_error("document changed during raw stream capture");
}
bool RawPixelTiledPredictionRequest::matches(const Document& doc) const {return authority_->matches(doc);}
bool RawPixelTiledPredictionRequest::publication_matches(const Document& doc) const {return authority_->publication_matches(doc);}
const ScientificRawLattice& RawPixelTiledPredictionRequest::lattice() const {return data_->lattice;}
const RawPixelStreamRegion& RawPixelTiledPredictionRequest::region() const {return data_->region;}
const RawPixelStreamOptions& RawPixelTiledPredictionRequest::options() const {return data_->options;}
const RawPixelClassifier& RawPixelTiledPredictionRequest::model() const {return data_->prototype->model;}
ScientificRawWindow RawPixelTiledPredictionRequest::window(std::uint64_t index) const {return data_->window(index);}
std::uint64_t RawPixelTiledPredictionRequest::total_windows() const {return data_->windows;}
std::uint64_t RawPixelTiledPredictionRequest::total_samples() const {return data_->samples;}
std::size_t RawPixelTiledPredictionRequest::estimated_work_bytes() const {return data_->options.work_bytes;}
const std::string& RawPixelTiledPredictionRequest::sha256() const {return data_->sha;}

std::string RawPixelTiledPredictionRequest::input_sha256(std::uint64_t index,const std::atomic<bool>* cancelled) const {
    detail::export_checkpoint(cancelled);const auto& d=*data_;auto selection=d.prototype->model.recipe().selection();selection.work_bytes=std::min(selection.work_bytes,d.prototype->budget-d.prototype->fixed);
    detail::ScientificRawReserve reserve;reserve.expected_series=d.prototype->model.recipe().series();
    const auto digest=detail::digest_scientific_raw_window_reserved(*d.source,d.window(index),selection,reserve,cancelled);raw_pixel_verify_window_digest(*d.source,d.prototype->source,d.prototype->window,digest);
    const auto features=detail::raw_feature_input_sha256(digest.input_sha256,d.prototype->model.forest().features(),true);
    const auto result=raw_grid_analysis_hash(features,d.prototype->source.calibration.x,d.prototype->source.calibration.y,d.prototype->mask);
    if(!raw_pixel_publication_window_source_matches(*d.source,d.prototype->source,d.prototype->window)) throw std::runtime_error("raw stream source changed during hash-only validation");detail::export_checkpoint(cancelled);return result;
}
struct RawPixelPredictionStream::Impl {
    RawPixelTiledPredictionRequest request;
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::atomic<State> state{State::Ready};
    std::atomic<std::uint64_t> completed{0},samples{0},valid{0},classified{0};
    std::array<std::atomic<std::uint64_t>,256> counts{};
    std::mutex consumer;mutable std::mutex publication;
    std::string input_chain,result_chain;
    Impl(const RawPixelTiledPredictionRequest& r,std::shared_ptr<std::atomic<bool>> flag):request(r),cancelled(std::move(flag)),input_chain(r.sha256()),result_chain(r.sha256()) {for(auto& count:counts) count.store(0);}
};
RawPixelPredictionStream::RawPixelPredictionStream(const RawPixelTiledPredictionRequest& request):RawPixelPredictionStream(request,std::make_shared<std::atomic<bool>>(false)) {}
RawPixelPredictionStream::RawPixelPredictionStream(const RawPixelTiledPredictionRequest& request,std::shared_ptr<std::atomic<bool>> flag):impl_(std::make_unique<Impl>(request,std::move(flag))) {}
RawPixelPredictionStream::~RawPixelPredictionStream()=default;
void RawPixelPredictionStream::accept_block(const RawPixelWindowClassification& result,const std::atomic<bool>* cancelled) {
    auto& s=*impl_;const auto index=s.completed.load();std::uint64_t valid=0,classified=0;
    for(std::size_t i=0;i<result.valid().size();++i) {if((i&4095)==0) {stopped(*s.cancelled);detail::export_checkpoint(cancelled);}valid+=bool(result.valid()[i]);}for(auto count:result.class_counts()) classified+=count;
    const auto next_input=chain("qpx.raw-pixel-stream-input-chain/1",s.input_chain,index,result.input_sha256()),next_result=chain("qpx.raw-pixel-stream-result-chain/1",s.result_chain,index,result.sha256());
    std::lock_guard<std::mutex> publication(s.publication);stopped(*s.cancelled);detail::export_checkpoint(cancelled);
    s.input_chain=next_input;s.result_chain=next_result;s.valid.fetch_add(valid);s.classified.fetch_add(classified);s.samples.fetch_add(result.labels().size());
    for(std::size_t i=0;i<result.class_counts().size();++i) s.counts[i].fetch_add(result.class_counts()[i]);
    s.completed.store(index+1);s.state.store(index+1==s.request.total_windows()?State::Complete:State::Ready);
}
std::shared_ptr<const RawPixelWindowClassification> RawPixelPredictionStream::next() {return next_impl(nullptr);}
std::shared_ptr<const RawPixelWindowClassification> RawPixelPredictionStream::next_impl(const std::atomic<bool>* cancelled) {
    auto& s=*impl_;std::unique_lock<std::mutex> consumer(s.consumer,std::try_to_lock);if(!consumer.owns_lock()) throw std::logic_error("raw prediction stream already has an active consumer");
    stopped(*s.cancelled);detail::export_checkpoint(cancelled);if(s.state.load()==State::Failed) throw std::logic_error("raw prediction stream has failed");
    const auto index=s.completed.load();if(index==s.request.total_windows()) return {};s.state.store(State::Producing);
    try {
        const auto& d=*s.request.data_;auto result=RawPixelWindowPredictionKernel::run_window(*d.prototype,d.window(index),*d.source,cancelled?cancelled:s.cancelled.get());
        if(!raw_pixel_publication_window_source_matches(*d.source,d.prototype->source,d.prototype->window)) throw std::runtime_error("raw prediction stream source changed before block delivery");
        accept_block(*result,cancelled);return result;
    }catch(...) {s.state.store(State::Failed);throw;}
}
void RawPixelPredictionStream::restore_block(const RawPixelWindowClassification& block,const std::atomic<bool>* cancelled) {
    auto& s=*impl_;std::unique_lock<std::mutex> consumer(s.consumer,std::try_to_lock);if(!consumer.owns_lock()) throw std::logic_error("raw prediction stream already has an active consumer");
    stopped(*s.cancelled);detail::export_checkpoint(cancelled);if(s.state.load()==State::Failed) throw std::logic_error("raw prediction stream has failed");
    const auto index=s.completed.load();const auto window=s.request.window(index);s.state.store(State::Producing);
    try {
        const auto& actual=block.window();if(block.model_sha256()!=s.request.model().sha256() || actual.x().first_core_index()!=window.x().first_core_index() || actual.y().first_core_index()!=window.y().first_core_index() || actual.core_width()!=window.core_width() || actual.core_height()!=window.core_height()) throw std::invalid_argument("raw stream resume block/model geometry mismatch");
        if(s.request.input_sha256(index,cancelled)!=block.input_sha256()) throw std::runtime_error("raw stream completed-window source content changed");accept_block(block,cancelled);
    }catch(...) {s.state.store(State::Failed);throw;}
}
void RawPixelPredictionStream::cancel() {std::lock_guard<std::mutex> lock(impl_->publication);impl_->cancelled->store(true,std::memory_order_relaxed);}
bool RawPixelPredictionStream::busy() const {return impl_->state.load()==State::Producing;}
bool RawPixelPredictionStream::matches(const Document& doc) const {return impl_->request.matches(doc);}
std::string RawPixelPredictionStream::state() const {if(impl_->cancelled->load()) return "cancelled";switch(impl_->state.load()) {case State::Ready:return "ready";case State::Producing:return "producing";case State::Complete:return "complete";default:return "failed";}}
std::uint64_t RawPixelPredictionStream::total_windows() const {return impl_->request.total_windows();}
std::uint64_t RawPixelPredictionStream::completed_windows() const {return impl_->completed.load();}
std::uint64_t RawPixelPredictionStream::completed_samples() const {return impl_->samples.load();}
std::uint64_t RawPixelPredictionStream::valid_samples() const {return impl_->valid.load();}
std::uint64_t RawPixelPredictionStream::classified_samples() const {return impl_->classified.load();}
std::vector<std::uint64_t> RawPixelPredictionStream::class_counts() const {std::lock_guard<std::mutex> lock(impl_->publication);std::vector<std::uint64_t> out;out.reserve(impl_->request.model().forest().classes().size());for(std::size_t i=0;i<impl_->request.model().forest().classes().size();++i) out.push_back(impl_->counts[i].load());return out;}
std::size_t RawPixelPredictionStream::estimated_work_bytes() const {return impl_->request.estimated_work_bytes();}
std::string RawPixelPredictionStream::input_sha256() const {std::lock_guard<std::mutex> lock(impl_->publication);if(impl_->cancelled->load() || impl_->state.load()!=State::Complete) throw std::logic_error("raw stream input commitment requires successful exhaustion");return impl_->input_chain;}
std::string RawPixelPredictionStream::sha256() const {std::lock_guard<std::mutex> lock(impl_->publication);if(impl_->cancelled->load() || impl_->state.load()!=State::Complete) throw std::logic_error("raw stream result commitment requires successful exhaustion");return impl_->result_chain;}

struct RawPixelPredictionStreamJob::Impl {
    struct Completion {std::shared_ptr<const RawPixelWindowClassification> block;std::uint64_t valid=0;};
    const std::thread::id owner=std::this_thread::get_id();
    std::shared_ptr<std::atomic<bool>> cancelled=std::make_shared<std::atomic<bool>>(false);
    std::optional<RawPixelTiledPredictionRequest> request;
    std::unique_ptr<RawPixelPredictionStream> stream;
    std::future<Completion> future;std::shared_ptr<const RawPixelWindowClassification> pending;std::uint64_t pending_valid=0;
    RawPixelStreamJobStats stats;bool stale=false;std::chrono::steady_clock::time_point started;
    static std::future<Completion> launch(RawPixelPredictionStream* stream) {return std::async(std::launch::async,[stream] {Completion out;out.block=stream->next();if(out.block) out.valid=std::uint64_t(std::count(out.block->valid().begin(),out.block->valid().end(),std::uint8_t(1)));return out;});}
};
RawPixelPredictionStreamJob::RawPixelPredictionStreamJob():impl_(std::make_unique<Impl>()) {}
RawPixelPredictionStreamJob::~RawPixelPredictionStreamJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelPredictionStreamJob::busy() const {owner(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawPixelPredictionStreamJob::cancel() {impl_->cancelled->store(true,std::memory_order_relaxed);}
void RawPixelPredictionStreamJob::await_completion() {owner(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelPredictionStreamJob::request(const RawPixelTiledPredictionRequest& request) {
    owner(impl_->owner);auto& s=*impl_;if(s.future.valid() || s.pending) return false;if(s.stats.generation==UINT64_MAX) throw std::overflow_error("raw stream generation exhausted");
    RawPixelStreamJobStats stats;stats.generation=s.stats.generation+1;stats.state="running";stats.phase="predicting";stats.total_windows=request.total_windows();stats.total_samples=request.total_samples();stats.class_counts.resize(request.model().forest().classes().size());
    auto stream=std::unique_ptr<RawPixelPredictionStream>(new RawPixelPredictionStream(request,s.cancelled));const auto old_cancel=s.cancelled->load();s.cancelled->store(false);std::future<Impl::Completion> future;
    try {future=Impl::launch(stream.get());}catch(...) {s.cancelled->store(old_cancel);throw;}
    s.future=std::move(future);s.stream=std::move(stream);s.request=request;s.pending_valid=0;s.stale=false;s.stats=std::move(stats);s.started=std::chrono::steady_clock::now();return true;
}
RawPixelStreamJobStats RawPixelPredictionStreamJob::poll(const Document& doc) {
    owner(impl_->owner);auto& s=*impl_;
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled->store(true);s.pending.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto out=s.future.get();
            if(s.stale || (!s.cancelled->load() && !s.request->publication_matches(doc))) {s.stale=true;s.stats.state="stale";s.cancelled->store(true);}
            else if(s.cancelled->load()) s.stats.state="cancelled";
            else {if(!out.block) throw std::logic_error("raw stream worker exhausted before expected block");s.pending=std::move(out.block);s.pending_valid=out.valid;s.stats.state="block_ready";}
        }catch(const std::exception& e) {s.stats.state=s.stale?"stale":s.cancelled->load()?"cancelled":"failed";s.stats.error=failure(e);s.pending.reset();}
    }
    if(s.stream) s.stats.produced_windows=s.stream->completed_windows();
    if(s.cancelled->load() && !s.stale) {s.pending.reset();s.stats.state=busy()?"cancelling":"cancelled";}
    if(!s.future.valid() && s.stats.state!="block_ready" && s.stats.state!="complete") {s.pending.reset();s.stream.reset();s.request.reset();}
    if(s.stats.generation) s.stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s.started).count();
    s.stats.phase=s.future.valid()?"predicting":s.stats.state=="block_ready"?"waiting for consumer":"finished";return s.stats;
}
RawPixelStreamJobStats RawPixelPredictionStreamJob::wait(const Document& doc) {owner(impl_->owner);poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const RawPixelWindowClassification> RawPixelPredictionStreamJob::take(const Document& doc) {
    owner(impl_->owner);auto& s=*impl_;if(poll(doc).state!="block_ready") return {};
    if(!s.request->publication_matches(doc)) {s.stale=true;s.cancelled->store(true);s.pending.reset();s.stats.state="stale";s.stream.reset();s.request.reset();return {};}
    if(s.cancelled->load()) {poll(doc);return {};}
    auto block=std::move(s.pending);++s.stats.delivered_windows;s.stats.delivered_samples+=std::uint64_t(block->window().core_width())*std::uint64_t(block->window().core_height());s.stats.valid_samples+=s.pending_valid;
    for(std::size_t i=0;i<block->class_counts().size();++i) {s.stats.class_counts[i]+=block->class_counts()[i];s.stats.classified_samples+=block->class_counts()[i];}
    if(s.stats.delivered_windows==s.stats.total_windows) {s.stats.state="complete";s.stats.phase="finished";}
    else {
        try {s.future=Impl::launch(s.stream.get());s.stats.state="running";s.stats.phase="predicting";}
        catch(const std::exception& e) {s.stats.state="failed";s.stats.error=failure(e);s.stats.phase="finished";s.stream.reset();s.request.reset();}
    }
    return block;
}
std::string RawPixelPredictionStreamJob::input_sha256() const {owner(impl_->owner);if(impl_->stats.state!="complete" || impl_->cancelled->load() || !impl_->stream) throw std::logic_error("raw stream job has not delivered every block");return impl_->stream->input_sha256();}
std::string RawPixelPredictionStreamJob::sha256() const {owner(impl_->owner);if(impl_->stats.state!="complete" || impl_->cancelled->load() || !impl_->stream) throw std::logic_error("raw stream job has not delivered every block");return impl_->stream->sha256();}
}
