#include "qupathxx/density_workflow.hpp"
#include "qupathxx/project.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
namespace fs=std::filesystem;
using namespace qpx;
void checked(bool v,int line){if(!v)throw std::runtime_error("Native density manifest contract failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F action){bool refused=false;try{action();}catch(const std::exception&){refused=true;}check(refused);}
void write(const fs::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f<<s;if(!f)throw std::runtime_error("Fixture write failed");}
int main(){try{
    Project source;const auto root=fs::current_path()/(source.identity()+"-density-manifest");fs::create_directory(root);
    auto doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));doc->set_calibration(.5,.5);
    check(doc->add_rectangle({0,0,8,8})==1);check(doc->add_rectangle({1,1,6,6})==2);source.add_document(doc);source.save_new((root/"source.qpxproj").u8string());
    const auto example=load_project_density_manifest((fs::path(__FILE__).parent_path().parent_path().parent_path()/"examples/project_density_manifest.json").u8string());
    check(example.recipe().selections.size()==2 && example.recipe().selections[0].image_id==1 && example.recipe().selections[1].image_id==2);
    check(example.recipe().selections[0].ids[0]==std::vector<std::uint64_t>{10} && example.recipe().selections[1].ids[2]==std::vector<std::uint64_t>{9});
    check(example.recipe().selections[0].ids[3]==std::vector<std::uint64_t>({20,21}) && example.recipe().options.prefer_nucleus && !example.recipe().options.polygonize_curves);
    check(fs::u8path(example.project())==fs::path(example.path()).parent_path()/"study.qpxproj");
    const std::string valid=R"({"schema":"qpx.project-density/1","project":"source.qpxproj","images":[{"image_id":1,"windows":[2,1],"tissue":[2],"excluded":[],"cells":[],"record_id":1}],"options":{"prefer_nucleus":false}})";
    const auto path=root/"selections.json";write(path,valid);auto manifest=load_project_density_manifest(path.u8string());
    check(manifest.path()==fs::canonical(path).u8string() && manifest.project()==fs::canonical(root/"source.qpxproj").u8string() && manifest.sha256().size()==64);
    check(manifest.recipe().selections.size()==1 && manifest.recipe().selections[0].ids[0]==std::vector<std::uint64_t>({1,2}));
    check(manifest.recipe().selections[0].record_id==1 && !manifest.recipe().options.prefer_nucleus);
    check(preview_project_density(manifest.project(),manifest.recipe()).valid());manifest.verify(true);
    auto absolute=valid;absolute.replace(absolute.find("source.qpxproj"),std::string("source.qpxproj").size(),fs::canonical(root/"source.qpxproj").generic_u8string());
    write(root/"absolute.json",absolute);check(load_project_density_manifest((root/"absolute.json").u8string()).project()==manifest.project());
    fs::create_directory(root/"nested");auto relative=valid;relative.replace(relative.find("source.qpxproj"),std::string("source.qpxproj").size(),"../source.qpxproj");
    write(root/"nested/relative.json",relative);check(load_project_density_manifest((root/"nested/relative.json").u8string()).project()==manifest.project());
    const auto invalid=root/"invalid.json";
    auto mutation=[&](const std::string& from,const std::string& to){auto value=valid;const auto at=value.find(from);check(at!=std::string::npos);value.replace(at,from.size(),to);write(invalid,value);rejects([&]{load_project_density_manifest(invalid.u8string());});};
    mutation("\"image_id\":1","\"image_id\":true");mutation("\"image_id\":1","\"image_id\":1.0");mutation("\"image_id\":1","\"image_id\":-1");
    mutation("[2,1]","[1,1]");mutation("[2,1]","[]");mutation("\"cells\":[]","\"cells\":[0]");
    mutation("\"cells\":[]","\"cells\":[18446744073709551616]");
    mutation("\"prefer_nucleus\":false","\"work_bytes\":1.5");mutation("\"prefer_nucleus\":false","\"snapshot_bytes\":0");
    mutation("\"prefer_nucleus\":false","\"max_geometry_operations\":true");
    std::string many="[";for(int i=1;i<=10001;++i){if(i>1)many+=',';many+=std::to_string(i);}many+=']';mutation("[2,1]",many);
    mutation("\"schema\":","\"unknown\":false,\"schema\":");
    mutation("false","\"false\"");mutation("false","NaN");mutation("\"prefer_nucleus\":false","\"unused\":1");
    mutation("source.qpxproj","source\\u0000.qpxproj");mutation("\"schema\":","\"schema\":\"qpx.project-density/1\",\"schema\":");
    for(const auto& text:{std::string("[]"),std::string("{}"),std::string("{\"a\":1}")}){write(invalid,text);rejects([&]{load_project_density_manifest(invalid.u8string());});}
    auto duplicate=valid;const auto start=duplicate.find("[{"),end=duplicate.find("}],");check(start!=std::string::npos && end!=std::string::npos);duplicate.insert(end+1,","+duplicate.substr(start+1,end-start));write(invalid,duplicate);rejects([&]{load_project_density_manifest(invalid.u8string());});
    {std::ofstream oversized(root/"large.json",std::ios::binary);oversized.seekp(16u*1024*1024);oversized.put(' ');}
    rejects([&]{load_project_density_manifest((root/"large.json").u8string());});
    std::atomic<bool> stop{true};rejects([&]{load_project_density_manifest(path.u8string(),&stop);});
    write(path,valid+" ");rejects([&]{manifest.verify();});
    std::cout<<"Native manifest: cohorts, paths, strict JSON/types/duplicates/limits, owned source guard and cancellation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
