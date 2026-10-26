#pragma once
#include "raw_pixel_workflow_checks.hpp"
#include "qupathxx/raw_pixel_archive.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_save.hpp"
#include "qupathxx/project_recovery.hpp"
#include "../raw_pixel_archive.hpp"
namespace raw_archive_checks {
using raw_feature_checks::check;
using raw_feature_checks::rejects;
inline void simple_tiff(const std::filesystem::path& path) {
    std::vector<std::uint8_t> bytes(160+8*6*2*2,0);auto u16=[&](std::size_t p,std::uint16_t v) {bytes[p]=std::uint8_t(v);bytes[p+1]=std::uint8_t(v>>8);};auto u32=[&](std::size_t p,std::uint32_t v) {for(int i=0;i<4;++i) bytes[p+i]=std::uint8_t(v>>(8*i));};
    bytes[0]=bytes[1]='I';u16(2,42);u32(4,8);u16(8,12);std::size_t at=10;auto tag=[&](std::uint16_t id,std::uint16_t type,std::uint32_t count,std::uint32_t value) {u16(at,id);u16(at+2,type);u32(at+4,count);u32(at+8,value);at+=12;};
    tag(256,4,1,8);tag(257,4,1,6);tag(258,3,2,0x00100010);tag(259,3,1,1);tag(262,3,1,1);tag(273,4,1,160);tag(277,3,1,2);tag(278,4,1,6);tag(279,4,1,192);tag(284,3,1,1);tag(338,3,1,0);tag(339,3,2,0x00010001);
    for(std::size_t y=0;y<6;++y) for(std::size_t x=0;x<8;++x) for(std::size_t c=0;c<2;++c) u16(160+((y*8+x)*2+c)*2,std::uint16_t(x+8*y+100*c));std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));check(bool(stream));
}
inline void run() {
    using namespace qpx;
    auto source=std::make_shared<raw_workflow_checks::Reader>();Document doc(source);const auto a=doc.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A"),b=doc.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B");
    const ScientificGrid grid(7,5,{0,6,4},{0,5,2});ScientificRawOptions selection;selection.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};RawPixelTrainingOptions options;options.max_samples=20;options.max_samples_per_class=10;
    const RawPixelTrainingRequest request(doc,{a,b},grid,selection,features,options);const auto table=sample_raw_pixel_training(request);const auto archive=archive_raw_pixel_training(table);const auto decoded=RawPixelArchive::from_bytes(archive->bytes());check(decoded->bytes()==archive->bytes() && decoded->sha256()==archive->sha256() && decoded->purpose()=="sampling" && !decoded->model());check(decoded->table()->values()==table->values() && decoded->table()->sha256()==table->sha256() && decoded->table()->coordinates()==table->coordinates());
    const auto initial=doc.revision();rejects<std::invalid_argument>([&]{doc.keep_raw_pixel_training(request,decoded);});check(doc.revision()==initial && doc.raw_pixel_record_count()==0);
    const auto retained=doc.keep_raw_pixel_training(request,archive,"Raw samples");check(doc.raw_pixel_record_count()==1 && doc.raw_pixel_record(retained)->archive->sha256()==archive->sha256());
    rejects<std::invalid_argument>([&]{doc.keep_raw_pixel_training(request,archive);});check(doc.raw_pixel_record_count()==1);
    const auto model=std::make_shared<const RawPixelClassifier>(raw_classifier_checks::forest(request.recipe()),request.recipe());rejects<std::invalid_argument>([&]{archive_raw_pixel_training(table,model);});
    const auto roi=doc.add_roi(Roi::rectangle({0,0,3,5}));const RawPixelPredictionRequest prediction(doc,*model,grid,roi);const auto result=predict_raw_pixel_region(prediction);const auto prediction_archive=archive_raw_pixel_prediction(prediction,result);const auto replay=RawPixelArchive::from_bytes(prediction_archive->bytes());
    check(replay->bytes()==prediction_archive->bytes() && replay->result()->sha256()==result->sha256() && replay->result()->labels()==result->labels() && replay->result()->valid()==result->valid());check(replay->grid().x().extent()==6 && replay->grid().x().samples()==4 && replay->grid().y().samples()==2);
    rejects<std::invalid_argument>([&]{doc.keep_raw_pixel_prediction(prediction,replay);});const auto kept=doc.keep_raw_pixel_prediction(prediction,prediction_archive);check(doc.raw_pixel_record_count()==2);
    const RawPixelPredictionRequest another(doc,*model,grid,roi);rejects<std::invalid_argument>([&]{archive_raw_pixel_prediction(another,result);});
    auto damaged=prediction_archive->bytes();damaged.back()^=1;rejects<std::invalid_argument>([&]{RawPixelArchive::from_bytes(damaged);});damaged=prediction_archive->bytes();damaged[3]=2;rejects<std::invalid_argument>([&]{RawPixelArchive::from_bytes(damaged);});damaged.resize(32);rejects<std::invalid_argument>([&]{RawPixelArchive::from_bytes(damaged);});
    // Reach semantic validation with a repaired outer checksum but a changed
    // retained result commitment, rather than testing only checksum rejection.
    damaged=prediction_archive->bytes();const auto position=std::search(damaged.begin(),damaged.end(),result->sha256().begin(),result->sha256().end());check(position!=damaged.end());*position=*position=='a'?'b':'a';damaged.resize(damaged.size()-64);damaged=raw_classifier_checks::checksum(std::move(damaged));rejects<std::invalid_argument>([&]{RawPixelArchive::from_bytes(damaged);});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{archive_raw_pixel_prediction(prediction,result,&cancelled);});rejects<std::runtime_error>([&]{RawPixelArchive::from_bytes(archive->bytes(),&cancelled);});
    const auto directory=std::filesystem::temp_directory_path()/("qpx-raw-archive-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);const auto file=directory/"raw.qxp";
    save_raw_pixel_archive(*prediction_archive,file.u8string());check(load_raw_pixel_archive(file.u8string())->sha256()==prediction_archive->sha256());rejects<std::exception>([&]{save_raw_pixel_archive(*archive,file.u8string());});save_raw_pixel_archive(*archive,file.u8string(),true);rejects<std::runtime_error>([&]{save_raw_pixel_archive(*prediction_archive,file.u8string(),true,&cancelled);});check(load_raw_pixel_archive(file.u8string())->sha256()==archive->sha256());
    {std::ofstream unrelated(file,std::ios::binary|std::ios::trunc);unrelated<<"preserve image";}rejects<std::invalid_argument>([&]{save_raw_pixel_archive(*archive,file.u8string(),true);});{std::ifstream stream(file,std::ios::binary);const std::string content((std::istreambuf_iterator<char>(stream)),{});check(content=="preserve image");}
    const auto row=doc.raw_pixel_record(kept);const auto payload=RawPixelArchiveCodec::encode_record(*row,nullptr);check(RawPixelArchiveCodec::decode_record(payload.data(),payload.size(),doc.metadata(),nullptr)->archive->sha256()==row->archive->sha256());
    const auto prior_revision=doc.revision();doc.update_roi(roi,Roi::rectangle({1,0,2,5}));rejects<std::invalid_argument>([&]{doc.keep_raw_pixel_prediction(prediction,prediction_archive);});check(doc.revision()==prior_revision+1 && doc.raw_pixel_record_count()==2);check(doc.remove_raw_pixel_record(kept) && !doc.remove_raw_pixel_record(kept) && result->sha256()==replay->result()->sha256());
    if(has_tiff() && has_project()) {
        const auto image=directory/"source.tif";simple_tiff(image);auto project=std::make_shared<Project>();const auto image_id=project->add_tiff_image(image.u8string(),{});const auto live=project->document(image_id);const auto left=live->add_detection(Roi::rectangle({0,0,3,6}),"A",0,"A"),right=live->add_detection(Roi::rectangle({4,0,4,6}),"B",0,"B");live->set_calibration(.5,.25);
        const ScientificGrid real_grid(8,6,{0,8,4},{0,6,3});const RawPixelTrainingRequest real_request(*live,{left,right},real_grid,selection,features,options);const auto real_archive=archive_raw_pixel_training(sample_raw_pixel_training(real_request));const auto training_id=live->keep_raw_pixel_training(real_request,real_archive);
        const RawPixelClassifier real_model(raw_classifier_checks::forest(real_request.recipe()),real_request.recipe());const RawPixelPredictionRequest real_prediction(*live,real_model,real_grid,left);const auto real_pred_archive=archive_raw_pixel_prediction(real_prediction,predict_raw_pixel_region(real_prediction));const auto prediction_id=live->keep_raw_pixel_prediction(real_prediction,real_pred_archive);
        const auto project_path=(directory/"project.qpxproj").u8string();project->save(project_path);auto opened=Project::open(project_path);check(opened->document(image_id)->raw_pixel_record_count()==2 && opened->document(image_id)->raw_pixel_record(training_id)->archive->sha256()==real_archive->sha256());check(opened->document(image_id)->raw_pixel_record(prediction_id)->archive->result()->sha256()==real_pred_archive->result()->sha256());
        live->rename_annotation(left,"Left after save");const auto recovery_path=(directory/"project.qpxrecover").u8string();ProjectAutosave autosave;check(autosave.request(*project,recovery_path) && autosave.wait().state=="ready");auto recovered=Project::recover(recovery_path,project_path);check(recovered->document(image_id)->raw_pixel_record_count()==2 && recovered->document(image_id)->raw_pixel_record(prediction_id)->archive->sha256()==real_pred_archive->sha256());recovered.reset();Project::discard_recovery_checked(Project::inspect_recovery(recovery_path));
        check(opened->document(image_id)->remove_raw_pixel_record(training_id));ProjectSaveJob save;check(save.request(opened,project_path));check(save.wait().state=="saved");auto again=Project::open(project_path);check(again->document(image_id)->raw_pixel_record_count()==1);
        auto corrupt_record=RawPixelArchiveCodec::encode_record(*again->document(image_id)->raw_pixel_record(prediction_id),nullptr);corrupt_record.back()^=1;rejects<std::invalid_argument>([&]{RawPixelArchiveCodec::decode_record(corrupt_record.data(),corrupt_record.size(),live->metadata(),nullptr);});
        const RawPixelPredictionRequest captured(*live,real_model,real_grid,left);const auto ready=archive_raw_pixel_prediction(captured,predict_raw_pixel_region(captured));{std::ofstream changed(image,std::ios::binary|std::ios::app);changed<<"changed";}const auto rev=live->revision();rejects<std::invalid_argument>([&]{live->keep_raw_pixel_prediction(captured,ready);});check(live->revision()==rev && live->raw_pixel_record_count()==2);
        // Close readers before removing test-owned files on Windows.
        again.reset();opened.reset();project.reset();live->server()->close();std::filesystem::remove(project_path);std::filesystem::remove(image);
        std::cout<<"Raw pixel archives: real TIFF retention, project save/reopen, snapshot save, stale-file refusal and historical-only decode passed\n";
    }
    std::filesystem::remove(file);std::filesystem::remove(directory);
    std::cout<<"Raw pixel archives: sampling/prediction semantic round trips, rational axes, authority refusal, tamper/cancel and atomic replacement passed\n";
}
}
