#include "../tissue_archive.hpp"
#include "qupathxx/project_save.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("tissue archive check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing tissue archive rejection");}
int main() {
 try {
    rejects<std::invalid_argument>([]{TissueRecord{}.payload_bytes();});
    StainTransform transform({{1,0,0},{0,1,0}});
    for(unsigned bits=0;bits<512;++bits) for(auto connectivity:{4,8}) {
        std::vector<std::uint8_t> rgb;for(int i=0;i<9;++i) for(int c=0;c<3;++c) rgb.push_back(bits&(1u<<i)?0:255);
        Document doc(Image(3,3,rgb));const auto obj=doc.add_rectangle({0,0,3,3});TissueParameters p;p.connectivity=connectivity;
        TissueRequest request(doc,obj,transform,p);const auto mask=detect_tissue(request);
        auto bytes=TissueArchiveCodec::encode(*mask);const auto loaded=TissueArchiveCodec::decode(bytes.data(),bytes.size(),mask->roi(),doc.metadata());
        check(loaded->labels()==mask->labels() && loaded->components().size()==mask->components().size() && loaded->foreground_pixels()==mask->foreground_pixels());
        for(std::size_t i=0;i<mask->components().size();++i) {
            const auto& a=mask->components()[i];const auto& b=loaded->components()[i];
            check(a.label==b.label && a.pixels==b.pixels && a.bounds.x==b.bounds.x && a.bounds.y==b.bounds.y && a.bounds.width==b.bounds.width && a.bounds.height==b.bounds.height && a.centroid.x==b.centroid.x && a.centroid.y==b.centroid.y);
        }
        const auto id=doc.keep_tissue(request,mask);check(doc.tissue_status(id)=="current" && doc.tissue_record(id)->result==mask);
        rejects<std::invalid_argument>([&]{doc.keep_tissue(request,loaded);});
        TissueRequest unrelated(doc,obj,transform,p);rejects<std::invalid_argument>([&]{doc.keep_tissue(unrelated,mask);});
        check(doc.verify_tissue(id,request,*mask));
        bytes[bytes.size()/2]^=1;rejects<std::invalid_argument>([&]{TissueArchiveCodec::decode(bytes.data(),bytes.size(),mask->roi(),doc.metadata());});
    }
    auto doc=std::make_shared<Document>(Image(17,19,std::vector<std::uint8_t>(17*19*3,0)));const auto obj=doc->add_rectangle({.2,.3,15.5,17.2});
    TissueRequest request(*doc,obj,transform);const auto mask=detect_tissue(request);const auto first=doc->keep_tissue(request,mask);
    auto copy=*doc;check(copy.tissue_status(first)=="unverified");
    check(doc->remove_tissue(first) && !doc->remove_tissue(first) && copy.tissue_record_count()==1);
    const auto second=doc->keep_tissue(request,mask);check(second>first && doc->latest_tissue(obj)->id==second && copy.tissue_record(first)->result==mask);
    auto replay=TissueRequest::replay(copy,first);check(copy.verify_tissue(first,replay,*detect_tissue(replay)) && copy.tissue_status(first)=="current");
    std::atomic<bool> cancel{true};rejects<std::runtime_error>([&]{TissueArchiveCodec::encode(*mask,&cancel);});
    const auto encoded=TissueArchiveCodec::encode(*mask);rejects<std::runtime_error>([&]{TissueArchiveCodec::decode(encoded.data(),encoded.size(),mask->roi(),doc->metadata(),&cancel);});
    doc->rename_annotation(obj,"Edited");check(doc->tissue_status(second)=="object_changed");doc->undo();check(doc->tissue_status(second)=="object_changed");
    rejects<std::invalid_argument>([&]{TissueRequest::replay(*doc,second);});
    if(has_project()) {
        auto project=std::make_shared<Project>();auto live=std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,0)));auto image=project->add_document(live);
        auto source=live->add_rectangle({0,0,2,2});TissueRequest capture(*live,source,transform);auto result=detect_tissue(capture);auto saved=live->keep_tissue(capture,result);
        const auto path=(std::filesystem::current_path()/(project->identity()+".qpxproj")).u8string();ProjectSaveJob job;check(job.request(project,path));
        check(live->remove_tissue(saved));auto newer=live->keep_tissue(capture,result);check(job.wait().state=="saved" && project->dirty());
        auto loaded=Project::open(path)->document(image);check(loaded->tissue_record_count()==1 && loaded->tissue_record(saved)->result->labels()==result->labels() && loaded->tissue_status(saved)=="unverified");
        check(job.request(project,path) && job.wait().state=="saved" && !project->dirty());check(Project::open(path)->document(image)->tissue_record(newer)->result->labels()==result->labels());
        std::filesystem::remove(std::filesystem::u8path(path));
    }
    Document tiny(Image(1,1,{0,0,0}));const auto one=tiny.add_rectangle({0,0,1,1});TissueRequest capture(tiny,one,transform);const auto result=detect_tissue(capture);
    for(int i=0;i<10000;++i) tiny.keep_tissue(capture,result);
    const auto revision=tiny.revision();rejects<std::length_error>([&]{tiny.keep_tissue(capture,result);});check(tiny.revision()==revision && tiny.tissue_record_count()==10000);
    std::cout<<"Tissue archive codec, authority, snapshot isolation and limits passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
