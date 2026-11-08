#include "qupathxx/density_workflow.hpp"
#include "qupathxx/nucls_import.hpp"
#include "qupathxx/project.hpp"
#include "nucls_png_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace qpx;
namespace fs=std::filesystem;
void checked(bool value,int line){if(!value)throw std::runtime_error("Workflow contract check at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E,class F>void rejects_at(F action,int line){bool hit=false;try{action();}catch(const E&){hit=true;}checked(hit,line);}
#define rejects(E,...) rejects_at<E>(__VA_ARGS__,__LINE__)
void write(const fs::path& path,const std::string& data){std::ofstream out(path,std::ios::binary);out.write(data.data(),data.size());if(!out)throw std::runtime_error("Fixture write failed");}
template<class Job>void done(Job& job){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(job.busy()){job.poll();if(job.busy() && std::chrono::steady_clock::now()>deadline){job.finish();check(false);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
template<class Ready>void until(Ready ready){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(!ready()){check(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(){try{
    Project source;const auto root=fs::current_path()/(source.identity()+"-workflow-contract");fs::create_directory(root);
    for(unsigned i=0;i<2;++i){auto doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));doc->set_calibration(1,1);doc->set_class("W",0);doc->set_class("T",0);doc->classify_annotation(doc->add_rectangle({0,0,8,8}),"W");doc->classify_annotation(doc->add_rectangle({0,0,8,8}),"T");doc->add_cell(Roi::rectangle({1,1,1,1}));source.add_document(doc);}
    const auto source_path=root/"source.qpxproj";source.save_new(source_path.u8string());
    ProjectDensityRecipe recipe;recipe.images={1,2};recipe.classes[0]={"W"};recipe.classes[1]={"T"};
    ProjectDensityJob job;const std::string overlong(32769,'x');
    rejects(std::invalid_argument,[&]{job.preview(overlong,recipe);});check(!job.busy());
    check(job.preview((root/"missing.qpxproj").u8string(),recipe));done(job);
    check(!job.error().empty() && !job.result());rejects(fs::filesystem_error,[&]{job.rethrow_error();});
    check(job.preview(source_path.u8string(),recipe));done(job);job.rethrow_error();check(job.error().empty() && job.result()->valid());
    auto forged=*job.result();forged.images.push_back(forged.images[0]);rejects(std::invalid_argument,[&]{job.run(forged,(root/"forged").u8string());});check(!job.busy() && !fs::exists(root/"forged"));
    auto captured_recipe=recipe;const auto manifest_path=root/"manifest.json";check(job.generate_manifest(source_path.u8string(),captured_recipe,manifest_path.u8string()));captured_recipe.classes[0]={"Missing"};done(job);
    job.rethrow_error();check(job.manifest_result() && job.manifest_result()->written && job.manifest_result()->preview.valid() && !job.result());
    const auto manifest=load_project_density_manifest(manifest_path.u8string());const auto existing=root/"existing";fs::create_directory(existing);
    check(job.run(manifest,existing.u8string()));done(job);rejects(WorkflowOutputExists,[&]{job.rethrow_error();});
    ProjectDensityRunOptions tiny;tiny.csv_bytes=1;check(job.run(manifest,(root/"csv-limit").u8string(),tiny));done(job);rejects(ProjectDensityCsvBudget,[&]{job.rethrow_error();});check(!fs::exists(root/"csv-limit"));
    ProjectDensityRunOptions paused;paused.pause_after_image=true;check(job.run(manifest,(root/"foreign").u8string(),paused));until([&]{return job.progress().completed.load()==1;});
    auto foreign=std::async(std::launch::async,[&]{rejects(std::logic_error,[&]{job.poll();});rejects(std::logic_error,[&]{job.result();});rejects(std::logic_error,[&]{job.continue_images();});job.cancel();});foreign.get();done(job);job.rethrow_error();check(job.result()->state=="cancelled" && job.result()->completed==1);
    check(job.run(manifest,(root/"control-failure").u8string(),paused));until([&]{return job.progress().completed.load()==1;});job.abort_control("client callback failure");done(job);job.rethrow_error();
    check(job.result()->state=="partial" && job.result()->error=="client callback failure" && job.result()->completed==1 && job.result()->not_run==1);
    auto doomed=std::make_unique<ProjectDensityJob>();check(doomed->run(manifest,(root/"destructor").u8string(),paused));until([&]{return doomed->progress().completed.load()==1;});
    auto retire=std::async(std::launch::async,[value=std::move(doomed)]() mutable {value.reset();});check(retire.wait_for(std::chrono::seconds(20))==std::future_status::ready);retire.get();check(fs::exists(root/"destructor/density.partial.csv"));
    std::atomic<bool> cancelled{true};rejects(WorkflowCancelled,[&]{load_project_density_manifest(manifest_path.u8string(),&cancelled);});
    rejects(WorkflowCancelled,[&]{preview_project_density(source_path.u8string(),recipe,&cancelled);});
    const auto rgb=root/"rgb",csv=root/"csv";fs::create_directory(rgb);fs::create_directory(csv);
    const std::vector<std::uint8_t> pixels(3*3*3,80);write(rgb/"Case.png",nucls_png_fixture::png(3,3,nucls_png_fixture::filtered(pixels,3,3,0)));
    const std::string header="raw_classification,main_classification,type,xmin,ymin,xmax,ymax,coords_x,coords_y\n";
    const std::string row="source,Class,rectangle,1,1,2,2,,\n";write(csv/"Case.csv",header+row);
    NuclsImportOptions options;options.mpp_x=.4;options.mpp_y=.7;options.pause_after_pair=true;NuclsImportJob importer;
    rejects(std::invalid_argument,[&]{importer.run(overlong,csv.u8string(),options);});check(!importer.busy());
    check(importer.run(rgb.u8string(),csv.u8string(),options,(root/"owned.qpxproj").u8string()));until([&]{return importer.progress().pairs.load()==1;});options.limits.file_bytes=1;options.representation="centroids";importer.continue_pairs();done(importer);importer.rethrow_error();
    check(importer.result() && importer.result()->written && importer.result()->options.representation=="outlines");
    options={};options.mpp_x=.4;options.mpp_y=.7;check(importer.run(rgb.u8string(),csv.u8string(),options,(root/"owned.qpxproj").u8string()));done(importer);rejects(WorkflowOutputExists,[&]{importer.rethrow_error();});
    options.pause_after_pair=true;check(importer.run(rgb.u8string(),csv.u8string(),options,(root/"cancel-import.qpxproj").u8string()));until([&]{return importer.progress().pairs.load()==1;});
    auto foreign_import=std::async(std::launch::async,[&]{rejects(std::logic_error,[&]{importer.poll();});rejects(std::logic_error,[&]{importer.progress();});importer.cancel();});foreign_import.get();done(importer);importer.rethrow_error();check(importer.result()->cancelled && !fs::exists(root/"cancel-import.qpxproj"));
    auto discard=std::make_unique<NuclsImportJob>();check(discard->run(rgb.u8string(),csv.u8string(),options,(root/"retired.qpxproj").u8string()));until([&]{return discard->progress().pairs.load()==1;});
    auto retire_import=std::async(std::launch::async,[value=std::move(discard)]() mutable {value.reset();});check(retire_import.wait_for(std::chrono::seconds(20))==std::future_status::ready);retire_import.get();check(!fs::exists(root/"retired.qpxproj"));
    options.pause_after_pair=false;rejects(WorkflowCancelled,[&]{import_nucls(rgb.u8string(),csv.u8string(),options,"",&cancelled);});
    options.limits.capture_bytes=1024*1024;std::string metadata(4096,'m');std::string data=header;for(unsigned i=0;i<300;++i)data+=metadata+",Class,rectangle,1,1,2,2,,\n";write(csv/"Case.csv",data);
    const auto bounded=import_nucls(rgb.u8string(),csv.u8string(),options,(root/"capture-limit.qpxproj").u8string());check(!bounded.valid && !bounded.written && bounded.images.size()==1 && bounded.images[0].issue_count>0 && !fs::exists(root/"capture-limit.qpxproj"));check(bounded.images[0].issues[0].find("capture")!=std::string::npos);
    std::cout<<"Workflow contracts: owner controls/cross-thread cancel/destruction, typed worker exceptions, native manifest jobs, owned arguments, control failure and incremental capture admission passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
