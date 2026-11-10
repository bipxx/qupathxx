#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/fluorescence_tiled_workflow.hpp"
#include "qupathxx/fluorescence_tiled_checkpoint.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_fixture.hpp"
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>
#include <map>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void near(double a,double b){check(std::isfinite(a) && std::abs(a-b)<1e-9,"Source-bound fluorescence differs from independent expected value");}
template<class E=std::exception,class F> void rejects(F action){try{action();}catch(const E&){return;}throw std::runtime_error("Expected fluorescence workflow refusal");}
class Reader final:public ImageServer {
public:
    std::shared_ptr<RawImageMetadata> raw=std::make_shared<RawImageMetadata>();
    std::shared_ptr<TiffView> view=std::make_shared<TiffView>();
    std::shared_ptr<const ReaderSource> observed=make_reader_source("fluorescence-reference","","memory");
    ImageMetadata display;
    mutable std::atomic<unsigned> reads{0},display_reads{0};
    mutable std::atomic<std::uint64_t> max_read_pixels{0};
    mutable std::atomic<bool> entered{false},finished{false};
    std::atomic<bool> block{false},release{false},is_closed{false};
    bool wrong_shape=false,decoder_failure=false,uniform=false;
    int dna_x=3,dna_y=3;unsigned switch_uniform_after=0;
    Reader(){raw->width=15;raw->height=9;raw->channels=3;raw->z_planes=2;raw->timepoints=2;raw->sample_type=SampleType::Float64;
        raw->levels={{15,9,1}};raw->channel_names={"Unused","DNA","Marker"};raw->dimension_order="XYZCT";raw->pixel_width_um=.5;raw->pixel_height_um=1;
        display.width=15;display.height=9;display.levels={{15,9,1}};display.pixel_width_um=.5;display.pixel_height_um=1;view->z=1;view->t=1;}
    const ImageMetadata& metadata() const override{return display;}
    std::string reader_id() const override{return "fluorescence-reference";}
    std::shared_ptr<const ReaderSource> reader_source() const override{return observed;}
    std::shared_ptr<const TiffView> tiff_view() const override{return view;}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>*) const override{return raw;}
    PixelBuffer read_region(const RegionRequest&) const override{++display_reads;throw std::runtime_error("RGB display reads forbidden");}
    RawPixelBuffer read_raw(const RawRegionRequest& r,const std::atomic<bool>* cancelled) const override {
        const auto read_number=++reads;max_read_pixels.store(std::max(max_read_pixels.load(),std::uint64_t(r.region.width*r.region.height)));entered.store(true);
        struct Finished {std::atomic<bool>& flag;~Finished(){flag.store(true);}} finished_scope{finished};
        while(block.load() && !release.load()){workflow_checkpoint(cancelled);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        workflow_checkpoint(cancelled);if(decoder_failure)throw std::runtime_error("Independent decoder failure");
        RawPixelBuffer out(r.region.width+(wrong_shape?1:0),r.region.height,r.channels,SampleType::Float64);
        auto& values=std::get<std::vector<double>>(out.writable_storage());std::fill(out.writable_valid().begin(),out.writable_valid().end(),1);
        for(std::int64_t y=0;y<r.region.height;++y)for(std::int64_t x=0;x<r.region.width;++x){const auto px=x+r.region.x,py=y+r.region.y;
            for(std::uint32_t c=0;c<r.channels;++c){const auto channel=r.first_channel+c;
                values[(std::size_t(y)*std::size_t(out.width())+std::size_t(x))*r.channels+c]=channel==0?1000+px:channel==1?(uniform || (switch_uniform_after && read_number>switch_uniform_after) || (px>=dna_x && px<=dna_x+2 && py>=dna_y && py<=dna_y+2)?100:0):double(px+10*py+1000*r.z+10000*r.t);}}
        return out;
    }
    void close() override{is_closed.store(true);}
    bool closed() const override{return is_closed.load();}
};
RawRegionRequest selected(){RawRegionRequest r;r.region={2,2,0,5,5};r.first_channel=1;r.channels=2;r.z=1;r.t=1;return r;}
FluorescenceCellOptions parameters(){FluorescenceCellOptions p;p.threshold=50;p.cell_expansion=1.5;return p;}
void source_contract() {
    auto reader=std::make_shared<Reader>();Document doc(reader);const auto id=doc.add_rectangle({2,2,5,5});
    const FluorescenceCellRequest request(doc,selected(),id,parameters(),{2,2});check(!reader->reads && !reader->display_reads,"Capture never decodes pixels");
    check(request.region().region.x==0 && request.region().region.width==9 && request.core_region().region.x==2 && request.core_region().region.width==5,"Explicit core/context geometry");
    check(request.estimated_work_bytes()<=parameters().work_bytes && request.object_id()==id,"Bounded native source request");
    const auto run=detect_fluorescence_cells(request);check(reader->reads==1 && !reader->display_reads,"One original raw read; no RGB or input-rescan fallback");
    check(run->raster().measurements.size()==1 && run->raster().measurements[0].nucleus.pixels==9 && run->raster().measurements[0].cell.pixels==25,"Analytical raw nucleus/body masks through source workflow");
    near(*run->raster().measurements[0].nucleus.channels[1].mean,11044);near(*run->raster().measurements[0].cell.channels[0].sum,900);
    check(run->region().first_channel==1 && run->region().channels==2 && run->region().z==1 && run->region().t==1 && run->source_metadata().channel_names[1]=="DNA","Channel block/Z/T/names preserved");
    check(!run->provenance()->source_only && run->provenance()->object_id==id && run->raster().boundaries[0].cell && !run->raster().boundaries[0].nucleus,"ROI provenance and explicit clipped-body flag");
    near(run->pixel_center(3,3).x,3.5);near(run->pixel_center(3,3).y,3.5);rejects<std::out_of_range>([&]{run->pixel_center(-1,0);});
    require_current_fluorescence_cells(doc,request,*run);check(reader->reads==1,"Publication currentness does not rescan raw input");
    const FluorescenceCellRequest equivalent(doc,selected(),id,parameters(),{2,2});const auto again=detect_fluorescence_cells(equivalent);
    check(again->sha256()==run->sha256() && again->input_sha256()==run->input_sha256() && !run->matches(equivalent),"Stable scientific identity differs from capture publication authority");
    rejects<WorkflowInputChanged>([&]{require_current_fluorescence_cells(doc,equivalent,*run);});
    Document other(reader);check(!request.matches(other),"Another document never inherits capture authority");
    doc.set_calibration(.75,1);check(!request.matches(doc),"Calibration invalidates live authority");rejects<WorkflowInputChanged>([&]{require_current_fluorescence_cells(doc,request,*run);});doc.undo();
    doc.update_roi(id,Roi::rectangle({2,2,4,5}));check(!request.matches(doc),"ROI mutation invalidates live authority");doc.undo();
    const FluorescenceCellRequest current(doc,selected(),id,parameters(),{2,2});check(current.publication_matches(doc),"Fresh capture after ROI undo is current");
    reader->raw->channel_names[1]="Changed DNA";check(!current.publication_matches(doc),"Actual metadata currentness is checked at publication");
    rejects<WorkflowInputChanged>([&]{detect_fluorescence_cells(current);});reader->raw->channel_names[1]="DNA";check(current.publication_matches(doc),"Restored metadata matches fresh capture");
    const auto before=reader->reads.load();auto small=parameters();small.work_bytes=65536;
    rejects<std::length_error>([&]{FluorescenceCellRequest bad(doc,selected(),id,small);});check(reader->reads==before,"Work refusal precedes pixel allocation/IO");
    auto blurred=parameters();blurred.sigma=.5;
    const FluorescenceCellRequest with_context(doc,selected(),id,blurred,{2,2}),full_context(doc,selected(),id,blurred,{32,32});
    const auto local=detect_fluorescence_cells(with_context),full=detect_fluorescence_cells(full_context);
    for(std::int64_t y=2;y<7;++y)for(std::int64_t x=2;x<7;++x){const auto a=std::size_t(y-local->region().region.y)*local->raster().width+std::size_t(x-local->region().region.x);
        const auto b=std::size_t(y-full->region().region.y)*full->raster().width+std::size_t(x-full->region().region.x);check(local->raster().nuclei[a]==full->raster().nuclei[b],"Sufficient finite Gaussian context preserves core segmentation across crops");}
    rejects<std::invalid_argument>([&]{FluorescenceCellRequest bad(doc,selected(),id,blurred,{0,0});});
    auto physical=parameters();physical.units=FluorescenceCellUnits::Micrometres;physical.cell_expansion=.75;physical.peak_prominence=.25;
    const auto calibrated=detect_fluorescence_cells(FluorescenceCellRequest(doc,selected(),id,physical,{0,0}));
    check(calibrated->raster().metric==std::array<double,2>{.5,1} && calibrated->raster().measurements[0].cell.pixels==15,"Effective document calibration reaches native kernel");
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{detect_fluorescence_cells(request,&cancelled);});
    check(std::async(std::launch::async,[&]{try{request.matches(doc);}catch(const std::logic_error&){return true;}return false;}).get(),"Live document currentness stays on owner thread");
    auto outside=selected();outside.region.x=-1;rejects<std::invalid_argument>([&]{FluorescenceCellRequest bad(doc,outside,0,parameters());});
    auto axis=selected();axis.z=2;rejects<std::exception>([&]{FluorescenceCellRequest bad(doc,axis,0,parameters());});
    reader->wrong_shape=true;rejects<WorkflowInputChanged>([&]{detect_fluorescence_cells(current);});reader->wrong_shape=false;
    const auto source_only=detect_fluorescence_cells(FluorescenceCellRequest(doc,selected(),0,parameters(),{0,0}));check(source_only->provenance()->source_only && !source_only->roi(),"Explicit crop retains honest source-only provenance");
    reader->close();check(!request.matches(doc),"Closed source invalidates request");
}
void same_core(const FluorescenceCellRun& a,const FluorescenceCellRun& b) {
    const auto& core=a.core_region().region;const auto& x=a.raster();const auto& y=b.raster();
    for(std::int64_t py=core.y;py<core.y+core.height;++py)for(std::int64_t px=core.x;px<core.x+core.width;++px){
        const auto i=std::size_t(py-a.region().region.y)*x.width+std::size_t(px-a.region().region.x),j=std::size_t(py-b.region().region.y)*y.width+std::size_t(px-b.region().region.x);
        check(x.nuclei[i]==y.nuclei[j] && x.bodies[i]==y.bodies[j],"Finite context changes cannot change core nucleus/body labels");}
    check(x.measurements.size()==y.measurements.size() && x.boundaries.size()==y.boundaries.size(),"Context preserves compartment/label cohort");
    const auto compartment=[](const RawCellCompartment& a,const RawCellCompartment& b){
        check(a.pixels==b.pixels && a.valid_pixels==b.valid_pixels && a.channels.size()==b.channels.size(),"Context preserves sampled compartments");
        for(std::size_t c=0;c<a.channels.size();++c){const auto& x=a.channels[c];const auto& y=b.channels[c];check(x.finite_samples==y.finite_samples && x.nonfinite_samples==y.nonfinite_samples,"Context preserves finite/missing counts");
            const auto value=[](const auto& a,const auto& b){check(bool(a)==bool(b),"Context preserves absent observations");if(a)near(*a,*b);};
            value(x.sum,y.sum);value(x.mean,y.mean);value(x.minimum,y.minimum);value(x.maximum,y.maximum);value(x.sample_standard_deviation,y.sample_standard_deviation);}};
    for(std::size_t i=0;i<x.measurements.size();++i){check(x.boundaries[i].nucleus==y.boundaries[i].nucleus && x.boundaries[i].cell==y.boundaries[i].cell,"Context preserves boundary interpretation");
        const auto& a=x.measurements[i];const auto& b=y.measurements[i];check(a.label==b.label && bool(a.cytoplasm)==bool(b.cytoplasm),"Context preserves label/empty cytoplasm identity");compartment(a.nucleus,b.nucleus);compartment(a.cell,b.cell);if(a.cytoplasm)compartment(*a.cytoplasm,*b.cytoplasm);}
}
void finite_boundaries() {
    auto reader=std::make_shared<Reader>();Document doc(reader);auto p=parameters();p.sigma=.5;p.threshold=30;
    const auto ellipse=doc.add_roi(Roi::ellipse({2.25,1.5,5.5,6}));
    const auto local=detect_fluorescence_cells(FluorescenceCellRequest(doc,selected(),ellipse,p,{2,2}));
    const auto large=detect_fluorescence_cells(FluorescenceCellRequest(doc,selected(),ellipse,p,{32,32}));same_core(*local,*large);
    const auto& r=local->raster();for(std::int64_t y=0;y<r.height;++y)for(std::int64_t x=0;x<r.width;++x)if(!doc.annotation(ellipse).geometry.contains_sample(local->pixel_center(x,y),RoiSamplingPolicy::GeometryCoverage))
        check(!r.nuclei[std::size_t(y)*r.width+x] && !r.bodies[std::size_t(y)*r.width+x],"Context pixels never publish outside fractional ROI eligibility");
    reader->uniform=true;p=parameters();p.sigma=.25;p.units=FluorescenceCellUnits::Micrometres;p.cell_expansion=.75;p.peak_prominence=.25;
    auto crop=selected();crop.region={0,0,0,5,5};
    const auto edge=detect_fluorescence_cells(FluorescenceCellRequest(doc,crop,0,p,{2,2}));
    const auto edge_large=detect_fluorescence_cells(FluorescenceCellRequest(doc,crop,0,p,{32,32}));same_core(*edge,*edge_large);
    check(edge->region().region.x==0 && edge->region().region.y==0 && edge->raster().measurements.size()==1,"Source clips unavailable context explicitly");
    const auto& m=edge->raster().measurements[0];check(m.nucleus.pixels==25 && m.cell.pixels==25 && !m.cytoplasm && edge->raster().boundaries[0].nucleus && edge->raster().boundaries[0].cell,"Clipped uniform source keeps all boundary samples and absent cytoplasm");
    near(*m.nucleus.channels[0].mean,100);near(*m.nucleus.channels[1].mean,11022);
    rejects<std::invalid_argument>([&]{FluorescenceCellRequest bad(doc,crop,0,p,{0,0});});
    crop.region={0,0,0,15,9};const auto whole=detect_fluorescence_cells(FluorescenceCellRequest(doc,crop,0,p,{0,0}));
    check(whole->raster().measurements.size()==1 && whole->raster().measurements[0].nucleus.pixels==135 && whole->raster().boundaries[0].nucleus,"Full-image boundary needs no fabricated outside context");
    near(*whole->raster().measurements[0].nucleus.channels[1].mean,11047);
}
void await_read(Reader& reader,FluorescenceCellJob& job) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!reader.entered.load()){if(std::chrono::steady_clock::now()>deadline){job.cancel();job.await_completion();throw std::runtime_error("Worker did not enter native read barrier");}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
void jobs() {
    auto reader=std::make_shared<Reader>();Document doc(reader);const auto id=doc.add_rectangle({2,2,5,5});FluorescenceCellRequest request(doc,selected(),id,parameters(),{2,2});
    FluorescenceCellJob job;reader->block.store(true);check(job.request(request),"Job starts");await_read(*reader,job);check(job.busy() && !job.request(request),"One worker at a time");
    const auto running=job.poll(doc);check(running.state=="running","Read barrier exposes running state");std::this_thread::sleep_for(std::chrono::milliseconds(5));
    doc.update_roi(id,Roi::rectangle({2,2,4,4}));reader->release.store(true);const auto stale=job.wait(doc);check(stale.state=="stale" && !job.result(),"Job refuses stale ROI instead of delivering into another live state");
    check(stale.elapsed_ms>running.elapsed_ms,"Terminal duration includes work after the last running poll");doc.undo();
    check(!request.matches(doc),"ROI undo restores geometry but advances object revision");request=FluorescenceCellRequest(doc,selected(),id,parameters(),{2,2});
    reader->entered.store(false);reader->release.store(false);check(job.request(request),"Next job generation starts");await_read(*reader,job);job.cancel();check(job.wait(doc).state=="cancelled" && !job.result(),"Cancellation joins native read and discards result");
    reader->block.store(false);check(job.request(request),"Ready generation starts");const auto ready=job.wait(doc);check(ready.state=="ready" && ready.generation==3 && ready.phase==FluorescenceCellPhase::Ready && job.result(),"Owned native result delivered");
    require_current_fluorescence_cells(doc,request,*job.result());doc.set_calibration(.75,1);check(job.poll(doc).state=="stale" && !job.result(),"Delivered result invalidates on calibration edit");doc.undo();
    request=FluorescenceCellRequest(doc,selected(),id,parameters(),{2,2});reader->decoder_failure=true;check(job.request(request),"Failed generation starts");check(job.wait(doc).state=="failed" && !job.result(),"Decoder error is surfaced without fallback");reader->decoder_failure=false;
    reader->entered.store(false);reader->finished.store(false);reader->block.store(true);reader->release.store(false);
    auto joined=std::make_unique<FluorescenceCellJob>();check(joined->request(request),"Destruction test starts");await_read(*reader,*joined);joined.reset();check(reader->finished.load(),"Destructor cancels and joins before releasing native owners");
}
void files(const fs::path& root) {
    if(!has_tiff())return;const auto path=root/"source.ome.tif";qpx_test::fluorescence_fixture(path,false);
    TiffOpenOptions opening;opening.view.z=1;Document doc(open_tiff(path.u8string(),opening));const auto id=doc.add_rectangle({0,0,2,2});
    RawRegionRequest r;r.region={0,0,0,2,2};r.channels=3;r.z=1;FluorescenceCellOptions p;p.threshold=20000;
    const FluorescenceCellRequest request(doc,r,id,p,{0,0});const auto run=detect_fluorescence_cells(request);
    check(run->raster().measurements.size()==1 && run->raster().measurements[0].nucleus.pixels==2,"Original uint16 OME axes/channel values");near(*run->raster().measurements[0].nucleus.channels[0].mean,26100);near(*run->raster().measurements[0].nucleus.channels[2].mean,412.5);
    auto display=*doc.server()->tiff_view();display.channels={2,0,-1};display.lower={0,0,0};display.upper={1,1,1};doc.set_tiff_view(display);
    require_current_fluorescence_cells(doc,request,*run);check(detect_fluorescence_cells(FluorescenceCellRequest(doc,r,id,p,{0,0}))->sha256()==run->sha256(),"Presentation does not change raw result identity");
    auto empty_parameters=p;empty_parameters.threshold=1000000000;empty_parameters.cell_expansion=0;empty_parameters.sigma=0;
    const FluorescenceTiledRequest persisted(doc,r,0,empty_parameters);
    const auto checkpoint=checkpoint_tiled_fluorescence_cells(persisted,(root/"file-checkpoint").u8string());
    Document fresh(open_tiff(path.u8string(),opening));const FluorescenceTiledRequest reopened(fresh,r,0,empty_parameters);
    check(reopened.source_sha256()==persisted.source_sha256() && reopened.sha256()==persisted.sha256(),"File-backed checkpoint identity survives a fresh reader generation");
    const auto resumed=checkpoint_tiled_fluorescence_cells(reopened,(root/"file-checkpoint").u8string());
    check(resumed.state=="ready" && resumed.reused_tiles==1 && !resumed.segmented_tiles && resumed.output_sha256==checkpoint.output_sha256,"Fresh file-backed source reuses exact saved zero-cell geometry");
    std::ofstream changed(path,std::ios::binary|std::ios::app);changed.put('x');changed.close();check(bool(changed),"Mutate only the independently authored local fixture");
    check(!request.publication_matches(doc),"Publication rechecks actual files, not retained decoder/cache observations");
    rejects<WorkflowInputChanged>([&]{require_current_fluorescence_cells(doc,request,*run);});rejects<WorkflowInputChanged>([&]{detect_fluorescence_cells(request);});
    rejects<WorkflowInputChanged>([&]{checkpoint_tiled_fluorescence_cells(reopened,(root/"file-checkpoint").u8string());});
}
std::string checkpoint_bytes(const fs::path& file){std::ifstream in(file,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void checkpoints(const fs::path& root) {
    auto reader=std::make_shared<Reader>();reader->raw->width=80;reader->raw->height=52;reader->raw->levels={{80,52,1}};
    reader->display.width=80;reader->display.height=52;reader->display.levels={{80,52,1}};reader->dna_x=37;reader->dna_y=20;
    Document doc(reader);auto region=selected();region.region={0,0,0,80,52};auto p=parameters();p.sigma=.7;p.threshold_method=FluorescenceThreshold::Otsu256;p.max_cells=1000;
    FluorescenceTileOptions tiles;tiles.width=20;tiles.height=26;tiles.halo=12;const FluorescenceTiledRequest request(doc,region,0,p,tiles);
    const auto baseline=checkpoint_tiled_fluorescence_cells(request,(root/"full").u8string());
    check(baseline.state=="ready" && baseline.cells==1 && baseline.segmented_tiles==8 && !baseline.reused_tiles && baseline.output_sha256.size()==64,"Uninterrupted checkpoint publishes complete scientific output identity");
    std::atomic<bool> cancelled{false};std::size_t delivered=0;
    rejects<WorkflowCancelled>([&]{checkpoint_tiled_fluorescence_cells(request,(root/"partial").u8string(),{},[&](std::size_t,const FluorescenceTileResult&){if(++delivered==3)cancelled.store(true);},&cancelled);});
    check(delivered==3 && fs::exists(root/"partial"/"tile-2.qft") && !fs::exists(root/"partial"/"tile-3.qft") && !fs::exists(root/"partial"/"complete.qft"),"Interruption retains exactly committed tiles and advertises no complete workflow");
    const auto reads=reader->reads.load();std::uint64_t cells=0;
    const auto resumed=checkpoint_tiled_fluorescence_cells(request,(root/"partial").u8string(),{},[&](std::size_t,const FluorescenceTileResult& tile){cells+=tile.cells.size();});
    check(resumed.state=="ready" && resumed.reused_tiles==3 && resumed.segmented_tiles==5 && cells==1 && reader->reads.load()-reads==8,"Resume reuses threshold and completed segmentation with one raw verification read per tile");
    check(resumed.input_sha256==baseline.input_sha256 && resumed.output_sha256==baseline.output_sha256 && resumed.resolved_threshold==baseline.resolved_threshold,"Interrupted/resumed scientific input/output and threshold equal uninterrupted run");
    for(const auto& entry:fs::directory_iterator(root/"full"))check(checkpoint_bytes(entry.path())==checkpoint_bytes(root/"partial"/entry.path().filename()),"All serialized masks, IDs, moments and completion facts are byte-identical after resume");
    FluorescenceCellJob job;check(job.request(request,(root/"partial").u8string(),{}),"Existing native job resumes a checkpoint directory");
    check(job.wait(doc).state=="ready" && job.tiled_result()->reused_tiles==8 && !job.tiled_result()->segmented_tiles,"Fully completed job verifies/reuses rather than resegments");
    tiles.width=40;const FluorescenceTiledRequest changed(doc,region,0,p,tiles);const auto before=reader->reads.load();
    rejects<std::invalid_argument>([&]{checkpoint_tiled_fluorescence_cells(changed,(root/"partial").u8string());});check(reader->reads.load()==before,"Changed layout/options refuse before raw reads");
    reader->uniform=true;rejects<WorkflowInputChanged>([&]{checkpoint_tiled_fluorescence_cells(request,(root/"partial").u8string());});reader->uniform=false;
    fs::copy(root/"full",root/"corrupt",fs::copy_options::recursive);
    {std::fstream file(root/"corrupt"/"tile-1.qft",std::ios::binary|std::ios::in|std::ios::out);file.seekp(40);file.put('x');check(bool(file),"Actually corrupt one retained checkpoint record");}
    rejects<std::invalid_argument>([&]{checkpoint_tiled_fluorescence_cells(request,(root/"corrupt").u8string());});
    std::uint64_t total=0;for(const auto& entry:fs::directory_iterator(root/"full"))total+=fs::file_size(entry.path());
    check(total>4096,"Storage fixture covers a finite quota");FluorescenceCheckpointOptions limited;limited.record_bytes=4096;limited.total_bytes=total-1;
    rejects<WorkflowCaptureLimit>([&]{checkpoint_tiled_fluorescence_cells(request,(root/"limited").u8string(),limited);});
    check(!fs::exists(root/"limited"/"complete.qft"),"Exceeded quota never publishes complete status");
    const auto restored=checkpoint_tiled_fluorescence_cells(request,(root/"limited").u8string());
    check(restored.reused_tiles==8 && !restored.segmented_tiles && restored.output_sha256==baseline.output_sha256,"Raised storage quota reuses committed geometry without changing scientific output");
    const auto staging=root/"limited"/("tile-0.qft.qpx-tmp-"+std::string(24,'a'));
    {std::ofstream file(staging,std::ios::binary);file<<std::string(4096,'s');check(bool(file),"Retain an authored orphan staging fixture");}
    limited.total_bytes=total;const auto before_staging=reader->reads.load();
    rejects<std::invalid_argument>([&]{checkpoint_tiled_fluorescence_cells(request,(root/"limited").u8string(),limited);});
    check(reader->reads.load()==before_staging && fs::file_size(staging)==4096,"Orphan staging counts toward quota before raw work and is preserved");
    fs::create_directory(root/"foreign");{std::ofstream file(root/"foreign"/"keep.bin");file<<"keep";}
    rejects<std::invalid_argument>([&]{checkpoint_tiled_fluorescence_cells(request,(root/"foreign").u8string());});
    check(checkpoint_bytes(root/"foreign"/"keep.bin")=="keep","Unowned directory content is refused and preserved");
    reader->uniform=true;p.threshold_method=FluorescenceThreshold::Explicit;p.threshold=50;p.sigma=0;tiles.width=20;
    const FluorescenceTiledRequest cut(doc,region,0,p,tiles);const auto unresolved=checkpoint_tiled_fluorescence_cells(cut,(root/"unresolved").u8string());
    check(unresolved.state=="unresolved" && unresolved.unresolved_tiles==8 && !fs::exists(root/"unresolved"/"complete.qft"),"Cut foreground never advertises complete checkpoint status");
    for(std::size_t i=0;i<8;++i)check(!fs::exists(root/"unresolved"/("tile-"+std::to_string(i)+".qft")),"Unresolved tile is not stored as a complete reusable result");
    for(std::size_t i=0;i<8;++i)check(fs::exists(root/"unresolved"/("tile-"+std::to_string(i)+".qfu")),"Unresolved status is retained in an explicitly partial record");
    const auto retried=checkpoint_tiled_fluorescence_cells(cut,(root/"unresolved").u8string());
    check(!retried.reused_tiles && retried.segmented_tiles==8 && retried.output_sha256==unresolved.output_sha256,"Partial records are reprocessed and verified, never reused as complete");
}
void tiled_workflow() {
    auto reader=std::make_shared<Reader>();reader->raw->width=80;reader->raw->height=52;reader->raw->levels={{80,52,1}};
    reader->display.width=80;reader->display.height=52;reader->display.levels={{80,52,1}};reader->dna_x=37;reader->dna_y=20;
    Document doc(reader);auto region=selected();region.region={0,0,0,80,52};
    auto p=parameters();p.sigma=.7;p.threshold_method=FluorescenceThreshold::Otsu256;p.max_cells=1000;
    const auto whole=detect_fluorescence_cells(FluorescenceCellRequest(doc,region,0,p,{3,3}));
    check(whole->raster().measurements.size()==1,"Global Otsu reference has one synthetic nucleus");
    FluorescenceTileOptions tiles;tiles.width=20;tiles.height=26;tiles.halo=12;
    const FluorescenceTiledRequest request(doc,region,0,p,tiles);const auto before=reader->reads.load();
    check(request.matches(doc) && before==1 && request.tile_count()==8,"Tiled capture reads no pixels and declares finite cores");
    reader->max_read_pixels.store(0);std::map<std::string,std::vector<std::size_t>> outputs;
    const auto result=detect_tiled_fluorescence_cells(request,[&](std::size_t,const FluorescenceTileResult& tile){
        for(const auto& cell:tile.cells) {
            std::vector<std::size_t> actual,expected;
            for(const auto& span:cell.cell)for(std::int64_t x=span.x;x<span.x+span.width;++x)actual.push_back(std::size_t(span.y*80+x));
            for(std::size_t i=0;i<whole->raster().bodies.size();++i)if(whole->raster().bodies[i])expected.push_back(i);
            check(actual==expected,"Source-backed tiled Otsu body equals untiled whole-ROI geometry");
            check(cell.measurements.cell.channels[1].sum==whole->raster().measurements[0].cell.channels[1].sum &&
                cell.measurements.cell.channels[1].sample_standard_deviation==whole->raster().measurements[0].cell.channels[1].sample_standard_deviation,"Source-backed tile retains exact raw marker moments");
            check(outputs.emplace(cell.id,std::move(actual)).second,"Source workflow publishes one global cell identity");
        }
    });
    check(result.state=="ready" && result.tiles==8 && result.cells==1 && result.finite_pixels==80*52 && result.accounted_work_bytes<=p.work_bytes,"Bounded source-backed tiled report covers each ROI pixel once");
    check(result.resolved_threshold==whole->raster().resolved_threshold && reader->reads.load()-before==3*request.tile_count() && reader->max_read_pixels.load()<80*52,"Global threshold equals untiled Otsu through three bounded passes, never a full-slide read");
    tiles.width=40;const FluorescenceTiledRequest alternate(doc,region,0,p,tiles);
    check(alternate.sha256()!=request.sha256() && alternate.source_sha256()==request.source_sha256(),"Resume request binds tile layout; scientific source identity does not");
    std::map<std::string,std::vector<std::size_t>> moved;
    const auto other=detect_tiled_fluorescence_cells(alternate,[&](std::size_t,const FluorescenceTileResult& tile){for(const auto& c:tile.cells){std::vector<std::size_t> body;for(const auto& s:c.cell)for(std::int64_t x=s.x;x<s.x+s.width;++x)body.push_back(std::size_t(s.y*80+x));moved.emplace(c.id,std::move(body));}});
    check(other.resolved_threshold==result.resolved_threshold && moved==outputs,"Global Otsu values/IDs/geometry are independent of tile layout");
    reader->reads.store(0);reader->switch_uniform_after=unsigned(request.tile_count());
    rejects<WorkflowInputChanged>([&]{detect_tiled_fluorescence_cells(request);});reader->switch_uniform_after=0;
    reader->block.store(true);reader->release.store(false);reader->entered.store(false);
    FluorescenceCellJob job;check(job.request(request),"Existing cell job accepts tiled request");await_read(*reader,job);
    check(!job.request(alternate),"Tiled and untiled work share one job execution slot");job.cancel();
    check(job.wait(doc).state=="cancelled" && !job.tiled_result() && !job.result(),"Tiled read cancellation discards delivery");
    reader->entered.store(false);check(job.request(request),"Next tiled generation starts");await_read(*reader,job);doc.set_calibration(.75,1);reader->release.store(true);
    check(job.wait(doc).state=="stale" && !job.tiled_result(),"Owner calibration mutation prevents tiled result delivery");doc.undo();
    reader->block.store(false);const FluorescenceTiledRequest fresh(doc,region,0,p,tiles);check(job.request(fresh),"Fresh tiled request starts");
    check(job.wait(doc).state=="ready" && job.tiled_result() && !job.result(),"Existing native job delivers the typed tiled report");
    reader->uniform=true;p.sigma=0;const FluorescenceTiledRequest constant_request(doc,region,0,p,tiles);
    const auto constant=detect_tiled_fluorescence_cells(constant_request);
    check(constant.state=="ready" && constant.resolved_threshold==100 && !constant.cells,"Global constant Otsu resolves once and returns a valid zero-cell result");
}
int main(int argc,char** argv){try {
    tiled_workflow();
    source_contract();finite_boundaries();jobs();const auto root=fs::current_path()/("fluorescence-source-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Create unique native fixture directory");checkpoints(root);files(root);
    if(argc>1){Document real(open_tiff(argv[1]));const auto m=real.server()->raw_metadata();RawRegionRequest r;r.region={0,0,0,m->width,m->height};r.channels=m->channels;FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.min_nucleus_area=5;p.cell_expansion=3;
        const FluorescenceCellRequest request(real,r,0,p,{0,0});FluorescenceCellJob job;check(job.request(request),"Real native job starts");check(job.wait(real).state=="ready" && job.result(),"Real source job completes");require_current_fluorescence_cells(real,request,*job.result());std::cout<<"Real source-bound job: "<<job.result()->raster().measurements.size()<<" candidates; annotation acceptance remains pending\n";}
    std::cout<<"Fluorescence source: one raw read, axes/calibration/ROI/context, stable identity, capture authority, stale/cancel/jobs and actual file guards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
