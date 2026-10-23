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

using namespace qpx;
void check(bool b) {if(!b) throw std::runtime_error("TMA check failed");}
template<class F> void rejects(F f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing TMA rejection");}
int main() {
    namespace fs=std::filesystem;const auto root=fs::current_path()/("tma-test-"+std::to_string(std::random_device{}()));
    try {
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
        {const auto a=doc->annotation(ids[0]);const auto table="source_object_id,source_object_revision,value\r\n"+std::to_string(a.id)+','+std::to_string(a.revision)+",\"a,b\n\"\"q\"\"\"\r\n";
         const auto joined=tma_context_csv(*snapshot,table);check(joined.find("qpx.tma-context/1")!=std::string::npos && joined.find("patient-1")!=std::string::npos);rejects([&]{tma_context_csv(*snapshot,table,1);});rejects([&]{tma_metadata_csv(*snapshot,1);});
         const auto stale="source_object_id,source_object_revision\r\n"+std::to_string(a.id)+','+std::to_string(a.revision+1)+"\r\n";rejects([&]{tma_context_csv(*snapshot,stale);});}

        if(has_project()) {
            fs::create_directory(root);Project project;const auto image=project.add_document(doc,"TMA image");project.save((root/"source.qpxproj").u8string());
            const auto opened=Project::open((root/"source.qpxproj").u8string());const auto restored=opened->document(image);check(restored->tma_core_count()==6 && restored->tma_grid()->info()->name()=="Relabeled" && restored->annotation(ids[1]).tma_core->missing() && restored->annotation(filled).tma_core->case_id()=="replacement");
            ProjectBatchOptions batch;batch.tma_only=true;ProjectBatchRequest capture(project,StainTransform({{1,0,0},{0,1,0}}),{},batch);check(capture.object_count()==5);doc->set_tma_missing({ids[0]},true);
            const auto result=run_project_batch(capture,(root/"batch").u8string());if(result.state!="succeeded") throw std::runtime_error(result.error);check(Project::open(result.images[0].path)->document(image)->tma_core_count()==6 && result.images[0].records==5);check(batch_checkpoint_csv(result.images[0].path).find("tma_case_id")!=std::string::npos);check(run_project_batch(capture,(root/"batch").u8string()).reused==1);
            check(root.parent_path()==fs::current_path());fs::remove_all(root);
        }
        doc->clear_tma_grid();check(!doc->tma_grid() && doc->object_count()>0);check(doc->undo() && doc->tma_core_count()==6);doc->clear_tma_grid(true);check(!doc->tma_core_count() && doc->object_count()==1);check(doc->undo() && doc->tma_core_count()==6);
        doc.reset();check(snapshot->core(0,1)->tma_core->missing() && original->core(1,2)->name=="B-3");
        std::cout<<"TMA grid commands, missing cores, case IDs, snapshots, history and persistence passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
