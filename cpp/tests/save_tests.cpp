#include "qupathxx/project_save.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("background save check failed");}
int main() {
    try {
        auto project=std::make_shared<Project>(); ProjectSaveJob job;
        if(!has_project()) {
            bool rejected=false; try {job.request(project,"disabled.qpxproj");} catch(const std::runtime_error&) {rejected=true;}
            check(rejected && !std::filesystem::exists("disabled.qpxproj")); return 0;
        }
        auto doc=std::make_shared<Document>(Image::demo()); auto image=project->add_document(doc);
        auto object=doc->add_rectangle({0,0,10,10},"Captured");
        auto path=(std::filesystem::current_path()/(project->identity()+".qpxproj")).u8string();
        check(job.request(project,path)); check(!job.request(project,path));
        doc->rename_annotation(object,"Live edit");
        auto stats=job.wait(); check(stats.state=="saved" && stats.applied && stats.completed==1 && stats.generation==1);
        check(project->dirty() && project->generation()==1 && Project::open(path)->document(image)->annotation(object).name=="Captured");
        check(job.request(project,path)); check(job.wait().state=="saved" && !project->dirty() && project->generation()==2);
        auto reopened=Project::open(path); check(reopened->document(image)->annotation(object).name=="Live edit");
        project->rename_image(image,"Latest writer"); project->save(path);
        check(job.request(reopened,path)); check(job.wait().state=="error" && reopened->generation()==2);
        std::filesystem::remove(std::filesystem::u8path(path));
        std::cout<<"Native background save isolation, baseline reconciliation and optional-build checks passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
