#include "qupathxx/tma.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_batch.hpp"
#include "../tma_internal.hpp"
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace qpx;
void check(bool b) {if(!b) throw std::runtime_error("TMA check failed");}
template<class F> void rejects(F f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing TMA rejection");}
void case_summary_contracts() {
    auto doc=std::make_shared<Document>(Image(256,192,std::vector<std::uint8_t>(256*192*3,255)));
    TmaGridOptions o;o.rows=2;o.columns=3;o.origin={30,30};o.row_step={0,60};o.column_step={60,0};o.width=30;o.height=24;
    const auto ids=doc->create_tma_grid(o);
    const std::string case_a="case,\"组织\"\nA";
    doc->set_tma_case_ids({{ids[0],case_a},{ids[1],case_a},{ids[2],"B"},{ids[4],case_a}});
    doc->set_tma_missing({ids[1]},true);doc->remove_annotation(ids[5]);const auto snapshot=doc->tma_grid();
    auto row=[&](std::size_t slot,const std::string& value,const std::string& weight) {
        return std::to_string(ids[slot])+','+std::to_string(doc->annotation(ids[slot]).revision)+','+value+','+weight+"\r\n";
    };
    const std::string header="source_object_id,source_object_revision,Value,Weight\r\n";
    const auto table=header+row(0,"2","1")+row(0,"4","3")+row(4,"8","2")+row(1,"100","20")+row(2,"10","0")+row(3,"12","2");
    const auto summary=tma_case_summary_csv(*snapshot,table,"Value","Weight");
    check(summary.find("qpx.tma-case-summary/1")!=std::string::npos && summary.find("case,\"\"组织\"\"\nA")!=std::string::npos);
    check(summary.find(",6,1,")!=std::string::npos && summary.find(",3,2,1,2,3,1,\"Value\",\"Weight\",")!=std::string::npos);
    check(summary.find(",14,6,30,")!=std::string::npos && summary.find(",5,2,8\r\n")!=std::string::npos);
    check(summary.find(",10,0,0,10,,10,10\r\n")!=std::string::npos);
    check(summary.find("\"\","+std::to_string(ids[3])+",1,1,0,1,1,0,")!=std::string::npos);
    const auto empty=tma_case_summary_csv(*snapshot,header,"Value","Weight");check(empty.find("no_measurements")!=std::string::npos && empty.find("nan")==std::string::npos);
    const auto metadata=tma_case_summary_csv(*snapshot);check(metadata.find("no_measurements")!=std::string::npos);
    rejects([&]{tma_case_summary_csv(*snapshot,table,"missing");});rejects([&]{tma_case_summary_csv(*snapshot,table,"Value","Weight",1);});
    rejects([&]{tma_case_summary_csv(*snapshot,header+row(0,"nan","1"),"Value","Weight");});
    rejects([&]{tma_case_summary_csv(*snapshot,header+row(0,"2","-1"),"Value","Weight");});
    rejects([&]{tma_case_summary_csv(*snapshot,header+row(0,"1e308","1e308"),"Value","Weight");});
    rejects([&]{tma_case_summary_csv(*snapshot,header+row(0,"1e-300","1e-300"),"Value","Weight");});
    const auto unversioned="source_object_id,Value\n"+std::to_string(ids[0])+",2\n";
    rejects([&]{tma_case_summary_csv(*snapshot,unversioned,"Value");});
    check(tma_case_summary_csv(*snapshot,unversioned,"Value","",64*1024*1024,true).find("unversioned")!=std::string::npos);
    doc->set_tma_case_ids({{ids[0],"changed"}});const auto current=doc->tma_grid();
    rejects([&]{tma_case_summary_csv(*current,table,"Value","Weight",64*1024*1024,true);});
    std::atomic<bool> stop{true};rejects([&]{tma_case_summary_csv(*snapshot,table,"Value","Weight",64*1024*1024,false,&stop);});
    bool cancelled_before_io=false;
    try {save_tma_case_summary_csv(*snapshot,"","","","",64*1024*1024,false,&stop);}
    catch(const std::exception& e) {cancelled_before_io=std::string(e.what()).find("cancel")!=std::string::npos;}
    check(cancelled_before_io);
    std::cout<<"TMA case file cancellation precedes output path access\n";
    const auto selectors=std::to_string(ids[0])+','+std::to_string(snapshot->core(0,0)->revision)+',';
    const auto signed_table="object_id,object_revision,Value\n"+selectors+"1e16\n"+selectors+"1\n"+selectors+"-1e16\n";
    check(tma_case_summary_csv(*snapshot,signed_table,"Value").find(",1,3,1,0.33333333333333331,")!=std::string::npos);
    const auto tiny_table="object_id,object_revision,Value\n"+selectors+"1e-323\n"+selectors+"0\n"+selectors+"0\n"+selectors+"0\n"+selectors+"0\n";
    rejects([&]{tma_case_summary_csv(*snapshot,tiny_table,"Value");});
    std::cout<<"TMA compensated signed sums and nonzero mean underflow checks passed\n";
    doc.reset();check(tma_case_summary_csv(*snapshot,table,"Value","Weight")==summary);
    std::cout<<"TMA case aggregation, weighted arithmetic, missing/unassigned cores, stale revisions and cancellation passed\n";
}
void cohort_summary_contracts() {
    auto make=[] {
        auto doc=std::make_shared<Document>(Image(256,192,std::vector<std::uint8_t>(256*192*3,32)));
        TmaGridOptions o;o.rows=1;o.columns=3;o.origin={30,30};o.row_step={0,60};o.column_step={60,0};o.width=30;o.height=24;
        doc->create_tma_grid(o);return doc;
    };
    auto a=make(),b=make();const auto ai=a->tma_core_ids(),bi=b->tma_core_ids();check(ai==bi);
    a->set_tma_case_ids({{ai[0],"P"}});a->remove_annotation(ai[2]);
    b->set_tma_case_ids({{bi[0],"P"},{bi[1],"P"}});b->set_tma_missing({bi[1]},true);
    const auto ag=a->tma_grid(),bg=b->tma_grid();const TmaCohort cohort{{11,ag},{22,bg}};
    auto row=[](std::uint64_t image,const Document& doc,std::uint64_t id,const std::string& value,const std::string& weight) {
        return std::to_string(image)+','+std::to_string(id)+','+std::to_string(doc.annotation(id).revision)+','+value+','+weight+"\r\n";
    };
    const std::string header="image_id,source_object_id,source_object_revision,Value,Weight\r\n";
    const auto table=header+row(11,*a,ai[0],"2","1")+row(22,*b,bi[0],"4","3")+row(11,*a,ai[0],"8","2")+
        row(22,*b,bi[1],"100","20")+row(11,*a,ai[1],"10","0")+row(22,*b,bi[2],"12","2");
    const auto summary=tma_cohort_case_summary_csv(cohort,table,"Value","Weight");
    check(summary.find("qpx.tma-cohort-case-summary/1")!=std::string::npos);
    check(summary.find(",\"P\",,,2,3,2,1,2,3,1,\"Value\",\"Weight\",")!=std::string::npos);
    check(summary.find(",14,6,30,4.666666666666667,5,2,8\r\n")!=std::string::npos);
    check(summary.find(",\"\",11,"+std::to_string(ai[1])+",1,1,1,0,1,1,0,")!=std::string::npos);
    check(summary.find(",\"\",22,"+std::to_string(bi[2])+",1,1,1,0,1,1,0,")!=std::string::npos);
    check(summary.find(",10,0,0,10,,10,10\r\n")!=std::string::npos);
    check(summary.find(",12,2,24,12,12,12,12\r\n")!=std::string::npos);
    const auto manifest=tma_cohort_manifest_csv(cohort);
    check(manifest.find("qpx.tma-cohort-sources/1")!=std::string::npos && manifest.find("\",11,")!=std::string::npos && manifest.find("\",22,")!=std::string::npos);
    check(tma_cohort_case_summary_csv({{22,bg},{11,ag}},table,"Value","Weight")==summary);
    check(tma_cohort_manifest_csv({{22,bg},{11,ag}})==manifest);
    auto alias=table;alias.replace(0,8,"source_image_id");const auto aliased=tma_cohort_case_summary_csv(cohort,alias,"Value","Weight");
    check(aliased.find(",14,6,30,4.666666666666667,5,2,8\r\n")!=std::string::npos);
    const auto no_images="source_object_id,source_object_revision,Value\r\n";
    rejects([&]{tma_cohort_case_summary_csv(cohort,no_images,"Value");});
    rejects([&]{tma_cohort_case_summary_csv(cohort,header+row(33,*a,ai[0],"2","1"),"Value","Weight");});
    rejects([&]{tma_cohort_case_summary_csv(cohort,header+row(11,*b,bi[2],"2","1"),"Value","Weight");});
    rejects([&]{tma_cohort_manifest_csv({});});rejects([&]{tma_cohort_manifest_csv({{0,ag}});});
    rejects([&]{tma_cohort_manifest_csv({{11,nullptr}});});
    rejects([&]{tma_cohort_manifest_csv(cohort,ag->payload_bytes()+bg->payload_bytes()-1);});
    rejects([&]{tma_cohort_case_summary_csv(cohort,table,"Value","Weight",1);});
    const auto legacy="image_id,object_id,Value\n11,"+std::to_string(ai[0])+",2\n";
    rejects([&]{tma_cohort_case_summary_csv(cohort,legacy,"Value");});
    check(tma_cohort_case_summary_csv(cohort,legacy,"Value","",64*1024*1024,true).find("unversioned")!=std::string::npos);
    const auto inventory=tma_cohort_case_summary_csv(cohort);
    check(inventory.find("no_measurements")!=std::string::npos && inventory.find(",\"P\",,,2,3,2,1,0,0,0,")!=std::string::npos);
    std::atomic<bool> stop{true};rejects([&]{tma_cohort_manifest_csv(cohort,64*1024*1024,&stop);});
    rejects([&]{tma_cohort_case_summary_csv(cohort,table,"Value","Weight",64*1024*1024,false,&stop);});
    b->set_tma_case_ids({{bi[0],"changed"}});const TmaCohort changed{{11,ag},{22,b->tma_grid()}};
    check(tma_cohort_manifest_csv(changed)!=manifest);
    rejects([&]{tma_cohort_case_summary_csv(changed,table,"Value","Weight",64*1024*1024,true);});
    a.reset();b.reset();check(tma_cohort_case_summary_csv(cohort,table,"Value","Weight")==summary);
    std::cout<<"TMA cohort raw-row pooling, image/core namespaces, isolated unassigned cores, source manifests, stale revisions and cancellation passed\n";
}
int main() {
    namespace fs=std::filesystem;const auto root=fs::current_path()/("tma-test-"+std::to_string(std::random_device{}()));
    try {
        case_summary_contracts();
        cohort_summary_contracts();
        auto doc=std::make_shared<Document>(Image(256,192,std::vector<std::uint8_t>(256*192*3,32)));
        TmaGridOptions options;options.rows=2;options.columns=3;options.origin={30,30};options.row_step={0,60};options.column_step={60,0};options.width=30;options.height=24;
        options.name="Test grid";options.row_labels={"A","B"};const auto ids=doc->create_tma_grid(options);check(ids.size()==6 && doc->tma_core_count()==6 && doc->tma_core_ids().size()==6 && doc->revision()==1);
        const auto original=doc->tma_grid();check(original->info()->name()=="Test grid" && original->core(1,2)->name=="B-3");
        const auto revision=doc->revision();doc->set_tma_missing({ids[1],ids[1]},true);check(doc->revision()==revision+1 && doc->tma_core_ids().size()==5 && !original->core(0,1)->tma_core->missing());
        doc->set_tma_missing({ids[1]},true);check(doc->revision()==revision+1);
        doc->set_tma_case_ids({{ids[0],"patient-1"},{ids[1],"patient-1"}});check(doc->annotation(ids[0]).tma_core->case_id()=="patient-1");
        check(doc->undo() && doc->annotation(ids[0]).tma_core->case_id().empty());check(doc->redo() && doc->annotation(ids[1]).tma_core->case_id()=="patient-1");
        doc->rename_annotation(ids[0],"Sample one");check(doc->annotation(ids[0]).tma_core->case_id()=="patient-1");
        doc->set_tma_labels("Relabeled",{"X","Y"},{"a","b","c"},false);check(doc->annotation(ids[0]).name=="Sample one" && doc->annotation(ids[0]).tma_core->row_label()=="X");
        const auto child=doc->add_cell(Roi::ellipse({26,27,5,4}),Roi::ellipse({27,28,2,1}),"Cell",ids[0]);const auto before=doc->annotation(child).bounds(),nucleus_before=doc->annotation(child).nucleus->bounds();doc->translate_tma_grid(4,7);
        check(doc->annotation(child).bounds().x==before.x+4 && doc->annotation(child).nucleus->bounds().y==nucleus_before.y+7);check(doc->undo());
        const auto r=doc->revision();rejects([&]{doc->update_roi(ids[0],Roi::rectangle({10,10,30,24}));});check(doc->revision()==r && doc->annotation(ids[0]).geometry.kind()==RoiKind::Ellipse);
        const auto ordinary=doc->add_rectangle({200,140,10,10});const auto state=doc->revision();rejects([&]{doc->set_tma_missing({ids[0],ordinary},true);});check(doc->revision()==state && !doc->annotation(ids[0]).tma_core->missing());
        rejects([&]{doc->tma_grid(1);});auto bad=options;bad.rows=0;rejects([&]{bad.validate();});bad=options;bad.row_step=bad.column_step;rejects([&]{bad.validate();});
        const auto blob=TmaCodec::encode(*doc->tma_grid()->info());check(TmaCodec::decode(blob.data(),blob.size())->same_layout(*doc->tma_grid()->info()));auto corrupt=blob;corrupt.back()^=1;rejects([&]{TmaCodec::decode(corrupt.data(),corrupt.size());});
        doc->remove_annotation(ids[2]);check(!doc->tma_grid()->core(0,2));
        {const auto csv=tma_metadata_csv(*doc->tma_grid());std::istringstream input(csv);std::string line;std::size_t lines=0;while(std::getline(input,line)) {check(std::count(line.begin(),line.end(),',')==17);++lines;}check(lines==7 && csv.find("vacant")!=std::string::npos);}
        const auto filled=doc->add_tma_core(0,2,{135,18,30,24},"replacement");check(filled!=ids[2] && doc->tma_grid()->core(0,2)->id==filled);
        const auto snapshot=doc->tma_grid();
        check(fs::create_directory(root));
        {
            const auto a=snapshot->core(0,0);const auto input=root/"case-input.csv",output=root/"cases.csv",inventory=root/"inventory.csv";
            const auto table="object_id,object_revision,Value,Weight\r\n"+std::to_string(a->id)+','+std::to_string(a->revision)+",2,3\r\n";
            {std::ofstream stream(input,std::ios::binary);stream.write(table.data(),table.size());check(bool(stream));}
            auto read=[](const fs::path& path) {std::ifstream stream(path,std::ios::binary);check(bool(stream));return std::string(std::istreambuf_iterator<char>(stream),{});};
            const auto expected=tma_case_summary_csv(*snapshot,table,"Value","Weight");
            check(save_tma_case_summary_csv(*snapshot,output.u8string(),input.u8string(),"Value","Weight")==expected.size());
            check(read(output)==expected && read(input)==table);
            rejects([&]{save_tma_case_summary_csv(*snapshot,output.u8string());});check(read(output)==expected);
            rejects([&]{save_tma_case_summary_csv(*snapshot,input.u8string(),input.u8string(),"Value");});check(read(input)==table);
            rejects([&]{save_tma_case_summary_csv(*snapshot,(root/"limited.csv").u8string(),input.u8string(),"Value","",1);});check(!fs::exists(root/"limited.csv"));
            check(save_tma_case_summary_csv(*snapshot,inventory.u8string())==tma_case_summary_csv(*snapshot).size());
            check(read(inventory)==tma_case_summary_csv(*snapshot));
            const auto oversized=root/"oversized.csv",rejected=root/"oversized-output.csv";
            {std::ofstream stream(oversized,std::ios::binary);const std::string bytes(2*1024*1024,'x');stream.write(bytes.data(),bytes.size());check(bool(stream));}
#ifdef _WIN32
            IO_COUNTERS before{},after{};check(GetProcessIoCounters(GetCurrentProcess(),&before)!=0);
#endif
            rejects([&]{save_tma_case_summary_csv(*snapshot,rejected.u8string(),oversized.u8string(),"Value","",1024*1024);});
#ifdef _WIN32
            check(GetProcessIoCounters(GetCurrentProcess(),&after)!=0);
            const auto read_bytes=after.ReadTransferCount-before.ReadTransferCount;
            std::cout<<"Oversized CSV read-transfer bytes: "<<read_bytes<<'\n'<<std::flush;
            check(read_bytes==0);
#endif
            check(!fs::exists(rejected) && fs::file_size(oversized)==2*1024*1024);
            std::cout<<"Oversized CSV file budget and output protection passed\n";
            for(const auto& entry:fs::directory_iterator(root)) check(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos);
            std::cout<<"TMA case file exports preserve captured CSV bytes and existing files\n";
        }
        {const auto a=doc->annotation(ids[0]);const auto table="source_object_id,source_object_revision,value\r\n"+std::to_string(a.id)+','+std::to_string(a.revision)+",\"a,b\n\"\"q\"\"\"\r\n";
         const auto joined=tma_context_csv(*snapshot,table);check(joined.find("qpx.tma-context/1")!=std::string::npos && joined.find("patient-1")!=std::string::npos);rejects([&]{tma_context_csv(*snapshot,table,1);});rejects([&]{tma_metadata_csv(*snapshot,1);});
         const auto stale="source_object_id,source_object_revision\r\n"+std::to_string(a.id)+','+std::to_string(a.revision+1)+"\r\n";rejects([&]{tma_context_csv(*snapshot,stale);});}

        if(has_project()) {
            Project project;const auto image=project.add_document(doc,"TMA image");project.save((root/"source.qpxproj").u8string());
            const auto opened=Project::open((root/"source.qpxproj").u8string());const auto restored=opened->document(image);check(restored->tma_core_count()==6 && restored->tma_grid()->info()->name()=="Relabeled" && restored->annotation(ids[1]).tma_core->missing() && restored->annotation(filled).tma_core->case_id()=="replacement");
            ProjectBatchOptions batch;batch.tma_only=true;ProjectBatchRequest capture(project,StainTransform({{1,0,0},{0,1,0}}),{},batch);check(capture.object_count()==5);doc->set_tma_missing({ids[0]},true);
            const auto result=run_project_batch(capture,(root/"batch").u8string());if(result.state!="succeeded") throw std::runtime_error(result.error);check(Project::open(result.images[0].path)->document(image)->tma_core_count()==6 && result.images[0].records==5);check(batch_checkpoint_csv(result.images[0].path).find("tma_case_id")!=std::string::npos);check(run_project_batch(capture,(root/"batch").u8string()).reused==1);
        }
        doc->clear_tma_grid();check(!doc->tma_grid() && doc->object_count()>0);check(doc->undo() && doc->tma_core_count()==6);doc->clear_tma_grid(true);check(!doc->tma_core_count() && doc->object_count()==1);check(doc->undo() && doc->tma_core_count()==6);
        doc.reset();check(snapshot->core(0,1)->tma_core->missing() && original->core(1,2)->name=="B-3");
        check(root.parent_path()==fs::current_path());fs::remove_all(root);
        std::cout<<"TMA grid commands, missing cores, case IDs, snapshots, history and persistence passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
