#include "density_snapshot.hpp"
#include "spatial_snapshot.hpp"
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
void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("spatial density analysis cancelled");}
void on_thread(std::thread::id owner) {if(owner!=std::this_thread::get_id()) throw std::runtime_error("spatial density controller must run on its owner thread");}
void text(Sha256& h,const std::string& s) {h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double v) {std::uint64_t bits;std::memcpy(&bits,&v,8);h.integer(bits);}
double elapsed(Clock::time_point t) {return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
std::string geometry_hash(const Roi& roi) {const auto bytes=GeometryArchiveCodec::encode(roi);Sha256 h;h.update(bytes.data(),bytes.size());return h.finish();}
std::size_t vertices(const Roi& roi) {
    if(roi.kind()==RoiKind::Ellipse) return 512;
    if(roi.kind()==RoiKind::Rectangle) return 4;
    std::size_t n=0;for(const auto& path:roi.paths()) n+=path.size();return n;
}
std::vector<std::uint64_t> selection(const std::vector<std::uint64_t>& ids,bool required) {
    if((required && ids.empty()) || ids.size()>10000) throw std::invalid_argument("density area cohort requires 1..10000 IDs (exclusions may be empty)");
    auto result=ids;std::sort(result.begin(),result.end());
    if(std::adjacent_find(result.begin(),result.end())!=result.end()) throw std::invalid_argument("duplicate density area ID");return result;
}
SpatialDistanceOptions capture_options(const SpatialDensityOptions& p) {
    p.validate();SpatialDistanceOptions out;out.units=SpatialDistanceUnits::Micrometres;out.prefer_nucleus=p.prefer_nucleus;
    out.exclude_self=false;out.snapshot_bytes=p.snapshot_bytes;out.work_bytes=p.work_bytes;out.max_distance_tests=p.max_membership_tests;return out;
}
// Scaling by exponents avoids overflowing the intermediate pixel-area product
// when its final square-millimetre value is representable (also on MSVC).
double physical_area(double area,double sx,double sy) {
    int a=0,x=0,y=0;
    const auto mantissa=std::frexp(area,&a)*std::frexp(sx,&x)*std::frexp(sy,&y)/1e6;
    const auto result=std::ldexp(mantissa,a+x+y);
    if(!std::isfinite(result) || result<=0) throw std::overflow_error("density calibrated area is not positive finite");return result;
}
}
void SpatialDensityOptions::validate() const {
    SpatialDistanceOptions p;p.snapshot_bytes=snapshot_bytes;p.work_bytes=work_bytes;p.max_distance_tests=max_membership_tests;p.validate();
    if(!max_geometry_operations || max_geometry_operations>1000000 || !max_geometry_vertices || max_geometry_vertices>1000000)
        throw std::invalid_argument("invalid density geometry budgets");
}
SpatialDensityRequest::SpatialDensityRequest(const Document& doc,const std::vector<std::uint64_t>& windows,
    const std::vector<std::uint64_t>& tissue,const std::vector<std::uint64_t>& excluded,const std::vector<std::uint64_t>& cells,SpatialDensityOptions options) {
    const auto started=Clock::now();const auto parameters=capture_options(options);
    if(!has_geos()) throw std::runtime_error("spatial density requires GEOS topology");
    const auto w=selection(windows,true),t=selection(tissue,true),e=selection(excluded,false);
    std::vector<std::uint64_t> area_ids=w;area_ids.insert(area_ids.end(),t.begin(),t.end());area_ids.insert(area_ids.end(),e.begin(),e.end());
    std::sort(area_ids.begin(),area_ids.end());area_ids.erase(std::unique(area_ids.begin(),area_ids.end()),area_ids.end());
    auto d=std::make_shared<Data>(SpatialDistanceRequest(doc,area_ids,cells,parameters),options);
    for(std::size_t i=0;i<d->capture.target_count();++i) if(d->capture.target_object(i).kind!=ObjectKind::Cell) throw std::invalid_argument("density targets must be Cell objects");
    d->bytes=d->capture.snapshot_bytes()+sizeof(Data)+sizeof(*this)+512;
    std::size_t total_vertices=0;
    auto collect=[&](const auto& ids,auto& out) {
        const auto overhead=ids.size()*(sizeof(SpatialDensityAreaSnapshot)+128);
        if(d->bytes>options.snapshot_bytes || overhead>options.snapshot_bytes-d->bytes) throw std::length_error("density capture exceeds snapshot budget");
        d->bytes+=overhead;out.reserve(ids.size());
        for(auto id:ids) {
            const auto& object=doc.objects().at(id);const auto& roi=object.geometry;
            if(!roi.is_area() || (roi.kind()==RoiKind::Ellipse && !options.polygonize_curves)) throw std::invalid_argument("density requires area geometry; curves require explicit polygonization");
            const auto bytes=roi.payload_bytes();
            if(bytes>(options.snapshot_bytes-d->bytes)/3) throw std::length_error("density geometry exceeds snapshot budget");
            const auto n=vertices(roi);
            if(n>options.max_geometry_vertices-total_vertices) throw std::length_error("density input vertex budget exceeded");
            total_vertices+=n;d->bytes+=bytes;out.push_back({id,object.revision,roi,geometry_hash(roi)});
        }
    };
    collect(w,d->windows);collect(t,d->tissue);collect(e,d->excluded);
    // Conservative geometry allowance plus bounded query IDs, index and rows.
    // GEOS internal temporary allocation is deliberately not a process RSS cap.
    d->work=d->work_bound();
    if(d->work>options.work_bytes) throw std::length_error("density result/geometry exceeds work budget");
    d->digest=d->input_digest();d->capture_ms=elapsed(started);data_=std::move(d);
}
std::string SpatialDensityRequest::Data::input_digest() const {
    Sha256 h;text(h,"qpx.roi-tissue-density-input/1");text(h,"qpx.polygon512-closed-exclusion/1");text(h,capture.input_sha256());h.integer(options.polygonize_curves);
    for(const auto* group:{&windows,&tissue,&excluded}) {h.integer(group->size());for(const auto& a:*group) {h.integer(a.id);h.integer(a.revision);text(h,a.geometry_sha256);}}return h.finish();
}
std::size_t SpatialDensityRequest::Data::work_bound() const {
    return capture.required_work_bytes()+bytes+sizeof(SpatialDensityResult)+8192+
        options.max_geometry_vertices*1024+windows.size()*(sizeof(SpatialDensityRow)+128)+3*capture.target_count()*sizeof(std::uint64_t);
}
bool SpatialDensityRequest::Data::same_geometry(const Data& other) const {
    if(options.polygonize_curves!=other.options.polygonize_curves || !capture.data_->same_geometry(*other.capture.data_)) return false;
    auto same=[](const auto& a,const auto& b) {
        if(a.size()!=b.size()) return false;
        for(std::size_t i=0;i<a.size();++i) if(a[i].id!=b[i].id || a[i].revision!=b[i].revision || a[i].geometry_sha256!=b[i].geometry_sha256) return false;
        return true;
    };
    return same(windows,other.windows) && same(tissue,other.tissue) && same(excluded,other.excluded);
}
void SpatialDensityRequest::on_owner() const {on_thread(owner_);}
bool SpatialDensityRequest::matches(const Document& doc) const {on_owner();return data_->capture.matches(doc);}
std::size_t SpatialDensityRequest::snapshot_bytes() const {return data_->bytes;}
std::size_t SpatialDensityRequest::required_work_bytes() const {return data_->work;}
double SpatialDensityRequest::capture_ms() const {return data_->capture_ms;}
const std::string& SpatialDensityRequest::input_sha256() const {return data_->digest;}
const std::string& SpatialDensityResult::input_sha256() const {return snapshot_->digest;}
bool SpatialDensityResult::belongs_to(const SpatialDensityRequest& r) const {return snapshot_==r.data_;}
#define QPX_DENSITY_ACCESSORS(Type,ptr) \
std::size_t Type::window_count() const {return ptr->windows.size();} \
std::size_t Type::tissue_count() const {return ptr->tissue.size();} \
std::size_t Type::exclusion_count() const {return ptr->excluded.size();} \
std::size_t Type::cell_count() const {return ptr->capture.target_count();} \
const SpatialDensityAreaSnapshot& Type::window_object(std::size_t i) const {return ptr->windows.at(i);} \
const SpatialDensityAreaSnapshot& Type::tissue_object(std::size_t i) const {return ptr->tissue.at(i);} \
const SpatialDensityAreaSnapshot& Type::exclusion_object(std::size_t i) const {return ptr->excluded.at(i);} \
const SpatialObjectSnapshot& Type::cell_object(std::size_t i) const {return ptr->capture.target_object(i);} \
double Type::x_scale() const {return ptr->capture.x_scale();} \
double Type::y_scale() const {return ptr->capture.y_scale();} \
std::uint64_t Type::document_revision() const {return ptr->capture.document_revision();} \
SpatialDensityOptions Type::options() const {return ptr->options;} \
const std::shared_ptr<const AnalysisProvenance>& Type::provenance() const {return ptr->capture.provenance();}
QPX_DENSITY_ACCESSORS(SpatialDensityRequest,data_)
QPX_DENSITY_ACCESSORS(SpatialDensityResult,snapshot_)
#undef QPX_DENSITY_ACCESSORS
std::size_t SpatialDensityResult::payload_bytes() const {
    auto bytes=sizeof(*this)+snapshot_->bytes+rows_.capacity()*sizeof(SpatialDensityRow)+sha256_.capacity();
    for(const auto& row:rows_) bytes+=row.geometry_sha256.capacity()+(row.footprint?row.footprint->payload_bytes():0);return bytes;
}
std::string SpatialDensityResult::result_digest() const {
    Sha256 hash;text(hash,"qpx.roi-tissue-density-result/1");text(hash,snapshot_->digest);hash.integer(rows_.size());
    for(const auto& row:rows_) {
        hash.integer(row.window_id);hash.integer(row.count);real(hash,row.area_pixels);real(hash,row.area_mm2);
        hash.integer(bool(row.density_per_mm2));if(row.density_per_mm2) real(hash,*row.density_per_mm2);text(hash,row.geometry_sha256);
    }
    return hash.finish();
}
std::size_t SpatialDensityRecord::payload_bytes() const {
    if(!result) throw std::invalid_argument("density record has no result");return sizeof(*this)+256+name.capacity()+producer.capacity()+result->payload_bytes();
}
struct SpatialDensityKernel {
    static std::shared_ptr<const SpatialDensityResult> run(const SpatialDensityRequest& request,const std::atomic<bool>* cancelled,
        std::atomic<std::uint64_t>* progress=nullptr,std::atomic<std::uint64_t>* tests=nullptr,std::atomic<std::uint64_t>* operations=nullptr) {
        checkpoint(cancelled);const auto started=Clock::now();const auto& d=*request.data_;
        auto out=std::shared_ptr<SpatialDensityResult>(new SpatialDensityResult());out->snapshot_=request.data_;out->rows_.reserve(request.window_count());
        auto check_geometry=[&](const Roi& roi) {checkpoint(cancelled);if(vertices(roi)>d.options.max_geometry_vertices) throw std::length_error("density output vertex budget exceeded");};
        auto boolean=[&](const Roi& a,const Roi& b,BooleanOp op) {
            checkpoint(cancelled);
            if(out->geometry_operations_==d.options.max_geometry_operations) throw std::length_error("density geometry operation budget exceeded");
            ++out->geometry_operations_;if(operations) operations->store(out->geometry_operations_,std::memory_order_relaxed);
            auto result=a.boolean_operation(b,op);checkpoint(cancelled);if(result) check_geometry(*result);return result;
        };
        auto unite=[&](const auto& cohort) {
            std::optional<Roi> result;
            for(const auto& a:cohort) {checkpoint(cancelled);auto polygon=a.geometry.as_polygon();check_geometry(polygon);
                result=result?boolean(*result,polygon,BooleanOp::Union):std::optional<Roi>(std::move(polygon));}
            return result;
        };
        const auto tissue=unite(d.tissue),excluded=unite(d.excluded);
        const auto bounds=Roi::rectangle({0,0,double(request.provenance()->source.width),double(request.provenance()->source.height)});
        std::vector<SpatialEntry> entries;entries.reserve(request.cell_count());
        for(std::size_t i=0;i<request.cell_count();++i) {checkpoint(cancelled);const auto& a=request.cell_object(i);entries.push_back({a.id,{a.centroid.x,a.centroid.y,0,0}});}
        SpatialIndex index(std::move(entries));checkpoint(cancelled);
        if(index.payload_bytes()>SpatialIndex::estimated_bytes(request.cell_count())) throw std::length_error("density index exceeds admitted bound");
        auto contains=[&](const Roi& roi,Point point) {
            checkpoint(cancelled);if(out->membership_tests_==d.options.max_membership_tests) throw std::length_error("density membership budget exceeded");
            ++out->membership_tests_;if(tests) tests->store(out->membership_tests_,std::memory_order_relaxed);return roi.contains(point);
        };
        std::size_t retained_vertices=0,retained_bytes=0;
        const auto fixed_bytes=out->payload_bytes()+index.payload_bytes()+3*request.cell_count()*sizeof(std::uint64_t)+d.options.max_geometry_vertices*1024+8192;
        if(fixed_bytes>d.options.work_bytes) throw std::length_error("density allocation exceeds work budget");
        for(const auto& window:d.windows) {
            checkpoint(cancelled);SpatialDensityRow row;row.window_id=window.id;row.window_revision=window.revision;
            auto footprint=boolean(window.geometry.as_polygon(),bounds,BooleanOp::Intersection);
            if(footprint) footprint=boolean(*footprint,*tissue,BooleanOp::Intersection);
            if(footprint && excluded) footprint=boolean(*footprint,*excluded,BooleanOp::Difference);
            if(footprint) {
                const auto n=vertices(*footprint);
                if(n>d.options.max_geometry_vertices-retained_vertices) throw std::length_error("density retained vertex budget exceeded");retained_vertices+=n;
                row.area_pixels=footprint->area();row.area_mm2=physical_area(row.area_pixels,request.x_scale(),request.y_scale());
                auto query=footprint->bounds();
                // Polygon envelopes store extent=max-min. Round extent outward
                // so reconstructed maxima include every represented vertex.
                query.width=std::nextafter(query.width,std::numeric_limits<double>::infinity());
                query.height=std::nextafter(query.height,std::numeric_limits<double>::infinity());
                const auto candidates=index.query(query);checkpoint(cancelled);
                if(candidates.capacity()>3*request.cell_count()) throw std::length_error("density candidates exceed admitted capacity");
                for(auto id:candidates) {
                    std::size_t lo=0,hi=request.cell_count();while(lo<hi) {auto mid=lo+(hi-lo)/2;if(request.cell_object(mid).id<id) lo=mid+1;else hi=mid;}
                    const auto point=request.cell_object(lo).centroid;
                    if(contains(*footprint,point) && (!excluded || !contains(*excluded,point))) ++row.count;
                }
                row.density_per_mm2=row.count/row.area_mm2;
                if(!std::isfinite(*row.density_per_mm2)) throw std::overflow_error("density is not finite");
                row.geometry_sha256=geometry_hash(*footprint);row.footprint=std::move(footprint);
            }
            retained_bytes+=row.geometry_sha256.capacity()+(row.footprint?row.footprint->payload_bytes():0);
            if(retained_bytes>d.options.work_bytes-fixed_bytes)
                throw std::length_error("density allocation exceeds work budget");
            out->rows_.push_back(std::move(row));
            if(progress) progress->store(out->rows_.size(),std::memory_order_relaxed);
        }
        checkpoint(cancelled);out->sha256_=out->result_digest();out->elapsed_ms_=elapsed(started);return out;
    }
};
std::shared_ptr<const SpatialDensityResult> measure_spatial_density(const SpatialDensityRequest& request,const std::atomic<bool>* cancelled) {
    return SpatialDensityKernel::run(request,cancelled);
}
struct SpatialDensityJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::optional<SpatialDensityRequest> request;
    std::shared_ptr<const SpatialDensityResult> result;
    std::future<std::shared_ptr<const SpatialDensityResult>> future;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> progress{0},tests{0},operations{0};
    bool stale=false;SpatialDensityStats stats;Clock::time_point started;
};
SpatialDensityJob::SpatialDensityJob():impl_(std::make_unique<Impl>()) {}
SpatialDensityJob::~SpatialDensityJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool SpatialDensityJob::busy() const {on_thread(impl_->owner);return impl_->future.valid();}
void SpatialDensityJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
bool SpatialDensityJob::request(const SpatialDensityRequest& request) {
    auto& s=*impl_;on_thread(s.owner);request.on_owner();if(s.future.valid()) return false;
    if(s.stats.generation==UINT64_MAX) throw std::overflow_error("spatial job generation exhausted");
    const auto old_cancel=s.cancelled.exchange(false);const auto old_progress=s.progress.exchange(0),old_tests=s.tests.exchange(0),old_operations=s.operations.exchange(0);
    std::future<std::shared_ptr<const SpatialDensityResult>> next;
    try {next=std::async(std::launch::async,[request,&s] {return SpatialDensityKernel::run(request,&s.cancelled,&s.progress,&s.tests,&s.operations);});}
    catch(...) {if(old_cancel) s.cancelled.store(true);s.progress.store(old_progress);s.tests.store(old_tests);s.operations.store(old_operations);throw;}
    s.future=std::move(next);s.request=request;s.result.reset();s.stale=false;const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.total_rows=request.window_count();s.stats.snapshot_bytes=request.snapshot_bytes();s.stats.work_bytes=request.required_work_bytes();s.stats.capture_ms=request.capture_ms();s.started=Clock::now();return true;
}
SpatialDensityStats SpatialDensityJob::poll(const Document& doc) {
    auto& s=*impl_;on_thread(s.owner);const bool was_busy=s.future.valid();
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=s.future.valid()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=s.future.get();if(s.stale) s.stats.state="stale";else if(s.cancelled.load()) s.stats.state="cancelled";else {s.result=std::move(result);s.stats.state="ready";}}
        catch(const std::exception& e) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";s.stats.error=e.what();s.result.reset();}
    }
    if(s.cancelled.load() && !s.stale && s.request) {s.result.reset();s.stats.state=s.future.valid()?"cancelling":"cancelled";}
    s.stats.completed_rows=s.progress.load();s.stats.membership_tests=s.tests.load();s.stats.geometry_operations=s.operations.load();if(was_busy) s.stats.elapsed_ms=elapsed(s.started);
    if(!s.future.valid() && !s.result) s.request.reset();return s.stats;
}
SpatialDensityStats SpatialDensityJob::wait(const Document& doc) {poll(doc);if(impl_->future.valid()) impl_->future.wait();return poll(doc);}
void SpatialDensityJob::wait_work() {on_thread(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
std::shared_ptr<const SpatialDensityResult> SpatialDensityJob::result() const {on_thread(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->result;}
}
