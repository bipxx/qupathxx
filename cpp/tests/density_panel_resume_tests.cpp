#include "../project_density_panel.hpp"
#include "../batch_checkpoint.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace qpx;
using namespace qpx::detail;
namespace fs=std::filesystem;
void checked(bool ok,int line){if(!ok)throw std::runtime_error("density resume check at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F fn){bool hit=false;try{fn();}catch(const std::exception&){hit=true;}check(hit);}
std::string read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& p,const std::string& text){std::ofstream out(p,std::ios::binary);out<<text;}
void replace(std::string& text,const std::string& from,const std::string& to){const auto at=text.find(from);check(at!=std::string::npos);text.replace(at,from.size(),to);}
int main(){try{
    Project project;const auto root=fs::current_path()/(project.identity()+"-density-resume");fs::create_directory(root);
    for(int i=0;i<2;++i){
        auto doc=std::make_shared<Document>(Image(24,24,std::vector<std::uint8_t>(24*24*3,80)));doc->set_calibration(.5,.25);
        for(const auto& name:{"Windows","Tissue","Tumor"})doc->set_class(name,0x008800);
        auto window=doc->add_rectangle({0,0,12,12});doc->classify_annotation(window,"Windows");
        auto tissue=doc->add_rectangle({0,0,10,10});doc->classify_annotation(tissue,"Tissue");
        for(int x:{2,5,8})doc->add_cell(Roi::rectangle({double(x)-1,double(x)-1,2,2}),{},"Cell",0,"Tumor");
        project.add_document(doc,"Image "+std::to_string(i));
    }
    const auto source=root/"source.qpxproj";project.save_new(source.u8string());const auto original=read(source);
    DensityPanelRecipe recipe;recipe.images={1,2};recipe.classes[0]={"Windows"};recipe.classes[1]={"Tissue"};
    auto preview=preview_project_density(source.u8string(),recipe);check(preview.valid());
    const auto first=run_project_density(preview,(root/"first").u8string());check(first.computed==2 && first.reused==0);
    const auto previous_bytes=read(first.images[0].checkpoint),receipt_bytes=read(root/"first/image-1.json");
    const auto resume=[&](const DensityPanelReport& p,const std::string& name,const fs::path& previous,DensityPanelProgress& progress){
        return run_project_density(p,(root/name).u8string(),nullptr,&progress,64u*1024*1024,previous.u8string());};
    DensityPanelProgress p;
    const auto reused=resume(preview,"reused",root/"first",p);
    check(reused.state=="completed" && reused.reused==2 && reused.computed==0 && p.kernel_calls.load()==0);
    check(reused.images[0].checkpoint==first.images[0].checkpoint && reused.images[0].result_sha256==first.images[0].result_sha256);
    check(!fs::exists(root/"reused/image-1.qpxproj") && read(root/"reused/density.csv").find(",reused,")!=std::string::npos);
    const auto chained=resume(preview,"chained",root/"reused",p);check(chained.reused==2 && p.kernel_calls.load()==0);
    check(read(first.images[0].checkpoint)==previous_bytes && read(root/"first/image-1.json")==receipt_bytes && read(source)==original);
    const auto cache=[&](const std::string& name,bool second=true){auto dir=root/name;fs::create_directory(dir);fs::copy_file(root/"first/image-1.json",dir/"image-1.json");if(second)fs::copy_file(root/"first/image-2.json",dir/"image-2.json");return dir;};
    const auto partial=cache("partial-cache",false);p.kernel_calls.store(0);
    const auto resumed_partial=resume(preview,"partial-resume",partial,p);check(resumed_partial.reused==1 && resumed_partial.computed==1 && p.kernel_calls.load()==1);
    auto small=preview;small.recipe.options.work_bytes/=2;p.kernel_calls.store(0);
    const auto admitted=resume(small,"smaller-budget",root/"first",p);check(admitted.computed==2 && p.kernel_calls.load()==2);
    const auto engine_dir=cache("engine-cache");auto text=read(engine_dir/"image-1.json");
    replace(text,build_environment()->source_tree_sha256,std::string(64,'0'));write(engine_dir/"image-1.json",text);p.kernel_calls.store(0);
    const auto changed_engine=resume(preview,"engine-run",engine_dir,p);check(changed_engine.computed==1 && changed_engine.reused==1 && p.kernel_calls.load()==1);
    check(changed_engine.images[0].reuse_reason=="Engine identity changed");
    const auto duplicate=cache("duplicate-cache");text=read(duplicate/"image-1.json");text.insert(1,"\"schema\":\"duplicate\",");write(duplicate/"image-1.json",text);p.kernel_calls.store(0);
    const auto duplicate_result=resume(preview,"duplicate-run",duplicate,p);check(duplicate_result.failed==1 && duplicate_result.reused==1 && p.kernel_calls.load()==0);
    const auto broken=cache("broken-cache");const auto bad_file=broken/"bad.qpxproj";fs::copy_file(first.images[0].checkpoint,bad_file);
    text=read(broken/"image-1.json");replace(text,batch_json_quote(first.images[0].checkpoint),batch_json_quote(bad_file.u8string()));write(broken/"image-1.json",text);
    {std::ofstream append(bad_file,std::ios::app|std::ios::binary);append<<"changed";}const auto bad_bytes=read(bad_file);p.kernel_calls.store(0);
    const auto corrupt=resume(preview,"corrupt-run",broken,p);check(corrupt.failed==1 && corrupt.reused==1 && p.kernel_calls.load()==0);
    check(corrupt.images[0].error.find("checksum")!=std::string::npos && read(bad_file)==bad_bytes);
    const auto stale_dir=cache("stale-cache");const auto stale_file=stale_dir/"stale.qpxproj";auto saved=Project::open(first.images[0].checkpoint);
    saved->document(first.images[0].checkpoint_image)->update_rectangle(1,{0,0,8,8});saved->save_new(stale_file.u8string());
    text=read(stale_dir/"image-1.json");replace(text,batch_json_quote(first.images[0].checkpoint),batch_json_quote(stale_file.u8string()));
    replace(text,first.images[0].checkpoint_sha256,fingerprint_file(stale_file.u8string())->sha256);write(stale_dir/"image-1.json",text);p.kernel_calls.store(0);
    const auto stale=resume(preview,"stale-run",stale_dir,p);check(stale.failed==1 && stale.reused==1 && p.kernel_calls.load()==0);
    rejects([&]{run_project_density(preview,(root/"first/nested").u8string(),nullptr,nullptr,64u*1024*1024,(root/"first").u8string());});
    if(fs::exists(root/"FIRST"))rejects([&]{run_project_density(preview,(root/"FIRST/nested-case").u8string(),nullptr,nullptr,64u*1024*1024,(root/"first").u8string());});
    project.rename_image(1,"Renamed, unrelated metadata");project.save(source.u8string());preview=preview_project_density(source.u8string(),recipe);p.kernel_calls.store(0);
    const auto unrelated=resume(preview,"unrelated",root/"first",p);check(unrelated.reused==2 && p.kernel_calls.load()==0);
    project.document(1)->update_rectangle(1,{0,0,8,8});project.save(source.u8string());preview=preview_project_density(source.u8string(),recipe);p.kernel_calls.store(0);
    const auto relevant=resume(preview,"relevant",root/"first",p);check(relevant.computed==1 && relevant.reused==1 && p.kernel_calls.load()==1);
    project.document(2)->set_calibration(.75,.25);project.save(source.u8string());preview=preview_project_density(source.u8string(),recipe);p.kernel_calls.store(0);
    const auto calibrated=resume(preview,"calibrated",root/"first",p);check(calibrated.computed==2 && p.kernel_calls.load()==2);
    std::cout<<"density resume checks passed: no-kernel reuse/chains, partial receipts, unrelated/relevant edits, calibration, budgets, engine changes, corruption/replay refusal, no copies/overwrite\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
