#include "qupathxx/core.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("document edit check failed"); }
int main() {
    try {
        Document d(Image::demo());
        auto parent=d.add_rectangle({0,0,200,200},"Parent");
        auto child=d.execute({ObjectEdit::create({10,10,20,20},"Child",parent,"Tumor")}).at(0);
        const auto revision=d.revision();
        try {
            d.execute({ObjectEdit::replace(child,{20,20,10,10},"Changed",parent),ObjectEdit::create({-1,0,1,1})});
            throw std::runtime_error("invalid batch was accepted");
        } catch(const std::invalid_argument&) {}
        check(d.revision()==revision && d.annotation(child).name=="Child");
        check(d.children(parent)==std::vector<std::uint64_t>{child});
        check(d.remove_annotation(parent) && d.object_count()==0);
        check(d.undo() && d.object_count()==2 && d.annotation(child).class_name=="Tumor");
        check(d.children(parent)==std::vector<std::uint64_t>{child});
        check(d.redo() && d.object_count()==0);
        check(d.undo()); d.set_calibration(.5,.25); check(d.area_um2(child)==50);
        check(d.undo() && !d.pixel_width_um()); check(d.redo() && d.pixel_height_um()==.25);
        d.clear_history(); d.set_history_limits(1,1024*1024);
        d.rename_annotation(child,"Renamed"); d.classify_annotation(child,"Stroma");
        check(d.undo() && d.annotation(child).class_name=="Tumor" && !d.undo());
        auto cell=d.add_cell(Roi::rectangle({30,30,10,10}),Roi::ellipse({60,60,4,4}),"Cell",parent);
        check(d.annotation(cell).kind==ObjectKind::Cell && d.annotation(cell).is_detection());
        check(d.pick({62,62})==std::vector<std::uint64_t>{parent,cell});
        const auto builds=d.spatial_stats().builds;
        d.update_nucleus(cell,Roi::rectangle({100,100,2,2}));
        check(d.pick({101,101})==std::vector<std::uint64_t>{parent,cell});
        check(d.spatial_stats().builds==builds);
        check(d.undo() && d.annotation(cell).nucleus->kind()==RoiKind::Ellipse);
        d.rename_annotation(cell,"Renamed cell");
        check(d.annotation(cell).kind==ObjectKind::Cell && d.annotation(cell).nucleus.has_value());
        d.set_history_limits(128,64*1024*1024);
        check(d.remove_annotation(parent) && d.object_count()==0);
        check(d.undo() && d.annotation(cell).nucleus.has_value());
        const auto saved=d.revision();
        try { d.update_nucleus(cell,Roi::rectangle({-1,0,1,1})); throw std::runtime_error("invalid nucleus accepted"); }
        catch(const std::invalid_argument&) {}
        check(d.revision()==saved);
        std::cout<<"Atomic edits, subtree restore, calibration and bounded undo passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
