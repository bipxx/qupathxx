#include "qupathxx/spatial_frontend.hpp"
#include "qupathxx/source_identity.hpp"
#include "spatial_output_guard.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <variant>
#include <type_traits>
namespace qpx {
void GuiSpatialOptions::validate() const {
    analysis.validate();detail::bounded_utf8(csv_path,32768);
    if(operation!=SpatialOperation::NearestDistance && operation!=SpatialOperation::RadiusCount && operation!=SpatialOperation::TissueDensity) throw std::invalid_argument("invalid spatial operation");
    if(operation==SpatialOperation::RadiusCount) {SpatialNeighborhoodOptions o;static_cast<SpatialDistanceOptions&>(o)=analysis;o.radius=radius;o.validate();}
    if(operation==SpatialOperation::TissueDensity) {
        density.validate();if(sources.size()>10000 || tissue.size()>10000 || excluded.size()>10000 || (run && tissue.empty())) throw std::invalid_argument("invalid density viewer cohorts");
    }
    if(sources.size()>100000 || targets.size()>100000 || (run && (sources.empty() || record_id)) || !csv_bytes || csv_bytes>1024u*1024*1024)
        throw std::invalid_argument("invalid spatial viewer options");
}
namespace detail {
namespace {
constexpr std::size_t path_limit=16u*1024*1024;
void add_path(std::vector<std::string>& paths,std::size_t& bytes,const std::string& value,std::size_t limit=path_limit) {
    if(value.empty()) return;const auto path=export_path(value);
    const auto charge=512+4*path.size();if(bytes>limit || charge>limit-bytes) throw std::length_error("spatial protected paths exceed16 MiB snapshot limit");bytes+=charge;paths.push_back(path);
}
}
std::vector<std::string> spatial_project_paths(const Project* project,const std::vector<ProjectImage>* catalog,std::size_t limit) {
    limit=std::min(limit,path_limit);std::vector<std::string> paths;std::size_t bytes=0;if(!project) return paths;
    if(!catalog) throw std::invalid_argument("spatial export requires the owner image catalog");
    for(const auto* value:{&project->path(),&project->recovery_source()}) if(!value->empty()) {
        add_path(paths,bytes,*value,limit);for(const auto* suffix:{"-journal","-wal","-shm"}) add_path(paths,bytes,*value+suffix,limit);
    }
    for(const auto& image:*catalog) if(!image.embedded) add_path(paths,bytes,image.source_path,limit);return paths;
}
namespace {
// Typed operation states share one publication/export/lifecycle controller.
// No frontend computes centroids, distances or counts itself.
template<class Request,class Result,class Record,class Job,SpatialOperation Operation> struct SpatialMode {
    static constexpr bool neighborhood=Operation==SpatialOperation::RadiusCount,density=Operation==SpatialOperation::TissueDensity;
    using RequestType=Request;using RecordType=Record;using JobType=Job;
    std::unique_ptr<Job> job=std::make_unique<Job>();
    std::optional<Request> request;
    std::shared_ptr<const Record> record;
    std::shared_ptr<const Result> result;
    bool verifying=false;
};
using DistanceMode=SpatialMode<SpatialDistanceRequest,SpatialDistanceResult,SpatialDistanceRecord,SpatialDistanceJob,SpatialOperation::NearestDistance>;
using NeighborhoodMode=SpatialMode<SpatialNeighborhoodRequest,SpatialNeighborhoodResult,SpatialNeighborhoodRecord,SpatialNeighborhoodJob,SpatialOperation::RadiusCount>;
using DensityMode=SpatialMode<SpatialDensityRequest,SpatialDensityResult,SpatialDensityRecord,SpatialDensityJob,SpatialOperation::TissueDensity>;
template<class Mode> auto saved_record(const Document& doc,std::uint64_t id) {
    if constexpr(Mode::density) return doc.density_record(id);else if constexpr(Mode::neighborhood) return doc.neighborhood_record(id);else return doc.spatial_record(id);
}
template<class Mode,class ResultPtr> bool verify_record(Document& doc,std::uint64_t id,const typename Mode::RequestType& request,const ResultPtr& result) {
    if constexpr(Mode::density) return doc.verify_density(id,request,result);else if constexpr(Mode::neighborhood) return doc.verify_neighborhood(id,request,result);else return doc.verify_spatial(id,request,result);
}
}
struct SpatialFrontend::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::variant<DistanceMode,NeighborhoodMode,DensityMode> mode;
    std::shared_ptr<const char> identity;
    GuiSpatialReport view;
    struct Export {bool committed=false;std::string error;std::size_t bytes=0;};
    std::future<Export> future;std::atomic<bool> cancelled{false};bool detached=false;
    std::uint64_t status_revision=UINT64_MAX;
    void on_owner() const {if(owner!=std::this_thread::get_id()) throw std::runtime_error("spatial frontend must run on its owner thread");}
    bool busy() const {return std::visit([](const auto& m){return m.job->busy();},mode) || future.valid();}
    void idle() const {on_owner();if(busy()) throw std::runtime_error("a spatial operation is already running");}
    GuiSpatialReport fresh_view(const std::string& state) const {
        auto next=view;next.state=state;next.error.clear();next.record_status.clear();next.result.reset();next.neighborhood_result.reset();next.density_result.reset();next.analysis={};next.density_analysis={};
        next.export_state="idle";next.export_error.clear();next.export_committed=false;next.export_rows=next.export_bytes=0;next.path.clear();return next;
    }
    template<class Mode> void start(const Document& doc,typename Mode::RequestType capture,std::shared_ptr<const typename Mode::RecordType> retained={}) {
        idle();auto next_view=fresh_view("running");Mode next;next.job->request(capture);next.request=std::move(capture);
        next.record=std::move(retained);next.result=next.record?next.record->result:nullptr;next.verifying=bool(next.record);
        mode=std::move(next);identity=doc.analysis_identity_.token;view=std::move(next_view);status_revision=UINT64_MAX;
    }
    template<class Mode> void inspect(const Document& doc,std::uint64_t id) {
        idle();auto record=saved_record<Mode>(doc,id);auto next_view=fresh_view("historical");Mode next;
        next.record=std::move(record);next.result=next.record->result;mode=std::move(next);
        identity=doc.analysis_identity_.token;view=std::move(next_view);status_revision=UINT64_MAX;
    }
};
SpatialFrontend::SpatialFrontend():impl_(std::make_unique<Impl>()) {}
SpatialFrontend::~SpatialFrontend() {impl_->cancelled.store(true);std::visit([](auto& m){m.job->cancel();},impl_->mode);if(impl_->future.valid()) impl_->future.wait();}
bool SpatialFrontend::busy() const {impl_->on_owner();return impl_->busy();}
bool SpatialFrontend::same_document(const Document& doc) const {impl_->on_owner();return !impl_->identity || impl_->identity==doc.analysis_identity_.token;}
void SpatialFrontend::measure(const Document& doc,const std::vector<std::uint64_t>& sources,const std::vector<std::uint64_t>& targets,SpatialDistanceOptions options) {
    impl_->idle();impl_->start<DistanceMode>(doc,SpatialDistanceRequest(doc,sources,targets,options));
}
void SpatialFrontend::measure_neighborhood(const Document& doc,const std::vector<std::uint64_t>& sources,const std::vector<std::uint64_t>& targets,SpatialNeighborhoodOptions options) {
    impl_->idle();impl_->start<NeighborhoodMode>(doc,SpatialNeighborhoodRequest(doc,sources,targets,options));
}
void SpatialFrontend::measure_density(const Document& doc,const std::vector<std::uint64_t>& windows,const std::vector<std::uint64_t>& tissue,
    const std::vector<std::uint64_t>& excluded,const std::vector<std::uint64_t>& cells,SpatialDensityOptions options) {
    impl_->idle();impl_->start<DensityMode>(doc,SpatialDensityRequest(doc,windows,tissue,excluded,cells,options));
}
void SpatialFrontend::inspect_density(const Document& doc,std::uint64_t id) {impl_->inspect<DensityMode>(doc,id);}
void SpatialFrontend::verify_density(const Document& doc,std::uint64_t id) {
    impl_->idle();impl_->start<DensityMode>(doc,SpatialDensityRequest::replay(doc,id),doc.density_record(id));
}
void SpatialFrontend::inspect(const Document& doc,std::uint64_t id) {impl_->inspect<DistanceMode>(doc,id);}
void SpatialFrontend::inspect_neighborhood(const Document& doc,std::uint64_t id) {impl_->inspect<NeighborhoodMode>(doc,id);}
void SpatialFrontend::verify(const Document& doc,std::uint64_t id) {
    impl_->idle();impl_->start<DistanceMode>(doc,SpatialDistanceRequest::replay(doc,id),doc.spatial_record(id));
}
void SpatialFrontend::verify_neighborhood(const Document& doc,std::uint64_t id) {
    impl_->idle();impl_->start<NeighborhoodMode>(doc,SpatialNeighborhoodRequest::replay(doc,id),doc.neighborhood_record(id));
}
std::uint64_t SpatialFrontend::keep(Document& doc,std::string name) {
    auto& s=*impl_;s.idle();return std::visit([&](auto& m)->std::uint64_t {
        using Mode=std::decay_t<decltype(m)>;
        if(!m.request || !m.result || m.verifying) throw std::invalid_argument("no current spatial preview to retain");
        auto next_view=s.view;next_view.state="historical";auto next_job=std::make_unique<typename Mode::JobType>();std::uint64_t id;
        if constexpr(Mode::density) {if(name=="Spatial distances") name="Tissue cell density";id=doc.keep_density(*m.request,m.result,std::move(name));}
        else if constexpr(Mode::neighborhood) {if(name=="Spatial distances") name="Spatial neighborhoods";id=doc.keep_neighborhood(*m.request,m.result,std::move(name));}
        else id=doc.keep_spatial(*m.request,m.result,std::move(name));
        m.record=saved_record<Mode>(doc,id);m.job=std::move(next_job);m.request.reset();s.view=std::move(next_view);s.status_revision=UINT64_MAX;return id;
    },s.mode);
}
void SpatialFrontend::export_csv(const Document& doc,const std::string& path,std::size_t limit,const std::vector<std::string>& supplied,std::size_t snapshot_bytes) {
    auto& s=*impl_;s.idle();if(!same_document(doc)) throw std::invalid_argument("no spatial table for this document");
    std::visit([&](auto& m) {
        using Mode=std::decay_t<decltype(m)>;
        if(!m.result) throw std::invalid_argument("no spatial table for this document");
        if(m.request && !m.request->matches(doc)) throw std::invalid_argument("spatial preview is stale; select a historical record to export it");
        if(!limit || limit>1024u*1024*1024) throw std::invalid_argument("spatial CSV limit must be1 byte..1 GiB");
        snapshot_bytes=std::min(snapshot_bytes,path_limit);if(!snapshot_bytes)throw std::invalid_argument("spatial export snapshot budget is zero");
        const auto destination=export_path(path);std::vector<std::string> paths;std::size_t bytes=0;
        for(const auto& value:supplied) add_path(paths,bytes,value,snapshot_bytes);
        const auto provenance=m.result->provenance();
        if(!provenance->locator.empty() && std::filesystem::u8path(provenance->locator).is_absolute()) add_path(paths,bytes,provenance->locator,snapshot_bytes);
        if(provenance->reader_source) {if(provenance->reader_source->primary) add_path(paths,bytes,provenance->reader_source->primary->path,snapshot_bytes);for(const auto& file:provenance->reader_source->companions) add_path(paths,bytes,file.path,snapshot_bytes);}
        for(const auto& input:paths) protect_spatial_path(input,"",destination);
        auto next_view=s.view;next_view.export_state="writing";next_view.export_error.clear();next_view.export_committed=false;next_view.export_bytes=0;next_view.export_rows=m.result->rows().size();next_view.path=destination;
        const auto result=m.result;s.cancelled.store(false);s.detached=false;
        auto future=std::async(std::launch::async,[result,paths=std::move(paths),destination,limit,&s] {
            Impl::Export e;try {
                auto guard=[&]{for(const auto& input:paths) protect_spatial_path(input,"",destination);};
                if constexpr(Mode::density) e.bytes=save_spatial_density_csv(*result,destination,limit,false,&s.cancelled,guard);
                else if constexpr(Mode::neighborhood) e.bytes=save_spatial_neighborhood_csv(*result,destination,limit,false,&s.cancelled,guard);
                else e.bytes=save_spatial_distance_csv(*result,destination,limit,false,&s.cancelled,guard);
                e.committed=true;
            }catch(const std::exception& error) {e.error=error.what();}return e;
        });s.future=std::move(future);s.view=std::move(next_view);
    },s.mode);
}
void SpatialFrontend::poll(Document& doc) {
    auto& s=*impl_;s.on_owner();
    if(s.identity && s.identity!=doc.analysis_identity_.token) {
        std::visit([](auto& m){m.job->cancel();m.record.reset();m.result.reset();},s.mode);
        s.cancelled.store(true);s.detached=true;s.view.state="stale";s.status_revision=UINT64_MAX;
        if(!s.future.valid()) {s.view.export_state="idle";s.view.export_committed=false;s.view.export_rows=s.view.export_bytes=0;s.view.path.clear();}
    }
    s.identity=doc.analysis_identity_.token;
    std::visit([&](auto& m) {
        using Mode=std::decay_t<decltype(m)>;
        if(m.request) {
            const auto stats=m.job->poll(doc);s.view.state=stats.state;s.view.error=stats.error;
            if constexpr(Mode::density) s.view.density_analysis=stats;else s.view.analysis=stats;
            if(!m.job->busy()) {
                const auto result=m.job->result();
                if(result && m.verifying) {
                    try {const bool verified=m.record && verify_record<Mode>(doc,m.record->id,*m.request,result);s.view.state=verified?"verified":"verification_failed";}
                    catch(const std::exception& e) {s.view.state="failed";s.view.error=e.what();}
                    m.request.reset();m.verifying=false;s.status_revision=UINT64_MAX;
                } else if(!m.verifying) m.result=result;
                if(!result && s.view.state!="ready") {m.request.reset();m.verifying=false;}
            }
        }
        if(m.record && s.status_revision!=doc.revision()) {
            try {if constexpr(Mode::density) s.view.record_status=doc.density_status(m.record->id);else if constexpr(Mode::neighborhood) s.view.record_status=doc.neighborhood_status(m.record->id);else s.view.record_status=doc.spatial_status(m.record->id);}
            catch(const std::out_of_range&) {s.view.record_status="removed";}
            s.status_revision=doc.revision();if(s.view.state=="verified" && s.view.record_status!="current") s.view.state="historical";
        } else if(!m.record) s.view.record_status.clear();
    },s.mode);
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {const auto exported=s.future.get();s.view.export_committed=exported.committed;s.view.export_bytes=exported.bytes;s.view.export_error=exported.error;
            s.view.export_state=exported.committed?(s.detached?"detached":"saved"):s.cancelled.load()?"cancelled":"failed";
        }catch(const std::exception& e) {s.view.export_state="failed";s.view.export_error=e.what();}
    }
}
void SpatialFrontend::cancel() {impl_->on_owner();std::visit([](auto& m){m.job->cancel();},impl_->mode);impl_->cancelled.store(true);}
void SpatialFrontend::finish(Document& doc,bool wait) {
    impl_->on_owner();if(!wait) cancel();std::visit([](auto& m){m.job->wait_work();},impl_->mode);if(impl_->future.valid()) impl_->future.wait();poll(doc);
}
GuiSpatialReport SpatialFrontend::report() const {
    const auto& s=*impl_;s.on_owner();auto out=s.view;out.busy=s.busy();out.result.reset();out.neighborhood_result.reset();out.density_result.reset();
    std::visit([&](const auto& m) {
        using Mode=std::decay_t<decltype(m)>;out.record_id=m.record?m.record->id:0;
        if constexpr(Mode::density) {out.operation=SpatialOperation::TissueDensity;out.density_result=m.result;}
        else if constexpr(Mode::neighborhood) {out.operation=SpatialOperation::RadiusCount;out.neighborhood_result=m.result;}
        else {out.operation=SpatialOperation::NearestDistance;out.result=m.result;}
        if(m.result) {out.rows=m.result->rows().size();
            if constexpr(Mode::density) {out.sources=m.result->window_count();out.targets=m.result->cell_count();}
            else {out.sources=m.result->source_count();out.targets=m.result->target_count();}
        }
    },s.mode);return out;
}
}
}
