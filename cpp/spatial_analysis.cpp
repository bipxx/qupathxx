#include "spatial_snapshot.hpp"
#include "utf8_text.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("spatial distance analysis cancelled");}
void on_thread(std::thread::id owner) {if(owner!=std::this_thread::get_id()) throw std::runtime_error("spatial distance controller must run on its owner thread");}
void real(Sha256& h,double value) {std::uint64_t bits;static_assert(sizeof(value)==sizeof(bits));std::memcpy(&bits,&value,8);h.integer(bits);}
void text(Sha256& h,const std::string& value) {h.integer(value.size());h.update(value.data(),value.size());}
double elapsed(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
std::vector<std::uint64_t> selection(const std::vector<std::uint64_t>& ids,bool source) {
    if((source && ids.empty()) || ids.size()>100000) throw std::invalid_argument("spatial selection requires 1..100000 sources and 0..100000 targets");
    auto out=ids;std::sort(out.begin(),out.end());
    if(std::adjacent_find(out.begin(),out.end())!=out.end()) throw std::invalid_argument("duplicate spatial object ID");
    return out;
}
}
void SpatialDistanceOptions::validate() const {
    if(units!=SpatialDistanceUnits::Pixels && units!=SpatialDistanceUnits::Micrometres)
        throw std::invalid_argument("invalid spatial distance units");
    constexpr std::size_t mb=1024*1024;
    if(snapshot_bytes<mb || snapshot_bytes>256u*mb || work_bytes<mb || work_bytes>1024u*mb ||
       !max_distance_tests || max_distance_tests>(1ULL<<40)) throw std::invalid_argument("invalid spatial distance budgets");
}
SpatialDistanceRequest::SpatialDistanceRequest(const Document& doc,const std::vector<std::uint64_t>& source_ids,
    const std::vector<std::uint64_t>& target_ids,SpatialDistanceOptions options)
    :identity_(doc.analysis_identity_.token),owner_(std::this_thread::get_id()) {
    const auto started=Clock::now();options.validate();
    if(source_ids.size()>100000 || target_ids.size()>100000 || source_ids.empty())
        throw std::invalid_argument("spatial selection requires 1..100000 sources and 0..100000 targets");
    if((source_ids.size()+target_ids.size())*2*sizeof(std::uint64_t)+sizeof(Data)+sizeof(*this)+256>options.snapshot_bytes)
        throw std::length_error("spatial selection exceeds snapshot budget");
    auto sources=selection(source_ids,true),targets=selection(target_ids,false);
    auto d=std::make_shared<Data>();d->options=options;d->revision=doc.revision();
    if(options.units==SpatialDistanceUnits::Micrometres) {
        if(!doc.pixel_width_um() || !doc.pixel_height_um()) throw std::invalid_argument("micrometre spatial distances require complete calibration");
        d->sx=*doc.pixel_width_um();d->sy=*doc.pixel_height_um();
        if(!std::isfinite(d->sx) || !std::isfinite(d->sy) || d->sx<=0 || d->sy<=0) throw std::invalid_argument("invalid spatial calibration");
    }
    const auto selection_bytes=(sources.size()+targets.size())*2*sizeof(std::uint64_t);
    d->bytes=sizeof(Data)+sizeof(*this)+256;
    auto admit=[&](std::size_t bytes) {if(d->bytes>options.snapshot_bytes || bytes>options.snapshot_bytes-d->bytes) throw std::length_error("spatial capture exceeds snapshot budget");};
    admit(selection_bytes);
    std::vector<std::uint64_t> all;all.reserve(sources.size()+targets.size());all.insert(all.end(),sources.begin(),sources.end());all.insert(all.end(),targets.begin(),targets.end());
    std::sort(all.begin(),all.end());all.erase(std::unique(all.begin(),all.end()),all.end());
    for(auto id:all) if(!doc.objects().count(id)) throw std::out_of_range("spatial object does not exist");
    d->bytes+=all.size()*(sizeof(SpatialObjectSnapshot)+128)+(sources.size()+targets.size())*sizeof(std::size_t);admit(selection_bytes);
    d->provenance=capture_source_provenance(*doc.server(),d->revision,doc.pixel_width_um(),doc.pixel_height_um());
    d->bytes+=d->provenance->payload_bytes();admit(selection_bytes);d->objects.reserve(all.size());
    std::size_t geometry_work=0;
    for(auto id:all) {
        const auto& a=doc.objects().at(id);const bool nucleus=options.prefer_nucleus && a.kind==ObjectKind::Cell && bool(a.nucleus);
        const auto& roi=nucleus?*a.nucleus:a.geometry;
        const auto bytes=roi.payload_bytes();admit(selection_bytes+geometry_work);
        if(bytes>(options.snapshot_bytes-d->bytes-selection_bytes-geometry_work)/3) throw std::length_error("spatial geometry capture exceeds snapshot budget");
        const auto encoded=GeometryArchiveCodec::encode(roi);Sha256 hash;hash.update(encoded.data(),encoded.size());
        d->objects.push_back({id,a.revision,roi.centroid(),a.kind,nucleus,hash.finish()});geometry_work+=bytes;
    }
    // Charge the captured input geometry conservatively even though only its
    // centroid and identity are retained. This also bounds owner capture work.
    d->bytes+=geometry_work;admit(selection_bytes);
    auto indexes=[&](const auto& ids,auto& into) {into.reserve(ids.size());for(auto id:ids) into.push_back(std::lower_bound(all.begin(),all.end(),id)-all.begin());};
    indexes(sources,d->sources);indexes(targets,d->targets);
    d->work=d->bytes+sizeof(SpatialDistanceResult)+sources.size()*sizeof(SpatialDistanceRow)+SpatialIndex::estimated_bytes(targets.size())+8192;
    if(d->work>options.work_bytes) throw std::length_error("spatial distance result/index exceeds work budget");
    d->digest=d->input_digest();d->capture_ms=elapsed(started);
    if(identity_!=doc.analysis_identity_.token || d->revision!=doc.revision()) throw std::runtime_error("document changed during spatial capture");
    data_=std::move(d);
}
std::string SpatialDistanceRequest::Data::input_digest() const {
    Sha256 hash;text(hash,"qpx.centroid-nearest/1");hash.integer(unsigned(options.units));hash.integer(options.prefer_nucleus);hash.integer(options.exclude_self);
    hash.integer(revision);hash.integer(provenance->source.width);hash.integer(provenance->source.height);real(hash,sx);real(hash,sy);
    for(auto v:{provenance->effective_pixel_width_um,provenance->effective_pixel_height_um}) {hash.integer(bool(v));if(v) real(hash,*v);}
    hash.integer(objects.size());
    for(const auto& a:objects) {hash.integer(a.id);hash.integer(a.revision);hash.integer(unsigned(a.kind));hash.integer(a.nucleus);real(hash,a.centroid.x);real(hash,a.centroid.y);text(hash,a.geometry_sha256);}
    for(const auto* ids:{&sources,&targets}) {hash.integer(ids->size());for(auto i:*ids) hash.integer(objects[i].id);}
    return hash.finish();
}
bool SpatialDistanceRequest::Data::same_geometry(const Data& b) const {
    if(options.units!=b.options.units || options.prefer_nucleus!=b.options.prefer_nucleus || options.exclude_self!=b.options.exclude_self ||
       sx!=b.sx || sy!=b.sy || sources!=b.sources || targets!=b.targets || objects.size()!=b.objects.size() ||
       provenance->source.width!=b.provenance->source.width || provenance->source.height!=b.provenance->source.height ||
       provenance->effective_pixel_width_um!=b.provenance->effective_pixel_width_um || provenance->effective_pixel_height_um!=b.provenance->effective_pixel_height_um) return false;
    for(std::size_t i=0;i<objects.size();++i) {
        const auto& a=objects[i];const auto& c=b.objects[i];
        if(a.id!=c.id || a.revision!=c.revision || a.kind!=c.kind || a.nucleus!=c.nucleus || a.geometry_sha256!=c.geometry_sha256 || a.centroid.x!=c.centroid.x || a.centroid.y!=c.centroid.y) return false;
    }
    return true;
}
bool SpatialDistanceRequest::Data::matches_objects(const Document& doc) const {
    if(doc.metadata().width!=provenance->source.width || doc.metadata().height!=provenance->source.height ||
       doc.pixel_width_um()!=provenance->effective_pixel_width_um || doc.pixel_height_um()!=provenance->effective_pixel_height_um) return false;
    for(const auto& a:objects) {
        const auto it=doc.objects().find(a.id);
        if(it==doc.objects().end() || it->second.revision!=a.revision || it->second.kind!=a.kind ||
           (options.prefer_nucleus && it->second.kind==ObjectKind::Cell && bool(it->second.nucleus))!=a.nucleus) return false;
    }
    return true;
}
std::size_t SpatialDistanceResult::source_count() const {return snapshot_->sources.size();}
std::size_t SpatialDistanceResult::target_count() const {return snapshot_->targets.size();}
const SpatialObjectSnapshot& SpatialDistanceResult::source_object(std::size_t i) const {return snapshot_->objects.at(snapshot_->sources.at(i));}
const SpatialObjectSnapshot& SpatialDistanceResult::target_object(std::size_t i) const {return snapshot_->objects.at(snapshot_->targets.at(i));}
std::size_t SpatialDistanceRecord::payload_bytes() const {
    if(!result) throw std::invalid_argument("spatial record has no result");
    return sizeof(*this)+256+name.capacity()+producer.capacity()+result->payload_bytes();
}
void Document::restore_spatial(std::shared_ptr<const SpatialDistanceRecord> record) {
    if(!record || !record->result || !record->id || record->id>revision_ || !record->captured_next_id || record->captured_next_id>next_id_ || record->result->document_revision()>=record->id)
        throw std::invalid_argument("invalid spatial record identity/revision bounds");
    detail::bounded_utf8(record->name,256);detail::bounded_utf8(record->producer,256);
    if(record->name.empty() || record->producer.empty()) throw std::invalid_argument("spatial record requires a name and producer");
    const auto& d=*record->result->snapshot_;
    if(d.provenance->source.width!=metadata().width || d.provenance->source.height!=metadata().height) throw std::invalid_argument("spatial record image dimensions mismatch");
    for(const auto& a:d.objects) if(a.id>=record->captured_next_id) throw std::invalid_argument("spatial record exceeds captured object identity bounds");
    const auto id=record->id;
    if(spatial_records_.count(id) || analysis_.count(id) || raw_analysis_.count(id) || tissue_.count(id) || cells_.count(id) || classifiers_.count(id) || pixels_.count(id) || raw_pixels_.count(id) || streams_.count(id) || raw_streams_.count(id)) throw std::invalid_argument("duplicate spatial record ID");
    const auto bytes=record->payload_bytes();if(spatial_records_.size()>=10000 || bytes>256u*1024*1024-spatial_bytes_) throw std::length_error("spatial records exceed 10000-record/256 MiB retained limit");
    spatial_records_.emplace(id,StoredSpatial{std::move(record),{},false});spatial_bytes_+=bytes;
}
std::uint64_t Document::keep_spatial(const SpatialDistanceRequest& request,std::shared_ptr<const SpatialDistanceResult> result,std::string name) {
    if(!result || !request.matches(*this) || !result->belongs_to(request)) throw std::invalid_argument("spatial result does not match its current request");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");
    auto record=std::make_shared<SpatialDistanceRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->result=std::move(result);
    ++revision_;try {restore_spatial(record);}catch(...) {--revision_;throw;}
    auto& stored=spatial_records_.at(record->id);stored.identity=analysis_identity_.token;stored.verified=true;return record->id;
}
std::shared_ptr<const SpatialDistanceRecord> Document::spatial_record(std::uint64_t id) const {
    const auto it=spatial_records_.find(id);if(it==spatial_records_.end()) throw std::out_of_range("spatial record does not exist");return it->second.value;
}
std::vector<std::shared_ptr<const SpatialDistanceRecord>> Document::spatial_records() const {
    std::vector<std::shared_ptr<const SpatialDistanceRecord>> out;out.reserve(spatial_records_.size());for(const auto& entry:spatial_records_) out.push_back(entry.second.value);return out;
}
std::string Document::spatial_status(std::uint64_t id) const {
    const auto record=spatial_record(id);const auto& stored=spatial_records_.at(id);
    if(!stored.identity || stored.identity!=analysis_identity_.token) return "unverified";
    if(!record->result->snapshot_->matches_objects(*this)) return "objects_or_calibration_changed";
    return stored.verified?"current":"verification_failed";
}
bool Document::verify_spatial(std::uint64_t id,const SpatialDistanceRequest& request,std::shared_ptr<const SpatialDistanceResult> result) {
    if(!result || !request.matches(*this) || !result->belongs_to(request)) throw std::invalid_argument("spatial verification requires a current captured result");
    const auto record=spatial_record(id);const auto& old=*record->result;
    if(!old.snapshot_->same_geometry(*result->snapshot_)) throw std::invalid_argument("verification request does not match saved spatial geometry/parameters");
    bool same=old.rows().size()==result->rows().size();
    for(std::size_t i=0;same && i<old.rows().size();++i) same=old.rows()[i].target_id==result->rows()[i].target_id && old.rows()[i].distance==result->rows()[i].distance;
    auto& stored=spatial_records_.at(id);stored.identity=analysis_identity_.token;stored.verified=same;return same;
}
bool Document::remove_spatial(std::uint64_t id) {
    const auto it=spatial_records_.find(id);if(it==spatial_records_.end()) return false;
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");spatial_bytes_-=it->second.value->payload_bytes();spatial_records_.erase(it);++revision_;return true;
}
SpatialDistanceRequest SpatialDistanceRequest::replay(const Document& doc,std::uint64_t id) {
    const auto result=doc.spatial_record(id)->result;const auto& d=*result->snapshot_;
    std::vector<std::uint64_t> sources,targets;sources.reserve(d.sources.size());targets.reserve(d.targets.size());
    for(auto i:d.sources) sources.push_back(d.objects[i].id);for(auto i:d.targets) targets.push_back(d.objects[i].id);
    SpatialDistanceRequest request(doc,sources,targets,d.options);
    if(!d.same_geometry(*request.data_)) throw std::invalid_argument("saved spatial geometry or calibration has changed");return request;
}
void SpatialDistanceRequest::on_owner() const {on_thread(owner_);}
bool SpatialDistanceRequest::matches(const Document& doc) const {on_owner();return identity_==doc.analysis_identity_.token && data_->revision==doc.revision();}
std::size_t SpatialDistanceRequest::source_count() const {return data_->sources.size();}
std::size_t SpatialDistanceRequest::target_count() const {return data_->targets.size();}
const SpatialObjectSnapshot& SpatialDistanceRequest::source_object(std::size_t i) const {return data_->objects.at(data_->sources.at(i));}
const SpatialObjectSnapshot& SpatialDistanceRequest::target_object(std::size_t i) const {return data_->objects.at(data_->targets.at(i));}
std::size_t SpatialDistanceRequest::snapshot_bytes() const {return data_->bytes;}
std::size_t SpatialDistanceRequest::required_work_bytes() const {return data_->work;}
std::uint64_t SpatialDistanceRequest::document_revision() const {return data_->revision;}
double SpatialDistanceRequest::capture_ms() const {return data_->capture_ms;}
double SpatialDistanceRequest::x_scale() const {return data_->sx;}
double SpatialDistanceRequest::y_scale() const {return data_->sy;}
SpatialDistanceOptions SpatialDistanceRequest::options() const {return data_->options;}
const std::string& SpatialDistanceRequest::input_sha256() const {return data_->digest;}
const std::shared_ptr<const AnalysisProvenance>& SpatialDistanceRequest::provenance() const {return data_->provenance;}
const std::string& SpatialDistanceResult::input_sha256() const {return snapshot_->digest;}
SpatialDistanceOptions SpatialDistanceResult::options() const {return snapshot_->options;}
double SpatialDistanceResult::x_scale() const {return snapshot_->sx;}
double SpatialDistanceResult::y_scale() const {return snapshot_->sy;}
std::uint64_t SpatialDistanceResult::document_revision() const {return snapshot_->revision;}
const std::shared_ptr<const AnalysisProvenance>& SpatialDistanceResult::provenance() const {return snapshot_->provenance;}
std::size_t SpatialDistanceResult::payload_bytes() const {return sizeof(*this)+snapshot_->bytes+rows_.capacity()*sizeof(SpatialDistanceRow)+sha256_.capacity();}
bool SpatialDistanceResult::belongs_to(const SpatialDistanceRequest& request) const {return snapshot_==request.data_;}
struct SpatialDistanceKernel {
    static std::shared_ptr<const SpatialDistanceResult> run(const SpatialDistanceRequest& request,const std::atomic<bool>* cancelled,
        std::atomic<std::uint64_t>* progress=nullptr,std::atomic<std::uint64_t>* tests=nullptr) {
        checkpoint(cancelled);const auto started=Clock::now();const auto& d=*request.data_;
        std::vector<SpatialEntry> entries;entries.reserve(d.targets.size());
        for(auto i:d.targets) {checkpoint(cancelled);const auto& a=d.objects[i];entries.push_back({a.id,{a.centroid.x,a.centroid.y,0,0}});}
        SpatialIndex index(std::move(entries));checkpoint(cancelled);
        if(index.payload_bytes()>SpatialIndex::estimated_bytes(d.targets.size())) throw std::length_error("spatial index allocation exceeds admitted bound");
        auto out=std::shared_ptr<SpatialDistanceResult>(new SpatialDistanceResult());out->snapshot_=request.data_;out->rows_.reserve(d.sources.size());
        if(out->payload_bytes()+index.payload_bytes()+8192>d.options.work_bytes) throw std::length_error("spatial result allocation exceeds work budget");
        auto remaining=d.options.max_distance_tests;Sha256 hash;text(hash,"qpx.centroid-nearest-result/1");text(hash,d.digest);hash.integer(d.sources.size());
        try {
            for(auto i:d.sources) {
                checkpoint(cancelled);const auto& a=d.objects[i];SpatialDistanceRow row;
                row.source_id=a.id;row.source_revision=a.revision;row.source_centroid=a.centroid;row.source_kind=a.kind;row.source_nucleus=a.nucleus;
                const auto nearest=index.nearest_point(a.centroid,d.sx,d.sy,d.options.exclude_self?std::optional<std::uint64_t>(a.id):std::nullopt,cancelled,&remaining);
                hash.integer(a.id);hash.integer(bool(nearest));
                if(nearest) {
                    const auto target=std::lower_bound(d.objects.begin(),d.objects.end(),nearest->id,[](const auto& a,auto id){return a.id<id;});
                    if(target==d.objects.end() || target->id!=nearest->id) throw std::logic_error("spatial index returned an uncaptured object");
                    row.target_id=target->id;row.target_revision=target->revision;row.target_centroid=target->centroid;row.target_kind=target->kind;row.target_nucleus=target->nucleus;row.distance=nearest->distance;
                    hash.integer(target->id);real(hash,*row.distance);
                }
                out->rows_.push_back(row);if(tests) tests->store(d.options.max_distance_tests-remaining,std::memory_order_relaxed);if(progress) progress->store(out->rows_.size(),std::memory_order_relaxed);
            }
        }catch(...) {if(tests) tests->store(d.options.max_distance_tests-remaining,std::memory_order_relaxed);throw;}
        checkpoint(cancelled);out->sha256_=hash.finish();out->distance_tests_=d.options.max_distance_tests-remaining;out->elapsed_ms_=elapsed(started);return out;
    }
};
std::shared_ptr<const SpatialDistanceResult> measure_spatial_distances(const SpatialDistanceRequest& request,const std::atomic<bool>* cancelled) {return SpatialDistanceKernel::run(request,cancelled);}
struct SpatialDistanceJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::optional<SpatialDistanceRequest> request;
    std::shared_ptr<const SpatialDistanceResult> result;
    std::future<std::shared_ptr<const SpatialDistanceResult>> future;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> progress{0},tests{0};
    bool stale=false;SpatialDistanceStats stats;Clock::time_point started;
};
SpatialDistanceJob::SpatialDistanceJob():impl_(std::make_unique<Impl>()) {}
SpatialDistanceJob::~SpatialDistanceJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool SpatialDistanceJob::busy() const {on_thread(impl_->owner);return impl_->future.valid();}
void SpatialDistanceJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
bool SpatialDistanceJob::request(const SpatialDistanceRequest& request) {
    auto& s=*impl_;on_thread(s.owner);request.on_owner();if(s.future.valid()) return false;
    if(s.stats.generation==UINT64_MAX) throw std::overflow_error("spatial job generation exhausted");
    const auto old_cancel=s.cancelled.exchange(false);const auto old_progress=s.progress.exchange(0),old_tests=s.tests.exchange(0);
    std::future<std::shared_ptr<const SpatialDistanceResult>> next;
    try {next=std::async(std::launch::async,[request,&s] {return SpatialDistanceKernel::run(request,&s.cancelled,&s.progress,&s.tests);});}
    catch(...) {if(old_cancel) s.cancelled.store(true);s.progress.store(old_progress);s.tests.store(old_tests);throw;}
    s.future=std::move(next);s.request=request;s.result.reset();s.stale=false;const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.total_rows=request.source_count();s.stats.snapshot_bytes=request.snapshot_bytes();s.stats.work_bytes=request.required_work_bytes();s.stats.capture_ms=request.capture_ms();s.started=Clock::now();return true;
}
SpatialDistanceStats SpatialDistanceJob::poll(const Document& doc) {
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
SpatialDistanceStats SpatialDistanceJob::wait(const Document& doc) {poll(doc);if(impl_->future.valid()) impl_->future.wait();return poll(doc);}
void SpatialDistanceJob::wait_work() {on_thread(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
std::shared_ptr<const SpatialDistanceResult> SpatialDistanceJob::result() const {on_thread(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->result;}
}
