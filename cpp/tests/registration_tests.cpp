#include "qupathxx/registration.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/workflow.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void near(double actual,double expected,double tolerance=1e-12) {check(std::abs(actual-expected)<=tolerance,"Coordinate differs from independent expected value");}
template<class E=std::exception,class F> void rejects(F action) {
    try {action();}catch(const E&) {return;}throw std::runtime_error("Expected registration refusal");
}
std::shared_ptr<const ObjectProperties> facts(std::string id,double count) {
    return std::make_shared<ObjectProperties>(std::map<std::string,std::string>{{"omics.profile","qpx.spatial-counts/1"},
        {"omics.matrix_sha256",std::string(64,'a')},{"omics.features_sha256",std::string(64,'b')},
        {"omics.positions_sha256",std::string(64,'c')},{"omics.observation_id",std::move(id)}},
        std::map<std::string,double>{{"omics.count:G",count}});
}
std::shared_ptr<Document> document() {
    auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));
    for(unsigned i=0;i<3;++i) {
        const auto id=doc->add_detection(Roi::points({{double(1+i*2),double(2+i)}}));doc->set_object_properties(id,facts("spot-"+std::to_string(i),i));
    }
    return doc;
}
ExpressionDataset capture(const Document& doc) {
    ExpressionCaptureOptions options;options.coordinate_frame="original-spatial-px";return ExpressionDataset::capture(doc,options);
}
CoordinateFrame source_frame(const ExpressionDataset& dataset) {
    CoordinateFrame frame;frame.id=dataset.coordinate_frame();frame.source_id=dataset.source_id();frame.plane=dataset.plane();return frame;
}
void affine_tests() {
    const Point p{2,3};const Affine2D translation{{1,0,5,0,1,-2}},scale{{2,0,0,0,3,0}},rotation{{0,-1,0,1,0,0}},reflection{{-1,0,0,0,1,0}};
    near(translation.map(p).x,7);near(translation.map(p).y,1);near(scale.map(p).x,4);near(scale.map(p).y,9);
    near(rotation.map(p).x,-3);near(rotation.map(p).y,2);near(reflection.map(p).x,-2);near(reflection.map(p).y,3);
    const auto composed=translation.then(scale);near(composed.map(p).x,14);near(composed.map(p).y,3);
    for(const auto& affine:{translation,scale,rotation,reflection,composed,Affine2D{{1,.25,4,-.5,2,3}}}) {
        const auto restored=affine.inverse().map(affine.map(p));near(restored.x,p.x);near(restored.y,p.y);
        const auto identity=affine.then(affine.inverse());near(identity.map(p).x,p.x);near(identity.map(p).y,p.y);
    }
    const Affine2D large{{1e200,0,0,0,2e200,0}},small{{1e-200,0,0,0,2e-200,0}};
    near(large.inverse().map(large.map(p)).x,p.x);near(small.inverse().map(small.map(p)).y,p.y);
    rejects<std::invalid_argument>([]{Affine2D{{1,2,0,2,4,0}}.validate();});
    rejects<std::invalid_argument>([]{Affine2D{{1,0,0,0,1e-13,0}}.validate();});
    rejects<std::invalid_argument>([]{Affine2D{{1,0,NAN,0,1,0}}.validate();});
    rejects<std::invalid_argument>([]{Affine2D{}.map({INFINITY,0});});
    rejects<std::invalid_argument>([]{Affine2D{{2,0,0,0,2,0}}.map({std::numeric_limits<double>::max(),0});});
}
void units_tests() {
    CoordinateFrame pixel;pixel.id="original-px";pixel.source_id="original";
    auto physical=pixel;physical.id="physical-um";physical.unit=CoordinateUnit::Micrometre;
    rejects<std::invalid_argument>([&]{coordinate_unit_conversion(pixel,physical);});
    pixel.micrometres_per_pixel=std::array<double,2>{.5,2};
    const auto to_um=coordinate_unit_conversion(pixel,physical);near(to_um.map({4,3}).x,2);near(to_um.map({4,3}).y,6);
    const auto to_px=coordinate_unit_conversion(physical,pixel);near(to_px.map({2,6}).x,4);near(to_px.map({2,6}).y,3);
    const auto id=coordinate_unit_conversion(pixel,pixel);near(id.map({4,3}).x,4);
    auto invalid=pixel;invalid.micrometres_per_pixel=std::array<double,2>{0,1};rejects<std::invalid_argument>([&]{invalid.validate();});
    invalid=pixel;invalid.unit=CoordinateUnit::Micrometre;rejects<std::invalid_argument>([&]{invalid.validate();});
}
void request_and_persistence_tests() {
    auto doc=document();const auto dataset=capture(*doc);const auto source=source_frame(dataset);
    const auto target=image_coordinate_frame(*doc,"persistent-image-1");
    RegistrationRequest request(dataset,source,target,Affine2D{{2,0,1,0,3,-2}});
    const auto field=registration_field(request);check(field.positions.size()==3 && field.inside_image==std::vector<std::uint8_t>({1,1,1}),"Registered positions retain cohort");
    near(field.positions[0].x,3);near(field.positions[0].y,4);near(field.positions[2].x,11);near(field.positions[2].y,10);
    check(field.dataset.counts()==dataset.counts() && capture(*doc).sha256()==dataset.sha256(),"Transform never rewrites raw coordinates/counts");
    const auto outside=registration_field(RegistrationRequest(dataset,source,target,Affine2D{{1,0,-20,0,1,0}}));
    check(outside.positions.size()==3 && outside.inside_image==std::vector<std::uint8_t>({0,0,0}),"Outside positions retained without clipping");
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{registration_field(request,&cancelled);});
    rejects<WorkflowCaptureLimit>([&]{RegistrationRequest(dataset,source,target,{},16);});
    auto wrong=source;wrong.id="wrong";rejects<std::invalid_argument>([&]{RegistrationRequest(dataset,wrong,target,{});});
    wrong=source;wrong.source_id="wrong";rejects<std::invalid_argument>([&]{RegistrationRequest(dataset,wrong,target,{});});
    wrong=source;wrong.unit=CoordinateUnit::Micrometre;rejects<std::invalid_argument>([&]{RegistrationRequest(dataset,wrong,target,{});});
    auto calibrated=target;calibrated.micrometres_per_pixel=std::array<double,2>{.5,2};
    RegistrationRequest physical(dataset,wrong,calibrated,coordinate_unit_conversion(wrong,calibrated));
    near(registration_field(physical).positions[0].x,2);near(registration_field(physical).positions[0].y,1);
    ImageDataValue::Map other{{"user-note",ImageDataValue{std::string("keep me")}}};
    doc->set_image_data_properties(std::make_shared<ImageDataProperties>(other));
    const auto before=doc->image_data_properties();accept_registration(*doc,"alignment",request);
    const auto stored=stored_registration(doc->image_data_properties(),"alignment");
    check(stored && stored->sha256()==request.definition().sha256() && doc->image_data_properties()->values().count("user-note"),"Persisted definition and user metadata");
    check(!stored_registration(doc->image_data_properties(),"absent"),"Absent transform is explicit");
    doc->undo();check(doc->image_data_properties()->equals(*before),"Accept uses native undo");doc->redo();
    check(stored_registration(doc->image_data_properties(),"alignment")->sha256()==request.definition().sha256(),"Native redo restores transform");
    auto corrupt=doc->image_data_properties()->values();
    auto& definitions=std::get<ImageDataValue::Map>(corrupt.at("qupathxx.registrations/1").value);
    auto& definition=std::get<ImageDataValue::Map>(definitions.at("alignment").value);
    std::get<ImageDataValue::List>(definition.at("transform").value)[2]=ImageDataValue{7.};
    rejects<std::invalid_argument>([&]{stored_registration(std::make_shared<ImageDataProperties>(corrupt),"alignment");});
    doc->rename_annotation(1,"a display name");accept_registration(*doc,"alignment",request);
    const auto properties_before_stale=doc->image_data_properties();
    doc->set_object_properties(1,facts("spot-0",99));
    rejects<WorkflowInputChanged>([&]{accept_registration(*doc,"alignment",request);});check(doc->image_data_properties()==properties_before_stale,"Stale refusal preserves current transform");doc->undo();
    doc->edit_vertex(1,0,0,{2,2});rejects<WorkflowInputChanged>([&]{accept_registration(*doc,"alignment",request);});doc->undo();
    doc->set_calibration(.5,2);rejects<WorkflowInputChanged>([&]{accept_registration(*doc,"alignment",request);});doc->undo();
    const auto changed=image_coordinate_frame(Document(Image(16,16,std::vector<std::uint8_t>(16*16*3,81))),"persistent-image-1");
    rejects<WorkflowInputChanged>([&]{require_current_registration(request.definition(),dataset,changed);});
    rejects<WorkflowCancelled>([&]{accept_registration(*doc,"cancelled",request,&cancelled);});
    check(!stored_registration(doc->image_data_properties(),"cancelled"),"Cancellation does not publish");
    if(has_project()) {
        Project project;project.add_document(doc);const auto file=fs::current_path()/(project.identity()+"-registration.qpx");project.save_new(file.u8string());
        const auto reopened=Project::open(file.u8string());const auto reopened_doc=reopened->document(1);
        const auto saved=stored_registration(reopened_doc->image_data_properties(),"alignment");
        check(saved && saved->sha256()==request.definition().sha256(),"Save/reopen retains definition");
        require_current_registration(*saved,capture(*reopened_doc),image_coordinate_frame(*reopened_doc,"persistent-image-1"));
        check(capture(*reopened_doc).sha256()==dataset.sha256(),"Save/reopen original source coordinates/counts remain exact");
    }
}
void linked_source_tests() {
    if(!has_project())return;
    Project project;const auto directory=fs::current_path()/(project.identity()+"-registration-files");fs::create_directory(directory);
    const auto path=directory/"source.ppm";
    const auto write=[&](char pixel) {std::ofstream file(path,std::ios::binary);file<<"P6\n2 2\n255\n"<<std::string(12,pixel);};write('a');
    const auto image=project.add_image(path.u8string(),"ppm");const auto doc=project.document(image);
    const auto first=image_coordinate_frame(*doc,"linked-image");
    const auto file=directory/"linked.qpx";project.save_new(file.u8string());const auto reopened=Project::open(file.u8string());
    check(image_coordinate_frame(*reopened->document(1),"linked-image").sha256()==first.sha256(),"Linked/lazy reopen excludes process generations");
    write('b');rejects<WorkflowInputChanged>([&]{image_coordinate_frame(*doc,"linked-image");});
}
int main(int argc,char** argv) {try {
    affine_tests();units_tests();request_and_persistence_tests();linked_source_tests();
    if(argc>=2) {
        auto project=Project::open_without_journal_recovery(argv[1]);auto doc=project->document(1);
        const auto dataset=capture(*doc);RegistrationRequest request(dataset,source_frame(dataset),image_coordinate_frame(*doc,project->identity()+":1"),{});
        const auto field=registration_field(request);check(field.positions.size()==2702,"Real demo transform covers all spots");
        accept_registration(*doc,"real-demo-identity",request);check(capture(*doc).sha256()==dataset.sha256(),"Real demo positions/counts stay exact");
    }
    if(argc>=3) {
        auto project=Project::open_without_journal_recovery(argv[2]);auto doc=project->document(1);
        const auto frame=image_coordinate_frame(*doc,"real-fluorescence");const auto original=doc->server()->tiff_view();
        check(bool(original),"Real fluorescence example uses the admitted TIFF reader");
        auto display=*original;display.lower[0]=0;display.upper[0]=255;doc->set_tiff_view(display);
        check(image_coordinate_frame(*doc,"real-fluorescence").sha256()==frame.sha256(),"TIFF display windows do not invalidate coordinate registration");
    }
    std::cout<<"Registration: analytical affine/inverse/units, native point mapping, stale input refusal, undo and save/reopen passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
