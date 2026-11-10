#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_source.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/registration.hpp"
#include "../raw_analysis_internal.hpp"
#include "../reader_file_guard.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F>void rejects(F fn){bool b=false;try{fn();}catch(const std::exception&){b=true;}check(b,"Expected linked Zarr refusal");}
void sql(const fs::path& path,const char* command){sqlite3* db=nullptr;check(sqlite3_open(path.u8string().c_str(),&db)==SQLITE_OK,"Open own test project");const auto rc=sqlite3_exec(db,command,nullptr,nullptr,nullptr);sqlite3_close(db);check(rc==SQLITE_OK,"Update own corrupt fixture");}
int main(int argc,char** argv){try{
    const auto root=fs::current_path()/("zarr-project-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);
    const auto store=root/"source.zarr";fs::copy(fs::path(QPX_ZARR_FIXTURES)/"plain",store,fs::copy_options::recursive);
    OmeZarrOpenOptions options;options.view.z=1;options.view.t=1;options.view.channels={0,1,2};options.view.lower={10100,11100,12100};options.view.upper={10146,11146,12146};options.source.chunk_bytes=1024*1024;options.source.encoded_bytes=2u*1024*1024;options.work_bytes=128u*1024*1024;
    Project project;const auto id=project.add_document(std::make_shared<Document>(open_ome_zarr(store.u8string(),options)));auto doc=project.document(id);const auto rgb=doc->server()->read_region({3,2,0,1,1}).bytes();
    RawRegionRequest region;region.region={0,0,0,7,5};region.channels=3;region.z=1;region.t=1;RawAnalysisRequest request(*doc,region);const auto result=analyze_raw(request);const auto kept=doc->keep_raw_analysis(request,result);
    check(result->statistics().means==std::vector<std::optional<double>>{10123,11123,12123},"Project raw measurement changed");
    const auto encoded=RawAnalysisCodec::encode(*doc->raw_analysis_record(kept),doc->metadata());check(encoded[3]==5 && RawAnalysisCodec::decode(encoded.data(),encoded.size(),doc->metadata())->result->source()->directory_sha256==result->source()->directory_sha256,"Natural directory raw record lost QRA5 identity");
    ScientificRawOptions grid_options;grid_options.channels=3;grid_options.z=1;grid_options.t=1;RawAnalysisRequest grid_request(*doc,ScientificGrid(7,5,{0,7,7},{0,5,5}),grid_options);const auto grid=analyze_raw(grid_request);const auto grid_id=doc->keep_raw_analysis(grid_request,grid);const auto grid_bytes=RawAnalysisCodec::encode(*doc->raw_analysis_record(grid_id),doc->metadata());check(grid_bytes[3]==6 && RawAnalysisCodec::decode(grid_bytes.data(),grid_bytes.size(),doc->metadata())->result->grid().has_value(),"Directory grid record lost QRA6 identity");doc->remove_raw_analysis(grid_id);
    const auto frame=image_coordinate_frame(*doc,"before");
    const auto path=root/"study.qpx";project.save_new(path.u8string());
    auto reopened=Project::open(path.u8string());auto restored=reopened->document(id);check(restored->server()->reader_id()=="ome-zarr" && !restored->server()->tiff_view() && restored->active_plane()==ObjectPlane(-1,1,1),"Saved source/view/plane did not reopen");
    check(restored->server()->ome_zarr_options()->source.chunk_bytes==options.source.chunk_bytes && restored->server()->ome_zarr_options()->work_bytes==options.work_bytes,"Saved resource limits changed");
    check(restored->server()->read_region({3,2,0,1,1}).bytes()==rgb,"Saved projection changed RGB");
    const auto replay=RawAnalysisRequest::replay(*restored,kept);check(restored->verify_raw_analysis(kept,replay,analyze_raw(replay)),"Saved raw record cannot replay natively");
    fs::rename(store,root/"offline.zarr");auto offline=Project::open(path.u8string());check(offline->document(id)->raw_analysis_record_count()==1 && offline->document(id)->active_plane()==ObjectPlane(-1,1,1),"Offline project cannot inspect saved view/records");offline->save_new((root/"offline-saved.qpx").u8string());rejects([&]{offline->document(id)->server()->read_region({0,0,0,1,1});});fs::rename(root/"offline.zarr",store);
    const auto fingerprint=fingerprint_file((store/".zattrs").u8string());auto review=reopened->review_source_verification(id,*fingerprint);check(review->coverage()=="directory_primary_only","Project review overclaims primary hash coverage");reopened->apply_source_review(review,false);
    {std::fstream file(store/"0/0.0.0.0.0",std::ios::binary|std::ios::in|std::ios::out);file.put(char(1));}
    auto stale=Project::open(path.u8string());rejects([&]{stale->document(id)->server()->read_region({3,2,0,1,1});});
    const auto unchanged=fingerprint_file((store/".zattrs").u8string());check(unchanged->sha256==fingerprint->sha256,"Test unexpectedly changed primary metadata");
    auto changed=reopened->review_source_verification(id,*unchanged);check(changed->comparison()=="changed_dependencies" && changed->requires_acceptance(),"Changed chunk escaped metadata-only source review");rejects([&]{reopened->apply_source_review(changed,false);});reopened->apply_source_review(changed,true);
    check(image_coordinate_frame(*reopened->document(id),"before").source_id!=frame.source_id,"Registration image identity ignored changed directory");
    const auto broken=root/"bad.qpx";fs::copy_file(path,broken);sql(broken,"UPDATE tiff_views SET payload=x'000102'");rejects([&]{Project::open(broken.u8string());});
    detail::guard_file_output(store.u8string(),"",(root/"outside.npy").u8string());rejects([&]{detail::guard_file_output(store.u8string(),"",(store/"inside.npy").u8string());});
    if(argc==3){OmeZarrOpenOptions real;real.view.channels={0,1,-1};real.view.lower={7,0,0};real.view.upper={48,255,1};Project output;const auto actual=output.add_document(std::make_shared<Document>(open_ome_zarr(argv[1],real)));RawRegionRequest all;all.region={0,0,0,512,512};all.channels=2;auto document=output.document(actual);RawAnalysisRequest captured(*document,all);document->keep_raw_analysis(captured,analyze_raw(captured));output.save_new(argv[2]);auto saved=Project::open(argv[2]);check(saved->document(actual)->raw_analysis_record_count()==1 && saved->document(actual)->server()->read_region({0,0,0,512,512}).bytes()==document->server()->read_region({0,0,0,512,512}).bytes(),"Full real saved project changed projection/record");}
    std::cout<<"Native Zarr project view/options/offline records/QRA5/replay/directory review/frame identity checks pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
