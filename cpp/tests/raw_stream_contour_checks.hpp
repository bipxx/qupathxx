#pragma once
#include "raw_pixel_replay_checks.hpp"
#include "qupathxx/raw_pixel_stream_contours.hpp"
#include <type_traits>
#include "qupathxx/raw_pixel_file_jobs.hpp"
namespace raw_stream_contour_checks {
static_assert(!std::is_default_constructible_v<qpx::RawPixelStreamContours>);
static_assert(!std::is_default_constructible_v<qpx::RawPixelFileContours>);
static_assert(!std::is_convertible_v<qpx::RawPixelFileContours,qpx::RawPixelStreamContours>);
inline void require(bool value,const char* message) {if(!value) throw std::runtime_error(std::string("Raw streamed contour workflow: ")+message);}
inline void run() {
    using namespace qpx;using raw_feature_checks::rejects;
    auto source=std::make_shared<raw_workflow_checks::Reader>();Document doc(source);
    const auto roi=doc.add_roi(Roi::rectangle({0,0,6,5}));doc.set_calibration(.5,.25);
    const ScientificGrid grid(7,5,{0,6,4,1},{0,5,2,1});const ScientificRawLattice lattice(grid);
    ScientificRawOptions selection;selection.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};
    const RawPixelRecipe recipe(*source->raw,grid,selection,features);const RawPixelClassifier model(raw_classifier_checks::forest(recipe),recipe);
    RawPixelStreamOptions stream_options;stream_options.tile_width=2;stream_options.tile_height=1;stream_options.work_bytes=8*1024*1024;
    const RawPixelTiledPredictionRequest request(doc,model,lattice,stream_options,{},roi);RawPixelStreamContourJob job;
    if(!has_geos()) {
        const auto reads=source->reads.load();rejects<std::runtime_error>([&]{trace_raw_pixel_stream(request);});rejects<std::runtime_error>([&]{job.request(request);});
        require(source->reads.load()==reads,"unavailable geometry performed IO");std::cout<<"Raw streamed contour workflow: GEOS-unavailable direct/job pre-IO refusal passed\n";return;
    }
    RawPixelStreamContourOptions options;options.boundary_work_bytes=1024*1024;options.contours.work_bytes=1024*1024;options.work_bytes=16*1024*1024;
    const auto reads=source->reads.load();auto tiny=options;tiny.work_bytes=1;rejects<std::length_error>([&]{trace_raw_pixel_stream(request,tiny);});rejects<std::length_error>([&]{job.request(request,tiny);});
    tiny=options;tiny.boundary_work_bytes=1;rejects<std::length_error>([&]{trace_raw_pixel_stream(request,tiny);});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{trace_raw_pixel_stream(request,options,&cancelled);});require(source->reads.load()==reads,"admission/cancel performed IO");
    const auto traced=trace_raw_pixel_stream(request,options);RawPixelPredictionStream reference(request);while(reference.next()) {}
    require(traced->input_sha256()==reference.input_sha256() && traced->prediction_sha256()==reference.sha256() && traced->request_sha256()==request.sha256(),"completed commitment differs from producer");
    require(traced->class_counts()==reference.class_counts() && traced->completed_windows()==request.total_windows() && traced->completed_samples()==8 && traced->valid_samples()==reference.valid_samples(),"complete stream census differs");
    require(traced->payload_bytes()<=traced->estimated_work_bytes() && traced->estimated_work_bytes()<=options.work_bytes,"retained/phase work admission differs");
    const RawPixelPredictionRequest full_request(doc,model,grid,roi);const auto full=trace_raw_pixel_prediction(full_request,predict_raw_pixel_region(full_request));
    require(full->contours()->components().size()==traced->contours()->components().size(),"full/stream class geometry differs");
    for(std::size_t i=0;i<traced->contours()->components().size();++i) {
        const auto& a=traced->contours()->components()[i];const auto& b=full->contours()->components()[i];
        require(a.label==b.label && a.pixels==b.pixels && a.geometry.equals(b.geometry),"full/stream rational geometry differs");
    }
    const RawPixelTiledPredictionRequest cropped(doc,model,lattice,stream_options,RawPixelStreamRegion{1,0,2,2},roi);
    const auto crop=trace_raw_pixel_stream(cropped,options);require(crop->completed_samples()==4 && crop->region().first_x==1 && crop->lattice().x().samples()==4,"crop lost global denominator");
    for(const auto& c:crop->contours()->components()) require(c.geometry.area()==c.pixels*3.75 && c.geometry.bounds().x>=1.5 && c.geometry.bounds().x+c.geometry.bounds().width<=4.5,"cropped world phase/area differs");
    require(job.request(request,options) && job.wait(doc).state=="ready" && job.result()->prediction_sha256()==traced->prediction_sha256(),"job/direct parity");
    const auto retained_job=job.result();
    require(job.poll(doc).produced_windows==request.total_windows() && job.poll(doc).delivered_samples==8,"job progress census");
    auto off_thread=std::async(std::launch::async,[&]{try {job.result();return false;}catch(const std::runtime_error&) {return true;}});require(off_thread.get(),"cross-thread result access accepted");
    require(job.request(request,options),"retry");job.await_completion();job.cancel();require(job.poll(doc).state=="cancelled" && !job.result(),"completed unpublished cancellation");
    source->arm();require(job.request(request,options),"blocked dispatch");source->wait_entered();require(job.busy() && !job.request(request,options),"busy replacement accepted");std::async(std::launch::async,[&]{job.cancel();}).get();source->release();require(job.wait(doc).state=="cancelled" && !job.result(),"in-read cancellation published result");
    source->failing.store(true);require(job.request(request,options) && job.wait(doc).state=="failed" && !job.result(),"decoder failure delivered");source->failing.store(false);require(job.request(request,options) && job.wait(doc).state=="ready","failed job retry");
    Document copied=doc;require(job.request(request,options),"copied owner dispatch");job.await_completion();require(job.poll(copied).state=="stale" && !job.result(),"copied document delivered");
    const RawPixelTiledPredictionRequest foreign(doc,model,lattice,stream_options,{},roi);const auto revision=doc.revision();const auto count=doc.object_count();
    rejects<std::invalid_argument>([&]{publish_raw_pixel_stream_contours(doc,foreign,*traced);});rejects<std::invalid_argument>([&]{publish_raw_pixel_stream_contours(copied,request,*traced);});
    rejects<std::invalid_argument>([&]{publish_raw_pixel_stream_contours(doc,request,*traced,"");});rejects<std::length_error>([&]{publish_raw_pixel_stream_contours(doc,request,*traced,"Raw",1);});
    require(doc.revision()==revision && doc.object_count()==count,"failed admission/apply mutated document");
    const auto ids=publish_raw_pixel_stream_contours(doc,request,*traced);require(ids.size()==traced->contours()->components().size() && doc.revision()==revision+1 && doc.object_count()==count+ids.size(),"atomic streamed apply");
    for(std::size_t i=0;i<ids.size();++i) require(doc.annotation(ids[i]).parent_id==roi && doc.annotation(ids[i]).class_name==model.forest().classes()[traced->contours()->components()[i].label-1],"published parent/class");
    rejects<std::invalid_argument>([&]{publish_raw_pixel_stream_contours(doc,request,*traced);});require(doc.undo() && doc.object_count()==count && doc.redo() && doc.object_count()==count+ids.size(),"atomic undo redo");
    const RawPixelTiledPredictionRequest fresh(doc,model,lattice,stream_options,{},roi);source->arm();require(job.request(fresh,options),"edit-stale dispatch");source->wait_entered();doc.rename_annotation(roi,"Changed ROI");source->release();require(job.wait(doc).state=="stale" && !job.result(),"edit during IO delivered");
    const RawPixelTiledPredictionRequest metadata(doc,model,lattice,stream_options,{},roi);require(job.request(metadata,options),"late metadata dispatch");job.await_completion();const auto order=source->raw->dimension_order;source->raw->dimension_order+="changed";require(job.poll(doc).state=="stale" && !job.result(),"late metadata delivered");source->raw->dimension_order=order;
    if(has_tiff() && has_project()) {
        const auto directory=std::filesystem::temp_directory_path()/("qpx-stream-contours-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));require(std::filesystem::create_directory(directory),"TIFF fixture directory");const auto path=directory/"source.tif";raw_archive_checks::simple_tiff(path);
        auto project=std::make_shared<Project>();const auto image=project->add_tiff_image(path.u8string(),{});auto live=project->document(image);const auto target=live->add_roi(Roi::rectangle({0,0,8,6}));
        const ScientificRawLattice real_lattice(8,6,{1,6,4},{0,5,2});const auto raw=live->server()->raw_metadata();const RawPixelRecipe real_recipe(*raw,real_lattice,selection,features);const RawPixelClassifier real_model(raw_classifier_checks::forest(real_recipe),real_recipe);
        const RawPixelTiledPredictionRequest real(*live,real_model,real_lattice,stream_options,RawPixelStreamRegion{1,0,2,2},target);
        const auto direct=trace_raw_pixel_stream(real,options);require(job.request(real,options) && job.wait(*live).state=="ready" && job.result()->prediction_sha256()==direct->prediction_sha256(),"real TIFF job/direct parity");
        const auto live_ids=publish_raw_pixel_stream_contours(*live,real,*direct);require(!live_ids.empty() && live->undo() && live->redo(),"real TIFF atomic apply/undo/redo");
        // A saved file is historical geometry; replay must infer a fresh capture.
        const auto saved_path=directory/"saved.qws";const RawPixelTiledPredictionRequest saved_request(*live,real_model,real_lattice,stream_options,RawPixelStreamRegion{1,0,2,2},target);
        RawPixelStreamSaveOptions save_options;save_options.limits.work_bytes=1024*1024;save_raw_pixel_stream(saved_request,saved_path.u8string(),save_options);
        auto history=std::make_shared<const RawPixelStreamFile>(saved_path.u8string(),save_options.limits);
        rejects<std::invalid_argument>([&]{capture_raw_pixel_stream_replay(*live,*history);});
        const auto historical=trace_raw_pixel_stream_file(history,options);const auto fresh=trace_raw_pixel_stream(saved_request,options);
        require(history->verified() && historical->link()->summary().complete && historical->completed_samples()==fresh->completed_samples(),"historical verified coverage");
        require(historical->contours()->object_id()==0 && historical->contours()->input_sha256().empty(),"historical geometry acquired authority");
        require(historical->contours()->components().size()==fresh->contours()->components().size(),"historical component count");
        for(std::size_t i=0;i<fresh->contours()->components().size();++i) require(historical->contours()->components()[i].geometry.equals(fresh->contours()->components()[i].geometry),"historical geometry differs");
        RawPixelStreamFileReadJob reader;require(reader.trace(history,options) && reader.wait().state=="ready" && !reader.take_block() && reader.take_contours(),"historical reader job delivery");
        const auto replay_capture=capture_raw_pixel_stream_replay(*live,*history);const auto replay=trace_raw_pixel_stream_replay(replay_capture,history,options);
        require(replay->prediction_sha256()==history->declared_summary().sha256 && replay->input_sha256()==history->declared_summary().input_sha256,"fresh replay commitments differ");
        const auto replay_ids=publish_raw_pixel_stream_contours(*live,replay_capture,*replay,"Replay");require(!replay_ids.empty(),"fresh replay apply");live->undo();
        const auto next_capture=capture_raw_pixel_stream_replay(*live,*history);require(job.request_replay(next_capture,history,options) && job.wait(*live).state=="ready","replay job");
        const auto retained_replay=job.result();{std::ofstream corrupt(saved_path,std::ios::binary|std::ios::app);corrupt<<"changed";}
        require(job.poll(*live).state=="failed" && !job.result(),"late history mutation delivered ready result");
        rejects<std::runtime_error>([&]{publish_raw_pixel_stream_contours(*live,next_capture,*retained_replay);});
        require(historical->contours()->components().size()==fresh->contours()->components().size(),"historical snapshot mutated");
        const auto partial_path=directory/"partial.qws";save_options.max_new_windows=1;save_raw_pixel_stream(saved_request,partial_path.u8string(),save_options);auto partial=std::make_shared<const RawPixelStreamFile>(partial_path.u8string(),save_options.limits);
        rejects<std::invalid_argument>([&]{trace_raw_pixel_stream_file(partial,options);});
        std::filesystem::remove(saved_path);std::filesystem::remove(partial_path);
        std::cout<<"Raw historical contour workflow: verified one-pass geometry, typed authority separation, fresh replay/apply/undo, late file refusal and partial-prefix rejection passed\n";
        const RawPixelTiledPredictionRequest again(*live,real_model,real_lattice,stream_options,RawPixelStreamRegion{1,0,2,2},target);const auto guarded=trace_raw_pixel_stream(again,options);require(job.request(again,options),"late file dispatch");job.await_completion();{std::ofstream append(path,std::ios::binary|std::ios::app);append<<"changed";}
        const auto before_objects=live->object_count();const auto before_revision=live->revision();require(job.poll(*live).state=="stale" && !job.result(),"late file delivered");rejects<std::invalid_argument>([&]{publish_raw_pixel_stream_contours(*live,again,*guarded);});require(live->object_count()==before_objects && live->revision()==before_revision,"late file refusal mutated document");
        live->server()->close();project.reset();std::filesystem::remove(path);std::filesystem::remove(directory);
        std::cout<<"Raw streamed contour workflow: real TIFF cropped direct/job parity, atomic apply/undo/redo and late-file refusal passed\n";
    }
    require(retained_job && retained_job->prediction_sha256()==traced->prediction_sha256(),"retained job result changed after cancellation/replacement");
    require(traced->completed_samples()==8 && traced->contours()->components().size()==full->contours()->components().size(),"retained result lost lifetime");
    std::cout<<"Raw streamed contour workflow: completed producer commitments, rational full/cropped geometry, direct/job progress, pre-IO/retained/edit/ID quotas, cancel/failure/stale/copy/thread, exact capture and atomic apply/undo/redo passed\n";
}
}
