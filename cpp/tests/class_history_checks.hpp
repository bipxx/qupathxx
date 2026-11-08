#pragma once
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>

namespace class_history_checks {
inline void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class Action>void rejects(Action action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,"class history refusal missing");}
inline void run() {
    using namespace qpx;
    auto doc=std::make_shared<Document>(Image::demo());
    const std::string root="Cleared color",child=root+": Positive",grand=child+": High",flat=root+": Literal";
    doc->replace_class_registry({ObjectClass::definition(root,std::nullopt),ObjectClass::definition(child,std::nullopt,root),ObjectClass::definition(grand,0x80123456,child),ObjectClass::definition(flat,0xabcdef)});
    Ring ring;for(int i=0;i<2000;++i) {const auto angle=i*2*std::acos(-1.)/2000;ring.push_back({200+100*std::cos(angle),180+100*std::sin(angle)});}
    const auto geometry=has_geos()?Roi::polygon(std::move(ring)):Roi::ellipse({100,80,200,200});
    const auto cell=doc->add_cell(geometry,Roi::rectangle({199,179,2,2}),"Large cell",0,grand);
    const auto detection=doc->add_detection(Roi::rectangle({195,175,2,2}),"Child",cell,child);
    const auto unrelated=doc->add_detection(Roi::rectangle({350,300,2,2}),"Literal",0,flat);
    auto facts=std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"note",std::string(4000,'x')}},std::map<std::string,double>{{"score",3.5}},true,0xabcdef,.875);
    doc->set_object_properties(cell,facts);
    const auto saved=doc->annotation(cell),other=doc->annotation(unrelated);
    const auto before=doc->classes();doc->pick({200,180});const auto builds=doc->spatial_stats().builds;
    doc->clear_history();doc->set_history_limits(128,16384);const auto revision=doc->revision();
    doc->rename_class(root,"Positive");
    const auto bytes=doc->history_bytes();require(bytes>0 && bytes<16384,"class-only rename charged unchanged geometry/properties");
    require(doc->revision()==revision+1 && doc->undo_label()=="Rename class","rename was not one field transaction");
    require(doc->annotation(cell).class_name=="Positive: Positive: High" && doc->annotation(detection).class_name=="Positive: Positive","subtree associations changed");
    require(doc->class_path("Positive: Positive: High")==std::vector<std::string>({"Positive","Positive","High"}),"subtree parents were flattened");
    require(!doc->object_class("Positive").color_argb() && doc->object_class("Positive").color==0xfa3e3e && doc->object_class("Positive: Positive").color==0xbb2e2e,"absent colors were not resolved for new paths");
    require(doc->object_class("Positive: Positive: High").color_argb()==0x80123456,"rename changed raw ARGB");
    require(doc->annotation(cell).properties==facts && doc->annotation(cell).class_probability()==.875,"administrative rename cleared probability or copied properties");
    require(doc->annotation(cell).geometry.equals(saved.geometry) && doc->annotation(cell).nucleus->equals(*saved.nucleus) && doc->annotation(detection).parent_id==cell,"rename changed geometry/nucleus/object hierarchy");
    require(doc->annotation(unrelated).revision==other.revision && doc->annotation(unrelated).class_name==flat && doc->object_class(flat).parent.empty(),"rename matched unrelated literal prefix");
    doc->pick({200,180});require(doc->spatial_stats().builds==builds && doc->spatial_stats().dirty_entries==0,"field rename invalidated spatial geometry");
    const auto renamed=doc->classes();Document independent=*doc;
    require(doc->undo() && doc->classes()==before && doc->annotation(cell).class_name==grand,"rename undo failed");
    require(independent.classes()==renamed && independent.undo() && independent.classes()==before,"copied rename history aliases another document");
    require(doc->redo() && doc->classes()==renamed && independent.redo() && independent.classes()==renamed,"rename redo failed");
    if(has_project()) {
        Project project;const auto image=project.add_document(doc);
        const auto path=std::filesystem::temp_directory_path()/(project.identity()+"-class-history.qpxproj");
        struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};
        project.save(path.u8string());auto restored=Project::open(path.u8string())->document(image);
        require(restored->classes()==renamed && restored->annotation(cell).class_name=="Positive: Positive: High" && restored->annotation(detection).class_name=="Positive: Positive","rename snapshot publication retained old class strings");
        require(restored->annotation(cell).properties->equals(*facts) && restored->annotation(cell).geometry.equals(saved.geometry),"rename snapshot changed immutable facts/geometry");
    }
    const auto stable=doc->revision();const auto stable_bytes=doc->history_bytes();
    rejects([&]{doc->rename_class("Positive",flat);});require(doc->revision()==stable && doc->history_bytes()==stable_bytes && doc->classes()==renamed,"collision partially published");
    doc->set_history_limits(128,1);rejects([&]{doc->rename_class("Positive","Rejected");});
    require(doc->revision()==stable && doc->classes()==renamed && doc->annotation(cell).properties==facts,"failed field budget partially published");
    doc->set_history_limits(0,0);doc->rename_class("Positive","Unrecorded");
    require(!doc->can_undo() && doc->annotation(cell).class_name=="Unrecorded: Positive: High" && doc->annotation(cell).properties==facts,"unrecorded rename changed state policy");
    // Reclassification still clears probability; only administrative rename bypasses it.
    doc->apply_classifications({{cell,"Actual classification"}});
    require(!doc->annotation(cell).class_probability() && doc->annotation(cell).properties->metadata()==facts->metadata() && doc->annotation(cell).locked(),"classification probability-reset policy regressed");
    Document unused(Image::demo());unused.set_class_definition(ObjectClass::definition(root,std::nullopt));unused.clear_history();
    unused.rename_class(root,"Positive");require(unused.object_class("Positive").color==0xfa3e3e && unused.undo() && unused.object_class(root).color==0xc81767 && unused.redo(),"unused class-only history failed");
    Document overlap(Image::demo());overlap.replace_class_registry({ObjectClass::definition("A",0x112233),ObjectClass::definition("A: B",std::nullopt,"A")});
    const auto a=overlap.add_detection(Roi::rectangle({1,1,2,2}),"Root",0,"A"),b=overlap.add_detection(Roi::rectangle({4,4,2,2}),"Child",0,"A: B");
    const auto original=overlap.classes();overlap.rename_class("A","A: B");
    require(overlap.object_class("A: B").parent.empty() && overlap.object_class("A: B: B").parent=="A: B" && overlap.annotation(a).class_name=="A: B" && overlap.annotation(b).class_name=="A: B: B","overlapping subtree target lost identity/counts");
    require(overlap.class_count("A: B")==1 && overlap.class_count("A: B: B")==1 && overlap.undo() && overlap.classes()==original,"overlapping rename counts/undo failed");
    std::cout<<"Class-only history: dense_polygon_vertices="<<(has_geos()?2000:0)<<" history_bytes="<<bytes<<" budget=16384; raw/resolved parents, probability/properties, geometry/index, snapshot, copy/undo/redo, collision/budget refusal, unused/overlap and unrecorded controls passed\n";
}
}
