#include "../viewer_bridge.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/tma.hpp"
#include "qupathxx/raw_pixel_frontend.hpp"
#include "quantitative_tiff_fixture.hpp"
#include "object_forest_fixture.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <future>
#include <fstream>
#include <random>
#include <thread>
using namespace qpx;
void checked(bool value,int line) {if(!value) throw std::runtime_error("viewer session check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F> void rejects(F f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing viewer rejection");}
struct Harness {
    std::shared_ptr<Project> project=std::make_shared<Project>();std::vector<ProjectImage> images;ProjectSaveJob save_job;
    std::vector<std::uint64_t> selected;bool closed=false;detail::ViewerContext context;
    Harness() {auto a=std::make_shared<Document>(Image(32,24,std::vector<std::uint8_t>(32*24*3,16)));a->add_rectangle({1,2,20,16});project->add_document(a,"first");auto b=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,32)));b->add_rectangle({0,0,16,16});project->add_document(b,"second");images=project->images();
        context.project=project;context.catalog=&images;context.activate=[&](auto id){project->set_active_image(id);};context.select=[&](const auto& ids){for(auto id:ids) project->document(project->active_image())->annotation(id);selected=ids;};
        context.save=[&](const auto& path){return save_job.request(project,path);};context.save_stats=[&]{return save_job.poll();};context.cancel_save=[&]{save_job.cancel();};context.close=[&](bool){closed=true;};
    }
    void step(detail::ViewerOwner& owner) {++context.frame;context.active_image=project->active_image();owner.pump(context);}
    ViewerEvent until(detail::ViewerOwner& owner,ViewerSession& session,std::uint64_t ticket) {const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);for(;;) {step(owner);const auto event=session.poll(ticket);if(event.state!="queued" && event.state!="running") return event;if(std::chrono::steady_clock::now()>end) throw std::runtime_error("viewer test timeout");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
};

