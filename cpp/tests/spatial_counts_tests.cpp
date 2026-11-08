#include "qupathxx/spatial_counts.hpp"
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value) {if(!value)throw std::runtime_error("Spatial count check failed");}
template<class E=std::exception,class F>void rejects(F action) {try {action();}catch(const E&){return;}throw std::runtime_error("Expected spatial count rejection");}
void write(const fs::path& path,const std::string& text) {std::ofstream file(path,std::ios::binary);file<<text;file.close();if(!file)throw std::runtime_error("Fixture write failed");}
std::string bytes(const fs::path& path) {std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
const std::string matrix="%%MatrixMarket matrix coordinate integer general\n% independently specified counts\n3 3 4\n2 1 2\n1 2 9007199254740992\n3 2 99\n1 1 5\n";
const std::string features="feature_id\nG1\nG2\nG3\n";
const std::string positions="observation_id\tx_px\ty_px\nA\t1.5\t2\nB\t3\t4.5\nZero\t0\t0\n";
void prepare(const fs::path& root) {
    if(!fs::create_directory(root))throw std::runtime_error("Fixture directory must be new");
    Project project;auto doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));
    doc->add_rectangle({0,0,1,1},"existing");project.add_document(doc);
    project.save_new((root/"input.qpx").u8string());write(root/"counts.mtx",matrix);write(root/"features.tsv",features);write(root/"positions.tsv",positions);
}
void verify(const fs::path& path) {
    auto project=Project::open(path.u8string());auto doc=project->document(1);
    check(doc->object_count()==4 && doc->annotation(1).name=="existing");
    const std::vector<std::string> names{"A","B","Zero"};
    const double g1[]={5,9007199254740992.,0},g2[]={2,0,0};
    for(std::size_t i=0;i<3;++i) {
        const auto object=doc->annotation(i+2);check(object.name==names[i] && object.geometry.kind()==RoiKind::Points && object.plane==ObjectPlane(-1,0,0));
        check(object.properties->metadata().at("omics.observation_id")==names[i]);
        const auto& values=object.properties->measurements();check(values.size()==2 && values.at("omics.count:G1")==g1[i] && values.at("omics.count:G2")==g2[i]);
        check(object.properties->metadata().at("omics.matrix_sha256").size()==64);
    }
    check(doc->annotation(2).geometry.centroid().x==1.5 && doc->annotation(3).geometry.centroid().y==4.5);
}
int main(int argc,char** argv) {try {
    if(argc==3 && std::string(argv[1])=="--prepare") {prepare(fs::u8path(argv[2]));return 0;}
    if(argc==3 && std::string(argv[1])=="--verify") {verify(fs::u8path(argv[2]));return 0;}
    Project identity;const auto root=fs::current_path()/(identity.identity()+"-spatial-counts");prepare(root);
    SpatialCountsOptions options;options.features={"G2","G1"};
    const auto read=[&]{return read_spatial_counts((root/"counts.mtx").u8string(),(root/"features.tsv").u8string(),(root/"positions.tsv").u8string(),options);};
    auto actual=read();check(actual.matrix_features==3 && actual.matrix_entries==4 && actual.observations.size()==3 && actual.features==options.features);
    check(actual.counts==std::vector<std::uint64_t>({2,5,0,9007199254740992ULL,0,0}));
    const auto input_before=bytes(root/"input.qpx");
    const auto imported=[&](const char* name){return import_spatial_counts((root/"input.qpx").u8string(),(root/name).u8string(),1,(root/"counts.mtx").u8string(),(root/"features.tsv").u8string(),(root/"positions.tsv").u8string(),options);};
    check(imported("output.qpx").find("qpx.spatial-counts/1")!=std::string::npos);verify(root/"output.qpx");
    const auto output_before=bytes(root/"output.qpx");rejects<WorkflowOutputExists>([&]{imported("output.qpx");});
    check(bytes(root/"input.qpx")==input_before && bytes(root/"output.qpx")==output_before);
    SpatialImportRequest request;request.input_project=(root/"input.qpx").u8string();request.output_project=(root/"preview.qpx").u8string();
    request.image_id=1;request.matrix=(root/"counts.mtx").u8string();request.features=(root/"features.tsv").u8string();
    request.positions=(root/"positions.tsv").u8string();request.options=options;
    const auto preview=preview_spatial_import(request);
    check(!fs::exists(root/"preview.qpx") && bytes(root/"input.qpx")==input_before);
    check(preview->projection().counts==actual.counts && preview->coordinate_frame()=="image-level-zero-px" && preview->project_sha256().size()==64);
    check(preview->json().find("qpx.spatial-import-preview/1")!=std::string::npos);
    request.options.features={"G3"};check(preview->projection().features==options.features);request.options=options;
    SpatialImportJob job;check(job.publish(preview));job.wait();job.rethrow_error();
    check(job.phase()==SpatialImportPhase::Complete && !job.report().empty());verify(root/"preview.qpx");
    check(Project::open((root/"preview.qpx").u8string())->image_metadata(1).at("omics.import_recipe")==preview->json());
    check(preview->json().find("\"max_entries\":1000000")!=std::string::npos && preview->json().find("\"feature_table\"")!=std::string::npos);
    auto chained=request;chained.input_project=(root/"preview.qpx").u8string();chained.output_project=(root/"chained.qpx").u8string();
    publish_spatial_import(*preview_spatial_import(chained));
    auto chained_project=Project::open(chained.output_project);std::size_t recipes=0;
    for(const auto& item:chained_project->image_metadata(1))if(item.first.find("omics.import_recipe:")==0)++recipes;
    check(recipes==2 && chained_project->document(1)->object_count()==7);
    rejects<WorkflowOutputExists>([&]{publish_spatial_import(*preview);});
    request.output_project=(root/"stale.qpx").u8string();const auto stale=preview_spatial_import(request);
    write(root/"positions.tsv",positions+"extra\t0\t0\n");rejects<WorkflowInputChanged>([&]{publish_spatial_import(*stale);});check(!fs::exists(root/"stale.qpx"));write(root/"positions.tsv",positions);
    request.output_project=(root/"stale-project.qpx").u8string();const auto stale_project=preview_spatial_import(request);
    auto revised=Project::open(request.input_project);revised->rename_image(1,"changed");revised->save(request.input_project);
    rejects<WorkflowInputChanged>([&]{publish_spatial_import(*stale_project);});check(!fs::exists(root/"stale-project.qpx"));
    write(root/"input.qpx",input_before);
    request.output_project=(root/"collision.qpx").u8string();const auto collision=preview_spatial_import(request);
    write(root/"collision.qpx","keep-me");rejects<WorkflowOutputExists>([&]{publish_spatial_import(*collision);});check(bytes(root/"collision.qpx")=="keep-me");
    request.output_project=(root/"async-preview.qpx").u8string();check(job.preview(request));job.wait();job.rethrow_error();
    check(job.phase()==SpatialImportPhase::Ready && job.result() && !fs::exists(root/"async-preview.qpx"));
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{read_spatial_counts("missing","missing","missing",options,&cancelled);});
    rejects<WorkflowCancelled>([&]{preview_spatial_import(request,&cancelled);});
    rejects<WorkflowCancelled>([&]{publish_spatial_import(*job.result(),&cancelled);});check(!fs::exists(root/"async-preview.qpx"));
    rejects<WorkflowCancelled>([&]{import_spatial_counts("missing",(root/"cancelled.qpx").u8string(),1,"missing","missing","missing",options,&cancelled);});check(!fs::exists(root/"cancelled.qpx"));
    options.work_bytes=1;rejects<WorkflowCaptureLimit>(read);options.work_bytes=128u*1024*1024;
    options.file_bytes=1;rejects<WorkflowCaptureLimit>(read);options.file_bytes=32u*1024*1024;
    options.max_entries=3;rejects<WorkflowCaptureLimit>(read);options.max_entries=1000000;
    options.features={"G1","G1"};rejects<std::invalid_argument>(read);options.features={"Absent"};rejects<std::invalid_argument>(read);options.features={"G2","G1"};
    for(const auto& bad:{
        "%%MatrixMarket matrix coordinate real general\n3 3 1\n1 1 1.5\n",
        "%%MatrixMarket matrix coordinate integer general\n3 3 1\n1 1 -1\n",
        "%%MatrixMarket matrix coordinate integer general\n3 3 1\n1 1 9007199254740993\n",
        "%%MatrixMarket matrix coordinate integer general\n3 3 2\n1 1 1\n1 1 2\n",
        "%%MatrixMarket matrix coordinate integer general\n3 3 1\n0 1 2\n",
        "%%MatrixMarket matrix coordinate integer general\n3 3 1\n4 1 2\n",
        "%%MatrixMarket matrix coordinate integer general\n3 3 2\n1 1 1\n",
        "%%MatrixMarket matrix coordinate integer general\n3 3 0\n1 1 1\n"}) {
        write(root/"counts.mtx",bad);rejects<std::invalid_argument>(read);
    }
    write(root/"counts.mtx",matrix);
    write(root/"features.tsv","feature_id\nG1\nG1\nG3\n");rejects<std::invalid_argument>(read);write(root/"features.tsv",features);
    for(const auto& bad:std::vector<std::string>{"observation_id\tx_um\ty_um\nA\t1\t2\n",positions+"extra\t1\t2\n",
        "observation_id\tx_px\ty_px\nA\t1\t2\nA\t2\t3\nZ\t0\t0\n",
        "observation_id\tx_px\ty_px\nA\tnan\t2\nB\t2\t3\nZ\t0\t0\n"}) {
        write(root/"positions.tsv",bad);rejects<std::invalid_argument>(read);
    }
    write(root/"positions.tsv","observation_id\tx_px\ty_px\nA\t8\t2\nB\t2\t3\nZ\t0\t0\n");
    rejects<std::invalid_argument>([&]{imported("outside.qpx");});check(!fs::exists(root/"outside.qpx"));write(root/"positions.tsv",positions);
    options.z=1;rejects<std::invalid_argument>([&]{imported("plane.qpx");});check(!fs::exists(root/"plane.qpx"));options.z=0;
    write(root/"counts.mtx","%%MatrixMarket matrix coordinate integer general\n3 3 4\n1 1 5\n3 2 99\n1 2 9007199254740992\n2 1 2\n");
    const auto reordered=read();check(reordered.counts==actual.counts && reordered.source_sha256[0]!=actual.source_sha256[0]);
    check(bytes(root/"input.qpx")==input_before);
    std::cout<<"Spatial counts: exact selected values, native preview/job/recipe lineage, atomic save/reopen, stale-input/collision refusal, cancellation and budgets passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
