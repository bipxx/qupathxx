#pragma once
#include "raw_pixel_archive_checks.hpp"
#include "qupathxx/raw_pixel_contours.hpp"
#include <type_traits>
namespace raw_replay_checks {
static_assert(!std::is_default_constructible_v<qpx::RawPixelContours>);
inline void require(bool value,const char* text) {if(!value) throw std::runtime_error(std::string("Raw replay/apply: ")+text);}
inline void run() {
    using namespace qpx;using raw_feature_checks::rejects;
    const char* stage="capture and replay verification";
    try {
    auto source=std::make_shared<raw_workflow_checks::Reader>();Document doc(source);
    const auto a=doc.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A"),b=doc.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B"),roi=doc.add_roi(Roi::rectangle({0,0,6,5}));
    const ScientificGrid grid(7,5,{0,6,4,1},{0,5,2,1});ScientificRawOptions selection;selection.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};RawPixelTrainingOptions options;options.max_samples=20;options.max_samples_per_class=10;
    const RawPixelTrainingRequest training(doc,{a,b},grid,selection,features,options);const auto table=sample_raw_pixel_training(training);const auto sampled=archive_raw_pixel_training(table);const auto training_id=doc.keep_raw_pixel_training(training,sampled);
    const RawPixelClassifier model(raw_classifier_checks::forest(training.recipe()),training.recipe());const RawPixelPredictionRequest prediction(doc,model,grid,roi);const auto result=predict_raw_pixel_region(prediction);const auto archived=archive_raw_pixel_prediction(prediction,result);const auto prediction_id=doc.keep_raw_pixel_prediction(prediction,archived);
    const auto reads=source->reads.load();const auto revision=doc.revision();const auto replay=RawPixelPredictionRequest::replay(doc,prediction_id);const auto training_replay=RawPixelTrainingRequest::replay(doc,training_id);
    require(source->reads.load()==reads,"replay performed pixel IO before worker");
    rejects<std::invalid_argument>([&]{RawPixelPredictionRequest::replay(doc,training_id);});rejects<std::invalid_argument>([&]{RawPixelTrainingRequest::replay(doc,prediction_id);});
    const auto fresh=predict_raw_pixel_region(replay);const auto fresh_table=sample_raw_pixel_training(training_replay);
    require(doc.verify_raw_pixel_prediction(prediction_id,replay,fresh) && doc.verify_raw_pixel_training(training_id,training_replay,fresh_table),"direct verification");
    rejects<std::invalid_argument>([&]{doc.verify_raw_pixel_prediction(prediction_id,replay,archived->result());});rejects<std::invalid_argument>([&]{doc.verify_raw_pixel_training(training_id,training_replay,sampled->table());});
    const auto detached=RawPixelArchive::from_bytes(archived->bytes());rejects<std::invalid_argument>([&]{doc.verify_raw_pixel_prediction(prediction_id,replay,detached->result());});
    RawPixelPredictionJob job;require(job.request(replay) && job.wait(doc).state=="ready" && job.verify_result(doc,prediction_id),"prediction job verification");
    RawPixelTrainingJob samples;require(samples.sample(training_replay) && samples.wait(doc).state=="ready" && samples.verify_result(doc,training_id),"sample job verification");
    require(doc.revision()==revision && doc.raw_pixel_record(prediction_id)->archive->bytes()==archived->bytes(),"verification mutated document/archive");
    source->altered.store(true);require(!doc.verify_raw_pixel_prediction(prediction_id,replay,predict_raw_pixel_region(replay)),"changed input bits accepted");require(!doc.verify_raw_pixel_training(training_id,training_replay,sample_raw_pixel_training(training_replay)),"changed sample bits accepted");source->altered.store(false);
    // Model/threshold changes are parameter errors, even when the raster happens
    // to have equal labels. A different capture cannot authorize old results.
    const RawPixelPredictionRequest threshold(doc,model,grid,roi,.9);rejects<std::invalid_argument>([&]{doc.verify_raw_pixel_prediction(prediction_id,threshold,predict_raw_pixel_region(threshold));});
    auto changed_features=features;changed_features.scales={2,2};const RawPixelRecipe changed_recipe(*source->raw,grid,selection,changed_features);const RawPixelClassifier changed_model(raw_classifier_checks::forest(changed_recipe),changed_recipe);const RawPixelPredictionRequest changed_prediction(doc,changed_model,grid,roi);
    rejects<std::invalid_argument>([&]{doc.verify_raw_pixel_prediction(prediction_id,changed_prediction,predict_raw_pixel_region(changed_prediction));});
    Document copied=doc;rejects<std::invalid_argument>([&]{copied.verify_raw_pixel_prediction(prediction_id,replay,fresh);});
    job.cancel();rejects<std::runtime_error>([&]{job.verify_result(doc,prediction_id);});
    auto owner_error=std::async(std::launch::async,[&]{try {samples.verify_result(doc,training_id);return false;}catch(const std::runtime_error&) {return true;}});require(owner_error.get(),"cross-thread verification accepted");
    RawPixelContourJob contours_job;
    if(!has_geos()) {
        rejects<std::runtime_error>([&]{trace_raw_pixel_prediction(replay,fresh);});rejects<std::runtime_error>([&]{contours_job.request(replay,fresh);});
        std::cout<<"Raw pixel contours: GEOS-unavailable direct/job refusal passed\n";
    } else {
        stage="direct contour trace";
        const auto traced=trace_raw_pixel_prediction(replay,fresh);require(!traced->contours()->components().empty(),"empty contour fixture");
        for(const auto& component:traced->contours()->components()) {
            require(std::abs(component.geometry.area()-double(component.pixels)*3.75)<1e-9,"rational per-axis contour area");
            for(std::int64_t y=0;y<2;++y) for(std::int64_t x=0;x<4;++x) require(component.geometry.contains({grid.x().center(x).binary64(),grid.y().center(y).binary64()})==(fresh->labels()[std::size_t(y*4+x)]==component.label),"raster center/class mismatch");
        }
        stage="historical and foreign capture refusal";
        rejects<std::invalid_argument>([&]{trace_raw_pixel_prediction(replay,detached->result());});rejects<std::invalid_argument>([&]{trace_raw_pixel_prediction(threshold,fresh);});
        stage="contour cancellation and budget refusal";
        std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{trace_raw_pixel_prediction(replay,fresh,{},&cancelled);});TissueContourOptions tiny;tiny.work_bytes=1;rejects<std::length_error>([&]{trace_raw_pixel_prediction(replay,fresh,tiny);});
        stage="contour job delivery";
        const auto count=doc.object_count();require(contours_job.request(replay,fresh) && contours_job.wait(doc).state=="ready" && contours_job.result(),"contour job delivery");
        require(contours_job.result()->prediction_sha256()==fresh->sha256(),"contour prediction identity");
        stage="completed contour cancellation";
        require(contours_job.request(replay,fresh),"contour retry");contours_job.await_completion();contours_job.cancel();require(contours_job.poll(doc).state=="cancelled" && !contours_job.result(),"completed unpublished contour cancellation");
        stage="invalid name atomic refusal";
        rejects<std::invalid_argument>([&]{publish_raw_pixel_contours(doc,replay,*traced,"");});require(doc.object_count()==count && doc.revision()==revision,"failed apply was not atomic");
        stage="first live contour publication";
        const auto ids=publish_raw_pixel_contours(doc,replay,*traced);require(ids.size()==traced->contours()->components().size() && doc.object_count()==count+ids.size() && doc.revision()==revision+1,"atomic contour apply");
        for(std::size_t i=0;i<ids.size();++i) require(doc.annotation(ids[i]).parent_id==roi && doc.annotation(ids[i]).class_name==model.forest().classes()[traced->contours()->components()[i].label-1],"annotation parent/class");
        stage="duplicate publication refusal and undo redo";
        rejects<std::invalid_argument>([&]{publish_raw_pixel_contours(doc,replay,*traced);});require(doc.undo() && doc.object_count()==count,"undo contour command");require(doc.redo() && doc.object_count()==count+ids.size(),"redo contour command");
        for(auto id:ids) require(doc.annotation(id).parent_id==roi,"redo lost object identity");
        std::cout<<"Raw pixel contours: rational axes/context, raster membership/area, bounded conversion, cancel, atomic apply/undo/redo and stale authority passed\n";
    }
    stage="post-edit replay refusal";
    doc.rename_annotation(a,"Changed A");rejects<std::invalid_argument>([&]{RawPixelTrainingRequest::replay(doc,training_id);});
    doc.set_calibration(.5,.25);rejects<std::invalid_argument>([&]{RawPixelPredictionRequest::replay(doc,prediction_id);});
    std::cout<<"Raw pixel replay: pre-IO capture, source-bit comparison, historical/cross-document refusal, recipe/calibration/annotation checks and owner-job verification passed\n";
    } catch(const std::exception& error) {
        throw std::runtime_error(std::string("Raw replay stage [")+stage+"]: "+error.what());
    }
}
}
