#include "qupathxx/landmark_registration.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
using namespace qpx;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
bool same_properties(const std::shared_ptr<const ImageDataProperties>& a,const std::shared_ptr<const ImageDataProperties>& b) {return a==b || (a && b && a->equals(*b));}
void near(double a,double b,double tolerance=1e-10) {if(!std::isfinite(a) || std::abs(a-b)>tolerance)throw std::runtime_error("Landmark result differs: "+std::to_string(a)+" expected "+std::to_string(b));}
template<class E=std::exception,class F> void rejects(F action) {try{action();}catch(const E&){return;}throw std::runtime_error("Expected landmark refusal");}
std::vector<LandmarkPair> pairs(const Affine2D& transform) {
    std::vector<LandmarkPair> result;unsigned id=0;
    for(const Point source:std::vector<Point>{{0,0},{4,0},{0,6},{4,6},{1,2}})
        result.push_back({"L"+std::to_string(++id),source,transform.map(source),id==5});
    return result;
}
void matrix_near(const Affine2D& a,const Affine2D& b,double tolerance=1e-10) {for(unsigned i=0;i<6;++i)near(a.matrix[i],b.matrix[i],tolerance);}
void fit_tests() {
    const Affine2D translation{{1,0,5,0,1,-3}},similarity{{0,-2,5,2,0,-3}},reflection{{0,2,5,2,0,-3}},affine{{2,.5,5,-.25,-3,-3}};
    auto landmarks=pairs(translation);landmarks.back().target.x+=3;landmarks.back().target.y+=4;
    const auto translated=fit_landmarks(landmarks,{LandmarkModel::Translation});matrix_near(translated.transform(),translation);
    near(translated.training_rmse(),0);near(*translated.held_out_rmse(),5);near(translated.residuals().back().delta.x,-3);
    matrix_near(fit_landmarks(pairs(similarity),{LandmarkModel::Similarity}).transform(),similarity);
    matrix_near(fit_landmarks(pairs(reflection),{LandmarkModel::Similarity,true}).transform(),reflection);
    matrix_near(fit_landmarks(pairs(affine)).transform(),affine);
    landmarks=pairs(affine);landmarks.pop_back();
    // Alternating corner perturbations are orthogonal to [x,y,1], hence the
    // independently known least-squares affine coefficients stay unchanged.
    for(unsigned i=0;i<4;++i)landmarks[i].target.x+=(i==0 || i==3)?1:-1;
    const auto noisy=fit_landmarks(landmarks);matrix_near(noisy.transform(),affine);near(noisy.training_rmse(),1);
    check(!noisy.held_out_rmse(),"Absent held-out observations must not report zero");
    landmarks=pairs(translation);for(auto& pair:landmarks) {pair.source.x+=1e12;pair.source.y-=1e12;pair.target=translation.map(pair.source);}
    const auto large=fit_landmarks(landmarks);for(const auto& pair:landmarks) {near(large.transform().map(pair.source).x,pair.target.x,.001);near(large.transform().map(pair.source).y,pair.target.y,.001);}
    const auto single=fit_landmarks({{"only",{7,8},{10,12}}},{LandmarkModel::Translation});near(single.transform().matrix[2],3);near(single.transform().matrix[5],4);
    check(fit_landmarks(pairs(affine)).sha256()==fit_landmarks(pairs(affine)).sha256(),"Deterministic fit identity");
}
void refusal_tests() {
    const auto valid=pairs({});
    rejects<std::invalid_argument>([]{fit_landmarks({});});
    rejects<std::invalid_argument>([]{fit_landmarks({{"L1",{0,0},{0,0}}});});
    rejects<std::invalid_argument>([]{fit_landmarks({{"L1",{0,0},{0,0},true}},{LandmarkModel::Translation});});
    rejects<std::invalid_argument>([]{fit_landmarks({{"L1",{0,0},{0,0}},{"L2",{1,1},{1,1}},{"L3",{2,2},{2,2}}});});
    rejects<std::invalid_argument>([]{fit_landmarks({{"L1",{0,0},{0,0}},{"L2",{0,0},{1,1}}},{LandmarkModel::Similarity});});
    auto invalid=valid;invalid[1].id=invalid[0].id;rejects<std::invalid_argument>([&]{fit_landmarks(invalid);});
    invalid=valid;invalid[0].source.x=std::numeric_limits<double>::infinity();rejects<std::invalid_argument>([&]{fit_landmarks(invalid);});
    invalid=valid;invalid[0].target.y=std::numeric_limits<double>::quiet_NaN();rejects<std::invalid_argument>([&]{fit_landmarks(invalid);});
    rejects<std::invalid_argument>([&]{fit_landmarks(valid,{LandmarkModel::Affine,true});});
    rejects<std::invalid_argument>([&]{fit_landmarks(valid,{static_cast<LandmarkModel>(8)});});
    rejects<WorkflowCaptureLimit>([&]{fit_landmarks(valid,{},32);});
    invalid=valid;invalid.reserve(4096);rejects<WorkflowCaptureLimit>([&]{fit_landmarks(std::move(invalid),{},4096);});
    rejects<std::invalid_argument>([&]{fit_landmarks(valid,{},0);});
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{fit_landmarks(valid,{},65536,&cancelled);});
    invalid.clear();for(unsigned i=0;i<1025;++i)invalid.push_back({std::to_string(i),{0,0},{1,1}});
    rejects<std::invalid_argument>([&]{fit_landmarks(invalid,{LandmarkModel::Translation});});
}
std::shared_ptr<Document> make_document() {
    auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));
    for(unsigned i=0;i<3;++i) {
        const auto id=doc->add_detection(Roi::points({{double(1+i*2),double(2+i)}}));
        doc->set_object_properties(id,std::make_shared<ObjectProperties>(std::map<std::string,std::string>{{"omics.profile","qpx.spatial-counts/1"},
            {"omics.matrix_sha256",std::string(64,'a')},{"omics.features_sha256",std::string(64,'b')},{"omics.positions_sha256",std::string(64,'c')},
            {"omics.observation_id","spot-"+std::to_string(i)}},std::map<std::string,double>{{"omics.count:G",double(i)}}));
    }
    return doc;
}
ExpressionDataset capture(const Document& doc) {ExpressionCaptureOptions options;options.coordinate_frame="source-px";return ExpressionDataset::capture(doc,options);}
CoordinateFrame source_frame(const ExpressionDataset& dataset) {CoordinateFrame frame;frame.id=dataset.coordinate_frame();frame.source_id=dataset.source_id();frame.plane=dataset.plane();return frame;}
void persistence_tests(std::shared_ptr<Document> doc,const std::string& image_id) {
    const auto dataset=capture(*doc);const auto before=doc->image_data_properties();
    const auto fit=fit_landmarks(pairs(Affine2D{{1,0,-10,0,1,8}}),{LandmarkModel::Translation});
    const RegistrationRequest request(dataset,source_frame(dataset),image_coordinate_frame(*doc,image_id),fit.transform());
    const auto count=doc->objects().size();accept_landmark_registration(*doc,"alignment",request,fit);
    check(active_registration_id(doc->image_data_properties())==std::optional<std::string>("alignment"),"Accept sets active registration");
    const auto stored=stored_landmark_fit(doc->image_data_properties(),"alignment");check(stored && stored->sha256()==fit.sha256(),"Exact fit evidence retained");
    const auto field=active_registration_field(*doc,dataset);check(field && field->positions.size()==dataset.observations().size(),"Active mapping covers source cohort");
    near(field->positions[0].x,dataset.observations()[0].position.x-10);near(field->positions[0].y,dataset.observations()[0].position.y+8);
    check(capture(*doc).sha256()==dataset.sha256() && doc->objects().size()==count,"Accept preserves raw positions/counts/object IDs");
    doc->undo();check(same_properties(doc->image_data_properties(),before),"One undo restores definition/evidence/active selection");
    check(!active_registration_field(*doc,dataset),"Undo clears active mapping");doc->redo();
    check(stored_landmark_fit(doc->image_data_properties(),"alignment")->sha256()==fit.sha256(),"Redo retains exact fit");
    const auto accepted=doc->image_data_properties();
    rejects<std::invalid_argument>([&]{accept_landmark_registration(*doc,"mismatch",RegistrationRequest(dataset,source_frame(dataset),request.definition().target,{}),fit);});
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{accept_landmark_registration(*doc,"cancelled",request,fit,&cancelled);});
    const auto first=dataset.observations().front();
    doc->edit_vertex(first.object_id,0,0,{first.position.x+1,first.position.y});
    rejects<WorkflowInputChanged>([&]{accept_landmark_registration(*doc,"stale",request,fit);});
    rejects<WorkflowInputChanged>([&]{active_registration_field(*doc,capture(*doc));});doc->undo();
    doc->set_calibration(.5,2);rejects<WorkflowInputChanged>([&]{accept_landmark_registration(*doc,"stale",request,fit);});doc->undo();
    check(doc->image_data_properties()->equals(*accepted),"Refusals never publish");
    using V=ImageDataValue;
    const auto tamper=[&](const char* key,V value) {auto values=accepted->values();auto& record=std::get<V::Map>(std::get<V::Map>(values.at("qupathxx.landmark-registrations/1").value).at("alignment").value);
        record[key]=std::move(value);rejects<std::invalid_argument>([&]{stored_landmark_fit(std::make_shared<ImageDataProperties>(values),"alignment");});};
    tamper("model",V{std::int64_t(1ULL<<32)});tamper("unit",V{std::int64_t(1ULL<<32)});tamper("training_rmse",V{9.});tamper("schema",V{std::string("wrong")});
    if(has_project()) {
        Project project;project.add_document(doc);const auto file=std::filesystem::current_path()/(project.identity()+"-landmarks.qpx");project.save_new(file.u8string());
        const auto reopened=Project::open(file.u8string());const auto saved=reopened->document(1);
        check(stored_landmark_fit(saved->image_data_properties(),"alignment")->sha256()==fit.sha256(),"Save/reopen exact coefficients/residuals, no refit");
        check(capture(*saved).sha256()==dataset.sha256(),"Save/reopen exact original coordinates/counts");
        near(active_registration_field(*saved,capture(*saved))->positions[0].x,field->positions[0].x);
    }
}
int main(int argc,char** argv) {try {
    fit_tests();refusal_tests();persistence_tests(make_document(),"test-image");
    if(argc>1) {const auto project=Project::open_without_journal_recovery(argv[1]);persistence_tests(project->document(1),project->identity()+":1");}
    std::cout<<"Landmarks: independent least-squares/residuals, held-out isolation, rank/budget/cancel, atomic undo and exact save/reopen passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
