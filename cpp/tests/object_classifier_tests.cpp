#include "qupathxx/object_classifier_archive.hpp"
#include "../object_classifier_archive.hpp"
#include "../sha256.hpp"
#include "object_forest_fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void checked(bool v,int line) {if(!v) throw std::runtime_error("object classifier check failed at line "+std::to_string(line));}
#define check(value) checked((value),__LINE__)
template<class F> void rejects(F f) {try {f();} catch(const std::exception&) {return;}throw std::runtime_error("missing object classifier rejection");}
int main() {try {
    {
        Document d(Image::demo());
        const auto parent=d.add_rectangle({0,0,100,100});
        const auto a=d.add_cell(Roi::rectangle({1,1,10,10}),Roi::rectangle({2,2,2,2}),"Cell",parent,"old");
        const auto b=d.add_detection(Roi::rectangle({20,20,5,5}));
        d.set_calibration(.5,.25);d.set_class("old",0x123456);d.clear_history();
        const auto saved=d.annotation(a);const auto structure=d.structure_revision(),calibration=d.calibration_revision(),revision=d.revision();
        d.apply_classifications({{b,"new"},{a,"new"}},"Batch classes");
        check(d.revision()==revision+1 && d.render_revision()==d.revision() && d.structure_revision()==structure && d.calibration_revision()==calibration);
        check(d.class_count("new")==2 && d.class_count("old")==0 && d.class_count("")==0 && d.undo_label()=="Batch classes");
        const auto changed=d.annotation(a);
        check(changed.geometry.equals(saved.geometry) && changed.nucleus->equals(*saved.nucleus) && changed.parent_id==parent && changed.name==saved.name && changed.cell_feature_link==saved.cell_feature_link);
        Document copied=d,assigned(Image::demo());assigned=d;
        check(d.undo() && d.annotation(a).class_name=="old" && d.annotation(b).class_name.empty() && d.class_count("old")==1 && d.object_class("old").color==0x123456);
        rejects([&]{d.object_class("new");});
        check(copied.undo() && assigned.undo()); // Mutable undo payloads must be independent.
        check(d.redo() && copied.redo() && assigned.redo());
        d.rename_annotation(b,"Renamed");check(d.undo());const auto untouched=d.annotation(b).revision;
        d.apply_classifications({{a,""}});check(!d.can_redo() && d.class_count("new")==1 && d.class_count("")==0);
        check(d.undo() && d.annotation(b).revision==untouched && d.class_count("new")==2);
        d.rename_class("new","renamed");check(d.undo() && d.undo() && d.annotation(a).class_name=="old");
        check(d.redo() && d.redo() && d.annotation(a).class_name=="renamed" && d.undo() && d.annotation(a).class_name=="new");
        const auto before=d.revision(),bytes=d.history_bytes();
        rejects([&]{d.apply_classifications({{a,"next"},{999999,"next"}});});
        rejects([&]{d.apply_classifications({{a,"new"},{a,"new"}});});
        rejects([&]{d.apply_classifications({{a,std::string(4097,'x')}});});
        check(d.revision()==before && d.history_bytes()==bytes && d.annotation(a).class_name=="new");
        d.apply_classifications({{a,"new"}});d.apply_classifications({});check(d.revision()==before && d.can_redo());
        d.set_history_limits(128,1);rejects([&]{d.apply_classifications({{a,"next"}});});
        check(d.revision()==before && d.annotation(a).class_name=="new");
        d.set_history_limits(0,0);d.apply_classifications({{a,"next"}});check(!d.can_undo() && d.class_count("next")==1);
    }
    Document doc(Image(512,512,std::vector<std::uint8_t>(512*512*3,80)));
    std::vector<std::uint64_t> ids;
    for(int i=0;i<8;++i) {const auto side=i<3?2.+i:i<6?20.+i-3:i==6?3.5:21.5;ids.push_back(doc.add_detection(Roi::rectangle({double(i*50),10,side,side}),"Object",0,i<3?"small":i<6?"large":""));}
    ObjectFeatureRequest request(doc,ids);check(request.matches(doc) && request.row_count()==8);
    auto table=extract_object_features(request);check(table->values().size()==40 && table->object_ids()==ids && table->features()[0].unit=="px2");
    check(table->values()[0]==4 && table->values()[1]==8 && table->source_anchor()->object_id==ids.front());
    check(extract_object_features(request)->sha256()==table->sha256());
    auto reversed=ids;std::reverse(reversed.begin(),reversed.end());check(extract_object_features(ObjectFeatureRequest(doc,reversed))->sha256()==table->sha256());
    auto duplicate=ids;duplicate.push_back(ids.front());rejects([&]{ObjectFeatureRequest bad(doc,duplicate);});
    rejects([&]{ObjectFeatureRequest bad(doc,{});});
    Document independent=doc;check(!request.matches(independent));
    std::atomic<bool> cancelled{true};rejects([&]{extract_object_features(request,&cancelled);});
    {
        auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture());auto result=classify_objects(request,model);
        auto archive=archive_object_classifier(result->table(),model,result);
        ObjectClassifierRecord record;record.id=doc.revision()+1;record.name="Prediction audit";record.archive=archive;
        const auto bytes=ObjectClassifierArchiveCodec::encode(record,nullptr);
        auto restored=ObjectClassifierArchiveCodec::decode(bytes.data(),bytes.size(),doc.metadata(),nullptr);
        check(restored->archive->sha256()==archive->sha256() && restored->archive->model()->sha256()==model.sha256() && restored->archive->table()->sha256()==table->sha256());
        check(restored->archive->result()->predictions()[0].votes==result->predictions()[0].votes && restored->archive->table()->body_roi(0).equals(doc.annotation(ids[0]).geometry));
        check(!restored->archive->table()->cell_origin(0) && !restored->archive->table()->cell_feature(0));
        for(const auto length:std::vector<std::size_t>{0,1,4,67,bytes.size()-1}) rejects([&]{ObjectClassifierArchiveCodec::decode(bytes.data(),length,doc.metadata(),nullptr);});
        auto corrupt=bytes;corrupt.back()^=1;rejects([&]{ObjectClassifierArchiveCodec::decode(corrupt.data(),corrupt.size(),doc.metadata(),nullptr);});
        corrupt=bytes;for(int i=0;i<8;++i) corrupt[4+i]=0;Sha256 checksum;checksum.update(corrupt.data(),corrupt.size()-64);const auto hash=checksum.finish();std::copy(hash.begin(),hash.end(),corrupt.end()-64);
        rejects([&]{ObjectClassifierArchiveCodec::decode(corrupt.data(),corrupt.size(),doc.metadata(),nullptr);});
        rejects([&]{ObjectClassifierArchiveCodec::encode(record,&cancelled);});rejects([&]{ObjectClassifierArchiveCodec::decode(bytes.data(),bytes.size(),doc.metadata(),&cancelled);});
        ObjectClassifierJob publication;check(publication.predict(request,model) && publication.wait(doc).state=="ready" && publication.archive());
        const auto before=doc.revision();const auto changed=publication.apply(doc);check(changed.size()==8 && doc.revision()==before+1 && doc.classifier_record_count()==1);
        const auto audit=doc.classifier_records().back();check(audit->id==doc.revision() && audit->applied && audit->changed_objects==changed);
        check(doc.undo() && doc.annotation(ids[0]).class_name=="small" && doc.classifier_record_count()==1);
        check(doc.redo() && doc.annotation(ids[0]).class_name=="B");check(doc.undo());
        check(doc.remove_classifier_record(audit->id) && !doc.remove_classifier_record(audit->id));
        // These mutations retire the original request, preserving strict authority.
        request=ObjectFeatureRequest(doc,ids);table=extract_object_features(request);
    }
    if(has_classifier_training()) {
        ForestParameters p;p.trees=24;p.max_depth=4;
        auto model=train_object_forest(*table,p);check(model.training_rows()==6 && model.classes()==std::vector<std::string>({"large","small"}));
        {
            Document retained=doc;ObjectFeatureRequest training_request(retained,ids);ObjectClassifierJob training;
            check(training.train(training_request,p) && training.wait(retained).state=="ready" && training.archive());
            const auto archive=training.archive();const auto id=training.retain_training(retained,"Training cohort");
            check(training.poll(retained).state=="retained" && retained.classifier_record(id)->archive==archive);
            const auto bytes=ObjectClassifierArchiveCodec::encode(*retained.classifier_record(id),nullptr);
            check(ObjectClassifierArchiveCodec::decode(bytes.data(),bytes.size(),retained.metadata(),nullptr)->archive->sha256()==archive->sha256());
            rejects([&]{retained.keep_classifier_training(training_request,archive);});
        }
        auto result=classify_objects(request,model);check(result->predictions().size()==8 && result->model_sha256()==model.sha256());
        const auto saved=doc.annotation(ids[6]);const auto revision=doc.revision();doc.clear_history();
        auto changed=apply_object_classification(doc,request,*result);check(changed==std::vector<std::uint64_t>({ids[6],ids[7]}) && doc.revision()==revision+1);
        check(doc.annotation(ids[6]).class_name=="small" && doc.annotation(ids[7]).class_name=="large");
        check(doc.annotation(ids[6]).name==saved.name && doc.annotation(ids[6]).geometry.equals(saved.geometry) && doc.annotation(ids[6]).kind==saved.kind);
        check(doc.undo_label()=="Apply object classifier" && doc.undo() && doc.annotation(ids[6]).class_name.empty() && doc.annotation(ids[7]).class_name.empty());
        check(doc.redo() && doc.annotation(ids[7]).class_name=="large");rejects([&]{apply_object_classification(doc,request,*result);});
        auto current=ObjectFeatureRequest(doc,ids);rejects([&]{apply_object_classification(doc,current,*result);});
        ObjectClassifierJob job;check(job.train(current,p));check(!job.train(current,p));check(job.wait(doc).state=="ready" && job.model() && job.table());
        check(job.predict(current,*job.model()));job.cancel();check(job.wait(doc).state=="cancelled" && !job.result() && !job.model());
        check(job.predict(current,model));doc.rename_annotation(ids[0],"Changed");check(job.wait(doc).state=="stale" && !job.result());
        auto fresh=ObjectFeatureRequest(doc,ids);check(job.predict(fresh,model));check(job.wait(doc).state=="ready");job.apply(doc);check(job.poll(doc).state=="applied");rejects([&]{job.apply(doc);});
        auto no_labels=ObjectFeatureRequest(doc,{ids[0]});rejects([&]{train_object_forest(*extract_object_features(no_labels),p);});
    }
    if(has_geos()) {
        Document rgb(Image(16,16,std::vector<std::uint8_t>(16*16*3,0)));const auto parent=rgb.add_rectangle({0,0,16,16});
        CellParameters parameters;parameters.sigma=0;parameters.min_nucleus_pixels=1;parameters.min_seed_distance=0;parameters.cell_expansion=1;
        CellRequest request(rgb,parent,StainTransform({{1,0,0},{0,1,0}}),parameters);const auto detected=detect_cells(request);const auto cells=rgb.publish_cells(request,detected);
        ObjectFeatureOptions options;options.cell_rgb=true;const auto features=ObjectFeatureRequest(rgb,cells,options);const auto table=extract_object_features(features);
        std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:table->features()) schema.emplace_back(f.name,f.unit);
        const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture(schema));const auto result=classify_objects(features,model);
        const auto archive=archive_object_classifier(result->table(),model,result);ObjectClassifierRecord record;record.id=rgb.revision()+1;record.name="Cell measurements";record.archive=archive;
        const auto bytes=ObjectClassifierArchiveCodec::encode(record,nullptr);const auto restored=ObjectClassifierArchiveCodec::decode(bytes.data(),bytes.size(),rgb.metadata(),nullptr);
        const auto origin=restored->archive->table()->cell_origin(0);check(origin && origin->source->object_id==parent && origin->source->input_sha256==detected->nuclei()->input_sha256());
        check(restored->archive->sha256()==archive->sha256() && restored->archive->table()->nucleus_roi(0));
        check(restored->archive->table()->cell_feature(0)->cell_rgb==detected->features()[0].cell_rgb);
    }
    const auto cell=doc.add_cell(Roi::rectangle({20,100,4,4}),Roi::rectangle({21,101,2,2}));doc.set_calibration(2,.5);
    ObjectFeatureOptions options;options.nucleus_shape=true;
    auto calibrated=extract_object_features(ObjectFeatureRequest(doc,{cell},options));
    check(calibrated->features().size()==10 && calibrated->features()[0].unit=="um2");
    const auto& values=calibrated->values();check(values[0]==16 && values[1]==20 && values[2]==8 && values[3]==2 && values[5]==4 && values[6]==10);
    options.calibrated=false;check(extract_object_features(ObjectFeatureRequest(doc,{cell},options))->features()[0].unit=="px2");
    options.cell_rgb=true;rejects([&]{ObjectFeatureRequest bad(doc,{cell},options);});
    std::cout<<"object classifier checks passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
