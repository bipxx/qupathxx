#include "qupathxx/spatial_frontend.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace qpx;
namespace fs=std::filesystem;
void checked(bool value,int line) {if(!value)throw std::runtime_error("spatial frontend check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F fn) {bool hit=false;try{fn();}catch(const std::exception&){hit=true;}check(hit);}
std::string read(const fs::path& p) {std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main() {try {
    auto doc=std::make_shared<Document>(Image::demo());const auto a=doc->add_detection(Roi::points({{32,32}})),b=doc->add_detection(Roi::points({{35,36}}));
    detail::SpatialFrontend frontend;check(frontend.same_document(*doc));frontend.measure(*doc,{a},{b});check(frontend.busy());
    rejects([&]{frontend.inspect(*doc,1);});frontend.finish(*doc,true);auto ready=frontend.report();check(!ready.busy && ready.state=="ready" && ready.result->rows()[0].distance==5);
    auto retained=ready.result;const auto id=frontend.keep(*doc,"Owner table");frontend.poll(*doc);check(frontend.report().record_id==id && frontend.report().record_status=="current");
    auto clone=*doc;check(!frontend.same_document(clone));frontend.poll(clone);check(!frontend.report().result && frontend.report().state=="stale");
    frontend.inspect(clone,id);frontend.poll(clone);check(frontend.report().state=="historical" && frontend.report().record_status=="unverified");
    frontend.verify(clone,id);frontend.finish(clone,true);check(frontend.report().state=="verified" && frontend.report().record_status=="current");
    clone.rename_annotation(a,"changed");frontend.poll(clone);check(frontend.report().state=="historical" && frontend.report().record_status=="objects_or_calibration_changed");
    rejects([&]{frontend.verify(clone,id);});check(retained->rows()[0].distance==5);
    Project project;const auto root=fs::current_path()/(project.identity()+"-spatial-frontend");fs::create_directory(root);
    const auto image=project.add_document(doc,"Linked",(root/"missing.ppm").u8string(),"ppm");const auto source=root/"source.qpxproj";
    if(has_project())project.save(source.u8string());const auto catalog=project.images();const auto paths=detail::spatial_project_paths(&project,&catalog);
    frontend.inspect(*doc,id);const auto output=root/"distances.csv";frontend.export_csv(*doc,output.u8string(),65536,paths);check(frontend.busy());frontend.finish(*doc,true);
    check(frontend.report().export_state=="saved" && frontend.report().export_committed && read(output)==spatial_distance_csv(*retained));
    frontend.export_csv(*doc,output.u8string(),65536,paths);frontend.finish(*doc,true);check(frontend.report().export_state=="failed" && read(output)==spatial_distance_csv(*retained));
    rejects([&]{frontend.export_csv(*doc,(root/"missing.ppm").u8string(),65536,paths);});
    rejects([&]{frontend.export_csv(*doc,(root/"small.csv").u8string(),65536,paths,1);});check(!fs::exists(root/"small.csv"));
    frontend.export_csv(*doc,(root/"limited.csv").u8string(),1,paths);frontend.finish(*doc,true);check(frontend.report().export_state=="failed" && !fs::exists(root/"limited.csv"));
    rejects([&]{detail::spatial_project_paths(&project,nullptr);});
    auto wrong=std::async(std::launch::async,[&]{rejects([&]{frontend.report();});rejects([&]{frontend.cancel();});});wrong.get();
    frontend.measure(*doc,{a,b},{a,b});frontend.cancel();frontend.finish(*doc,true);check(frontend.report().state=="cancelled" && !frontend.report().result);
    frontend.measure(*doc,{a},{b});doc->rename_annotation(a,"during work");frontend.finish(*doc,true);check(frontend.report().state=="stale" && !frontend.report().result);
    frontend.measure(*doc,{a},{b});frontend.finish(*doc,true);doc->rename_annotation(b,"after ready");frontend.poll(*doc);check(frontend.report().state=="stale" && !frontend.report().result);
    frontend.inspect(*doc,id);check(doc->remove_spatial(id));frontend.poll(*doc);check(frontend.report().record_status=="removed" && frontend.report().result==retained);
    frontend.export_csv(*doc,(root/"removed.csv").u8string(),65536,paths);frontend.finish(*doc,true);check(frontend.report().export_state=="saved");
    check(!fs::exists(root/"missing.ppm"));for(const auto& p:fs::directory_iterator(root))check(p.path().filename().u8string().find(".qpx-tmp-")==std::string::npos);
    fs::remove_all(root);
    {detail::SpatialFrontend shutdown;shutdown.measure(*doc,{a},{b});} // Joins before captured native state retires.
    std::cout<<"Spatial shared frontend: capture/keep/history/verify, clone and revision guards, cancellation/retry, atomic protected export, budgets, removed-record ownership and shutdown passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
