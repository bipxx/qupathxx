#include "qupathxx/raw_image.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_batch.hpp"
#include "qupathxx/raw_tables.hpp"
#include "qupathxx/tma.hpp"
#include "../raw_analysis_internal.hpp"
#include "qupathxx/core.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("raw image check failed");}
template<class F> void rejects(F&& f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing raw rejection");}
int main() {
    namespace fs=std::filesystem;const auto path=fs::current_path()/("raw-test-"+std::to_string(std::random_device{}())+".tif");
    try {
        RawPixelBuffer owned(2,2,1,SampleType::UInt16);check(owned.payload_bytes()==12 && std::get<std::vector<std::uint16_t>>(owned.storage()).size()==4);
        rejects([] {RawPixelBuffer invalid(0,2,1,SampleType::UInt8);});rejects([] {RawPixelBuffer invalid(100000,100000,3,SampleType::Float64);});
        TiffView view;view.channels={0,0,0};view.lower={0,0,0};view.upper={65535,65535,65535};check(tiff_view_decode(tiff_view_encode(view)).upper==view.upper);
        auto corrupt=tiff_view_encode(view);corrupt.back()^=1;rejects([&]{tiff_view_decode(corrupt);});
        if(has_tiff()) {
            // Independent minimal TIFF byte fixture: no libtiff encoder involved.
            std::vector<unsigned char> bytes={'I','I',42,0,8,0,0,0};
            auto u16=[&](std::uint16_t n){bytes.push_back(n&255);bytes.push_back(n>>8);};auto u32=[&](std::uint32_t n){for(int i=0;i<4;++i) bytes.push_back((n>>(8*i))&255);};
            u16(10);auto tag=[&](std::uint16_t id,std::uint16_t type,std::uint32_t n){u16(id);u16(type);u32(1);u32(n);};
            tag(256,4,2);tag(257,4,2);tag(258,3,16);tag(259,3,1);tag(262,3,1);tag(273,4,134);tag(277,3,1);tag(278,4,2);tag(279,4,8);tag(339,3,1);u32(0);
            for(auto value:{1,256,40000,65535}) u16(static_cast<std::uint16_t>(value));
            std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();
            auto server=open_tiff(path.u8string());const auto metadata=server->raw_metadata();check(metadata && metadata->sample_type==SampleType::UInt16 && metadata->channels==1);
            RawRegionRequest request;request.region=RegionRequest(0,0,0,2,2);const auto raw=server->read_raw(request);check(std::get<std::vector<std::uint16_t>>(raw.storage())==std::vector<std::uint16_t>({1,256,40000,65535}));
            const auto display=server->read_region(request.region);check(display.bytes()[0]==0 && display.bytes()[4]==1 && display.bytes()[8]==156 && display.bytes()[12]==255);
            const auto statistics=raw_region_statistics(*server,request);check(statistics.valid_pixels==4 && statistics.finite_samples[0]==4 && statistics.means[0]==26448.);
            request.region=RegionRequest(-1,-1,0,4,4);const auto padded=server->read_raw(request);check(std::count(padded.valid().begin(),padded.valid().end(),1)==4);
            check(server->tiff_read_stats()->decoded_blocks==1 && server->tiff_read_stats()->cache_hits>=3);
            {
                Document document(server);const auto object=document.add_rectangle({0,0,2,2});
                RawRegionRequest selection;selection.region=RegionRequest(0,0,0,1,1);
                RawAnalysisRequest capture(document,selection,object);
                const auto result=analyze_raw(capture);check(result->statistics().valid_pixels==4 && result->statistics().means[0]==26448.);
                check(result->input_sha256().size()==64 && result->metadata().sample_type==SampleType::UInt16);
                const auto digest=raw_analysis_input_digest(capture);check(digest.input_sha256==result->input_sha256() && digest.tiles==result->tiles() && digest.raw_bytes==12);
                const auto retained=document.keep_raw_analysis(capture,result);check(document.raw_analysis_status(retained)=="current" && document.raw_analysis_record_count()==1);
                auto display_view=*server->tiff_view();display_view.upper={40000,40000,40000};document.set_tiff_view(display_view);
                check(capture.matches(document) && document.raw_analysis_status(retained)=="current");const auto repeated=analyze_raw(RawAnalysisRequest(document,selection,object));
                check(repeated->input_sha256()==result->input_sha256() && repeated->statistics().means==result->statistics().means);
                const auto replay=RawAnalysisRequest::replay(document,retained);const auto replayed=analyze_raw(replay);const auto revision=document.revision();
                check(document.verify_raw_analysis(retained,replay,replayed) && document.revision()==revision);
                Document copied=document;check(!capture.matches(copied) && copied.raw_analysis_status(retained)=="unverified");
                check(copied.remove_raw_analysis(retained) && copied.raw_analysis_record_count()==0 && document.raw_analysis_record_count()==1);
                RawAnalysisJob job;check(job.request(replay));check(job.wait(document).state=="ready");const auto second=job.keep_result(document);
                check(second!=retained && document.raw_analysis_record_count()==2 && document.raw_analysis_page(0,1).front()->id==retained);
                check(document.latest_raw_analysis(object)->id==second);
                check(job.request(replay));job.cancel();check(job.wait(document).state=="cancelled");
                std::atomic<bool> cancelled{true};rejects([&]{analyze_raw(capture,&cancelled);});
                {
                    const auto held=document.raw_analysis_record(retained);const auto encoded=RawAnalysisCodec::encode(*held,document.metadata());
                    const auto decoded=RawAnalysisCodec::decode(encoded.data(),encoded.size(),document.metadata());
                    check(decoded->result->input_sha256()==result->input_sha256() && decoded->result->statistics().means==result->statistics().means);
                    check(RawAnalysisCodec::encode(*decoded,document.metadata())==encoded);
                    auto corrupt=encoded;corrupt.back()^=1;rejects([&]{RawAnalysisCodec::decode(corrupt.data(),corrupt.size(),document.metadata());});
                    if(has_project()) {
                        const auto project_path=fs::path(path.u8string()+".qpxproj");
                        {
                            Project project;const auto image=project.add_document(std::make_shared<Document>(document),"Raw archive",path.u8string(),"tiff");project.save(project_path.u8string());
                            const auto loaded=Project::open(project_path.u8string());const auto restored=loaded->document(image);
                            check(restored->raw_analysis_record_count()==2 && restored->raw_analysis_status(retained)=="unverified");
                            const auto replayed=RawAnalysisRequest::replay(*restored,retained);const auto measured=analyze_raw(replayed);const auto before=restored->revision();
                            check(restored->verify_raw_analysis(retained,replayed,measured) && restored->raw_analysis_status(retained)=="current" && restored->revision()==before);
                        }
                        check(project_path.parent_path()==fs::current_path());fs::remove(project_path);
                    }
                }
                {
                    Document detached(open_tiff(path.u8string()));const auto area=detached.add_rectangle({0,0,2,2});
                    std::weak_ptr<ImageServer> previous=detached.server();RawAnalysisJob detached_job;
                    check(detached_job.request(RawAnalysisRequest(detached,selection,area)));check(detached_job.wait(detached).state=="ready");
                    auto view=*detached.server()->tiff_view();view.upper={40000,40000,40000};detached.set_tiff_view(view);
                    check(previous.expired() && detached_job.poll(detached).state=="ready");
                    check(detached_job.keep_result(detached)>0);
                }
                {
                    RawTableRequest table(document,{retained,retained},true);check(table.records()==1 && table.rows()==1);
                    const auto csv=raw_analysis_csv(table);check(csv.find("qpx.raw-analysis-csv/1")!=std::string::npos && csv.find(result->input_sha256())!=std::string::npos);
                    const auto csv_path=fs::path(path.u8string()+".raw.csv");const auto bytes=save_raw_analysis_csv(table,csv_path.u8string());check(bytes==csv.size());
                    rejects([&]{save_raw_analysis_csv(table,csv_path.u8string());});rejects([&]{save_raw_analysis_csv(table,path.u8string(),true);});
                    check(csv_path.parent_path()==fs::current_path());fs::remove(csv_path);
                    RawTableJob writer;check(writer.request(table,csv_path.u8string()));const auto saved=writer.wait();check(saved.state=="saved" && saved.completed_rows==1 && saved.bytes==csv.size());
                    check(csv_path.parent_path()==fs::current_path());fs::remove(csv_path);
                    std::atomic<bool> cancelled{true};rejects([&]{save_raw_analysis_csv(table,csv_path.u8string(),false,1024*1024,&cancelled);});check(!fs::exists(csv_path));
                }
                {
                    Document tissue(open_tiff(path.u8string()));TmaGridOptions grid;grid.rows=grid.columns=1;grid.origin={1,1};grid.width=grid.height=2;
                    const auto core=tissue.create_tma_grid(grid).front();tissue.set_tma_case_ids({{core,"patient-A"}});
                    const auto child=tissue.add_rectangle({.5,.5,1,1},"Child");tissue.reparent_annotation(child,core);
                    RawAnalysisRequest selected(tissue,selection,child);const auto measured=analyze_raw(selected);
                    check(measured->object_context().tma && measured->object_context().tma->case_id=="patient-A");
                    const auto id=tissue.keep_raw_analysis(selected,measured);RawTableRequest frozen(tissue,{id},true);
                    tissue.set_tma_case_ids({{core,"patient-B"}});check(!selected.matches(tissue) && tissue.raw_analysis_status(id)=="context_changed");
                    check(measured->object_context().tma->case_id=="patient-A" && raw_analysis_csv(frozen).find("patient-B")==std::string::npos);
                    rejects([&]{RawTableRequest invalid(tissue,{id},true);});rejects([&]{RawAnalysisRequest::replay(tissue,id);});
                    const auto current=analyze_raw(RawAnalysisRequest(tissue,selection,child));check(current->input_sha256()==measured->input_sha256());
                    const auto encoded=RawAnalysisCodec::encode(*tissue.raw_analysis_record(id),tissue.metadata());const auto restored=RawAnalysisCodec::decode(encoded.data(),encoded.size(),tissue.metadata());
                    check(restored->result->object_context().tma->case_id=="patient-A");
                }
                {
                    Document nested(open_tiff(path.u8string()));TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={.9,1};grid.column_step={.2,0};grid.width=grid.height=1.5;
                    const auto cores=nested.create_tma_grid(grid);nested.set_tma_case_ids({{cores[0],"A"},{cores[1],"B"}});
                    const auto parent=nested.add_rectangle({.7,.7,.4,.4},"Parent"),child=nested.add_rectangle({.8,.8,.2,.2},"Child");
                    nested.reparent_annotation(parent,cores[0]);nested.reparent_annotation(child,parent);
                    RawAnalysisRequest original(nested,selection,child);const auto result=analyze_raw(original);const auto id=nested.keep_raw_analysis(original,result);
                    const auto child_revision=nested.annotation(child).revision;RawTableRequest frozen(nested,{id},true);
                    nested.reparent_annotation(parent,cores[1]);check(nested.annotation(child).revision==child_revision);
                    check(!original.matches(nested) && nested.raw_analysis_status(id)=="context_changed");
                    rejects([&]{RawAnalysisRequest::replay(nested,id);});rejects([&]{RawTableRequest invalid(nested,{id},true);});
                    check(analyze_raw(RawAnalysisRequest(nested,selection,child))->input_sha256()==result->input_sha256());
                    nested.reparent_annotation(parent,0);check(!original.matches(nested) && nested.raw_analysis_status(id)=="context_changed");
                    RawAnalysisRequest unscoped(nested,selection,child);const auto none=nested.keep_raw_analysis(unscoped,analyze_raw(unscoped));
                    nested.reparent_annotation(parent,cores[0]);check(!unscoped.matches(nested) && nested.raw_analysis_status(none)=="context_changed");
                    check(original.matches(nested) && nested.raw_analysis_status(id)=="current");
                    check(raw_analysis_csv(frozen).find("qpx.raw-analysis-csv/1")!=std::string::npos);
                }
                document.remove_annotation(object);check(!capture.matches(document) && document.raw_analysis_status(retained)=="object_changed");
            }
            if(has_project()) {
                Project project;const auto image=project.add_tiff_image(path.u8string(),TiffOpenOptions{},"Batch raw");const auto doc=project.document(image);doc->add_rectangle({0,0,2,2},"original");
                ProjectBatchOptions options;options.kind=ProjectBatchKind::Raw;options.roi_tile_size=16;
                const StainTransform ignored({{1,0,0},{0,1,0}});const ProjectBatchRequest captured(project,ignored,{},options);
                doc->rename_annotation(1,"edited after capture");const auto output=fs::path(path.u8string()+".batch");
                const auto first=run_project_batch(captured,output.u8string());if(first.state!="succeeded") throw std::runtime_error(first.error+(first.images.empty()?"":first.images[0].error));
                check(first.saved==1 && first.images[0].raw_tiles==1 && first.images[0].raw_hashed_tiles==0 && doc->raw_analysis_record_count()==0);
                const auto result=Project::open(first.images[0].path)->document(image);check(result->raw_analysis_record_count()==1);
                const auto record=result->raw_analysis_records().front();check(record->result->statistics().means[0]==26448. && record->result->object_context().name=="original");
                check(batch_checkpoint_csv(first.images[0].path).find("qpx.raw-analysis-csv/1")!=std::string::npos);
                const auto resumed=run_project_batch(captured,output.u8string());if(resumed.state!="succeeded") throw std::runtime_error(resumed.error+(resumed.images.empty()?"":resumed.images[0].error));
                check(resumed.reused==1 && resumed.images[0].raw_tiles==0 && resumed.images[0].raw_hashed_tiles==1 && resumed.images[0].path==first.images[0].path);
                check(output.parent_path()==fs::current_path());fs::remove_all(output);
            }
            auto alternate=*server->tiff_view();alternate.upper={40000,40000,40000};auto projected=server->with_tiff_view(alternate);
            check(projected->read_region(RegionRequest(0,0,0,2,2)).bytes()[8]==255);
            check(projected->tiff_read_stats()->decoded_blocks==1 && server->tiff_view()->upper[0]==65535);
            server->close();check(server->closed());
            check(std::get<std::vector<std::uint16_t>>(projected->read_raw(request).storage())[10]==65535);
            check(projected->tiff_read_stats()->decoded_blocks==1);projected->close();rejects([&]{server->read_raw(request);});check(std::get<std::vector<std::uint16_t>>(raw.storage())[2]==40000);
            check(path.parent_path()==fs::current_path());fs::remove(path);
        } else rejects([&]{open_tiff(path.u8string());});
        std::cout<<"raw quantitative buffers, TIFF decode/cache, preview and statistics passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
