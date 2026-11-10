#include "qupathxx/h5ad.hpp"
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/backed_spatial_import.hpp"
#include "qupathxx/expression_matrix_spatial.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <map>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F fn){bool refused=false;try{fn();}catch(const E&){refused=true;}check(refused,"Expected indexed H5AD refusal");}
H5adOptions options(bool continuous=false){H5adOptions p;p.domain=continuous?ExpressionValueDomain::Continuous:ExpressionValueDomain::Counts;p.value_domain=continuous?"explicit-processed-X":"raw-counts";p.frame.id="authored-spatial";p.frame.source_id="authored-fixture";p.frame.unit=CoordinateUnit::Micrometre;return p;}
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void no_stage(const fs::path& root){for(const auto& e:fs::directory_iterator(root))check(e.path().filename().u8string().rfind(".qpx-expression-",0)!=0 && e.path().filename().u8string().find(".qpx-tmp-")==std::string::npos,"Failed import leaked private staging");}
void sql(const fs::path& path,const char* text){sqlite3* db=nullptr;check(sqlite3_open(path.u8string().c_str(),&db)==SQLITE_OK,"Open own corrupt sidecar");const auto rc=sqlite3_exec(db,text,nullptr,nullptr,nullptr);const auto error=std::string(sqlite3_errmsg(db));sqlite3_close(db);if(rc!=SQLITE_OK)throw std::runtime_error(error);}
void values(const ExpressionSparseBlock& b,bool continuous,const std::vector<std::uint64_t>& cohort){
    const std::uint64_t counts[3][4]={{0,7,0,9007199254740993ULL},{3,0,5,0},{0,11,0,0}};
    const double reals[3][4]={{0,-1.25,0,2.5},{.125,0,3.75,0},{0,11,0,0}};
    check(b.domain==(continuous?ExpressionValueDomain::Continuous:ExpressionValueDomain::Counts) && (continuous?b.counts.empty():b.continuous.empty()),"Sparse block confused value domains");
    for(std::size_t f=0;f<b.features.size();++f){auto at=b.offsets[f];for(auto row:cohort){bool present=at<b.offsets[f+1] && b.observation_indices[at]==row;if(continuous){const auto v=present?b.continuous[at]:0;check(v==reals[row][b.features[f]],"Continuous value/cohort/axis changed");}else{const auto v=present?b.counts[at]:0;check(v==counts[row][b.features[f]],"Exact count/cohort/axis changed");}if(present)++at;}check(at==b.offsets[f+1],"Sparse block retained an unselected row");}
}
std::shared_ptr<const ExpressionMatrixSource> exercise(const fs::path& root){
    const fs::path fixtures=QPX_H5AD_FIXTURES;ExpressionMatrixImportOptions limits;limits.work_bytes=64u*1024*1024;std::shared_ptr<const ExpressionMatrixSource> last;
    for(const std::string encoding:{"array","csr_matrix","csc_matrix"})for(bool continuous:{false,true}){
        const auto name=encoding+(continuous?"-continuous":"");const auto input=root/(name+".h5ad"),store=root/(name+".qmx");fs::copy_file(fixtures/(name+".h5ad"),input);const auto before=bytes(input);const auto p=options(continuous);
        const auto imported=import_h5ad_matrix(input.u8string(),store.u8string(),p,limits);check(imported.peak_work_bytes<=limits.work_bytes && imported.matrix_bytes==before.size(),"H5AD decoder/writer did not share declared work/file bounds");
        auto source=open_expression_matrix(imported.path,imported.file_sha256,limits.work_bytes);const auto& m=source->metadata();
        check(m.input_matrix=="X" && m.input_spatial=="obsm/spatial","Original H5AD dataset keys were not retained");check(m.domain==*p.domain && m.value_domain==p.value_domain && m.spatial_frame && m.spatial_frame->sha256()==p.frame.sha256() && m.spatial_sha256.size()==64,"Indexed interpretation/coordinate frame was not retained");
        check(m.source_sha256==imported.metadata.source_sha256 && m.features==4 && m.observations==3 && m.entries==(encoding=="array"?12u:5u),"Indexed source identity/axes/entry count differ");
        check(source->observation_page(0,3)==std::vector<std::string>{"spot_c","spot_a","spot_b"} && source->feature_page(2,1).at(0).id==u8"gene_\u03b1" && source->feature_index(u8"gene_\u03b1")==2,"Indexed UTF8 catalogue order differs");
        values(source->read({{3,1,0,2},std::nullopt}),continuous,{0,1,2});const auto selected=source->read({{3,1},std::vector<std::uint64_t>{0,2}});values(selected,continuous,{0,2});
        const auto positions=source->spatial_page(0,3);check(positions[0].id=="spot_c" && positions[0].position.x==100.25 && positions[0].position.y==-.5 && positions[2].position.y==12.5,"Embedded source coordinates changed");
        const auto snapshot=ExpressionMatrixSpatialSnapshot::create(source,positions,*m.spatial_frame,limits.work_bytes);const auto projected=snapshot.dataset({1,3},limits.work_bytes);
        check(projected.domain()==*p.domain && projected.features()==std::vector<std::string>{"gene_a","gene_b"},"Spatial projection lost typed domain/axes");
        if(continuous)check(projected.continuous()==std::vector<double>{-1.25,2.5,0,0,11,0},"Indexed continuous spatial projection differs");else check(projected.counts()==std::vector<std::uint64_t>{7,9007199254740993ULL,0,0,11,0},"Indexed count projection lost integer precision");
        check(bytes(input)==before,"Original H5AD input mutated");fs::remove(input);const auto reopened=open_expression_matrix(imported.path,imported.file_sha256,limits.work_bytes);check(reopened->read({{3,1},std::vector<std::uint64_t>{0,2}}).sha256==selected.sha256 && reopened->spatial_page(0,1).at(0).position.y==-.5,"Backed data required the original H5AD input");last=source;
        rejects<WorkflowOutputExists>([&]{import_h5ad_matrix((fixtures/(name+".h5ad")).u8string(),store.u8string(),p,limits);});
    }
    const auto old=open_expression_matrix((fixtures/"schema2.qmx").u8string());check(old->metadata().input_matrix.empty() && old->metadata().spatial_frame && old->spatial_page(0,1).at(0).position.y==-.5,"Historical schema2 coordinates no longer read");values(old->read({{0,1,2,3},std::nullopt}),false,{0,1,2});
    auto layer_options=options();layer_options.matrix="layers/counts";const auto layered=import_h5ad_matrix((fixtures/"selected-layer.h5ad").u8string(),(root/"layer.qmx").u8string(),layer_options,limits);auto layer_source=open_expression_matrix(layered.path,layered.file_sha256);const auto& recipe=layer_source->metadata();check(recipe.input_matrix=="layers/counts" && recipe.input_spatial=="obsm/spatial","Nondefault layer selection lost on reopen");H5adOptions replay;replay.matrix=recipe.input_matrix;replay.spatial=recipe.input_spatial;replay.domain=recipe.domain;replay.value_domain=recipe.value_domain;replay.frame=*recipe.spatial_frame;H5adReader replayed((fixtures/"selected-layer.h5ad").u8string(),replay);check(replayed.metadata().source_sha256==recipe.input_sha256[1],"Persisted native H5AD recipe cannot be replayed exactly");
    auto calibrated=options();calibrated.frame.id=u8"\u03c6-calibrated";calibrated.frame.unit=CoordinateUnit::Pixel;calibrated.frame.plane=ObjectPlane(-1,2,3);calibrated.frame.image_level_zero=true;calibrated.frame.width=1024;calibrated.frame.height=2048;calibrated.frame.micrometres_per_pixel=std::array<double,2>{.375,.625};
    const auto framed=import_h5ad_matrix((fixtures/"array.h5ad").u8string(),(root/"calibrated.qmx").u8string(),calibrated,limits);check(open_expression_matrix(framed.path,framed.file_sha256)->metadata().spatial_frame->sha256()==calibrated.frame.sha256(),"Coordinate frame Unicode/plane/calibration/extent did not round trip exactly");
    for(int failure=0;failure<4;++failure){const auto store=root/("refused-"+std::to_string(failure)+".qmx");auto bounded=limits;std::atomic<bool> cancelled{failure==3};if(failure==0)bounded.work_bytes=16u*1024*1024;if(failure==1)bounded.store_bytes=4096;const auto input=fixtures/(failure==2?"duplicate-coordinate.h5ad":"array.h5ad");rejects([&]{import_h5ad_matrix(input.u8string(),store.u8string(),options(),bounded,&cancelled);});check(!fs::exists(store),"Failed/cancelled H5AD import advertised partial output");no_stage(root);}
    for(int failure=0;failure<5;++failure){const auto path=root/("corrupt-"+std::to_string(failure)+".qmx");fs::copy_file(last->path(),path);
        if(failure==0){sql(path,"UPDATE counts SET value=x'000000000000f87f' WHERE feature=1 AND observation=0");auto corrupt=open_expression_matrix(path.u8string());rejects([&]{corrupt->read({{1},std::vector<std::uint64_t>{2}});});}
        if(failure==1){sql(path,"UPDATE coordinates SET x=x'000000000000f03f' WHERE observation=0");auto corrupt=open_expression_matrix(path.u8string());rejects([&]{corrupt->spatial_page(0,1);});}
        if(failure==2){sql(path,"UPDATE metadata SET value='counts' WHERE key='value_kind'");rejects([&]{open_expression_matrix(path.u8string());});}
        if(failure==3){sql(path,"UPDATE metadata SET value='{}' WHERE key='coordinate_frame'");rejects([&]{open_expression_matrix(path.u8string());});}
        if(failure==4){sql(path,"UPDATE metadata SET value='layers/other' WHERE key='input_matrix'");rejects([&]{open_expression_matrix(path.u8string());});}
    }
    return last;
}
void project_route(const fs::path& root,std::shared_ptr<const ExpressionMatrixSource> source){
    Project base;auto doc=std::make_shared<Document>(Image(256,256,std::vector<std::uint8_t>(256*256*3,30)));base.add_document(doc);doc->set_calibration(.5,.5);base.save_new((root/"base.qpx").u8string());
    BackedSpatialImportRequest request;request.base.input_project=(root/"base.qpx").u8string();request.base.image_id=1;request.base.output_project=(root/"imported.qpx").u8string();request.store=source->path();request.expected_store_sha256=source->file_sha256();request.alignment.matrix={1,0,2,0,1,2};
    SpatialImportJob job;check(job.preview(request,source),"Submit embedded-coordinate preview");job.wait();if(job.phase()!=SpatialImportPhase::Ready)job.rethrow_error();const auto preview=job.backed_result();check(bool(preview),"Embedded preview absent");
    check(preview->request().position_unit==CoordinateUnit::Micrometre && preview->mapped_positions()[0].x==202.5 && preview->mapped_positions()[0].y==1,"Embedded physical units/explicit alignment were guessed or dropped");
    check(preview->spatial().dataset({1,3}).continuous()==std::vector<double>{-1.25,2.5,0,0,11,0},"Prepared continuous import changed values");
    check(job.publish(preview),"Publish prepared H5AD observations");job.wait();if(job.phase()!=SpatialImportPhase::Complete)job.rethrow_error();
    const auto report=nlohmann::json::parse(job.report());check(report.at("state")=="saved","Embedded project publication failed");fs::rename(fs::u8path(source->path()),root/"original-store-offline.qmx");
    auto project=Project::open(request.base.output_project);const auto id=expression_matrix_ids(*project,1).at(0);const auto native=reopen_expression_matrix(*project,1,id);check(native->metadata().domain==ExpressionValueDomain::Continuous && native->metadata().spatial_frame->unit==CoordinateUnit::Micrometre,"Owned source lost value/source-coordinate domain");
    const auto snapshot=ExpressionMatrixSpatialSnapshot::capture(*project->document(1),native,preview->image_frame().id);check(snapshot.dataset({1,3}).continuous()==std::vector<double>{-1.25,2.5,0,0,11,0},"Owned project needed original H5AD/store or changed values");
}
int main(){try{const auto root=fs::current_path()/("h5ad-index-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);const auto source=exercise(root);project_route(root,source);
    ExpressionMatrixImportOptions limits;limits.work_bytes=64u*1024*1024;SpatialImportJob job;const auto store=root/"job.qmx";check(job.index_h5ad((fs::path(QPX_H5AD_FIXTURES)/"csr_matrix.h5ad").u8string(),store.u8string(),options(),limits),"Submit native H5AD index job");job.wait();if(job.phase()!=SpatialImportPhase::MatrixReady)job.rethrow_error();check(job.matrix_source() && job.matrix_index_result() && job.matrix_source()->metadata().spatial_frame && job.matrix_source()->spatial_page(0,1).at(0).id=="spot_c","Existing import job lost native indexed H5AD output");no_stage(root);
    std::cout<<"Native H5AD indexed domains/coordinates/cohorts/offline project/job/atomic guards pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
