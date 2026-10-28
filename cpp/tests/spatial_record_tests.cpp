#include "qupathxx/spatial_analysis.hpp"
#include "qupathxx/project_save.hpp"
#include "spatial_archive.hpp"
#include "sha256.hpp"
#include "reader_file_guard.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#ifdef QPX_SPATIAL_SQLITE
#include <sqlite3.h>
#endif
using namespace qpx;
namespace fs=std::filesystem;
void checked(bool v,int line) {if(!v) throw std::runtime_error("spatial records check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E=std::exception,class F> void rejects(F fn) {bool caught=false;try {fn();}catch(const E&) {caught=true;}check(caught);}
std::string read(const fs::path& p) {std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void reseal(std::vector<std::uint8_t>& b) {Sha256 h;h.update(b.data(),b.size()-64);const auto sum=h.finish();std::copy(sum.begin(),sum.end(),b.end()-64);}
struct Scene {
    std::shared_ptr<Document> doc=std::make_shared<Document>(Image::demo());
    std::uint64_t a=doc->add_detection(Roi::points({{32,32}})),b=doc->add_cell(Roi::rectangle({34,35,4,4}),Roi::rectangle({35,36,2,2})),c=doc->add_detection(Roi::points({{32,32}}));
    Scene() {doc->set_calibration(.5,.25);}
    SpatialDistanceRequest request() const {SpatialDistanceOptions o;o.units=SpatialDistanceUnits::Micrometres;return {*doc,{b,a},{c,b,a},o};}
};
void retained_contracts() {
    Scene s;auto request=s.request();const auto result=measure_spatial_distances(request);const auto revision=s.doc->revision();
    rejects([&]{s.doc->keep_spatial(request,result,"");});check(s.doc->revision()==revision && s.doc->spatial_record_count()==0);
    auto fresh=s.request();rejects([&]{s.doc->keep_spatial(fresh,result);});
    const auto id=s.doc->keep_spatial(request,result,"Nearest targets");check(id==revision+1 && s.doc->spatial_status(id)=="current" && s.doc->spatial_record_count()==1);
    rejects([&]{s.doc->keep_spatial(request,result);});
    const auto record=s.doc->spatial_record(id);check(record->result==result && record->name=="Nearest targets" && record->captured_next_id>s.c);
    Document clone=*s.doc;check(clone.spatial_record(id)==record && clone.spatial_status(id)=="unverified");
    auto replay=SpatialDistanceRequest::replay(clone,id);check(clone.verify_spatial(id,replay,measure_spatial_distances(replay)) && clone.spatial_status(id)=="current");
    check(clone.revision()==s.doc->revision());
    s.doc->add_rectangle({100,100,3,3});check(s.doc->spatial_status(id)=="current"); // Explicit cohorts do not include new objects.
    s.doc->set_calibration(.75,.25);check(s.doc->spatial_status(id)=="objects_or_calibration_changed");
    rejects([&]{SpatialDistanceRequest::replay(*s.doc,id);});s.doc->set_calibration(.5,.25);
    auto current=SpatialDistanceRequest::replay(*s.doc,id);check(s.doc->verify_spatial(id,current,measure_spatial_distances(current)));
    s.doc->remove_annotation(s.c);check(s.doc->spatial_status(id)=="objects_or_calibration_changed");
    rejects([&]{SpatialDistanceRequest::replay(*s.doc,id);});check(record->result->sha256()==result->sha256());
    check(s.doc->remove_spatial(id) && !s.doc->remove_spatial(id) && s.doc->spatial_record_count()==0);rejects<std::out_of_range>([&]{s.doc->spatial_record(id);});
    check(!spatial_distance_csv(*record->result).empty()); // Historical ownership survives removal.
    Scene closed;auto rq=closed.request();const auto rr=measure_spatial_distances(rq);closed.doc->server()->close();
    const auto retained=closed.doc->keep_spatial(rq,rr);check(closed.doc->spatial_status(retained)=="current");
    auto again=SpatialDistanceRequest::replay(*closed.doc,retained);check(closed.doc->verify_spatial(retained,again,measure_spatial_distances(again)));
    Document capped(Image::demo());const auto object=capped.add_rectangle({1,1,1,1});
    for(std::size_t i=0;i<10000;++i) {SpatialDistanceRequest rq(capped,{object},{});capped.keep_spatial(rq,measure_spatial_distances(rq));}
    SpatialDistanceRequest full(capped,{object},{});const auto full_result=measure_spatial_distances(full);const auto before=capped.revision();
    rejects<std::length_error>([&]{capped.keep_spatial(full,full_result);});check(capped.revision()==before && capped.spatial_record_count()==10000);
    check(capped.remove_spatial(capped.spatial_records().front()->id));SpatialDistanceRequest retry(capped,{object},{});capped.keep_spatial(retry,measure_spatial_distances(retry));check(capped.spatial_record_count()==10000);
    std::cout<<"Spatial retained records: exact capture authority, explicit cohorts, clone/replay, calibration/object staleness, closed source and historical ownership passed\n";
}
void codec_contracts() {
    Scene s;auto request=s.request();const auto result=measure_spatial_distances(request);const auto id=s.doc->keep_spatial(request,result);
    const auto record=s.doc->spatial_record(id);const auto bytes=SpatialDistanceArchiveCodec::encode(*record);
    const auto restored=SpatialDistanceArchiveCodec::decode(bytes.data(),bytes.size());
    check(restored->id==id && restored->captured_next_id==record->captured_next_id && restored->name==record->name && restored->producer==record->producer);
    check(restored->result->sha256()==result->sha256() && restored->result->input_sha256()==result->input_sha256());
    check(restored->result->source_count()==2 && restored->result->target_count()==3 && !restored->result->belongs_to(request));
    check(spatial_distance_csv(*restored->result)==spatial_distance_csv(*result));check(SpatialDistanceArchiveCodec::encode(*restored)==bytes);
    for(auto n:{std::size_t(0),std::size_t(4),std::size_t(20),std::size_t(128),bytes.size()-1}) rejects([&]{SpatialDistanceArchiveCodec::decode(bytes.data(),n);});
    auto bad=bytes;bad[0]='X';rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;bad[20]^=1;rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;bad.insert(bad.end()-64,0);reseal(bad);rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;std::fill(bad.begin()+4,bad.begin()+12,0);reseal(bad);rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;const auto at=std::search(bad.begin(),bad.end(),result->input_sha256().begin(),result->input_sha256().end());check(at!=bad.end());*at=*at=='0'?'1':'0';reseal(bad);rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;bad[bad.size()-64-8]^=1;reseal(bad);rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    // A foreign libm may round hypot by one ULP. Preserve that archived value
    // and its own identity instead of rewriting it during structural decoding.
    auto foreign=bytes;const auto rounded=std::nextafter(*result->rows().back().distance,INFINITY);std::uint64_t bits;std::memcpy(&bits,&rounded,8);
    for(int i=0;i<8;++i) foreign[foreign.size()-64-8+i]=std::uint8_t(bits>>(8*i));
    Sha256 h;auto text=[&](const std::string& value) {h.integer(value.size());h.update(value.data(),value.size());};text("qpx.centroid-nearest-result/1");text(result->input_sha256());h.integer(result->rows().size());
    for(std::size_t i=0;i<result->rows().size();++i) {const auto& row=result->rows()[i];h.integer(row.source_id);h.integer(bool(row.target_id));if(row.target_id) {h.integer(*row.target_id);const auto value=i+1==result->rows().size()?rounded:*row.distance;std::memcpy(&bits,&value,8);h.integer(bits);}}
    const auto foreign_hash=h.finish();auto where=std::search(foreign.begin(),foreign.end(),result->sha256().begin(),result->sha256().end());check(where!=foreign.end());std::copy(foreign_hash.begin(),foreign_hash.end(),where);reseal(foreign);
    const auto portable=SpatialDistanceArchiveCodec::decode(foreign.data(),foreign.size());check(portable->result->sha256()==foreign_hash && portable->result->rows().back().distance==rounded);
    std::atomic<bool> cancelled{true};rejects([&]{SpatialDistanceArchiveCodec::encode(*record,&cancelled);});rejects([&]{SpatialDistanceArchiveCodec::decode(bytes.data(),bytes.size(),&cancelled);});
    SpatialDistanceRequest empty(*s.doc,{s.a},{});const auto empty_result=measure_spatial_distances(empty);const auto empty_id=s.doc->keep_spatial(empty,empty_result);
    const auto eb=SpatialDistanceArchiveCodec::encode(*s.doc->spatial_record(empty_id));const auto er=SpatialDistanceArchiveCodec::decode(eb.data(),eb.size());check(!er->result->rows()[0].distance && er->result->target_count()==0);
    std::cout<<"Spatial QSD1 archives: deterministic roundtrip, provenance/input/result identity, nullable rows, corruption/truncation/semantic refusal and cancellation passed\n";
}
void export_contracts(const fs::path& root) {
    Scene s;auto request=s.request();const auto result=measure_spatial_distances(request);const auto csv=spatial_distance_csv(*result);const auto target=root/"distances.csv";
    check(csv.find(",um,matched,qpx.centroid-nearest/1,")!=std::string::npos);
    check(csv.find(",0.5,0.25,true,true\n")!=std::string::npos);
    check(save_spatial_distance_csv(*result,target.u8string())==csv.size() && read(target)==csv);
    rejects([&]{save_spatial_distance_csv(*result,target.u8string());});check(read(target)==csv);
    rejects<std::length_error>([&]{save_spatial_distance_csv(*result,target.u8string(),32,true);});check(read(target)==csv);
    rejects<std::length_error>([&]{spatial_distance_csv(*result,32);});rejects([&]{spatial_distance_csv(*result,0);});
    std::atomic<bool> cancelled{true};rejects([&]{save_spatial_distance_csv(*result,target.u8string(),65536,true,&cancelled);});check(read(target)==csv);
    rejects([&]{save_spatial_distance_csv(*result,target.u8string(),65536,true,nullptr,[]{throw std::runtime_error("changed input before publication");});});check(read(target)==csv);
    cancelled=false;rejects([&]{save_spatial_distance_csv(*result,target.u8string(),65536,true,&cancelled,[&]{cancelled=true;});});check(read(target)==csv);cancelled=false;
    const auto raced=root/"raced.csv";
    rejects([&]{save_spatial_distance_csv(*result,raced.u8string(),65536,false,nullptr,[&]{std::ofstream(raced)<<"another writer";});});check(read(raced)=="another writer");
    const auto input=root/"observed-input.txt";{std::ofstream(input)<<"original input";}detail::ReaderFileGuard observed(input.u8string());
    rejects([&]{save_spatial_distance_csv(*result,target.u8string(),65536,true,nullptr,[&]{std::ofstream(input)<<"changed input";observed.verify();});});check(read(target)==csv);
    SpatialDistanceRequest empty(*s.doc,{s.a},{});const auto missing=measure_spatial_distances(empty);const auto missing_csv=spatial_distance_csv(*missing);
    const auto line=missing_csv.substr(missing_csv.find('\n')+1);check(std::count(line.begin(),line.end(),',')==21 && line.find(",px,no_target,")!=std::string::npos);
    check(line.find(",,,,,,,,px,no_target,")!=std::string::npos); // Seven absent fields need eight separators.
    check(save_spatial_distance_csv(*missing,target.u8string(),65536,true)==missing_csv.size() && read(target)==missing_csv);
    SpatialDistanceOptions o;o.exclude_self=false;SpatialDistanceRequest zero(*s.doc,{s.a},{s.a},o);const auto zr=measure_spatial_distances(zero);
    check(spatial_distance_csv(*zr).find(",0,px,matched,")!=std::string::npos);
    rejects([&]{save_spatial_distance_csv(*result,(root/"absent"/"out.csv").u8string());});
    for(const auto& p:fs::directory_iterator(root)) check(p.path().filename().u8string().find(".qpx-tmp-")==std::string::npos);
    std::cout<<"Spatial CSV: float64 units/identity, absent versus zero, streaming atomic no-overwrite/replacement, limits/cancellation and temporary cleanup passed\n";
}
#ifdef QPX_SPATIAL_SQLITE
void sql(const fs::path& path,const std::string& command) {
    sqlite3* db=nullptr;check(sqlite3_open(path.u8string().c_str(),&db)==SQLITE_OK);char* error=nullptr;const auto code=sqlite3_exec(db,command.c_str(),nullptr,nullptr,&error);
    const std::string message=error?error:"";sqlite3_free(error);sqlite3_close(db);if(code!=SQLITE_OK) throw std::runtime_error(message);
}
void project_contracts(const fs::path& root) {
    Scene s;auto project=std::make_shared<Project>();const auto image=project->add_document(s.doc);
    auto request=s.request();const auto result=measure_spatial_distances(request);const auto id=s.doc->keep_spatial(request,result);
    const auto path=root/"spatial.qpxproj";ProjectSaveJob save;check(save.request(project,path.u8string()));check(s.doc->remove_spatial(id));
    const auto saved=save.wait();check(saved.state=="saved" && project->dirty());
    auto loaded=Project::open(path.u8string());auto doc=loaded->document(image);check(doc->spatial_record_count()==1 && doc->spatial_status(id)=="unverified");
    check(doc->spatial_record(id)->result->sha256()==result->sha256());auto replay=SpatialDistanceRequest::replay(*doc,id);check(doc->verify_spatial(id,replay,measure_spatial_distances(replay)));
    check(!loaded->dirty());
    const auto before=read(path);rejects([&]{loaded->save_new(path.u8string());});check(read(path)==before);
    ProjectSaveJob limited(1);rejects([&]{limited.request(loaded,(root/"limited.qpxproj").u8string());});check(!fs::exists(root/"limited.qpxproj"));
    loaded->save_new((root/"copy.qpxproj").u8string());check(Project::open((root/"copy.qpxproj").u8string())->document(image)->spatial_record_count()==1);
    Scene linked_scene;auto linked=std::make_shared<Project>();const auto linked_id=linked->add_document(linked_scene.doc,"Missing linked image",(root/"missing.ppm").u8string(),"ppm");
    auto lr=linked_scene.request();const auto lid=linked_scene.doc->keep_spatial(lr,measure_spatial_distances(lr));linked->save((root/"linked.qpxproj").u8string());
    auto restored_link=Project::open((root/"linked.qpxproj").u8string())->document(linked_id);auto link_replay=SpatialDistanceRequest::replay(*restored_link,lid);
    check(restored_link->verify_spatial(lid,link_replay,measure_spatial_distances(link_replay)) && !fs::exists(root/"missing.ppm"));
    const auto bad=root/"bad.qpxproj";fs::copy_file(path,bad);sql(bad,"UPDATE spatial_distance_records SET payload=zeroblob(128)");rejects([&]{Project::open(bad.u8string());});
    fs::remove(bad);fs::copy_file(path,bad);sql(bad,"UPDATE spatial_distance_records SET id='999999'");rejects([&]{Project::open(bad.u8string());});
    fs::remove(bad);fs::copy_file(path,bad);sql(bad,"UPDATE spatial_distance_records SET image_id='999999'");rejects([&]{Project::open(bad.u8string());});
    std::cout<<"Spatial projects: schema14 save/reopen, immutable async save, history/reverification, snapshot budget, missing linked-image zero-IO replay and corrupt/orphan refusal passed\n";
    // Empty historical projects retain their exact schema until the first save.
    Project empty;const auto empty_image=empty.add_document(std::make_shared<Document>(Image::demo()));const auto base=root/"empty.qpxproj";empty.save(base.u8string());
    const std::vector<std::pair<int,std::string>> tables={{14,"spatial_distance_records"},{13,"raw_analysis_records"},{12,"tiff_views"},{11,"tma_cores"},{11,"tma_grids"},{10,"pixel_stream_records"},{9,"pixel_classifier_records"},{8,"classifier_records"},{7,"analysis_provenance"},{6,"cell_feature_links"},{5,"cell_records"},{4,"tissue_records"},{3,"analysis_records"},{2,"views"}};
    for(int version=1;version<14;++version) {
        const auto old=root/("legacy-"+std::to_string(version)+".qpxproj");fs::copy_file(base,old);std::string statement;
        for(const auto& entry:tables) if(entry.first>version) statement+="DROP TABLE "+entry.second+";";
        statement+="PRAGMA user_version="+std::to_string(version);sql(old,statement);const auto original=read(old);
        auto legacy=Project::open(old.u8string());check(read(old)==original && legacy->document(empty_image)->spatial_record_count()==0);
        auto d=legacy->document(empty_image);const auto oid=d->add_rectangle({1,1,2,2});SpatialDistanceRequest r(*d,{oid},{});d->keep_spatial(r,measure_spatial_distances(r));
        if(version==13) {
            sql(old,"CREATE TRIGGER fail_save BEFORE INSERT ON images BEGIN SELECT RAISE(ABORT,'injected migration failure'); END;");const auto prior=read(old);
            rejects([&]{legacy->save(old.u8string());});check(read(old)==prior);
            sql(old,"DROP TRIGGER fail_save");
        }
        legacy->save(old.u8string());check(Project::open(old.u8string())->document(empty_image)->spatial_record_count()==1);
    }
    std::cout<<"Spatial migrations: all13 legacy schemas open without modification and transactionally upgrade with retained distance rows passed\n";
}
#endif
int main() {
    try {
        Project identity;const auto root=fs::current_path()/(identity.identity()+"-spatial-records");fs::create_directory(root);
        retained_contracts();codec_contracts();export_contracts(root);
#ifdef QPX_SPATIAL_SQLITE
        project_contracts(root);
#else
        check(!has_project());std::cout<<"Spatial project archive capability unavailable in this build\n";
#endif
        fs::remove_all(root);return 0;
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