void raw_pixel_session_contracts(const std::filesystem::path& root) {
    if(!has_project() || !has_tiff()) {std::cout<<"Raw pixel viewer unavailable: TIFF/project OFF\n";return;}
    Harness h;const auto path=root/"raw-viewer.tif";qpx_test::raw_pixel_tiff_fixture(path);auto server=open_tiff(path.u8string());auto doc=std::make_shared<Document>(server);
    const auto a=doc->add_detection(Roi::rectangle({0,0,3,6}),"A",0,"A"),b=doc->add_detection(Roi::rectangle({4,0,4,6}),"B",0,"B");const auto target=doc->add_rectangle({0,0,8,6});const auto image=h.project->add_document(doc);h.images=h.project->images();
    ScientificRawLattice lattice(8,6,{0,8,7,2},{0,6,5,2});ScientificRawOptions selection;selection.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};
    RawPixelRecipe recipe(*server->raw_metadata(),lattice,selection,features);std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:recipe.feature_schema()) schema.emplace_back(f.name,f.unit);
    const RawPixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),recipe);ViewerSession session((root/"raw-owner.qpxproj").u8string());detail::ViewerOwner owner(session);
    auto ticket=session.predict_raw_pixels(image,model,lattice.window(1,1,2,2),target);auto result=h.until(owner,session,ticket);
    auto expected=predict_raw_pixel_window(RawPixelWindowPredictionRequest(*doc,model,lattice.window(1,1,2,2),target));check(result.state=="succeeded" && result.raw_pixel_prediction && result.raw_pixel_prediction->labels()==expected->labels() && result.raw_pixel_prediction->vote_fractions()==expected->vote_fractions() && !result.committed);session.take(ticket);check(session.stats().retained_bytes==0);
    ticket=session.predict_raw_pixels(image,model,lattice.window(0,0,2,2),target);h.step(owner);doc->rename_annotation(target,"edited during preview");check(h.until(owner,session,ticket).state=="stale");session.take(ticket);
    RawPixelStreamOptions stream;stream.tile_width=stream.tile_height=2;stream.work_bytes=16u*1024*1024;RawPixelStreamSaveOptions save;save.max_new_windows=1;const auto output=root/"viewer-prediction.qws";
    ticket=session.export_raw_pixels(image,model,lattice,output.u8string(),stream,save,target,false);result=h.until(owner,session,ticket);check(result.state=="partial" && result.committed && result.raw_pixel_link && !result.raw_pixel_link->summary().complete);session.take(ticket);
    save.resume_path=output.u8string();save.replace_existing=true;save.max_new_windows=UINT64_MAX;ticket=session.export_raw_pixels(image,model,lattice,output.u8string(),stream,save,target,true);result=h.until(owner,session,ticket);check(result.state=="succeeded" && result.committed && result.record_id && result.raw_pixel_link->summary().complete);const auto record=result.record_id;session.take(ticket);
    const auto revision=doc->revision();ticket=session.read_raw_pixel_stream(image,"",0,{},record);result=h.until(owner,session,ticket);check(result.state=="succeeded" && result.verified && result.raw_pixel_prediction && doc->revision()==revision && !result.committed);const auto retained=result.raw_pixel_prediction;session.take(ticket);
    ticket=session.read_raw_pixel_stream(image,"",0,{},record);h.step(owner);++h.context.epoch;check(h.until(owner,session,ticket).state=="stale");session.take(ticket);
    RawPixelTrainingSettings settings;settings.lattice=lattice;settings.selection=selection;settings.features=features;settings.tiles.width=settings.tiles.height=2;settings.forest.trees=5;settings.forest.max_depth=3;settings.forest.work_bytes=2u*1024*1024;
    if(has_classifier_training()) {
        // The table API reuses a real live capture without another TIFF read.
        RawPixelTiledTrainingRequest capture(*doc,{a,b},lattice,selection,features,settings.sampling,settings.tiles);auto table=sample_raw_pixel_tiled_training(capture);const auto before=*server->tiff_read_stats();RawPixelTiledTrainingJob fit;check(fit.train_table(capture,table,settings.forest) && fit.wait(*doc).state=="ready" && fit.table()==table);const auto after=*server->tiff_read_stats();check(before.decoded_blocks==after.decoded_blocks && before.cache_hits==after.cache_hits);
        const auto archive=archive_raw_pixel_tiled_training(table);const auto historical=RawPixelArchive::from_bytes(archive->bytes());rejects([&]{fit.train_table(capture,historical->tiled_table(),settings.forest);});RawPixelTiledTrainingRequest other(*doc,{a,b},lattice,selection,features,settings.sampling,settings.tiles);rejects([&]{fit.train_table(other,table,settings.forest);});
        settings.checkpoint_path=(root/"viewer-training.qtc").u8string();settings.checkpoint.max_new_windows=1;ticket=session.train_raw_pixels(image,{a,b},settings,true);result=h.until(owner,session,ticket);check(result.state=="partial" && result.raw_pixel_checkpoint && !result.raw_pixel_training && !result.record_id);session.take(ticket);
        settings.checkpoint.resume_path=settings.checkpoint_path;settings.checkpoint.replace_existing=true;settings.checkpoint.max_new_windows=UINT64_MAX;ticket=session.train_raw_pixels(image,{a,b},settings,true);result=h.until(owner,session,ticket);check(result.state=="succeeded" && result.raw_pixel_training && result.raw_pixel_training->model() && result.record_id && result.committed);session.take(ticket);
    } else {ticket=session.train_raw_pixels(image,{a,b},settings);check(h.until(owner,session,ticket).state=="failed");session.take(ticket);}
    ticket=session.predict_raw_pixels(image,model,lattice.window(0,0,2,2),target);check(session.cancel(ticket) && session.take(ticket).state=="cancelled");
    ticket=session.predict_raw_pixels(image,model,lattice.window(0,0,2,2),target);h.step(owner);h.project->remove_image(image);check(h.until(owner,session,ticket).state=="stale");session.take(ticket);owner.finish(h.context);check(!retained->labels().empty());server->close();
    std::cout<<"Raw pixel viewer: owner training/checkpoint resume, bounded prediction, verified file retention, historical isolation, stale/cancel/removal and join passed; training="<<has_classifier_training()<<'\n';
}

