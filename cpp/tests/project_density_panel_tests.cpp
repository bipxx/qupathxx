#include "../atomic_export.hpp"
#include "../project_density_panel.hpp"
#include "qupathxx/project.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace qpx;
using namespace qpx::detail;
namespace fs=std::filesystem;
void checked(bool ok,int line){if(!ok)throw std::runtime_error("density panel check at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F fn){bool hit=false;try{fn();}catch(const std::exception&){hit=true;}check(hit);}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
DensityPanelRecipe recipe(){DensityPanelRecipe r;r.images={2,1};r.classes[0]={"Windows"};r.classes[1]={"Tissue"};r.classes[2]={"Excluded"};return r;}
void area(Document& doc,Rect bounds,const std::string& name){const auto id=doc.add_rectangle(bounds);doc.classify_annotation(id,name);}
std::shared_ptr<Project> scene(const fs::path& source,bool slow=false){
    auto project=std::make_shared<Project>();
    for(int i=0;i<(slow?3:2);++i){
        auto doc=std::make_shared<Document>(Image(24,24,std::vector<std::uint8_t>(24*24*3,80)));
        doc->set_calibration(i?.5:2,i?.25:.5);
        for(const auto& name:{"Windows","Tissue","Excluded","Tumor"})doc->set_class(name,0x008800);
        area(*doc,{0,0,12,12},"Windows");area(*doc,{0,0,10,10},"Tissue");area(*doc,{4,0,2,10},"Excluded");
        if(i==1 && slow){for(int n=0;n<199;++n)area(*doc,{0,0,12,12},"Windows");for(int n=0;n<5000;++n)doc->add_cell(Roi::rectangle({1,1,2,2}),{},"Cell",0,"Tumor");}
        else for(int x:{2,5,8})doc->add_cell(Roi::rectangle({double(x)-1,double(x)-1,2,2}),{},"Cell",0,"Tumor");
        project->add_document(doc,i==0?"Case 1, \"quoted\"\nline":i==1?"Case 2":"Case 3");
    }project->save_new(source.u8string());return project;
}
int main(int argc,char** argv){try{
    if(argc==3) {
        const std::string source=argv[1];const auto before=read(fs::u8path(source));
        DensityPanelRecipe r;r.images={1,2};r.classes[0]={"Analysis windows"};r.classes[1]={"Annotated FOV support"};
        const auto preview=preview_project_density(source,r);check(preview.valid());
        check(preview.images[0].ids[3].size()==32 && preview.images[1].ids[3].size()==28);
        const auto report=run_project_density(preview,argv[2]);check(report.state=="completed" && report.rows==2);
        for(std::size_t i=0;i<report.images.size();++i) {
            const auto& row=report.images[i];auto saved=Project::open(row.checkpoint);auto doc=saved->document(row.checkpoint_image);
            const auto stored=doc->density_record(row.record)->result;
            check(stored->sha256()==row.result_sha256);
            check(stored->rows()[0].count==(i?27:31) && stored->rows()[0].area_pixels==(i?101124:104006));
            auto replay=SpatialDensityRequest::replay(*doc,row.record);check(doc->verify_density(row.record,replay,measure_spatial_density(replay)));
        }
        check(read(fs::u8path(source))==before);
        std::cout<<"public density panel route passed: 32/28 Cells, 31/27 in FOV, native records replay, source unchanged\n";
        return 0;
    }
    Project id;const auto root=fs::current_path()/(id.identity()+"-density-panel");fs::create_directory(root);
    const auto source=root/"source.qpxproj";auto project=scene(source);const auto original=read(source);
    auto r=recipe();auto preview=preview_project_density(source.u8string(),r);
    check(preview.valid() && preview.images.size()==2 && preview.images[0].id==1);
    check(preview.images[0].ids[0].size()==1 && preview.images[0].ids[3].size()==3 && *preview.images[1].x==.5);
    check(read(source)==original && !project->dirty());
    auto report=run_project_density(preview,(root/"run").u8string());
    check(report.state=="completed" && report.completed==2 && report.rows==2 && report.failed==0);
    check(report.csv.find("density.csv")!=std::string::npos && fs::exists(report.report));
    const auto combined=read(report.csv);check(combined.find("\"Case 1, \"\"quoted\"\"\nline\"")!=std::string::npos);
    for(const auto& row:report.images){
        auto saved=Project::open(row.checkpoint);auto doc=saved->document(row.checkpoint_image);
        auto request=SpatialDensityRequest::replay(*doc,row.record);const auto result=measure_spatial_density(request);
        check(result->rows()[0].count==2 && result->rows()[0].area_pixels==80);
        const auto stored=doc->density_record(row.record)->result;
        check(stored->sha256()==row.result_sha256);
        check(doc->verify_density(row.record,request,result));
        const auto csv=spatial_density_csv(*stored);check(combined.find(csv.substr(csv.find('\n')+1))!=std::string::npos);
    }
    rejects([&]{run_project_density(preview,(root/"run").u8string());});check(read(source)==original);
    const auto quota=run_project_density(preview,(root/"csv-quota").u8string(),nullptr,nullptr,400);
    check(quota.state=="partial" && quota.rows==0 && fs::exists(quota.report));
    check(read(quota.csv).find("polygonize_curves\n")!=std::string::npos);
    auto wrong=r;wrong.images={1,1};rejects([&]{preview_project_density(source.u8string(),wrong);});
    wrong=r;wrong.classes[0]={"windows"};check(!preview_project_density(source.u8string(),wrong).valid());
    wrong=r;wrong.images={999};check(!preview_project_density(source.u8string(),wrong).valid());
    wrong=r;wrong.all_cells=false;wrong.classes[3]={"Tumor"};check(preview_project_density(source.u8string(),wrong).valid());
    wrong.classes[3]={"Tissue"};check(!preview_project_density(source.u8string(),wrong).valid());
    project->document(2)->clear_calibration();project->save(source.u8string());
    check(!preview_project_density(source.u8string(),r).valid());
    rejects([&]{run_project_density(preview,(root/"stale").u8string());});check(!fs::exists(root/"stale"));
    {std::ofstream sidecar(source.u8string()+"-wal");sidecar<<"preserve";}
    rejects([&]{preview_project_density(source.u8string(),r);});check(read(source.u8string()+"-wal")=="preserve");
    // Separate valid fixture, preserving the intentionally invalid source above.
    const auto slow_source=root/"slow.qpxproj";auto slow=scene(slow_source,true);const auto slow_bytes=read(slow_source);
    auto slow_preview=preview_project_density(slow_source.u8string(),r);
    check(slow_preview.valid());
    std::atomic<bool> cancel{false};DensityPanelProgress progress;
    auto future=std::async(std::launch::async,[&]{return run_project_density(slow_preview,(root/"cancel").u8string(),&cancel,&progress);});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(progress.completed.load()<1 && future.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready){if(std::chrono::steady_clock::now()>=deadline){cancel.store(true);future.wait();check(false);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(progress.completed.load()==1);cancel.store(true);const auto cancelled=future.get();
    check(cancelled.state=="cancelled" && cancelled.completed==1 && cancelled.rows==1);
    check(cancelled.csv.find("partial")!=std::string::npos && fs::exists(cancelled.images[0].checkpoint));
    check(read(slow_source)==slow_bytes);
    // A user-created checkpoint collision fails that image and preserves it.
    cancel.store(false);progress.completed.store(0);progress.image.store(0);
    auto three=r;three.images={1,2,3};const auto three_preview=preview_project_density(slow_source.u8string(),three);
    auto collision=std::async(std::launch::async,[&]{return run_project_density(three_preview,(root/"collision").u8string(),&cancel,&progress);});
    const auto collision_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(progress.image.load()!=2 && collision.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready) {
        if(std::chrono::steady_clock::now()>=collision_deadline) {cancel.store(true);collision.wait();check(false);}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(progress.image.load()!=2 || collision.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
        cancel.store(true);collision.wait();check(false);
    }
    {AtomicExportFile kept((root/"collision/image-2.qpxproj").u8string(),128);const std::string text="preserve user file";kept.write(text.data(),text.size());kept.commit(false);}
    const auto partial=collision.get();check(partial.state=="partial" && partial.completed==2 && partial.failed==1 && partial.images[2].state=="saved");
    check(read(root/"collision/image-2.qpxproj")=="preserve user file" && !partial.images[1].error.empty());
    ProjectDensityPanelJob job;check(job.preview(slow_source.u8string(),r));check(!job.preview(slow_source.u8string(),r));
    const auto job_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(job.busy()) {
        job.poll();
        if(job.busy() && std::chrono::steady_clock::now()>=job_deadline) {job.cancel();job.finish();check(false);}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(job.error().empty() && job.result()->valid());
    std::cout<<"density panel checks passed: preview, selection, calibration, CSV/checkpoints, stale source, cancellation, per-image failure, async job\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
