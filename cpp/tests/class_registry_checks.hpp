#pragma once
#include "qupathxx/project.hpp"
#include <filesystem>
#include <iostream>

namespace class_registry_checks {
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Action> void refuses(Action action) {bool failed=false;try{action();}catch(const std::invalid_argument&){failed=true;}if(!failed)throw std::runtime_error("invalid class graph was accepted");}
inline void run() {
    using namespace qpx;
    const auto root=ObjectClass::definition("Other base",0x204080);
    const auto child=ObjectClass::definition("Other base: Positive",std::nullopt,"Other base");
    const auto grand=ObjectClass::definition("Other base: Positive: High",0x80123456u,"Other base: Positive");
    const auto literal=ObjectClass::definition("Other base: Literal",0xabcdef);
    Document doc(Image::demo());doc.replace_class_registry({grand,child,literal,root});
    require(doc.object_class(child.name).color==0x183060 && !doc.object_class(child.name).color_argb(),"absent class color lost raw/resolved distinction");
    require(doc.object_class(grand.name).color==0x123456 && doc.object_class(grand.name).color_argb()==0x80123456u,"ARGB was narrowed");
    require(doc.class_path(grand.name)==std::vector<std::string>({"Other base","Positive","High"}),"explicit class path changed");
    require(doc.class_path(literal.name)==std::vector<std::string>({literal.name}),"flat delimiter name was interpreted as ancestry");
    const auto child_object=doc.add_rectangle({1,2,3,4}),grand_object=doc.add_rectangle({10,10,2,3}),flat_object=doc.add_rectangle({20,20,2,2});
    doc.classify_objects({child_object},child.name);doc.classify_objects({grand_object},grand.name);doc.classify_objects({flat_object},literal.name);
    const auto revision=doc.revision();doc.set_class_definition(doc.object_class(child.name));require(doc.revision()==revision,"equal class definition dirtied document");
    const auto object_revision=doc.annotation(child_object).revision;doc.pick({2,3});const auto builds=doc.spatial_stats().builds;
    doc.set_class(root.name,0x4080a0);require(doc.object_class(child.name).color==0x306078 && !doc.object_class(child.name).color_argb(),"ancestor color did not resolve absent child color");
    require(doc.annotation(child_object).revision==object_revision,"color-only edit changed object revision");
    require(doc.undo() && doc.object_class(child.name).color==0x183060,"class color undo failed");
    require(doc.redo() && doc.object_class(child.name).color==0x306078,"class color redo failed");
    doc.rename_class(root.name,"Renamed");
    require(doc.annotation(child_object).class_name=="Renamed: Positive" && doc.annotation(grand_object).class_name=="Renamed: Positive: High","subtree rename lost associations");
    require(doc.annotation(flat_object).class_name==literal.name && doc.object_class(literal.name).parent.empty(),"rename matched an unrelated string prefix");
    require(doc.object_class("Renamed: Positive: High").parent=="Renamed: Positive" && doc.object_class("Renamed: Positive: High").color_argb()==0x80123456u,"rename changed class metadata");
    require(doc.annotation(child_object).geometry.area()==12 && doc.annotation(grand_object).geometry.area()==6,"class rename changed geometry");
    doc.pick({2,3});require(doc.spatial_stats().builds==builds && doc.spatial_stats().dirty_entries==0,"class edit invalidated spatial geometry");
    require(doc.undo() && doc.class_path(grand.name).size()==3 && doc.annotation(child_object).class_name==child.name,"subtree undo failed");
    require(doc.redo() && doc.annotation(child_object).class_name=="Renamed: Positive","subtree redo failed");
    const auto stable=doc.revision();
    refuses([&]{doc.remove_class("Renamed");});
    refuses([&]{doc.rename_class("Renamed",literal.name);});
    refuses([&]{doc.set_class_definition(ObjectClass::definition("Ghost: Leaf",std::nullopt,"Ghost"));});
    refuses([&]{doc.replace_class_registry({root,root});});
    refuses([&]{doc.set_class_definition(ObjectClass::definition("Bad",0x112233,"Other parent"));});
    require(doc.revision()==stable && doc.object_count()==3,"invalid graph partially published");
    Document deep(Image::demo());std::vector<ObjectClass> chain;std::string name="Depth",parent;
    for(int i=0;i<32;++i){chain.push_back(ObjectClass::definition(name,0x112233,parent));parent=name;name+=": x";}
    deep.replace_class_registry(chain);require(deep.class_path(parent).size()==32,"valid depth boundary refused");
    refuses([&]{deep.set_class_definition(ObjectClass::definition(name,0x112233,parent));});
    require(deep.classes().size()==32,"over-depth edit partially published");
    const auto cleared=ObjectClass::definition("Cleared color",std::nullopt);require(!cleared.color_argb() && cleared.color==0xc81767,"cleared color fallback differs from fresh QuPath");
    if(has_project()) {
        auto project=std::make_shared<Project>();auto image_doc=std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,8)));
        image_doc->replace_class_registry({root,child,grand,literal});const auto id=project->add_document(image_doc,"Classes");
        const auto object=image_doc->add_detection(Roi::rectangle({0,0,1,1}),"Cell",0,grand.name);
        const auto before=image_doc->revision();project->set_class_registry({root,child,grand,literal},{grand.name,child.name,grand.name});
        require(image_doc->revision()==before && project->class_defaults().size()==3,"project registry mutated image or deduplicated palette");
        const auto path=std::filesystem::temp_directory_path()/(project->identity()+"-class-registry.qpxproj");
        struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};
        project->save(path.u8string());auto restored=Project::open(path.u8string());
        require(restored->classes()==project->classes() && restored->class_defaults()==project->class_defaults(),"project class registry/palette changed on reopen");
        require(restored->document(id)->object_class(child.name)==image_doc->object_class(child.name) && restored->document(id)->object_class(grand.name)==grand,"document class semantics changed on reopen");
        require(restored->document(id)->annotation(object).class_name==grand.name,"stored object class association changed");
        project->set_class_registry({ObjectClass::definition(root.name,0x102030),child,grand},{child.name,child.name});
        require(image_doc->object_class(root.name).color==0x204080,"project default edit silently changed image");
        const auto apply_revision=image_doc->revision();project->apply_class_defaults(id);
        require(image_doc->revision()==apply_revision+1 && image_doc->object_class(root.name).color==0x102030 && image_doc->object_class(child.name).color==0x0c1824,"default application was not one resolved native edit");
        require(image_doc->object_class(grand.name).color_argb()==0x80123456u,"default application erased unrelated descendant ARGB");
        require(image_doc->undo() && image_doc->object_class(root.name).color==0x204080,"default application undo failed");
        Project empty;empty.set_class_registry({root,child},{child.name,root.name,child.name});const auto empty_path=path.parent_path()/(empty.identity()+"-empty-classes.qpxproj");Cleanup empty_cleanup{empty_path};
        empty.save(empty_path.u8string());auto empty_restored=Project::open(empty_path.u8string());require(empty_restored->images().empty() && empty_restored->classes()==empty.classes() && empty_restored->class_defaults()==empty.class_defaults(),"empty-project palette roundtrip failed");
    }
    std::cout<<"Native class ancestry: raw/resolved ARGB, explicit paths, sparse history/subtree rename, atomic refusal and schema26 project/default roundtrips passed\n";
}
}
