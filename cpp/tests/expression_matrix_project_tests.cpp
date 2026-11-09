#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_save.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected expression project refusal");}
void write(const fs::path& path,const std::string& text){std::ofstream out(path,std::ios::binary);out<<text;out.close();check(bool(out),"Write expression project fixture");}
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void no_temporary(const fs::path& root){for(const auto& entry:fs::recursive_directory_iterator(root))check(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos,"No leaked copy temporary");}
void exercise(const fs::path& root) {
    using V=ImageDataValue;
    const auto bundle=root/"linked",assets=bundle/"assets";fs::create_directories(assets);
    write(root/"matrix.mtx","%%MatrixMarket matrix coordinate integer general\n2 3 3\n1 1 9007199254740993\n1 3 5\n2 2 7\n");
    write(root/"features.tsv","feature_id\nA\nB\n");write(root/"observations.tsv","observation_id\nX\nY\nZ\n");
    const auto imported=import_expression_matrix((root/"matrix.mtx").u8string(),(root/"features.tsv").u8string(),(root/"observations.tsv").u8string(),(assets/"source.qmx").u8string());
    auto source=open_expression_matrix(imported.path,imported.file_sha256);
    const ExpressionMatrixSelection selection{{0,1},std::nullopt};const auto block=source->read(selection);
    auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,20)));
    project->add_document(doc);doc->set_image_data_properties(std::make_shared<ImageDataProperties>(V::Map{{"note",V{std::string("preserve")}}}));
    rejects<std::invalid_argument>([&]{keep_expression_matrix(*project,1,source,true);});
    check(expression_matrix_ids(*project,1).empty(),"Unsaved owned refusal leaves metadata untouched");
    const auto id=keep_expression_matrix(*project,1,source);
    check(id==imported.metadata.source_sha256 && expression_matrix_ids(*project,1)==std::vector<std::string>{id},"Keep uses content identity");
    rejects<WorkflowOutputExists>([&]{keep_expression_matrix(*project,1,source);});
    doc->undo();check(expression_matrix_ids(*project,1).empty(),"Keep is one Undo command");doc->redo();
    project->save_new((bundle/"study.qpx").u8string());
    check(reopen_expression_matrix(*project,1,id)->read(selection).sha256==block.sha256,"Saved external reference reads exact counts");
    const auto source_bytes=bytes(assets/"source.qmx");
    rejects([&]{project->save((assets/"source.qmx").u8string());});
    const auto alias=assets/"alias.qmx";fs::create_hard_link(assets/"source.qmx",alias);
    rejects([&]{project->save(alias.u8string());});check(bytes(alias)==source_bytes,"Project output cannot overwrite source/alias");fs::remove(alias);
    const auto moved=root/"relocated";fs::rename(bundle,moved);
    auto relocated=Project::open((moved/"study.qpx").u8string());
    check(reopen_expression_matrix(*relocated,1,id)->read(selection).sha256==block.sha256,"External relative path follows a relocated project tree");
    auto ref=load_expression_matrix_association(*relocated,1,id);
    check(fs::u8path(ref.path)==moved/"assets/source.qmx" && !ref.owned,"Live external locator resolves absolute");
    const auto alternative=root/"same.qmx";fs::copy_file(moved/"assets/source.qmx",alternative);
    auto exact=open_expression_matrix(alternative.u8string(),imported.file_sha256);
    auto linked_doc=relocated->document(1);relink_expression_matrix(*relocated,1,id,exact);
    check(load_expression_matrix_association(*relocated,1,id).path==alternative.u8string(),"Verified relink changes locator only");
    linked_doc->undo();check(load_expression_matrix_association(*relocated,1,id).path==ref.path,"Relink Undo restores locator");linked_doc->redo();
    fs::rename(alternative,root/"offline.qmx");rejects([&]{reopen_expression_matrix(*relocated,1,id);});
    relocated->save((moved/"missing.qpx").u8string());
    check(expression_matrix_ids(*Project::open((moved/"missing.qpx").u8string()),1).size()==1,"Missing external source remains identifiable and savable");
    auto offline=open_expression_matrix((root/"offline.qmx").u8string(),imported.file_sha256);relink_expression_matrix(*relocated,1,id,offline);
    const auto owned_a=root/"owned-a",owned_b=root/"owned-b",owned_c=root/"owned-c";
    fs::create_directory(owned_a);fs::create_directory(owned_b);fs::create_directory(owned_c);
    auto owner=std::make_shared<Project>();owner->add_document(std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,40))));
    owner->save_new((owned_a/"study.qpx").u8string());const auto owned_id=keep_expression_matrix(*owner,1,offline,true);
    const auto owned_ref=load_expression_matrix_association(*owner,1,owned_id);
    const auto owned_file=fs::u8path(owned_ref.path);
    check(owned_ref.owned && bytes(owned_a/owned_file)==source_bytes,"Owned retention copies exact bytes");
    const auto stamp=fs::last_write_time(owned_a/owned_file);owner->save((owned_a/"study.qpx").u8string());
    check(fs::last_write_time(owned_a/owned_file)==stamp,"Ordinary save leaves owned bytes untouched");
    remove_expression_matrix(*owner,1,owned_id);
    check(expression_matrix_ids(*owner,1).empty() && fs::exists(owned_a/owned_file),"Remove never deletes shared bytes");
    owner->document(1)->undo();check(reopen_expression_matrix(*owner,1,owned_id)->read(selection).sha256==block.sha256,"Undo restores owned exact source");
    owner->save_new((owned_b/"study.qpx").u8string());
    check(bytes(owned_b/owned_file)==source_bytes && reopen_expression_matrix(*owner,1,owned_id)->read(selection).sha256==block.sha256,"Save As copies before commit and live project resolves destination");
    fs::rename(owned_a,root/"owned-a-offline");fs::rename(root/"offline.qmx",root/"original-offline.qmx");
    auto owned_reopened=Project::open((owned_b/"study.qpx").u8string());
    check(reopen_expression_matrix(*owned_reopened,1,owned_id)->read(selection).sha256==block.sha256,"Owned project does not depend on original location");
    ProjectSaveJob save;check(save.request(owner,(owned_c/"study.qpx").u8string()),"Async snapshot save starts");save.wait();
    check(save.stats().state=="saved" && reopen_expression_matrix(*owner,1,owned_id)->read(selection).sha256==block.sha256,"Async Save As copies and resolves owned data without Python");
    auto check_c=Project::open((owned_c/"study.qpx").u8string());
    check(reopen_expression_matrix(*check_c,1,owned_id)->read(selection).counts==block.counts,"Exact large uint64 survives owned project persistence");
    const auto collision=root/"collision";fs::create_directory(collision);fs::create_directory(collision/".qpx-expression");write(collision/owned_file,"unrelated bytes");
    const auto revision=owner->document(1)->revision();
    rejects([&]{owner->save_new((collision/"study.qpx").u8string());});
    check(!fs::exists(collision/"study.qpx") && bytes(collision/owned_file)=="unrelated bytes" && owner->document(1)->revision()==revision,"Conflicting owned file refuses project publication without overwrite");
    std::atomic<bool> cancel{true};rejects<WorkflowCancelled>([&]{relink_expression_matrix(*owner,1,owned_id,exact,&cancel);});
    check(owner->document(1)->revision()==revision,"Cancelled relink does not publish");
    write(root/"different.mtx","%%MatrixMarket matrix coordinate integer general\n2 3 1\n1 1 4\n");
    auto other=import_expression_matrix((root/"different.mtx").u8string(),(root/"features.tsv").u8string(),(root/"observations.tsv").u8string(),(root/"different.qmx").u8string());
    rejects<WorkflowInputChanged>([&]{relink_expression_matrix(*owner,1,owned_id,open_expression_matrix(other.path,other.file_sha256));});
    check(owner->document(1)->revision()==revision,"Different content cannot relink under old identity");
    auto malformed=owner->document(1)->image_data_properties()->values();
    auto& malformed_records=std::get<V::Map>(malformed.at("qupathxx.expression-matrices/1").value);
    auto& malformed_record=std::get<V::Map>(malformed_records.at(owned_id).value);
    malformed_record.at("path")=V{std::string("../outside.qmx")};
    owner->document(1)->set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(malformed)));
    rejects<std::invalid_argument>([&]{expression_matrix_ids(*owner,1);});
    rejects<std::invalid_argument>([&]{owner->save_new((owned_c/"invalid.qpx").u8string());});
    check(!fs::exists(owned_c/"invalid.qpx"),"Malformed stored owned path cannot publish a project");
    owner->document(1)->undo();
    const auto moved_owned=root/"owned-relocated";fs::rename(owned_c,moved_owned);
    check(reopen_expression_matrix(*Project::open((moved_owned/"study.qpx").u8string()),1,owned_id)->read(selection).sha256==block.sha256,"Owned bundle relocation keeps source/result identity");
    auto bad=load_expression_matrix_association(*owner,1,owned_id);bad.path="../outside.qmx";rejects<std::invalid_argument>([&]{bad.validate();});
    check(linked_doc->image_data_properties()->values().count("note"),"Association commands preserve unrelated properties");
    no_temporary(root);
}
int main(){try{if(!has_project() || !has_expression_matrix_storage())return 0;const auto root=fs::current_path()/("expression-project-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);exercise(root);std::cout<<"Native expression associations: external/owned relocation, exact verified relink, Undo, Save As/async persistence, missing/corrupt/cancel and protected output pass\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