void axis_session_contracts(const std::filesystem::path& root) {
    Harness h;auto doc=h.project->document(1);TmaGridOptions o;o.rows=2;o.columns=2;o.origin={8,7};o.row_step={0,8};o.column_step={9,0};o.width=4;o.height=4;doc->create_tma_grid(o);
    ViewerSession session((root/"axis").u8string());detail::ViewerOwner owner(session);
    std::vector<Rect> bounds{{4,5,4,4},{13,5,4,4}};
    auto ticket=session.insert_tma_row(1,1,"inserted",bounds,true,doc->revision());bounds[0].x=25;
    auto event=h.until(owner,session,ticket);check(event.state=="succeeded" && event.committed && event.object_ids.size()==2);
    check(doc->annotation(event.object_ids[0]).bounds().x==4 && doc->annotation(event.object_ids[0]).tma_core->missing());session.take(ticket);
    const auto revision=doc->revision();ticket=session.remove_tma_row(1,1,revision-1);
    check(h.until(owner,session,ticket).state=="stale" && doc->revision()==revision);session.take(ticket);
    ticket=session.remove_tma_column(1,0);check(session.cancel(ticket));check(session.take(ticket).state=="cancelled" && doc->revision()==revision);
    ticket=session.remove_tma_row(1,1,revision);event=h.until(owner,session,ticket);
    check(event.state=="succeeded" && event.committed && event.object_ids.empty() && doc->tma_core_count()==4);session.take(ticket);
    ticket=session.insert_tma_column(1,1,"vacant",std::vector<Rect>{});check(h.until(owner,session,ticket).state=="succeeded" && doc->tma_grid()->info()->columns()==3);session.take(ticket);
    owner.finish(h.context);
    ViewerLimits limits;limits.max_edits=2;ViewerSession bounded((root/"axis-bounded").u8string(),limits);detail::ViewerOwner controller(bounded);
    const auto before=doc->revision();ticket=bounded.remove_tma_column(1,0);
    check(h.until(controller,bounded,ticket).state=="failed" && doc->revision()==before);bounded.take(ticket);controller.finish(h.context);
    std::cout<<"TMA viewer axis commands: copied bounds, revision, cancellation, atomic result and edit admission passed\n";
}

