#include "raw_pixel_workflow_snapshot.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/object_classifier.hpp"
#include "raw_pixel_classifier_internal.hpp"
#include "raw_pixel_features.hpp"
#include "raw_analysis_internal.hpp"
#include "scientific_raw_sampling_internal.hpp"
#include "pixel_training_reservoir.hpp"
#include "reader_file_guard.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <limits>
#include <set>
#include <thread>

namespace qpx {
namespace {
void stopped(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("raw pixel workflow cancelled");}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
void floating(Sha256& hash,float value) {std::uint32_t bits;std::memcpy(&bits,&value,4);hash.integer(bits);}
std::uint64_t sample_count(const ScientificGrid& grid) {return std::uint64_t(grid.x().samples())*std::uint64_t(grid.y().samples());}
template<class Geometry> std::string metadata_epoch(const RawImageMetadata& m,const Geometry& grid) {
    Sha256 hash;text(hash,"qpx.raw-pixel-source-metadata/1");text(hash,detail::scientific_raw_input_sha256(m,grid,"metadata","",nullptr));
    for(auto value:{std::uint64_t(m.width),std::uint64_t(m.height),std::uint64_t(m.channels),std::uint64_t(m.z_planes),std::uint64_t(m.timepoints),std::uint64_t(m.series),std::uint64_t(m.series_count),std::uint64_t(m.sample_type)}) hash.integer(value);return hash.finish();
}
template<class Geometry> RawPixelSourceSnapshot capture_source(const Document& doc,const Geometry& grid,std::uint64_t object=0,std::size_t budget=1024ULL*1024*1024) {
    const auto source=doc.server();if(!source || source->closed()) throw std::invalid_argument("raw pixel workflow requires an open quantitative source");
    const auto metadata=source->raw_metadata();if(!metadata) throw std::invalid_argument("raw pixel workflow requires quantitative source metadata");
    if(metadata->payload_bytes()>budget/4) throw std::length_error("raw pixel source metadata exceeds capture budget");
    RawPixelSourceSnapshot snapshot;snapshot.metadata=*metadata;snapshot.epoch=metadata_epoch(snapshot.metadata,grid);snapshot.reader=source->reader_id();detail::bounded_utf8(snapshot.reader,65536);snapshot.calibration={doc.pixel_width_um(),doc.pixel_height_um()};snapshot.provenance=object?capture_provenance(*source,object,doc.annotation(object).revision,doc.revision(),false,snapshot.calibration.x,snapshot.calibration.y):capture_source_provenance(*source,doc.revision(),snapshot.calibration.x,snapshot.calibration.y);
    if(const auto observed=source->reader_source()) {observed->validate();auto copy=std::make_shared<ReaderSource>(*observed);if(copy->primary) copy->primary=std::make_shared<const FileVersion>(*copy->primary);snapshot.observed=std::move(copy);}return snapshot;
}
bool source_matches(const ImageServer& source,const RawPixelSourceSnapshot& snapshot) {
    if(source.closed() || source.reader_id()!=snapshot.reader) return false;const auto current=source.reader_source();
    if(bool(current)!=bool(snapshot.observed)) return false;if(!current) return true;
    if(current->primary || snapshot.observed->primary) return detail::same_reader_files(*current,*snapshot.observed);
    return current->generation==snapshot.observed->generation && current->scope==snapshot.observed->scope && current->reader==snapshot.observed->reader && current->locator==snapshot.observed->locator;
}
void verify_sample(const ImageServer& source,const RawPixelSourceSnapshot& snapshot,const ScientificRawGrid& input) {
    if(!source_matches(source,snapshot) || input.reader()!=snapshot.reader || metadata_epoch(input.metadata(),input.grid())!=snapshot.epoch) throw std::runtime_error("raw pixel source/metadata changed after owner capture");
}
template<class Geometry> bool publication_source_matches(const ImageServer& source,const RawPixelSourceSnapshot& snapshot,const Geometry& grid) {
    // Publication is a new trust boundary: recheck actual file versions, not
    // just a decoder's retained opening observation. Keep every file guarded
    // while comparing a compound set. Ordinary progress polls do no file IO.
    try {
        if(!source_matches(source,snapshot)) return false;const auto metadata=source.raw_metadata();if(!metadata || metadata_epoch(*metadata,grid)!=snapshot.epoch) return false;
        if(!snapshot.observed || !snapshot.observed->primary) return true;
        auto current=*snapshot.observed;std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards;
        guards.emplace_back(std::make_unique<detail::ReaderFileGuard>(current.primary->path));current.primary=guards.back()->version();
        for(auto& file:current.companions) {guards.emplace_back(std::make_unique<detail::ReaderFileGuard>(file.path));file=*guards.back()->version();}
        for(const auto& guard:guards) guard->verify();return detail::same_reader_files(current,*snapshot.observed);
    }catch(const std::runtime_error&) {return false;}catch(const std::invalid_argument&) {return false;}
}
RawImageMetadata calibrated_metadata(const RawPixelSourceSnapshot& source) {auto metadata=source.metadata;metadata.pixel_width_um=source.calibration.x;metadata.pixel_height_um=source.calibration.y;return metadata;}
std::size_t source_bytes(const RawPixelSourceSnapshot& source) {return 2*source.metadata.payload_bytes()+source.reader.capacity()+source.epoch.capacity()+(source.provenance?source.provenance->payload_bytes():0)+(source.observed?2*source.observed->payload_bytes():0);}
std::size_t remaining(std::size_t budget,std::size_t fixed) {if(fixed>=budget || budget-fixed<1024*1024) throw std::length_error("raw pixel workflow snapshot/output leaves insufficient feature workspace");return budget-fixed;}
}
std::string raw_pixel_source_metadata_epoch(const RawImageMetadata& metadata,const ScientificGrid& grid) {return metadata_epoch(metadata,grid);}
RawPixelSourceSnapshot raw_pixel_capture_window_source(const Document& doc,const ScientificRawWindow& window,std::uint64_t object,std::size_t budget) {return capture_source(doc,window,object,budget);}
bool raw_pixel_source_matches(const ImageServer& source,const RawPixelSourceSnapshot& snapshot) {return source_matches(source,snapshot);}
bool raw_pixel_publication_window_source_matches(const ImageServer& source,const RawPixelSourceSnapshot& snapshot,const ScientificRawWindow& window) {return publication_source_matches(source,snapshot,window);}
void raw_pixel_verify_window_sample(const ImageServer& source,const RawPixelSourceSnapshot& snapshot,const ScientificRawWindowData& input) {if(!source_matches(source,snapshot) || input.reader()!=snapshot.reader || metadata_epoch(input.metadata(),input.window())!=snapshot.epoch) throw std::runtime_error("raw window source/metadata changed after capture");}
std::size_t raw_pixel_source_snapshot_bytes(const RawPixelSourceSnapshot& source) {return source_bytes(source);}
RawPixelSourceSnapshot raw_pixel_capture_lattice_source(const Document& doc,const ScientificRawLattice& lattice,std::size_t budget) {return capture_source(doc,lattice,0,budget);}
bool raw_pixel_publication_lattice_source_matches(const ImageServer& source,const RawPixelSourceSnapshot& snapshot,const ScientificRawLattice& lattice) {return publication_source_matches(source,snapshot,lattice);}
void raw_pixel_verify_lattice_sample(const ImageServer& source,const RawPixelSourceSnapshot& snapshot,const ScientificRawLattice& lattice,const ScientificRawWindowData& input) {if(!source_matches(source,snapshot) || input.reader()!=snapshot.reader || metadata_epoch(input.metadata(),lattice)!=snapshot.epoch) throw std::runtime_error("raw training lattice source/metadata changed");}
std::string raw_pixel_training_sha256(const RawPixelTrainingTable& table,const std::atomic<bool>* cancelled) {
    Sha256 hash;text(hash,"qpx.raw-annotation-pixel-sampling/1");text(hash,table.input_sha256());for(const auto& f:table.recipe().feature_schema()) {text(hash,f.name);text(hash,f.unit);}for(const auto& name:table.classes()) text(hash,name);hash.integer(table.row_count());const auto columns=table.recipe().feature_schema().size();
    for(std::size_t row=0;row<table.row_count();++row) {stopped(cancelled);hash.integer(table.labels()[row]);hash.integer(table.annotation_ids()[row]);hash.integer(table.coordinates()[2*row]);hash.integer(table.coordinates()[2*row+1]);for(std::size_t c=0;c<columns;++c) floating(hash,table.values()[row*columns+c]);}return hash.finish();
}
std::string raw_pixel_prediction_sha256(const RawPixelClassification& result,double minimum,const std::atomic<bool>* cancelled) {
    Sha256 hash;text(hash,"qpx.raw-pixel-region/2");text(hash,result.model_sha256());text(hash,result.feature_sha256());text(hash,result.input_sha256());std::uint64_t bits;std::memcpy(&bits,&minimum,8);hash.integer(bits);
    const auto& g=result.grid();for(auto v:{g.image_width(),g.image_height(),g.x().origin(),g.x().extent(),g.x().samples(),g.x().context(),g.y().origin(),g.y().extent(),g.y().samples(),g.y().context()}) hash.integer(v);
    for(std::size_t row=0;row<result.labels().size();++row) {if((row&4095)==0) stopped(cancelled);hash.integer(result.valid()[row]);hash.integer(result.labels()[row]);if(result.valid()[row]) floating(hash,result.vote_fractions()[row]);else hash.integer(std::uint32_t(0x7fc00000));}return hash.finish();
}
void RawPixelTrainingOptions::validate() const {
    if(max_samples<4 || max_samples>100000 || max_samples_per_class<2 || max_samples_per_class>50000 || !seed || !max_scanned_samples || max_scanned_samples>100000000 || work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("invalid raw pixel annotation training options");
}
struct RawPixelTrainingRequest::Authority {
    ObjectFeatureRequest objects;RawAnalysisRequest raw;
    Authority(const Document& doc,const std::vector<std::uint64_t>& ids,const ScientificGrid& grid,ScientificRawOptions selection,std::size_t budget):
        objects(doc,ids,[&] {ObjectFeatureOptions o;o.calibrated=false;o.snapshot_bytes=std::min<std::size_t>(64u*1024*1024,budget);return o;}()),raw(doc,grid,selection,ids.front()) {}
};
RawPixelTrainingRequest::RawPixelTrainingRequest(const Document& doc,const std::vector<std::uint64_t>& ids,ScientificGrid grid,ScientificRawOptions selection,RawPixelFeatureOptions features,RawPixelTrainingOptions options,bool physical):source_(doc.server()) {
    options.validate();if(ids.empty() || ids.size()>4096) throw std::invalid_argument("raw pixel training requires 1..4096 labelled area IDs");
    if(sample_count(grid)>options.max_scanned_samples) throw std::length_error("raw pixel training grid exceeds scan limit");
    auto selected=ids;std::sort(selected.begin(),selected.end());if(std::adjacent_find(selected.begin(),selected.end())!=selected.end()) throw std::invalid_argument("duplicate raw pixel training annotation ID");
    auto source=capture_source(doc,grid,0,options.work_bytes);const RawPixelRecipe recipe(calibrated_metadata(source),grid,selection,std::move(features),physical);
    auto authority=std::make_shared<Authority>(doc,selected,grid,selection,options.work_bytes);auto data=std::make_shared<Data>(std::move(grid),recipe,options,std::move(source));
    data->snapshot=sizeof(Data)+selected.size()*sizeof(Data::Row)+authority->objects.snapshot_bytes()+authority->objects.source_anchor()->payload_bytes()+2*authority->raw.object_context().payload_bytes()+source_bytes(data->source)+65536;
    const auto snapshot_limit=std::min<std::size_t>(64u*1024*1024,options.work_bytes);
    auto charge=[&](std::size_t bytes) {if(data->snapshot>snapshot_limit || bytes>snapshot_limit-data->snapshot) throw std::length_error("raw pixel annotation snapshot exceeds work budget");data->snapshot+=bytes;};charge(0);
    std::set<std::string> classes;data->rows.reserve(selected.size());
    for(auto id:selected) {auto annotation=doc.annotation(id);detail::bounded_utf8(annotation.class_name,256);if(!annotation.geometry.is_area() || annotation.class_name.empty()) throw std::invalid_argument("raw pixel training requires labelled area geometry");
        charge(annotation.geometry.payload_bytes()+annotation.class_name.size()+sizeof(std::string));classes.insert(annotation.class_name);if(classes.size()>256) throw std::invalid_argument("raw pixel training exceeds 256 classes");data->rows.push_back({id,std::move(annotation.geometry),0,annotation.revision,annotation.kind});}
    if(classes.size()<2 || options.max_samples/classes.size()<2) throw std::invalid_argument("raw pixel training requires at least two classes and two samples per class");data->classes.assign(classes.begin(),classes.end());
    for(auto& row:data->rows) {const auto name=doc.annotation(row.id).class_name;row.label=std::uint32_t(std::lower_bound(data->classes.begin(),data->classes.end(),name)-data->classes.begin());}
    charge(data->rows.size()*1024);std::vector<SpatialEntry> entries;entries.reserve(data->rows.size());for(std::size_t i=0;i<data->rows.size();++i) entries.push_back({i+1,data->rows[i].roi.bounds()});data->index=std::make_shared<const SpatialIndex>(std::move(entries));
    const auto columns=recipe.feature_schema().size();data->capacity=std::min(options.max_samples_per_class,options.max_samples/data->classes.size());
    data->fixed=data->snapshot+data->classes.size()*data->capacity*(8*columns+256)+262144;
    data->feature_options=recipe.features();data->feature_options.work_bytes=std::min(data->feature_options.work_bytes,remaining(options.work_bytes,data->fixed));
    data->work=data->fixed+detail::raw_feature_work_bytes(data->source.metadata,data->grid,selection,data->feature_options);
    authority_=std::move(authority);data_=std::move(data);if(!matches(doc)) throw std::runtime_error("document changed during raw pixel training capture");
}
bool RawPixelTrainingRequest::matches(const Document& doc) const {return authority_->objects.matches(doc) && authority_->raw.matches(doc) && source_matches(*doc.server(),data_->source);}
bool RawPixelTrainingRequest::publication_matches(const Document& doc) const {return matches(doc) && publication_source_matches(*source_,data_->source,data_->grid);}
const ScientificGrid& RawPixelTrainingRequest::grid() const {return data_->grid;}
const RawPixelRecipe& RawPixelTrainingRequest::recipe() const {return data_->recipe;}
const RawPixelTrainingOptions& RawPixelTrainingRequest::options() const {return data_->options;}
std::size_t RawPixelTrainingRequest::snapshot_bytes() const {return data_->snapshot;}
std::size_t RawPixelTrainingRequest::estimated_work_bytes() const {return data_->work;}
const RawPixelRecipe& RawPixelTrainingTable::recipe() const {return snapshot_->recipe;}
const ScientificGrid& RawPixelTrainingTable::grid() const {return snapshot_->grid;}
const std::vector<std::string>& RawPixelTrainingTable::classes() const {return snapshot_->classes;}
const RawPixelTrainingOptions& RawPixelTrainingTable::options() const {return snapshot_->options;}
std::size_t RawPixelTrainingTable::estimated_work_bytes() const {return snapshot_->work;}
std::size_t RawPixelTrainingTable::retained_bytes() const {return snapshot_->snapshot+sizeof(*this)+values_.capacity()*sizeof(float)+labels_.capacity()*sizeof(std::uint32_t)+coordinates_.capacity()*sizeof(std::int64_t)+(ids_.capacity()+eligible_.capacity()+counts_.capacity())*sizeof(std::uint64_t)+input_sha_.capacity()+sha_.capacity();}
struct RawPixelTrainingKernel {
    static std::shared_ptr<const RawPixelTrainingTable> run(const RawPixelTrainingRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr) {
        stopped(cancelled);const auto& d=*request.data_;if(!source_matches(*request.source_,d.source)) throw std::runtime_error("raw pixel source changed before sampling");
        if(phase) phase->store(1);auto selection=d.recipe.selection();selection.work_bytes=std::min(selection.work_bytes,d.options.work_bytes-d.fixed);detail::ScientificRawReserve reserve;reserve.expected_series=d.recipe.series();const auto input=detail::read_scientific_raw_reserved(*request.source_,d.grid,selection,reserve,cancelled);verify_sample(*request.source_,d.source,input);
        if(phase) phase->store(2);const auto features=detail::extract_raw_pixel_features(input,d.feature_options,cancelled);
        auto out=std::shared_ptr<RawPixelTrainingTable>(new RawPixelTrainingTable());out->snapshot_=request.data_;out->eligible_.resize(d.classes.size());out->counts_.resize(d.classes.size());
        detail::PixelTrainingReservoir reservoir(d.classes.size(),d.capacity,d.recipe.feature_schema().size(),d.options.seed);
        const auto width=std::size_t(d.grid.x().samples()),height=std::size_t(d.grid.y().samples()),columns=features->features.size();Sha256 input_hash;text(input_hash,"qpx.raw-annotation-pixel-input/1");text(input_hash,raw_grid_analysis_hash(features->input_sha256,d.source.calibration.x,d.source.calibration.y,std::nullopt));
        for(const auto& row:d.rows) {input_hash.integer(row.id);input_hash.integer(row.label);const auto bytes=GeometryArchiveCodec::encode(row.roi);input_hash.integer(bytes.size());input_hash.update(bytes.data(),bytes.size());}
        for(std::size_t y=0;y<height;++y) {stopped(cancelled);for(std::size_t x=0;x<width;++x) {
            if((x&4095)==0) stopped(cancelled);const auto index=y*width+x;std::uint32_t label=0;std::uint64_t owner=0;
            if(features->rows_valid[index]) {const Point point{d.grid.x().center(std::int64_t(x)).binary64(),d.grid.y().center(std::int64_t(y)).binary64()};
                for(const auto candidate:d.index->query({point.x,point.y,0,0})) {const auto& row=d.rows[candidate-1];if(row.roi.contains(point)) {if(label && label!=row.label+1) {label=UINT32_MAX;owner=0;break;}label=row.label+1;if(!owner || row.id<owner) owner=row.id;}}
            }
            input_hash.integer(label);input_hash.integer(owner);
            reservoir.observe(label,owner,std::int64_t(x),std::int64_t(y),features->values.data()+index*columns);
            if(completed) completed->store(index+1,std::memory_order_relaxed);
        }}
        auto selected=reservoir.finish(d.options.balanced,cancelled);
        out->values_=std::move(selected.values);out->labels_=std::move(selected.labels);out->coordinates_=std::move(selected.coordinates);out->ids_=std::move(selected.ids);
        out->eligible_=std::move(selected.eligible);out->counts_=std::move(selected.counts);out->ambiguous_=selected.ambiguous;
        out->input_sha_=input_hash.finish();out->sha_=raw_pixel_training_sha256(*out,cancelled);verify_sample(*request.source_,d.source,input);stopped(cancelled);return out;
    }
};
std::shared_ptr<const RawPixelTrainingTable> sample_raw_pixel_training(const RawPixelTrainingRequest& request,const std::atomic<bool>* cancelled) {return RawPixelTrainingKernel::run(request,cancelled);}
RawPixelClassifier train_raw_pixel_classifier(const RawPixelTrainingTable& table,ForestParameters parameters,const std::atomic<bool>* cancelled) {
    stopped(cancelled);parameters.work_bytes=std::min<std::uint64_t>(parameters.work_bytes,remaining(table.options().work_bytes,table.retained_bytes()));
    auto forest=train_forest(table.values(),table.labels(),table.recipe().feature_schema(),table.classes(),parameters);stopped(cancelled);return RawPixelClassifier(forest,table.recipe());
}
struct RawPixelPredictionRequest::Authority {RawAnalysisRequest raw;Authority(const Document& doc,const ScientificGrid& grid,ScientificRawOptions selection,std::uint64_t object):raw(doc,grid,selection,object) {}};
RawPixelPredictionRequest::RawPixelPredictionRequest(const Document& doc,const RawPixelClassifier& model,ScientificGrid grid,std::uint64_t object,double minimum,std::size_t budget):source_(doc.server()) {
    if(!std::isfinite(minimum) || minimum<0 || minimum>1) throw std::invalid_argument("raw prediction minimum vote fraction must be 0..1");if(minimum==0) minimum=0;
    if(!budget) budget=model.recipe().features().work_bytes;if(budget<1024*1024 || budget>1024ULL*1024*1024) throw std::invalid_argument("raw prediction budget must be 1..1024 MiB");
    auto source=capture_source(doc,grid,object,budget);auto authority=std::make_shared<Authority>(doc,grid,model.recipe().selection(),object);auto data=std::make_shared<Data>(std::move(grid),model,std::move(source));data->mask=authority->raw.mask();data->minimum=minimum;data->budget=budget;
    data->fixed=sizeof(Data)+source_bytes(data->source)+2*authority->raw.object_context().payload_bytes()+2*(data->mask?data->mask->payload_bytes():0)+9*sample_count(data->grid)+262144;
    data->work=data->fixed+RawPixelClassifierKernel::plan(data->source.metadata,data->grid,model.recipe().selection(),model,remaining(budget,data->fixed),data->source.calibration).work;
    authority_=std::move(authority);data_=std::move(data);if(!matches(doc)) throw std::runtime_error("document changed during raw pixel prediction capture");
}
bool RawPixelPredictionRequest::matches(const Document& doc) const {return authority_->raw.matches(doc) && source_matches(*doc.server(),data_->source);}
bool RawPixelPredictionRequest::publication_matches(const Document& doc) const {return matches(doc) && publication_source_matches(*source_,data_->source,data_->grid);}
const ScientificGrid& RawPixelPredictionRequest::grid() const {return data_->grid;}
const RawPixelClassifier& RawPixelPredictionRequest::model() const {return data_->model;}
std::size_t RawPixelPredictionRequest::estimated_work_bytes() const {return data_->work;}
struct RawPixelPredictionKernel {
    static std::shared_ptr<const RawPixelClassification> run(const RawPixelPredictionRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr) {
        stopped(cancelled);const auto& d=*request.data_;if(!source_matches(*request.source_,d.source)) throw std::runtime_error("raw pixel source changed before prediction");if(phase) phase->store(1);
        auto selection=d.model.recipe().selection();selection.work_bytes=std::min(selection.work_bytes,d.budget-d.fixed);detail::ScientificRawReserve reserve;reserve.expected_series=d.model.recipe().series();const auto input=detail::read_scientific_raw_reserved(*request.source_,d.grid,selection,reserve,cancelled);verify_sample(*request.source_,d.source,input);if(phase) phase->store(2);
        const auto classified=RawPixelClassifierKernel::run(input,d.model,d.minimum,d.budget-d.fixed,cancelled,d.source.calibration);auto output=std::make_shared<RawPixelClassification>(*classified);output->work_=d.work;std::fill(output->counts_.begin(),output->counts_.end(),0);
        const auto width=std::size_t(d.grid.x().samples()),height=std::size_t(d.grid.y().samples());
        for(std::size_t y=0;y<height;++y) {stopped(cancelled);for(std::size_t x=0;x<width;++x) {if((x&4095)==0) stopped(cancelled);const auto i=y*width+x;const bool inside=!d.mask || d.mask->contains({d.grid.x().center(std::int64_t(x)).binary64(),d.grid.y().center(std::int64_t(y)).binary64()});
            if(!inside) {output->valid_[i]=0;output->labels_[i]=0;output->fractions_[i]=std::numeric_limits<float>::quiet_NaN();}if(output->labels_[i]) ++output->counts_[output->labels_[i]-1];if(completed) completed->store(i+1,std::memory_order_relaxed);
        }}
        output->input_sha_=raw_grid_analysis_hash(classified->input_sha256(),d.source.calibration.x,d.source.calibration.y,d.mask);output->sha_=raw_pixel_prediction_sha256(*output,d.minimum,cancelled);output->capture_=d.capture;verify_sample(*request.source_,d.source,input);stopped(cancelled);return output;
    }
};
std::shared_ptr<const RawPixelClassification> predict_raw_pixel_region(const RawPixelPredictionRequest& request,const std::atomic<bool>* cancelled) {return RawPixelPredictionKernel::run(request,cancelled);}
std::shared_ptr<const RawPixelPredictionRequest::Data> RawPixelContourKernel::capture(const RawPixelPredictionRequest& request,const std::shared_ptr<const RawPixelClassification>& result) {
    if(!result || !result->capture_ || result->capture_!=request.data_->capture) throw std::invalid_argument("raw pixel contours require the matching live prediction capture");return request.data_;
}
bool RawPixelContourKernel::current(const Document& doc,const RawPixelPredictionRequest& request,const std::shared_ptr<const RawPixelClassification>& result) {capture(request,result);return request.publication_matches(doc);}

namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void owner_check(std::thread::id owner) {if(std::this_thread::get_id()!=owner) throw std::runtime_error("raw pixel controller must run on its owner thread");}
std::string failure_text(const std::exception& error) {try {const std::string text=error.what();detail::bounded_utf8(text,8192);return text;}catch(const std::invalid_argument&) {return "Raw pixel worker failed";}}
const char* phase_name(int phase) {return phase==1?"reading":phase==2?"sampling/predicting":phase==3?"training":"starting";}
}
struct RawPixelTrainingJob::Impl {
    struct Completion {std::shared_ptr<const RawPixelTrainingTable> table;std::shared_ptr<const RawPixelClassifier> model;};
    const std::thread::id owner=std::this_thread::get_id();
    std::optional<RawPixelTrainingRequest> request;
    std::future<Completion> future;
    std::shared_ptr<const RawPixelTrainingTable> table;
    std::shared_ptr<const RawPixelClassifier> model;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    RawPixelJobStats stats;bool stale=false;Clock::time_point started;
};
RawPixelTrainingJob::RawPixelTrainingJob():impl_(std::make_unique<Impl>()) {}
RawPixelTrainingJob::~RawPixelTrainingJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelTrainingJob::busy() const {owner_check(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawPixelTrainingJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawPixelTrainingJob::await_completion() {owner_check(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelTrainingJob::sample(const RawPixelTrainingRequest& request) {return start(request,std::nullopt);}
bool RawPixelTrainingJob::train(const RawPixelTrainingRequest& request,ForestParameters parameters) {owner_check(impl_->owner);parameters.validate();if(!has_classifier_training()) throw std::runtime_error("raw pixel classifier training requires the optional OpenCV backend");return start(request,parameters);}
bool RawPixelTrainingJob::start(const RawPixelTrainingRequest& request,std::optional<ForestParameters> parameters) {
    owner_check(impl_->owner);auto& s=*impl_;if(busy()) return false;if(s.stats.generation==UINT64_MAX) throw std::overflow_error("raw training generation exhausted");
    // Keep the captured request on the owner before launching; the worker's
    // copy can finish/destruct immediately without retiring the reader there.
    auto captured=request;const auto old_cancel=s.cancelled.load();const auto old_completed=s.completed.load();const auto old_phase=s.phase.load();s.cancelled.store(false);s.completed.store(0);s.phase.store(0);
    std::future<Impl::Completion> future;
    try {future=std::async(std::launch::async,[state=&s,captured,parameters] {Impl::Completion out;out.table=RawPixelTrainingKernel::run(captured,&state->cancelled,&state->completed,&state->phase);if(parameters) {state->phase.store(3);out.model=std::make_shared<const RawPixelClassifier>(train_raw_pixel_classifier(*out.table,*parameters,&state->cancelled));}return out;});}
    catch(...) {s.cancelled.store(old_cancel);s.completed.store(old_completed);s.phase.store(old_phase);throw;}
    s.future=std::move(future);s.request=std::move(captured);s.table.reset();s.model.reset();s.stale=false;s.started=Clock::now();const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.total_samples=sample_count(request.grid());return true;
}
RawPixelJobStats RawPixelTrainingJob::poll(const Document& doc) {
    owner_check(impl_->owner);auto& s=*impl_;
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.table.reset();s.model.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto completion=s.future.get();if(s.stale || (!s.cancelled.load() && !s.request->publication_matches(doc))) {s.stats.state="stale";s.stale=true;}
            else if(s.cancelled.load()) s.stats.state="cancelled";else {s.table=std::move(completion.table);s.model=std::move(completion.model);s.stats.state="ready";}}
        catch(const std::exception& error) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";s.stats.error=failure_text(error);s.table.reset();s.model.reset();}
        s.stats.phase="finished";s.stats.elapsed_ms=elapsed(s.started);
    }
    if(!s.future.valid() && s.cancelled.load() && !s.stale && s.stats.state=="ready") s.stats.state="cancelled";
    if(s.stats.state!="ready" && !s.future.valid()) {s.table.reset();s.model.reset();s.request.reset();}
    s.stats.completed_samples=s.completed.load();if(s.future.valid()) s.stats.phase=phase_name(s.phase.load());return s.stats;
}
RawPixelJobStats RawPixelTrainingJob::wait(const Document& doc) {owner_check(impl_->owner);poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const RawPixelTrainingTable> RawPixelTrainingJob::table() const {owner_check(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->table;}
std::shared_ptr<const RawPixelClassifier> RawPixelTrainingJob::model() const {owner_check(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->model;}
bool RawPixelTrainingJob::verify_result(const Document& document,std::uint64_t id) {if(poll(document).state!="ready" || !impl_->table || !impl_->request) throw std::runtime_error("raw pixel verification table is not ready/current");return document.verify_raw_pixel_training(id,*impl_->request,impl_->table,impl_->model);}
struct RawPixelPredictionJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::optional<RawPixelPredictionRequest> request;
    std::future<std::shared_ptr<const RawPixelClassification>> future;
    std::shared_ptr<const RawPixelClassification> result;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    RawPixelJobStats stats;bool stale=false;Clock::time_point started;
};
RawPixelPredictionJob::RawPixelPredictionJob():impl_(std::make_unique<Impl>()) {}
RawPixelPredictionJob::~RawPixelPredictionJob() {cancel();if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelPredictionJob::busy() const {owner_check(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void RawPixelPredictionJob::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);}
void RawPixelPredictionJob::await_completion() {owner_check(impl_->owner);if(impl_->future.valid()) impl_->future.wait();}
bool RawPixelPredictionJob::request(const RawPixelPredictionRequest& request) {
    owner_check(impl_->owner);auto& s=*impl_;if(busy()) return false;if(s.stats.generation==UINT64_MAX) throw std::overflow_error("raw prediction generation exhausted");
    auto captured=request;const auto old_cancel=s.cancelled.load();const auto old_completed=s.completed.load();const auto old_phase=s.phase.load();s.cancelled.store(false);s.completed.store(0);s.phase.store(0);
    std::future<std::shared_ptr<const RawPixelClassification>> future;
    try {future=std::async(std::launch::async,[state=&s,captured] {return RawPixelPredictionKernel::run(captured,&state->cancelled,&state->completed,&state->phase);});}
    catch(...) {s.cancelled.store(old_cancel);s.completed.store(old_completed);s.phase.store(old_phase);throw;}
    s.future=std::move(future);s.request=std::move(captured);s.result.reset();s.stale=false;s.started=Clock::now();const auto generation=s.stats.generation+1;s.stats={};s.stats.generation=generation;s.stats.state="running";s.stats.total_samples=sample_count(request.grid());return true;
}
RawPixelJobStats RawPixelPredictionJob::poll(const Document& doc) {
    owner_check(impl_->owner);auto& s=*impl_;
    if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.result.reset();s.stats.state=busy()?"cancelling":"stale";}
    if(s.future.valid() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=s.future.get();if(s.stale || (!s.cancelled.load() && !s.request->publication_matches(doc))) {s.stats.state="stale";s.stale=true;}
            else if(s.cancelled.load()) s.stats.state="cancelled";else {s.result=std::move(result);s.stats.state="ready";}}
        catch(const std::exception& error) {s.stats.state=s.stale?"stale":s.cancelled.load()?"cancelled":"failed";s.stats.error=failure_text(error);s.result.reset();}
        s.stats.phase="finished";s.stats.elapsed_ms=elapsed(s.started);
    }
    if(!s.future.valid() && s.cancelled.load() && !s.stale && s.stats.state=="ready") s.stats.state="cancelled";
    if(s.stats.state!="ready" && !s.future.valid()) {s.result.reset();s.request.reset();}
    s.stats.completed_samples=s.completed.load();if(s.future.valid()) s.stats.phase=phase_name(s.phase.load());return s.stats;
}
RawPixelJobStats RawPixelPredictionJob::wait(const Document& doc) {owner_check(impl_->owner);poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const RawPixelClassification> RawPixelPredictionJob::result() const {owner_check(impl_->owner);return impl_->cancelled.load()?nullptr:impl_->result;}
bool RawPixelPredictionJob::verify_result(const Document& document,std::uint64_t id) {if(poll(document).state!="ready" || !impl_->result || !impl_->request) throw std::runtime_error("raw pixel verification prediction is not ready/current");return document.verify_raw_pixel_prediction(id,*impl_->request,impl_->result);}
}
