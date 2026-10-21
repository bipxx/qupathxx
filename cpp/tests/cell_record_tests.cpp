#include "../cell_archive.hpp"
#include "qupathxx/project_save.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx;
void check(bool v) {if(!v) throw std::runtime_error("cell archive check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing cell archive rejection");}
int main() {
 try {
    rejects<std::invalid_argument>([]{CellRecord{}.payload_bytes();});
    StainTransform transform({{1,0,0},{0,1,0}});CellParameters p;p.sigma=0;p.min_nucleus_pixels=1;p.min_seed_distance=0;p.cell_expansion=1;
    for(unsigned bits=0;bits<512;++bits) for(auto connectivity:{4,8}) {
        std::vector<std::uint8_t> rgb;for(int i=0;i<9;++i) for(int c=0;c<3;++c) rgb.push_back(bits&(1u<<i)?0:255);
        Document doc(Image(3,3,rgb));const auto obj=doc.add_rectangle({0,0,3,3});p.connectivity=connectivity;
        CellRequest request(doc,obj,transform,p);const auto result=detect_cells(request);auto bytes=CellArchiveCodec::encode(*result);
        const auto loaded=CellArchiveCodec::decode(bytes.data(),bytes.size(),result->nuclei()->roi(),doc.metadata());
        check(loaded->nuclei()->labels()==result->nuclei()->labels() && loaded->cells()->labels()==result->cells()->labels());
        check(loaded->features().size()==result->features().size() && loaded->markers().size()==result->markers().size());
        const auto id=doc.keep_cells(request,result);check(doc.cell_status(id)=="current" && doc.cell_record(id)->result==result && doc.verify_cells(id,request,*result));
        rejects<std::invalid_argument>([&]{doc.keep_cells(request,loaded);});
        CellRequest unrelated(doc,obj,transform,p);rejects<std::invalid_argument>([&]{doc.keep_cells(unrelated,result);});
        bytes[bytes.size()/2]^=1;rejects<std::invalid_argument>([&]{CellArchiveCodec::decode(bytes.data(),bytes.size(),result->nuclei()->roi(),doc.metadata());});
    }
    auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,0)));const auto obj=doc->add_rectangle({.2,.3,14.5,14.2});
    CellRequest request(*doc,obj,transform,p);const auto result=detect_cells(request);const auto first=doc->keep_cells(request,result);
    auto copy=*doc;check(copy.cell_status(first)=="unverified");check(doc->remove_cells(first) && !doc->remove_cells(first) && copy.cell_record_count()==1);
    const auto second=doc->keep_cells(request,result);auto replay=CellRequest::replay(copy,first);
    check(copy.verify_cells(first,replay,*detect_cells(replay)) && copy.cell_status(first)=="current" && doc->latest_cells(obj)->id==second);
    std::atomic<bool> cancel{true};rejects<std::runtime_error>([&]{CellArchiveCodec::encode(*result,&cancel);});
    const auto encoded=CellArchiveCodec::encode(*result);rejects<std::runtime_error>([&]{CellArchiveCodec::decode(encoded.data(),encoded.size(),result->nuclei()->roi(),doc->metadata(),&cancel);});
    doc->rename_annotation(obj,"Changed");check(doc->cell_status(second)=="object_changed");doc->undo();rejects<std::invalid_argument>([&]{CellRequest::replay(*doc,second);});
    if(has_project()) {
        auto project=std::make_shared<Project>();auto live=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,0)));const auto image=project->add_document(live);
        const auto source=live->add_rectangle({0,0,8,8});CellRequest captured(*live,source,transform,p);const auto detected=detect_cells(captured);const auto saved=live->keep_cells(captured,detected);
        const auto path=(std::filesystem::current_path()/(project->identity()+".qpxproj")).u8string();ProjectSaveJob job;check(job.request(project,path));
        check(live->remove_cells(saved));const auto newer=live->keep_cells(captured,detected);check(job.wait().state=="saved" && project->dirty());
        const auto restored=Project::open(path)->document(image);check(restored->cell_record_count()==1 && restored->cell_record(saved)->result->nuclei()->labels()==detected->nuclei()->labels() && restored->cell_status(saved)=="unverified");
        check(job.request(project,path) && job.wait().state=="saved" && Project::open(path)->document(image)->cell_record(newer)->result->cells()->labels()==detected->cells()->labels());
        std::filesystem::remove(std::filesystem::u8path(path));
    }
    {
        Document linked(Image(16,16,std::vector<std::uint8_t>(16*16*3,0)));const auto source=linked.add_rectangle({0,0,16,16});
        CellRequest input(linked,source,transform,p);const auto detected=detect_cells(input);check(!detected->features().empty());
        if(has_geos()) {
            linked.clear_history();const auto ids=linked.publish_cells(input,detected);check(ids.size()==detected->features().size() && linked.cell_record_count()==1);
            const auto cell=ids.at(0);const auto run=linked.annotation(cell).cell_feature_link->record_id;
            check(linked.cell_object_status(cell)=="current" && linked.cell_object_feature(cell).cell_pixels==detected->features().at(0).cell_pixels);
            check(linked.undo() && linked.object_count()==1 && linked.cell_record_count()==1 && linked.redo());
            linked.rename_annotation(cell,"Renamed");check(linked.cell_object_status(cell)=="current");
            linked.update_nucleus(cell,std::nullopt);check(linked.cell_object_status(cell)=="geometry_changed" && linked.undo() && linked.cell_object_status(cell)=="current");
            auto forged=ObjectEdit::replace_geometry(cell,linked.annotation(cell).geometry,"forged",source,"",ObjectKind::Cell,linked.annotation(cell).nucleus);
            forged.value.cell_feature_link=CellFeatureLink{std::numeric_limits<std::uint64_t>::max(),1};
            const auto before=linked.revision();rejects<std::invalid_argument>([&]{linked.execute({forged});});check(linked.revision()==before);
            check(linked.remove_cells(run) && linked.cell_object_status(cell)=="archive_missing");rejects<std::out_of_range>([&]{linked.cell_object_feature(cell);});
        } else rejects<std::runtime_error>([&]{linked.publish_cells(input,detected);});
    }
    Document tiny(Image(1,1,{0,0,0}));const auto source=tiny.add_rectangle({0,0,1,1});CellRequest captured(tiny,source,transform,p);const auto small=detect_cells(captured);
    for(int i=0;i<10000;++i) tiny.keep_cells(captured,small);
    const auto revision=tiny.revision();rejects<std::length_error>([&]{tiny.keep_cells(captured,small);});check(tiny.revision()==revision && tiny.cell_record_count()==10000);
    std::cout<<"Cell archive exhaustive masks, authority, snapshots, cancellation and limits passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