int main() {
    namespace fs=std::filesystem;const auto root=fs::current_path()/("viewer-test-"+std::to_string(std::random_device{}()));
    try {
        fs::create_directory(root);raw_pixel_session_contracts(root);axis_session_contracts(root);ViewerLimits limits;limits.validate();limits.tickets=0;rejects([&]{limits.validate();});
        Harness h;ViewerSession session((root/"source.qpxproj").u8string());detail::ViewerOwner owner(session);rejects([&]{detail::ViewerOwner second(session);});
        auto id=session.catalog();auto e=h.until(owner,session,id);check(e.state=="succeeded" && e.images.size()==2);session.take(id);rejects([&]{session.poll(id);});
        id=session.inspect(1,1);e=h.until(owner,session,id);check(e.object && e.metadata && e.metadata->width==32);const auto revision=e.document_revision;session.take(id);
        id=session.execute(1,{ObjectEdit::create({0,0,3,3},"queued")},"script",revision);e=h.until(owner,session,id);check(e.committed && e.object_ids.size()==1 && h.project->document(1)->object_count()==2);session.take(id);
        id=session.execute(1,{ObjectEdit::remove(1)},"stale",revision);check(h.until(owner,session,id).state=="stale" && h.project->document(1)->object_count()==2);session.take(id);
        id=session.activate(2);check(h.until(owner,session,id).state=="succeeded");session.take(id);id=session.select(2,{1});check(h.until(owner,session,id).state=="succeeded" && h.selected==std::vector<std::uint64_t>{1});session.take(id);
        id=session.read_region(1,{0,0,0,4,3});e=h.until(owner,session,id);check(e.pixels && e.pixels->bytes()[0]==16);auto retained=e.pixels;session.take(id);check(session.stats().retained_bytes==0);
        const auto render_before=h.project->document(1)->render_revision();
        StainTransform transform({{1,0,0},{0,1,0}});
        id=session.analyze(1,1,transform);e=h.until(owner,session,id);check(e.state=="succeeded" && e.committed && e.analysis && e.samples==320);session.take(id);
        id=session.tissue(1,1,transform,{},true);e=h.until(owner,session,id);check(e.state=="succeeded" && e.tissue && e.record_id);session.take(id);
        id=session.cells(1,1,transform,{},true);e=h.until(owner,session,id);check(e.state=="succeeded" && e.cells && e.record_id);session.take(id);
        check(h.project->document(1)->render_revision()==render_before);
        id=session.export_measurements(0,MeasurementTableKind::Roi,(root/"roi.csv").u8string());e=h.until(owner,session,id);check(e.committed && fs::file_size(root/"roi.csv")>0);session.take(id);
        id=session.export_cells(0,(root/"cells.csv").u8string());e=h.until(owner,session,id);check(e.committed && fs::file_size(root/"cells.csv")>0);session.take(id);
        if(has_project()) {id=session.save((root/"saved.qpxproj").u8string());e=h.until(owner,session,id);check(e.committed && e.state=="succeeded");check(Project::open(e.path)->document(1)->analysis_record_count()==1);session.take(id);}
        h.context.blocked=true;id=session.catalog();h.step(owner);check(session.poll(id).state=="queued");check(session.cancel(id));check(session.take(id).state=="cancelled");h.context.blocked=false;
        id=session.catalog();++h.context.epoch;check(h.until(owner,session,id).state=="stale");session.take(id);
        id=session.catalog();auto waiting=std::async(std::launch::async,[&]{return session.wait(id,3);});h.step(owner);check(waiting.get().state=="succeeded");session.take(id);
        id=session.analyze(1,1,transform);h.step(owner);h.project->document(1)->rename_annotation(1,"changed");check(h.until(owner,session,id).state=="stale");session.take(id);
        id=session.read_region(1,{0,0,0,4,4});h.step(owner);h.project->remove_image(1);check(h.until(owner,session,id).state=="stale");session.take(id);
        for(int i=0;i<100;++i) {id=session.catalog();auto waiter=std::async(std::launch::async,[&]{try {return session.wait(id,2).state;}catch(const std::out_of_range&) {return std::string("consumed");}});h.step(owner);auto taken=session.take(id);auto notified=waiter.get();check(taken.state=="succeeded" && (notified=="succeeded" || notified=="consumed"));}
        id=session.close(true);check(h.until(owner,session,id).state=="succeeded" && h.closed);session.take(id);owner.finish(h.context);check(session.stats().state=="closed" && retained->bytes()[0]==16);rejects([&]{session.catalog();});
        {ViewerLimits small;small.tickets=1;ViewerSession bounded((root/"x").u8string(),small);for(int i=0;i<1000;++i) {auto t=bounded.catalog();rejects([&]{bounded.catalog();});check(bounded.cancel(t));bounded.take(t);}check(!bounded.stats().queued && !bounded.stats().queued_bytes);}
        {Harness test;ViewerLimits small;small.result_bytes=65536;ViewerSession bounded((root/"x").u8string(),small);detail::ViewerOwner controller(bounded);auto t=bounded.read_region(1,{0,0,0,256,256});check(test.until(controller,bounded,t).state=="failed");check(bounded.stats().retained_bytes==0);controller.finish(test.context);}
        {
            Harness paused;ViewerSession frozen((root/"frozen").u8string());detail::ViewerOwner controller(frozen);
            paused.context.frame=30;paused.context.active_image=1;paused.context.activate=[](auto){};
            auto ticket=frozen.activate(2);
            for(int i=0;i<5;++i) controller.pump(paused.context);
            check(frozen.take(ticket).state=="failed" && frozen.stats().frame==30);
            paused.context.activate=[&](auto image){paused.project->set_active_image(image);};
            ticket=frozen.activate(2);controller.pump(paused.context);
            paused.context.active_image=paused.project->active_image();controller.pump(paused.context);
            check(frozen.take(ticket).state=="succeeded" && frozen.stats().frame==30);
            for(int i=0;i<64;++i) {
                const std::uint64_t target=i%2+1;ticket=frozen.activate(target);
                auto observer=std::async(std::launch::async,[&,ticket,target] {
                    auto event=frozen.wait(ticket,3);return event.state=="succeeded" && frozen.stats().active_image==target;
                });
                controller.pump(paused.context);
                paused.context.active_image=paused.project->active_image();controller.pump(paused.context);
                check(observer.get());frozen.take(ticket);
            }
            controller.finish(paused.context);
        }
        {
            Harness test;std::vector<std::uint8_t> rgb(48*40*3,245);
            auto fill=[&](int x0,int y0,int width,int height) {for(int y=y0;y<y0+height;++y) for(int x=x0;x<x0+width;++x) {
                const auto index=(y*48+x)*3;rgb[index]=35;rgb[index+1]=45;rgb[index+2]=75;
            }};
            fill(7,8,8,10);fill(29,22,10,9);
            auto doc=std::make_shared<Document>(Image(48,40,std::move(rgb)));doc->set_calibration(.5,.25);
            auto object=doc->add_rectangle({0,0,48,40});auto image=test.project->add_document(doc,"physical areas");
            ViewerSession endpoint((root/"areas.qpxproj").u8string());detail::ViewerOwner controller(endpoint);
            CellParameters parameters;parameters.sigma=0;parameters.threshold=.5;parameters.min_nucleus_pixels=1;parameters.cell_expansion=3;
            rejects([&]{endpoint.cells_with_nucleus_area_limits(image,object,transform,-1.,{},parameters);});
            rejects([&]{endpoint.cells_with_nucleus_area_limits(image,object,transform,12.,11.,parameters);});
            check(endpoint.stats().tickets==0 && endpoint.stats().queued_bytes==0);
            auto ticket=endpoint.cells_with_nucleus_area_limits(image,object,transform,10.1,11.25,parameters,true);
            parameters.min_nucleus_pixels=10000;
            auto result=test.until(controller,endpoint,ticket);
            check(result.state=="succeeded" && result.committed && result.record_id && result.cells && result.cells->features().size()==1);
            check(result.cells->parameters().min_nucleus_pixels==81 && result.cells->parameters().max_nucleus_pixels==90);
            check(result.cells->features()[0].nucleus_pixels==90 && result.cells->calibrated_feature(1).nucleus_sample_area_um2==11.25);
            auto retained_cells=result.cells;endpoint.take(ticket);check(endpoint.stats().retained_bytes==0);
            parameters.min_nucleus_pixels=1;
            // Resolution happens when the owner handles the command, not on the producer.
            ticket=endpoint.cells_with_nucleus_area_limits(image,object,transform,10.1,11.25,parameters,true);
            doc->set_calibration(2,3);result=test.until(controller,endpoint,ticket);
            check(result.state=="succeeded" && result.cells && result.cells->features().empty());endpoint.take(ticket);
            const auto records=doc->cell_record_count();
            ticket=endpoint.cells_with_nucleus_area_limits(image,object,transform,10.1,11.25,parameters,true);
            test.step(controller);doc->set_calibration(.5,.25);
            check(test.until(controller,endpoint,ticket).state=="stale" && doc->cell_record_count()==records);endpoint.take(ticket);
            ticket=endpoint.cells_with_nucleus_area_limits(1,1,transform,0.,{},parameters,true);
            result=test.until(controller,endpoint,ticket);check(result.state=="failed" && !result.committed && !result.record_id);
            check(result.error.find("both pixel calibration axes")!=std::string::npos);endpoint.take(ticket);
            ticket=endpoint.cells_with_nucleus_area_limits(image,object,transform,{},0.,parameters,true);
            check(endpoint.cancel(ticket));check(endpoint.take(ticket).state=="cancelled" && doc->cell_record_count()==records);
            controller.finish(test.context);doc->server()->close();
            check(retained_cells->calibrated_feature(1).nucleus_sample_area_um2==11.25);
        }
        {
            Harness test;std::vector<std::uint8_t> rgb(6*9*3,255);
            for(int y=3;y<6;++y) for(int x=2;x<4;++x) for(int c=0;c<3;++c) rgb[(y*6+x)*3+c]=32;
            auto doc=std::make_shared<Document>(Image(6,9,std::move(rgb)));doc->set_calibration(.5,.25);
            const auto object=doc->add_rectangle({0,0,6,9}),image=test.project->add_document(doc,"grid cells");
            ViewerSession endpoint((root/"grid.qpxproj").u8string());detail::ViewerOwner controller(endpoint);
            GridCellSampling sampling{3,3,1,1};GridCellParameters p;p.units=GridCellUnits::Micrometres;
            p.sigma=0;p.log_prominence=.2;p.min_nucleus_area=.75;p.cell_expansion=.9;p.contours.work_bytes=1024*1024;
            auto invalid=sampling;invalid.samples_x=0;rejects([&]{endpoint.grid_cells(image,object,transform,invalid,p,true);});
            check(endpoint.stats().tickets==0 && endpoint.stats().queued_bytes==0);
            auto ticket=endpoint.grid_cells(image,object,transform,sampling,p,true);
            sampling.samples_x=1;p.min_nucleus_area=10000;
            auto result=test.until(controller,endpoint,ticket);
            check(result.state=="succeeded" && result.committed && result.record_id && result.cells && result.cells->features().size()==1);
            check(result.cells->nuclei()->scientific_grid()->x().samples()==3 && result.cells->features()[0].cell_pixels==3);
            check(result.cells->resolved_grid_parameters()->step_x==1. && result.cells->resolved_grid_parameters()->step_y==.75);
            const auto retained_cells=result.cells;endpoint.take(ticket);check(endpoint.stats().retained_bytes==0);
            sampling.samples_x=3;p.min_nucleus_area=.75;
            ticket=endpoint.grid_cells(image,object,transform,sampling,p,true);doc->set_calibration(1,1);
            result=test.until(controller,endpoint,ticket);
            check(result.state=="succeeded" && result.cells && result.cells->features()[0].cell_pixels==1);
            check(result.cells->resolved_grid_parameters()->step_x==2. && result.cells->resolved_grid_parameters()->step_y==3.);endpoint.take(ticket);
            const auto records=doc->cell_record_count();ticket=endpoint.grid_cells(image,object,transform,sampling,p,true);
            test.step(controller);doc->set_calibration(.5,.25);
            check(test.until(controller,endpoint,ticket).state=="stale" && doc->cell_record_count()==records);endpoint.take(ticket);
            ticket=endpoint.grid_cells(image,object,transform,sampling,p,true);check(endpoint.cancel(ticket));
            check(endpoint.take(ticket).state=="cancelled" && doc->cell_record_count()==records);
            controller.finish(test.context);doc->server()->close();check(retained_cells->calibrated_feature(1).cell_sample_area_um2==2.25);
            std::cout<<"Grid viewer producer copies, owner calibration capture, retention, cancellation and stale delivery passed\n";
        }
        if(has_tiff()) {
            // Independent little-endian TIFF fixture; no reader encoder shares its implementation.
            const auto input=root/"quantitative.tif";std::vector<unsigned char> bytes={'I','I',42,0,8,0,0,0};
            auto u16=[&](std::uint16_t n){bytes.push_back(n&255);bytes.push_back(n>>8);};auto u32=[&](std::uint32_t n){for(int i=0;i<4;++i) bytes.push_back((n>>(8*i))&255);};
            u16(10);auto tag=[&](std::uint16_t id,std::uint16_t type,std::uint32_t n){u16(id);u16(type);u32(1);u32(n);};
            tag(256,4,2);tag(257,4,2);tag(258,3,16);tag(259,3,1);tag(262,3,1);tag(273,4,134);tag(277,3,1);tag(278,4,2);tag(279,4,8);tag(339,3,1);u32(0);
            for(auto value:{1,256,40000,65535}) u16(static_cast<std::uint16_t>(value));
            {std::ofstream file(input,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
            Harness test;auto doc=std::make_shared<Document>(open_tiff(input.u8string()));const auto object=doc->add_rectangle({0,0,2,2});
            const auto image=test.project->add_document(doc,"raw",input.u8string(),"tiff");test.images=test.project->images();
            ViewerSession endpoint((root/"raw.qpxproj").u8string());detail::ViewerOwner controller(endpoint);
            RawRegionRequest region;region.region=RegionRequest(0,0,0,2,2);
            auto ticket=endpoint.analyze_raw(image,region,object);auto result=test.until(controller,endpoint,ticket);
            check(result.state=="succeeded" && !result.committed && !result.record_id && result.raw_analysis && result.samples==4);
            check(result.raw_analysis->statistics().means[0]==26448. && doc->raw_analysis_record_count()==0);endpoint.take(ticket);
            ticket=endpoint.analyze_raw(image,region,object,false,true);test.step(controller);
            // Preview changes do not invalidate the quantitative request captured by the owner.
            auto view=*doc->server()->tiff_view();view.upper={100,100,100};doc->set_tiff_view(view);
            result=test.until(controller,endpoint,ticket);check(result.state=="succeeded" && result.committed && result.record_id);
            const auto record=result.record_id;auto retained_raw=result.raw_analysis;endpoint.take(ticket);check(endpoint.stats().retained_bytes==0);
            const auto canonical=doc->revision();ticket=endpoint.verify_raw_analysis(image,record);result=test.until(controller,endpoint,ticket);
            check(result.state=="succeeded" && result.verified && !result.committed && result.record_id==record && doc->revision()==canonical);endpoint.take(ticket);
            ticket=endpoint.export_raw_measurements(image,(root/"raw.csv").u8string(),true,{record,record});result=test.until(controller,endpoint,ticket);
            check(result.state=="succeeded" && result.committed && result.rows==1 && fs::file_size(root/"raw.csv")>100);endpoint.take(ticket);
            ticket=endpoint.export_raw_measurements(image,input.u8string());check(test.until(controller,endpoint,ticket).state=="failed");endpoint.take(ticket);
            ticket=endpoint.analyze_raw(image,region,object,false,true);test.step(controller);doc->rename_annotation(object,"edited");
            check(test.until(controller,endpoint,ticket).state=="stale" && doc->raw_analysis_record_count()==1);endpoint.take(ticket);
            ticket=endpoint.verify_raw_analysis(image,record);check(test.until(controller,endpoint,ticket).state=="failed");endpoint.take(ticket);
            ticket=endpoint.export_raw_measurements(image,(root/"stale.csv").u8string(),true);check(test.until(controller,endpoint,ticket).state=="failed" && !fs::exists(root/"stale.csv"));endpoint.take(ticket);
            {
                RawGridSampling sampling{1,1,1,1};ScientificRawOptions options;
                const auto previous_records=doc->raw_analysis_record_count();
                auto invalid=sampling;invalid.samples_x=0;
                rejects([&]{endpoint.analyze_raw_grid(image,object,invalid,options);});
                rejects([&]{endpoint.analyze_raw_grid(image,0,sampling,options);});
                check(endpoint.stats().tickets==0 && endpoint.stats().queued_bytes==0);
                ticket=endpoint.analyze_raw_grid(image,object,sampling,options,false,true);
                sampling.samples_x=2;options.channels=0;doc->set_calibration(2.,3.);
                result=test.until(controller,endpoint,ticket);
                check(result.state=="succeeded" && result.committed && result.record_id && result.raw_analysis && result.samples==1);
                check(result.raw_analysis->grid() && result.raw_analysis->grid()->grid.x().samples()==1);
                check(result.raw_analysis->grid()->grid.x().context()==1 && result.raw_analysis->statistics().means[0]==26448.);
                check(result.raw_analysis->pixel_width_um()==2. && result.raw_analysis->pixel_height_um()==3.);
                const auto grid_record=result.record_id;auto retained_grid=result.raw_analysis;endpoint.take(ticket);
                check(endpoint.stats().retained_bytes==0 && doc->raw_analysis_record_count()==previous_records+1);
                ticket=endpoint.verify_raw_analysis(image,grid_record);result=test.until(controller,endpoint,ticket);
                check(result.state=="succeeded" && result.verified && !result.committed);endpoint.take(ticket);
                sampling.samples_x=1;options.channels=1;
                ticket=endpoint.analyze_raw_grid(image,object,sampling,options,false,true);test.step(controller);
                doc->set_calibration(3.,4.);
                check(test.until(controller,endpoint,ticket).state=="stale" && doc->raw_analysis_record_count()==previous_records+1);endpoint.take(ticket);
                ticket=endpoint.analyze_raw_grid(image,object,sampling,options,false,true);test.step(controller);check(endpoint.cancel(ticket));
                check(test.until(controller,endpoint,ticket).state=="cancelled" && doc->raw_analysis_record_count()==previous_records+1);endpoint.take(ticket);
                options.work_bytes=1;ticket=endpoint.analyze_raw_grid(image,object,sampling,options,false,true);
                check(test.until(controller,endpoint,ticket).state=="failed" && doc->raw_analysis_record_count()==previous_records+1);endpoint.take(ticket);
                check(endpoint.stats().retained_bytes==0 && retained_grid->statistics().means[0]==26448.);
                std::cout<<"Raw grid viewer copied counts/options, owner calibration, retention/replay, cancellation and stale delivery passed\n";
            }
            doc->server()->close();fs::remove(input);
            ticket=endpoint.export_raw_measurements(image,(root/"historical.csv").u8string());result=test.until(controller,endpoint,ticket);check(result.state=="succeeded" && result.committed);endpoint.take(ticket);
            check(retained_raw->statistics().means[0]==26448. && retained_raw->source()->primary);
            controller.finish(test.context);
        }
        fs::remove_all(root);std::cout<<"Viewer queue, owner dispatch, jobs, lifetime, limits, staleness and shutdown passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
