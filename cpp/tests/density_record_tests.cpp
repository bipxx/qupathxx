#include "qupathxx/spatial_density.hpp"
#include "qupathxx/spatial_neighborhood.hpp"
#include "qupathxx/project_save.hpp"
#include "density_archive.hpp"
#include "neighborhood_archive.hpp"
#include "spatial_archive.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#ifdef QPX_DENSITY_SQLITE
#include <sqlite3.h>
#endif
using namespace qpx;
namespace fs=std::filesystem;
void checked(bool value,int line) {if(!value) throw std::runtime_error("density record check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E=std::exception,class F> void rejects(F fn) {bool hit=false;try {fn();}catch(const E&) {hit=true;}check(hit);}
std::string read(const fs::path& p) {std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void reseal(std::vector<std::uint8_t>& b) {Sha256 hash;hash.update(b.data(),b.size()-64);const auto sum=hash.finish();std::copy(sum.begin(),sum.end(),b.end()-64);}
struct Scene {
    std::shared_ptr<Document> doc=std::make_shared<Document>(Image::demo());
    std::uint64_t window=doc->add_rectangle({0,0,20,20}),empty=doc->add_rectangle({30,30,2,2}),tissue=doc->add_rectangle({0,0,10,10}),excluded=doc->add_rectangle({4,0,2,10});
    std::uint64_t a=doc->add_cell(Roi::rectangle({1,1,2,2})),b=doc->add_cell(Roi::rectangle({4,4,2,2})),c=doc->add_cell(Roi::rectangle({7,7,2,2}));
    Scene() {doc->set_calibration(2,.5);}
    SpatialDensityRequest request() const {return {*doc,{empty,window},{tissue},{excluded},{c,b,a}};}
};
void retained_contracts() {
    Scene s;auto request=s.request();const auto result=measure_spatial_density(request);const auto revision=s.doc->revision();
    check(result->rows()[0].count==2 && result->rows()[0].area_pixels==80 && !result->rows()[1].density_per_mm2);
    rejects([&] {s.doc->keep_density(request,result,"");});check(s.doc->revision()==revision && !s.doc->density_record_count());
    auto fresh=s.request();rejects([&] {s.doc->keep_density(fresh,result);});
    const auto id=s.doc->keep_density(request,result,"Valid tissue density");check(id==revision+1 && s.doc->density_status(id)=="current");
    check(s.doc->density_record_count()==1 && s.doc->density_records().at(0)->id==id);
    rejects([&] {s.doc->keep_density(request,result);});
    Document copy=*s.doc;check(copy.density_status(id)=="unverified");auto replay=SpatialDensityRequest::replay(copy,id);
    check(copy.verify_density(id,replay,measure_spatial_density(replay)) && copy.density_status(id)=="current");
    s.doc->add_cell(Roi::rectangle({2,2,1,1}));replay=SpatialDensityRequest::replay(*s.doc,id);
    check(replay.cell_count()==3 && s.doc->verify_density(id,replay,measure_spatial_density(replay)));
    SpatialDensityRequest wrong(*s.doc,{s.window,s.empty},{s.tissue},{},{s.a,s.b,s.c});
    rejects([&] {s.doc->verify_density(id,wrong,measure_spatial_density(wrong));});
    auto policy=replay.options();policy.polygonize_curves=true;
    SpatialDensityRequest changed_policy(*s.doc,{s.window,s.empty},{s.tissue},{s.excluded},{s.a,s.b,s.c},policy);
    rejects([&] {s.doc->verify_density(id,changed_policy,measure_spatial_density(changed_policy));});
    const auto held=s.doc->density_record(id);s.doc->update_rectangle(s.excluded,{3,0,2,10});
    check(s.doc->density_status(id)=="objects_or_calibration_changed");rejects([&] {SpatialDensityRequest::replay(*s.doc,id);});
    check(s.doc->remove_density(id) && !s.doc->remove_density(id) && !s.doc->density_record_count());
    check(held->result->rows().size()==2 && !spatial_density_csv(*held->result).empty());
    check(copy.density_record_count()==1 && copy.density_status(id)=="current");
    copy.set_calibration(1,1);rejects([&] {SpatialDensityRequest::replay(copy,id);});
    std::cout<<"Density retention: keep/list/status, independent copies, explicit-cohort replay, stale geometry/calibration/policy and historical lifetime passed\n";
}
void codec_and_csv(const fs::path& root) {
    Scene s;auto request=s.request();auto result=measure_spatial_density(request);const auto id=s.doc->keep_density(request,result);
    const auto record=s.doc->density_record(id);const auto bytes=SpatialDensityArchiveCodec::encode(*record);
    check(std::equal(bytes.begin(),bytes.begin()+4,"QDN\1"));
    const auto restored=SpatialDensityArchiveCodec::decode(bytes.data(),bytes.size());
    check(restored->id==id && restored->captured_next_id==record->captured_next_id && restored->result->geometry_operations()==result->geometry_operations());
    check(restored->result->sha256()==result->sha256() && restored->result->input_sha256()==result->input_sha256());
    check(SpatialDensityArchiveCodec::encode(*restored)==bytes && spatial_density_csv(*restored->result)==spatial_density_csv(*result));
    check(restored->result->rows()[0].footprint->equals(*result->rows()[0].footprint));
    for(auto length:{std::size_t(0),std::size_t(4),std::size_t(128),bytes.size()-1}) rejects([&] {SpatialDensityArchiveCodec::decode(bytes.data(),length);});
    auto bad=bytes;bad[0]='X';rejects([&] {SpatialDensityArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;bad[20]^=1;rejects([&] {SpatialDensityArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;bad.insert(bad.end()-64,0);reseal(bad);rejects([&] {SpatialDensityArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;std::fill(bad.begin()+4,bad.begin()+12,0);reseal(bad);rejects([&] {SpatialDensityArchiveCodec::decode(bad.data(),bad.size());});
    // The final row is empty: zero its area, then forge a nonzero count while
    // preserving the transport checksum. Semantic validation must reject it.
    bad=bytes;bad[bad.size()-64-32]=1;reseal(bad);rejects([&] {SpatialDensityArchiveCodec::decode(bad.data(),bad.size());});
    const auto csv=spatial_density_csv(*result);check(std::count(csv.begin(),csv.end(),'\n')==3 && csv.find("count,area_pixels,area_mm2,density_per_mm2,status")!=std::string::npos && csv.find(",empty_area,cells/mm2,")!=std::string::npos);
    const auto target=root/"density.csv";check(save_spatial_density_csv(*result,target.u8string())==csv.size() && read(target)==csv);
    rejects([&] {save_spatial_density_csv(*result,target.u8string());});check(read(target)==csv);
    rejects([&] {save_spatial_density_csv(*result,target.u8string(),csv.size()-1,true);});check(read(target)==csv);
    rejects([&] {save_spatial_density_csv(*result,target.u8string(),65536,true,nullptr,[] {throw std::runtime_error("publication rejected");});});check(read(target)==csv);
    std::atomic<bool> stop{true};rejects([&] {SpatialDensityArchiveCodec::decode(bytes.data(),bytes.size(),&stop);});
    rejects([&] {save_spatial_density_csv(*result,target.u8string(),65536,true,&stop);});check(read(target)==csv);
    for(const auto& entry:fs::directory_iterator(root)) check(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos);
    // Body geometry of an area-role Cell and an explicitly accepted ellipse
    // must roundtrip as captured, without substituting the nucleus or analytic area.
    const auto body=s.doc->add_cell(Roi::rectangle({0,0,10,10}),Roi::rectangle({1,1,2,2}));
    const auto curve=s.doc->add_roi(Roi::ellipse({0,0,10,10}));SpatialDensityOptions p;p.polygonize_curves=true;
    SpatialDensityRequest curved(*s.doc,{body},{curve},{},{s.a},p);const auto curve_id=s.doc->keep_density(curved,measure_spatial_density(curved));
    const auto curved_bytes=SpatialDensityArchiveCodec::encode(*s.doc->density_record(curve_id));
    check(SpatialDensityArchiveCodec::encode(*SpatialDensityArchiveCodec::decode(curved_bytes.data(),curved_bytes.size()))==curved_bytes);
    std::cout<<"Density QDN1/CSV: exact geometry/value/policy roundtrip, corrupted/truncated/resealed semantic rejection and bounded atomic publication passed\n";
}
void eager_wkt_compatibility() {
    const auto path=fs::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/density-eager-wkt-v1/record.qdn";
    const auto raw=read(path);std::vector<std::uint8_t> bytes(raw.begin(),raw.end());
    Sha256 hash;hash.update(bytes.data(),bytes.size());
    check(hash.finish()=="392fc09c6325cb1eb39d20ed4b74817f7f11ba55e918cba63d3fdfd22903dda4");
    const auto restored=SpatialDensityArchiveCodec::decode(bytes.data(),bytes.size());
    check(restored->id==8 && restored->result->window_count()==1 && restored->result->tissue_count()==1 && restored->result->exclusion_count()==1);
    check(restored->result->input_sha256()=="2a03fe96c61e361d4aba88a6715bd8de811d4a240843137bcb88610c7c5952a6");
    check(restored->result->sha256()=="6e8274db523e923fb01e971296e1213f6493593dfa05b28f63df23d44efce844");
    check(SpatialDensityArchiveCodec::encode(*restored)==bytes);
    // Locate the capture fields without assuming string lengths. Alter both
    // byte/work charges and reseal, so checksum or work equality cannot mask
    // an erroneous tolerance for arbitrary admission slack.
    std::size_t at=20;
    auto integer=[&](std::size_t offset){std::uint64_t n=0;for(int i=0;i<8;++i)n|=std::uint64_t(bytes[offset+i])<<(8*i);return n;};
    for(int i=0;i<2;++i)at+=8+std::size_t(integer(at));
    at+=16;const auto captured=at+8*8,work=at+9*8;
    for(const auto delta:{std::int64_t(-1),std::int64_t(1)}) {
        auto bad=bytes;
        for(auto offset:{captured,work}) {
            const auto n=std::uint64_t(std::int64_t(integer(offset))+delta);
            for(int i=0;i<8;++i)bad[offset+i]=std::uint8_t(n>>(8*i));
        }
        reseal(bad);rejects([&]{SpatialDensityArchiveCodec::decode(bad.data(),bad.size());});
    }
    std::cout<<"Historical eager polygon/multipolygon WKT charge: exact golden QDN1 roundtrip and resealed off-by-one admission refusal passed\n";
}
#ifdef QPX_DENSITY_SQLITE
void sql(const fs::path& path,const std::string& command) {
    sqlite3* db=nullptr;check(sqlite3_open(path.u8string().c_str(),&db)==SQLITE_OK);char* error=nullptr;const auto code=sqlite3_exec(db,command.c_str(),nullptr,nullptr,&error);
    const std::string message=error?error:"";sqlite3_free(error);sqlite3_close(db);if(code!=SQLITE_OK) throw std::runtime_error(message);
}
void projects(const fs::path& root) {
    Scene s;auto project=std::make_shared<Project>();const auto image=project->add_document(s.doc);
    auto request=s.request();const auto result=measure_spatial_density(request);const auto id=s.doc->keep_density(request,result);
    SpatialDistanceRequest nearest(*s.doc,{s.a},{s.b});const auto nearest_id=s.doc->keep_spatial(nearest,measure_spatial_distances(nearest));
    SpatialNeighborhoodRequest neighborhood(*s.doc,{s.a},{s.b});const auto neighborhood_id=s.doc->keep_neighborhood(neighborhood,measure_spatial_neighborhoods(neighborhood));
    const auto old_distance=SpatialDistanceArchiveCodec::encode(*s.doc->spatial_record(nearest_id));
    const auto old_neighborhood=SpatialNeighborhoodArchiveCodec::encode(*s.doc->neighborhood_record(neighborhood_id));
    const auto path=root/"density.qpxproj";ProjectSaveJob save;check(save.request(project,path.u8string()));check(s.doc->remove_density(id));
    check(save.wait().state=="saved" && project->dirty());
    auto loaded=Project::open(path.u8string());auto doc=loaded->document(image);
    check(doc->density_record_count()==1 && doc->density_status(id)=="unverified" && doc->spatial_record_count()==1 && doc->neighborhood_record_count()==1);
    check(doc->density_record(id)->result->sha256()==result->sha256() && SpatialDistanceArchiveCodec::encode(*doc->spatial_record(nearest_id))==old_distance && SpatialNeighborhoodArchiveCodec::encode(*doc->neighborhood_record(neighborhood_id))==old_neighborhood);
    auto replay=SpatialDensityRequest::replay(*doc,id);check(doc->verify_density(id,replay,measure_spatial_density(replay)) && !loaded->dirty());
    doc->rename_annotation(s.a,"unsaved");
    sql(path,"CREATE TRIGGER reject_density_insert BEFORE INSERT ON spatial_density_records BEGIN SELECT RAISE(ABORT,'deliberate write failure'); END;");
    const auto guarded=read(path);rejects([&] {loaded->save(path.u8string());});check(read(path)==guarded);
    sql(path,"DROP TRIGGER reject_density_insert;");
    ProjectSaveJob limited(1);rejects([&] {limited.request(loaded,(root/"limited.qpxproj").u8string());});check(!fs::exists(root/"limited.qpxproj"));
    auto old=root/"legacy.qpxproj";fs::copy_file(fs::path(QPX_DENSITY_SOURCE_ROOT)/"docs/benchmarks/large-slide-workflow/legacy.qpxproj",old);
    auto legacy=Project::open(old.u8string());check(legacy->document(1)->density_record_count()==0);
    const auto budget=std::size_t(11489+fs::absolute(old).lexically_normal().u8string().size()-88);
    ProjectSaveJob exact(budget);check(exact.request(legacy,old.u8string()));check(exact.wait().applied && exact.stats().snapshot_bytes==budget);
    ProjectSaveJob short_by_one(budget-1);rejects([&] {short_by_one.request(legacy,old.u8string());});
    check(Project::open(old.u8string())->document(1)->tissue_record_count()==1);
    auto older=root/"schema28.qpxproj";fs::copy_file(old,older);sql(older,"DROP TABLE spatial_density_records; PRAGMA user_version=28;");
    const auto prior=read(older);auto upgrade=Project::open(older.u8string());check(read(older)==prior);
    upgrade->save(older.u8string());check(Project::open(older.u8string())->document(1)->density_record_count()==0);
    auto damaged=root/"bad-record.qpxproj";fs::copy_file(path,damaged);sql(damaged,"UPDATE spatial_density_records SET payload=X'51444E01';");
    rejects([&] {Project::open(damaged.u8string());});
    std::cout<<"Density projects: immutable async snapshot, mixed QSD1/QSN1/QDN1, save rollback, schema28 upgrade and exact inactive legacy snapshot boundary passed\n";
}
#endif
int main() {try {
    const auto root=fs::temp_directory_path()/("qupathxx-density-records-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    retained_contracts();codec_and_csv(root);eager_wkt_compatibility();
#ifdef QPX_DENSITY_SQLITE
    projects(root);
#endif
    std::cout<<"Artifacts: "<<root.u8string()<<'\n';return 0;
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
