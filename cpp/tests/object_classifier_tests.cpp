#include "qupathxx/object_classifier.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool v) {if(!v) throw std::runtime_error("object classifier check failed");}
template<class F> void rejects(F f) {try {f();} catch(const std::exception&) {return;}throw std::runtime_error("missing object classifier rejection");}
int main() {try {
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
    if(has_classifier_training()) {
        ForestParameters p;p.trees=24;p.max_depth=4;
        auto model=train_object_forest(*table,p);check(model.training_rows()==6 && model.classes()==std::vector<std::string>({"large","small"}));
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
    const auto cell=doc.add_cell(Roi::rectangle({20,100,4,4}),Roi::rectangle({21,101,2,2}));doc.set_calibration(2,.5);
    ObjectFeatureOptions options;options.nucleus_shape=true;
    auto calibrated=extract_object_features(ObjectFeatureRequest(doc,{cell},options));
    check(calibrated->features().size()==10 && calibrated->features()[0].unit=="um2");
    const auto& values=calibrated->values();check(values[0]==16 && values[1]==20 && values[2]==8 && values[3]==2 && values[5]==4 && values[6]==10);
    options.calibrated=false;check(extract_object_features(ObjectFeatureRequest(doc,{cell},options))->features()[0].unit=="px2");
    options.cell_rgb=true;rejects([&]{ObjectFeatureRequest bad(doc,{cell},options);});
    std::cout<<"object classifier checks passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
