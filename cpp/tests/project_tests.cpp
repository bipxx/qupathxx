#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/project_source.hpp"
#include "qupathxx/project_save.hpp"
#include "../project_source_internal.hpp"
#include <fstream>
#include "qupathxx/cells.hpp"
#include <cstdlib>
#include "qupathxx/object_classifier_archive.hpp"
#include "object_forest_fixture.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("project check failed"); }
void verified_relink_handoff() {
    Project project;const auto root=std::filesystem::temp_directory_path()/project.identity();
    std::filesystem::create_directory(root);
    auto ppm=[&](const char* name,int value) {const auto path=root/name;std::ofstream file(path,std::ios::binary);file<<"P6\n2 2\n255\n";for(int i=0;i<12;++i)file.put(char(value));return path;};
    const auto original=ppm("original.ppm",16),candidate=ppm("candidate.ppm",96);
    const auto image=project.add_image(original.u8string(),"ppm");const auto doc=project.document(image);
    const auto roi=doc->add_rectangle({0,0,2,2});doc->set_calibration(.25,.5);
    const auto old=doc->server();const auto generation=old->reader_source()->generation;
    project.relink(image,candidate.u8string());const auto linked=doc->server();const auto source=linked->reader_source();
    check(source->primary && source->scope=="single_file" && source->generation!=generation);
    std::filesystem::remove(candidate);
    check(doc->mean_intensity(roi)==96 && linked->reader_source()->generation==source->generation);
    check(doc->pixel_width_um()==.25 && doc->pixel_height_um()==.5);
    Document old_doc(old);const auto old_roi=old_doc.add_rectangle({0,0,2,2});check(old_doc.mean_intensity(old_roi)==16);
    linked->close();check(old_doc.mean_intensity(old_roi)==16);
    project.relink(image,original.u8string());
    const auto first=project.review_source_verification(image,*fingerprint_file(original.u8string()));
    check(first->comparison()=="unverified" && !first->consumed() && !project.source_verification(image));
    project.apply_source_review(first);const auto baseline=project.source_verification(image);
    check(first->consumed() && baseline && doc->mean_intensity(roi)==16);
    const auto facts=source_file_association_json(*baseline);const auto saved=root/"verified.qpxproj";project.save(saved.u8string());
    const auto restored=Project::open(saved.u8string());check(source_file_association_json(*restored->source_verification(image))==facts);
    check(restored->document(image)->server()->reader_source()->scope=="unopened");
    auto encoded=detail::encode_source_verification(*baseline);check(source_file_association_json(*detail::decode_source_verification(encoded.data(),encoded.size()))==facts);
    auto refuses=[](auto call) {bool rejected=false;try {call();}catch(const std::exception&){rejected=true;}check(rejected);};
    encoded.back()^=1;refuses([&]{detail::decode_source_verification(encoded.data(),encoded.size());});
    ppm("candidate.ppm",96);const auto changed=project.review_source_verification(image,*fingerprint_file(candidate.u8string()));
    check(changed->comparison()=="changed_bytes");refuses([&]{project.apply_source_review(changed);});refuses([&]{project.relink(image,candidate.u8string());});
    check(!changed->consumed() && project.source_verification(image)==baseline);project.apply_source_review(changed,true);check(doc->mean_intensity(roi)==96);
    const auto stale=project.review_source_verification(image,*fingerprint_file(candidate.u8string()));project.clear_source_verification(image);refuses([&]{project.apply_source_review(stale);});
    const auto edited=project.review_source_verification(image,*fingerprint_file(candidate.u8string()));ppm("candidate.ppm",192);refuses([&]{project.apply_source_review(edited);});
    const auto observed_change=project.review_source_verification(image,*fingerprint_file(candidate.u8string()));
    check(observed_change->comparison()=="changed_observed_source" && observed_change->requires_acceptance());
    refuses([&]{project.apply_source_review(observed_change);});project.apply_source_review(observed_change,true);
    std::cout<<"Project source verification: historical round trip, reviewed replacement, stale/file-change refusal and codec checks passed\n";
    std::filesystem::remove_all(root);
    std::cout<<"Verified relink reader handoff, source observation, owned pixels and independent old-owner lifetime passed\n";
}
void object_property_roundtrip() {
    Project project;auto doc=std::make_shared<Document>(Image::demo());auto image=project.add_document(doc);auto object=doc->add_rectangle({1,2,10,12});
    auto properties=std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"note","native"}},std::map<std::string,double>{{"Area",3.5},{"zero",-0.0}});
    doc->set_object_properties(object,properties);doc->set_object_edit_state(object,true,0x123456,.875);properties=doc->annotation(object).properties;const auto path=(std::filesystem::temp_directory_path()/(project.identity()+".qpxproj")).u8string();project.save(path);
    auto restored=Project::open(path)->document(image);check(restored->annotation(object).properties->equals(*properties));
    doc->set_object_properties(object,nullptr);check(doc->undo() && doc->annotation(object).properties==properties && restored->annotation(object).properties->metadata().at("note")=="native");
    std::filesystem::remove(std::filesystem::u8path(path));
    std::cout<<"Project object properties: exact native persistence and independent immutable owner passed\n";
}
int main() {
    try {
        Project p;
        if(!has_project()) {
            bool rejected=false; try { p.save("disabled.qpxproj"); } catch(const std::runtime_error&) { rejected=true; }
            check(rejected); return 0;
        }
        verified_relink_handoff();
        object_property_roundtrip();
        if(const auto fixture=std::getenv("QUPATHXX_HAMAMATSU_FIXTURE");fixture && has_openslide()) {
            auto slide=std::make_shared<Document>(open_slide(fixture));
            const auto roi=slide->add_rectangle({11264,17664,256,256});
            CellParameters parameters;parameters.algorithm=CellAlgorithm::LogWatershed;parameters.threshold=.4;
            parameters.level=0;parameters.tile_size=64;parameters.min_nucleus_pixels=16;parameters.max_nucleus_pixels=512;
            CellRequest request(*slide,roi,StainTransform({{.65,.7,.29},{.07,.99,.11}}),parameters);
            const auto detected=detect_cells(request);check(detected->features().size()>0);
            const auto retained=slide->keep_cells(request,detected);
            Project linked;const auto image=linked.add_document(slide);
            check(!linked.images().front().embedded && linked.images().front().reader=="openslide");
            const auto path=(std::filesystem::current_path()/(linked.identity()+".ndpi.qpxproj")).u8string();
            linked.save(path);const auto restored=Project::open(path)->document(image);
            const auto replay=CellRequest::replay(*restored,retained);
            check(restored->verify_cells(retained,replay,*detect_cells(replay)));
            check(restored->pixel_width_um()==slide->pixel_width_um() && restored->pixel_height_um()==slide->pixel_height_um());
            std::filesystem::remove(std::filesystem::u8path(path));
            std::cout<<"Native NDPI inferred link, LoG archive replay and calibration passed\n";
        }
        auto doc=std::make_shared<Document>(Image::demo()); auto image=p.add_document(doc,"Embedded");
        auto parent=doc->add_rectangle({0,0,100,100},"Parent");
        auto cell=doc->add_cell(Roi::ellipse({10,10,20,20}),Roi::rectangle({15,15,3,3}),"Cell",parent,"Tumor");
        doc->set_calibration(.5,.25); doc->set_class("Tumor",0x123456);
        auto second=p.add_document(std::make_shared<Document>(Image::demo()),"Second");
        p.rename_image(second,"Renamed second");
        ProjectView view{{30,40},2.5,{DisplayMode::Green,5,220,1.7}};
        p.set_view(image,view);
        p.set_active_image(second);
        auto path=(std::filesystem::current_path()/(p.identity()+".qpxproj")).u8string();
        ObjectFeatureOptions features;features.calibrated=false;
        ObjectClassifierJob classifier;const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture());
        check(classifier.predict(ObjectFeatureRequest(*doc,{cell},features),model) && classifier.wait(*doc).state=="ready");classifier.apply(*doc);
        const auto audit=doc->classifier_records().back();
        p.save(path); check(!p.dirty() && p.generation()==1);
        auto loaded=Project::open(path); auto restored=loaded->document(image);
        check(loaded->images().size()==2 && loaded->active_image()==second && loaded->identity()==p.identity());
        check(loaded->images().at(1).name=="Renamed second");
        check(loaded->view(image).has_value() && *loaded->view(image)==view && !loaded->view(second));
        check(restored->annotation(cell).geometry.kind()==RoiKind::Ellipse && restored->annotation(cell).nucleus->area()==9);
        check(restored->children(parent)==std::vector<std::uint64_t>{cell} && restored->object_class("Tumor").color==0x123456);
        check(restored->area_um2(cell)==doc->area_um2(cell) && !restored->can_undo());
        check(restored->mean_intensity(cell)==doc->mean_intensity(cell));
        check(restored->classifier_record_count()==1 && restored->classifier_record(audit->id)->archive->sha256()==audit->archive->sha256());
        check(restored->classifier_record(audit->id)->changed_objects==std::vector<std::uint64_t>{cell});
        doc->rename_annotation(cell,"Saved update"); p.save(path);
        restored->rename_annotation(cell,"Stale update"); bool rejected=false;
        try { loaded->save(path); } catch(const std::runtime_error&) { rejected=true; }
        check(rejected && loaded->dirty() && loaded->generation()==1);
        check(Project::open(path)->document(image)->annotation(cell).name=="Saved update");
        std::filesystem::remove(std::filesystem::u8path(path));
        auto legacy_path=path+".legacy";
        std::filesystem::copy_file(std::filesystem::u8path(QUPATHXX_V1_FIXTURE),std::filesystem::u8path(legacy_path));
        auto legacy=Project::open(legacy_path);
        check(legacy->images().size()==2 && !legacy->view(1));
        check(legacy->document(1)->annotation(2).geometry.kind()==RoiKind::Ellipse);
        check(legacy->document(1)->mean_intensity(1)==60);
        legacy->set_view(1,view); legacy->save(legacy_path);
        check(*Project::open(legacy_path)->view(1)==view);
        std::filesystem::remove(std::filesystem::u8path(legacy_path));
        std::cout<<"Multi-image project round trip, source pixels and stale saves passed; SQLite "<<sqlite_version()<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
