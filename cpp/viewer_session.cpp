#include "qupathxx/tma.hpp"
#include "viewer_bridge.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include "calibrated_cell_numeric.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <cmath>
#include <limits>
namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
enum class Action {Catalog,Inspect,Execute,Select,Activate,Region,Analyze,Tissue,Cells,Measurements,CellTable,Save,Close,RawMetadata,RawRegion,TiffView,RawAnalyze,RawVerify,RawTable,TmaAxis,TmaDearray};
const char* name(Action a) {static const char* values[]={"catalog","inspect","execute","select","activate","region","analyze","tissue","cells","measurements","cell_table","save","close","raw_metadata","raw_region","tiff_view","raw_analyze","raw_verify","raw_table","tma_axis","tma_dearray"};return values[static_cast<int>(a)];}
bool terminal(const std::string& state) {return state!="queued" && state!="running";}
std::string error_text(const std::exception& e) {std::string text=e.what();try {detail::bounded_utf8(text,8192);}catch(...) {return "viewer operation failed (invalid or oversized error text)";}return text;}
struct TmaAxisCommand {bool column=false,remove=false,missing=false;std::size_t index=0;std::optional<std::vector<Rect>> bounds;};
struct Command {
    Action action;std::uint64_t image=0,object=0,after=0;std::optional<std::uint64_t> revision;std::size_t limit=64;
    bool flag=false,nucleus=false;std::string text;std::vector<ObjectEdit> edits;std::vector<std::uint64_t> ids;
    RegionRequest region{0,0,0,1,1};std::shared_ptr<const StainTransform> transform;
    RawRegionRequest raw_region;TiffView tiff_view;std::optional<TmaAxisCommand> tma_axis;
    std::optional<TmaDearrayOptions> tma_dearray;
    std::optional<RawGridSampling> raw_grid;ScientificRawOptions raw_grid_options;
    RoiSamplingPolicy sampling_policy=RoiSamplingPolicy::GeometryCoverage;
    AnalysisKind analysis=AnalysisKind::OpticalDensityMeans;MeasurementTableKind table=MeasurementTableKind::Roi;
    TissueParameters tissue;CellParameters cells;std::optional<double> cell_min_area,cell_max_area;
    std::optional<GridCellSampling> cell_grid;GridCellParameters grid_cells;
    explicit Command(Action a):action(a) {}
    std::size_t bytes() const {std::size_t n=sizeof(Command)+text.capacity()+edits.capacity()*sizeof(ObjectEdit)+ids.capacity()*8+(transform?sizeof(StainTransform)+128:0);
        if(tma_axis && tma_axis->bounds) n+=tma_axis->bounds->capacity()*sizeof(Rect);
        if(tma_dearray)n+=tma_dearray->name.capacity();
        std::set<const TmaGridInfo*> grids;
        for(const auto& e:edits) {n+=e.value.name.capacity()+e.value.class_name.capacity();if(e.kind==ObjectEditKind::Create || e.kind==ObjectEditKind::Replace) n+=e.value.geometry.payload_bytes()+(e.value.nucleus?e.value.nucleus->payload_bytes():0);if(e.value.tma_core) {n+=e.value.tma_core->payload_bytes();if(grids.insert(e.value.tma_core->grid_info().get()).second) n+=e.value.tma_core->grid_info()->payload_bytes();}}return n;}
};
std::size_t payload(const ViewerEvent& e) {
    std::size_t n=e.object_ids.capacity()*8+e.images.capacity()*sizeof(ProjectImage);
    for(const auto& i:e.images) n+=i.name.capacity()+i.source_path.capacity()+i.reader.capacity();
    if(e.object) n+=sizeof(Annotation)+e.object->name.capacity()+e.object->class_name.capacity()+e.object->geometry.payload_bytes()+(e.object->nucleus?e.object->nucleus->payload_bytes():0);
    if(e.object && e.object->tma_core) n+=e.object->tma_core->payload_bytes()+e.object->tma_core->grid_info()->payload_bytes();
    if(e.metadata) n+=sizeof(ImageMetadata)+e.metadata->vendor.capacity()+e.metadata->levels.capacity()*sizeof(ImageLevel);
    if(e.analysis) n+=sizeof(AnalysisResult)+(e.analysis->provenance?e.analysis->provenance->payload_bytes():0);
    if(e.tissue) n+=e.tissue->retained_bytes();if(e.cells) n+=e.cells->retained_bytes();if(e.pixels) n+=sizeof(PixelBuffer)+e.pixels->bytes().capacity();
    if(e.raw_analysis) n+=e.raw_analysis->payload_bytes();
    if(e.tma_dearray)n+=e.tma_dearray->payload_bytes();
    if(e.raw_pixels) n+=sizeof(RawPixelBuffer)+64+e.raw_pixels->payload_bytes();if(e.raw_metadata) n+=e.raw_metadata->payload_bytes();return n;
}
}
struct ViewerSession::Impl {
    struct Ticket {ViewerEvent event;std::unique_ptr<Command> command;std::atomic<bool> cancelled{false};std::size_t queued=0,retained=0;};
    const std::string path;const ViewerLimits limits;mutable std::mutex mutex;mutable std::condition_variable changed;
    std::map<std::uint64_t,std::shared_ptr<Ticket>> tickets;std::deque<std::uint64_t> queue;std::uint64_t next=1;ViewerStats stats;
    Impl(std::string p,ViewerLimits l):path(std::move(p)),limits(l) {}
    std::uint64_t submit(Command command) {
        const auto bytes=command.bytes();if(bytes>limits.command_bytes) throw std::length_error("viewer command exceeds byte limit");auto ticket=std::make_shared<Ticket>();ticket->command=std::make_unique<Command>(std::move(command));ticket->queued=bytes;
        std::lock_guard<std::mutex> lock(mutex);if(stats.state=="closed" || stats.state=="failed") throw std::runtime_error("viewer session is closed");
        if(tickets.size()>=limits.tickets || bytes>limits.queued_bytes-stats.queued_bytes) throw std::length_error("viewer queue is full; consume completed tickets");if(next==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("viewer ticket identity exhausted");
        const auto id=next;ticket->event.ticket=id;ticket->event.epoch=stats.epoch;ticket->event.action=name(ticket->command->action);ticket->event.image_id=ticket->command->image;ticket->event.object_id=ticket->command->object;
        tickets.emplace(id,ticket);try {queue.push_back(id);}catch(...) {tickets.erase(id);throw;}++next;stats.queued_bytes+=bytes;stats.queued=queue.size();stats.tickets=tickets.size();changed.notify_all();return id;
    }
    std::shared_ptr<Ticket> find(std::uint64_t id) const {const auto it=tickets.find(id);if(it==tickets.end()) throw std::out_of_range("unknown or consumed viewer ticket");return it->second;}
};
void ViewerLimits::validate() const {
    if(!tickets || tickets>1024 || !max_edits || max_edits>4096 || command_bytes<4096 || command_bytes>32*1024*1024 || queued_bytes<command_bytes || queued_bytes>256*1024*1024 || result_bytes<65536 || result_bytes>512*1024*1024 || region_bytes<4 || region_bytes>256*1024*1024 || snapshot_bytes<4096 || snapshot_bytes>512*1024*1024) throw std::invalid_argument("invalid viewer session limits");
}
ViewerSession::ViewerSession(std::string path,ViewerLimits limits) {limits.validate();detail::bounded_utf8(path,32768);impl_=std::make_shared<Impl>(detail::export_path(path),limits);}
ViewerSession::~ViewerSession()=default;
std::string ViewerSession::project_path() const {return impl_->path;}
ViewerLimits ViewerSession::limits() const {return impl_->limits;}
ViewerStats ViewerSession::stats() const {std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->stats;}
ViewerEvent ViewerSession::poll(std::uint64_t id) const {std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->find(id)->event;}
ViewerEvent ViewerSession::wait(std::uint64_t id,double timeout) const {
    if(!std::isfinite(timeout) || timeout<0 || timeout>3600) throw std::invalid_argument("viewer wait timeout must be between 0 and 3600 seconds");
    std::unique_lock<std::mutex> lock(impl_->mutex);auto ticket=impl_->find(id);impl_->changed.wait_for(lock,std::chrono::duration<double>(timeout),[&]{return terminal(ticket->event.state);});return ticket->event;
}
ViewerEvent ViewerSession::take(std::uint64_t id) {
    std::shared_ptr<Impl::Ticket> ticket;ViewerEvent result;
    {std::lock_guard<std::mutex> lock(impl_->mutex);ticket=impl_->find(id);if(!terminal(ticket->event.state)) throw std::runtime_error("viewer ticket is not complete");result=ticket->event;impl_->stats.retained_bytes-=ticket->retained;impl_->tickets.erase(id);impl_->stats.tickets=impl_->tickets.size();}
    return result;
}

bool ViewerSession::cancel(std::uint64_t id) {
    std::unique_ptr<Command> released;std::lock_guard<std::mutex> lock(impl_->mutex);auto ticket=impl_->find(id);if(terminal(ticket->event.state)) return false;ticket->cancelled.store(true);
    if(ticket->event.state=="queued") {auto it=std::find(impl_->queue.begin(),impl_->queue.end(),id);if(it!=impl_->queue.end()) impl_->queue.erase(it);impl_->stats.queued_bytes-=ticket->queued;ticket->queued=0;released=std::move(ticket->command);ticket->event.state="cancelled";impl_->stats.queued=impl_->queue.size();++impl_->stats.completed;impl_->changed.notify_all();}return true;
}
std::uint64_t ViewerSession::catalog(std::uint64_t after,std::size_t limit) {if(!limit || limit>256) throw std::invalid_argument("catalog page limit must be 1..256");Command c(Action::Catalog);c.after=after;c.limit=limit;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::inspect(std::uint64_t image,std::uint64_t object) {Command c(Action::Inspect);c.image=image;c.object=object;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::detect_tma_grid(std::uint64_t image,TmaDearrayOptions options,std::optional<std::uint64_t> revision) {
    if(!image)throw std::invalid_argument("TMA detection requires an image ID");
    options.validate();
    Command c(Action::TmaDearray);c.image=image;c.tma_dearray=std::move(options);c.revision=revision;return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::execute(std::uint64_t image,const std::vector<ObjectEdit>& edits,std::string label,std::optional<std::uint64_t> revision) {
    if(!image || edits.empty() || edits.size()>impl_->limits.max_edits) throw std::invalid_argument("invalid viewer edit count or image ID");detail::bounded_utf8(label,4096);Command c(Action::Execute);c.image=image;c.revision=revision;c.edits=edits;c.text=std::move(label);return impl_->submit(std::move(c));
}
namespace {
Command axis_command(std::uint64_t image,std::size_t index,bool column,bool remove,std::string label,std::optional<std::vector<Rect>> bounds,bool missing,std::optional<std::uint64_t> revision) {
    if(!image || index>512) throw std::invalid_argument("invalid TMA axis image or index");detail::bounded_utf8(label,128);
    if(bounds) {if(bounds->size()>512) throw std::length_error("TMA bounds exceed 512 slots");for(const auto& value:*bounds) Roi::ellipse(value);}
    Command c(Action::TmaAxis);c.image=image;c.revision=revision;c.text=std::move(label);c.tma_axis=TmaAxisCommand{column,remove,missing,index,std::move(bounds)};return c;
}
}
std::uint64_t ViewerSession::insert_tma_row(std::uint64_t image,std::size_t index,std::string label,std::optional<std::vector<Rect>> bounds,bool missing,std::optional<std::uint64_t> revision) {return impl_->submit(axis_command(image,index,false,false,std::move(label),std::move(bounds),missing,revision));}
std::uint64_t ViewerSession::insert_tma_column(std::uint64_t image,std::size_t index,std::string label,std::optional<std::vector<Rect>> bounds,bool missing,std::optional<std::uint64_t> revision) {return impl_->submit(axis_command(image,index,true,false,std::move(label),std::move(bounds),missing,revision));}
std::uint64_t ViewerSession::remove_tma_row(std::uint64_t image,std::size_t index,std::optional<std::uint64_t> revision) {return impl_->submit(axis_command(image,index,false,true,"",{},false,revision));}
std::uint64_t ViewerSession::remove_tma_column(std::uint64_t image,std::size_t index,std::optional<std::uint64_t> revision) {return impl_->submit(axis_command(image,index,true,true,"",{},false,revision));}
std::uint64_t ViewerSession::select(std::uint64_t image,const std::vector<std::uint64_t>& ids) {if(!image || ids.size()>impl_->limits.max_edits) throw std::invalid_argument("invalid viewer selection");Command c(Action::Select);c.image=image;c.ids=ids;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::activate(std::uint64_t image) {if(!image) throw std::invalid_argument("viewer activation requires an image ID");Command c(Action::Activate);c.image=image;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::read_region(std::uint64_t image,RegionRequest region) {if(region_byte_count(region.width,region.height)>impl_->limits.region_bytes) throw std::length_error("viewer region exceeds byte limit");Command c(Action::Region);c.image=image;c.region=region;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::inspect_raw(std::uint64_t image) {Command c(Action::RawMetadata);c.image=image;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::read_raw(std::uint64_t image,RawRegionRequest r) {
    const auto unit=std::uint64_t(r.channels)+1;
    const auto limit=impl_->limits.region_bytes;
    if(!r.channels || r.channels>65536 || r.region.width<=0 || r.region.height<=0 || r.region.level<0 || std::uint64_t(r.region.width)>limit/unit || std::uint64_t(r.region.height)>limit/(unit*std::uint64_t(r.region.width))) throw std::length_error("viewer raw region exceeds byte limit");
    Command c(Action::RawRegion);c.image=image;c.raw_region=r;return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::set_tiff_view(std::uint64_t image,TiffView view) {view.validate();Command c(Action::TiffView);c.image=image;c.tiff_view=view;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::analyze(std::uint64_t image,std::uint64_t object,const StainTransform& transform,AnalysisKind kind,bool nucleus) {return analyze_with_sampling_policy(image,object,transform,RoiSamplingPolicy::GeometryCoverage,kind,nucleus);}
std::uint64_t ViewerSession::analyze_with_sampling_policy(std::uint64_t image,std::uint64_t object,const StainTransform& transform,RoiSamplingPolicy policy,AnalysisKind kind,bool nucleus) {validate_sampling_policy(policy);Command c(Action::Analyze);c.sampling_policy=policy;c.image=image;c.object=object;c.transform=std::make_shared<const StainTransform>(transform);c.analysis=kind;c.flag=nucleus;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::tissue(std::uint64_t image,std::uint64_t object,const StainTransform& transform,TissueParameters p,bool retain) {p.validate();Command c(Action::Tissue);c.image=image;c.object=object;c.transform=std::make_shared<const StainTransform>(transform);c.tissue=p;c.flag=retain;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::cells(std::uint64_t image,std::uint64_t object,const StainTransform& transform,CellParameters p,bool retain) {return cells_with_nucleus_area_limits(image,object,transform,{},{},p,retain);}
std::uint64_t ViewerSession::grid_cells(std::uint64_t image,std::uint64_t object,const StainTransform& transform,GridCellSampling sampling,GridCellParameters p,bool retain) {
    sampling.validate();p.validate();Command c(Action::Cells);c.image=image;c.object=object;c.transform=std::make_shared<const StainTransform>(transform);
    c.cell_grid=sampling;c.grid_cells=p;c.flag=retain;return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::cells_with_nucleus_area_limits(std::uint64_t image,std::uint64_t object,const StainTransform& transform,
    std::optional<double> minimum,std::optional<double> maximum,CellParameters p,bool retain) {
    p.validate();detail::validate_cell_area_limits(minimum,maximum);
    Command c(Action::Cells);c.image=image;c.object=object;c.transform=std::make_shared<const StainTransform>(transform);
    c.cells=p;c.flag=retain;c.cell_min_area=minimum;c.cell_max_area=maximum;return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::export_measurements(std::uint64_t image,MeasurementTableKind kind,const std::string& path,bool current,const std::vector<std::uint64_t>& ids) {if(ids.size()>100000 || (!image && !ids.empty())) throw std::invalid_argument("record selection requires one image and at most 100000 records");Command c(Action::Measurements);c.ids=ids;c.image=image;c.table=kind;c.text=detail::export_path(path);c.flag=current;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::export_cells(std::uint64_t image,const std::string& path,bool current,const std::vector<std::uint64_t>& ids) {if(ids.size()>100000 || (!image && !ids.empty())) throw std::invalid_argument("record selection requires one image and at most 100000 records");Command c(Action::CellTable);c.ids=ids;c.image=image;c.text=detail::export_path(path);c.flag=current;return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::analyze_raw(std::uint64_t image,RawRegionRequest region,std::uint64_t object,bool nucleus,bool retain) {
    Command c(Action::RawAnalyze);c.image=image;c.object=object;c.raw_region=region;c.nucleus=nucleus;c.flag=retain;return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::analyze_raw_grid(std::uint64_t image,std::uint64_t object,RawGridSampling sampling,ScientificRawOptions options,bool nucleus,bool retain) {
    sampling.validate();options.validate();
    if(!object) throw std::invalid_argument("raw grid measurement requires an area object ID");
    Command c(Action::RawAnalyze);c.image=image;c.object=object;c.raw_grid=sampling;c.raw_grid_options=options;c.nucleus=nucleus;c.flag=retain;
    return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::verify_raw_analysis(std::uint64_t image,std::uint64_t record) {
    if(!record) throw std::invalid_argument("raw verification requires a record ID");Command c(Action::RawVerify);c.image=image;c.after=record;return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::export_raw_measurements(std::uint64_t image,const std::string& path,bool current,const std::vector<std::uint64_t>& ids) {
    if(ids.size()>10000) throw std::invalid_argument("raw selection exceeds 10000 records");Command c(Action::RawTable);c.image=image;c.ids=ids;c.text=detail::export_path(path);c.flag=current;return impl_->submit(std::move(c));
}
std::uint64_t ViewerSession::save(const std::string& path) {Command c(Action::Save);c.text=detail::export_path(path);return impl_->submit(std::move(c));}
std::uint64_t ViewerSession::close(bool discard) {Command c(Action::Close);c.flag=discard;return impl_->submit(std::move(c));}
namespace detail {
struct ViewerOwner::Impl {
    using Ticket=ViewerSession::Impl::Ticket;
    struct Work {
        std::shared_ptr<Ticket> ticket;std::unique_ptr<Command> command;ViewerEvent seed;Clock::time_point started;std::uint64_t cycle=0;
        std::optional<AnalysisRequest> analysis;std::optional<TissueRequest> tissue;std::optional<CellRequest> cells;
        std::optional<RawAnalysisRequest> raw;std::optional<RawTableRequest> raw_table;
        std::optional<MeasurementTableRequest> table;std::optional<CellTableRequest> cell_table;
        std::shared_ptr<ImageServer> reader,replacement;std::future<ViewerEvent> future;std::size_t region_limit=0;
        std::shared_ptr<const char> tma_identity;
    };
    std::shared_ptr<ViewerSession::Impl> state;std::unique_ptr<Work> work;bool ended=false;
    std::uint64_t cycle=0;
    explicit Impl(ViewerSession& session):state(session.impl_) {std::lock_guard<std::mutex> lock(state->mutex);if(state->stats.state!="created") throw std::runtime_error("viewer session can only be run once");state->stats.state="opening";}
    void reserve(const std::shared_ptr<Ticket>& t,const ViewerEvent& e) {const auto bytes=payload(e);std::lock_guard<std::mutex> lock(state->mutex);if(bytes>state->limits.result_bytes-(state->stats.retained_bytes-t->retained)) throw std::length_error("viewer result budget exceeded; consume completed tickets or disable retained results");state->stats.retained_bytes=state->stats.retained_bytes-t->retained+bytes;t->retained=bytes;}
    void complete(const std::shared_ptr<Ticket>& t,ViewerEvent e) {
        e.bytes=payload(e);reserve(t,e);std::lock_guard<std::mutex> lock(state->mutex);t->event=std::move(e);++state->stats.completed;state->changed.notify_all();
    }
    void failure(const std::shared_ptr<Ticket>& t,const std::string& message,const std::string& status="failed",bool committed=false,std::uint64_t record=0) {
        ViewerEvent e;{std::lock_guard<std::mutex> lock(state->mutex);e.ticket=t->event.ticket;e.epoch=t->event.epoch;e.image_id=t->event.image_id;e.object_id=t->event.object_id;e.action=t->event.action;}
        e.state=status;e.error=message;e.committed=committed;e.record_id=record;complete(t,std::move(e));
    }
    void update(const ViewerContext& c) {
        if(c.epoch==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("viewer workspace epoch exhausted");
        std::lock_guard<std::mutex> lock(state->mutex);state->stats.epoch=c.epoch+1;state->stats.frame=c.frame;state->stats.active_image=c.active_image;state->stats.project_identity=c.opening?std::string{}:c.project->identity();
        state->stats.state=c.opening?"opening":"running";state->stats.active_ticket=work?work->ticket->event.ticket:0;
        if(c.active_image) {const auto d=c.project->document(c.active_image);state->stats.document_revision=d->revision();state->stats.objects=d->object_count();}else {state->stats.document_revision=0;state->stats.objects=0;}
    }
    void poll(const ViewerContext& c) {
        if(!work) return;auto& w=*work;auto& command=*w.command;const bool stale=w.seed.epoch!=c.epoch+1;
        if(command.action==Action::Save) {
            if(w.ticket->cancelled.load() && c.cancel_save) c.cancel_save();const auto stats=c.save_stats();
            if(stats.state=="saving") return;auto e=w.seed;e.path=stats.path;e.committed=stats.state=="saved" || stats.state=="detached";e.state=e.committed?(stats.applied && !stale?"succeeded":"detached"):stats.state=="cancelled"?"cancelled":"failed";e.error=stats.error;e.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-w.started).count();complete(w.ticket,std::move(e));work.reset();return;
        }
        if(command.action==Action::Activate) {
            if(stale || w.ticket->cancelled.load()) {failure(w.ticket,stale?"workspace replaced":"activation cancelled",stale?"stale":"cancelled");work.reset();return;}
            if(c.active_image==command.image) {
                // A waiting producer can wake inside complete(). Publish the
                // adopted image snapshot before its successful activation.
                update(c);auto e=w.seed;e.state="succeeded";complete(w.ticket,std::move(e));work.reset();
            }
            else if(!c.blocked && cycle-w.cycle>2) {failure(w.ticket,"image activation failed or was superseded");work.reset();}return;
        }
        if(stale || c.closing) w.ticket->cancelled.store(true);
        if(w.future.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        const bool export_job=command.action==Action::Measurements || command.action==Action::CellTable || command.action==Action::RawTable;
        if(c.blocked && !stale && !c.closing && !w.ticket->cancelled.load() && !export_job) return;
        try {
            auto e=w.future.get();e.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-w.started).count();
            if(!export_job && (stale || c.closing || w.ticket->cancelled.load())) {failure(w.ticket,stale?"workspace replaced":"viewer job cancelled",stale?"stale":"cancelled");work.reset();return;}
            if(export_job) {if(stale && e.committed) e.state="detached";complete(w.ticket,std::move(e));work.reset();return;}
            std::shared_ptr<Document> doc;try {doc=c.project->document(e.image_id);}catch(const std::out_of_range&) {failure(w.ticket,"source image removed","stale");work.reset();return;}bool matches=true;
            if(w.analysis) matches=w.analysis->matches(*doc,e.object_id);if(w.tissue) matches=w.tissue->matches(*doc,e.object_id);if(w.cells) matches=w.cells->matches(*doc,e.object_id);if(w.raw) matches=w.raw->matches(*doc);if(w.reader) matches=doc->server()==w.reader;
            if(command.action==Action::TmaDearray)matches=matches && doc->analysis_identity_.token==w.tma_identity && doc->revision()==w.seed.document_revision;
            if(!matches) {failure(w.ticket,"source changed before publication","stale");work.reset();return;}
            if(e.state=="failed" || e.state=="cancelled") {complete(w.ticket,std::move(e));work.reset();return;}
            auto tissue=e.tissue;auto cells=e.cells;if(!command.flag) {e.tissue.reset();e.cells.reset();}reserve(w.ticket,e);
            if(w.replacement) {doc->adopt_tiff_source(std::move(w.replacement));e.committed=true;if(c.refresh_source) c.refresh_source(e.image_id);}
            if(w.analysis) e.record_id=doc->keep_analysis(*w.analysis,*e.analysis);
            if(w.tissue) e.record_id=doc->keep_tissue(*w.tissue,tissue);
            if(w.cells) e.record_id=doc->keep_cells(*w.cells,cells);
            if(w.raw && command.action==Action::RawVerify) {
                e.record_id=command.after;e.verified=doc->verify_raw_analysis(command.after,*w.raw,e.raw_analysis);
                if(!e.verified) {e.state="failed";e.error="raw input or statistics differ from archived capture";}
            }else if(w.raw && command.flag) e.record_id=doc->keep_raw_analysis(*w.raw,e.raw_analysis);
            e.committed=e.committed || (e.record_id!=0 && command.action!=Action::RawVerify);e.document_revision=doc->revision();complete(w.ticket,std::move(e));
        }catch(const std::exception& e) {failure(w.ticket,error_text(e),w.ticket->cancelled.load()?"cancelled":"failed");}work.reset();
    }
    void start(const ViewerContext& c,const std::shared_ptr<Ticket>& ticket,std::unique_ptr<Command> command) {
        auto e=ticket->event;if(e.epoch!=c.epoch+1) {failure(ticket,"workspace replaced before command execution","stale");return;}
        if(ticket->cancelled.load()) {failure(ticket,"viewer command cancelled","cancelled");return;}
        const auto started=Clock::now();std::shared_ptr<Document> doc;
        auto done=[&] {e.state="succeeded";e.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-started).count();if(doc) e.document_revision=doc->revision();complete(ticket,std::move(e));};
        if(command->action==Action::Catalog) {
            if(c.catalog) {const auto begin=std::upper_bound(c.catalog->begin(),c.catalog->end(),command->after,[](auto id,const auto& image){return id<image.id;});const auto count=std::min<std::size_t>(command->limit,c.catalog->end()-begin);e.images.assign(begin,begin+count);}done();return;
        }
        if(command->action==Action::Close) {if(work) {work->ticket->cancelled.store(true);if(work->command->action==Action::Save && c.cancel_save) c.cancel_save();}c.close(command->flag);done();return;}
        const bool project_action=command->action==Action::Save || ((command->action==Action::Measurements || command->action==Action::CellTable) && !command->image);
        if(!project_action) {e.image_id=command->image?command->image:c.active_image;if(!e.image_id) throw std::invalid_argument("viewer has no active image");doc=c.project->document(e.image_id);e.document_revision=doc->revision();}
        if(command->action==Action::Inspect) {e.metadata=doc->metadata();if(command->object) e.object=doc->annotation(command->object);done();return;}
        if(command->action==Action::Execute) {
            if(command->revision && *command->revision!=doc->revision()) {failure(ticket,"document revision changed","stale");return;}
            e.object_ids.reserve(command->edits.size());reserve(ticket,e);const auto ids=doc->execute(command->edits,command->text);e.object_ids.assign(ids.begin(),ids.end());e.committed=true;done();return;
        }
        if(command->action==Action::TmaAxis) {
            if(command->revision && *command->revision!=doc->revision()) {failure(ticket,"document revision changed","stale");return;}
            const auto& axis=*command->tma_axis;
            auto edits=axis.remove?doc->tma_remove_axis_edits(axis.column,axis.index,state->limits.max_edits):
                doc->tma_insert_axis_edits(axis.column,axis.index,command->text,axis.bounds,axis.missing,state->limits.max_edits);
            Command captured(Action::Execute);captured.edits=std::move(edits);
            if(captured.bytes()>state->limits.command_bytes) throw std::length_error("TMA axis edits exceed viewer command budget");
            const auto created=std::count_if(captured.edits.begin(),captured.edits.end(),[](const auto& edit){return edit.kind==ObjectEditKind::Create;});
            e.object_ids.reserve(std::size_t(created));reserve(ticket,e);
            if(ticket->cancelled.load()) {failure(ticket,"viewer command cancelled","cancelled");return;}
            const auto ids=doc->execute(captured.edits,axis.remove?(axis.column?"Remove TMA column":"Remove TMA row"):(axis.column?"Insert TMA column":"Insert TMA row"));
            e.object_ids.assign(ids.begin(),ids.end());e.committed=true;done();return;
        }
        if(command->action==Action::Select) {if(e.image_id!=c.active_image) throw std::invalid_argument("activate the target image before selecting objects");c.select(command->ids);done();return;}
        if(work) throw std::runtime_error("a viewer script job is already running; wait for its ticket");
        auto pending=std::make_unique<Work>();pending->ticket=ticket;pending->command=std::move(command);pending->seed=e;pending->started=started;pending->cycle=cycle;auto& cmd=*pending->command;
        if(cmd.action==Action::Save) {if(!c.save(cmd.text)) throw std::runtime_error("a GUI save is already running");work=std::move(pending);return;}
        if(cmd.action==Action::Activate) {c.activate(e.image_id);work=std::move(pending);return;}
        if(cmd.action==Action::TmaDearray) {
            if(cmd.revision && *cmd.revision!=doc->revision()) {failure(ticket,"document revision changed","stale");return;}
            if(doc->tma_core_count())throw std::invalid_argument("Clear the existing TMA grid before detecting another grid");
            cmd.tma_dearray=tma_dearray_options_for_document(*doc,*cmd.tma_dearray);
            pending->reader=doc->server();pending->tma_identity=doc->analysis_identity_.token;
        }
        else if(cmd.action==Action::Region) {validate_region(doc->metadata(),cmd.region);pending->reader=doc->server();}
        else if(cmd.action==Action::RawRegion || cmd.action==Action::RawMetadata || cmd.action==Action::TiffView) {
            pending->reader=doc->server();pending->region_limit=state->limits.region_bytes;
            if(cmd.action==Action::TiffView) {const auto previous=pending->reader->tiff_view();if(!previous || previous->series!=cmd.tiff_view.series) throw std::invalid_argument("viewer TIFF changes require the same series; open other series as separate images");}
        }
        else if(cmd.action==Action::RawAnalyze) pending->raw.emplace(cmd.raw_grid?
            RawAnalysisRequest::for_grid_roi(*doc,cmd.object,*cmd.raw_grid,cmd.raw_grid_options,cmd.nucleus):
            RawAnalysisRequest(*doc,cmd.raw_region,cmd.object,cmd.nucleus));
        else if(cmd.action==Action::RawVerify) pending->raw.emplace(RawAnalysisRequest::replay(*doc,cmd.after));
        else if(cmd.action==Action::RawTable) {
            pending->region_limit=state->limits.result_bytes;std::vector<std::string> paths{state->path};
            if(!c.project->path().empty()) paths.push_back(c.project->path());if(!c.project->recovery_source().empty()) paths.push_back(c.project->recovery_source());
            pending->raw_table.emplace(*doc,cmd.ids,cmd.flag,state->limits.snapshot_bytes,paths);
        }
        else if(cmd.action==Action::Analyze) pending->analysis.emplace(*doc,e.object_id,*cmd.transform,cmd.analysis,cmd.flag,256,cmd.sampling_policy);
        else if(cmd.action==Action::Tissue) pending->tissue.emplace(*doc,e.object_id,*cmd.transform,cmd.tissue);
        else if(cmd.action==Action::Cells) pending->cells.emplace(cmd.cell_grid?CellRequest::for_grid_roi(*doc,e.object_id,*cmd.transform,*cmd.cell_grid,cmd.grid_cells):
            CellRequest::with_nucleus_area_limits(*doc,e.object_id,*cmd.transform,cmd.cell_min_area,cmd.cell_max_area,cmd.cells));
        else if(cmd.action==Action::Measurements) {MeasurementTableOptions o;o.byte_limit=state->limits.result_bytes;o.snapshot_budget=state->limits.snapshot_bytes;o.require_current=cmd.flag;if(doc) pending->table.emplace(*doc,cmd.table,cmd.ids,o);else pending->table.emplace(*c.project,cmd.table,std::vector<std::uint64_t>{},o);}
        else if(cmd.action==Action::CellTable) {CellTableOptions o;o.byte_limit=state->limits.result_bytes;o.snapshot_budget=state->limits.snapshot_bytes;o.require_current=cmd.flag;if(doc) pending->cell_table.emplace(*doc,cmd.ids,o);else pending->cell_table.emplace(*c.project,std::vector<std::uint64_t>{},o);}
        if(pending->raw) pending->seed.object_id=pending->raw->object_id();
        auto* borrowed=pending.get();pending->future=std::async(std::launch::async,[borrowed] {
            auto& w=*borrowed;auto e=w.seed;const auto* cancel=&w.ticket->cancelled;
            try {
                export_checkpoint(cancel);
                if(w.reader) {
                    const auto action=w.command->action;
                    if(action==Action::TmaDearray) {e.tma_dearray=qpx::detect_tma_grid(*w.reader,*w.command->tma_dearray,cancel);e.components=e.tma_dearray->candidates;e.rows=e.tma_dearray->grid.rows;}
                    else if(action==Action::TiffView) {w.replacement=w.reader->with_tiff_view(w.command->tiff_view,cancel);e.raw_metadata=w.replacement->raw_metadata();e.tiff_view=*w.replacement->tiff_view();e.metadata=w.replacement->metadata();}
                    else if(action==Action::RawMetadata || action==Action::RawRegion) {
                        e.raw_metadata=w.reader->raw_metadata(cancel);if(!e.raw_metadata) throw std::invalid_argument("image has no raw quantitative samples");if(auto view=w.reader->tiff_view()) e.tiff_view=*view;
                        if(action==Action::RawRegion) {const auto& r=w.command->raw_region;r.validate(*e.raw_metadata);const auto bytes=std::uint64_t(r.region.width)*r.region.height*(std::uint64_t(r.channels)*sample_bytes(e.raw_metadata->sample_type)+1);if(bytes>w.region_limit) throw std::length_error("viewer raw region exceeds byte limit");e.raw_pixels=std::make_shared<const RawPixelBuffer>(w.reader->read_raw(r,cancel));}
                    }else e.pixels=std::make_shared<const PixelBuffer>(w.reader->read_region(w.command->region));
                }
                else if(w.raw) {e.raw_analysis=qpx::analyze_raw(*w.raw,cancel);e.samples=e.raw_analysis->statistics().valid_pixels;}
                else if(w.raw_table) {e.bytes=save_raw_analysis_csv(*w.raw_table,w.command->text,false,w.region_limit,cancel);e.path=w.command->text;e.rows=w.raw_table->rows();e.committed=true;}
                else if(w.analysis) {e.analysis=qpx::analyze(*w.analysis,cancel);e.samples=e.analysis->samples;}
                else if(w.tissue) {e.tissue=detect_tissue(*w.tissue,cancel);e.samples=e.tissue->samples();e.components=e.tissue->components().size();}
                else if(w.cells) {e.cells=detect_cells(*w.cells,cancel);e.samples=e.cells->nuclei()->samples();e.components=e.cells->features().size();}
                else if(w.table) {save_measurements_csv(*w.table,w.command->text,cancel);e.path=w.command->text;e.rows=w.table->row_count();e.committed=true;}
                else if(w.cell_table) {save_cell_measurements_csv(*w.cell_table,w.command->text,cancel);e.path=w.command->text;e.rows=w.cell_table->row_count();e.committed=true;}
                e.state="succeeded";
            }catch(const std::exception& error) {e.state=cancel->load()?"cancelled":"failed";e.error=error_text(error);}return e;
        });work=std::move(pending);
    }
};
ViewerOwner::ViewerOwner(ViewerSession& session):impl_(std::make_unique<Impl>(session)) {}
ViewerOwner::~ViewerOwner() {try {fail("viewer ended before completion was observed; inspect any authorized output files");}catch(...) {}}
void ViewerOwner::poll(const ViewerContext& context) {if(impl_->ended) return;impl_->poll(context);impl_->update(context);}
void ViewerOwner::pump(const ViewerContext& context) {
    if(impl_->ended) return;
    if(impl_->cycle==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("viewer owner cycle exhausted");
    ++impl_->cycle;
    poll(context);if(impl_->ended || context.closing) return;std::shared_ptr<Impl::Ticket> ticket;std::unique_ptr<Command> command;
    {std::lock_guard<std::mutex> lock(impl_->state->mutex);if(impl_->state->queue.empty()) return;auto position=impl_->state->queue.begin();if(context.blocked) {position=std::find_if(position,impl_->state->queue.end(),[&](auto id){return impl_->state->find(id)->command->action==Action::Close;});if(position==impl_->state->queue.end()) return;}const auto id=*position;impl_->state->queue.erase(position);ticket=impl_->state->find(id);impl_->state->stats.queued_bytes-=ticket->queued;ticket->queued=0;command=std::move(ticket->command);ticket->event.state="running";impl_->state->stats.queued=impl_->state->queue.size();}
    const auto start=Clock::now();try {impl_->start(context,ticket,std::move(command));}catch(const std::exception& e) {impl_->failure(ticket,error_text(e));}
    const auto ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();std::lock_guard<std::mutex> lock(impl_->state->mutex);impl_->state->stats.max_command_ms=std::max(impl_->state->stats.max_command_ms,ms);
}
void ViewerOwner::finish(const ViewerContext& context) {
    if(impl_->ended) return;
    if(impl_->work) {auto& w=*impl_->work;w.ticket->cancelled.store(true);if(w.command->action==Action::Save) {if(context.cancel_save) context.cancel_save();impl_->poll(context);}else if(w.future.valid()) {w.future.wait();auto closing=context;closing.closing=true;impl_->poll(closing);}if(impl_->work) {impl_->failure(w.ticket,"viewer closed before completion","cancelled");impl_->work.reset();}}
    fail("");
}
ViewerStats ViewerOwner::stats() const {std::lock_guard<std::mutex> lock(impl_->state->mutex);return impl_->state->stats;}
void ViewerOwner::cancel_active() {if(impl_->work) impl_->work->ticket->cancelled.store(true);}

void ViewerOwner::fail(const std::string& message) {
    std::string error=message;if(!error.empty()) try {bounded_utf8(error,8192);}catch(...) {error="viewer failed (invalid or oversized error text)";}
    if(impl_->ended) return;if(impl_->work) {auto ticket=impl_->work->ticket;ticket->cancelled.store(true);if(impl_->work->future.valid()) impl_->work->future.wait();impl_->failure(ticket,error.empty()?"viewer closed":error,"cancelled");impl_->work.reset();}
    auto& s=*impl_->state;std::vector<std::unique_ptr<Command>> released;
    {std::lock_guard<std::mutex> lock(s.mutex);released.reserve(s.queue.size());
        for(auto id:s.queue) {auto ticket=s.find(id);ticket->queued=0;released.push_back(std::move(ticket->command));ticket->event.state="cancelled";ticket->event.error=error.empty()?"viewer closed before command execution":error;++s.stats.completed;}
        s.queue.clear();s.stats.queued=0;s.stats.queued_bytes=0;s.stats.active_ticket=0;s.stats.state=error.empty()?"closed":"failed";s.stats.error=error;
    }
    impl_->ended=true;s.changed.notify_all();
}
}
}
