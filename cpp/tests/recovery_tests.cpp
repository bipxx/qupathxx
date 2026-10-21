#include "qupathxx/project_recovery.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("recovery check failed"); }
int main() {
    try {
        Project p; ProjectAutosave autosave;
        if(!has_project()) {
            bool rejected=false; try { autosave.request(p,"disabled.qpxrecover"); } catch(const std::runtime_error&) { rejected=true; }
            check(rejected && !std::filesystem::exists("disabled.qpxrecover")); return 0;
        }
        auto doc=std::make_shared<Document>(Image::demo()); auto image=p.add_document(doc);
        auto object=doc->add_rectangle({0,0,10,10},"Original");
        auto base=(std::filesystem::current_path()/(p.identity()+".qpxproj")).u8string(); p.save(base);
        doc->rename_annotation(object,"Captured");
        auto path=base+".qpxrecover";
        check(autosave.request(p,path)); doc->rename_annotation(object,"Later");
        auto stats=autosave.wait(); check(stats.state=="ready" && stats.completed==1);
        check(p.dirty() && p.generation()==1 && Project::open(base)->document(image)->annotation(object).name=="Original");
        auto restored=Project::recover(path);
        check(restored->dirty() && restored->generation()==1 && restored->document(image)->annotation(object).name=="Captured");
        check(Project::inspect_recovery(path).base_status=="matching");
        p.restore_recovery(path); check(p.document(image)->annotation(object).name=="Captured" && doc->annotation(object).name=="Later");
        autosave.clear(); check(autosave.wait().state=="idle" && !std::filesystem::exists(std::filesystem::u8path(path)));
        std::filesystem::remove(std::filesystem::u8path(base)); std::filesystem::remove(std::filesystem::u8path(path+".lock"));
        std::cout<<"Native async snapshot isolation, primary preservation and restoration passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
