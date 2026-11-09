#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_fixture.hpp"
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>
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
    mutable std::atomic<bool> entered{false},finished{false};
    std::atomic<bool> block{false},release{false},is_closed{false};
    bool wrong_shape=false,decoder_failure=false,uniform=false;
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
        ++reads;entered.store(true);
        struct Finished {std::atomic<bool>& flag;~Finished(){flag.store(true);}} finished_scope{finished};
        while(block.load() && !release.load()){workflow_checkpoint(cancelled);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        workflow_checkpoint(cancelled);if(decoder_failure)throw std::runtime_error("Independent decoder failure");
        RawPixelBuffer out(r.region.width+(wrong_shape?1:0),r.region.height,r.channels,SampleType::Float64);
        auto& values=std::get<std::vector<double>>(out.writable_storage());std::fill(out.writable_valid().begin(),out.writable_valid().end(),1);
        for(std::int64_t y=0;y<r.region.height;++y)for(std::int64_t x=0;x<r.region.width;++x){const auto px=x+r.region.x,py=y+r.region.y;
            for(std::uint32_t c=0;c<r.channels;++c){const auto channel=r.first_channel+c;
                values[(std::size_t(y)*std::size_t(out.width())+std::size_t(x))*r.channels+c]=channel==0?1000+px:channel==1?(uniform || (px>=3 && px<=5 && py>=3 && py<=5)?100:0):double(px+10*py+1000*r.z+10000*r.t);}}
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
    std::ofstream changed(path,std::ios::binary|std::ios::app);changed.put('x');changed.close();check(bool(changed),"Mutate only the independently authored local fixture");
    check(!request.publication_matches(doc),"Publication rechecks actual files, not retained decoder/cache observations");
    rejects<WorkflowInputChanged>([&]{require_current_fluorescence_cells(doc,request,*run);});rejects<WorkflowInputChanged>([&]{detect_fluorescence_cells(request);});
}
int main(int argc,char** argv){try {
    source_contract();finite_boundaries();jobs();const auto root=fs::current_path()/("fluorescence-source-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Create unique native fixture directory");files(root);
    if(argc>1){Document real(open_tiff(argv[1]));const auto m=real.server()->raw_metadata();RawRegionRequest r;r.region={0,0,0,m->width,m->height};r.channels=m->channels;FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.min_nucleus_area=5;p.cell_expansion=3;
        const FluorescenceCellRequest request(real,r,0,p,{0,0});FluorescenceCellJob job;check(job.request(request),"Real native job starts");check(job.wait(real).state=="ready" && job.result(),"Real source job completes");require_current_fluorescence_cells(real,request,*job.result());std::cout<<"Real source-bound job: "<<job.result()->raster().measurements.size()<<" candidates; annotation acceptance remains pending\n";}
    std::cout<<"Fluorescence source: one raw read, axes/calibration/ROI/context, stable identity, capture authority, stale/cancel/jobs and actual file guards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
