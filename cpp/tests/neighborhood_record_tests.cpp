#include "qupathxx/spatial_neighborhood.hpp"
#include "qupathxx/project_save.hpp"
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
#ifdef QPX_NEIGHBORHOOD_SQLITE
#include <sqlite3.h>
#endif
using namespace qpx;
namespace fs=std::filesystem;
void checked(bool value,int line) {if(!value) throw std::runtime_error("neighborhood record check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E=std::exception,class F> void rejects(F fn) {bool hit=false;try {fn();}catch(const E&) {hit=true;}check(hit);}
std::string read(const fs::path& p) {std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void reseal(std::vector<std::uint8_t>& b) {Sha256 hash;hash.update(b.data(),b.size()-64);const auto sum=hash.finish();std::copy(sum.begin(),sum.end(),b.end()-64);}
struct Scene {
    std::shared_ptr<Document> doc=std::make_shared<Document>(Image::demo());
    std::uint64_t a=doc->add_detection(Roi::points({{32,32}})),b=doc->add_detection(Roi::points({{35,36}})),c=doc->add_detection(Roi::points({{32,32}}));
    Scene() {doc->set_calibration(.5,.25);}
    SpatialNeighborhoodRequest request() const {SpatialNeighborhoodOptions p;p.radius=3;p.units=SpatialDistanceUnits::Micrometres;return {*doc,{b,a},{c,b,a},p};}
};
void retained_contracts() {
    Scene s;auto request=s.request();const auto result=measure_spatial_neighborhoods(request);const auto revision=s.doc->revision();
    rejects([&] {s.doc->keep_neighborhood(request,result,"");});check(s.doc->revision()==revision && !s.doc->neighborhood_record_count());
    auto fresh=s.request();rejects([&] {s.doc->keep_neighborhood(fresh,result);});
    const auto id=s.doc->keep_neighborhood(request,result,"Within radius");check(id==revision+1 && s.doc->neighborhood_status(id)=="current");
    check(s.doc->neighborhood_record_count()==1 && s.doc->neighborhood_records().at(0)->id==id);
    rejects([&] {s.doc->keep_neighborhood(request,result);});
    Document copy=*s.doc;check(copy.neighborhood_status(id)=="unverified");auto replay=SpatialNeighborhoodRequest::replay(copy,id);
    check(copy.verify_neighborhood(id,replay,measure_spatial_neighborhoods(replay)) && copy.neighborhood_status(id)=="current");
    s.doc->add_detection(Roi::points({{33,32}}));replay=SpatialNeighborhoodRequest::replay(*s.doc,id);
    check(replay.target_count()==3 && s.doc->verify_neighborhood(id,replay,measure_spatial_neighborhoods(replay)));
    auto wrong=replay.options();wrong.radius=2;SpatialNeighborhoodRequest wrong_request(*s.doc,{s.a,s.b},{s.a,s.b,s.c},wrong);
    rejects([&] {s.doc->verify_neighborhood(id,wrong_request,measure_spatial_neighborhoods(wrong_request));});
    const auto held=s.doc->neighborhood_record(id);s.doc->update_rectangle(s.a,{1,1,2,2});
    check(s.doc->neighborhood_status(id)=="objects_or_calibration_changed");rejects([&] {SpatialNeighborhoodRequest::replay(*s.doc,id);});
    check(s.doc->remove_neighborhood(id) && !s.doc->remove_neighborhood(id) && !s.doc->neighborhood_record_count());
    check(held->result->rows().size()==2 && !spatial_neighborhood_csv(*held->result).empty());
    Document capped(Image::demo());const auto object=capped.add_rectangle({1,1,1,1});
    for(int i=0;i<10000;++i) {SpatialNeighborhoodRequest rq(capped,{object},{});capped.keep_neighborhood(rq,measure_spatial_neighborhoods(rq));}
    SpatialNeighborhoodRequest full(capped,{object},{});const auto out=measure_spatial_neighborhoods(full);const auto before=capped.revision();
    rejects<std::length_error>([&] {capped.keep_neighborhood(full,out);});check(capped.revision()==before && capped.neighborhood_record_count()==10000);
    check(capped.remove_neighborhood(capped.neighborhood_records().front()->id));
    SpatialNeighborhoodRequest retry(capped,{object},{});capped.keep_neighborhood(retry,measure_spatial_neighborhoods(retry));
    std::cout<<"Neighborhood retention: keep/list/status, independent copy, explicit-cohort replay, stale geometry, historical lifetime and record cap passed\n";
}
void codec_and_csv(const fs::path& root) {
    Scene s;auto request=s.request();auto result=measure_spatial_neighborhoods(request);const auto id=s.doc->keep_neighborhood(request,result);
    const auto record=s.doc->neighborhood_record(id);
    const auto bytes=SpatialNeighborhoodArchiveCodec::encode(*record);
    check(std::equal(bytes.begin(),bytes.begin()+4,"QSN\1"));
    const auto restored=SpatialNeighborhoodArchiveCodec::decode(bytes.data(),bytes.size());
    check(restored->id==id && restored->captured_next_id==record->captured_next_id && restored->result->options().radius==3);
    check(restored->result->sha256()==result->sha256() && restored->result->input_sha256()==result->input_sha256());
    check(SpatialNeighborhoodArchiveCodec::encode(*restored)==bytes && spatial_neighborhood_csv(*restored->result)==spatial_neighborhood_csv(*result));
    for(auto length:{std::size_t(0),std::size_t(4),std::size_t(128),bytes.size()-1}) rejects([&] {SpatialNeighborhoodArchiveCodec::decode(bytes.data(),length);});
    auto bad=bytes;bad[0]='X';rejects([&] {SpatialNeighborhoodArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;bad[20]^=1;rejects([&] {SpatialNeighborhoodArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;std::fill(bad.end()-72,bad.end()-64,255);reseal(bad);rejects([&] {SpatialNeighborhoodArchiveCodec::decode(bad.data(),bad.size());}); // Count exceeds cohort.
    bad=bytes;bad.insert(bad.end()-64,0);reseal(bad);rejects([&] {SpatialNeighborhoodArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;std::fill(bad.begin()+4,bad.begin()+12,0);reseal(bad);rejects([&] {SpatialNeighborhoodArchiveCodec::decode(bad.data(),bad.size());});
    const auto csv=spatial_neighborhood_csv(*result);check(std::count(csv.begin(),csv.end(),'\n')==3 && csv.find("count,radius,units,target_count")!=std::string::npos);
    const auto target=root/"counts.csv";check(save_spatial_neighborhood_csv(*result,target.u8string())==csv.size() && read(target)==csv);
    rejects([&] {save_spatial_neighborhood_csv(*result,target.u8string());});check(read(target)==csv);
    rejects([&] {save_spatial_neighborhood_csv(*result,target.u8string(),csv.size()-1,true);});check(read(target)==csv);
    rejects([&] {save_spatial_neighborhood_csv(*result,target.u8string(),65536,true,nullptr,[] {throw std::runtime_error("publication rejected");});});check(read(target)==csv);
    std::atomic<bool> stop{true};rejects([&] {SpatialNeighborhoodArchiveCodec::decode(bytes.data(),bytes.size(),&stop);});
    rejects([&] {save_spatial_neighborhood_csv(*result,target.u8string(),65536,true,&stop);});check(read(target)==csv);
    for(const auto& entry:fs::directory_iterator(root)) check(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos);
    std::cout<<"Neighborhood QSN1/CSV: exact roundtrip, corrupted/truncated/invalid-count rejection, locale-independent rows and bounded atomic publication passed\n";
}
#ifdef QPX_NEIGHBORHOOD_SQLITE
void sql(const fs::path& path,const std::string& command) {
    sqlite3* db=nullptr;check(sqlite3_open(path.u8string().c_str(),&db)==SQLITE_OK);char* error=nullptr;const auto code=sqlite3_exec(db,command.c_str(),nullptr,nullptr,&error);
    const std::string message=error?error:"";sqlite3_free(error);sqlite3_close(db);if(code!=SQLITE_OK) throw std::runtime_error(message);
}
void projects(const fs::path& root) {
    Scene s;auto project=std::make_shared<Project>();const auto image=project->add_document(s.doc);
    auto request=s.request();const auto result=measure_spatial_neighborhoods(request);const auto id=s.doc->keep_neighborhood(request,result);
    SpatialDistanceRequest nearest(*s.doc,{s.a},{s.b});const auto nearest_id=s.doc->keep_spatial(nearest,measure_spatial_distances(nearest));
    const auto old_bytes=SpatialDistanceArchiveCodec::encode(*s.doc->spatial_record(nearest_id));
    const auto path=root/"neighborhood.qpxproj";ProjectSaveJob save;check(save.request(project,path.u8string()));check(s.doc->remove_neighborhood(id));
    check(save.wait().state=="saved" && project->dirty());
    auto loaded=Project::open(path.u8string());auto doc=loaded->document(image);
    check(doc->neighborhood_record_count()==1 && doc->neighborhood_status(id)=="unverified" && doc->spatial_record_count()==1);
    check(doc->neighborhood_record(id)->result->sha256()==result->sha256() && SpatialDistanceArchiveCodec::encode(*doc->spatial_record(nearest_id))==old_bytes);
    auto replay=SpatialNeighborhoodRequest::replay(*doc,id);check(doc->verify_neighborhood(id,replay,measure_spatial_neighborhoods(replay)) && !loaded->dirty());
    const auto before=read(path);doc->rename_annotation(s.a,"unsaved");
    sql(path,"CREATE TRIGGER reject_neighborhood_insert BEFORE INSERT ON spatial_neighborhood_records BEGIN SELECT RAISE(ABORT,'deliberate write failure'); END;");
    const auto guarded=read(path);rejects([&] {loaded->save(path.u8string());});check(read(path)==guarded);
    sql(path,"DROP TRIGGER reject_neighborhood_insert;");
    ProjectSaveJob limited(1);rejects([&] {limited.request(loaded,(root/"limited.qpxproj").u8string());});check(!fs::exists(root/"limited.qpxproj"));
    auto old=root/"legacy.qpxproj";fs::copy_file(fs::path(QPX_NEIGHBORHOOD_SOURCE_ROOT)/"docs/benchmarks/large-slide-workflow/legacy.qpxproj",old);
    auto legacy=Project::open(old.u8string());check(legacy->document(1)->neighborhood_record_count()==0);
    const auto budget=std::size_t(11489+fs::absolute(old).lexically_normal().u8string().size()-88);
    ProjectSaveJob exact(budget);check(exact.request(legacy,old.u8string()));check(exact.wait().applied && exact.stats().snapshot_bytes==budget);
    ProjectSaveJob short_by_one(budget-1);rejects([&] {short_by_one.request(legacy,old.u8string());});
    check(Project::open(old.u8string())->document(1)->tissue_record_count()==1);
    // Recreate a pre-extension schema; open is read-only, save upgrades atomically.
    auto older=root/"schema27.qpxproj";fs::copy_file(old,older);sql(older,"DROP TABLE spatial_neighborhood_records; PRAGMA user_version=27;");
    const auto prior=read(older);auto upgrade=Project::open(older.u8string());check(read(older)==prior);
    upgrade->save(older.u8string());check(Project::open(older.u8string())->document(1)->neighborhood_record_count()==0);
    auto damaged=root/"bad-record.qpxproj";fs::copy_file(path,damaged);sql(damaged,"UPDATE spatial_neighborhood_records SET payload=X'51534E01';");
    rejects([&] {Project::open(damaged.u8string());});
    std::cout<<"Neighborhood projects: async snapshot, mixed QSD1/QSN1 replay, write rollback, schema27 upgrade and exact legacy snapshot boundary passed\n";
}
#endif
int main() {try {
    const auto root=fs::temp_directory_path()/("qupathxx-neighborhood-records-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    retained_contracts();codec_and_csv(root);
#ifdef QPX_NEIGHBORHOOD_SQLITE
    projects(root);
#endif
    std::cout<<"Artifacts: "<<root.u8string()<<'\n';return 0;
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
