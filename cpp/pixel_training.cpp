#include "pixel_workflow_snapshot.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
void stop(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("pixel training cancelled");}
double elapsed(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void text(Sha256& h,const std::string& s) {h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double x) {std::uint64_t bits;std::memcpy(&bits,&x,8);h.integer(bits);}
void resolution_hash(Sha256& h,const PixelResolution& r) {real(h,r.downsample);h.integer(r.physical());if(r.physical()) {real(h,*r.pixel_width_um);real(h,*r.pixel_height_um);}}
void metadata_check(const ImageMetadata& m) {
    if(m.width<=0 || m.height<=0 || m.levels.empty() || m.levels.size()>1024 || m.pixel_type!=PixelType::UInt8 || m.layout!=PixelLayout::RGBA) throw std::invalid_argument("invalid pixel training source metadata");
    for(std::size_t i=0;i<m.levels.size();++i) {const auto& l=m.levels[i];if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1 || (!i && (l.width!=m.width || l.height!=m.height || l.downsample!=1))) throw std::invalid_argument("invalid pixel training pyramid");}
}
std::int64_t edge(double x,double scale,std::int64_t limit) {const auto v=std::ceil(static_cast<long double>(x)/scale-.5L);if(v<=0) return 0;if(v>=limit) return limit;return static_cast<std::int64_t>(v);}
std::int64_t origin(std::int64_t x,double scale) {const auto v=std::round(static_cast<long double>(x)*scale);if(!std::isfinite(v) || v<0 || v>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("pixel training read origin overflow");return static_cast<std::int64_t>(v);}
std::uint64_t mix(std::uint64_t x) {x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
struct Sample {std::uint64_t priority,id;std::int64_t x,y;std::vector<float> values;};
bool less_sample(const Sample& a,const Sample& b) {if(a.priority!=b.priority) return a.priority<b.priority;if(a.y!=b.y) return a.y<b.y;return a.x<b.x;}
bool close(double a,double b) {return std::isfinite(a) && std::isfinite(b) && std::abs(a-b)<=1e-9*std::max(std::abs(a),std::abs(b));}
}
ForestParameters pixel_forest_parameters() {ForestParameters p;p.trees=32;p.max_depth=8;return p;}
void PixelTrainingOptions::validate() const {
    features.validate();
    if(level<0 || tile_size<16 || tile_size>512 || max_samples<4 || max_samples>100000 || max_samples_per_class<2 || max_samples_per_class>50000 || !seed || !max_scanned_pixels || max_scanned_pixels>100000000 || work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("invalid annotation pixel training options");
}
PixelTrainingRequest::PixelTrainingRequest(const Document& doc,const std::vector<std::uint64_t>& ids,const StainTransform& transform,PixelTrainingOptions options):source_(doc.server()) {
    const auto start=Clock::now();options.validate();
    if(ids.empty() || ids.size()>4096) throw std::invalid_argument("pixel training requires 1..4096 labelled area IDs");
    if(!source_ || source_->closed()) throw std::invalid_argument("pixel training requires an open source");
    const auto& metadata=source_->metadata();metadata_check(metadata);if(static_cast<std::size_t>(options.level)>=metadata.levels.size()) throw std::out_of_range("invalid pixel training level");
    auto selected=ids;std::sort(selected.begin(),selected.end());if(std::adjacent_find(selected.begin(),selected.end())!=selected.end()) throw std::invalid_argument("duplicate pixel training annotation ID");
    ObjectFeatureOptions guard;guard.calibrated=false;guard.snapshot_bytes=std::min<std::size_t>(options.work_bytes,256u*1024*1024);authority_.emplace(doc,std::vector<std::uint64_t>{selected.front()},guard);
    auto data=std::make_shared<Data>(options,transform);data->revision=doc.revision();data->metadata=metadata;data->provenance=authority_->source_anchor();
    data->resolution.downsample=metadata.levels[options.level].downsample;
    if(options.calibrated && doc.pixel_width_um() && doc.pixel_height_um()) {data->resolution.pixel_width_um=*doc.pixel_width_um()*data->resolution.downsample;data->resolution.pixel_height_um=*doc.pixel_height_um()*data->resolution.downsample;}data->resolution.validate();
    data->bytes=sizeof(Data)+selected.size()*sizeof(Data::Row)+authority_->snapshot_bytes()+data->provenance->payload_bytes();
    const auto snapshot_limit=std::min<std::size_t>(64u*1024*1024,options.work_bytes);
    auto charge=[&](std::size_t n){if(data->bytes>snapshot_limit || n>snapshot_limit-data->bytes) throw std::length_error("pixel annotation snapshot exceeds memory budget");data->bytes+=n;};
    std::set<std::string> classes;
    for(auto id:selected) {const auto a=doc.annotation(id);detail::bounded_utf8(a.class_name,256);if(!a.geometry.is_area() || a.class_name.empty()) throw std::invalid_argument("pixel training requires labelled area geometry");
        const auto b=a.geometry.bounds();if(b.x<0 || b.y<0 || static_cast<long double>(b.x)+b.width>metadata.width || static_cast<long double>(b.y)+b.height>metadata.height) throw std::invalid_argument("pixel training ROI must lie inside the image");
        charge(a.geometry.payload_bytes()+a.class_name.size()+sizeof(std::string));classes.insert(a.class_name);if(classes.size()>256) throw std::invalid_argument("pixel training exceeds 256 classes");
    }
    if(classes.size()<2 || options.max_samples/classes.size()<2) throw std::invalid_argument("pixel training requires two classes and at least two samples per class");data->classes.assign(classes.begin(),classes.end());data->rows.reserve(selected.size());
    for(auto id:selected) {const auto a=doc.annotation(id);data->rows.push_back({id,a.geometry,static_cast<std::uint32_t>(std::lower_bound(data->classes.begin(),data->classes.end(),a.class_name)-data->classes.begin()),a.revision,a.kind});}
    charge(0);if(!authority_->matches(doc)) throw std::runtime_error("document changed during pixel training capture");data->capture_ms=elapsed(start);data_=std::move(data);
}
bool PixelTrainingRequest::matches(const Document& doc) const {return authority_ && authority_->matches(doc);}
PixelTrainingOptions PixelTrainingRequest::options() const {return data_->options;}
PixelResolution PixelTrainingRequest::resolution() const {return data_->resolution;}
std::uint64_t PixelTrainingRequest::document_revision() const {return data_->revision;}
std::size_t PixelTrainingRequest::snapshot_bytes() const {return data_->bytes;}
double PixelTrainingRequest::capture_ms() const {return data_->capture_ms;}
const std::shared_ptr<const AnalysisProvenance>& PixelTrainingRequest::provenance() const {return data_->provenance;}
const std::vector<std::string>& PixelTrainingTable::classes() const {return snapshot_->classes;}
PixelResolution PixelTrainingTable::resolution() const {return snapshot_->resolution;}
std::size_t PixelTrainingTable::retained_bytes() const {return snapshot_->bytes+row_count()*(features_.size()*sizeof(float)+28)+262144;}
PixelTrainingOptions PixelTrainingTable::options() const {return snapshot_->options;}
const StainTransform& PixelTrainingTable::transform() const {return snapshot_->transform;}
const std::shared_ptr<const AnalysisProvenance>& PixelTrainingTable::provenance() const {return snapshot_->provenance;}
struct PixelTrainingKernel {
    static std::shared_ptr<const PixelTrainingTable> run(const PixelTrainingRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<std::uint64_t>* total=nullptr,std::atomic<int>* phase=nullptr) {
        const auto start=Clock::now();stop(cancelled);const auto& d=*request.data_;const auto& p=d.options;const auto& level=d.metadata.levels[p.level];const auto classes=d.classes.size(),columns=p.features.feature_count(),capacity=std::min(p.max_samples_per_class,p.max_samples/classes);
        const auto fixed=d.bytes+classes*capacity*(columns*8+256)+262144;
        if(fixed>=p.work_bytes) throw std::length_error("pixel training sample reservoir exceeds memory budget");
        // Tile/ROI incidences are bounded before insertion; map nodes and vector
        // capacity growth are conservatively charged independently of pixels.
        using Key=std::pair<std::int64_t,std::int64_t>;std::map<Key,std::vector<std::size_t>> tiles;std::size_t plan_bytes=0;std::uint64_t scanned=0;
        if(phase) phase->store(1);
        for(std::size_t i=0;i<d.rows.size();++i) {stop(cancelled);const auto b=d.rows[i].roi.bounds();const auto x0=edge(b.x,level.downsample,level.width),y0=edge(b.y,level.downsample,level.height),x1=edge(b.x+b.width,level.downsample,level.width),y1=edge(b.y+b.height,level.downsample,level.height);if(x0>=x1 || y0>=y1) continue;
            for(auto ty=y0/static_cast<std::int64_t>(p.tile_size);ty<=(y1-1)/static_cast<std::int64_t>(p.tile_size);++ty) for(auto tx=x0/static_cast<std::int64_t>(p.tile_size);tx<=(x1-1)/static_cast<std::int64_t>(p.tile_size);++tx) {
                stop(cancelled);const Key key{ty,tx};auto found=tiles.find(key);const bool fresh=found==tiles.end();const auto charge=16+(fresh?256:0);
                if(static_cast<std::size_t>(charge)>p.work_bytes-fixed-plan_bytes) throw std::length_error("pixel training tile plan exceeds memory budget");
                if(fresh) {const auto w=std::min<std::int64_t>(p.tile_size,level.width-tx*p.tile_size),h=std::min<std::int64_t>(p.tile_size,level.height-ty*p.tile_size);const auto count=static_cast<std::uint64_t>(w)*h;if(count>p.max_scanned_pixels-scanned) throw std::length_error("pixel training scan limit exceeded; use a coarser level or smaller annotations");scanned+=count;found=tiles.emplace(key,std::vector<std::size_t>{}).first;}
                plan_bytes+=charge;found->second.push_back(i);
            }
        }
        if(total) total->store(tiles.size());if(phase) phase->store(2);
        auto out=std::shared_ptr<PixelTrainingTable>(new PixelTrainingTable());out->snapshot_=request.data_;out->features_=pixel_feature_schema(p.features,d.transform);out->eligible_.resize(classes);out->counts_.resize(classes);out->scanned_=scanned;out->tiles_=tiles.size();out->work_=fixed+plan_bytes;
        std::vector<std::vector<Sample>> reservoirs(classes);for(auto& r:reservoirs) r.reserve(capacity);
        Sha256 input;const std::string tag="qpx.annotation-pixel-input/1";input.update(tag.data(),tag.size());input.integer(d.metadata.width);input.integer(d.metadata.height);input.integer(p.level);resolution_hash(input,d.resolution);input.integer(p.tile_size);for(const auto& name:d.classes) text(input,name);
        const auto halo=p.features.halo();std::uint64_t done=0;
        for(const auto& [key,rows]:tiles) {
            stop(cancelled);const auto x=key.second*static_cast<std::int64_t>(p.tile_size),y=key.first*static_cast<std::int64_t>(p.tile_size),w=std::min<std::int64_t>(p.tile_size,level.width-x),h=std::min<std::int64_t>(p.tile_size,level.height-y);
            const auto cx=std::max<std::int64_t>(0,x-halo),cy=std::max<std::int64_t>(0,y-halo),right=x+w+std::min(halo,level.width-x-w),bottom=y+h+std::min(halo,level.height-y-h),cw=right-cx,ch=bottom-cy;
            const auto external=static_cast<std::size_t>(cw)*ch*8+static_cast<std::size_t>(w)*h*12;
            if(external>p.work_bytes-fixed-plan_bytes || p.work_bytes-fixed-plan_bytes-external<1024*1024) throw std::length_error("pixel training tile scratch exceeds memory budget");
            auto features_options=p.features;features_options.work_bytes=std::min(features_options.work_bytes,p.work_bytes-fixed-plan_bytes-external);const PixelFeatureWindow window{level.width,level.height,x,y,w,h,cx,cy};
            const auto work=pixel_feature_work_bytes(cw,ch,features_options,window)+external+fixed+plan_bytes;out->work_=std::max(out->work_,work);
            const RegionRequest read{origin(cx,level.downsample),origin(cy,level.downsample),p.level,cw,ch};validate_region(d.metadata,read);const auto pixels=request.source_->read_region(read);stop(cancelled);
            if(pixels.width()!=cw || pixels.height()!=ch || pixels.bytes().size()!=region_byte_count(cw,ch)) throw std::runtime_error("pixel training reader returned incorrect dimensions or storage");
            for(auto v:{x,y,w,h,cx,cy,cw,ch}) input.integer(v);input.update(pixels.bytes().data(),pixels.bytes().size());
            const auto count=static_cast<std::size_t>(w)*h;std::vector<std::uint32_t> mask(count);std::vector<std::uint64_t> owners(count);bool needed=false;
            for(std::int64_t yy=0;yy<h;++yy) {stop(cancelled);for(std::int64_t xx=0;xx<w;++xx) {
                const auto index=static_cast<std::size_t>(yy*w+xx),source=static_cast<std::size_t>((y+yy-cy)*cw+x+xx-cx);if(pixels.bytes()[source*4+3]!=255) continue;
                const Point point{(static_cast<double>(x+xx)+.5)*level.downsample,(static_cast<double>(y+yy)+.5)*level.downsample};
                for(auto row:rows) {const auto& a=d.rows[row];if(!a.roi.contains(point)) continue;if(mask[index] && mask[index]!=a.label+1) {mask[index]=std::numeric_limits<std::uint32_t>::max();owners[index]=0;break;}mask[index]=a.label+1;if(!owners[index]) owners[index]=a.id;}
                if(mask[index]==std::numeric_limits<std::uint32_t>::max()) {++out->ambiguous_;continue;}if(!mask[index]) continue;
                const auto label=mask[index]-1;++out->eligible_[label];const Sample candidate{mix(p.seed^mix(x+xx)^mix(std::uint64_t(y+yy)^0x6a09e667f3bcc909ULL)),owners[index],x+xx,y+yy,{}};const auto& heap=reservoirs[label];if(heap.size()<capacity || less_sample(candidate,heap.front())) needed=true;
            }}
            for(std::size_t i=0;i<count;++i) {if((i&4095)==0) stop(cancelled);input.integer(mask[i]);input.integer(owners[i]);}
            if(needed) {
                const auto features=extract_pixel_features(pixels,d.transform,features_options,window,cancelled);
                for(std::int64_t yy=0;yy<h;++yy) {stop(cancelled);for(std::int64_t xx=0;xx<w;++xx) {const auto index=static_cast<std::size_t>(yy*w+xx);if(!mask[index] || mask[index]==std::numeric_limits<std::uint32_t>::max()) continue;const auto label=mask[index]-1;auto& heap=reservoirs[label];Sample candidate{mix(p.seed^mix(x+xx)^mix(std::uint64_t(y+yy)^0x6a09e667f3bcc909ULL)),owners[index],x+xx,y+yy,{}};if(heap.size()==capacity && !less_sample(candidate,heap.front())) continue;
                    candidate.values.assign(features->values().begin()+index*columns,features->values().begin()+(index+1)*columns);if(heap.size()==capacity) {std::pop_heap(heap.begin(),heap.end(),less_sample);heap.back()=std::move(candidate);std::push_heap(heap.begin(),heap.end(),less_sample);}else {heap.push_back(std::move(candidate));std::push_heap(heap.begin(),heap.end(),less_sample);}
                }}
            }
            if(completed) completed->store(++done);
        }
        std::size_t minimum=capacity;for(const auto& heap:reservoirs) {if(heap.size()<2) throw std::invalid_argument("each pixel class requires two unambiguous opaque pixel centers");minimum=std::min(minimum,heap.size());}
        std::size_t rows=0;for(const auto& heap:reservoirs) rows+=p.balanced?minimum:heap.size();out->values_.reserve(rows*columns);out->labels_.reserve(rows);out->coordinates_.reserve(rows*2);out->ids_.reserve(rows);
        for(std::size_t label=0;label<classes;++label) {auto& heap=reservoirs[label];std::sort(heap.begin(),heap.end(),less_sample);const auto count=p.balanced?minimum:heap.size();out->counts_[label]=count;
            for(std::size_t i=0;i<count;++i) {stop(cancelled);const auto& sample=heap[i];out->values_.insert(out->values_.end(),sample.values.begin(),sample.values.end());out->labels_.push_back(static_cast<std::uint32_t>(label));out->coordinates_.push_back(sample.x);out->coordinates_.push_back(sample.y);out->ids_.push_back(sample.id);}
        }
        out->input_sha_=input.finish();Sha256 digest;const std::string domain="qpx.annotation-pixel-sampling/1";digest.update(domain.data(),domain.size());resolution_hash(digest,d.resolution);digest.integer(rows);for(const auto& f:out->features_) {text(digest,f.name);text(digest,f.unit);}for(const auto& name:d.classes) text(digest,name);
        for(std::size_t i=0;i<rows;++i) {stop(cancelled);digest.integer(out->labels_[i]);digest.integer(out->coordinates_[2*i]);digest.integer(out->coordinates_[2*i+1]);digest.integer(out->ids_[i]);for(std::size_t j=0;j<columns;++j) {std::uint32_t bits;std::memcpy(&bits,&out->values_[i*columns+j],4);digest.integer(bits);}}
        out->sha_=digest.finish();out->ms_=elapsed(start);stop(cancelled);return out;
    }
};
std::shared_ptr<const PixelTrainingTable> sample_pixel_training(const PixelTrainingRequest& request,const std::atomic<bool>* cancelled) {return PixelTrainingKernel::run(request,cancelled);}
PixelClassifier train_pixel_classifier(const PixelTrainingTable& table,ForestParameters parameters,const std::atomic<bool>* cancelled) {
    stop(cancelled);const auto p=table.options();const auto retained=table.retained_bytes();if(retained>=p.work_bytes || p.work_bytes-retained<1024*1024) throw std::length_error("pixel training forest exceeds remaining work budget");parameters.work_bytes=std::min<std::uint64_t>(parameters.work_bytes,p.work_bytes-retained);
    auto forest=train_forest(table.values(),table.labels(),table.features(),table.classes(),parameters);stop(cancelled);return PixelClassifier(forest,p.features,table.transform(),table.resolution());
}
std::int32_t pixel_classifier_level(const Document& doc,const PixelClassifier& model) {
    const auto resolution=model.resolution();if(!resolution) throw std::invalid_argument("pixel classifier has no source sampling resolution");const auto server=doc.server();if(!server || server->closed()) throw std::invalid_argument("pixel prediction requires an open source");const auto& metadata=server->metadata();metadata_check(metadata);
    std::int32_t found=-1;for(std::size_t i=0;i<metadata.levels.size();++i) {const auto scale=metadata.levels[i].downsample;bool match=false;if(resolution->physical()) match=doc.pixel_width_um() && doc.pixel_height_um() && close(*doc.pixel_width_um()*scale,*resolution->pixel_width_um) && close(*doc.pixel_height_um()*scale,*resolution->pixel_height_um);else match=close(scale,resolution->downsample);if(match && (found<0 || scale<metadata.levels[found].downsample)) found=static_cast<std::int32_t>(i);}
    if(found<0) throw std::invalid_argument("no natural pyramid level matches pixel classifier sampling resolution");return found;
}
struct PixelTrainingJob::Impl {
    struct Completion {std::shared_ptr<const PixelTrainingTable> table;std::shared_ptr<const PixelClassifier> model;std::string error;double ms=0;};
    std::optional<PixelTrainingRequest> request;std::future<Completion> future;ForestParameters parameters;std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0},total{0};std::atomic<int> phase{0};bool stale=false;PixelTrainingStats stats;std::shared_ptr<const PixelTrainingTable> table;std::shared_ptr<const PixelClassifier> model;
};
PixelTrainingJob::PixelTrainingJob():impl_(std::make_unique<Impl>()) {}
PixelTrainingJob::~PixelTrainingJob() {cancel();if(busy()) impl_->future.wait();}
bool PixelTrainingJob::busy() const {return impl_->future.valid();}
void PixelTrainingJob::cancel() {auto& s=*impl_;s.cancelled.store(true);s.table.reset();s.model.reset();if(s.request) s.stats.state=busy()?"cancelling":"cancelled";if(!busy()) s.request.reset();}
bool PixelTrainingJob::train(const PixelTrainingRequest& request,ForestParameters parameters) {
    auto& s=*impl_;if(busy()) return false;parameters.validate();if(s.stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("pixel training generation exhausted");s.request=request;s.parameters=parameters;s.table.reset();s.model.reset();s.stale=false;s.cancelled.store(false);s.completed.store(0);s.total.store(0);s.phase.store(1);++s.stats.generation;s.stats.state="running";s.stats.phase="planning";s.stats.error.clear();s.stats.elapsed_ms=0;s.stats.completed_tiles=s.stats.total_tiles=0;
    try {s.future=std::async(std::launch::async,[state=&s] {Impl::Completion out;const auto start=Clock::now();try {out.table=PixelTrainingKernel::run(*state->request,&state->cancelled,&state->completed,&state->total,&state->phase);state->phase.store(3);out.model=std::make_shared<const PixelClassifier>(train_pixel_classifier(*out.table,state->parameters,&state->cancelled));}catch(const std::exception& e) {out.table.reset();out.model.reset();out.error=e.what();}out.ms=elapsed(start);return out;});}
    catch(...) {s.request.reset();s.stats.state="failed";s.stats.phase="finished";s.stats.error="could not start pixel training worker";throw;}return true;
}
PixelTrainingStats PixelTrainingJob::poll(const Document& doc) {
    auto& s=*impl_;if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.table.reset();s.model.reset();s.stats.state=busy()?"cancelling":"stale";}
    s.stats.completed_tiles=s.completed.load();s.stats.total_tiles=s.total.load();if(busy()) s.stats.phase=s.phase.load()==1?"planning":s.phase.load()==2?"sampling":"training";
    if(busy() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto out=s.future.get();s.stats.elapsed_ms=out.ms;s.stats.completed_tiles=s.completed.load();s.stats.total_tiles=s.total.load();if(s.stale) s.stats.state="stale";else if(s.cancelled.load()) s.stats.state="cancelled";else if(!out.error.empty()) {s.stats.state="failed";s.stats.error=std::move(out.error);}else {s.table=std::move(out.table);s.model=std::move(out.model);s.stats.state="ready";}}catch(const std::exception& e) {s.stats.state="failed";s.stats.error=e.what();}s.stats.phase="finished";}
    if(!busy() && !s.model) s.request.reset();return s.stats;
}
PixelTrainingStats PixelTrainingJob::wait(const Document& doc) {poll(doc);if(busy()) impl_->future.wait();return poll(doc);}
std::shared_ptr<const PixelTrainingTable> PixelTrainingJob::table() const {return impl_->table;}
std::shared_ptr<const PixelClassifier> PixelTrainingJob::model() const {return impl_->model;}
}
