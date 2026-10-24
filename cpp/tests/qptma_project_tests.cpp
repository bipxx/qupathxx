#include "qupathxx/project.hpp"
#include "qupathxx/qptma.hpp"
#include "qptma_project_cli.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace qpx;
void check(bool ok){if(!ok)throw std::runtime_error("qptma project assertion failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("missing qptma project rejection");}
std::string bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
int main(int argc,char** argv){try {
#ifndef QUPATHXX_HAS_PROJECT
    std::cout<<"QPTMA project storage disabled in this configuration\n";return 0;
#else
    check(argc==3);namespace fs=std::filesystem;
    const auto root=fs::u8path(argv[2])/("project-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    auto first=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,42)));
    first->add_rectangle({1,1,3,3},"Retained annotation");first->set_calibration(.5,.25);
    auto second=std::make_shared<Document>(Image(256,192,std::vector<std::uint8_t>(256*192*3,255)));
    Project project;const auto a=project.add_document(first,"untouched"),b=project.add_document(second,"TMA target");project.set_active_image(a);
    ProjectView view;view.center={3,4};view.zoom=.75;project.set_view(a,view);
    const auto source=(root/"source.qpxproj").u8string();project.save(source);const auto original=bytes(source),identity=project.identity();
    const auto generation=project.generation(),revision=second->revision();std::atomic<bool> cancelled{false};
    auto assert_clean=[&]{check(project.path()==source && project.generation()==generation && second->revision()==revision && !project.dirty() && bytes(source)==original);
        for(const auto& item:fs::directory_iterator(root))check(item.path().filename().u8string().rfind(".qpx-save-new-",0)!=0 && item.path().filename().u8string().find(".qpx-tmp-")==std::string::npos);};
    rejects([&]{project.save_new(source);});assert_clean();
    rejects([&]{project.save_new((root/"small-snapshot.qpxproj").u8string(),1);});check(!fs::exists(root/"small-snapshot.qpxproj"));assert_clean();
    rejects([&]{project.save_new((root/"small-file.qpxproj").u8string(),512*1024*1024,4096);});check(!fs::exists(root/"small-file.qpxproj"));assert_clean();
    cancelled=true;rejects([&]{project.save_new((root/"cancelled.qpxproj").u8string(),512*1024*1024,1024*1024,&cancelled);});cancelled=false;assert_clean();
    rejects([&]{project.save_new((root/"guard-failed.qpxproj").u8string(),512*1024*1024,1024*1024,nullptr,[]{throw std::runtime_error("changed source");});});check(!fs::exists(root/"guard-failed.qpxproj"));assert_clean();
    const auto raced=root/"raced.qpxproj";
    rejects([&]{project.save_new(raced.u8string(),512*1024*1024,1024*1024,nullptr,[&]{std::ofstream out(raced,std::ios::binary);out<<"existing target";});});check(bytes(raced)=="existing target");assert_clean();
    const auto wire=(fs::u8path(argv[1])/"fractional-import.qptma").u8string(),wire_before=bytes(wire);
    const auto output=(root/"imported.qpxproj").u8string();
    auto result=detail::qptma_project_cli({"batch","--import-qptma",source,wire,output,"--image",std::to_string(b)},cancelled);check(result && *result==0);
    check(bytes(source)==original && bytes(wire)==wire_before);const auto imported=Project::open(output);
    check(imported->identity()==identity && imported->images().size()==2 && imported->active_image()==a && imported->view(a) && *imported->view(a)==view);
    check(imported->document(a)->annotations().size()==1 && imported->document(a)->annotations()[0].name=="Retained annotation" && imported->document(a)->image().pixels()==first->image().pixels());
    check(imported->document(a)->pixel_width_um()==.5 && imported->document(a)->pixel_height_um()==.25 && imported->document(b)->tma_core_count()==6);
    const auto exported=(root/"exported.qptma").u8string();result=detail::qptma_project_cli({"batch","--export-qptma",output,exported,"--image",std::to_string(b)},cancelled);check(result && *result==0);
    const auto data=load_qptma(exported);check(data.cores[0].bounds.x==12.75 && data.cores[1].missing && data.cores[3].case_id=="patient \xce\xb1");
    const auto prior_export=bytes(exported);result=detail::qptma_project_cli({"batch","--export-qptma",output,exported,"--image",std::to_string(b)},cancelled);check(result && *result==1 && bytes(exported)==prior_export);
    result=detail::qptma_project_cli({"batch","--import-qptma",source,wire,source,"--image",std::to_string(b)},cancelled);check(result && *result==1 && bytes(source)==original);
    rejects([&]{detail::qptma_project_cli({"batch","--export-qptma",output,exported},cancelled);});
    auto journal=fs::u8path(source);journal+="-journal";{std::ofstream out(journal);out<<"journal preflight marker";}
    result=detail::qptma_project_cli({"batch","--export-qptma",source,(root/"journal.qptma").u8string(),"--image",std::to_string(b)},cancelled);check(result && *result==1 && bytes(journal)=="journal preflight marker" && !fs::exists(root/"journal.qptma") && bytes(source)==original);
    std::cout<<"QPTMA project atomic new-file publication, budgets, guards, races, persistence and explicit-image CLI passed\n";
#endif
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
