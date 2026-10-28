#include "qupathxx/spatial_analysis.hpp"
#include "qupathxx/project.hpp"
#include "spatial_cli.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
using namespace qpx;
namespace fs=std::filesystem;
void checked(bool value,int line) {if(!value) throw std::runtime_error("spatial CLI check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F fn) {bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed);}
std::string read(const fs::path& path) {std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
struct Capture {
    std::ostringstream out,error;std::streambuf* old_out=std::cout.rdbuf(out.rdbuf());std::streambuf* old_error=std::cerr.rdbuf(error.rdbuf());
    ~Capture() {std::cout.rdbuf(old_out);std::cerr.rdbuf(old_error);}
};
int main() {try {
    std::atomic<bool> cancelled{false};check(!detail::spatial_cli({"batch","--unrelated"},cancelled));
    if(!has_project()) {check(detail::spatial_cli({"batch","--measure-spatial","disabled.qpxproj","new.qpxproj","--image","1","--source","1"},cancelled)==1);std::cout<<"Spatial CLI project capability unavailable\n";return 0;}
    Project project;const auto root=fs::current_path()/(project.identity()+"-spatial-cli");fs::create_directory(root);
    auto doc=std::make_shared<Document>(Image::demo());const auto a=doc->add_detection(Roi::points({{32,32}}));
    const auto b=doc->add_cell(Roi::rectangle({34,34,8,8}),Roi::rectangle({34,35,2,2}));const auto c=doc->add_detection(Roi::points({{32,32}}));doc->set_calibration(.5,.25);
    const auto image=project.add_document(doc,"Missing image",(root/"absent.ppm").u8string(),"ppm");
    const auto other=project.add_document(std::make_shared<Document>(Image::demo()),"Untouched");project.set_active_image(other);
    const auto source=root/"source.qpxproj",output=root/"measured.qpxproj";project.save(source.u8string());const auto original=read(source);
    const auto before_revision=doc->revision();
    std::vector<std::string> measure={"batch","--measure-spatial",source.u8string(),output.u8string(),"--image",std::to_string(image),"--source",std::to_string(b),"--source",std::to_string(a),"--target",std::to_string(c),"--units","um","--name","Nearest cohort"};
    {Capture log;check(detail::spatial_cli(measure,cancelled)==0);const auto text=log.out.str();check(log.error.str().empty() && text.find("\"command\":\"measure\"")!=std::string::npos && std::count(text.begin(),text.end(),'\n')==1);}
    check(read(source)==original && doc->revision()==before_revision && !project.dirty());auto restored=Project::open(output.u8string());auto computed=restored->document(image);
    check(restored->identity()==project.identity() && restored->active_image()==other && restored->images().size()==2 && computed->spatial_record_count()==1);
    const auto record=computed->spatial_records()[0];const auto id=std::to_string(record->id);
    check(record->name=="Nearest cohort" && record->result->rows().size()==2 && record->result->rows()[0].distance==0 && computed->spatial_status(record->id)=="unverified");
    check(record->result->rows()[1].distance==std::hypot(1.5,1.) && record->result->options().units==SpatialDistanceUnits::Micrometres);
    std::vector<std::string> export_args={"batch","--export-spatial",output.u8string(),(root/"table.csv").u8string(),"--image",std::to_string(image),"--record",id};
    {Capture log;check(detail::spatial_cli(export_args,cancelled)==0);check(log.out.str().find("\"historical\":true")!=std::string::npos);}
    check(read(root/"table.csv")==spatial_distance_csv(*record->result));const auto table=read(root/"table.csv"),saved=read(output);
    std::vector<std::string> verify={"batch","--verify-spatial",output.u8string(),"--image",std::to_string(image),"--record",id};
    {Capture log;check(detail::spatial_cli(verify,cancelled)==0);check(log.out.str().find("\"state\":\"verified\"")!=std::string::npos);}
    check(read(output)==saved && !fs::exists(root/"absent.ppm"));
    {Capture log;check(detail::spatial_cli(measure,cancelled)==1 && detail::spatial_cli(export_args,cancelled)==1);}check(read(output)==saved && read(root/"table.csv")==table);
    for(auto path:{source,fs::path(source.u8string()+"-journal"),root/"absent.ppm"}) {
        auto bad=measure;bad[3]=path.u8string();{Capture log;check(detail::spatial_cli(bad,cancelled)==1);}check(read(source)==original && !fs::exists(root/"absent.ppm"));
    }
    std::error_code error;fs::create_hard_link(source,root/"hardlink.qpxproj",error);
    if(!error) {auto bad=measure;bad[3]=(root/"hardlink.qpxproj").u8string();Capture log;check(detail::spatial_cli(bad,cancelled)==1 && read(source)==original);}
    auto bounded=export_args;bounded[3]=(root/"limited.csv").u8string();bounded.insert(bounded.end(),{"--csv-bytes","1"});{Capture log;check(detail::spatial_cli(bounded,cancelled)==1);}check(!fs::exists(root/"limited.csv"));
    auto snapshot=measure;snapshot[3]=(root/"limited.qpxproj").u8string();snapshot.insert(snapshot.end(),{"--save-snapshot-bytes","1"});{Capture log;check(detail::spatial_cli(snapshot,cancelled)==1);}check(!fs::exists(root/"limited.qpxproj"));
    auto stopped=measure;stopped[3]=(root/"cancelled.qpxproj").u8string();cancelled=true;{Capture log;check(detail::spatial_cli(stopped,cancelled)==130);}cancelled=false;check(!fs::exists(root/"cancelled.qpxproj"));
    auto empty=measure;empty[3]=(root/"empty.qpxproj").u8string();empty.erase(empty.begin()+10,empty.begin()+12);{Capture log;check(detail::spatial_cli(empty,cancelled)==0);}
    const auto missing=Project::open(empty[3])->document(image)->spatial_records()[0]->result;check(missing->target_count()==0 && !missing->rows()[0].distance && !missing->rows()[1].distance);
    for(const auto& extra:std::vector<std::vector<std::string>>{{"--image","1"},{"--units","nm"},{"--source",std::to_string(a)},{"--source","-1"},{"--work-bytes","1"},{"--unknown","1"},{"--body","--body"},{"--name",""}}) {
        auto bad=measure;bad.insert(bad.end(),extra.begin(),extra.end());rejects([&]{detail::spatial_cli(bad,cancelled);});
    }
    rejects([&]{detail::spatial_cli({"batch","--verify-spatial",output.u8string(),"--image",std::to_string(image)},cancelled);});
    computed->remove_annotation(c);restored->save((root/"changed.qpxproj").u8string());verify[2]=(root/"changed.qpxproj").u8string();{Capture log;check(detail::spatial_cli(verify,cancelled)==1);}
    for(const auto& item:fs::directory_iterator(root)) check(item.path().filename().u8string().find(".qpx-tmp-")==std::string::npos && item.path().filename().u8string().rfind(".qpx-save-new-",0)!=0);
    check(read(source)==original);fs::remove_all(root);
    std::cout<<"Spatial CLI: native measure/save/reopen, historical streamed export, fresh verification, missing-source zero-IO, guarded inputs, no-overwrite, budgets, cancellation and argument refusal passed\n";
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
