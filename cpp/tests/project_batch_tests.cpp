#include "qupathxx/project_batch.hpp"
#include "qupathxx/input_digest.hpp"
#include "qupathxx/object_classifier_archive.hpp"
#include "object_forest_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("project batch check failed");}
template<class F> void rejects(F f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing batch rejection");}
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::current_path()/("batch-test-"+std::to_string(std::random_device{}()));
    try {
        fs::create_directory(root);auto doc=std::make_shared<Document>(Image(32,24,std::vector<std::uint8_t>(32*24*3,16)));
        const auto object=doc->add_rectangle({1,2,20,16});StainTransform t({{1,0,0},{0,1,0}});
        AnalysisRequest a(*doc,object,t);check(input_sha256(a)==analyze(a).input_sha256);
        TissueRequest tr(*doc,object,t);check(input_sha256(tr)==detect_tissue(tr)->input_sha256());
        CellRequest cr(*doc,object,t);check(input_sha256(cr)==detect_cells(cr)->nuclei()->input_sha256());
        Project p;const auto image=p.add_document(doc,"quoted, image\nname");
        if(!has_project()) {rejects([&]{ProjectBatchRequest r(p,t);});fs::remove_all(root);return 0;}
        ObjectClassifierJob classifier;const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture());
        check(classifier.predict(ObjectFeatureRequest(*doc,{object}),model) && classifier.wait(*doc).state=="ready");classifier.apply(*doc);
        const auto audit=doc->classifier_records().back();
        const ProjectBatchRequest captured(p,t);check(captured.image_count()==1 && captured.object_count()==1);
        doc->rename_annotation(object,"later");
        auto r=run_project_batch(captured,(root/"roi").u8string());if(r.state!="succeeded") throw std::runtime_error(r.error+(r.images.empty()?"":r.images[0].error));
        check(r.saved==1 && r.images[0].records==1 && doc->analysis_records().empty());
        auto restored=Project::open(r.images[0].path);check(restored->document(image)->annotation(object).name!="later" && restored->document(image)->analysis_records().size()==1);
        check(restored->document(image)->classifier_record_count()==1 && restored->document(image)->classifier_record(audit->id)->archive->sha256()==audit->archive->sha256());
        const auto csv=batch_checkpoint_csv(r.images[0].path);check(csv.find("batch_image_name")!=std::string::npos && csv.find("quoted, image\nname")!=std::string::npos);
        save_batch_checkpoint_csv(r.images[0].path,(root/"measurements.csv").u8string());
        rejects([&]{save_batch_checkpoint_csv(r.images[0].path,r.images[0].path);});
        auto resume=run_project_batch(captured,(root/"roi").u8string());if(resume.state!="succeeded") throw std::runtime_error(resume.error);check(resume.reused==1 && resume.images[0].path==r.images[0].path);
        restored->save(r.images[0].path);check(run_project_batch(captured,(root/"roi").u8string()).state=="failed");
        for(auto kind:{ProjectBatchKind::Tissue,ProjectBatchKind::Cells}) {ProjectBatchOptions o;o.kind=kind;ProjectBatchJob job;check(job.request(ProjectBatchRequest(p,t,{},o),(root/(kind==ProjectBatchKind::Tissue?"tissue":"cells")).u8string()));check(job.wait().state=="succeeded");check(job.result()->saved==1);check(!batch_checkpoint_csv(job.result()->images[0].path).empty());}
        std::atomic<bool> cancelled{true};check(run_project_batch(captured,(root/"cancel").u8string(),&cancelled).state=="cancelled");
        ProjectBatchOptions small;small.materialize_budget=1;small.failure=ProjectBatchFailure::Continue;check(run_project_batch(ProjectBatchRequest(p,t,{},small),(root/"small").u8string()).state=="partial");
        small={};small.snapshot_budget=ProjectBatchRequest(p,t).snapshot_bytes()-1;rejects([&]{ProjectBatchRequest r(p,t,{},small);});
        Project many;many.add_document(doc,"first");auto other=std::make_shared<Document>(Image(32,24,std::vector<std::uint8_t>(32*24*3,32)));other->add_rectangle({0,0,16,16});many.add_document(other,"second");
        ProjectBatchRequest multi(many,t);cancelled.store(false);auto interrupted=run_project_batch(multi,(root/"interrupted").u8string(),&cancelled,[&](const auto& item){if(item.state=="saved") cancelled.store(true);});
        check(interrupted.state=="cancelled" && interrupted.saved==1);cancelled.store(false);auto restarted=run_project_batch(multi,(root/"interrupted").u8string(),&cancelled);check(restarted.state=="succeeded" && restarted.reused==1 && restarted.saved==1);
        fs::remove_all(root);std::cout<<"Batch workflows, snapshot, restart, invalidation, bounds, CSV and cancellation passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
