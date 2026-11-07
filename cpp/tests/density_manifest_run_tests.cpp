#include "qupathxx/density_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/workflow.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace qpx;
using Json=nlohmann::json;
namespace fs=std::filesystem;
void checked(bool v,int line){if(!v)throw std::runtime_error("Manifest run failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects_at(F action,int line){bool refused=false;try{action();}catch(const std::exception&){refused=true;}checked(refused,line);}
#define rejects(...) rejects_at(__VA_ARGS__,__LINE__)
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::string& data){std::ofstream out(path,std::ios::binary);out<<data;if(!out)throw std::runtime_error("Fixture write failed");}
struct Scene {
    Project project;fs::path source,manifest;Json value;
    explicit Scene(const fs::path& root,bool retained=false) {
        fs::create_directory(root);source=root/"source.qpxproj";manifest=root/"selections.json";
        value={{"schema","qpx.project-density/1"},{"project","source.qpxproj"},{"images",Json::array()}};
        for(int i=0;i<2;++i) {
            auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));doc->set_calibration(2,.5);
            for(const auto* name:{"Window","Tissue","Tumor"})doc->set_class(name,0x008800);
            for(const auto bounds:{Rect{0,0,12,12},Rect{0,0,8,8}})doc->classify_annotation(doc->add_rectangle(bounds),"Window");
            doc->classify_annotation(doc->add_rectangle({0,0,10,10}),"Tissue");
            doc->add_cell(Roi::rectangle({1,1,2,2}),{},"Cell",0,"Tumor");doc->add_cell(Roi::rectangle({8,8,2,2}),{},"Cell",0,"Tumor");
            Json entry={{"image_id",i+1},{"windows",{1,2}},{"tissue",{3}},{"excluded",Json::array()},{"cells",{4,5}}};
            if(retained){SpatialDensityRequest request(*doc,{1,2},{3},{},{4,5});entry["record_id"]=doc->keep_density(request,measure_spatial_density(request));}
            value["images"].push_back(entry);project.add_document(doc,i?"Simple":"Case 1, \"quoted\"\nline");
        }
        project.save_new(source.u8string());save();
    }
    void save(){write(manifest,value.dump());}
    ProjectDensityReport run(const fs::path& output,ProjectDensityRunOptions options={},ProjectDensityProgress* p=nullptr){return run_project_density(load_project_density_manifest(manifest.u8string()),output.u8string(),options,nullptr,p);}
};
ProjectDensityReport controlled(Scene& scene,const fs::path& output,const std::function<void(const Json&,ProjectDensityJob&)>& control) {
    ProjectDensityRunOptions options;options.pause_after_image=true;ProjectDensityJob job;
    check(job.run(load_project_density_manifest(scene.manifest.u8string()),output.u8string(),options));
    check(!job.run(load_project_density_manifest(scene.manifest.u8string()),output.u8string(),options));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    std::vector<std::string> events;
    while(job.busy()) {
        for(const auto& event:job.take_images()){events.push_back(event);control(Json::parse(event),job);job.continue_images();}
        job.poll();
        if(job.busy() && std::chrono::steady_clock::now()>deadline){job.cancel();job.finish();check(false);}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(job.error().empty() && job.result());return *job.result();
}
int main(){try{
    Project id;const auto root=fs::current_path()/(id.identity()+"-manifest-run");fs::create_directory(root);
    Scene scene(root/"base");const auto source_before=read(scene.source),manifest_before=read(scene.manifest);
    ProjectDensityProgress progress;const auto first=scene.run(root/"new-parent/first",{},&progress);
    check(first.state=="completed" && first.computed==2 && first.rows==4 && first.failed==0 && progress.kernel_calls.load()==2);
    auto report=Json::parse(first.json);check(report==Json::parse(read(first.report)));
    check(report["schema"]=="qpx.project-density-report/1" && report["selected"]==2 && report["not_run"]==0 && report["images"].size()==2);
    check(report["images"][0]["state"]=="complete" && report["images"][0]["reuse"]=="computed");
    check(progress.take_images().size()==2 && progress.take_images().empty());
    const auto csv=read(first.csv);std::string expected=csv.substr(0,csv.find('\n')+1);
    for(const auto& row:first.images) {
        auto saved=Project::open_without_journal_recovery(row.checkpoint);auto doc=saved->document(row.checkpoint_image);auto stored=doc->density_record(row.record)->result;
        check(stored->sha256()==row.result_sha256 && stored->rows()[0].count==2 && stored->rows()[1].count==1);
        const auto native=spatial_density_csv(*stored);const auto prefix=scene.project.identity()+","+std::to_string(row.id)+","+
            (row.id==1?"\"Case 1, \"\"quoted\"\"\nline\"":"Simple")+","+std::to_string(row.record)+",computed,"+row.input_sha256+","+row.checkpoint+",";
        for(auto at=native.find('\n')+1;at<native.size();){const auto end=native.find('\n',at);expected+=prefix+native.substr(at,end-at+1);at=end+1;}
        check(Json::parse(read(root/"new-parent/first"/("image-"+std::to_string(row.id)+".json")))["schema"]=="qpx.project-density-checkpoint/1");
    }
    check(csv==expected && read(scene.source)==source_before && read(scene.manifest)==manifest_before);
    ProjectDensityRunOptions resume;resume.resume_from=(root/"new-parent/first").u8string();
    const auto reused=scene.run(root/"reused",resume,&progress);
    check(reused.state=="completed" && reused.reused==2 && reused.computed==0 && progress.kernel_calls.load()==0);
    check(!fs::exists(root/"reused/image-1.qpxproj") && reused.images[0].checkpoint==first.images[0].checkpoint);
    check(Json::parse(reused.json)["images"][0]["reuse"]=="checkpoint");
    resume.resume_from=(root/"reused").u8string();check(scene.run(root/"chained",resume,&progress).reused==2 && progress.kernel_calls.load()==0);
    auto cache=root/"legacy-cache";fs::create_directory(cache);
    for(int image:{1,2}){auto receipt=Json::parse(read(root/"new-parent/first"/("image-"+std::to_string(image)+".json")));receipt["legacy_note"]="preserve";write(cache/("image-"+std::to_string(image)+".json"),receipt.dump());}
    resume.resume_from=cache.u8string();const auto legacy=scene.run(root/"legacy",resume,&progress);check(legacy.reused==2 && Json::parse(legacy.json)["images"][0]["legacy_note"]=="preserve");
    auto changed_engine=Json::parse(read(cache/"image-1.json"));changed_engine["engine"]["source_tree_sha256"]=std::string(64,'0');write(cache/"image-1.json",changed_engine.dump());
    check(scene.run(root/"engine",resume,&progress).computed==1 && progress.kernel_calls.load()==1);
    write(cache/"image-1.json",read(root/"new-parent/first/image-1.json"));
    auto duplicate=read(cache/"image-1.json");duplicate.insert(1,"\"schema\":\"duplicate\",");write(cache/"image-1.json",duplicate);
    const auto bad_receipt=scene.run(root/"duplicate",resume,&progress);check(bad_receipt.failed==1 && bad_receipt.reused==1 && progress.kernel_calls.load()==0);
    resume.resume_from=(root/"new-parent/first").u8string();resume.recompute=true;
    check(scene.run(root/"forced",resume,&progress).computed==2 && progress.kernel_calls.load()==2);
    resume.recompute=false;scene.value["options"]={{"work_bytes",256u*1024*1024}};scene.save();check(scene.run(root/"larger",resume,&progress).reused==2 && progress.kernel_calls.load()==0);
    scene.value["options"]={{"work_bytes",64u*1024*1024}};scene.save();check(scene.run(root/"smaller",resume,&progress).computed==2 && progress.kernel_calls.load()==2);
    scene.value.erase("options");scene.save();scene.project.rename_image(1,"Renamed");scene.project.save(scene.source.u8string());
    check(scene.run(root/"metadata",resume,&progress).reused==2 && progress.kernel_calls.load()==0);
    scene.project.document(1)->update_rectangle(1,{0,0,8,8});scene.project.save(scene.source.u8string());
    check(scene.run(root/"geometry",resume,&progress).computed==1 && progress.kernel_calls.load()==1);
    Scene hints(root/"hints",true);const auto hinted=hints.run(root/"hinted",{},&progress);
    check(hinted.reused==2 && hinted.computed==0 && progress.kernel_calls.load()==0 && Json::parse(hinted.json)["images"][0]["reuse"]=="source_record");
    hints.value["images"][0]["record_id"]=999999;hints.save();check(hints.run(root/"missing-hint",{},&progress).computed==1 && progress.kernel_calls.load()==1);
    hints.project.document(2)->update_rectangle(1,{0,0,8,8});hints.project.save(hints.source.u8string());
    check(hints.run(root/"stale-hint",{},&progress).computed==2 && progress.kernel_calls.load()==2);
    Scene invalid(root/"invalid");invalid.value["images"][0]["cells"]={1};invalid.save();
    const auto partial=invalid.run(root/"partial");check(partial.state=="partial" && partial.failed==1 && partial.computed==1 && partial.not_run==0 && partial.rows==2);
    ProjectDensityRunOptions stop;stop.stop_on_error=true;const auto stopped=invalid.run(root/"stopped",stop);
    check(stopped.failed==1 && stopped.not_run==1 && Json::parse(stopped.json)["images"].size()==1);
    Scene budget(root/"budget");const auto baseline=budget.run(root/"budget-baseline");
    ProjectDensityRunOptions quota;const auto header=read(baseline.csv).find('\n')+1;quota.csv_bytes=header+1;
    const auto limited=budget.run(root/"limited",quota);check(limited.state=="partial" && limited.rows==0 && limited.failed==1 && limited.not_run==1);
    check(read(limited.csv).size()==header && fs::exists(root/"limited/image-1.json") && fs::exists(root/"limited/image-1.qpxproj"));
    resume.resume_from=(root/"limited").u8string();const auto recovered=budget.run(root/"recovered",resume,&progress);check(recovered.computed==1 && recovered.reused==1 && progress.kernel_calls.load()==1);

    auto future_csv=read(baseline.csv);const auto later=root/"later-limit";
    for(const auto& row:baseline.images){const auto next=(later/("image-"+std::to_string(row.id)+".qpxproj")).u8string();for(std::size_t at=0;(at=future_csv.find(row.checkpoint,at))!=std::string::npos;at+=next.size())future_csv.replace(at,row.checkpoint.size(),next);}
    const auto second=future_csv.find(budget.project.identity()+",2,Simple,");check(second!=std::string::npos);
    quota.csv_bytes=future_csv.find('\n',second)+2;check(quota.csv_bytes<future_csv.size());
    const auto later_limit=budget.run(later,quota);check(later_limit.computed==1 && later_limit.failed==1 && later_limit.rows==2 && later_limit.not_run==0);
    check(read(later_limit.csv)==future_csv.substr(0,second) && fs::exists(later/"image-2.json"));
    resume.resume_from=later.u8string();check(budget.run(root/"later-recovered",resume,&progress).reused==2 && progress.kernel_calls.load()==0);
    quota.csv_bytes=64u*1024*1024;quota.checkpoint_bytes=1;const auto no_save=budget.run(root/"no-save",quota);
    check(no_save.failed==2 && no_save.rows==0 && !fs::exists(root/"no-save/image-1.qpxproj"));
    quota.checkpoint_bytes=1024ull*1024*1024;quota.project_bytes=1;rejects([&]{budget.run(root/"project-limit",quota);});check(!fs::exists(root/"project-limit"));
    quota.project_bytes=1024ull*1024*1024;quota.csv_bytes=1;rejects([&]{budget.run(root/"header-limit",quota);});check(!fs::exists(root/"header-limit"));
    rejects([&]{budget.run(root/"limited");});resume.resume_from=(root/"limited").u8string();rejects([&]{budget.run(root/"limited/nested",resume);});
    Scene events(root/"events");const auto cancelled=controlled(events,root/"cancelled",[](const Json& event,ProjectDensityJob& job){check(event["state"]=="complete");job.cancel();});
    check(cancelled.state=="cancelled" && cancelled.computed==1 && cancelled.not_run==1 && cancelled.rows==2 && !fs::exists(root/"cancelled/image-2.qpxproj"));
    resume.resume_from=(root/"cancelled").u8string();check(events.run(root/"cancel-resume",resume,&progress).reused==1 && progress.kernel_calls.load()==1);
    Scene mutation(root/"mutation");const auto changed=controlled(mutation,root/"changed",[&](const Json& event,ProjectDensityJob&){if(event["image_id"]==1)write(mutation.manifest,read(mutation.manifest)+" ");});
    check(changed.state=="partial" && changed.computed==1 && changed.failed==1 && !changed.error.empty() && !fs::exists(root/"changed/density.csv"));

    Scene source_change(root/"source-change");const auto changed_source=controlled(source_change,root/"changed-source",[&](const Json& event,ProjectDensityJob&){if(event["image_id"]==1)write(source_change.source,read(source_change.source)+"changed");});
    check(changed_source.state=="partial" && changed_source.computed==1 && changed_source.failed==1 && !changed_source.error.empty());
    Scene stale_manifest(root/"stale-manifest");const auto loaded=load_project_density_manifest(stale_manifest.manifest.u8string());write(stale_manifest.manifest,read(stale_manifest.manifest)+" ");
    bool typed=false;try{run_project_density(loaded,(root/"stale-manifest-output").u8string());}catch(const WorkflowInputChanged&){typed=true;}
    check(typed && !fs::exists(root/"stale-manifest-output"));
    Scene checkpoint_change(root/"checkpoint-change");const auto checkpoint_result=controlled(checkpoint_change,root/"checkpoint-result",[&](const Json& event,ProjectDensityJob&){if(event["image_id"]==1){const auto path=event["checkpoint"].get<std::string>();write(path,read(path)+"changed");}});
    check(checkpoint_result.state=="partial" && !checkpoint_result.error.empty() && !fs::exists(root/"checkpoint-result/density.csv"));
    Scene sidecar(root/"sidecar");const auto sidecar_result=controlled(sidecar,root/"sidecar-result",[&](const Json& event,ProjectDensityJob&){if(event["image_id"]==1)write(fs::u8path(sidecar.source.u8string()+"-wal"),"keep");});
    check(sidecar_result.state=="partial" && sidecar_result.computed==1 && !sidecar_result.error.empty());

    Scene scratch(root/"scratch");const auto scratch_result=controlled(scratch,root/"scratch-result",[&](const Json& event,ProjectDensityJob&){if(event["image_id"]==1)write(root/"scratch-result/.image-2.json.part","keep");});
    check(scratch_result.computed==1 && scratch_result.failed==1 && read(root/"scratch-result/.image-2.json.part")=="keep" && fs::exists(root/"scratch-result/image-2.qpxproj") && !fs::exists(root/"scratch-result/image-2.json"));
    const auto linked_root=root/"linked";fs::create_directory(linked_root);Project linked;
    auto linked_doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));linked_doc->set_calibration(1,1);
    const auto area=linked_doc->add_rectangle({0,0,8,8}),cell=linked_doc->add_cell(Roi::rectangle({1,1,2,2}));
    const auto absent=linked_root/"absent.ppm";linked.add_document(linked_doc,"Missing linked source",absent.u8string(),"ppm");
    const auto linked_source=linked_root/"source.qpxproj",linked_manifest=linked_root/"manifest.json";linked.save_new(linked_source.u8string());
    const Json linked_value={{"schema","qpx.project-density/1"},{"project",linked_source.u8string()},{"images",Json::array({{{"image_id",1},{"windows",{area}},{"tissue",{area}},{"excluded",Json::array()},{"cells",{cell}}}})}};
    write(linked_manifest,linked_value.dump());const auto linked_report=run_project_density(load_project_density_manifest(linked_manifest.u8string()),(linked_root/"run").u8string());
    check(linked_report.state=="completed" && !fs::exists(absent));
    const auto linked_checkpoint=Project::open_without_journal_recovery(linked_report.images[0].checkpoint)->images()[0];check(!linked_checkpoint.embedded && fs::u8path(linked_checkpoint.source_path)==absent);
    const auto protected_output=linked_root/"protected";Project protected_project;
    protected_project.add_document(linked_doc,"Protected missing source",(protected_output/"missing.ppm").u8string(),"ppm");
    const auto protected_source=linked_root/"protected-source.qpxproj",protected_manifest=linked_root/"protected-manifest.json";protected_project.save_new(protected_source.u8string());
    auto protected_value=linked_value;protected_value["project"]=protected_source.u8string();write(protected_manifest,protected_value.dump());
    rejects([&]{run_project_density(load_project_density_manifest(protected_manifest.u8string()),protected_output.u8string());});check(!fs::exists(protected_output));
    ProjectDensityRecipe classes;classes.images={1,2};classes.classes[0]={"Window"};classes.classes[1]={"Tissue"};
    const auto gui=run_project_density(preview_project_density(events.source.u8string(),classes),(root/"gui").u8string());
    resume.resume_from=(root/"gui").u8string();check(events.run(root/"refuse-gui",resume).failed==2);
    const auto gui_refuse=run_project_density(preview_project_density(events.source.u8string(),classes),(root/"gui-refuse").u8string(),nullptr,nullptr,64u*1024*1024,(root/"cancel-resume").u8string());check(gui_refuse.failed==2);
    const auto corrupt_path=fs::u8path(baseline.images[0].checkpoint);write(corrupt_path,read(corrupt_path)+"changed");const auto damaged=read(corrupt_path);
    resume.resume_from=(root/"budget-baseline").u8string();const auto corrupt=budget.run(root/"corrupt",resume,&progress);
    check(corrupt.failed==1 && corrupt.reused==1 && progress.kernel_calls.load()==0 && corrupt.images[0].error.find("checksum")!=std::string::npos && read(corrupt_path)==damaged);
    std::cout<<"Native manifest run: native CSV/records, legacy adapters, hints/reuse/no-kernel, mutation, limits, atomic rows, progress acknowledgements/cancellation and no-overwrite passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
