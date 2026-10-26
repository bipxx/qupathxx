#include "raw_pixel_tiled_training_internal.hpp"
#include "object_feature_snapshot.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
#include "raw_pixel_features.hpp"
#include "raw_analysis_internal.hpp"
#include "scientific_raw_sampling_internal.hpp"
#include "pixel_training_reservoir.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <map>
#include <set>
#include <thread>

namespace qpx {
namespace {
void stopped(const std::atomic<bool>* flag) {if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("raw tiled training cancelled");}
void owner(std::thread::id id) {if(std::this_thread::get_id()!=id) throw std::runtime_error("raw tiled training controller must run on its owner thread");}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
std::size_t remaining(std::size_t budget,std::size_t fixed) {
    if(fixed>=budget || budget-fixed<1024*1024) throw std::length_error("raw tiled training leaves insufficient window workspace");return budget-fixed;
}
// Bounds choose conservative candidates; exact ROI membership is evaluated at
// each core center. Binary search uses the same rounded center as that predicate.
std::int64_t center_bound(const ScientificGridAxis& axis,double edge,bool upper) {
    std::int64_t first=0,stop=axis.samples();
    while(first<stop) {const auto mid=first+(stop-first)/2;const auto value=axis.center(mid).binary64();if(upper?value<=edge:value<edge) first=mid+1;else stop=mid;}
    return first;
}
using Interval=std::pair<std::int64_t,std::int64_t>;
using Tile=RawPixelTrainingTile;
struct Event {Interval columns;int delta;};
using Events=std::map<std::int64_t,std::vector<Event>>;
template<class Visit> void sweep(const Events& events,Visit visit) {
    std::map<Interval,std::size_t> active;
    for(auto it=events.begin();it!=events.end();++it) {
        for(const auto& event:it->second) {
            if(event.delta>0) ++active[event.columns];
            else {auto found=active.find(event.columns);if(found==active.end()) throw std::logic_error("invalid raw tile interval sweep");if(!--found->second) active.erase(found);}
        }
        const auto next=std::next(it);if(next==events.end() || active.empty()) continue;
        std::optional<Interval> merged;
        for(const auto& entry:active) {
            if(merged && entry.first.first<=merged->second) merged->second=std::max(merged->second,entry.first.second);
            else {if(merged) visit(it->first,next->first,*merged);merged=entry.first;}
        }
        if(merged) visit(it->first,next->first,*merged);
    }
}
}
void RawPixelTileOptions::validate() const {
    if(width<1 || width>4096 || height<1 || height>4096 || !max_windows || max_windows>1000000) throw std::invalid_argument("raw training tiles require 1..4096 axes and 1..1000000 windows");
}
struct RawPixelTiledTrainingRequest::Authority {
    ObjectFeatureRequest objects;
    Authority(const Document& doc,const std::vector<std::uint64_t>& ids,std::size_t budget):objects(doc,ids,[&] {ObjectFeatureOptions o;o.calibrated=false;o.snapshot_bytes=budget;return o;}()) {}
};
void prepare_raw_pixel_tiled_snapshot(RawPixelTiledTrainingRequest::Data& data,std::size_t annotation_bytes,bool admit_features) {
    const auto options=data.options;const auto tiles=data.tiles;options.validate();tiles.validate();const auto limit=std::min<std::size_t>(64u*1024*1024,options.work_bytes);
    if(!data.annotations || data.annotations->rows.empty() || data.annotations->rows.size()>4096) throw std::invalid_argument("invalid raw tiled annotation count");
    std::uint64_t previous=0;for(const auto& row:data.annotations->rows) {if(!row.id || row.id<=previous || !row.revision || !data.source.provenance || row.revision>data.source.provenance->document_revision || unsigned(row.kind)>2) throw std::invalid_argument("invalid raw tiled annotation identity/revision");previous=row.id;}
    data.snapshot=sizeof(RawPixelTiledTrainingRequest::Data)+raw_pixel_source_snapshot_bytes(data.source)+annotation_bytes+data.annotations->rows.size()*1024+1024*1024;
    auto charge=[&](std::size_t bytes) {if(data.snapshot>limit || bytes>limit-data.snapshot) throw std::length_error("raw tiled annotation/plan snapshot exceeds budget");data.snapshot+=bytes;};charge(0);
    std::set<std::string> classes;
    for(const auto& row:data.annotations->rows) {detail::bounded_utf8(row.label,256);if(!row.body.is_area() || row.label.empty()) throw std::invalid_argument("raw tiled training requires labelled area geometry");classes.insert(row.label);if(classes.size()>256) throw std::invalid_argument("raw tiled training exceeds 256 classes");}
    if(classes.size()<2 || options.max_samples/classes.size()<2) throw std::invalid_argument("raw tiled training requires two classes and two samples per class");
    charge(classes.size()*512+data.annotations->rows.size()*sizeof(std::uint32_t));data.classes.assign(classes.begin(),classes.end());data.labels.reserve(data.annotations->rows.size());
    Events events;std::vector<SpatialEntry> entries;entries.reserve(data.annotations->rows.size());
    for(std::size_t i=0;i<data.annotations->rows.size();++i) {
        const auto& row=data.annotations->rows[i];data.labels.push_back(std::uint32_t(std::lower_bound(data.classes.begin(),data.classes.end(),row.label)-data.classes.begin()));
        const auto bounds=row.body.bounds();entries.push_back({i+1,bounds});
        const auto x0=center_bound(data.lattice.x(),bounds.x,false),x1=center_bound(data.lattice.x(),bounds.x+bounds.width,true);
        const auto y0=center_bound(data.lattice.y(),bounds.y,false),y1=center_bound(data.lattice.y(),bounds.y+bounds.height,true);
        if(x0==x1 || y0==y1) continue;
        const Interval columns{x0/tiles.width,(x1-1)/tiles.width+1};events[y0/tiles.height].push_back({columns,1});events[(y1-1)/tiles.height+1].push_back({columns,-1});
    }
    std::size_t windows=0;
    sweep(events,[&](std::int64_t first,std::int64_t stop,Interval cols) {
        const auto count=std::uint64_t(stop-first)*std::uint64_t(cols.second-cols.first);
        if(count>tiles.max_windows-windows) throw std::length_error("raw tiled training exceeds window limit");windows+=std::size_t(count);
        const auto width=std::min(cols.second*tiles.width,data.lattice.x().samples())-cols.first*tiles.width;
        const auto height=std::min(stop*tiles.height,data.lattice.y().samples())-first*tiles.height;
        const auto cells=std::uint64_t(width)*std::uint64_t(height);
        if(cells>options.max_scanned_samples-data.scanned) throw std::length_error("raw tiled training exceeds candidate scan limit");data.scanned+=cells;
    });
    if(!windows) throw std::invalid_argument("raw training annotations cover no global sample centers");
    charge(windows*sizeof(Tile));data.plan.reserve(windows);
    sweep(events,[&](std::int64_t first,std::int64_t stop,Interval cols) {for(auto y=first;y<stop;++y) for(auto x=cols.first;x<cols.second;++x) data.plan.push_back({x*tiles.width,y*tiles.height});});
    // Sweep emits disjoint bands/ranges; sort makes traversal independent of
    // interval decomposition while preserving unique core ownership.
    std::sort(data.plan.begin(),data.plan.end(),[](const Tile& a,const Tile& b) {return a.y==b.y?a.x<b.x:a.y<b.y;});
    data.index=std::make_shared<const SpatialIndex>(std::move(entries));
    data.capacity=std::min(options.max_samples_per_class,options.max_samples/data.classes.size());
    data.fixed=data.snapshot+data.classes.size()*data.capacity*(8*data.recipe.feature_schema().size()+256)+262144;
    data.features=data.recipe.features();
    if(admit_features) {data.features.work_bytes=std::min(data.features.work_bytes,remaining(options.work_bytes,data.fixed));auto admitted=data.recipe.selection();admitted.work_bytes=std::min(admitted.work_bytes,remaining(options.work_bytes,data.fixed));detail::raw_feature_work_bytes(data.source.metadata,data.window(data.plan.front()),admitted,data.features);}
}
RawPixelTiledTrainingRequest::RawPixelTiledTrainingRequest(const Document& doc,const std::vector<std::uint64_t>& ids,
    ScientificRawLattice lattice,ScientificRawOptions selection,RawPixelFeatureOptions features,RawPixelTrainingOptions options,RawPixelTileOptions tiles,bool physical):
    source_(doc.server()),identity_(doc.analysis_identity_.token),revision_(doc.revision()) {
    options.validate();tiles.validate();if(ids.empty() || ids.size()>4096) throw std::invalid_argument("raw tiled training requires 1..4096 labelled area IDs");
    auto selected=ids;std::sort(selected.begin(),selected.end());if(std::adjacent_find(selected.begin(),selected.end())!=selected.end()) throw std::invalid_argument("duplicate raw tiled training annotation ID");
    const auto limit=std::min<std::size_t>(64u*1024*1024,options.work_bytes);
    auto source=raw_pixel_capture_lattice_source(doc,lattice,limit);
    auto metadata=source.metadata;metadata.pixel_width_um=source.calibration.x;metadata.pixel_height_um=source.calibration.y;
    RawPixelRecipe recipe(metadata,lattice,selection,std::move(features),physical);
    const auto capture_fixed=sizeof(Data)+raw_pixel_source_snapshot_bytes(source)+selected.size()*1024+1024*1024;
    if(capture_fixed>=limit || limit-capture_fixed<1024*1024) throw std::length_error("raw tiled source leaves insufficient annotation capture budget");
    auto authority=std::make_shared<Authority>(doc,selected,limit-capture_fixed);
    auto data=std::make_shared<Data>(std::move(lattice),std::move(recipe),std::move(source),options,tiles);
    data->annotations=authority->objects.data_;
    prepare_raw_pixel_tiled_snapshot(*data,authority->objects.snapshot_bytes(),true);
    authority_=std::move(authority);data_=std::move(data);if(!matches(doc)) throw std::runtime_error("document changed during raw tiled training capture");
}
bool RawPixelTiledTrainingRequest::matches(const Document& doc) const {return identity_==doc.analysis_identity_.token && revision_==doc.revision() && source_==doc.server() && authority_->objects.matches(doc) && raw_pixel_source_matches(*source_,data_->source);}
bool RawPixelTiledTrainingRequest::publication_matches(const Document& doc) const {return matches(doc) && raw_pixel_publication_lattice_source_matches(*source_,data_->source,data_->lattice);}
const ScientificRawLattice& RawPixelTiledTrainingRequest::lattice() const {return data_->lattice;}
const RawPixelRecipe& RawPixelTiledTrainingRequest::recipe() const {return data_->recipe;}
const RawPixelTrainingOptions& RawPixelTiledTrainingRequest::options() const {return data_->options;}
const RawPixelTileOptions& RawPixelTiledTrainingRequest::tile_options() const {return data_->tiles;}
std::size_t RawPixelTiledTrainingRequest::window_count() const {return data_->plan.size();}
std::uint64_t RawPixelTiledTrainingRequest::scanned_samples() const {return data_->scanned;}
std::size_t RawPixelTiledTrainingRequest::snapshot_bytes() const {return data_->snapshot;}
std::size_t RawPixelTiledTrainingRequest::estimated_work_bytes() const {return data_->options.work_bytes;}
const ScientificRawLattice& RawPixelTiledTrainingTable::lattice() const {return snapshot_->lattice;}
const RawPixelRecipe& RawPixelTiledTrainingTable::recipe() const {return snapshot_->recipe;}
const RawPixelTrainingOptions& RawPixelTiledTrainingTable::options() const {return snapshot_->options;}
const RawPixelTileOptions& RawPixelTiledTrainingTable::tile_options() const {return snapshot_->tiles;}
const std::vector<std::string>& RawPixelTiledTrainingTable::classes() const {return snapshot_->classes;}
std::size_t RawPixelTiledTrainingTable::estimated_work_bytes() const {return snapshot_->options.work_bytes;}
std::size_t RawPixelTiledTrainingTable::retained_bytes() const {return snapshot_->snapshot+sizeof(*this)+values_.capacity()*sizeof(float)+labels_.capacity()*sizeof(std::uint32_t)+coordinates_.capacity()*sizeof(std::int64_t)+(ids_.capacity()+eligible_.capacity()+counts_.capacity())*sizeof(std::uint64_t)+input_sha_.capacity()+sha_.capacity();}

std::string raw_pixel_tiled_training_sha256(const RawPixelTiledTrainingTable& table,const std::atomic<bool>* cancelled) {
    Sha256 result;text(result,"qpx.raw-tiled-pixel-training/1");text(result,table.input_sha256());const auto schema=table.recipe().feature_schema();for(const auto& f:schema) {text(result,f.name);text(result,f.unit);}for(const auto& name:table.classes()) text(result,name);result.integer(table.row_count());
    for(std::size_t row=0;row<table.row_count();++row) {stopped(cancelled);result.integer(table.labels()[row]);result.integer(table.annotation_ids()[row]);result.integer(table.coordinates()[2*row]);result.integer(table.coordinates()[2*row+1]);for(std::size_t col=0;col<schema.size();++col) {std::uint32_t bits;std::memcpy(&bits,&table.values()[row*schema.size()+col],4);result.integer(bits);}}return result.finish();
}
namespace {
void begin_training_input(Sha256& hash,const RawPixelTiledTrainingRequest::Data& d) {
    text(hash,"qpx.raw-tiled-annotation-input/1");const auto recipe_bytes=d.recipe.bytes();hash.integer(recipe_bytes.size());hash.update(recipe_bytes.data(),recipe_bytes.size());
        for(auto v:{d.lattice.image_width(),d.lattice.image_height(),d.lattice.x().origin(),d.lattice.x().extent(),d.lattice.x().samples(),d.lattice.x().context(),d.lattice.y().origin(),d.lattice.y().extent(),d.lattice.y().samples(),d.lattice.y().context(),d.tiles.width,d.tiles.height}) hash.integer(v);
        hash.integer(d.tiles.max_windows);hash.integer(d.options.seed);hash.integer(d.options.max_samples);hash.integer(d.options.max_samples_per_class);hash.integer(d.options.max_scanned_samples);hash.integer(d.options.balanced);
        for(std::size_t i=0;i<d.annotations->rows.size();++i) {const auto& row=d.annotations->rows[i];hash.integer(row.id);hash.integer(row.revision);hash.integer(static_cast<int>(row.kind));text(hash,row.label);const auto bytes=GeometryArchiveCodec::encode(row.body);hash.integer(bytes.size());hash.update(bytes.data(),bytes.size());}
}
}
std::string raw_pixel_tiled_request_sha256(const RawPixelTiledTrainingRequest::Data& d) {
    Sha256 hash;text(hash,"qpx.raw-tiled-training-checkpoint-request/1");begin_training_input(hash,d);hash.integer(d.options.work_bytes);hash.integer(d.recipe.selection().work_bytes);hash.integer(d.recipe.features().work_bytes);
    text(hash,d.source.reader);text(hash,d.source.epoch);
    // Calibration also matters for unfiltered recipes and an empty prefix.
    for(const auto value:{d.source.calibration.x,d.source.calibration.y}) {hash.integer(bool(value));if(value) {std::uint64_t bits;const auto v=*value;std::memcpy(&bits,&v,8);hash.integer(bits);}}
    return hash.finish();
}
std::shared_ptr<const RawPixelTiledTrainingTable> RawPixelTiledTrainingKernel::run(const std::shared_ptr<const RawPixelTiledTrainingRequest::Data>& captured,ImageServer& source,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed,std::atomic<int>* phase,RawPixelTiledTrainingPrefix* prefix) {
        stopped(cancelled);const auto& d=*captured;if(!raw_pixel_source_matches(source,d.source)) throw std::runtime_error("raw tiled training source changed before sampling");
        const auto columns=d.recipe.feature_schema().size();detail::PixelTrainingReservoir reservoir(d.classes.size(),d.capacity,columns,d.options.seed);
        Sha256 input_hash;begin_training_input(input_hash,d);
        const auto target=prefix?std::min<std::uint64_t>(prefix->target,d.plan.size()):d.plan.size();
        if(prefix && (prefix->expected_windows>target || (!prefix->expected_sha.empty() && prefix->expected_windows==0 && prefix->expected_sha!=raw_pixel_tiled_request_sha256(d)))) throw std::invalid_argument("raw tiled checkpoint empty prefix mismatch");
        std::string chain;if(prefix) {chain=raw_pixel_tiled_request_sha256(d);prefix->completed=0;prefix->sha=chain;}
        std::uint64_t done=0;
        for(std::size_t tile_index=0;tile_index<target;++tile_index) {
            const auto& tile=d.plan[tile_index];
            stopped(cancelled);const auto window=d.window(tile);auto selection=d.recipe.selection();selection.work_bytes=std::min(selection.work_bytes,d.options.work_bytes-d.fixed);
            // Admit this window before its first read; do not reserve full-slide
            // feature planes or claim later windows have already been admitted.
            detail::raw_feature_work_bytes(d.source.metadata,window,selection,d.features);
            if(phase) phase->store(1);detail::ScientificRawReserve reserve;reserve.expected_series=d.recipe.series();
            const auto input=detail::read_scientific_raw_window_reserved(source,window,selection,reserve,cancelled);raw_pixel_verify_lattice_sample(source,d.source,d.lattice,input);
            if(phase) phase->store(2);const auto features=detail::extract_raw_pixel_features(input,d.features,cancelled);
            std::optional<Sha256> window_hash;if(prefix) window_hash.emplace();
            const auto feature_hash=raw_grid_analysis_hash(features->input_sha256,d.source.calibration.x,d.source.calibration.y,std::nullopt);
            auto geometry=[&](Sha256& h) {h.integer(tile.x);h.integer(tile.y);h.integer(window.core_width());h.integer(window.core_height());text(h,feature_hash);};
            geometry(input_hash);if(prefix) geometry(*window_hash);
            for(std::int64_t y=0;y<window.core_height();++y) {stopped(cancelled);for(std::int64_t x=0;x<window.core_width();++x) {
                if((x&4095)==0) stopped(cancelled);const auto index=std::size_t(y*window.core_width()+x);const auto gx=tile.x+x,gy=tile.y+y;std::uint32_t label=0;std::uint64_t id=0;
                if(features->rows_valid[index]) {
                    const Point point{d.lattice.x().center(gx).binary64(),d.lattice.y().center(gy).binary64()};
                    for(const auto candidate:d.index->query({point.x,point.y,0,0})) {const auto rowindex=std::size_t(candidate-1);const auto& row=d.annotations->rows[rowindex];if(row.body.contains(point)) {if(label && label!=d.labels[rowindex]+1) {label=UINT32_MAX;id=0;break;}label=d.labels[rowindex]+1;if(!id || row.id<id) id=row.id;}}
                }
                input_hash.integer(label);input_hash.integer(id);if(prefix) {window_hash->integer(label);window_hash->integer(id);}reservoir.observe(label,id,gx,gy,features->values.data()+index*columns);++done;
            }if(completed) completed->store(done,std::memory_order_relaxed);}
            raw_pixel_verify_lattice_sample(source,d.source,d.lattice,input);
            if(prefix) {Sha256 h;text(h,"qpx.raw-tiled-training-prefix/1");text(h,chain);h.integer(tile_index);text(h,window_hash->finish());chain=h.finish();prefix->completed=tile_index+1;prefix->sha=chain;
                if(prefix->completed==prefix->expected_windows && !prefix->expected_sha.empty() && chain!=prefix->expected_sha) throw std::invalid_argument("raw tiled checkpoint prefix input changed");}
        }
        stopped(cancelled);if(target<d.plan.size()) return {};
        auto selected=reservoir.finish(d.options.balanced,cancelled);auto out=std::shared_ptr<RawPixelTiledTrainingTable>(new RawPixelTiledTrainingTable());out->snapshot_=captured;
        out->values_=std::move(selected.values);out->labels_=std::move(selected.labels);out->coordinates_=std::move(selected.coordinates);out->ids_=std::move(selected.ids);out->eligible_=std::move(selected.eligible);out->counts_=std::move(selected.counts);out->ambiguous_=selected.ambiguous;out->input_sha_=input_hash.finish();
        out->sha_=raw_pixel_tiled_training_sha256(*out,cancelled);stopped(cancelled);return out;
    }
std::shared_ptr<const RawPixelTiledTrainingTable> RawPixelTiledTrainingKernel::run(const RawPixelTiledTrainingRequest& request,const std::atomic<bool>* cancelled) {return run(request.data_,*request.source_,cancelled);}
std::shared_ptr<const RawPixelTiledTrainingTable> sample_raw_pixel_tiled_training(const RawPixelTiledTrainingRequest& request,const std::atomic<bool>* cancelled) {return RawPixelTiledTrainingKernel::run(request,cancelled);}
RawPixelClassifier train_raw_pixel_tiled_classifier(const RawPixelTiledTrainingTable& table,ForestParameters parameters,const std::atomic<bool>* cancelled) {
    stopped(cancelled);parameters.work_bytes=std::min<std::uint64_t>(parameters.work_bytes,remaining(table.options().work_bytes,table.retained_bytes()));
    auto forest=train_forest(table.values(),table.labels(),table.recipe().feature_schema(),table.classes(),parameters);stopped(cancelled);return RawPixelClassifier(forest,table.recipe());
}
struct RawPixelTiledTrainingJob::Impl {
    struct Completion {std::shared_ptr<const RawPixelTiledTrainingTable> table;std::shared_ptr<const RawPixelClassifier> model;};
    const std::thread::id owner=std::this_thread::get_id();std::optional<RawPixelTiledTrainingRequest> request;
    std::future<Completion> future;std::shared_ptr<const RawPixelTiledTrainingTable> table;std::shared_ptr<const RawPixelClassifier> model;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    bool stale=false;RawPixelJobStats stats;std::chrono::steady_clock::time_point started;
};
RawPixelTiledTrainingJob::RawPixelTiledTrainingJob():impl_(std::make_unique<Impl>()) {}
RawPixelTiledTrainingJob::~RawPixelTiledTrainingJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelTiledTrainingJob::busy() const {owner(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawPixelTiledTrainingJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawPixelTiledTrainingJob::await_completion() {owner(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelTiledTrainingJob::sample(const RawPixelTiledTrainingRequest& request) {return start(request,std::nullopt);}
bool RawPixelTiledTrainingJob::train(const RawPixelTiledTrainingRequest& request,ForestParameters parameters) {owner(impl_->owner);parameters.validate();if(!has_classifier_training()) throw std::runtime_error("raw tiled training requires the optional OpenCV backend");return start(request,parameters);}
bool RawPixelTiledTrainingJob::start(const RawPixelTiledTrainingRequest& request,std::optional<ForestParameters> parameters) {
    owner(impl_->owner);auto& s=*impl_;if(busy()) return false;if(s.stats.generation==UINT64_MAX) throw std::overflow_error("raw tiled training generation exhausted");
    auto captured=request;const auto old_cancel=s.cancelled.load();const auto old_completed=s.completed.load();const auto old_phase=s.phase.load();s.cancelled.store(false);s.completed.store(0);s.phase.store(0);
    std::future<Impl::Completion> future;
    try {future=std::async(std::launch::async,[state=&s,data=captured.data_,source=captured.source_.get(),parameters] {Impl::Completion out;out.table=RawPixelTiledTrainingKernel::run(data,*source,&state->cancelled,&state->completed,&state->phase);if(parameters) {state->phase.store(3);out.model=std::make_shared<const RawPixelClassifier>(train_raw_pixel_tiled_classifier(*out.table,*parameters,&state->cancelled));}return out;});}
    catch(...) {s.cancelled.store(old_cancel);s.completed.store(old_completed);s.phase.store(old_phase);throw;}
    s.future=std::move(future);s.request=std::move(captured);s.table.reset();s.model.reset();s.stale=false;s.started=std::chrono::steady_clock::now();const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.total_samples=request.scanned_samples();return true;
}
RawPixelJobStats RawPixelTiledTrainingJob::poll(const Document& doc) {
    owner(impl_->owner);auto& s=*impl_;
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.table.reset();s.model.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto completion=s.future.get();if(s.stale || (!s.cancelled.load() && !s.request->publication_matches(doc))) {s.stats.state="stale";s.stale=true;}
            else if(s.cancelled.load()) s.stats.state="cancelled";else {s.table=std::move(completion.table);s.model=std::move(completion.model);s.stats.state="ready";}}
        catch(const std::exception& error) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";try {s.stats.error=error.what();detail::bounded_utf8(s.stats.error,8192);}catch(const std::invalid_argument&) {s.stats.error="Raw tiled training failed";}s.table.reset();s.model.reset();}
        s.stats.phase="finished";s.stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-s.started).count();
    }
    if(!s.future.valid() && s.cancelled.load() && !s.stale && s.stats.state=="ready") s.stats.state="cancelled";
    if(s.stats.state!="ready" && !s.future.valid()) {s.table.reset();s.model.reset();s.request.reset();}
    s.stats.completed_samples=s.completed.load();if(s.future.valid()) {const auto p=s.phase.load();s.stats.phase=p==1?"reading":p==2?"sampling":p==3?"training":"starting";}return s.stats;
}
RawPixelJobStats RawPixelTiledTrainingJob::wait(const Document& doc) {owner(impl_->owner);poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const RawPixelTiledTrainingTable> RawPixelTiledTrainingJob::table() const {owner(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->table;}
std::shared_ptr<const RawPixelClassifier> RawPixelTiledTrainingJob::model() const {owner(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->model;}
bool RawPixelTiledTrainingJob::verify_result(const Document& doc,std::uint64_t id) {if(poll(doc).state!="ready" || !impl_->table || !impl_->request) throw std::runtime_error("raw tiled verification table is not ready/current");return doc.verify_raw_pixel_tiled_training(id,*impl_->request,impl_->table,impl_->model);}
}
