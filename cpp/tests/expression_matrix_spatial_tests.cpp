#include "qupathxx/expression_matrix_spatial.hpp"
#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/gene_view.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <future>
#include <limits>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected spatial matrix refusal");}
void write(const fs::path& path,const std::string& text){std::ofstream out(path,std::ios::binary);out<<text;out.close();check(bool(out),"Write spatial matrix fixture");}
void exercise(const fs::path& root) {
    write(root/"matrix.mtx","%%MatrixMarket matrix coordinate integer general\n2 3 3\n1 1 9007199254740993\n1 3 5\n2 2 7\n");
    write(root/"features.tsv","feature_id\nA\nB\n");write(root/"observations.tsv","observation_id\nX\nY\nZ\n");
    const auto imported=import_expression_matrix((root/"matrix.mtx").u8string(),(root/"features.tsv").u8string(),(root/"observations.tsv").u8string(),(root/"source.qmx").u8string());
    auto source=open_expression_matrix(imported.path,imported.file_sha256);
    check(source->observation_indices({"Z","X","Y","X"})==std::vector<std::uint64_t>{2,0,1,0},"Batched exact ID lookup preserves order and repeated lookups");
    rejects<std::invalid_argument>([&]{source->observation_indices({"absent"});});
    CoordinateFrame frame;frame.id="declared-coordinates";frame.source_id="explicit-coordinate-fixture";
    std::vector<ExpressionObservation> observations{{40,"Z",{9,8}},{12,"X",{1,2}},{13,"Y",{3,4}}};
    auto rows=ExpressionMatrixSpatialSnapshot::create(source,observations,frame);
    auto projected=std::async(std::launch::async,[&]{return rows.dataset({1,0});}).get();
    check(projected.features()==std::vector<std::string>{"B","A"} && projected.counts()==std::vector<std::uint64_t>{0,5,0,9007199254740993ull,7,0},"Selected feature/coordinate row order joins exact uint64 counts");
    check(projected.observations()[0].object_id==40 && projected.observations()[0].position.x==9 && projected.observations()[1].id=="X","Explicit geometry and object IDs retained");
    check(projected.valid()==std::vector<std::uint8_t>(6,1),"Implicit sparse zeros are measured, never missing");
    auto physical=frame;physical.unit=CoordinateUnit::Micrometre;
    const auto physical_rows=ExpressionMatrixSpatialSnapshot::create(source,observations,physical);
    check(physical_rows.dataset({1,0}).sha256()!=projected.sha256() && projected.source_id()==rows.sha256(),"Declared coordinate units/provenance participate in projected scientific identity");
    Document target(Image(16,16,std::vector<std::uint8_t>(16*16*3,0)));
    const auto registered=registration_field(RegistrationRequest(projected,rows.dataset_frame(),image_coordinate_frame(target,"target"),Affine2D{}));
    check(registered.positions[0].x==9 && registered.positions[1].y==2,"Joined dataset/frame reuse the native registration contract");
    fs::rename(root/"matrix.mtx",root/"matrix-offline.mtx");
    check(rows.dataset({0}).counts()==std::vector<std::uint64_t>{5,9007199254740993ull,0},"Selected gene reads do not need original matrix text");
    const auto empty=ExpressionMatrixSpatialSnapshot::create(source,{},frame).dataset({0});
    check(empty.counts().empty() && empty.observations().empty(),"Explicit empty spatial cohort remains empty");
    rejects<std::invalid_argument>([&]{rows.dataset({0,0});});
    rejects<WorkflowCaptureLimit>([&]{rows.dataset({0},4u*1024*1024);});
    auto bad=observations;bad[1].id="Z";rejects<std::invalid_argument>([&]{ExpressionMatrixSpatialSnapshot::create(source,bad,frame);});
    bad=observations;bad[1].object_id=40;rejects<std::invalid_argument>([&]{ExpressionMatrixSpatialSnapshot::create(source,bad,frame);});
    bad=observations;bad[1].id="unknown";rejects<std::invalid_argument>([&]{ExpressionMatrixSpatialSnapshot::create(source,bad,frame);});
    bad=observations;bad[1].position.x=std::numeric_limits<double>::infinity();rejects<std::invalid_argument>([&]{ExpressionMatrixSpatialSnapshot::create(source,bad,frame);});
    std::atomic<bool> cancel{true};rejects<WorkflowCancelled>([&]{rows.dataset({0},64u*1024*1024,&cancel);});
    GeneViewSession view;view.bind(rows);GeneViewOptions options;options.enabled=true;options.genes={"A","B"};view.configure(options);view.wait();
    check(view.result() && view.error().empty() && view.dataset()->features()==std::vector<std::string>{"A"} && view.result()->fields[0]->legend.zeros==1,"Existing native gene view reads one selected backed gene");
    check(view.result()->fields[0]->dataset.counts()[1]==9007199254740993ull,"Presentation retains exact raw tooltip counts above double integer precision");
    options.compare=true;options.shared_range=true;view.configure(options);view.wait();
    check(view.result() && view.result()->shared_range && view.result()->fields[1]->feature=="B" && view.dataset()->features().size()==2,"Backed comparison uses existing shared legend service");
    for(int i=0;i<12;++i){options.compare=false;options.genes[0]=i%2?"B":"A";view.configure(options);view.poll();}
    view.wait();check(view.result() && view.result()->fields[0]->feature=="B" && view.dataset()->features()==std::vector<std::string>{"B"},"Latest generation wins repeated backed gene changes");
    options.genes[0]="unknown";view.configure(options);view.wait();check(!view.result() && !view.error().empty(),"Absent backed gene yields a visible native error");
    options.genes[0]="A";view.configure(options);view.poll();view.clear();view.wait();
    check(!view.result() && !view.dataset() && !view.spatial_snapshot(),"Cleared/replaced owner cannot receive pending backed results");
    view.bind(projected);view.wait();check(view.result() && !view.spatial_snapshot(),"Legacy selected snapshots still bind after backed mode");
    Project project;auto doc=std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,25)));project.add_document(doc);
    const auto id=keep_expression_matrix(project,1,source);const std::string frame_id=project.identity()+":1";
    const auto image_frame=image_coordinate_frame(*doc,frame_id);std::vector<Annotation> points;
    for(std::size_t i=0;i<observations.size();++i) {
        Annotation point{};point.id=i+1;point.kind=ObjectKind::Detection;point.name=observations[i].id;
        point.geometry=Roi::points({Point(double(i%2),double(i/2))});
        point.properties=expression_matrix_point_properties(id,observations[i].id,image_frame);
        check(point.properties->measurements().empty(),"Backed point stores no duplicated/raw-double counts");points.push_back(std::move(point));
    }
    const auto created=doc->append_objects(points,{},"Import backed observations");
    auto captured=ExpressionMatrixSpatialSnapshot::capture(*doc,source,frame_id);
    const auto expected=captured.dataset({0,1}).sha256();
    check(captured.observations()[0].id=="Z" && captured.dataset({0}).counts()[0]==5,"Owner capture joins point barcode instead of object/axis order");
    rejects<std::invalid_argument>([&]{remove_expression_matrix(project,1,id);});
    doc->set_calibration(.5,.5);rejects<WorkflowInputChanged>([&]{ExpressionMatrixSpatialSnapshot::capture(*doc,source,frame_id);});doc->undo();
    check(ExpressionMatrixSpatialSnapshot::capture(*doc,source,frame_id).sha256()==captured.sha256(),"Calibration Undo restores exact spatial source/frame association");
    rejects<std::invalid_argument>([&]{ExpressionMatrixSpatialSnapshot::capture(*doc,source,frame_id,ObjectPlane(0,0,0));});
    project.save_new((root/"study.qpx").u8string());auto reopened=Project::open((root/"study.qpx").u8string());
    auto again=ExpressionMatrixSpatialSnapshot::capture(*reopened->document(1),reopen_expression_matrix(*reopened,1,id),frame_id);
    check(again.sha256()==captured.sha256() && again.dataset({0,1}).sha256()==expected,"Point/frame/source/selected-count identities survive project reopen");
    doc->remove_objects(created);remove_expression_matrix(project,1,id);doc->undo();doc->undo();
    check(expression_matrix_points_reference(*doc,id) && expression_matrix_ids(project,1)==std::vector<std::string>{id},"Observation removal and association removal Undo retain reference integrity");
    view.bind(rows);options.genes={"A","B"};view.configure(options);
    {std::ofstream changed(root/"source.qmx",std::ios::binary|std::ios::app);changed.put('x');changed.close();check(bool(changed),"Actually mutate the backed file before the pending query");}
    view.wait();check(!view.result() && !view.error().empty(),"Changed backed file cannot publish a stale gene result");
}
int main(){try{if(!has_project() || !has_expression_matrix_storage())return 0;const auto root=fs::current_path()/("expression-spatial-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);exercise(root);std::cout<<"Native spatial matrix: exact ID joins, bounded selected values, point/frame persistence, protected references and coalesced gene display pass\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
