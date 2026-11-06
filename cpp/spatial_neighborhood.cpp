#include "neighborhood_snapshot.hpp"
#include "sha256.hpp"
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <stdexcept>

namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("spatial neighborhood analysis cancelled");}
void on_thread(std::thread::id owner) {if(owner!=std::this_thread::get_id()) throw std::runtime_error("spatial neighborhood controller must run on its owner thread");}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
void real(Sha256& hash,double value) {std::uint64_t bits;static_assert(sizeof(value)==sizeof(bits));std::memcpy(&bits,&value,8);hash.integer(bits);}
double elapsed(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
SpatialDistanceRequest capture(const Document& doc,const std::vector<std::uint64_t>& sources,
    const std::vector<std::uint64_t>& targets,const SpatialNeighborhoodOptions& options) {
    options.validate();return SpatialDistanceRequest(doc,sources,targets,options);
}
}
void SpatialNeighborhoodOptions::validate() const {
    SpatialDistanceOptions::validate();
    if(!std::isfinite(radius) || radius<=0) throw std::invalid_argument("spatial neighborhood radius must be positive and finite");
}
SpatialNeighborhoodRequest::SpatialNeighborhoodRequest(const Document& doc,const std::vector<std::uint64_t>& sources,
    const std::vector<std::uint64_t>& targets,SpatialNeighborhoodOptions options) {
    auto d=std::make_shared<Data>(capture(doc,sources,targets,options),options);
    // Keep the existing capture's conservative geometry/index/row allowance,
    // then charge this wrapper and its smaller count rows additionally. This
    // intentionally overestimates work instead of undercharging shared owners.
    const auto extra=sizeof(Data)+sizeof(*this)+sizeof(SpatialNeighborhoodResult)+512;
    d->bytes=d->capture.snapshot_bytes()+extra;
    d->work=d->capture.required_work_bytes()+extra+d->capture.source_count()*sizeof(SpatialNeighborhoodRow);
    if(d->bytes>options.snapshot_bytes) throw std::length_error("spatial neighborhood capture exceeds snapshot budget");
    if(d->work>options.work_bytes) throw std::length_error("spatial neighborhood result/index exceeds work budget");
    Sha256 hash;text(hash,"qpx.centroid-radius-count-input/1");text(hash,d->capture.input_sha256());real(hash,options.radius);
    d->digest=hash.finish();data_=std::move(d);
}
void SpatialNeighborhoodRequest::on_owner() const {on_thread(owner_);}
bool SpatialNeighborhoodRequest::matches(const Document& doc) const {on_owner();return data_->capture.matches(doc);}
std::size_t SpatialNeighborhoodRequest::source_count() const {return data_->capture.source_count();}
std::size_t SpatialNeighborhoodRequest::target_count() const {return data_->capture.target_count();}
const SpatialObjectSnapshot& SpatialNeighborhoodRequest::source_object(std::size_t i) const {return data_->capture.source_object(i);}
const SpatialObjectSnapshot& SpatialNeighborhoodRequest::target_object(std::size_t i) const {return data_->capture.target_object(i);}
std::size_t SpatialNeighborhoodRequest::snapshot_bytes() const {return data_->bytes;}
std::size_t SpatialNeighborhoodRequest::required_work_bytes() const {return data_->work;}
std::uint64_t SpatialNeighborhoodRequest::document_revision() const {return data_->capture.document_revision();}
double SpatialNeighborhoodRequest::capture_ms() const {return data_->capture.capture_ms();}
double SpatialNeighborhoodRequest::x_scale() const {return data_->capture.x_scale();}
double SpatialNeighborhoodRequest::y_scale() const {return data_->capture.y_scale();}
SpatialNeighborhoodOptions SpatialNeighborhoodRequest::options() const {return data_->options;}
const std::string& SpatialNeighborhoodRequest::input_sha256() const {return data_->digest;}
const std::shared_ptr<const AnalysisProvenance>& SpatialNeighborhoodRequest::provenance() const {return data_->capture.provenance();}
const std::string& SpatialNeighborhoodResult::input_sha256() const {return snapshot_->digest;}
SpatialNeighborhoodOptions SpatialNeighborhoodResult::options() const {return snapshot_->options;}
double SpatialNeighborhoodResult::x_scale() const {return snapshot_->capture.x_scale();}
double SpatialNeighborhoodResult::y_scale() const {return snapshot_->capture.y_scale();}
std::uint64_t SpatialNeighborhoodResult::document_revision() const {return snapshot_->capture.document_revision();}
const std::shared_ptr<const AnalysisProvenance>& SpatialNeighborhoodResult::provenance() const {return snapshot_->capture.provenance();}
std::size_t SpatialNeighborhoodResult::payload_bytes() const {return sizeof(*this)+snapshot_->bytes+rows_.capacity()*sizeof(SpatialNeighborhoodRow)+sha256_.capacity();}
bool SpatialNeighborhoodResult::belongs_to(const SpatialNeighborhoodRequest& request) const {return snapshot_==request.data_;}
std::size_t SpatialNeighborhoodResult::source_count() const {return snapshot_->capture.source_count();}
std::size_t SpatialNeighborhoodResult::target_count() const {return snapshot_->capture.target_count();}
const SpatialObjectSnapshot& SpatialNeighborhoodResult::source_object(std::size_t i) const {return snapshot_->capture.source_object(i);}
const SpatialObjectSnapshot& SpatialNeighborhoodResult::target_object(std::size_t i) const {return snapshot_->capture.target_object(i);}
std::size_t SpatialNeighborhoodRecord::payload_bytes() const {
    if(!result) throw std::invalid_argument("spatial neighborhood record has no result");
    return sizeof(*this)+256+name.capacity()+producer.capacity()+result->payload_bytes();
}
struct SpatialNeighborhoodKernel {
    static std::shared_ptr<const SpatialNeighborhoodResult> run(const SpatialNeighborhoodRequest& request,const std::atomic<bool>* cancelled,
        std::atomic<std::uint64_t>* progress=nullptr,std::atomic<std::uint64_t>* tests=nullptr) {
        checkpoint(cancelled);const auto started=Clock::now();const auto& d=*request.data_;
        std::vector<SpatialEntry> entries;entries.reserve(request.target_count());
        for(std::size_t i=0;i<request.target_count();++i) {
            checkpoint(cancelled);const auto& a=request.target_object(i);entries.push_back({a.id,{a.centroid.x,a.centroid.y,0,0}});
        }
        SpatialIndex index(std::move(entries));checkpoint(cancelled);
        if(index.payload_bytes()>SpatialIndex::estimated_bytes(request.target_count())) throw std::length_error("spatial neighborhood index exceeds admitted bound");
        auto out=std::shared_ptr<SpatialNeighborhoodResult>(new SpatialNeighborhoodResult());out->snapshot_=request.data_;out->rows_.reserve(request.source_count());
        if(out->payload_bytes()+index.payload_bytes()+8192>d.options.work_bytes) throw std::length_error("spatial neighborhood allocation exceeds work budget");
        auto remaining=d.options.max_distance_tests;Sha256 hash;text(hash,"qpx.centroid-radius-count-result/1");text(hash,d.digest);hash.integer(request.source_count());
        try {
            for(std::size_t i=0;i<request.source_count();++i) {
                checkpoint(cancelled);const auto& a=request.source_object(i);SpatialNeighborhoodRow row;
                row.source_id=a.id;row.source_revision=a.revision;row.source_centroid=a.centroid;row.source_kind=a.kind;row.source_nucleus=a.nucleus;
                row.count=index.count_points_within(a.centroid,d.options.radius,request.x_scale(),request.y_scale(),
                    d.options.exclude_self?std::optional<std::uint64_t>(a.id):std::nullopt,cancelled,&remaining);
                hash.integer(a.id);hash.integer(row.count);out->rows_.push_back(row);
                if(tests) tests->store(d.options.max_distance_tests-remaining,std::memory_order_relaxed);
                if(progress) progress->store(out->rows_.size(),std::memory_order_relaxed);
            }
        }catch(...) {if(tests) tests->store(d.options.max_distance_tests-remaining,std::memory_order_relaxed);throw;}
        checkpoint(cancelled);out->sha256_=hash.finish();out->distance_tests_=d.options.max_distance_tests-remaining;out->elapsed_ms_=elapsed(started);return out;
    }
};
std::shared_ptr<const SpatialNeighborhoodResult> measure_spatial_neighborhoods(const SpatialNeighborhoodRequest& request,const std::atomic<bool>* cancelled) {
    return SpatialNeighborhoodKernel::run(request,cancelled);
}
struct SpatialNeighborhoodJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::optional<SpatialNeighborhoodRequest> request;
    std::shared_ptr<const SpatialNeighborhoodResult> result;
    std::future<std::shared_ptr<const SpatialNeighborhoodResult>> future;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> progress{0},tests{0};
    bool stale=false;SpatialNeighborhoodStats stats;Clock::time_point started;
};
SpatialNeighborhoodJob::SpatialNeighborhoodJob():impl_(std::make_unique<Impl>()) {}
SpatialNeighborhoodJob::~SpatialNeighborhoodJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool SpatialNeighborhoodJob::busy() const {on_thread(impl_->owner);return impl_->future.valid();}
void SpatialNeighborhoodJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
bool SpatialNeighborhoodJob::request(const SpatialNeighborhoodRequest& request) {
    auto& s=*impl_;on_thread(s.owner);request.on_owner();if(s.future.valid()) return false;
    if(s.stats.generation==UINT64_MAX) throw std::overflow_error("spatial job generation exhausted");
    const auto old_cancel=s.cancelled.exchange(false);const auto old_progress=s.progress.exchange(0),old_tests=s.tests.exchange(0);
    std::future<std::shared_ptr<const SpatialNeighborhoodResult>> next;
    try {next=std::async(std::launch::async,[request,&s] {return SpatialNeighborhoodKernel::run(request,&s.cancelled,&s.progress,&s.tests);});}
    catch(...) {if(old_cancel) s.cancelled.store(true);s.progress.store(old_progress);s.tests.store(old_tests);throw;}
    s.future=std::move(next);s.request=request;s.result.reset();s.stale=false;const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.total_rows=request.source_count();s.stats.snapshot_bytes=request.snapshot_bytes();s.stats.work_bytes=request.required_work_bytes();s.stats.capture_ms=request.capture_ms();s.started=Clock::now();return true;
}
SpatialNeighborhoodStats SpatialNeighborhoodJob::poll(const Document& doc) {
    auto& s=*impl_;on_thread(s.owner);const bool was_busy=s.future.valid();
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=s.future.valid()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=s.future.get();if(s.stale) s.stats.state="stale";else if(s.cancelled.load()) s.stats.state="cancelled";else {s.result=std::move(result);s.stats.state="ready";}}
        catch(const std::exception& e) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";s.stats.error=e.what();s.result.reset();}
    }
    if(s.cancelled.load() && !s.stale && s.request) {s.result.reset();s.stats.state=s.future.valid()?"cancelling":"cancelled";}
    s.stats.completed_rows=s.progress.load();s.stats.distance_tests=s.tests.load();if(was_busy) s.stats.elapsed_ms=elapsed(s.started);
    if(!s.future.valid() && !s.result) s.request.reset();return s.stats;
}
SpatialNeighborhoodStats SpatialNeighborhoodJob::wait(const Document& doc) {poll(doc);if(impl_->future.valid()) impl_->future.wait();return poll(doc);}
void SpatialNeighborhoodJob::wait_work() {on_thread(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
std::shared_ptr<const SpatialNeighborhoodResult> SpatialNeighborhoodJob::result() const {on_thread(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->result;}
}
