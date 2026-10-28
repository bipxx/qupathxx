#include "qupathxx/object_classifier.hpp"
#include "qupathxx/cells.hpp"
#include "qupathxx/source_identity.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include "object_feature_snapshot.hpp"
#include "object_classifier_archive.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>

namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("object classification cancelled");}
void digest_text(Sha256& hash,const std::string& s) {hash.integer(s.size());hash.update(s.data(),s.size());}
std::vector<ClassifierFeature> feature_schema(const ObjectFeatureOptions& options,bool physical) {
    std::vector<ClassifierFeature> features;const auto area=physical?"um2":"px2",length=physical?"um":"px";
    auto shape=[&](const std::string& prefix) {for(const auto& f:std::vector<ClassifierFeature>{{"area",area},{"perimeter",length},{"width",length},{"height",length},{"circularity","1"}}) features.emplace_back(prefix+f.name,f.unit);};
    shape("object.");if(options.nucleus_shape) shape("nucleus.");
    if(options.cell_rgb) for(const auto* compartment:{"cell","nucleus"}) for(const auto* channel:{"red","green","blue"}) features.emplace_back(std::string(compartment)+".mean_"+channel,"device-rgb8");
    if(options.cytoplasm_rgb) for(const auto* channel:{"red","green","blue"}) features.emplace_back(std::string("cytoplasm.mean_")+channel,"device-rgb8");
    return features;
}
}
void ObjectFeatureOptions::validate() const {
    if(snapshot_bytes<1024*1024 || snapshot_bytes>256u*1024*1024 || (cytoplasm_rgb && !cell_rgb)) throw std::invalid_argument("invalid object feature options");
}
ObjectFeatureOptions ObjectFeatureOptions::from_model(const ForestModel& model) {
    for(bool calibrated:{false,true}) for(bool nucleus:{false,true}) for(bool rgb:{false,true}) for(bool cytoplasm:{false,true}) {
        if(cytoplasm && !rgb) continue;
        ObjectFeatureOptions options;options.calibrated=calibrated;options.nucleus_shape=nucleus;options.cell_rgb=rgb;options.cytoplasm_rgb=cytoplasm;
        const auto expected=feature_schema(options,calibrated);const auto& actual=model.features();if(expected.size()!=actual.size()) continue;
        bool match=true;for(std::size_t i=0;i<actual.size();++i) match=match && expected[i].name==actual[i].name && expected[i].unit==actual[i].unit;
        if(match) return options;
    }
    throw std::invalid_argument("model feature schema is not supported by the object feature extractor");
}
ObjectFeatureRequest::ObjectFeatureRequest(const Document& doc,const std::vector<std::uint64_t>& ids,ObjectFeatureOptions options):identity_(doc.analysis_identity_.token),source_(doc.server()) {
    const auto started=Clock::now();options.validate();
    if(ids.empty() || ids.size()>100000) throw std::invalid_argument("object feature selection requires 1..100000 IDs");
    if(!source_ || source_->closed()) throw std::invalid_argument("object features require an open source");
    auto selected=ids;if(!std::is_sorted(selected.begin(),selected.end())) std::sort(selected.begin(),selected.end());
    if(std::adjacent_find(selected.begin(),selected.end())!=selected.end()) throw std::invalid_argument("duplicate object feature ID");
    auto data=std::make_shared<Data>();data->options=options;data->revision=doc.revision();data->next_id=doc.next_id_;
    auto charge=[&](std::size_t n) {if(n>options.snapshot_bytes-data->bytes) throw std::length_error("object feature snapshot exceeds memory budget");data->bytes+=n;};
    charge(sizeof(Data)+selected.size()*sizeof(Data::Row));data->rows.reserve(selected.size());
    if(options.cell_rgb) {charge(selected.size()*(sizeof(CellFeature)+sizeof(Data::Origin)));data->cells.reserve(selected.size());data->origins.reserve(selected.size());}
    if(options.calibrated && doc.pixel_width_um() && doc.pixel_height_um()) {data->physical=true;data->sx=*doc.pixel_width_um();data->sy=*doc.pixel_height_um();}
    std::map<std::uint64_t,std::shared_ptr<const ObjectCellFeatureSource>> source_runs;
    for(auto id:selected) {
        auto a=doc.annotation(id);detail::bounded_utf8(a.class_name,256);
        if(!a.geometry.is_area() || (options.nucleus_shape && !a.nucleus)) throw std::invalid_argument("object features require area geometry and requested nuclei");
        charge(a.class_name.size()+a.geometry.payload_bytes()+((options.nucleus_shape || options.cell_rgb) && a.nucleus?a.nucleus->payload_bytes():0));
        Data::Row row{id,a.revision,std::move(a.class_name),a.geometry,(options.nucleus_shape || options.cell_rgb)?a.nucleus:std::nullopt,a.kind};
        if(options.cell_rgb) {
            if(a.kind!=ObjectKind::Cell || doc.cell_object_status(id)!="current") throw std::invalid_argument("RGB features require current verified published cell measurements");
            auto cell=doc.cell_object_feature(id);
            if(options.cytoplasm_rgb && !cell.cytoplasm_rgb) throw std::invalid_argument("cell has no cytoplasm measurements");
            data->cells.push_back(std::move(cell));
            const auto& link=*a.cell_feature_link;auto found=source_runs.find(link.record_id);
            if(found==source_runs.end()) {
                const auto record=doc.cell_record(link.record_id);const auto& result=*record->result;const auto& nucleus=*result.nuclei();
                auto run=std::make_shared<ObjectCellFeatureSource>();run->record_id=link.record_id;run->object_id=nucleus.object_id();run->object_revision=nucleus.object_revision();run->document_revision=nucleus.document_revision();run->cell_count=result.features().size();run->producer=record->producer;run->algorithm=result.algorithm();run->input_sha256=nucleus.input_sha256();run->roi=nucleus.roi();run->level=nucleus.level();run->white=nucleus.white();run->stains=nucleus.stains();run->provenance=nucleus.provenance();
                if(result.grid_parameters()) {
                    if(nucleus.nominal_sampling()) run->nominal_sampling=std::make_shared<const NominalCellSampling>(*nucleus.nominal_sampling());else run->scientific_grid=*nucleus.scientific_grid();run->grid_parameters=*result.grid_parameters();run->resolved_grid_parameters=*result.resolved_grid_parameters();
                    run->parameters.algorithm=CellAlgorithm::GridLogWatershedBodies;run->downsample=std::numeric_limits<double>::quiet_NaN();
                } else {run->parameters=result.parameters();run->downsample=nucleus.downsample();}
                charge(run->payload_bytes());found=source_runs.emplace(link.record_id,std::move(run)).first;
            }
            data->origins.push_back({link.record_id,link.label,found->second});
        }
        data->rows.push_back(std::move(row));
    }
    const auto& first=data->rows.front();
    data->anchor=capture_provenance(*source_,first.id,first.revision,data->revision,false,doc.pixel_width_um(),doc.pixel_height_um());
    if(data->revision!=doc.revision() || identity_!=doc.analysis_identity_.token || source_.get()!=doc.server().get()) throw std::runtime_error("document changed during object feature capture");
    charge(data->anchor->payload_bytes());data->capture_ms=elapsed(started);data_=std::move(data);
}
bool ObjectFeatureRequest::matches(const Document& doc) const {
    if(identity_!=doc.analysis_identity_.token || source_.get()!=doc.server().get() || source_->closed() || data_->revision!=doc.revision()) return false;
    const auto observed=source_->reader_source(),captured=data_->anchor->reader_source;
    return bool(observed)==bool(captured) && (!observed || observed->generation==captured->generation);
}
std::size_t ObjectFeatureRequest::row_count() const {return data_->rows.size();}
std::size_t ObjectFeatureRequest::snapshot_bytes() const {return data_->bytes;}
std::uint64_t ObjectFeatureRequest::document_revision() const {return data_->revision;}
double ObjectFeatureRequest::capture_ms() const {return data_->capture_ms;}
ObjectFeatureOptions ObjectFeatureRequest::options() const {return data_->options;}
const std::shared_ptr<const AnalysisProvenance>& ObjectFeatureRequest::source_anchor() const {return data_->anchor;}
ObjectFeatureOptions ObjectFeatureTable::options() const {return snapshot_->options;}
const Roi& ObjectFeatureTable::body_roi(std::size_t row) const {return snapshot_->rows.at(row).body;}
std::optional<Roi> ObjectFeatureTable::nucleus_roi(std::size_t row) const {return snapshot_->rows.at(row).nucleus;}
std::optional<ObjectFeatureCellOrigin> ObjectFeatureTable::cell_origin(std::size_t row) const {(void)snapshot_->rows.at(row);if(snapshot_->origins.empty()) return {};return snapshot_->origins.at(row);}
std::optional<CellFeature> ObjectFeatureTable::cell_feature(std::size_t row) const {(void)snapshot_->rows.at(row);if(snapshot_->cells.empty()) return {};return snapshot_->cells.at(row);}


