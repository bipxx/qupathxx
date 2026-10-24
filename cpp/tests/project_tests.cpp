#include "qupathxx/project.hpp"
#include "qupathxx/cells.hpp"
#include <cstdlib>
#include "qupathxx/object_classifier_archive.hpp"
#include "object_forest_fixture.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("project check failed"); }
int main() {
    try {
        Project p;
        if(!has_project()) {
            bool rejected=false; try { p.save("disabled.qpxproj"); } catch(const std::runtime_error&) { rejected=true; }
            check(rejected); return 0;
        }
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
