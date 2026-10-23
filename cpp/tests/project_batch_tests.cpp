#include "qupathxx/project_batch.hpp"
#include "qupathxx/input_digest.hpp"
#include "qupathxx/pixel_stream_file.hpp"
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
        PixelFeatureOptions features;features.sigmas={0,1};std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:pixel_feature_schema(features,t)) schema.emplace_back(f.name,f.unit);
        ProjectBatchOptions pixels;pixels.kind=ProjectBatchKind::Pixels;pixels.pixel_model=PixelClassifier(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),features,t,PixelResolution{});pixels.pixels.block_size=16;pixels.pixels.prediction.tile_size=16;pixels.pixels.prediction.work_bytes=8*1024*1024;
        const ProjectBatchRequest pixel_request(p,t,{},pixels);const auto prior=doc->revision();auto pixel_saved=run_project_batch(pixel_request,(root/"pixels").u8string());
        if(pixel_saved.state!="succeeded") throw std::runtime_error(pixel_saved.error+(pixel_saved.images.empty()?"":pixel_saved.images[0].error));
        check(pixel_saved.saved==1 && pixel_saved.images[0].records==1 && pixel_saved.images[0].inferred_blocks>0 && !pixel_saved.images[0].hashed_blocks && doc->revision()==prior && !doc->pixel_stream_record_count());
        const auto pixel_path=pixel_saved.images[0].path;const auto pixel_project=Project::open(pixel_path);const auto links=pixel_project->document(image)->pixel_stream_records();check(links.size()==1);
        PixelStreamFile pixel_file(links[0]->link->path());pixel_file.verify();check(pixel_file.link()->same_content(*links[0]->link));
        const auto reused=run_project_batch(pixel_request,(root/"pixels").u8string());check(reused.state=="succeeded" && reused.reused==1 && reused.images[0].path==pixel_path && reused.images[0].inferred_blocks==0 && reused.images[0].hashed_blocks==pixel_saved.images[0].inferred_blocks && reused.images[0].verified_blocks==reused.images[0].hashed_blocks);
        check(batch_checkpoint_csv(pixel_path).find("class_pixels")!=std::string::npos);
        {std::fstream file(fs::u8path(links[0]->link->path()),std::ios::binary|std::ios::in|std::ios::out);file.seekp(-1,std::ios::end);file.put('X');}
        check(run_project_batch(pixel_request,(root/"pixels").u8string()).state=="failed");
        pixels.resume=false;check(run_project_batch(ProjectBatchRequest(p,t,{},pixels),(root/"pixels").u8string()).saved==1);
        pixels.pixel_total_limit=4096;const auto limited=run_project_batch(ProjectBatchRequest(p,t,{},pixels),(root/"pixel-limit").u8string());check(limited.state=="failed");
        std::size_t retained=0;for(const auto& e:fs::recursive_directory_iterator(root/"pixel-limit")) if(e.path().extension()==".qps" || e.path().filename()=="result.qpxproj") ++retained;check(retained==0);
        fs::remove_all(root);std::cout<<"Batch workflows, snapshot, restart, invalidation, bounds, CSV and cancellation passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
