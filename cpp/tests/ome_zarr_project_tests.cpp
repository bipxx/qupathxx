#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_batch.hpp"
#include "qupathxx/project_source.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/registration.hpp"
#include "../raw_analysis_internal.hpp"
#include "../reader_file_guard.hpp"
#include <sqlite3.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F>void rejects(F fn){bool b=false;try{fn();}catch(const std::exception&){b=true;}check(b,"Expected linked Zarr refusal");}
void sql(const fs::path& path,const char* command){sqlite3* db=nullptr;check(sqlite3_open(path.u8string().c_str(),&db)==SQLITE_OK,"Open own test project");const auto rc=sqlite3_exec(db,command,nullptr,nullptr,nullptr);sqlite3_close(db);check(rc==SQLITE_OK,"Update own corrupt fixture");}
void refuses_batch_output(const Project& project,const fs::path& output,
                          const std::vector<ProjectBatchSelection>& selection={}) {
    const StainTransform stains({{1,0,0},{0,1,0}});
    const auto report=run_project_batch(ProjectBatchRequest(project,stains,selection),output.u8string());
    check(report.state=="failed" && report.report_path.empty() && report.images.empty() &&
          report.error.find("outside the protected input directory")!=std::string::npos && !fs::exists(output),
          "Batch wrote into a protected directory store");
}
void checks_store_save_destinations(const fs::path& root) {
    const auto store=root/"protected.zarr", template_path=root/"protected-template.qpx";
    fs::copy(fs::path(QPX_ZARR_FIXTURES)/"plain",store,fs::copy_options::recursive);
    Project project;
    project.add_image(store.u8string(),"ome-zarr","Linked source");
    project.save_new(template_path.u8string());
    check(Project::open_without_journal_recovery(template_path.u8string())->images().front().available,
          "Present OME-Zarr directory was listed as unavailable");
    const auto template_sha=fingerprint_file(template_path.u8string())->sha256;
    const auto output=store/"direct.qpx";
    auto refuse=[&](const fs::path& destination,bool save_new) {
        const auto working=Project::open_without_journal_recovery(template_path.u8string());
        try {
            if(save_new)working->save_new(destination.u8string());
            else working->save(destination.u8string());
        } catch(const std::invalid_argument& e) {
            const std::string message=e.what();
            check(message.find("protected input directory")!=std::string::npos ||
                  message=="fingerprint output must not replace its input",e.what());
            check(!fs::exists(destination) && working->path()==template_path.u8string() && !working->dirty(),
                  "Refused save changed its destination or the live project");
            return true;
        }
        return false;
    };
    const bool online_refused=refuse(output,false);
    const bool online_new_refused=refuse(store/"save-as.qpx",true);
    const auto held=root/"held.zarr";
    fs::rename(store,held);
    check(!Project::open_without_journal_recovery(template_path.u8string())->images().front().available,
          "Moved OME-Zarr directory was listed as available");
    const bool offline_new_refused=refuse(store,true);
    if(!offline_new_refused)fs::rename(store,root/"bad-save-new.qpx");
    const bool offline_save_refused=refuse(store,false);
    if(!offline_save_refused)fs::rename(store,root/"bad-save.qpx");
    check(fingerprint_file(template_path.u8string())->sha256==template_sha,"Save rewrote the template");
    if(!online_refused)std::cerr<<"Ordinary save wrote inside a linked OME-Zarr store\n";
    if(!offline_new_refused)std::cerr<<"Save As replaced the missing OME-Zarr directory with SQLite\n";
    if(!offline_save_refused)std::cerr<<"Ordinary save replaced the missing OME-Zarr directory with SQLite\n";
    check(online_refused && online_new_refused && offline_new_refused && offline_save_refused,
          "Project save did not protect its linked image");
}
void checks_raw_batch_planes(const fs::path& path,const fs::path& root,std::uint64_t image,
                             std::uint64_t saved_roi,std::uint64_t origin_roi,std::uint64_t channel_roi) {
    const auto project=Project::open(path.u8string());
    const StainTransform stains({{1,0,0},{0,1,0}});
    ProjectBatchOptions options;
    options.kind=ProjectBatchKind::Raw;
    options.raw.channels=3;
    auto run=[&](const char* name,std::uint64_t roi,std::uint32_t z,std::uint32_t t,double mean) {
        const auto output=(root/name).u8string();
        const ProjectBatchRequest request(*project,stains,{{image,{}}},options);
        check(request.object_count()==1,"Automatic batch did not capture exactly one matching ROI");
        const auto saved=run_project_batch(request,output);
        if(saved.saved!=1 || saved.images.size()!=1 || saved.images[0].records!=1)
            throw std::runtime_error("Raw plane batch failed: "+saved.error);
        const auto& checkpoint=saved.images[0].path;
        const auto reopened=Project::open(checkpoint);
        const auto records=reopened->document(image)->raw_analysis_records();
        check(!records.empty(),"Raw plane batch checkpoint contains no records");
        const auto& result=*records.back()->result;
        const auto depth=result.grid()?result.grid()->options.z:result.region().z;
        const auto time=result.grid()?result.grid()->options.t:result.region().t;
        check(depth==z && time==t && result.object_id()==roi &&
              result.grid().has_value()==options.raw_grid.has_value(),"Raw batch retained the wrong plane or geometry");
        const auto& means=result.statistics().means;
        check(means.size()==3,"Raw batch did not retain three channels");
        for(std::size_t channel=0;channel<means.size();++channel)
            check(means[channel] && std::abs(*means[channel]-(mean+1000.0*channel))<1e-12,
                  "Raw batch retained the wrong channel mean");
        const auto reused=run_project_batch(request,output);
        check(reused.reused==1 && reused.images.size()==1 && reused.images[0].path==checkpoint &&
              reused.images[0].pixel_sha256==saved.images[0].pixel_sha256,"Raw plane batch changed on reuse");
    };
    run("natural",saved_roi,1,1,10123);
    options.raw_grid=RawGridSampling{7,5};
    run("grid",saved_roi,1,1,10123);
    options.raw_grid.reset();
    options.raw_follow_display=false;
    run("explicit",origin_roi,0,0,23);
    options.raw.z=1;
    auto empty=run_project_batch(ProjectBatchRequest(*project,stains,{{image,{}}},options),(root/"empty-plane").u8string());
    check(empty.saved==1 && empty.images[0].objects==0 && empty.images[0].records==0,
          "An available plane without matching annotations did not save an empty table");
    options.raw.z=2;
    auto invalid=run_project_batch(ProjectBatchRequest(*project,stains,{{image,{}}},options),(root/"invalid-plane").u8string());
    check(invalid.failed==1 && invalid.error.find("outside the source domain")!=std::string::npos,
          "Invalid explicit axes were treated as an empty selection");
    options.raw_follow_display=true;
    auto mismatch=run_project_batch(ProjectBatchRequest(*project,stains,{{image,{origin_roi}}},options),(root/"off-plane").u8string());
    check(mismatch.failed==1 && mismatch.error.find("does not match the object plane")!=std::string::npos,
          "Explicit off-plane annotation was silently omitted");
    options.raw.first_channel=1;
    options.raw.channels=1;
    const ProjectBatchRequest single_channel(*project,stains,{{image,{}}},options);
    const auto channel=run_project_batch(single_channel,(root/"channel").u8string());
    check(single_channel.object_count()==2 && channel.saved==1 && channel.images[0].records==2,
          "Matching channel-specific object was omitted");
    const auto result=Project::open(channel.images[0].path)->document(image);
    check(result->latest_raw_analysis(channel_roi)->result->statistics().means==std::vector<std::optional<double>>{11123},
          "Channel-specific measurement changed");
}
void checks_source_review_limits(const fs::path& root) {
    const auto store=root/"large-metadata.zarr";
    fs::copy(fs::path(QPX_ZARR_FIXTURES)/"plain",store,fs::copy_options::recursive);
    const auto metadata=store/".zattrs";
    {
        std::ofstream file(metadata,std::ios::binary|std::ios::app);
        file<<std::string(1024u*1024,' ');
        file.close();
        check(bool(file),"Could not write the owned metadata-budget fixture");
    }
    rejects([&]{open_ome_zarr(store.u8string());});
    OmeZarrOpenOptions options;
    options.source.metadata_bytes=2u*1024*1024;
    Project project;
    const auto id=project.add_document(std::make_shared<Document>(open_ome_zarr(store.u8string(),options)));
    const auto saved=root/"large-metadata.qpx";
    project.save_new(saved.u8string());
    const auto reopened=Project::open(saved.u8string());
    const auto fingerprint=fingerprint_file(metadata.u8string());
    const auto review=reopened->review_source_verification(id,*fingerprint);
    reopened->apply_source_review(review);
    const auto baseline=reopened->source_verification(id);
    check(baseline && baseline->fingerprint.sha256==fingerprint->sha256 &&
          baseline->source.directory_sha256==reopened->document(id)->server()->reader_source()->directory_sha256,
          "Review did not preserve the admitted directory observation");
    const auto recheck=reopened->review_source_verification(id,*fingerprint);
    {
        std::fstream chunk(store/"0/0.0.0.0.0",std::ios::binary|std::ios::in|std::ios::out);
        chunk.put(char(1));
        chunk.close();
        check(bool(chunk),"Could not mutate the owned dependency");
    }
    rejects([&]{reopened->apply_source_review(recheck);});
    check(!recheck->consumed() && reopened->source_verification(id)==baseline,
          "Changed dependency bypassed review currentness under custom reader limits");
}
void checks_explicit_store_relocation(const fs::path& path,const fs::path& store,
                                      std::uint64_t image,std::uint64_t record) {
    const auto project=Project::open(path.u8string());
    const auto initial=fingerprint_file((store/".zattrs").u8string());
    project->apply_source_review(project->review_source_verification(image,*initial));
    const auto baseline=project->source_verification(image);
    const auto original=project->document(image)->server();
    const auto historical=project->document(image)->raw_analysis_record(record)->result->source();
    const auto destination=store.parent_path()/"relocated.zarr";
    fs::copy(store,destination,fs::copy_options::recursive);
    const auto fingerprint=fingerprint_file((destination/".zattrs").u8string());
    check(fingerprint->sha256==initial->sha256,"Relocation fixture changed the metadata bytes");
    rejects([&]{project->review_directory_source_verification(image,destination.u8string(),*initial);});
    const auto review=project->review_directory_source_verification(image,destination.u8string(),*fingerprint);
    check(review->path()==destination.u8string() && review->comparison()=="changed_dependencies" &&
          review->coverage()=="directory_primary_only" && review->requires_acceptance(),
          "Reviewed relocation did not bind the explicit destination store");
    rejects([&]{project->apply_source_review(review);});
    check(!review->consumed() && project->source_verification(image)==baseline &&
          project->document(image)->server()==original,"Refused relocation changed project state");
    project->apply_source_review(review,true);
    const auto saved_path=store.parent_path()/"relocated.qpx";
    project->save_new(saved_path.u8string());
    const auto saved=Project::open(saved_path.u8string());
    const auto doc=saved->document(image);
    doc->server()->require_raw_current();
    check(doc->server()->source_locator()==destination.u8string() &&
          doc->server()->reader_source()->directory_sha256==saved->source_verification(image)->source.directory_sha256 &&
          doc->raw_analysis_record(record)->result->source()->directory_sha256==historical->directory_sha256 &&
          doc->raw_analysis_record(record)->result->source()->locator==historical->locator,
          "Relocated project lost its new link or rewrote the historical source");
}
void checks_batch_source_verification(Project& project,const fs::path& root,std::uint64_t image) {
    const auto baseline=project.source_verification(image);
    check(bool(baseline),"Batch verification fixture lacks its source baseline");
    ProjectBatchOptions options;
    options.kind=ProjectBatchKind::Raw;
    options.raw.channels=3;
    const StainTransform stains({{1,0,0},{0,1,0}});
    const ProjectBatchRequest captured(project,stains,{{image,{}}},options);
    project.clear_source_verification(image);
    const auto output=(root/"verified-batch").u8string();
    const auto report=run_project_batch(captured,output);
    if(report.saved!=1) throw std::runtime_error("Verified source batch failed: "+report.error);
    const auto& path=report.images.at(0).path;
    const auto checkpoint=Project::open(path);
    const auto archived=checkpoint->source_verification(image);
    check(archived && source_file_association_json(*archived)==source_file_association_json(*baseline),
          "Batch checkpoint lost its captured source verification");
    rejects([&]{checkpoint->relink(image,archived->source.locator);});
    const auto reused=run_project_batch(captured,output);
    check(reused.reused==1 && reused.images.at(0).path==path,"Verified checkpoint did not reuse");
    const auto after_clear=run_project_batch(ProjectBatchRequest(project,stains,{{image,{}}},options),output);
    check(after_clear.saved==1 && after_clear.images.at(0).path!=path &&
          !Project::open(after_clear.images.at(0).path)->source_verification(image),
          "A new capture without a baseline reused the verified checkpoint");
}
int main(int argc,char** argv){try{
    const auto root=fs::current_path()/("zarr-project-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);
    checks_store_save_destinations(root);
    checks_source_review_limits(root);
    const auto store=root/"source.zarr";fs::copy(fs::path(QPX_ZARR_FIXTURES)/"plain",store,fs::copy_options::recursive);
    OmeZarrOpenOptions options;options.view.z=1;options.view.t=1;options.view.channels={0,1,2};options.view.lower={10100,11100,12100};options.view.upper={10146,11146,12146};options.source.chunk_bytes=1024*1024;options.source.encoded_bytes=2u*1024*1024;options.work_bytes=128u*1024*1024;
    Project project;const auto id=project.add_document(std::make_shared<Document>(open_ome_zarr(store.u8string(),options)));auto doc=project.document(id);const auto rgb=doc->server()->read_region({3,2,0,1,1}).bytes();
    RawRegionRequest region;region.region={0,0,0,7,5};region.channels=3;region.z=1;region.t=1;RawAnalysisRequest request(*doc,region);const auto result=analyze_raw(request);const auto kept=doc->keep_raw_analysis(request,result);
    check(result->statistics().means==std::vector<std::optional<double>>{10123,11123,12123},"Project raw measurement changed");
    const auto saved_roi=doc->add_rectangle({0,0,7,5},"Saved plane");
    doc->set_object_plane(saved_roi,ObjectPlane(-1,1,1));
    const auto origin_roi=doc->add_rectangle({0,0,7,5},"Origin plane");
    doc->set_object_plane(origin_roi,ObjectPlane(-1,0,0));
    const auto channel_roi=doc->add_rectangle({0,0,7,5},"Channel one");
    doc->set_object_plane(channel_roi,ObjectPlane(1,1,1));
    const auto encoded=RawAnalysisCodec::encode(*doc->raw_analysis_record(kept),doc->metadata());check(encoded[3]==5 && RawAnalysisCodec::decode(encoded.data(),encoded.size(),doc->metadata())->result->source()->directory_sha256==result->source()->directory_sha256,"Natural directory raw record lost QRA5 identity");
    ScientificRawOptions grid_options;grid_options.channels=3;grid_options.z=1;grid_options.t=1;RawAnalysisRequest grid_request(*doc,ScientificGrid(7,5,{0,7,7},{0,5,5}),grid_options);const auto grid=analyze_raw(grid_request);const auto grid_id=doc->keep_raw_analysis(grid_request,grid);const auto grid_bytes=RawAnalysisCodec::encode(*doc->raw_analysis_record(grid_id),doc->metadata());check(grid_bytes[3]==6 && RawAnalysisCodec::decode(grid_bytes.data(),grid_bytes.size(),doc->metadata())->result->grid().has_value(),"Directory grid record lost QRA6 identity");doc->remove_raw_analysis(grid_id);
    const auto frame=image_coordinate_frame(*doc,"before");
    const auto path=root/"study.qpx";project.save_new(path.u8string());
    checks_explicit_store_relocation(path,store,id,kept);
    auto reopened=Project::open(path.u8string());auto restored=reopened->document(id);check(restored->server()->reader_id()=="ome-zarr" && !restored->server()->tiff_view() && restored->active_plane()==ObjectPlane(-1,1,1),"Saved source/view/plane did not reopen");
    check(restored->server()->ome_zarr_options()->source.chunk_bytes==options.source.chunk_bytes && restored->server()->ome_zarr_options()->work_bytes==options.work_bytes,"Saved resource limits changed");
    check(restored->server()->read_region({3,2,0,1,1}).bytes()==rgb,"Saved projection changed RGB");
    const auto replay=RawAnalysisRequest::replay(*restored,kept);check(restored->verify_raw_analysis(kept,replay,analyze_raw(replay)),"Saved raw record cannot replay natively");
    checks_raw_batch_planes(path,root,id,saved_roi,origin_roi,channel_roi);
    refuses_batch_output(*reopened,store/"direct-output");
    auto unselected=Project::open(path.u8string());
    const auto embedded=unselected->add_document(std::make_shared<Document>(Image(1,1,{0,0,0})),"Embedded control");
    refuses_batch_output(*unselected,store/"unselected-output",{{embedded,{}}});
    fs::rename(store,root/"offline.zarr");auto offline=Project::open(path.u8string());check(offline->document(id)->raw_analysis_record_count()==1 && offline->document(id)->active_plane()==ObjectPlane(-1,1,1),"Offline project cannot inspect saved view/records");
    check(offline->document(id)->annotation(saved_roi).plane==ObjectPlane(-1,1,1) &&
          offline->document(id)->annotation(origin_roi).plane==ObjectPlane{},"Offline project changed annotation planes");
    offline->save_new((root/"offline-saved.qpx").u8string());rejects([&]{offline->document(id)->server()->read_region({0,0,0,1,1});});refuses_batch_output(*offline,store/"offline-output");check(!fs::exists(store),"Batch recreated a missing source store");fs::rename(root/"offline.zarr",store);
    const auto fingerprint=fingerprint_file((store/".zattrs").u8string());auto review=reopened->review_source_verification(id,*fingerprint);check(review->coverage()=="directory_primary_only","Project review overclaims primary hash coverage");reopened->apply_source_review(review,false);
    {
        auto unopened=Project::open(path.u8string());
        const auto initial=unopened->review_source_verification(id,*fingerprint);
        check(!initial->requires_acceptance() && initial->comparison()=="unverified","Unchanged first source review required replacement approval");
        unopened->apply_source_review(initial);
        check(unopened->source_verification(id) &&
              unopened->source_verification(id)->source.directory_sha256==result->source()->directory_sha256,
              "First review did not retain the verified baseline");
    }
    {std::fstream file(store/"0/0.0.0.0.0",std::ios::binary|std::ios::in|std::ios::out);file.put(char(1));}
    auto stale=Project::open(path.u8string());rejects([&]{stale->document(id)->server()->read_region({3,2,0,1,1});});
    const auto first_review=stale->review_source_verification(id,*fingerprint);
    check(first_review->comparison()=="changed_dependencies" && first_review->requires_acceptance(),
          "First review after reopen ignored the persisted Zarr directory identity");
    const auto before_source=stale->document(id)->server();
    rejects([&]{stale->apply_source_review(first_review);});
    check(!stale->dirty() && stale->document(id)->server()==before_source &&
          !first_review->consumed() && !stale->source_verification(id),
          "Declining a changed Zarr store changed the saved baseline");
    stale->apply_source_review(first_review,true);
    check(first_review->consumed() && stale->source_verification(id)->source.directory_sha256!=
          result->source()->directory_sha256,"Accepted Zarr replacement did not publish its new observation");
    const auto replacement_path=root/"accepted.qpx";
    stale->save_new(replacement_path.u8string());
    const auto accepted=Project::open(replacement_path.u8string());
    const auto accepted_doc=accepted->document(id);
    accepted_doc->server()->require_raw_current();
    check(accepted_doc->server()->reader_source()->directory_sha256==
          accepted->source_verification(id)->source.directory_sha256 &&
          accepted_doc->raw_analysis_record(kept)->result->source()->directory_sha256==
          result->source()->directory_sha256,"Saved replacement lost the current or historical directory identity");
    checks_batch_source_verification(*accepted,root,id);
    const auto unchanged=fingerprint_file((store/".zattrs").u8string());check(unchanged->sha256==fingerprint->sha256,"Test unexpectedly changed primary metadata");
    auto changed=reopened->review_source_verification(id,*unchanged);check(changed->comparison()=="changed_dependencies" && changed->requires_acceptance(),"Changed chunk escaped metadata-only source review");rejects([&]{reopened->apply_source_review(changed,false);});reopened->apply_source_review(changed,true);
    check(image_coordinate_frame(*reopened->document(id),"before").source_id!=frame.source_id,"Registration image identity ignored changed directory");
    const auto broken=root/"bad.qpx";fs::copy_file(path,broken);sql(broken,"UPDATE tiff_views SET payload=x'000102'");rejects([&]{Project::open(broken.u8string());});
    detail::guard_file_output(store.u8string(),"",(root/"outside.npy").u8string());rejects([&]{detail::guard_file_output(store.u8string(),"",(store/"inside.npy").u8string());});
    if(argc==3){OmeZarrOpenOptions real;real.view.channels={0,1,-1};real.view.lower={7,0,0};real.view.upper={48,255,1};Project output;const auto actual=output.add_document(std::make_shared<Document>(open_ome_zarr(argv[1],real)));RawRegionRequest all;all.region={0,0,0,512,512};all.channels=2;auto document=output.document(actual);RawAnalysisRequest captured(*document,all);document->keep_raw_analysis(captured,analyze_raw(captured));output.save_new(argv[2]);auto saved=Project::open(argv[2]);check(saved->document(actual)->raw_analysis_record_count()==1 && saved->document(actual)->server()->read_region({0,0,0,512,512}).bytes()==document->server()->read_region({0,0,0,512,512}).bytes(),"Full real saved project changed projection/record");}
    std::cout<<"Native Zarr project view/options/offline records/QRA5/replay/directory review/frame identity checks pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
