#include "qupathxx/qptma.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool ok){if(!ok)throw std::runtime_error("qptma check failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("qptma rejection missing");}
bool equal(const QptmaCore& a,const QptmaCore& b){return a.name==b.name && a.case_id==b.case_id && a.missing==b.missing && a.bounds.x==b.bounds.x && a.bounds.y==b.bounds.y && a.bounds.width==b.bounds.width && a.bounds.height==b.bounds.height;}
int main(int argc,char** argv){try {
    check(argc==3);const auto fixture=std::filesystem::u8path(argv[1]);
    const auto data=load_qptma((fixture/"fractional-import.qptma").u8string());
    const auto upstream=load_qptma((fixture/"qupath-export.qptma").u8string());
    check(data.rows==2 && data.columns==3 && data.cores.size()==6);
    check(data.cores[0].bounds.x==12.75 && data.cores[0].bounds.width==17.25 && upstream.cores[0].bounds.x==12 && upstream.cores[0].bounds.width==18);
    check(data.cores[1].missing && data.cores[4].missing && data.cores[0].case_id==data.cores[1].case_id && data.cores[2].case_id.empty());
    check(data.cores[3].case_id=="patient \xce\xb1" && data.cores[4].case_id=="comma, value" && data.cores[5].case_id=="quoted \"case\"");
    const auto text=qptma_text(data);auto round=parse_qptma(text);for(std::size_t i=0;i<6;++i)check(equal(data.cores[i],round.cores[i]));
    std::string crlf="\xef\xbb\xbf";for(char c:text){if(c=='\n')crlf+='\r';crlf+=c;}
    check(parse_qptma(crlf).cores[0].bounds.x==12.75);
    auto quoted=data;quoted.cores[0].name="\"quoted name\"";quoted.cores[0].case_id=" spaced ";
    check(equal(parse_qptma(qptma_text(quoted)).cores[0],quoted.cores[0]));
    Document doc(Image(256,192,std::vector<std::uint8_t>(256*192*3,255)));
    const auto ids=import_qptma(doc,data);check(ids.size()==6 && doc.tma_core_count()==6);
    const auto report=qptma_from_grid(*doc.tma_grid(),data.source_hint,data.image_name);
    check(report.omitted_metadata.size()==3);for(std::size_t i=0;i<6;++i)check(equal(report.data.cores[i],data.cores[i]));
    check(doc.undo() && doc.tma_core_count()==0);check(doc.redo() && doc.tma_core_count()==6);
    const auto revision=doc.revision();rejects([&]{import_qptma(doc,data);});check(doc.revision()==revision && doc.tma_core_count()==6);
    Document other(Image(256,192,std::vector<std::uint8_t>(256*192*3,255)));other.add_rectangle({1,1,2,2});
    auto invalid=data;invalid.cores.back().bounds.x=10000;const auto before=other.revision();
    rejects([&]{import_qptma(other,invalid);});check(other.revision()==before && other.tma_core_count()==0 && other.annotations().size()==1);
    const auto root=std::filesystem::u8path(argv[2])/("qptma-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);
    const auto output=(root/"native-export.qptma").u8string();check(save_qptma(report.data,output)==qptma_text(report.data).size());
    rejects([&]{save_qptma(report.data,output);});check(equal(load_qptma(output).cores[5],data.cores[5]));
    std::atomic<bool> cancelled{true};rejects([&]{load_qptma("nonexistent",1,&cancelled);});rejects([&]{save_qptma(data,(root/"cancelled.qptma").u8string(),1024,&cancelled);});check(!std::filesystem::exists(root/"cancelled.qptma"));
    rejects([&]{parse_qptma(text,text.size()-1);});rejects([&]{qptma_text(data,1);});rejects([&]{parse_qptma(text+"extra\n");});
    const auto replace=[&](std::string from,std::string to){auto bad=text;auto at=bad.find(from);check(at!=std::string::npos);bad.replace(at,from.size(),to);rejects([&]{parse_qptma(bad);});};
    replace("TMA grid width: 3","TMA grid width: 513");replace("TMA grid height: 2","TMA grid height: 0");replace("Core name\tX","Other\tX");
    replace("12.75","nan");replace("12.75","0x1");replace("17.25","-1");replace("\ttrue\t","\tTRUE\t");
    rejects([&]{auto bad=text;bad[0]='\0';parse_qptma(bad);});rejects([&]{auto bad=data;bad.cores.pop_back();qptma_text(bad);});
    QptmaData large;large.rows=100;large.columns=100;large.cores.assign(10000,data.cores[0]);check(parse_qptma(qptma_text(large)).cores.size()==10000);
    std::cout<<"QPTMA native fixture, fractional/quoted wire, atomic history, quotas and 10000-core round trips passed\n";
    std::cout<<"NATIVE_QPTMA_OUTPUT="<<output<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
