#pragma once
#include "gui_analysis_inputs_checks.hpp"
#include "gui_output_gate_checks.hpp"

namespace notifications_checks {
using namespace qpx;
struct Scope {
    ui::NotificationsTrace trace;ui::NotificationsTrace* old=ui::notifications_trace;
    Scope(){ui::notifications_trace=&trace;}~Scope(){ui::notifications_trace=old;}
};
struct Language {
    std::string previous=std::getenv("QUPATHXX_UI_LANGUAGE")?std::getenv("QUPATHXX_UI_LANGUAGE"):"";
    explicit Language(bool french) {
#ifdef _WIN32
        _putenv_s("QUPATHXX_UI_LANGUAGE",french?"fr":"en");
#else
        setenv("QUPATHXX_UI_LANGUAGE",french?"fr":"en",1);
#endif
    }
    ~Language(){
#ifdef _WIN32
        _putenv_s("QUPATHXX_UI_LANGUAGE",previous.c_str());
#else
        if(previous.empty())unsetenv("QUPATHXX_UI_LANGUAGE");else setenv("QUPATHXX_UI_LANGUAGE",previous.c_str(),1);
#endif
    }
};
inline std::size_t count(const ui::Notifications& n,const char* source,ui::Severity severity) {
    std::size_t result=0;for(std::size_t i=0;i<n.size();++i)if(n.newest(i).notice.source==source && n.newest(i).notice.severity==severity)++result;return result;
}
inline void model(const std::filesystem::path& dir) {
    ui::Notifications n;
    n.post({ui::Severity::Error,"Annotations","Native failure %s",{}});const auto first=n.current();
    check(first->text=="Annotations: Native failure %s" && !first->notice.message_key,"Native sentence was formatted or translated");
    {ui::NoticeException<std::invalid_argument> keyed("Unlock ROIs before merging");const auto message=ui::exception_notice("Annotations",keyed);check(message.message_key && message.message=="Unlock ROIs before merging","UI guard lost its typed catalogue key");}
    n.post({ui::Severity::Success,"Tables","The operation completed.",{},true});check(n.current()==first,"Unrelated success cleared standing error");
    n.post({ui::Severity::Success,"Annotations","The operation completed.",{},true});check(n.current()->notice.severity==ui::Severity::Success,"Same-source success did not replace error");
    const auto previous=n.current();const auto posted=n.posted();
    n.post({ui::Severity::Error,"Annotations","Foreground failure",{}});
    const auto history=n.size();n.resolve("Image");check(n.current()->notice.severity==ui::Severity::Error && n.posted()==posted+1,"Other-source resolution removed standing error or posted");
    n.resolve("Annotations");check(n.current()==previous && n.size()==history && n.newest(0).notice.message=="Foreground failure" && n.posted()==posted+1,"Resolution changed history, posted, or failed to restore previous notice");
    n.resolve("Annotations");check(n.current()==previous && n.posted()==posted+1,"Resolution changed a non-error notice");
    n.post({ui::Severity::Error,"Annotations","First failure",{}});n.post({ui::Severity::Error,"Project","Second failure",{}});check(n.current()->notice.source=="Project","Newest error did not replace standing error");n.dismiss_errors();check(n.current() && n.current()->notice.severity!=ui::Severity::Error,"Dismiss resurrected an old error");
    ui::Notifications bounded;for(int i=0;i<60;++i)bounded.post({ui::Severity::Info,"Project",std::to_string(i),{}});
    check(bounded.size()==50 && bounded.newest(0).notice.message=="59" && bounded.newest(49).notice.message=="10","Activity does not retain newest 50 of 60 in order");
    for(std::size_t i=0;i<50;++i)check(bounded.newest(i).notice.message==std::to_string(59-i),"Activity order differs");
    check(ui::Notifications::color(ui::Severity::Error).x==ui::design::color(ui::design::colors().error).x,"Error color does not use design token");
    output_gate_checks::Host host;const auto path=std::filesystem::absolute(dir/"notice.csv").lexically_normal();write(path,"csv\n");
    host.gate.succeeded(path.u8string());host.frame();check(host.notices.posted()==1 && host.notices.current()->notice.path==path.u8string(),"Export success lost its normal absolute path");
    check(!ImGui::FindWindowByName(ui::i18n::label("Export complete").c_str()),"Export complete window remains");
    const auto normal=ui::notice_path("\\\\?\\"+path.u8string());check(normal==path.u8string() && normal.rfind("\\\\?\\",0)!=0,"Extended prefix reaches notice path");
    Rect copy,folder,activity;
    host.panel=[&]{ImGui::SetNextWindowSize({1200,180});ImGui::Begin("Notification status");
        ui::LayoutTrace trace;auto* previous=ui::layout_trace;ui::layout_trace=&trace;host.notices.draw_status_bar();ui::layout_trace=previous;
        for(const auto& item:trace.items){const auto& r=item.rect;Rect box{r.Min.x,r.Min.y,r.GetWidth(),r.GetHeight()};
            if(item.command=="Activity")activity=box;else if(item.command=="Copy path" || item.command=="Dismiss error")copy=box;else if(item.command=="Open folder")folder=box;}
        ImGui::End();host.notices.draw_activity();};
    host.frame();host.frame();
    auto click=[&](Rect box){auto& io=ImGui::GetIO();io.AddMousePosEvent(float(box.x+box.width/2),float(box.y+box.height/2));host.frame();io.AddMouseButtonEvent(0,true);host.frame();io.AddMouseButtonEvent(0,false);host.frame();};
    click(copy);check(std::string(ImGui::GetClipboardText())==path.u8string(),"Copy path did not put normal absolute path on clipboard");
    bool opened=false;const auto previous_shell=ui::shell_open_override;ui::shell_open_override=[&](const std::string& value){check(std::filesystem::equivalent(std::filesystem::u8path(value),path.parent_path()),"Open folder chose wrong file");opened=true;};click(folder);ui::shell_open_override=previous_shell;check(opened,"Open folder did not invoke shell action");
    host.notices.post({ui::Severity::Error,"Annotations","A native failure",{}});host.frame();click(copy);check(host.notices.current()->notice.severity!=ui::Severity::Error,"Dismiss error control did not dismiss standing error");
    click(activity);host.frame();const auto* activity_window=ImGui::FindWindowByName(ui::i18n::label("Activity").c_str());check(activity_window && activity_window->Active,"Activity control did not open session history");
    ui::NoticeJob job;ui::Notifications jobs;
    job.observe(jobs,"Tables","running",false,false);job.observe(jobs,"Tables","failed",false,true,"Native table failure");
    for(int i=0;i<10;++i)job.observe(jobs,"Tables","failed",false,true,"Native table failure");
    check(jobs.posted()==1,"Unchanged failure posts every frame");job.observe(jobs,"Tables","running",false,false);job.observe(jobs,"Tables","cancelled",false,false);check(jobs.posted()==1,"User cancellation posted a notice");
    std::cout<<"Notifications: standing precedence/dismissal; 60/50 order; native prose literal; export receipt/prefix; one failure in ten frames; cancel silent passed\n";
}
inline void table(const std::filesystem::path& dir) {
    output_gate_checks::Host host;Document doc(Image::demo());const auto id=doc.add_rectangle({0,0,12,12});
    const AnalysisRequest request(doc,id,StainTransform({{1,0,0},{0,1,0}}));doc.keep_analysis(request,analyze(request));
    MeasurementTableExportJob job;const auto path=(std::filesystem::absolute(dir)/"background-table.csv").u8string();
    host.gate.submit(path,detail::output_scope("Tables"),[&](const std::string& output,ExportWritePolicy policy,const std::vector<std::string>& protected_paths){
        check(job.request_with_policy(MeasurementTableRequest(doc,MeasurementTableKind::Roi),output,policy,protected_paths),"Native table did not start");
        host.gate.watch(output,[&]()->std::optional<bool>{const auto state=job.poll();if(job.busy())return {};return state.state=="succeeded";},[&]{return job.poll().error;});
    },[](const std::string& message){throw std::runtime_error(message);});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(host.notices.posted()==0){check(std::chrono::steady_clock::now()<deadline,"Native table notification deadline");host.frame();std::this_thread::yield();}
    for(int i=0;i<10;++i)host.frame();check(host.notices.posted()==1 && host.notices.current()->notice.severity==ui::Severity::Success && host.notices.current()->notice.path==path && std::filesystem::is_regular_file(std::filesystem::u8path(path)),"Native table export did not post exactly one path receipt");
    std::cout<<"Native table export: one completion and path across ten frames passed\n";
}
inline void training(const std::filesystem::path& dir,bool cancel) {
    analysis_inputs_checks::Scene scene(dir,cancel?"notice-training-cancel":"notice-training",3);
    Scope observer;bool started=false,stopped=false,finished=false;int idle=0;
    observer.trace.after_frame=[&](ui::Notifications& n){
        const auto state=scene.inputs.jobs[3].state;
        if(!started && scene.snapshot.scope.trace.actions.count("Train labelled areas") && scene.snapshot.scope.trace.actions.at("Train labelled areas").enabled){scene.start(3,cancel);started=true;}
        if(cancel && started && scene.server->entered && !stopped){scene.activate="Cancel pixel work";stopped=true;}
        if(cancel && (state=="cancelling" || state=="cancelled"))scene.server->hold=false;
        if(started && (state=="ready" || state=="cancelled")){
            finished=true;const auto source=ui::command(ui::CommandId::Tool_PixelClassifier).caption;
            check(count(n,source,ui::Severity::Success)==(cancel?0u:1u),"Pixel training completion notice count differs");
            if(++idle==10)scene.commands.end_session=true;
        }
    };
    const auto r=scene.run();check(finished && r.pixel_training_state==(cancel?"cancelled":"ready"),"Native pixel training did not reach expected terminal state");
    std::cout<<"Native pixel training "<<(cancel?"cancel silent":"one completion across ten frames")<<" passed\n";
}
inline void batch(const std::filesystem::path& dir,bool cancel) {
    analysis_inputs_checks::Scene scene(dir,cancel?"notice-batch-cancel":"notice-batch");
    if(cancel)for(int i=0;i<999;++i){auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));doc->add_rectangle({0,0,16,16});scene.project->add_document(doc,"Batch "+std::to_string(i));}
    scene.options.batch_output_directory=(dir/(cancel?"batch-cancel":"batch-complete")).u8string();
    Scope observer;bool started=false,stopped=false,finished=false;int idle=0;
    observer.trace.after_frame=[&](ui::Notifications& n){
        if(!started && scene.snapshot.frame>3){scene.pending=GuiInputKind::RunBatchKey;started=true;}
        if(cancel && scene.commands.batch_busy && scene.commands.batch_state=="running" && !stopped){scene.pending=GuiInputKind::CancelBatchKey;stopped=true;}
        const auto state=scene.commands.batch_state;
        if(started && (state=="succeeded" || state=="cancelled")){
            finished=true;const auto source=ui::command(ui::CommandId::Tool_Batch).caption;
            check(count(n,source,ui::Severity::Success)==(cancel?0u:1u),"Batch completion notice count differs");if(++idle==10)scene.commands.end_session=true;
        }
    };
    const auto r=scene.run();check(finished && r.batch_state==(cancel?"cancelled":"succeeded"),"Native batch did not reach expected terminal state");
    std::cout<<"Native batch "<<(cancel?"cancel silent":"one completion across ten frames")<<" passed\n";
}
inline void inline_failure(const std::filesystem::path& dir) {
    analysis_inputs_checks::Scene scene(dir,"notice-inline");
    scene.edit=[](detail::AnalysisInputs& value){value.kind=1;value.vectors={1,0,0,2,0,0};value.edited(detail::AnalysisInputs::Kind|detail::AnalysisInputs::Vectors);};
    Scope observer;std::string message;int stable=0;
    observer.trace.after_frame=[&](ui::Notifications& n){if(!n.current() || n.current()->notice.severity!=ui::Severity::Error)return;
        message=n.current()->notice.message;check(n.current()->notice.source==ui::workflow_task_labels[1] && count(n,ui::workflow_task_labels[1],ui::Severity::Error)==1,"Inline failure posted more than once");if(++stable==10)scene.commands.end_session=true;};
    scene.run();check(stable==10 && !message.empty() && scene.snapshot.text.find(message)!=std::string::npos,"Inline failure disappeared from beside its controls");
    std::cout<<"Actual inline input failure: retained beside action and exactly one notice across ten frames passed\n";
}
inline void retry(const std::filesystem::path& dir) {
    analysis_inputs_checks::Scene scene(dir,"notice-retry");
    for(const auto& [id,a]:scene.doc->objects())scene.doc->set_object_edit_state(id,true,{},{});
    Scope observer;bool navigate=false,release=false,replaced=false;int stage=0;std::uint64_t posted=0;std::vector<GuiInput> clicks;
    const auto before=scene.snapshot.before;scene.snapshot.before=[&,before](int frame){if(before)before(frame);
        if(release){dispatch_checks::physical(ui::command(ui::CommandId::Header_AnnotationsPanel).chord,false);release=false;}
        if(frame>1 && !navigate){dispatch_checks::physical(ui::command(ui::CommandId::Header_AnnotationsPanel).chord,true);navigate=true;release=true;}};
    const auto replay=scene.commands.replay_input;scene.commands.replay_input=[&,replay](const auto& emit){replay(emit);if(!clicks.empty()){emit(clicks.front());clicks.erase(clicks.begin());}};
    observer.trace.after_frame=[&](ui::Notifications& n){
        if(stage==0 && scene.snapshot.scope.trace.actions.count("Object name")){posted=n.posted();scene.pending=GuiInputKind::BooleanKey;stage=1;}
        else if(stage==1 && n.current() && n.current()->notice.severity==ui::Severity::Error){
            check(n.current()->notice.message_key && n.current()->notice.message=="Unlock the ROI before editing its geometry","UI guard was not posted as its catalogue key");
            for(auto kind:{GuiInputKind::LeftDown,GuiInputKind::LeftUp}){GuiInput event;event.kind=kind;event.target=GuiInputTarget::ToggleObjectLock;event.x=event.y=.5;event.wait_for=GuiInputWait::TargetReady;clicks.push_back(event);}stage=2;
        }else if(stage==2 && clicks.empty() && ImGui::GetActiveID()==0 && !n.current()){
            check(n.posted()==posted+1 && count(n,"Annotations",ui::Severity::Error)==1,"Successful Unlock posted or discarded its error history");
            replaced=true;scene.pending=GuiInputKind::BooleanKey;stage=3;
        }else if(stage==3){for(const auto& [id,a]:scene.doc->objects())if(a.geometry.bounds().width==96){check(!a.locked() && !n.current() && n.posted()==posted+1 && count(n,"Annotations",ui::Severity::Error)==1,"Unlock/retry posted a notice or lost its resolved error history");stage=4;scene.commands.end_session=true;}}
    };
    scene.run();check(stage==4 && replaced,"Unlock instruction did not enable the actual Boolean retry and same-source error replacement");
    std::cout<<"Actual locked ROI error -> public Unlock control -> quiet resolution with history retained -> Boolean retry geometry passed\n";
}
inline void foreground_edits(const std::filesystem::path& dir) {
    for(bool previous:{false,true}) {
        Document doc(Image(64,48,std::vector<std::uint8_t>(64*48*3,80)));
        auto opts=::options(dir,previous?"quiet-edits-previous":"quiet-edits-idle","annotations");
        opts.settle_tiles=false;opts.inputs={key(2,GuiInputKind::RectangleKey)};
        for(int i=0;i<3;++i){GuiInput down;down.frame=4+3*i;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Image;down.x=2+16*i;down.y=4;auto up=down;up.frame++;up.kind=GuiInputKind::LeftUp;up.x+=10;up.y+=12;opts.inputs.push_back(down);opts.inputs.push_back(up);}
        opts.inputs.push_back(key(14,GuiInputKind::UndoKey));
        Scope observer;bool initialized=false,saw_three=false;std::uint64_t posted=0;const ui::Notifications::Entry* current=nullptr;
        observer.trace.before_frame=[&](ui::Notifications& n){if(!initialized){if(previous)n.post({ui::Severity::Info,"Project","Previous notice",{}});posted=n.posted();current=n.current();initialized=true;}};
        observer.trace.after_frame=[&](ui::Notifications& n){saw_three=saw_three || doc.object_count()==3;check(n.posted()==posted && n.current()==current,"Rectangle/Undo posted or changed idle/previous status");};
        const auto report=run_gui(doc,24,"",opts);
        check(report.replay_complete && saw_three && doc.object_count()==2,"Three rectangle draws and one Undo were not exercised");
    }
    std::cout<<"Three real rectangle draws and Undo: no posts; idle/previous status preserved passed\n";
    analysis_inputs_checks::Scene scene(dir,"quiet-other-source");
    scene.doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{1,0,0},{0,1,0}},{190,200,210},"Stored profile",{"First stain","Second stain"}));
    scene.options.analysis_kind=0;
    GuiInput down;down.frame=6;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::UseImageStains;down.wait_for=GuiInputWait::TargetReady;auto up=down;up.frame=7;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;scene.options.inputs={down,up};
    Scope observer;const ui::Notifications::Entry* error=nullptr;bool applied=false;
    observer.trace.before_frame=[&](ui::Notifications& n){if(!error){n.post({ui::Severity::Error,"Annotations","Annotation failure",{}});error=n.current();}};
    scene.inputs.after_frame=[&](const detail::AnalysisInputs& value,const auto&){if(value.kind==1 && value.white==std::vector<double>{190,200,210}){applied=true;scene.commands.end_session=true;}};
    observer.trace.after_frame=[&](ui::Notifications& n){check(n.current()==error && n.posted()==1,"Image profile edit removed another source's error or posted");};
    const auto report=scene.run();check(applied && report.replay_complete,"Other-source profile edit was not exercised");
    std::cout<<"Annotations error survives successful Image profile edit; no new notice passed\n";
}
inline void stain_failure(const std::filesystem::path& dir) {
    analysis_inputs_checks::Scene scene(dir,"notice-stain-failure");
    scene.options.stain_model_path=scene.options.layout_path;
    Scope observer;bool requested=false;int stable=0;std::string message;std::vector<GuiInput> clicks;
    const auto replay=scene.commands.replay_input;scene.commands.replay_input=[&,replay](const auto& emit){replay(emit);if(!clicks.empty()){emit(clicks.front());clicks.erase(clicks.begin());}};
    observer.trace.after_frame=[&](ui::Notifications& n){
        if(!requested && scene.snapshot.frame>3){for(auto kind:{GuiInputKind::LeftDown,GuiInputKind::LeftUp}){GuiInput e;e.kind=kind;e.target=GuiInputTarget::SaveStainModel;e.x=e.y=.5;e.wait_for=GuiInputWait::TargetReady;clicks.push_back(e);}requested=true;}
        if(!n.current() || n.current()->notice.source!="Stain model" || n.current()->notice.severity!=ui::Severity::Error)return;
        message=n.current()->notice.message;check(count(n,"Stain model",ui::Severity::Error)==1,"Refused stain-model export posted more than one error");if(++stable==10)scene.commands.end_session=true;
    };
    const auto r=scene.run();check(requested && stable==10 && r.stain_model_state=="failed" && !message.empty() && scene.snapshot.text.find(r.stain_model_error)!=std::string::npos,"Stain export refusal lost its inline/native report error");
    std::cout<<"Actual protected stain export: inline/report retained, one error across ten frames passed\n";
}
inline void mean_source(const std::filesystem::path& dir) {
    struct Reader:ImageServer {
        std::shared_ptr<ImageServer> inner;std::thread::id ui=std::this_thread::get_id();
        explicit Reader(std::shared_ptr<ImageServer> value):inner(std::move(value)){}
        const ImageMetadata& metadata()const override{return inner->metadata();}
        PixelBuffer read_region(const RegionRequest& r)const override{if(std::this_thread::get_id()!=ui && r.width==44 && r.height==60)throw std::runtime_error("Native mean read failed.");return inner->read_region(r);}
        void close()override{inner->close();}bool closed()const override{return inner->closed();}
    };
    analysis_inputs_checks::Scene scene(dir,"notice-mean-source");
    scene.server->inner=std::make_shared<Reader>(scene.server->inner);
    scene.options=::options(dir,scene.name,"workflow");scene.options.width=1600;scene.options.height=1400;scene.options.settle_tiles=false;scene.options.inputs={key(2,GuiInputKind::SelectAllKey)};
    bool requested=false;int stable=0;Scope observer;
    const auto before=scene.commands.before_context;
    scene.commands.before_context=[&,before](int frame){before(frame);if(!requested && frame>3){dispatch_checks::activate(dispatch_checks::task_body(),"Measure mean RGB");requested=true;}};
    observer.trace.after_frame=[&](ui::Notifications& n){
        if(!n.current() || n.current()->notice.message!="Native mean read failed.")return;
        check(n.current()->notice.source==ui::workflow_task_labels[0] && !n.current()->notice.message_key && count(n,ui::workflow_task_labels[0],ui::Severity::Error)==1,"Mean read failure did not post once from Measurements as literal native text");
        if(++stable==10)scene.commands.end_session=true;
    };
    scene.run();check(requested && stable==10 && scene.snapshot.text.find("Native mean read failed.")!=std::string::npos,"Mean error disappeared from its own inline control");
    std::cout<<"Native mean failure: Measurements source, inline retained, one literal notice across ten frames passed\n";
}
inline void cells_source(const std::filesystem::path& dir) {
    for(bool docked:{false,true}) {
        analysis_inputs_checks::Scene scene(dir,docked?"notice-cells-docked":"notice-cells-compact",2);
        scene.options.docked_workspace=docked;if(!docked)scene.options.layout_path.clear();
        const auto path=std::filesystem::absolute(dir/(docked?"cells-docked.csv":"cells-compact.csv")).lexically_normal().u8string();
        scene.options.cell_table_path=path;scene.options.cell_table_published_objects=false;
        Scope observer;int stage=0,stable=0;std::optional<std::string> action;
        scene.describe=[&]{return "stage="+std::to_string(stage)+" state="+scene.inputs.jobs[2].state+" records="+std::to_string(scene.doc->cell_record_count());};
        const auto pane_for_action=[&]{auto* pane=dispatch_checks::task_body();if(!docked)if(auto* parent=ImGui::FindWindowByName("Workspace"))for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->ParentWindow==parent && window->ChildId==ImHashStr("Inspector",0,parent->ID))pane=window;return pane;};
        const auto before_context=scene.commands.before_context;
        scene.commands.before_context=[&,before_context](int frame){before_context(frame);if(!action)return;
            auto* pane=pane_for_action();
            std::cout<<"Cells source activation "<<*action<<" pane="<<(pane?pane->Name:"missing")<<'\n';dispatch_checks::activate(pane,ui::command_label(ui::workflow_command(action->c_str()).id));action.reset();};
        const auto available=[&](const char* key){const auto i=scene.snapshot.scope.trace.actions.find(key);if(i==scene.snapshot.scope.trace.actions.end())return false;
            if(!i->second.enabled && !i->second.disabled)if(auto* pane=pane_for_action())ImGui::SetScrollY(pane,float(i->second.bounds.y-pane->Pos.y+pane->Scroll.y-pane->Size.y*.5));
            return i->second.enabled;};
        observer.trace.after_frame=[&](ui::Notifications& n){
            if(stage==0 && scene.snapshot.frame>3){scene.start(2);stage=1;std::cout<<"Cells source detection requested\n";}
            else if(stage==1 && scene.inputs.jobs[2].state=="ready" && available("Keep cell run (Ctrl+Shift+C)")){action="Keep cell run (Ctrl+Shift+C)";stage=2;std::cout<<"Cells source preview ready\n";}
            else if(stage==2 && scene.doc->cell_record_count() && available("Export cell table (Ctrl+Alt+X)")){action="Export cell table (Ctrl+Alt+X)";stage=3;std::cout<<"Cells source run kept\n";}
            else if(stage==3 && std::filesystem::is_regular_file(std::filesystem::u8path(path))){
                std::size_t receipts=0;for(std::size_t i=0;i<n.size();++i)if(n.newest(i).notice.path==path){++receipts;check(n.newest(i).notice.source==ui::workflow_task_labels[3] && n.newest(i).notice.severity==ui::Severity::Success,"Cells export lost its originating task");}
                if(!receipts)return;check(receipts==1,"Cells export duplicated its receipt");if(++stable==10){stage=4;scene.commands.end_session=true;}
            }
        };
        scene.run();check(stage==4,"Cells export source scene did not finish");
    }
    std::cout<<"Cells-origin native export: one receipt from its task in compact/docked layouts passed\n";
}
inline void scenes(const std::filesystem::path& dir) {
    for(bool french:{false,true})for(bool docked:{false,true})for(int mode=0;mode<3;++mode) {
        const auto name=std::string("notice-")+(french?"fr200-":"en-")+(docked?"docked-":"compact-")+std::to_string(mode);
        Language language(french);
        analysis_inputs_checks::Scene scene(dir,name,0,french);scene.options.docked_workspace=docked;
        if(!docked)scene.options.layout_path.clear();
        scene.options.width=french?2400:1600;scene.options.height=french?1600:1000;
        scene.options.measurement_table_path=(dir/(name+".csv")).u8string();
        const auto first=scene.doc->objects().begin()->first;const AnalysisRequest analysis(*scene.doc,first,StainTransform({{1,0,0},{0,1,0}}));scene.doc->keep_analysis(analysis,analyze(analysis));
        for(const auto& [id,a]:scene.doc->objects())scene.doc->set_object_edit_state(id,true,{},{});
        Scope observer;ui::Notifications* notices=nullptr;bool sent=false,failed=false,exported=false,opened=false;int stable=0;std::uint64_t error_sequence=0;
        bool annotations_requested=false,release_annotations=false;const auto before=scene.snapshot.before;
        scene.snapshot.before=[&,before](int frame){if(before)before(frame);
            if(release_annotations){dispatch_checks::physical(ui::command(ui::CommandId::Header_AnnotationsPanel).chord,false);release_annotations=false;}
            if(docked && frame>1 && !annotations_requested){dispatch_checks::physical(ui::command(ui::CommandId::Header_AnnotationsPanel).chord,true);annotations_requested=true;release_annotations=true;}
        };
        observer.trace.before_frame=[&](ui::Notifications& n){notices=&n;};
        observer.trace.after_frame=[&](ui::Notifications& n){
            if(!sent && (docked?scene.snapshot.scope.trace.actions.count("Object name")>0:scene.snapshot.frame>3)){scene.pending=GuiInputKind::BooleanKey;sent=true;}
            if(!failed && n.current() && n.current()->notice.severity==ui::Severity::Error){
                check(n.current()->notice.source=="Annotations","Annotations error has wrong source");failed=true;error_sequence=n.current()->sequence;scene.pending=GuiInputKind::ExportMeasurementTableKey;
            }
            if(failed)for(std::size_t i=0;i<n.size();++i)if(n.newest(i).notice.severity==ui::Severity::Success && !n.newest(i).notice.path.empty())exported=true;
            if(failed && exported){
                if(mode==0)check(n.current()->sequence==error_sequence,"Unrelated export removed standing Annotations error");
                if(mode==1 && !opened){n.dismiss_errors();opened=true;}
                if(mode==2 && !opened)opened=true;
                if(mode==2){bool show=true;n.draw_activity(&show);}
                if(++stable==10)scene.commands.end_session=true;
            }
        };
        const auto r=scene.run(true);check(failed && exported && notices && r.ui_overflow_items==0,"Notification scene did not reach error/export/capture with zero overflow");
    }
}
inline void run(const std::filesystem::path& dir,const std::string& part) {
    std::filesystem::create_directories(dir);
    if(part=="all" || part=="model")model(dir);
    if(part=="all" || part=="table")table(dir);
    if(part=="all" || part=="training"){training(dir,false);training(dir,true);}
    if(part=="all" || part=="batch"){batch(dir,false);batch(dir,true);}
    if(part=="all" || part=="inline")inline_failure(dir);
    if(part=="all" || part=="retry")retry(dir);
    if(part=="all" || part=="foreground")foreground_edits(dir);
    if(part=="all" || part=="stain-failure")stain_failure(dir);
    if(part=="all" || part=="mean-source")mean_source(dir);
    if(part=="all" || part=="cells-source")cells_source(dir);
    if(part=="all" || part=="scenes")scenes(dir);
}
}
