#include "qupathxx/backed_spatial_import.hpp"
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/gene_view.hpp"
#include "qupathxx/project_recovery.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected backed spatial import refusal");}
void write(const fs::path& path,const std::string& text){std::ofstream out(path,std::ios::binary);out<<text;out.close();check(bool(out),"Write backed import fixture");}
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void exercise(const fs::path& root) {
    fs::create_directory(root);write(root/"image.ppm","P6\n8 8\n255\n"+std::string(8*8*3,char(40)));
    write(root/"matrix.mtx","%%MatrixMarket matrix coordinate integer general\n2 3 3\n1 1 9007199254740993\n1 3 5\n2 2 7\n");
    write(root/"features.tsv","feature_id\nA\nB\n");write(root/"barcodes.tsv","observation_id\nX\nY\nZ\n");
    const auto imported=import_expression_matrix((root/"matrix.mtx").u8string(),(root/"features.tsv").u8string(),(root/"barcodes.tsv").u8string(),(root/"source.qmx").u8string());
    const std::string positions="observation_id\tx_px\ty_px\nZ\t1\t2\nX\t3\t4\nY\t5\t6\n";write(root/"positions.tsv",positions);
    BackedSpatialImportRequest request;request.base.image_path=(root/"image.ppm").u8string();request.base.image_id=1;request.base.output_project=(root/"direct.qpx").u8string();
    request.store=imported.path;request.expected_store_sha256=imported.file_sha256;request.positions=(root/"positions.tsv").u8string();
    const auto preview=preview_backed_spatial_import(request);
    check(!fs::exists(request.base.output_project) && !fs::exists(root/".qpx-expression"),"Preview publishes neither project nor owned bytes");
    check(preview->spatial().dataset({1,0}).counts()==std::vector<std::uint64_t>{0,5,0,9007199254740993ull,7,0},"Preview joins full exact counts to scrambled explicit coordinates");
    check(preview->mapped_positions()[1].x==3 && preview->position_sha256().size()==64,"Preview preserves declared coordinates/provenance");
    const auto report=nlohmann::json::parse(publish_backed_spatial_import(*preview));
    auto direct=Project::open(request.base.output_project);auto doc=direct->document(1);
    check(report.at("state")=="saved" && report.at("owned")==true && doc->object_count()==3,"New-image owned import publishes one complete project");
    const auto id=expression_matrix_ids(*direct,1).at(0);const auto owned=load_expression_matrix_association(*direct,1,id);
    check(owned.owned && fs::exists(root/fs::u8path(owned.path)),"First save materializes a canonical owned sidecar");
    for(const auto& [object_id,object]:doc->objects())check(object.properties->measurements().empty(),"Imported points contain no copied double counts");
    fs::rename(root/"source.qmx",root/"source-offline.qmx");
    auto stable=reopen_expression_matrix(*direct,1,id);
    auto captured=ExpressionMatrixSpatialSnapshot::capture(*doc,stable,preview->image_frame().id);
    check(captured.dataset({0}).counts()==std::vector<std::uint64_t>{5,9007199254740993ull,0},"Owned imported project reopens without original matrix store");
    rejects<WorkflowOutputExists>([&]{publish_backed_spatial_import(*preview);});
    request.store=stable->path();request.base.output_project=(root/"rejected.qpx").u8string();
    auto bad=request;bad.expected_store_sha256=std::string(64,'a');rejects<WorkflowInputChanged>([&]{preview_backed_spatial_import(bad);});
    bad=request;bad.expected_base_sha256=std::string(64,'a');rejects<WorkflowInputChanged>([&]{preview_backed_spatial_import(bad);});
    bad=request;bad.alignment.matrix[2]=100;rejects<std::invalid_argument>([&]{preview_backed_spatial_import(bad);});
    bad=request;bad.position_unit=CoordinateUnit::Micrometre;rejects<std::invalid_argument>([&]{preview_backed_spatial_import(bad);});
    bad=request;bad.base.plane=ObjectPlane(-1,1,0);rejects([&]{preview_backed_spatial_import(bad);});
    bad=request;bad.position_options.max_observations=1;rejects<WorkflowCaptureLimit>([&]{preview_backed_spatial_import(bad);});
    for(const auto& invalid:std::vector<std::string>{"observation_id\tx_px\ty_px\nunknown\t1\t2\n","observation_id\tx_px\ty_px\nX\t1\t2\nX\t2\t3\n","observation_id\tx_px\ty_px\nX\t-1\t2\n"}) {
        write(root/"positions.tsv",invalid);rejects<std::invalid_argument>([&]{preview_backed_spatial_import(request);});
        check(!fs::exists(request.base.output_project),"Malformed/unknown coordinate rows cannot publish");
    }
    write(root/"positions.tsv",positions);
    const auto stale=preview_backed_spatial_import(request);write(root/"positions.tsv",positions+"extra\t0\t0\n");
    rejects<WorkflowInputChanged>([&]{publish_backed_spatial_import(*stale);});check(!fs::exists(request.base.output_project),"Changed coordinates refuse publication");write(root/"positions.tsv",positions);
    std::atomic<bool> cancel{true};rejects<WorkflowCancelled>([&]{preview_backed_spatial_import(request,&cancel);});
    const auto cancelled=preview_backed_spatial_import(request);rejects<WorkflowCancelled>([&]{publish_backed_spatial_import(*cancelled,&cancel);});
    const auto collision=preview_backed_spatial_import(request);write(request.base.output_project,"preserve");
    rejects<WorkflowOutputExists>([&]{publish_backed_spatial_import(*collision);});check(bytes(request.base.output_project)=="preserve","Existing output remains unchanged");
    Project base;auto base_doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,70)));base_doc->set_calibration(.5,2);base_doc->add_rectangle({0,0,1,1});base.add_document(base_doc);
    const auto base_file=root/"base.qpx";base.save_new(base_file.u8string());const auto original_base=bytes(base_file);
    write(root/"physical.tsv","id\tx_um\ty_um\nZ\t0.5\t2\nX\t1\t4\n");
    BackedSpatialImportRequest calibrated=request;calibrated.base.image_path.clear();calibrated.base.input_project=base_file.u8string();calibrated.base.output_project=(root/"calibrated.qpx").u8string();
    calibrated.positions=(root/"physical.tsv").u8string();calibrated.position_options.columns={"id","x_um","y_um"};calibrated.position_unit=CoordinateUnit::Micrometre;
    calibrated.alignment.matrix={1,0,2,0,1,1};calibrated.owned=false;
    SpatialImportJob job;check(job.preview(calibrated),"Existing import worker accepts indexed source");job.wait();
    check(job.phase()==SpatialImportPhase::Ready && job.backed_result() && !job.result(),"Typed backed preview uses existing job phases");
    auto physical=job.backed_result();check(physical->mapped_positions()[0].x==3 && physical->mapped_positions()[0].y==2,"Explicit micrometre conversion precedes alignment");
    std::async(std::launch::async,[&]{rejects<std::logic_error>([&]{job.phase();});}).get();
    check(job.publish(physical),"Existing import worker publishes indexed project");job.wait();check(job.phase()==SpatialImportPhase::Complete && job.error().empty(),"Async backed publication completes");
    auto linked=Project::open(calibrated.base.output_project);check(linked->document(1)->object_count()==3 && !load_expression_matrix_association(*linked,1,id).owned,"Saved-project import preserves annotations and linked mode");
    check(bytes(base_file)==original_base,"Import never mutates original project");
    auto chained=calibrated;chained.base.input_project=calibrated.base.output_project;chained.base.output_project=(root/"duplicate.qpx").u8string();
    rejects<WorkflowOutputExists>([&]{preview_backed_spatial_import(chained);});
    chained.alignment.matrix[2]+=.25;chained.owned=true;chained.base.output_project=(root/"upgraded.qpx").u8string();
    const auto next=preview_backed_spatial_import(chained);publish_backed_spatial_import(*next);auto upgraded=Project::open(chained.base.output_project);
    check(expression_matrix_ids(*upgraded,1).size()==1 && load_expression_matrix_association(*upgraded,1,id).owned,"New coordinate scope reuses and upgrades one count association");
    const auto old_scope=ExpressionMatrixSpatialSnapshot::capture(*upgraded->document(1),reopen_expression_matrix(*upgraded,1,id),physical->image_frame().id);
    const auto new_scope=ExpressionMatrixSpatialSnapshot::capture(*upgraded->document(1),reopen_expression_matrix(*upgraded,1,id),next->image_frame().id);
    check(old_scope.observations().size()==2 && new_scope.observations().size()==2 && old_scope.observations()[0].position.x==3 && new_scope.observations()[0].position.x==3.25,"Overlapping barcodes in explicit coordinate scopes never merge");
    const auto conflict=root/"conflict";fs::create_directory(conflict);fs::create_directory(conflict/".qpx-expression");write(conflict/fs::u8path(owned.path),"unrelated");
    auto conflicting=request;conflicting.base.output_project=(conflict/"study.qpx").u8string();const auto conflict_preview=preview_backed_spatial_import(conflicting);
    rejects([&]{publish_backed_spatial_import(*conflict_preview);});check(!fs::exists(conflicting.base.output_project) && bytes(conflict/fs::u8path(owned.path))=="unrelated","Conflicting first-save owned bytes refuse atomic project publication");
    const auto recovery=root/"recovery",recovered_target=root/"recovered";fs::create_directory(recovery);fs::create_directory(recovered_target);
    fs::copy_file(fs::u8path(stable->path()),root/"pending.qmx");auto pending_source=open_expression_matrix((root/"pending.qmx").u8string(),stable->file_sha256());
    Project unsaved;unsaved.add_document(std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,0))));keep_expression_matrix(unsaved,1,pending_source,true);
    ProjectAutosave autosave;check(autosave.request(unsaved,(recovery/"pending.qpxrecover").u8string()),"Pending owned source can enter native recovery snapshot");autosave.wait();
    check(autosave.stats().checkpoint_published,"Unsaved recovery and owned bytes publish together");fs::rename(root/"pending.qmx",root/"pending-offline.qmx");
    auto recovered=Project::recover(autosave.stats().path);check(recovered->path().empty() && reopen_expression_matrix(*recovered,1,id)->read(ExpressionMatrixSelection{{0},std::nullopt}).counts.size()==2,"Unsaved recovery resolves its exact owned origin without original input");
    recovered->save_new((recovered_target/"study.qpx").u8string());fs::rename(recovery/fs::u8path(owned.path),recovery/"recovery-source-offline.qmx");
    check(reopen_expression_matrix(*recovered,1,id)->read(ExpressionMatrixSelection{{0},std::nullopt}).counts[0]==9007199254740993ull,"First primary save detaches recovered owned data from recovery origin");
    for(const auto& entry:fs::recursive_directory_iterator(root))check(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos,"No leaked import/copy temporary");
}
void real(const fs::path& root,const fs::path& store,const fs::path& demo) {
    BackedSpatialImportRequest request;request.base.input_project=(demo/"base.qpx").u8string();request.base.output_project=(root/"mouse-brain-backed.qpx").u8string();request.base.image_id=1;
    request.store=store.u8string();request.expected_store_sha256="79f407bf9b0810f417410961dd79db6a6ce1ff8999e085d1ef8f7865eaca7401";request.positions=(demo/"positions.tsv").u8string();
    const auto preview=preview_backed_spatial_import(request);check(preview->spatial().source()->metadata().features==32285 && preview->mapped_positions().size()==2702,"Full admitted source is associated without selected-gene truncation");
    const auto report=nlohmann::json::parse(publish_backed_spatial_import(*preview));auto project=Project::open(request.base.output_project);
    const auto id=expression_matrix_ids(*project,1).at(0);auto source=reopen_expression_matrix(*project,1,id);
    const auto spatial=ExpressionMatrixSpatialSnapshot::capture(*project->document(1),source,preview->image_frame().id);
    SpatialCountsOptions options;options.features=read_spatial_feature_catalogue((demo/"features.tsv").u8string()).features;
    const auto prior=read_spatial_counts((demo/"counts.mtx").u8string(),(demo/"features.tsv").u8string(),(demo/"positions.tsv").u8string(),options);
    std::vector<std::uint64_t> features;
    for(const auto& feature:options.features){const auto split=feature.find('|');check(split!=std::string::npos,"Independent marker carries stable source ID");const auto index=source->feature_index(feature.substr(split+1));check(bool(index),"Full store retains independent marker");features.push_back(*index);}
    const auto selected=spatial.dataset(features);check(selected.counts()==prior.counts && selected.observations().size()==prior.observations.size(),"Every real six-gene count matches independent retained projection");
    for(std::size_t i=0;i<prior.observations.size();++i)check(selected.observations()[i].id==prior.observations[i].id && selected.observations()[i].position.x==prior.observations[i].position.x && selected.observations()[i].position.y==prior.observations[i].position.y,"Every imported real barcode/coordinate remains exact");
    const auto unprojected=source->feature_page(0,1).at(0);GeneViewSession view;view.bind(spatial);GeneViewOptions display;display.enabled=true;display.genes={unprojected.id,selected.features()[0]};display.compare=true;view.configure(display);view.wait();
    check(view.result() && view.error().empty() && view.result()->fields[0]->legend.measured==2702 && view.result()->fields[1]->legend.measured==2702,"Native gene worker compares an unprojected real gene with a retained marker");
    nlohmann::json evidence={{"features",source->metadata().features},{"observations",2702},{"matrix_entries",source->metadata().entries},{"source_sha256",id},
        {"file_sha256",source->file_sha256()},{"spatial_sha256",spatial.sha256()},{"selected_sha256",selected.sha256()},{"selected_counts_equal",true},
        {"coordinates_equal",true},{"owned",load_expression_matrix_association(*project,1,id).owned},{"unprojected_id",unprojected.id},
        {"gene_request_sha256",view.result()->fields[0]->request_sha256},{"project",request.base.output_project},{"result",report}};
    write(root/"real.json",evidence.dump(2)+"\n");std::cout<<"Real native backed import: all32285 genes retained;2702 coordinates and every six-gene count exact; unprojected gene comparison/reopen passes\n";
}
int main(int argc,char** argv){try{if(!has_project() || !has_expression_matrix_storage())return 0;const auto root=argc==5 && std::string(argv[1])=="--real"?fs::u8path(argv[2]):fs::current_path()/("backed-spatial-import-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);exercise(root/"tiny");if(argc==5 && std::string(argv[1])=="--real")real(root,fs::u8path(argv[3]),fs::u8path(argv[4]));std::cout<<"Native backed import: shared preview/job/publication, typed coordinate joins, exact/owned/relinked persistence, scope/cancel/stale/conflict and first-save recovery pass\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
