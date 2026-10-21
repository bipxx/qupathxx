#include "qupathxx/core.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("class check failed"); }
int main() {
    try {
        Document d(Image::demo());
        auto cell=d.add_cell(Roi::rectangle({0,0,20,20}),Roi::rectangle({1,1,2,2}),"Cell",0,"Tumor");
        check(d.classes().size()==1 && d.class_count("Tumor")==1);
        check(d.undo() && d.classes().empty() && d.object_count()==0);
        check(d.redo() && d.class_count("Tumor")==1);
        d.pick({1,1}); const auto builds=d.spatial_stats().builds,revision=d.annotation(cell).revision;
        d.set_class("Tumor",0x123456);
        check(d.annotation(cell).revision==revision && d.object_class("Tumor").color==0x123456);
        d.rename_class("Tumor","Positive");
        check(d.annotation(cell).class_name=="Positive" && d.annotation(cell).nucleus->area()==4);
        check(d.class_count("Positive")==1 && d.class_count("Tumor")==0);
        check(d.undo() && d.annotation(cell).class_name=="Tumor");
        check(d.redo() && d.annotation(cell).class_name=="Positive");
        d.pick({1,1}); check(d.spatial_stats().builds==builds && d.spatial_stats().dirty_entries==0);
        try { d.remove_class("Positive"); throw std::runtime_error("removed used class"); } catch(const std::invalid_argument&) {}
        d.classify_objects({cell,cell},"Other");
        check(d.class_count("Other")==1 && d.class_count("Positive")==0);
        d.remove_class("Positive"); check(d.classes().size()==1);
        check(d.undo() && d.object_class("Positive").color==0x123456);
        const auto saved=d.revision();
        try { d.set_class("Invalid",0x1000000); throw std::runtime_error("invalid color accepted"); } catch(const std::invalid_argument&) {}
        check(d.revision()==saved && d.can_redo());
        d.classify_objects({cell},""); check(d.class_count("Other")==0);
        check(d.undo() && d.class_count("Other")==1);
        std::cout<<"Class registry, counts, atomic edits and history passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
