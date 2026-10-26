#include "qupathxx/raw_pixel_stream.hpp"
#include "raw_pixel_window_prediction_internal.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
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
struct RawPixelTiledPredictionRequest::Data {
    ScientificRawLattice lattice;RawPixelStreamRegion region;RawPixelStreamOptions options;
    std::uint64_t columns=0,windows=0,samples=0;
    std::shared_ptr<const RawPixelWindowPredictionRequest::Data> prototype;
    ImageServer* source=nullptr; // Borrowed: authority remains on the owner.
    std::string sha;
    Data(ScientificRawLattice l,RawPixelStreamRegion r,RawPixelStreamOptions o):lattice(std::move(l)),region(r),options(o) {}
    ScientificRawWindow window(std::uint64_t index) const {
        if(index>=windows) throw std::out_of_range("raw prediction stream window index");
        const auto x=region.first_x+std::int64_t(index%columns)*options.tile_width,y=region.first_y+std::int64_t(index/columns)*options.tile_height;
        return lattice.window(x,y,std::min(options.tile_width,region.first_x+region.width-x),std::min(options.tile_height,region.first_y+region.height-y));
    }
};
RawPixelTiledPredictionRequest::RawPixelTiledPredictionRequest(const Document& doc,const RawPixelClassifier& model,ScientificRawLattice lattice,RawPixelStreamOptions options,std::optional<RawPixelStreamRegion> region,std::uint64_t object) {
    options.validate();if(options.min_vote_fraction==0) options.min_vote_fraction=0;
    const auto bounds=region.value_or(RawPixelStreamRegion{0,0,lattice.x().samples(),lattice.y().samples()});
    if(bounds.first_x<0 || bounds.first_y<0 || bounds.width<1 || bounds.height<1 || bounds.first_x>lattice.x().samples() || bounds.width>lattice.x().samples()-bounds.first_x || bounds.first_y>lattice.y().samples() || bounds.height>lattice.y().samples()-bounds.first_y) throw std::invalid_argument("raw prediction region lies outside global core lattice");
    auto data=std::make_shared<Data>(std::move(lattice),bounds,options);data->samples=std::uint64_t(bounds.width)*std::uint64_t(bounds.height);
    if(data->samples>options.max_samples) throw std::length_error("raw prediction stream exceeds sample limit");
    data->columns=(std::uint64_t(bounds.width)+options.tile_width-1)/options.tile_width;const auto rows=(std::uint64_t(bounds.height)+options.tile_height-1)/options.tile_height;
    if(data->columns>options.max_windows/rows) throw std::length_error("raw prediction stream exceeds window limit");data->windows=data->columns*rows;
    // Covers producer/controller bookkeeping, progress arrays, digests and one
    // future slot. The window authority accounts for source/model/output copies.
    constexpr std::size_t overhead=131072;
    if(options.work_bytes<=overhead+1024*1024) throw std::length_error("raw prediction stream bookkeeping exceeds budget");
    auto authority=std::make_shared<RawPixelWindowPredictionRequest>(doc,model,data->window(0),object,options.min_vote_fraction,options.work_bytes-overhead);
    data->prototype=authority->data_;data->source=authority->source_.get();
    Sha256 h;text(h,"qpx.raw-pixel-stream-request/1");text(h,model.sha256());text(h,data->prototype->source.reader);text(h,data->prototype->source.epoch);
    for(auto v:{data->lattice.image_width(),data->lattice.image_height(),data->lattice.x().origin(),data->lattice.x().extent(),data->lattice.x().samples(),data->lattice.x().context(),data->lattice.y().origin(),data->lattice.y().extent(),data->lattice.y().samples(),data->lattice.y().context(),bounds.first_x,bounds.first_y,bounds.width,bounds.height,options.tile_width,options.tile_height}) h.integer(v);
    h.integer(options.max_samples);h.integer(options.max_windows);h.integer(options.work_bytes);number(h,options.min_vote_fraction);calibration(h,data->prototype->source.calibration.x);calibration(h,data->prototype->source.calibration.y);
    h.integer(bool(data->prototype->mask));if(data->prototype->mask) {const auto bytes=GeometryArchiveCodec::encode(*data->prototype->mask);h.integer(bytes.size());h.update(bytes.data(),bytes.size());}
    data->sha=h.finish();authority_=std::move(authority);data_=std::move(data);if(!matches(doc)) throw std::runtime_error("document changed during raw stream capture");
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
std::shared_ptr<const RawPixelWindowClassification> RawPixelPredictionStream::next() {
    auto& s=*impl_;std::unique_lock<std::mutex> consumer(s.consumer,std::try_to_lock);if(!consumer.owns_lock()) throw std::logic_error("raw prediction stream already has an active consumer");
    stopped(*s.cancelled);if(s.state.load()==State::Failed) throw std::logic_error("raw prediction stream has failed");
    const auto index=s.completed.load();if(index==s.request.total_windows()) return {};
    s.state.store(State::Producing);
    try {
        const auto& d=*s.request.data_;const auto window=d.window(index);
        auto result=RawPixelWindowPredictionKernel::run_window(*d.prototype,window,*d.source,s.cancelled.get());
        if(!raw_pixel_publication_window_source_matches(*d.source,d.prototype->source,d.prototype->window)) throw std::runtime_error("raw prediction stream source changed before block delivery");
        std::uint64_t valid=0,classified=0;for(std::size_t i=0;i<result->valid().size();++i) {if((i&4095)==0) stopped(*s.cancelled);valid+=bool(result->valid()[i]);}for(auto count:result->class_counts()) classified+=count;
        const auto next_input=chain("qpx.raw-pixel-stream-input-chain/1",s.input_chain,index,result->input_sha256());const auto next_result=chain("qpx.raw-pixel-stream-result-chain/1",s.result_chain,index,result->sha256());
        std::lock_guard<std::mutex> publication(s.publication);stopped(*s.cancelled);
        s.input_chain=next_input;s.result_chain=next_result;s.valid.fetch_add(valid);s.classified.fetch_add(classified);s.samples.fetch_add(std::uint64_t(window.core_width())*std::uint64_t(window.core_height()));
        for(std::size_t i=0;i<result->class_counts().size();++i) s.counts[i].fetch_add(result->class_counts()[i]);
        s.completed.store(index+1);s.state.store(index+1==d.windows?State::Complete:State::Ready);return result;
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