struct ObjectClassifierKernel {
    static std::shared_ptr<const ObjectFeatureTable> extract(const ObjectFeatureRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* progress=nullptr) {
        checkpoint(cancelled);const auto& data=*request.data_;
        auto out=std::shared_ptr<ObjectFeatureTable>(new ObjectFeatureTable());out->snapshot_=request.data_;out->anchor_=data.anchor;
        out->features_=feature_schema(data.options,data.physical);
        out->values_.reserve(data.rows.size()*out->features_.size());out->ids_.reserve(data.rows.size());out->revisions_.reserve(data.rows.size());out->labels_.reserve(data.rows.size());out->kinds_.reserve(data.rows.size());
        auto append=[&](double value) {
            if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max()/4) throw std::invalid_argument("object feature exceeds finite float32 range");
            const auto converted=static_cast<float>(value);if(value!=0 && converted==0) throw std::invalid_argument("object feature underflows float32 range");out->values_.push_back(converted);
        };
        auto shape=[&](const Roi& roi) {
            const auto bounds=roi.bounds();const auto area=roi.area()*data.sx*data.sy,perimeter=roi.scaled_length(data.sx,data.sy);
            if(!(area>0) || !(perimeter>0)) throw std::invalid_argument("object classifier requires positive area/perimeter");
            append(area);append(perimeter);append(bounds.width*data.sx);append(bounds.height*data.sy);
            // Algebra avoids overflowing perimeter*perimeter for large ROIs.
            append(4*3.14159265358979323846*(area/perimeter)/perimeter);
        };
        std::size_t index=0;
        for(const auto& row:data.rows) {
            checkpoint(cancelled);out->ids_.push_back(row.id);out->revisions_.push_back(row.revision);out->labels_.push_back(row.label);out->kinds_.push_back(row.kind);shape(row.body);
            if(data.options.nucleus_shape) shape(*row.nucleus);
            if(data.options.cell_rgb) {for(auto v:data.cells[index].cell_rgb) append(v);for(auto v:data.cells[index].nucleus_rgb) append(v);}
            if(data.options.cytoplasm_rgb) for(auto v:*data.cells[index].cytoplasm_rgb) append(v);
            ++index;
            if(progress) progress->store(out->ids_.size(),std::memory_order_relaxed);
        }
        Sha256 hash;const std::string domain="qpx.object-features/1";hash.update(domain.data(),domain.size());hash.integer(out->ids_.size());hash.integer(out->features_.size());
        for(const auto& f:out->features_) {digest_text(hash,f.name);digest_text(hash,f.unit);}
        for(std::size_t i=0;i<out->ids_.size();++i) {hash.integer(out->ids_[i]);hash.integer(out->revisions_[i]);hash.integer(static_cast<std::uint64_t>(out->kinds_[i]));digest_text(hash,out->labels_[i]);}
        for(auto f:out->values_) {std::uint32_t bits;std::memcpy(&bits,&f,4);hash.integer(bits);}
        checkpoint(cancelled);out->digest_=hash.finish();return out;
    }
    static ForestModel train(const ObjectFeatureTable& table,ForestParameters parameters,const std::atomic<bool>* cancelled) {
        checkpoint(cancelled);std::set<std::string> names;for(const auto& name:table.labels_) if(!name.empty()) {names.insert(name);if(names.size()>256) throw std::invalid_argument("object training exceeds 256 classes");}
        std::vector<std::string> classes(names.begin(),names.end());if(classes.size()<2) throw std::invalid_argument("object training requires two labelled classes");
        const auto columns=table.features_.size();std::vector<float> values;std::vector<std::uint32_t> labels;values.reserve(table.values_.size());labels.reserve(table.ids_.size());
        for(std::size_t row=0;row<table.ids_.size();++row) {
            checkpoint(cancelled);if(table.labels_[row].empty()) continue;
            labels.push_back(static_cast<std::uint32_t>(std::lower_bound(classes.begin(),classes.end(),table.labels_[row])-classes.begin()));
            values.insert(values.end(),table.values_.begin()+row*columns,table.values_.begin()+(row+1)*columns);
        }
        checkpoint(cancelled);auto model=train_forest(values,labels,table.features_,classes,parameters);checkpoint(cancelled);return model;
    }
    static std::shared_ptr<const ObjectClassificationResult> predict(std::shared_ptr<const ObjectFeatureTable> table,const ForestModel& model,const std::atomic<bool>* cancelled) {
        const auto started=Clock::now();checkpoint(cancelled);
        if(table->object_ids().size()*model.classes().size()>8000000) throw std::length_error("object classification output exceeds vote budget");
        auto result=std::shared_ptr<ObjectClassificationResult>(new ObjectClassificationResult());
        result->table_=std::move(table);result->model_=std::make_shared<const ForestModel>(model);result->model_sha256_=model.sha256();result->predictions_.reserve(result->table_->ids_.size());
        const auto columns=result->table_->features_.size();
        // Bounded calls give cancellation a checkpoint every 512 rows and
        // avoid a full rows*classes temporary on large object selections.
        for(std::size_t row=0;row<result->table_->ids_.size();row+=512) {
            checkpoint(cancelled);const auto stop=std::min(row+512,result->table_->ids_.size());
            std::vector<float> values(result->table_->values_.begin()+row*columns,result->table_->values_.begin()+stop*columns);
            auto predictions=model.predict(values,result->table_->features_);
            result->predictions_.insert(result->predictions_.end(),std::make_move_iterator(predictions.begin()),std::make_move_iterator(predictions.end()));
        }
        checkpoint(cancelled);result->elapsed_ms_=elapsed(started);return result;
    }
    static std::vector<std::uint64_t> apply(Document& doc,const ObjectFeatureRequest& request,const ObjectClassificationResult& result,double minimum,std::shared_ptr<const ObjectClassifierArchive> archive=nullptr) {
        if(!std::isfinite(minimum) || minimum<0 || minimum>1) throw std::invalid_argument("invalid minimum vote fraction");
        if(!request.matches(doc) || result.table_->snapshot_!=request.data_) throw std::invalid_argument("object classification is stale or belongs to another request");
        if(result.predictions_.size()!=request.row_count()) throw std::logic_error("object classification row count mismatch");
        std::vector<std::pair<std::uint64_t,std::string>> edits;std::vector<std::uint64_t> changed;
        edits.reserve(result.predictions_.size());changed.reserve(result.predictions_.size());
        for(std::size_t row=0;row<result.predictions_.size();++row) {
            const auto& p=result.predictions_[row];if(p.vote_fraction<minimum) continue;
            const auto id=result.table_->ids_[row];const auto& label=result.model_->classes().at(p.class_index);if(doc.objects().at(id).class_name==label) continue;
            edits.emplace_back(id,label);changed.push_back(id);
        }
        std::map<std::uint64_t,std::shared_ptr<const ObjectClassifierRecord>> record_nodes;std::size_t record_bytes=0;
        const auto before=doc.revision_;
        if(archive) {
            if(archive->table()!=result.table_ || !archive->result() || archive->result().get()!=&result) throw std::invalid_argument("classifier archive does not match prediction result");
            if(before==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");
            auto record=std::make_shared<ObjectClassifierRecord>();record->id=before+1;record->name="Applied object classifier";record->archive=std::move(archive);record->applied=true;record->minimum_vote_fraction=minimum;record->changed_objects=changed;record_bytes=record->payload_bytes();
            if(doc.classifiers_.size()>=10000 || record_bytes>256u*1024*1024-doc.classifiers_bytes_) throw std::length_error("classifier archive exceeds its 10000-record/256 MiB limit");
            record_nodes.emplace(record->id,std::move(record));
        }
        doc.apply_classifications(edits,"Apply object classifier");
        // All archive allocation and quota checks precede the class transaction.
        // The same revision identifies the edit and its retained audit record.
        if(!record_nodes.empty()) {if(doc.revision_==before) ++doc.revision_;doc.classifiers_.merge(record_nodes);doc.classifiers_bytes_+=record_bytes;}
        return changed;
    }
};
std::shared_ptr<const ObjectFeatureTable> extract_object_features(const ObjectFeatureRequest& request,const std::atomic<bool>* cancelled) {return ObjectClassifierKernel::extract(request,cancelled);}
ForestModel train_object_forest(const ObjectFeatureTable& table,ForestParameters parameters,const std::atomic<bool>* cancelled) {return ObjectClassifierKernel::train(table,parameters,cancelled);}
std::shared_ptr<const ObjectClassificationResult> classify_objects(const ObjectFeatureRequest& request,const ForestModel& model,const std::atomic<bool>* cancelled) {return ObjectClassifierKernel::predict(extract_object_features(request,cancelled),model,cancelled);}
std::vector<std::uint64_t> apply_object_classification(Document& doc,const ObjectFeatureRequest& request,const ObjectClassificationResult& result,double minimum) {return ObjectClassifierKernel::apply(doc,request,result,minimum);}

