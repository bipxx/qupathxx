#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
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
// Independent TIFF strips, including explicit planar channel storage. This
// exercises physical decode reuse without depending on an encoder or the LRU.
void multichannel_block_reuse(const std::filesystem::path& path) {
    for(bool separate:{false,true}) for(std::size_t cache:{std::size_t(0),std::size_t(1)}) {
        std::vector<unsigned char> bytes={'I','I',42,0,8,0,0,0};
        auto u16=[&](std::uint16_t n){bytes.push_back(n&255);bytes.push_back(n>>8);};
        auto u32=[&](std::uint32_t n){for(unsigned i=0;i<4;++i) bytes.push_back(std::uint8_t(n>>(8*i)));};
        auto tag=[&](std::uint16_t id,std::uint16_t type,std::uint32_t count,std::uint32_t value){u16(id);u16(type);u32(count);u32(value);};
        const auto strips=separate?6u:2u,block_bytes=separate?4u:12u,pixels=140u+8u*strips;
        u16(10);tag(256,4,1,2);tag(257,4,1,2);tag(258,3,3,134);tag(259,3,1,1);tag(262,3,1,2);
        tag(273,4,strips,140);tag(277,3,1,3);tag(278,4,1,1);tag(279,4,strips,140+4*strips);tag(284,3,1,separate?2:1);u32(0);
        check(bytes.size()==134);u16(16);u16(16);u16(16);
        for(unsigned i=0;i<strips;++i) u32(pixels+i*block_bytes);
        for(unsigned i=0;i<strips;++i) u32(block_bytes);
        if(separate) {for(unsigned c=0;c<3;++c) for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) u16(std::uint16_t(100*c+2*y+x+1));}
        else {for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) for(unsigned c=0;c<3;++c) u16(std::uint16_t(100*c+2*y+x+1));}
        std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();
        TiffOpenOptions options;options.cache_bytes=cache;auto server=open_tiff(path.u8string(),options);
        RawRegionRequest request;request.region={-1,-1,0,4,4};request.first_channel=1;request.channels=2;
        const auto raw=server->read_raw(request);const auto& values=std::get<std::vector<std::uint16_t>>(raw.storage());
        for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
            const bool inside=x>=1 && x<=2 && y>=1 && y<=2;check(raw.valid()[y*4+x]==inside);
            for(unsigned c=0;c<2;++c) check(values[(y*4+x)*2+c]==(inside?100*(c+1)+2*(y-1)+(x-1)+1:0));
        }
        check(server->tiff_read_stats()->decoded_blocks==(separate?4u:2u));
        check(server->tiff_read_stats()->peak_cache_bytes<=cache);server->close();std::filesystem::remove(path);
    }
}
// Two independent hand-encoded TIFFs deliberately share IFD and strip numbers.
// Distinct channel values expose a cache key that forgets the physical file.
void multifile_dispatch(const std::filesystem::path& path) {
    namespace fs=std::filesystem;auto companion=path;companion += ".other.tif";
    const auto write=[&](const fs::path& target,bool other) {
        const auto xml=std::string("<OME UUID=\"")+(other?"urn:other":"urn:first")+"\"><Image><Pixels DimensionOrder=\"XYZCT\" Type=\"uint16\" SizeX=\"2\" SizeY=\"2\" SizeC=\"2\" SizeZ=\"1\" SizeT=\"1\"><Channel Name=\"A\"/><Channel Name=\"B\"/>"
            "<TiffData IFD=\"0\" FirstC=\"0\" PlaneCount=\"1\"><UUID FileName=\""+path.filename().u8string()+"\">urn:first</UUID></TiffData>"
            "<TiffData IFD=\"0\" FirstC=\"1\" PlaneCount=\"1\"><UUID FileName=\""+companion.filename().u8string()+"\">urn:other</UUID></TiffData></Pixels></Image></OME>";
        std::vector<unsigned char> bytes={'I','I',42,0,8,0,0,0};
        auto u16=[&](std::uint16_t n){bytes.push_back(n&255);bytes.push_back(n>>8);};
        auto u32=[&](std::uint32_t n){for(unsigned i=0;i<4;++i) bytes.push_back(std::uint8_t(n>>(8*i)));};
        auto tag=[&](std::uint16_t id,std::uint16_t type,std::uint32_t count,std::uint32_t value){u16(id);u16(type);u32(count);u32(value);};
        u16(11);tag(256,4,1,2);tag(257,4,1,2);tag(258,3,1,16);tag(259,3,1,1);tag(262,3,1,1);
        tag(270,2,std::uint32_t(xml.size()+1),146);tag(273,4,1,std::uint32_t(147+xml.size()));tag(277,3,1,1);tag(278,4,1,2);tag(279,4,1,8);tag(284,3,1,1);u32(0);
        check(bytes.size()==146);bytes.insert(bytes.end(),xml.begin(),xml.end());bytes.push_back(0);for(unsigned i=1;i<=4;++i) u16(std::uint16_t(i+(other?100:0)));
        std::ofstream file(target,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();check(bool(file));
    };
    write(path,false);write(companion,true);
    for(const auto& entry:{path,companion}) {
        auto server=open_tiff(entry.u8string(),{});check(server->reader_source()->scope=="compound_files" && server->reader_source()->companions.size()==1);
        RawRegionRequest request;request.region={0,0,0,2,2};request.channels=2;
        for(unsigned repeat=0;repeat<2;++repeat) {const auto raw=server->read_raw(request);const auto& values=std::get<std::vector<std::uint16_t>>(raw.storage());for(unsigned i=0;i<4;++i) {check(values[2*i]==i+1 && values[2*i+1]==i+101);check(raw.valid()[i]);}}
        check(server->tiff_read_stats()->decoded_blocks==2 && server->tiff_read_stats()->cache_hits==2);
        auto view=server->with_tiff_view(*server->tiff_view());check(view->reader_source()->companions.size()==1);server->close();check(view->read_raw(request).valid()[0]);view->close();
    }
    auto server=open_tiff(path.u8string(),{});RawRegionRequest request;request.region={0,0,0,2,2};request.channels=2;auto retained=server->read_raw(request);
    // Append changes only the companion's observed version, not the primary.
    {std::ofstream file(companion,std::ios::binary|std::ios::app);file.put('x');}
    rejects([&]{server->read_raw(request);});request.region={-10,-10,0,2,2};rejects([&]{server->read_raw(request);});
    rejects([&]{server->read_region({0,0,0,2,2});});rejects([&]{server->with_tiff_view(*server->tiff_view());});
    server->close();check(std::get<std::vector<std::uint16_t>>(retained.storage())[1]==101);
    fs::remove(companion);rejects([&]{open_tiff(path.u8string(),{});});fs::remove(path);
}
int main() {
    namespace fs=std::filesystem;const auto path=fs::current_path()/("raw-test-"+std::to_string(std::random_device{}())+".tif");
    try {
        RawPixelBuffer owned(2,2,1,SampleType::UInt16);check(owned.payload_bytes()==12 && std::get<std::vector<std::uint16_t>>(owned.storage()).size()==4);
        rejects([] {RawPixelBuffer invalid(0,2,1,SampleType::UInt8);});rejects([] {RawPixelBuffer invalid(100000,100000,3,SampleType::Float64);});
        TiffView view;view.channels={0,0,0};view.lower={0,0,0};view.upper={65535,65535,65535};check(tiff_view_decode(tiff_view_encode(view)).upper==view.upper);
        auto corrupt=tiff_view_encode(view);corrupt.back()^=1;rejects([&]{tiff_view_decode(corrupt);});
        if(has_tiff()) {
            multifile_dispatch(path);
            std::cout<<"TIFF multi-file UUID dispatch, colliding cache keys, view ownership and companion mutation guards passed\n";
            multichannel_block_reuse(path);
            std::cout<<"TIFF interleaved block reuse without LRU, separate-plane selection and padded values passed\n";
            // Independent minimal TIFF byte fixture: no libtiff encoder involved.
            std::vector<unsigned char> bytes={'I','I',42,0,8,0,0,0};
            auto u16=[&](std::uint16_t n){bytes.push_back(n&255);bytes.push_back(n>>8);};auto u32=[&](std::uint32_t n){for(int i=0;i<4;++i) bytes.push_back((n>>(8*i))&255);};
            u16(10);auto tag=[&](std::uint16_t id,std::uint16_t type,std::uint32_t n){u16(id);u16(type);u32(1);u32(n);};
            tag(256,4,2);tag(257,4,2);tag(258,3,16);tag(259,3,1);tag(262,3,1);tag(273,4,134);tag(277,3,1);tag(278,4,2);tag(279,4,8);tag(339,3,1);u32(0);
            for(auto value:{1,256,40000,65535}) u16(static_cast<std::uint16_t>(value));
            std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();
            {
                // Fixed RFC1950 stream independently produced from the eight
                // little-endian source bytes, not libtiff's own encoder.
                auto compressed=bytes;compressed.resize(134);
                const unsigned char stream[]={0x78,0x9c,0x63,0x64,0x60,0x60,0x74,0x98,0xf3,0xff,0x3f,0x00,0x05,0xe6,0x02,0xdd};
                for(std::size_t at=10;at<130;at+=12) {
                    const auto id=unsigned(compressed[at])+(unsigned(compressed[at+1])<<8);
                    if(id==259 || id==279) {const auto value=id==259?8u:unsigned(sizeof(stream));for(unsigned i=0;i<4;++i) compressed[at+8+i]=std::uint8_t(value>>(8*i));}
                }
                compressed.insert(compressed.end(),std::begin(stream),std::end(stream));
                const auto compressed_path=fs::path(path.u8string()+".deflate.tif");
                std::ofstream compressed_file(compressed_path,std::ios::binary);compressed_file.write(reinterpret_cast<const char*>(compressed.data()),compressed.size());compressed_file.close();
                auto decoded=open_tiff(compressed_path.u8string());Document doc(decoded);
                const auto result=analyze_raw(RawAnalysisRequest(doc,ScientificGrid(2,2,{0,2,1},{0,2,1})));
                check(result->statistics().means[0]==26448. && result->statistics().valid_pixels==1);
                decoded->close();fs::remove(compressed_path);
                std::cout<<"Independent deflate TIFF typed grid analysis through prefixed zlib passed\n";
            }
            auto server=open_tiff(path.u8string());const auto metadata=server->raw_metadata();check(metadata && metadata->sample_type==SampleType::UInt16 && metadata->channels==1);
            RawRegionRequest request;request.region=RegionRequest(0,0,0,2,2);const auto raw=server->read_raw(request);check(std::get<std::vector<std::uint16_t>>(raw.storage())==std::vector<std::uint16_t>({1,256,40000,65535}));
            {
                Document grid_document(server);const ScientificGrid grid(2,2,{0,2,1},{0,2,1});
                RawAnalysisRequest captured(grid_document,grid);const auto result=analyze_raw(captured);
                check(result->grid() && result->statistics().pixels==1 && result->statistics().valid_pixels==1 && result->statistics().means[0]==26448.);
                check(result->grid()->source_metadata.sample_type==SampleType::UInt16 && raw_analysis_input_digest(captured).input_sha256==result->input_sha256());
                const auto object=grid_document.add_rectangle({0,0,1,2},"left");
                const ScientificGrid fine(2,2,{0,2,4,1},{0,2,4,1});
                const auto fine_result=analyze_raw(RawAnalysisRequest(grid_document,fine,{},object));
                check(fine_result->statistics().pixels==8 && fine_result->statistics().valid_pixels==8 && fine_result->statistics().means[0]==20000.5);
                const auto retained=grid_document.keep_raw_analysis(captured,result);
                RawAnalysisRequest fine_request(grid_document,fine,{},object);
                const auto second=grid_document.keep_raw_analysis(fine_request,analyze_raw(fine_request));
                check(raw_analysis_csv(RawTableRequest(grid_document,{retained,second},true)).find("qpx.raw-analysis-csv/2")!=std::string::npos);
                if(has_project()) {
                    const auto saved=fs::path(path.u8string()+".grid.qpxproj");
                    Project project;const auto image=project.add_document(std::make_shared<Document>(grid_document),"Grid raw records");project.save(saved.u8string());
                    const auto loaded=Project::open(saved.u8string());const auto reopened=loaded->document(image);
                    check(reopened->raw_analysis_record_count()==2 && reopened->raw_analysis_status(retained)=="unverified");
                    const auto replay=RawAnalysisRequest::replay(*reopened,retained);
                    check(reopened->verify_raw_analysis(retained,replay,analyze_raw(replay)) && reopened->raw_analysis_status(retained)=="current");
                    loaded->save(saved.u8string());const auto again=Project::open(saved.u8string())->document(image);
                    check(again->raw_analysis_record_count()==2 && again->raw_analysis_record(second)->result->grid());
                    fs::remove(saved);
                    std::cout<<"QRA2 TIFF grid SQLite save/reopen, explicit replay and second save passed\n";
                }
                std::cout<<"Actual UInt16 TIFF scientific grid analysis, ROI upsampling and hash-only digest passed\n";
            }
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
                            Project project;const auto image=project.add_document(std::make_shared<Document>(document),"Raw archive");
                            check(!project.images().front().embedded && project.images().front().reader=="tiff");project.save(project_path.u8string());
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
            if(has_project()) {
                Project project;const auto first_image=project.add_tiff_image(path.u8string(),TiffOpenOptions{},"Grid A");
                const auto second_image=project.add_tiff_image(path.u8string(),TiffOpenOptions{},"Grid B");
                project.document(first_image)->add_rectangle({0,0,2,2},"whole");project.document(second_image)->add_rectangle({0,0,2,2},"whole");
                ProjectBatchOptions options;options.kind=ProjectBatchKind::Raw;options.raw_grid=RawGridSampling{};
                options.raw_grid->samples_x=1;options.raw_grid->samples_y=1;options.raw_grid->context_x=1;
                const StainTransform ignored({{1,0,0},{0,1,0}});const ProjectBatchRequest captured(project,ignored,{},options);
                check(captured.parameters_json().find("qpx.raw-grid-statistics-input/1")!=std::string::npos);
                ProjectBatchOptions natural=options;natural.raw_grid.reset();const ProjectBatchRequest old(project,ignored,{},natural);
                check(old.parameters_sha256()!=captured.parameters_sha256() && old.parameters_json().find("raw_grid")==std::string::npos);
                auto invalid=options;invalid.raw.region.level=1;rejects([&]{invalid.validate();});invalid=options;invalid.roi_tile_size=16;rejects([&]{invalid.validate();});
                const auto folder=fs::path(path.u8string()+".grid-batch");std::atomic<bool> cancelled{false};
                const auto interrupted=run_project_batch(captured,folder.u8string(),&cancelled,[&](const auto& item){if(item.state=="saved") cancelled.store(true);});
                check(interrupted.state=="cancelled" && interrupted.saved==1);
                cancelled.store(false);const auto resumed=run_project_batch(captured,folder.u8string(),&cancelled);
                if(resumed.state!="succeeded") throw std::runtime_error(resumed.error+(resumed.images.empty()?"":resumed.images[0].error));
                check(resumed.reused==1 && resumed.saved==1 && resumed.images[0].raw_tiles==0 && resumed.images[0].raw_hashed_tiles==1);
                const auto repeated=run_project_batch(captured,folder.u8string());check(repeated.reused==2 && repeated.saved==0);
                for(const auto& image:repeated.images) {
                    check(image.raw_tiles==0 && image.raw_hashed_tiles==1 && batch_checkpoint_csv(image.path).find("qpx.raw-analysis-csv/2")!=std::string::npos);
                    const auto loaded=Project::open(image.path);const auto doc=loaded->document(image.image_id);const auto record=doc->raw_analysis_records().back();
                    check(record->result->grid() && record->result->statistics().means[0]==26448. && record->result->grid()->grid.x().context()==1);
                }
                check(project.document(first_image)->raw_analysis_record_count()==0 && project.document(second_image)->raw_analysis_record_count()==0);
                check(folder.parent_path()==fs::current_path());fs::remove_all(folder);
                std::cout<<"Raw grid TIFF batch distinct recipe, interrupted checkpoint resume and hash-only typed CSV passed\n";
            }
            auto alternate=*server->tiff_view();alternate.upper={40000,40000,40000};auto projected=server->with_tiff_view(alternate);
            check(projected->read_region(RegionRequest(0,0,0,2,2)).bytes()[8]==255);
            check(projected->tiff_read_stats()->decoded_blocks==1 && server->tiff_view()->upper[0]==65535);
            server->close();check(server->closed());
            check(std::get<std::vector<std::uint16_t>>(projected->read_raw(request).storage())[10]==65535);
            check(projected->tiff_read_stats()->decoded_blocks==1);projected->close();rejects([&]{server->read_raw(request);});check(std::get<std::vector<std::uint16_t>>(raw.storage())[2]==40000);
            check(path.parent_path()==fs::current_path());fs::remove(path);
        } else {rejects([&]{open_tiff(path.u8string());});std::cout<<"TIFF multi-file reader disabled; open refused\n";}
        std::cout<<"raw quantitative buffers, TIFF decode/cache, preview and statistics passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
