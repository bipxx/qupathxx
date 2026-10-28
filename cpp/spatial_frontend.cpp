#include "qupathxx/spatial_frontend.hpp"
#include "qupathxx/source_identity.hpp"
#include "spatial_output_guard.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
namespace qpx {
void GuiSpatialOptions::validate() const {
    analysis.validate();detail::bounded_utf8(csv_path,32768);
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
struct SpatialFrontend::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::unique_ptr<SpatialDistanceJob> job=std::make_unique<SpatialDistanceJob>();
    std::optional<SpatialDistanceRequest> request;
    std::shared_ptr<const char> identity;
    std::shared_ptr<const SpatialDistanceRecord> record;
    std::shared_ptr<const SpatialDistanceResult> result;
    GuiSpatialReport view;bool verifying=false;
    struct Export {bool committed=false;std::string error;std::size_t bytes=0;};
    std::future<Export> future;std::atomic<bool> cancelled{false};bool detached=false;
    std::uint64_t status_revision=UINT64_MAX;
    void on_owner() const {if(owner!=std::this_thread::get_id()) throw std::runtime_error("spatial frontend must run on its owner thread");}
    bool busy() const {return job->busy() || future.valid();}
    void idle() const {on_owner();if(busy()) throw std::runtime_error("a spatial operation is already running");}
    void start(const Document& doc,SpatialDistanceRequest capture,std::shared_ptr<const SpatialDistanceRecord> retained={}) {
        idle();auto next_view=view;next_view.state="running";next_view.error.clear();next_view.record_status.clear();
        next_view.export_state="idle";next_view.export_error.clear();next_view.export_committed=false;next_view.export_rows=next_view.export_bytes=0;next_view.path.clear();
        auto next=std::make_unique<SpatialDistanceJob>();next->request(capture);
        job.swap(next);request=std::move(capture);identity=doc.analysis_identity_.token;record=std::move(retained);result=record?record->result:nullptr;
        verifying=bool(record);view=std::move(next_view);status_revision=UINT64_MAX;
    }
};
SpatialFrontend::SpatialFrontend():impl_(std::make_unique<Impl>()) {}
SpatialFrontend::~SpatialFrontend() {impl_->cancelled.store(true);impl_->job->cancel();if(impl_->future.valid()) impl_->future.wait();}
bool SpatialFrontend::busy() const {impl_->on_owner();return impl_->busy();}
bool SpatialFrontend::same_document(const Document& doc) const {impl_->on_owner();return !impl_->identity || impl_->identity==doc.analysis_identity_.token;}
void SpatialFrontend::measure(const Document& doc,const std::vector<std::uint64_t>& sources,const std::vector<std::uint64_t>& targets,SpatialDistanceOptions options) {
    impl_->idle();impl_->start(doc,SpatialDistanceRequest(doc,sources,targets,options));
}
void SpatialFrontend::inspect(const Document& doc,std::uint64_t id) {
    auto& s=*impl_;s.idle();const auto record=doc.spatial_record(id);auto next=std::make_unique<SpatialDistanceJob>();
    s.job.swap(next);s.request.reset();s.identity=doc.analysis_identity_.token;s.record=record;s.result=record->result;s.verifying=false;s.view.state="historical";s.view.error.clear();s.status_revision=UINT64_MAX;
    s.view.export_state="idle";s.view.export_error.clear();s.view.export_committed=false;s.view.export_rows=s.view.export_bytes=0;s.view.path.clear();
}
void SpatialFrontend::verify(const Document& doc,std::uint64_t id) {
    impl_->idle();const auto record=doc.spatial_record(id);impl_->start(doc,SpatialDistanceRequest::replay(doc,id),record);
}
std::uint64_t SpatialFrontend::keep(Document& doc,std::string name) {
    auto& s=*impl_;s.idle();if(!s.request || !s.result || s.verifying) throw std::invalid_argument("no current spatial preview to retain");
    auto next=std::make_unique<SpatialDistanceJob>();auto next_view=s.view;next_view.state="historical";
    const auto id=doc.keep_spatial(*s.request,s.result,std::move(name));
    s.record=doc.spatial_record(id);s.job.swap(next);s.request.reset();s.view=std::move(next_view);s.status_revision=UINT64_MAX;return id;
}
void SpatialFrontend::export_csv(const Document& doc,const std::string& path,std::size_t limit,const std::vector<std::string>& supplied,std::size_t snapshot_bytes) {
    auto& s=*impl_;s.idle();if(!same_document(doc) || !s.result) throw std::invalid_argument("no spatial table for this document");
    if(s.request && !s.request->matches(doc)) throw std::invalid_argument("spatial preview is stale; select a historical record to export it");
    if(!limit || limit>1024u*1024*1024) throw std::invalid_argument("spatial CSV limit must be1 byte..1 GiB");
    snapshot_bytes=std::min(snapshot_bytes,path_limit);if(!snapshot_bytes)throw std::invalid_argument("spatial export snapshot budget is zero");
    const auto destination=export_path(path);std::vector<std::string> paths;std::size_t bytes=0;
    for(const auto& value:supplied) add_path(paths,bytes,value,snapshot_bytes);
    const auto provenance=s.result->provenance();
    if(!provenance->locator.empty() && std::filesystem::u8path(provenance->locator).is_absolute()) add_path(paths,bytes,provenance->locator,snapshot_bytes);
    if(provenance->reader_source) {if(provenance->reader_source->primary) add_path(paths,bytes,provenance->reader_source->primary->path,snapshot_bytes);for(const auto& file:provenance->reader_source->companions) add_path(paths,bytes,file.path,snapshot_bytes);}
    for(const auto& input:paths) protect_spatial_path(input,"",destination);
    auto next_view=s.view;next_view.export_state="writing";next_view.export_error.clear();next_view.export_committed=false;next_view.export_bytes=0;next_view.export_rows=s.result->rows().size();next_view.path=destination;
    const auto result=s.result;s.cancelled.store(false);s.detached=false;
    auto future=std::async(std::launch::async,[result,paths=std::move(paths),destination,limit,&s] {
        Impl::Export e;try {e.bytes=save_spatial_distance_csv(*result,destination,limit,false,&s.cancelled,[&]{for(const auto& input:paths) protect_spatial_path(input,"",destination);});e.committed=true;}
        catch(const std::exception& error) {e.error=error.what();}return e;
    });s.future=std::move(future);s.view=std::move(next_view);
}
void SpatialFrontend::poll(Document& doc) {
    auto& s=*impl_;s.on_owner();
    if(s.identity && s.identity!=doc.analysis_identity_.token) {
        s.job->cancel();s.cancelled.store(true);s.detached=true;s.record.reset();s.result.reset();s.view.state="stale";s.status_revision=UINT64_MAX;
        if(!s.future.valid()) {s.view.export_state="idle";s.view.export_committed=false;s.view.export_rows=s.view.export_bytes=0;s.view.path.clear();}
    }
    s.identity=doc.analysis_identity_.token;
    if(s.request) {
        s.view.analysis=s.job->poll(doc);s.view.state=s.view.analysis.state;s.view.error=s.view.analysis.error;
        if(!s.job->busy()) {
            const auto result=s.job->result();
            if(result && s.verifying) {
                try {const bool verified=s.record && doc.verify_spatial(s.record->id,*s.request,result);s.view.state=verified?"verified":"verification_failed";}
                catch(const std::exception& e) {s.view.state="failed";s.view.error=e.what();}
                s.request.reset();s.verifying=false;s.status_revision=UINT64_MAX;
            } else if(!s.verifying) s.result=result;
            if(!result && s.view.state!="ready") {s.request.reset();s.verifying=false;}
        }
    }
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {const auto exported=s.future.get();s.view.export_committed=exported.committed;s.view.export_bytes=exported.bytes;s.view.export_error=exported.error;
            s.view.export_state=exported.committed?(s.detached?"detached":"saved"):s.cancelled.load()?"cancelled":"failed";
        }catch(const std::exception& e) {s.view.export_state="failed";s.view.export_error=e.what();}
    }
    if(s.record && s.status_revision!=doc.revision()) {
        try {s.view.record_status=doc.spatial_status(s.record->id);}catch(const std::out_of_range&) {s.view.record_status="removed";}
        s.status_revision=doc.revision();
        if(s.view.state=="verified" && s.view.record_status!="current") s.view.state="historical";
    } else if(!s.record) s.view.record_status.clear();
}
void SpatialFrontend::cancel() {impl_->on_owner();impl_->job->cancel();impl_->cancelled.store(true);}
void SpatialFrontend::finish(Document& doc,bool wait) {
    impl_->on_owner();if(!wait) cancel();impl_->job->wait_work();if(impl_->future.valid()) impl_->future.wait();poll(doc);
}
GuiSpatialReport SpatialFrontend::report() const {
    const auto& s=*impl_;s.on_owner();auto out=s.view;out.busy=s.busy();out.record_id=s.record?s.record->id:0;out.result=s.result;
    if(s.result) {out.rows=s.result->rows().size();out.sources=s.result->source_count();out.targets=s.result->target_count();}return out;
}
}
}