struct ObjectClassifierJob::Impl {
    struct Completion {std::shared_ptr<const ObjectFeatureTable> table;std::shared_ptr<const ForestModel> model;std::shared_ptr<const ObjectClassificationResult> result;std::shared_ptr<const ObjectClassifierArchive> archive;std::string error;double elapsed_ms=0;};
    std::optional<ObjectFeatureRequest> request;
    std::shared_ptr<const ForestModel> input_model,model;
    std::shared_ptr<const ObjectFeatureTable> table;
    std::shared_ptr<const ObjectClassificationResult> result;
    std::shared_ptr<const ObjectClassifierArchive> archive;
    ForestParameters parameters;
    std::future<Completion> future;
    std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> progress{0};
    bool stale=false,training=false;
    ObjectClassifierStats stats;
    bool start(const ObjectFeatureRequest& r,bool train,ForestParameters p,std::shared_ptr<const ForestModel> input) {
        if(future.valid()) return false;
        p.validate();
        if(stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("object classifier generation exhausted");
        request=r;parameters=p;input_model=std::move(input);model.reset();table.reset();result.reset();archive.reset();cancelled.store(false);progress.store(0);stale=false;training=train;
        ++stats.generation;stats.state="running";stats.phase="features";stats.error.clear();stats.completed_rows=0;stats.total_rows=r.row_count();stats.elapsed_ms=0;
        try {future=std::async(std::launch::async,[this] {
            Completion out;const auto started=Clock::now();
            try {out.table=ObjectClassifierKernel::extract(*request,&cancelled,&progress);if(training) out.model=std::make_shared<const ForestModel>(ObjectClassifierKernel::train(*out.table,parameters,&cancelled));else {out.result=ObjectClassifierKernel::predict(out.table,*input_model,&cancelled);out.model=input_model;}out.archive=archive_object_classifier(out.table,*out.model,out.result,&cancelled);}
            catch(const std::exception& e) {out.error=e.what();out.table.reset();out.model.reset();out.result.reset();}
            out.elapsed_ms=elapsed(started);return out;
        });} catch(...) {request.reset();input_model.reset();stats.state="failed";stats.phase="finished";stats.error="could not start object classifier worker";throw;}
        return true;
    }
};
ObjectClassifierJob::ObjectClassifierJob():impl_(std::make_unique<Impl>()) {}
ObjectClassifierJob::~ObjectClassifierJob() {cancel();if(busy()) impl_->future.wait();}
bool ObjectClassifierJob::busy() const {return impl_->future.valid();}
bool ObjectClassifierJob::train(const ObjectFeatureRequest& r,ForestParameters p) {return impl_->start(r,true,p,{});}
bool ObjectClassifierJob::predict(const ObjectFeatureRequest& r,const ForestModel& m) {return impl_->start(r,false,{},std::make_shared<const ForestModel>(m));}
void ObjectClassifierJob::cancel() {impl_->cancelled.store(true);impl_->model.reset();impl_->table.reset();impl_->result.reset();impl_->archive.reset();if(impl_->request) impl_->stats.state=busy()?"cancelling":"cancelled";if(!busy()) {impl_->request.reset();impl_->input_model.reset();}}
ObjectClassifierStats ObjectClassifierJob::poll(const Document& doc) {
    auto& s=*impl_;if(s.request && !s.request->matches(doc)) {s.stale=true;s.cancelled.store(true);s.model.reset();s.table.reset();s.result.reset();s.archive.reset();s.stats.state=busy()?"cancelling":"stale";}
    s.stats.completed_rows=s.progress.load(std::memory_order_relaxed);if(busy() && s.stats.completed_rows==s.stats.total_rows) s.stats.phase=s.training?"training":"predicting";
    if(busy() && s.future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto out=s.future.get();s.stats.elapsed_ms=out.elapsed_ms;
            if(s.stale) s.stats.state="stale";else if(s.cancelled.load()) s.stats.state="cancelled";else if(!out.error.empty()) {s.stats.state="failed";s.stats.error=std::move(out.error);}else {s.table=std::move(out.table);s.model=std::move(out.model);s.result=std::move(out.result);s.archive=std::move(out.archive);s.stats.state="ready";}}
        catch(const std::exception& e) {s.stats.state="failed";s.stats.error=e.what();}
        s.stats.phase="finished";
    }
    if(!busy() && !s.model) {s.request.reset();s.input_model.reset();}return s.stats;
}
void ObjectClassifierJob::await_completion() {if(impl_->future.valid()) impl_->future.wait();}
ObjectClassifierStats ObjectClassifierJob::wait(const Document& doc) {poll(doc);await_completion();return poll(doc);}
std::shared_ptr<const ForestModel> ObjectClassifierJob::model() const {return impl_->model;}
std::shared_ptr<const ObjectFeatureTable> ObjectClassifierJob::table() const {return impl_->table;}
std::shared_ptr<const ObjectClassificationResult> ObjectClassifierJob::result() const {return impl_->result;}
std::shared_ptr<const ObjectClassifierArchive> ObjectClassifierJob::archive() const {return impl_->archive;}
std::uint64_t ObjectClassifierJob::retain_training(Document& doc,std::string name) {
    if(poll(doc).state!="ready" || !impl_->request || !impl_->archive || impl_->result) throw std::logic_error("no current training record to retain");
    const auto id=doc.keep_classifier_training(*impl_->request,impl_->archive,std::move(name));impl_->stats.state="retained";impl_->request.reset();impl_->input_model.reset();return id;
}
std::vector<std::uint64_t> ObjectClassifierJob::apply(Document& doc,double minimum,bool retain_record) {
    if(poll(doc).state!="ready" || !impl_->result || !impl_->request) throw std::logic_error("no current object predictions to apply");
    auto changed=ObjectClassifierKernel::apply(doc,*impl_->request,*impl_->result,minimum,retain_record?impl_->archive:nullptr);impl_->stats.state="applied";impl_->request.reset();impl_->input_model.reset();return changed;
}
}
