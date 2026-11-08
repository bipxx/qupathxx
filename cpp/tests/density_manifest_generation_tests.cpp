#include "qupathxx/density_workflow.hpp"
#include "qupathxx/project.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
using namespace qpx;
void checked(bool v,int line){if(!v)throw std::runtime_error("Manifest generation failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F f){bool hit=false;try{f();}catch(const std::exception&){hit=true;}check(hit);}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::string& data){std::ofstream out(path,std::ios::binary);out<<data;if(!out)throw std::runtime_error("Fixture write failed");}
ProjectDensityRecipe selectors(){ProjectDensityRecipe r;r.all_images=true;r.classes[0]={"Window"};r.classes[1]={"Tissue"};return r;}
int main(){try{
    Project project;const auto root=fs::current_path()/(project.identity()+"-manifest-generation");fs::create_directory(root);
    const auto path=root/"source.qpxproj";
    for(int i=0;i<2;++i) {
        auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));doc->set_calibration(2,.5);
        for(const auto* name:{"Window","Tissue","Other","Tumor"})doc->set_class(name,0x008800);
        doc->classify_annotation(doc->add_rectangle({0,0,12,12}),"Window");
        doc->classify_annotation(doc->add_rectangle({0,0,10,10}),"Tissue");
        if(i) {
            doc->add_cell(Roi::rectangle({1,1,2,2}),{},"Cell",0,"Tumor");
            doc->add_cell(Roi::rectangle({4,4,2,2}),{},"Cell",0,"Other");
        }
        project.add_document(doc,i?"second":"case, \"quoted\"\nline");
    }
    project.save_new(path.u8string());const auto before=read(path);auto recipe=selectors();
    const auto preview=generate_project_density_manifest(path.u8string(),recipe);
    check(preview.preview.valid() && !preview.written && preview.output.empty());
    check(preview.preview.images.size()==2 && preview.preview.images[0].ids[3].empty() && preview.preview.images[1].ids[3].size()==2);
    check(preview.preview.images[0].x==2 && preview.preview.images[0].y==.5);
    check(preview.json.find("\"written\": false")!=std::string::npos && preview.json.find("\"calibration_um\"")!=std::string::npos);
    check(read(path)==before && !project.dirty() && project.document(1)->density_records().empty());
    const auto output=root/"manifest.json";const auto generated=generate_project_density_manifest(path.u8string(),recipe,output.u8string());
    check(generated.written && read(output)==preview.manifest_json && generated.manifest_json==preview.manifest_json);
    const auto loaded=load_project_density_manifest(output.u8string());
    check(loaded.project()==fs::canonical(path).u8string() && loaded.recipe().selections.size()==2);
    check(loaded.recipe().selections[1].ids==preview.preview.images[1].ids);
    check(preview_project_density(loaded.project(),loaded.recipe()).valid());
    rejects([&]{generate_project_density_manifest(path.u8string(),recipe,output.u8string());});check(read(output)==preview.manifest_json);
    const auto blocked=root/"scratch.json";const auto scratch=root/".scratch.json.part";write(scratch,"keep");
    rejects([&]{generate_project_density_manifest(path.u8string(),recipe,blocked.u8string());});check(read(scratch)=="keep" && !fs::exists(blocked));
    auto chosen=recipe;chosen.all_images=false;chosen.images={2};chosen.all_cells=false;chosen.classes[3]={"Tumor","Tumor"};
    const auto selected=generate_project_density_manifest(path.u8string(),chosen);
    check(selected.preview.valid() && selected.preview.images.size()==1 && selected.preview.images[0].ids[3]==std::vector<std::uint64_t>{3});
    check(selected.preview.recipe.classes[3]==std::vector<std::string>{"Tumor"});
    chosen.images={1};check(!generate_project_density_manifest(path.u8string(),chosen).preview.valid());
    chosen.images={999};rejects([&]{generate_project_density_manifest(path.u8string(),chosen);});
    chosen.images={2,2};rejects([&]{generate_project_density_manifest(path.u8string(),chosen);});
    chosen=recipe;chosen.all_images=false;rejects([&]{generate_project_density_manifest(path.u8string(),chosen);});
    chosen=recipe;chosen.images={1};rejects([&]{generate_project_density_manifest(path.u8string(),chosen);});
    chosen=recipe;chosen.classes[0]={"window","Window: child"};chosen.classes[1]={"Absent"};
    const auto invalid=generate_project_density_manifest(path.u8string(),chosen,(root/"invalid.json").u8string());
    check(!invalid.preview.valid() && !invalid.written && !fs::exists(root/"invalid.json"));
    check(invalid.preview.images[0].issues.size()==3 && invalid.preview.images[1].issues.size()==3);
    chosen=recipe;chosen.all_cells=false;chosen.classes[3]={"Tissue"};check(!generate_project_density_manifest(path.u8string(),chosen).preview.valid());
    chosen=recipe;chosen.options.snapshot_bytes=1;rejects([&]{generate_project_density_manifest(path.u8string(),chosen);});
    chosen=recipe;chosen.options.prefer_nucleus=false;chosen.options.polygonize_curves=true;chosen.options.work_bytes*=2;
    const auto policy=root/"policy.json";generate_project_density_manifest(path.u8string(),chosen,policy.u8string());
    const auto policy_loaded=load_project_density_manifest(policy.u8string());
    check(!policy_loaded.recipe().options.prefer_nucleus && policy_loaded.recipe().options.polygonize_curves && policy_loaded.recipe().options.work_bytes==chosen.options.work_bytes);
    std::atomic<bool> stop{true};rejects([&]{generate_project_density_manifest(path.u8string(),recipe,(root/"cancel.json").u8string(),&stop);});check(!fs::exists(root/"cancel.json"));
    check(read(path)==before);
    auto doc=project.document(2);doc->clear_calibration();project.save(path.u8string());
    const auto calibration=generate_project_density_manifest(path.u8string(),recipe,(root/"calibration.json").u8string());
    check(!calibration.preview.valid() && calibration.preview.images[0].issues.empty() && !calibration.preview.images[1].issues.empty() && !fs::exists(root/"calibration.json"));
    doc->set_calibration(2,.5);doc->classify_annotation(doc->add_roi(Roi::ellipse({1,1,8,8})),"Window");project.save(path.u8string());
    check(!generate_project_density_manifest(path.u8string(),recipe).preview.valid());recipe.options.polygonize_curves=true;
    check(generate_project_density_manifest(path.u8string(),recipe).preview.valid());
    write(fs::u8path(path.u8string()+"-wal"),"keep");rejects([&]{generate_project_density_manifest(path.u8string(),recipe);});
    std::cout<<"Native manifest generation: exact class/all-image selection, all issues, no science execution, roundtrip/policy, calibration/curve admission, cancellation and no-overwrite passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
