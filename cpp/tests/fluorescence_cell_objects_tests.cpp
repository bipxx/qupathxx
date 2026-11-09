#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "quantitative_tiff_fixture.hpp"
#include "fluorescence_fixture.hpp"
#include <filesystem>
#include <iostream>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void near(double a,double b){check(std::isfinite(a) && std::abs(a-b)<1e-10,"Fluorescence object differs from independent expected value");}
template<class E=std::exception,class F>void rejects(F action){try{action();}catch(const E&){return;}throw std::runtime_error("Expected fluorescence object refusal");}
void exercise(const fs::path& root) {
    const auto file=root/"source.tif";qpx_test::raw_pixel_tiff_fixture(file,15,9);
    auto doc=std::make_shared<Document>(open_tiff(file.u8string()));const auto parent=doc->add_rectangle({1,1,12,7},"Source ROI");
    RawRegionRequest selection;selection.channels=2;FluorescenceCellOptions options;options.threshold=100;options.cell_expansion=1.5;
    const FluorescenceCellRequest request(*doc,selection,parent,options,{0,0});const auto run=detect_fluorescence_cells(request);
    check(run->raster().measurements.size()==1 && run->raster().measurements[0].nucleus.pixels==14 && run->raster().measurements[0].cell.pixels==27,"Analytical ramp foreground and calibrated-radius body");
    if(!has_geos()){rejects<std::runtime_error>([&]{trace_fluorescence_cells(run);});return;}
    const auto traced=trace_fluorescence_cells(run);check(traced->nuclei()->components().size()==1 && traced->cells()->components().size()==1,"Every raw label becomes native contours");
    near(traced->nuclei()->components()[0].geometry.area(),14);near(traced->cells()->components()[0].geometry.area(),27);
    check(traced->cells()->components()[0].geometry.covers(traced->nuclei()->components()[0].geometry),"Native body contains native nucleus");
    const auto record=keep_fluorescence_cell_run(*doc,request,*run);check(verify_fluorescence_cell_record(*doc,record,request,*run),"Keep native nonempty recipe before publication");
    const auto before=doc->object_count();const auto ids=publish_fluorescence_cells(*doc,request,*traced);
    check(ids.size()==1 && doc->object_count()==before+1,"Publication creates cells in one native command");
    const auto published=doc->annotation(ids[0]);check(published.kind==ObjectKind::Cell && published.nucleus && published.parent_id==parent && !published.cell_feature_link,"Raw cell never acquires an RGB feature-record link");
    const auto& values=published.properties->measurements();near(values.at("fluorescence.nucleus.C0.sum"),1541);near(values.at("fluorescence.cell.C0.mean"),102);near(values.at("fluorescence.cytoplasm.C1.mean"),2513./13);
    check(published.properties->metadata().at("fluorescence.cell_boundary")=="true" && published.properties->metadata().at("fluorescence.nucleus_boundary")=="true","Boundary observations remain explicitly flagged");
    check(verify_fluorescence_cell_publication(*doc,ids[0],request,*run),"New publication verifies against its immutable native run");
    check(verify_fluorescence_cell_publications(*doc,ids,request,*run) && !verify_fluorescence_cell_publications(*doc,{},request,*run),"Bulk verification requires the complete cohort");
    rejects<WorkflowOutputExists>([&]{publish_fluorescence_cells(*doc,request,*traced);});
    doc->undo();check(doc->object_count()==before,"One undo removes the complete cell/properties publication");doc->redo();check(doc->annotation(ids[0]).properties->equals(*published.properties),"Redo preserves exact raw facts and identities");
    const auto replay=replay_fluorescence_cell(*doc,ids[0]);const auto replayed=detect_fluorescence_cells(replay);check(replayed->sha256()==run->sha256() && replayed->context()==run->context() && verify_fluorescence_cell_publication(*doc,ids[0],replay,*replayed),"Exact recipe replay uses fresh capture authority");
    doc->rename_annotation(ids[0],"Display name");check(verify_fluorescence_cell_publication(*doc,ids[0],replay,*replayed),"Cosmetic name does not alter raw scientific observations");
    doc->update_roi(ids[0],Roi::rectangle({1,1,2,2}));check(!verify_fluorescence_cell_publication(*doc,ids[0],replay,*replayed),"Geometry edits invalidate observed publication identity");rejects<std::invalid_argument>([&]{replay_fluorescence_cell(*doc,ids[0]);});doc->undo();
    auto corrupt=doc->annotation(ids[0]).properties->metadata();corrupt["fluorescence.recipe"]="qpx.fluorescence-cell-recipe/1\tbad";
    doc->set_object_properties(ids[0],std::make_shared<ObjectProperties>(corrupt,values));rejects<std::invalid_argument>([&]{replay_fluorescence_cell(*doc,ids[0]);});doc->undo();
    doc->set_object_properties(ids[0],{});check(!verify_fluorescence_cell_publication(*doc,ids[0],replay,*replayed),"Cleared observations do not verify");doc->undo();
    if(has_project()) {Project project;project.add_document(doc);const auto saved=root/"cells.qpx";project.save_new(saved.u8string());const auto reopened=Project::open(saved.u8string());const auto restored=reopened->document(1);
        const auto replay=replay_fluorescence_cell(*restored,ids[0]);const auto result=detect_fluorescence_cells(replay);check(result->sha256()==run->sha256() && verify_fluorescence_cell_publication(*restored,ids[0],replay,*result),"Save/reopen exact source/recipe/cell fact association");
        check(verify_fluorescence_cell_record(*restored,record,replay,*result),"Common recipe/result identity also survives save/reopen");
        check(restored->annotation(ids[0]).geometry.equals(published.geometry) && restored->annotation(ids[0]).properties->equals(*published.properties),"Project stores exact raw geometry/properties");}
    const auto fresh=FluorescenceCellRequest(*doc,selection,parent,options,{0,0});const auto independent=detect_fluorescence_cells(fresh);
    rejects<WorkflowInputChanged>([&]{publish_fluorescence_cells(*doc,fresh,*traced);});
    TissueContourOptions bounded;bounded.work_bytes=128;rejects<std::length_error>([&]{trace_fluorescence_cells(independent,bounded);});
    bounded={};bounded.clip_to_roi=false;rejects<std::invalid_argument>([&]{trace_fluorescence_cells(independent,bounded);});
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{trace_fluorescence_cells(independent,{},&cancelled);});
    rejects<WorkflowCancelled>([&]{publish_fluorescence_cells(*doc,fresh,*traced,"Test",256*1024*1024,&cancelled);});
    const auto counts=doc->object_count();doc->set_calibration(.5,.75);rejects<WorkflowInputChanged>([&]{publish_fluorescence_cells(*doc,request,*traced);});check(doc->object_count()==counts,"Stale publication leaves all objects intact");doc->undo();
    const auto ellipse=doc->add_roi(Roi::ellipse({1.25,1.25,11.5,6.5}),"Fractional ellipse");const auto e_request=FluorescenceCellRequest(*doc,selection,ellipse,options,{0,0});const auto e_run=detect_fluorescence_cells(e_request);const auto e_trace=trace_fluorescence_cells(e_run);
    check(e_trace->cells()->components().size()==e_run->raster().measurements.size(),"Fractional ROI clipping preserves every cell label");
    for(const auto& c:e_trace->cells()->components())check(doc->annotation(ellipse).geometry.covers(c.geometry),"Contours stay inside their fractional source ROI");
}
void empty_records(const fs::path& root) {
    const auto file=root/"empty.tif";qpx_test::raw_pixel_tiff_fixture(file,15,9);auto doc=std::make_shared<Document>(open_tiff(file.u8string()));using V=ImageDataValue;
    doc->set_image_data_properties(std::make_shared<ImageDataProperties>(V::Map{{"user-note",V{std::string("retain")}}}));const auto before=doc->image_data_properties();
    RawRegionRequest r;r.region={0,0,0,15,9};r.channels=2;FluorescenceCellOptions p;p.threshold=10000;
    const FluorescenceCellRequest request(*doc,r,0,p,{0,0});const auto run=detect_fluorescence_cells(request);check(run->raster().measurements.empty(),"Zero-cell analysis is a valid result");
    const auto id=keep_fluorescence_cell_run(*doc,request,*run);check(fluorescence_cell_record_ids(*doc)==std::vector<std::string>{id} && verify_fluorescence_cell_record(*doc,id,request,*run),"Zero-cell recipe/result identity is retained without fabricated objects");
    rejects<WorkflowOutputExists>([&]{keep_fluorescence_cell_run(*doc,request,*run);});doc->undo();check(doc->image_data_properties()->equals(*before),"One undo restores prior image properties");doc->redo();
    remove_fluorescence_cell_record(*doc,id);check(fluorescence_cell_record_ids(*doc).empty() && doc->image_data_properties()->equals(*before),"Remove preserves unrelated user metadata");doc->undo();
    const auto replay=replay_fluorescence_cell_record(*doc,id);const auto checked=detect_fluorescence_cells(replay);check(verify_fluorescence_cell_record(*doc,id,replay,*checked) && checked->sha256()==run->sha256(),"Zero-cell record replays with exact identity");
    auto corrupt=doc->image_data_properties()->values();auto& saved=std::get<V::Map>(std::get<V::Map>(corrupt.at("qupathxx.fluorescence-cell-records/1").value).at(id).value);saved["cells"]=V{std::int64_t(1)};
    doc->set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(corrupt)));rejects<std::invalid_argument>([&]{fluorescence_cell_record_ids(*doc);});doc->undo();
    if(has_project()){Project project;project.add_document(doc);const auto path=root/"empty-record.qpx";project.save_new(path.u8string());const auto restored=Project::open(path.u8string())->document(1);const auto request=replay_fluorescence_cell_record(*restored,id);const auto result=detect_fluorescence_cells(request);
        check(verify_fluorescence_cell_record(*restored,id,request,*result) && !restored->object_count(),"Save/reopen preserves empty outcomes and user metadata without fake cells");}
}
void planes(const fs::path& root) {
    if(!has_geos())return;const auto file=root/"planes.ome.tif";qpx_test::fluorescence_fixture(file,false);TiffOpenOptions opening;opening.view.z=1;Document doc(open_tiff(file.u8string(),opening));
    RawRegionRequest selection;selection.region={0,0,0,2,2};selection.channels=3;selection.z=1;FluorescenceCellOptions p;p.threshold=20000;
    const FluorescenceCellRequest request(doc,selection,0,p,{0,0});const auto run=detect_fluorescence_cells(request);const auto traced=trace_fluorescence_cells(run);const auto ids=publish_fluorescence_cells(doc,request,*traced);
    check(doc.annotation(ids[0]).plane==ObjectPlane(-1,1,0) && verify_fluorescence_cell_publication(doc,ids[0],request,*run),"Publication uses captured Z/T, including source-only runs");
    check(!doc.annotation(ids[0]).properties->measurements().count("fluorescence.cytoplasm.C0.mean") && doc.annotation(ids[0]).properties->metadata().at("fluorescence.cytoplasm")=="absent","Empty cytoplasm persists as absent, not zero");
}
int main(int argc,char** argv){try {
    if(!has_tiff()){std::cout<<"Fluorescence object tests: TIFF capability disabled\n";return 0;}
    const auto root=fs::current_path()/("fluorescence-objects-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Unique test directory");exercise(root);planes(root);empty_records(root);
    if(argc>1 && has_geos()){Document doc(open_tiff(argv[1]));const auto m=doc.server()->raw_metadata();RawRegionRequest r;r.region={0,0,0,m->width,m->height};r.channels=m->channels;FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.min_nucleus_area=5;p.cell_expansion=3;
        const FluorescenceCellRequest request(doc,r,0,p,{0,0});const auto run=detect_fluorescence_cells(request);const auto traced=trace_fluorescence_cells(run);const auto ids=publish_fluorescence_cells(doc,request,*traced);
        check(ids.size()==run->raster().measurements.size() && verify_fluorescence_cell_publications(doc,ids,request,*run),"Every admitted real raw label publishes and verifies with one shared currentness check");std::cout<<"Real native cell publication: "<<ids.size()<<" labels; annotation acceptance remains pending\n";}
    std::cout<<"Fluorescence objects: typed contours, raw facts/recipe, atomic publication/undo, native replay/save/reopen and stale/cancel/budget checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
