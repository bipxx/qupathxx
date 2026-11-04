#include "qupathxx/core.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("geometry edit check failed"); }
int main() {
    try {
        auto line=Roi::polyline({{0,0},{3,0},{3,4}});
        check(line.with_vertex(0,2,{3,8}).perimeter()==11);
        check(line.inserted_vertex(0,1,{1,0}).without_vertex(0,1).equals(line));
        if(has_geos()) {
            auto a=Roi::rectangle({0,0,10,10}),b=Roi::rectangle({5,0,10,10});
            check(a.boolean_operation(b,BooleanOp::Union)->area()==150);
            check(a.boolean_operation(b,BooleanOp::Intersection)->area()==50);
            check(a.boolean_operation(b,BooleanOp::Difference)->area()==50);
            check(a.boolean_operation(b,BooleanOp::SymmetricDifference)->area()==100);
            check(!a.boolean_operation(a,BooleanOp::Difference));
            auto hole=a.with_hole(0,{{2,2},{8,2},{8,8},{2,8}});
            check(hole.area()==64 && !hole.contains({5,5}) && hole.without_hole(0,0).area()==100);
            Document d(Image::demo()); auto id=d.add_roi(hole),other=d.add_roi(b);
            d.apply_boolean(id,{other},BooleanOp::Union); check(d.undo() && d.annotation(id).geometry.equals(hole));
            d.edit_vertex(id,0,0,{1,0}); check(d.undo() && d.annotation(id).geometry.equals(hole));
            const auto revision=d.revision();
            try { d.edit_vertex(id,0,0,{15,5}); throw std::runtime_error("invalid vertex accepted"); }
            catch(const std::invalid_argument&) {}
            check(d.revision()==revision);
        } else {
            try { Roi::rectangle({0,0,1,1}).boolean_operation(Roi::rectangle({0,0,1,1}),BooleanOp::Union); throw std::logic_error("GEOS capability not enforced"); }
            catch(const std::runtime_error&) {}
        }
        Document merged(Image::demo());
        const auto p=merged.add_roi(Roi::points({{10,10},{20,20}})),q=merged.add_roi(Roi::points({{20,20},{30,10}}));
        const auto id=merged.merge_annotations({p,q});
        check(merged.object_count()==1 && merged.annotation(id).geometry.paths().front().size()==3);
        check(merged.undo_label()=="Merge annotations" && merged.undo() && merged.object_count()==2);
        check(merged.redo() && merged.object_count()==1 && merged.annotation(id).parent_id==0);
        const auto revision=merged.revision();
        try {merged.merge_annotations({id,id});throw std::logic_error("duplicate merge accepted");}catch(const std::invalid_argument&) {}
        check(merged.revision()==revision);
        if(has_geos()) {
            Document areas(Image::demo());const auto a=areas.add_rectangle({10,10,20,20}),b=areas.add_rectangle({20,10,20,20});
            const auto child=areas.add_detection(Roi::rectangle({12,12,2,2}),"child",a,"Stroma");
            areas.classify_objects({a,b},"Tumor");const auto result=areas.merge_annotations({a,b});
            check(areas.annotation(result).geometry.area()==600 && areas.annotation(result).class_name=="Tumor");
            check(areas.annotation(child).parent_id==0 && areas.undo() && areas.annotation(child).parent_id==a);
            check(areas.redo() && areas.annotation(child).parent_id==0);
        }
        std::cout<<"Annotation merge, point union, child preservation and atomic history passed\n";
        std::cout<<"Vertex edits, holes, area overlay and journal passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
