#pragma once
#include "qupathxx/raw_pixel_workflows.hpp"
#include "raw_pixel_classifier_checks.hpp"
#include "qupathxx/source_identity.hpp"
#include "../reader_file_guard.hpp"
#include <condition_variable>
#include <mutex>
#include <future>

namespace raw_workflow_checks {
using raw_feature_checks::check;
using raw_feature_checks::rejects;
class Reader final:public qpx::ImageServer {
    raw_feature_checks::Reader input_{0};
    std::shared_ptr<qpx::TiffView> view_=std::make_shared<qpx::TiffView>();
    std::shared_ptr<const qpx::ReaderSource> observed_=qpx::make_reader_source("raw-feature-reference","","memory");
    mutable std::mutex mutex_;mutable std::condition_variable condition_;
    mutable bool blocking_=false,entered_=false,released_=false;
public:
    mutable std::atomic<unsigned> reads{0};std::atomic<bool> failing{false},shut{false};
    std::shared_ptr<qpx::RawImageMetadata> raw=input_.raw;
    const qpx::ImageMetadata& metadata() const override {return input_.metadata();}
    std::string reader_id() const override {return input_.reader_id();}
    std::string source_locator() const override {return observed_->locator;}
    std::shared_ptr<const qpx::ReaderSource> reader_source() const override {return observed_;}
    std::shared_ptr<const qpx::TiffView> tiff_view() const override {return view_;}
    std::shared_ptr<const qpx::RawImageMetadata> raw_metadata(const std::atomic<bool>*) const override {return raw;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest&) const override {throw std::runtime_error("raw workflow used display IO");}
    qpx::RawPixelBuffer read_raw(const qpx::RawRegionRequest& request,const std::atomic<bool>* cancelled) const override {
        ++reads;{std::unique_lock<std::mutex> lock(mutex_);if(blocking_) {entered_=true;condition_.notify_all();if(!condition_.wait_for(lock,std::chrono::seconds(10),[&]{return released_;})) throw std::runtime_error("raw workflow fixture read barrier timed out");}}
        if(failing.load()) throw std::runtime_error("independent raw workflow decoder failure");return input_.read_raw(request,cancelled);
    }
    void arm() {std::lock_guard<std::mutex> lock(mutex_);blocking_=true;entered_=released_=false;}
    void wait_entered() const {std::unique_lock<std::mutex> lock(mutex_);check(condition_.wait_for(lock,std::chrono::seconds(5),[&]{return entered_;}));}
    void release() {std::lock_guard<std::mutex> lock(mutex_);released_=true;blocking_=false;condition_.notify_all();}
    void files(const std::string& primary,const std::string& companion) {
        qpx::detail::ReaderFileGuard a(primary),b(companion);observed_=qpx::make_compound_reader_source(reader_id(),a.version()->path,a.version(),{*b.version()});
    }
    void close() override {shut.store(true);}bool closed() const override {return shut.load();}
};
inline std::uint64_t integer(const std::vector<std::uint8_t>& bytes,std::size_t offset) {std::uint64_t value=0;for(int i=0;i<8;++i) value|=std::uint64_t(bytes.at(offset+i))<<(8*i);return value;}
inline void run() {
    using namespace qpx;
    auto source=std::make_shared<Reader>();Document doc(source);const auto a=doc.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A"),b=doc.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B"),duplicate=doc.add_detection(Roi::rectangle({0,0,2,5}),"A duplicate",0,"A");
    ScientificRawOptions selection;selection.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};RawPixelTrainingOptions options;options.max_samples=12;options.max_samples_per_class=6;
    const ScientificGrid grid(7,5,{0,7,7},{0,5,5});const RawPixelTrainingRequest request(doc,{b,duplicate,a},grid,selection,features,options);
    check(request.matches(doc) && source->reads.load()==0);const auto table=sample_raw_pixel_training(request);check(table->row_count()==12 && table->eligible_class_samples()==std::vector<std::uint64_t>({15,15}) && table->sampled_class_counts()==std::vector<std::uint64_t>({6,6}) && table->ambiguous_samples()==0);
    check(table->estimated_work_bytes()<=options.work_bytes && table->retained_bytes()<options.work_bytes);
    for(std::size_t row=0;row<table->row_count();++row) {const auto x=table->coordinates()[2*row],y=table->coordinates()[2*row+1];check(table->values()[2*row]==y*7+x && table->values()[2*row+1]==y*7+x+20);check(table->labels()[row]==(x<3?0u:1u) && table->annotation_ids()[row]==(x<3?a:b));}
    check(sample_raw_pixel_training(RawPixelTrainingRequest(doc,{a,b,duplicate},grid,selection,features,options))->sha256()==table->sha256());
    Document copied=doc;check(!request.matches(copied));auto bad=options;bad.max_scanned_samples=1;const auto before=source->reads.load();rejects<std::length_error>([&]{RawPixelTrainingRequest too_many(doc,{a,b},grid,selection,features,bad);});
    bad=options;bad.work_bytes=1024*1024;rejects<std::length_error>([&]{RawPixelTrainingRequest budget(doc,{a,b},grid,selection,features,bad);});rejects<std::invalid_argument>([&]{RawPixelTrainingRequest ids(doc,{a,a,b},grid,selection,features,options);});check(source->reads.load()==before);
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{sample_raw_pixel_training(request,&cancelled);});check(source->reads.load()==before);
    const auto conflict=doc.add_detection(Roi::rectangle({2,0,3,5}),"B conflict",0,"B");const auto conflicts=sample_raw_pixel_training(RawPixelTrainingRequest(doc,{a,b,conflict},grid,selection,features,options));check(conflicts->ambiguous_samples()==5 && conflicts->eligible_class_samples()[0]==10);doc.remove_annotation(conflict);
    // Independent non-unit rational QPR axes, including byte field order.
    const ScientificGrid fractional(7,5,{0,6,4},{0,5,2});const RawPixelTrainingRequest fractional_request(doc,{a,b},fractional,selection,features,options);const auto fractional_table=sample_raw_pixel_training(fractional_request);check(fractional_table->eligible_class_samples()==std::vector<std::uint64_t>({4,2}));
    const RawPixelClassifier fractional_model(raw_classifier_checks::forest(fractional_request.recipe()),fractional_request.recipe());const auto decoded=RawPixelClassifier::from_bytes(fractional_model.bytes());
    check(integer(fractional_model.bytes(),60)==3 && integer(fractional_model.bytes(),68)==2 && integer(fractional_model.bytes(),76)==5 && integer(fractional_model.bytes(),84)==2);check(decoded.recipe().step_x().numerator()==3 && decoded.recipe().step_x().denominator()==2 && decoded.recipe().step_y().numerator()==5 && decoded.recipe().step_y().denominator()==2);
    check(predict_raw_pixel_region(RawPixelPredictionRequest(doc,decoded,fractional))->grid().x().center(0).numerator()==3);
    const auto roi=doc.add_roi(Roi::rectangle({0,0,3,5}));const RawPixelRecipe recipe(*source->raw,grid,selection,features);const RawPixelClassifier model(raw_classifier_checks::forest(recipe),recipe);
    const RawPixelPredictionRequest prediction(doc,model,grid,roi);const auto result=predict_raw_pixel_region(prediction);check(result->class_counts()==std::vector<std::uint64_t>({2,13}));
    for(std::size_t y=0;y<5;++y) for(std::size_t x=0;x<7;++x) {const auto i=y*7+x;check(bool(result->valid()[i])==(x<3));if(x>=3) check(result->labels()[i]==0 && std::isnan(result->vote_fractions()[i]));}
    const auto revision=doc.revision();RawPixelTrainingJob sampling;check(sampling.sample(RawPixelTrainingRequest(doc,{a,b},grid,selection,features,options)));check(sampling.wait(doc).state=="ready" && sampling.table()->row_count()==12 && !sampling.model());
    if(has_classifier_training()) {ForestParameters p;p.trees=2;p.max_depth=3;check(sampling.train(RawPixelTrainingRequest(doc,{a,b},grid,selection,features,options),p));check(sampling.wait(doc).state=="ready" && sampling.model());}
    else {const auto count=source->reads.load();rejects<std::runtime_error>([&]{sampling.train(RawPixelTrainingRequest(doc,{a,b},grid,selection,features,options));});check(source->reads.load()==count);}
    RawPixelPredictionJob job;check(job.request(prediction));check(job.wait(doc).state=="ready" && job.result()->sha256()==result->sha256() && doc.revision()==revision);const auto owned=job.result();
    // Cancel while a real read callback is blocked; reject duplicate dispatch.
    source->arm();check(job.request(prediction));source->wait_entered();check(job.busy() && !job.request(prediction));job.cancel();source->release();check(job.wait(doc).state=="cancelled" && !job.result() && doc.revision()==revision);
    check(job.request(prediction));job.await_completion();check(!job.busy() && !job.result());job.cancel();check(job.poll(doc).state=="cancelled" && !job.result());
    check(sampling.sample(RawPixelTrainingRequest(doc,{a,b},grid,selection,features,options)));sampling.await_completion();sampling.cancel();check(sampling.poll(doc).state=="cancelled" && !sampling.table());
    source->failing.store(true);check(job.request(prediction));check(job.wait(doc).state=="failed" && !job.result());source->failing.store(false);check(job.request(prediction));check(job.wait(doc).state=="ready");
    // Completed workers cannot publish after ROI, label, calibration or metadata edits.
    check(job.request(prediction));job.await_completion();doc.update_roi(roi,Roi::rectangle({1,0,2,5}));check(job.poll(doc).state=="stale" && !job.result());
    check(sampling.sample(RawPixelTrainingRequest(doc,{a,b},grid,selection,features,options)));sampling.await_completion();doc.classify_annotation(a,"Changed");check(sampling.poll(doc).state=="stale" && !sampling.table());doc.classify_annotation(a,"A");
    const RawPixelPredictionRequest current(doc,model,grid,roi);check(job.request(current));job.await_completion();source->raw->channel_names[0]="mutated";check(job.poll(doc).state=="stale" && !job.result());source->raw->channel_names[0]="a";
    doc.set_calibration(.5,.25);auto physical_features=features;physical_features.sigmas={{.5,.5}};const ScientificGrid context(7,5,{0,7,7,6},{0,5,5,6});const RawPixelTrainingRequest physical_request(doc,{a,b},context,selection,physical_features,options,true);
    check(physical_request.recipe().features().sigmas[0].x==1 && physical_request.recipe().features().sigmas[0].y==2);const auto physical_table=sample_raw_pixel_training(physical_request);check(physical_table->row_count()==12);
    const RawPixelClassifier physical_model(raw_classifier_checks::forest(physical_request.recipe()),physical_request.recipe());check(job.request(RawPixelPredictionRequest(doc,physical_model,context)));check(job.wait(doc).state=="ready");
    check(job.request(RawPixelPredictionRequest(doc,physical_model,context)));job.await_completion();doc.set_calibration(.25,.5);check(job.poll(doc).state=="stale" && !job.result());
    rejects<std::invalid_argument>([&]{RawPixelPredictionRequest wrong_calibration(doc,physical_model,context);});
    auto unknown_source=std::make_shared<Reader>();Document unknown(unknown_source);const auto ua=unknown.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A"),ub=unknown.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B");rejects<std::invalid_argument>([&]{RawPixelTrainingRequest unknown_calibration(unknown,{ua,ub},context,selection,physical_features,options,true);});
    const auto wrong_thread=std::async(std::launch::async,[&] {try {job.result();return false;}catch(const std::runtime_error&) {return true;}}).get();check(wrong_thread);
    // Actual compound-file observations are checked fresh at publication,
    // including a companion mutation after the worker has completed.
    const auto directory=std::filesystem::temp_directory_path()/("qpx-raw-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);
    const auto primary=directory/"primary.bin",companion=directory/"companion.bin";{std::ofstream a(primary,std::ios::binary);a<<"primary";std::ofstream b(companion,std::ios::binary);b<<"companion";}
    auto compound=std::make_shared<Reader>();compound->files(primary.u8string(),companion.u8string());Document file_doc(compound);const RawPixelPredictionRequest file_request(file_doc,model,grid);RawPixelPredictionJob file_job;check(file_job.request(file_request));file_job.await_completion();{std::ofstream changed(companion,std::ios::binary|std::ios::app);changed<<" changed";}check(file_job.poll(file_doc).state=="stale" && !file_job.result());
    std::filesystem::remove(primary);std::filesystem::remove(companion);std::filesystem::remove(directory);
    check(owned->sha256()==result->sha256() && owned->labels()[0]==1);
    std::cout<<"Owner-captured raw training/prediction: deterministic labels, rational axes, document calibration, bounded IO, cancellation, stale compound sources and owner-thread publication passed\n";
}
}
