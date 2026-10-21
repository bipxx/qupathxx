#include "qupathxx/project_save.hpp"
#include "qupathxx/project_recovery.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("object snapshot check failed");}
void equal(const std::vector<Annotation>& a,const std::vector<Annotation>& b) {
    check(a.size()==b.size());
    for(std::size_t i=0;i<a.size();++i) check(a[i].id==b[i].id && a[i].name==b[i].name && a[i].class_name==b[i].class_name && a[i].parent_id==b[i].parent_id && a[i].revision==b[i].revision && a[i].kind==b[i].kind && a[i].geometry.equals(b[i].geometry) && a[i].nucleus.has_value()==b[i].nucleus.has_value() && (!a[i].nucleus || a[i].nucleus->equals(*b[i].nucleus)));
}
int main() {
 try {
    if(!has_project()) {std::cout<<"Object snapshots require project storage\n";return 0;}
    auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image::demo());const auto image=project->add_document(doc);
    const auto parent=doc->add_rectangle({0,0,512,512},"Parent");std::vector<ObjectEdit> edits;
    for(int i=0;i<517;++i) {
        const auto shape=Roi::ellipse({double(i%256),double(i/256),1,1});
        edits.push_back(ObjectEdit::create_geometry(shape,"Object "+std::to_string(i),parent,i%2?"Tumor":"Stroma",ObjectKind::Cell,Roi::rectangle({double(i%256)+.25,double(i/256)+.25,.5,.5})));
    }
    const auto ids=doc->execute(edits);const auto first=doc->annotations();
    const auto path=(std::filesystem::current_path()/(project->identity()+"-objects.qpxproj")).u8string();
    ProjectSaveJob save;check(save.request(project,path));
    doc->rename_annotation(ids[254],std::string(4096,'x'));doc->rename_annotation(ids[255],"Boundary");
    doc->rename_class("Tumor","Immune");doc->remove_annotation(ids[516]);check(doc->undo());check(doc->redo());
    check(save.wait().applied && project->dirty());auto loaded=Project::open(path)->document(image);equal(first,loaded->annotations());
    check(loaded->class_count("Tumor")==258 && loaded->object_count()==518);
    auto copied=std::make_shared<Document>(*doc);copied->rename_annotation(ids[256],"Independent copy");
    auto second=doc->annotations();check(save.request(project,path));doc->remove_annotation(parent);
    check(doc->object_count()==0 && save.wait().applied && project->dirty());equal(second,Project::open(path)->document(image)->annotations());
    const auto occupied=save.stats().snapshot_bytes;check(save.request(project,path));check(save.wait().applied && save.stats().snapshot_bytes<occupied);
    check(Project::open(path)->document(image)->object_count()==0);check(doc->undo());
    const auto checkpoint=(std::filesystem::current_path()/(project->identity()+"-objects.qpxrecover")).u8string();
    auto recovery=std::make_unique<ProjectAutosave>();const auto captured=doc->annotations();check(recovery->request(*project,checkpoint));doc->remove_annotation(parent);
    check(recovery->wait().completed==1);auto recovered=Project::recover(checkpoint)->document(image);equal(captured,recovered->annotations());
    check(Project::inspect_recovery(checkpoint).objects==captured.size());
    auto other=std::make_shared<Project>();const auto other_image=other->add_document(copied);
    const auto other_path=(std::filesystem::current_path()/(other->identity()+"-copy.qpxproj")).u8string();
    const auto copy_state=copied->annotations();ProjectSaveJob copy_save;check(copy_save.request(other,other_path));copied->rename_annotation(ids[256],"Later copy edit");
    check(copy_save.wait().applied);equal(copy_state,Project::open(other_path)->document(other_image)->annotations());
    recovery->clear();recovery.reset();
    std::filesystem::remove(std::filesystem::u8path(checkpoint));
    std::filesystem::remove(std::filesystem::u8path(checkpoint+".lock"));
    std::filesystem::remove(std::filesystem::u8path(checkpoint+".session"));
    std::filesystem::remove(std::filesystem::u8path(path));std::filesystem::remove(std::filesystem::u8path(other_path));
    std::cout<<"Object snapshot boundaries, copy, undo, empty blocks and recovery isolation passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
