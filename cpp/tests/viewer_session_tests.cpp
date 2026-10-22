#include "../viewer_bridge.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <future>
#include <random>
#include <thread>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("viewer session check failed");}
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
int main() {
    namespace fs=std::filesystem;const auto root=fs::current_path()/("viewer-test-"+std::to_string(std::random_device{}()));
    try {
        fs::create_directory(root);ViewerLimits limits;limits.validate();limits.tickets=0;rejects([&]{limits.validate();});
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
        fs::remove_all(root);std::cout<<"Viewer queue, owner dispatch, jobs, lifetime, limits, staleness and shutdown passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
