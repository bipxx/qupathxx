#pragma once
#include "gui_reasons_help_checks.hpp"
#include "../gui_analysis_inputs.hpp"

namespace analysis_inputs_checks {
using Inputs=detail::AnalysisInputs;using Consumer=detail::InputConsumer;using Feedback=detail::AnalysisInputFeedback;
inline constexpr unsigned masks[]={63,14,6,6,12};
inline constexpr GuiInputKind starts[]={GuiInputKind::AnalyzeKey,GuiInputKind::DetectTissueKey,GuiInputKind::DetectCellsKey,GuiInputKind::SelectAllKey,GuiInputKind::EstimateStainsKey};
inline void capture_view(const std::filesystem::path& path) {
    check(ui::layout_stats.overflow_items==0,"Captured input view overflow: "+ui::layout_stats.first_overflow);
    dialog_checks::capture(path,glfwGetCurrentContext());std::cout<<"Capture overflow=0 "<<path.filename().u8string()<<'\n';
}
struct Reader:ImageServer {
    std::shared_ptr<ImageServer> inner;std::thread::id ui=std::this_thread::get_id();
    mutable std::atomic<bool> hold{false},entered{false};bool training=false;
    Reader(){const StainTransform truth({{.65,.70,.29},{.07,.99,.11}});std::vector<std::uint8_t> rgb(128*96*3);
        for(int y=0;y<96;++y)for(int x=0;x<128;++x)for(int c=0;c<3;++c){const auto a=x%8==0?0.:.1+.6*x/127,b=x%8==1?0.:.1+.7*y/95;rgb[(y*128+x)*3+c]=std::uint8_t(std::round(255*std::pow(10.,-a*truth.stains()[0][c]-b*truth.stains()[1][c])));}
        inner=dense_image_server(Image(128,96,std::move(rgb)));
    }
    const ImageMetadata& metadata()const override{return inner->metadata();}
    PixelBuffer read_region(const RegionRequest& r)const override {
        const bool job=(training && r.width==16 && r.height==16) || ((r.x==8 || r.x==60) && r.y==8 && r.width==44 && r.height==60);
        if(std::this_thread::get_id()!=ui && job && hold){entered=true;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);
            while(hold){if(std::chrono::steady_clock::now()>end){hold=false;throw std::runtime_error("Shared-input reader deadline");}std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
        return inner->read_region(r);
    }
    void close()override{inner->close();}bool closed()const override{return inner->closed();}
};
struct Scene {
    std::filesystem::path out;std::string name;std::shared_ptr<Reader> server=std::make_shared<Reader>();
    std::shared_ptr<Document> doc=std::make_shared<Document>(server);std::shared_ptr<Project> project=std::make_shared<Project>();GuiOptions options;
    analysis_tasks_checks::Snapshot snapshot;ui::CommandTrace commands;detail::AnalysisInputsTrace inputs;
    ui::CommandTrace* old_commands=ui::command_trace;detail::AnalysisInputsTrace* old_inputs=detail::analysis_inputs_trace;
    std::optional<GuiInputKind> pending;std::optional<std::string> activate;std::function<void(Inputs&)> edit;
    std::function<std::string()> describe;
    std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    Scene(const std::filesystem::path& directory,std::string stem,int consumer=0,bool french=false):out(directory),name(std::move(stem)) {
        std::filesystem::create_directories(out);const auto source=out/(name+".ppm");const auto pixels=server->inner->read_region({0,0,0,128,96}).bytes();std::string rgb;for(std::size_t i=0;i<pixels.size();i+=4)rgb.append(reinterpret_cast<const char*>(pixels.data()+i),3);write(source,"P6\n128 96\n255\n"+rgb);
        doc->set_class("A",0xff0000);doc->set_class("B",0x00ff00);const auto a=doc->add_rectangle({8,8,44,60},"A"),b=doc->add_rectangle({60,8,44,60},"B");doc->apply_classifications({{a,"A"},{b,"B"}});
        project->add_document(doc,"Shared input fixture",source.u8string());
        options=::options(out,name,"workflow",consumer==1?"tissue":consumer==2?"cells":"region");options.width=french?2400:1600;options.height=french?2200:1400;options.settle_tiles=false;options.inputs={key(2,GuiInputKind::SelectAllKey)};
        options.pixel_open=consumer==3;auto p=std::make_shared<PixelTrainingOptions>();p->tile_size=16;p->features.sigmas={0};options.pixel_training_options=p;auto f=std::make_shared<ForestParameters>();f->trees=3;f->max_depth=3;options.pixel_parameters=f;
        CellParameters c;c.sigma=0;options.cell_parameters=std::make_shared<const CellParameters>(c);
        options.analysis_kind=consumer==4?1:0;
        if(french){options.ui_scale=2;auto ini=read(std::filesystem::u8path(options.layout_path));ini.replace(ini.find("language=en"),11,"language=fr");write(std::filesystem::u8path(options.layout_path),ini);}
        ui::command_trace=&commands;detail::analysis_inputs_trace=&inputs;
        snapshot.before=[&](int){check(std::chrono::steady_clock::now()<deadline,"Shared input scene timed out: "+name+(describe?" / "+describe():std::string{}));};
        commands.replay_input=[&](const auto& emit){if(pending){emit(key(0,*pending));pending.reset();}};
        commands.before_context=[&](int){
            if(auto* pixel=ImGui::FindWindowByName(ui::tool_window_label(ui::ToolId::PixelClassifier))){ImGui::SetWindowPos(pixel,{20,80});ImGui::SetWindowSize(pixel,options.ui_scale>1?ImVec2{1100,1500}:ImVec2{700,1100});}
            if(activate){auto* pane=dispatch_checks::task_body();const char* label=activate->c_str();
            if(*activate=="##Task")pane=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);
            else if(activate->front()=='@'){label=activate->c_str()+1;for(const auto& popup:ImGui::GetCurrentContext()->OpenPopupStack)if(popup.Window && popup.Window->WasActive)pane=popup.Window;}
            else if(*activate=="Train labelled areas" || *activate=="Cancel pixel work"){pane=ImGui::FindWindowByName(ui::tool_window_label(ui::ToolId::PixelClassifier));label=ui::command_label(ui::workflow_command(label).id);}
            else if(activate->find("###")==std::string::npos && *activate!="Intensity statistics" && *activate!="Load stain model")label=ui::command_label(ui::workflow_command(label).id);
            if(*activate=="Intensity statistics" && pane){auto& g=*ImGui::GetCurrentContext();const auto id=ImHashStr("##value",0,ImHashStr("Intensity statistics",0,pane->ID));ImGui::FocusWindow(pane);g.NavActivateId=g.NavActivateDownId=g.NavActivatePressedId=id;}
            else dispatch_checks::activate(pane,label);activate.reset();}};
        inputs.before_frame=[&](Inputs& value){if(edit){auto apply=std::move(edit);edit={};apply(value);}};
    }
    ~Scene(){server->hold=false;detail::analysis_inputs_trace=old_inputs;ui::command_trace=old_commands;}
    void start(int consumer,bool held=false){server->training=server->training || consumer==3;server->hold=held;server->entered=false;if(consumer==3)activate="Train labelled areas";else pending=starts[consumer];}
    GuiReport run(bool capture=false){
        const auto path=out/(name+"-capture.ppm"),bottom=out/(name+"-capture-bottom.ppm");int phase=0;
        if(capture){const auto previous=inputs.after_render;inputs.after_render=[&,previous]{if(previous)previous();
            if(phase==0 && commands.end_session){capture_view(path);if(auto* pane=dispatch_checks::task_body())ImGui::SetScrollY(pane,pane->ScrollMax.y);commands.end_session=false;phase=1;}
            else if(phase==1){capture_view(bottom);commands.end_session=true;phase=2;}
        };}
        const auto result=run_project_gui(project,100000,{},options);if(capture)check(std::filesystem::exists(path) && std::filesystem::exists(bottom),"State-driven captures were not written");return result;
    }
};
inline void change(Inputs& v,unsigned field) {
    if(field==Inputs::Kind)v.kind=1-v.kind;
    if(field==Inputs::Vectors)v.vectors[0]+=.01;
    if(field==Inputs::White)v.white[0]-=1;
    if(field==Inputs::Nucleus)v.nucleus=!v.nucleus;
    if(field==Inputs::IntensityStatistics)v.intensity_statistics=!v.intensity_statistics;
    if(field==Inputs::RoiSampling)v.roi_sampling=v.roi_sampling==RoiSamplingPolicy::GeometryCoverage?RoiSamplingPolicy::PolygonWinding:RoiSamplingPolicy::GeometryCoverage;
    v.edited(field);
}
inline std::string report_message(const GuiReport& r,int c){return c==0?r.analysis_error:c==1?r.tissue_error:c==2?r.cell_error:c==3?r.pixel_error:r.stain_estimate_error;}
inline bool keep_training_available(const Scene& scene) {
    const auto found=scene.snapshot.scope.trace.actions.find("Keep training cohort");
    return found!=scene.snapshot.scope.trace.actions.end() && found->second.enabled && !found->second.disabled;
}
inline std::size_t occurrences(const std::string& text,const std::string& value) {
    std::size_t count=0,pos=0;while((pos=text.find(value,pos))!=std::string::npos){++count;pos+=value.size();}return count;
}
inline void matrix(const std::filesystem::path& out) {
    for(std::size_t i=0;i<5;++i)check(detail::analysis_input_consumers[i].fields==masks[i],"Consumer declaration differs from independent request-read matrix");
    std::size_t count=0;
    for(bool running:{true,false})for(int c=0;c<5;++c)for(unsigned field:{1u,2u,4u,8u,16u,32u}) {
        Scene scene(out,"t51-matrix-"+std::to_string(c)+"-"+std::to_string(field)+(running?"-running":"-finished"),c);int stage=0;bool done=false,keep_before=false;const bool cancels=(masks[c]&field)!=0 && (c!=3 || running);std::uint64_t generation=0;
        scene.inputs.after_frame=[&](const Inputs&,const Feedback& feedback){const auto& job=scene.inputs.jobs[c];
            if(stage==0 && scene.commands.task>=0 && scene.snapshot.frame>2){scene.start(c,running);stage=1;return;}
            if(stage==1){if(running?(!job.busy || !scene.server->entered):(job.busy || job.state!="ready"))return;generation=job.generation;if(c==3 && !running){keep_before=keep_training_available(scene);check(keep_before && scene.inputs.training_trained && job.has_result,"Finished training fixture lacks its table, trained flag or Keep action");}scene.edit=[&](Inputs& v){change(v,field);};stage=2;return;}
            if(stage==2){check(job.generation==generation,"Input edit started another native job");
                check(feedback.last_edit[c]==cancels,"Input edit notification differs from mask: "+scene.name);
                check(!feedback.last_edit[(c+1)%5],"Idle consumer received a cancellation sentence");
                if(cancels)check(job.state=="cancelling" || job.state=="cancelled","Affected native consumer was not cancelled: "+scene.name+" / "+job.state);
                else check(running?job.busy && job.state=="running":job.has_result && job.state=="ready","Unrelated native work/result was lost: "+scene.name);
                check(feedback.message(static_cast<Consumer>(c),generation).empty()!=cancels,"Consumer message lifetime missing");
                if(c==3 && !running)check(job.state=="ready" && job.has_result && scene.inputs.training_trained && keep_training_available(scene)==keep_before,"Input edit changed finished training data, trained flag or Keep availability");
                // Keep Cells held through construction of the actual replay report.
                if(c==2 && running && !cancels){done=true;scene.commands.end_session=true;stage=4;return;}
                scene.server->hold=false;stage=3;return;
            }
            if(stage==3 && !job.busy){check(cancels?job.state=="cancelled":job.state=="ready","Final native state differs from mask: "+scene.name);done=true;scene.commands.end_session=true;stage=4;}
        };
        scene.snapshot.after=[&](int){if(stage>=3 && c==3 && !running){check(scene.snapshot.text.find(ui::i18n::text(detail::analysis_input_consumers[3].cancelled))==std::string::npos,"Finished training shows an input-cancellation sentence in the editor/window");check(scene.snapshot.text.find("Training: "+ui::caption(ui::parse_run_state("ready"),"ready"))!=std::string::npos,"Finished training lost its ordinary ready status line");}};
        scene.inputs.report=[&](const GuiReport& result){if(c==2 && running && !cancels){check(result.cell_state=="running","Replay report lost unrelated running cell detection");scene.server->hold=false;}};
        const auto result=scene.run();check(done,"Input matrix did not complete");check(report_message(result,c)==(cancels?detail::analysis_input_consumers[c].cancelled:std::string{}),"Replay report lacks exact cancellation sentence: "+scene.name);++count;
        std::cout<<scene.name<<" passed\n";
    }
    std::cout<<"Shared inputs: "<<count<<" actual native running/finished consumer-field cases passed\n";
}
inline void unit_editor(const std::filesystem::path&) {
    simplification_tools_checks::NativeUi host;GuiOptions options;Inputs inputs(options);std::size_t edits=0;inputs.apply_edit=[&](unsigned){++edits;};
    auto draw=[&]{ImGui::Begin("Shared editor");detail::draw_analysis_inputs_editor(inputs,63);ImGui::End();};host.settle(draw);
    check(inputs.revision==0 && edits==0,"Drawing unchanged inputs reported an edit");inputs.edited(0);check(inputs.revision==0,"Empty edit bumped revision");
    bool focus_white=false;auto white=[&]{ImGui::Begin("White editor");if(focus_white){ImGui::SetKeyboardFocusHere();focus_white=false;}detail::draw_analysis_inputs_editor(inputs,Inputs::White);ImGui::End();};host.fit("White editor",700,250,white);
    const auto type=[&](const char* value){focus_white=true;host.settle(white);check(ImGui::GetActiveID()!=0,"White probe did not focus an input");auto& io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);host.frame(white);io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);host.frame(white);io.AddInputCharactersUTF8(value);host.frame(white);io.AddKeyEvent(ImGuiKey_Enter,true);host.frame(white);io.AddKeyEvent(ImGuiKey_Enter,false);host.settle(white);};
    type("255");check(inputs.revision==0 && edits==0,"Writing the same white value notified consumers");type("240");check(inputs.white[0]==240 && inputs.changed_fields==Inputs::White && inputs.revision>0,"White widget did not report its actual changed field");
    inputs=Inputs(options);edits=0;inputs.apply_edit=[&](unsigned){++edits;};
    const StainTransform transform({{.65,.70,.29},{.07,.99,.11}});inputs.use_stains(transform.stains(),transform.white());const auto revision=inputs.revision;inputs.use_stains(transform.stains(),transform.white());
    check(inputs.revision==revision && edits==1,"Identical complete stain assignment cancelled consumers");
    inputs.white[0]=254;inputs.edited(Inputs::White);inputs.use_stains(transform.stains(),{254,255,255});
    check(inputs.changed_fields==Inputs::Vectors,"Refreshing a cached reference reported an unchanged requested White as edited");
    Feedback feedback;feedback.begin(Feedback::Editor::Region);feedback.record(Consumer::Cells,7);check(!feedback.message(Consumer::Cells,7).empty(),"Cancellation not retained");
    check(feedback.report_message(Consumer::Cells,7,"Native failure")=="Native failure\n"+feedback.message(Consumer::Cells,7),"Cancellation overwrote the native error in the replay report");
    check(feedback.report_message(Consumer::Cells,7,"")==feedback.message(Consumer::Cells,7),"Empty native error altered the cancellation sentence");
    feedback.observe(Consumer::Cells,8);check(feedback.message(Consumer::Cells,7).empty(),"Cancellation revived after another generation");
    check(feedback.report_message(Consumer::Cells,8,"Native failure")=="Native failure","Expired cancellation changed the native error");
    std::cout<<"Editor idle/no-op assignment and cancellation generation lifetime passed\n";
}
inline void in_place(const std::filesystem::path& out) {
    for(int c:{0,1,2})for(bool french:{false,true}) {
        Scene scene(out,"t51-editor-"+std::to_string(c)+(french?"-fr200":"-normal"),c,french);scene.options.analysis_kind=0;scene.options.tissue_feature=1;
        auto cells=std::make_shared<CellParameters>();cells->sigma=0;cells->feature=TissueFeature::Stain1;scene.options.cell_parameters=cells;
        int stage=0;bool edited=false,done=false;std::string visible;std::uint64_t revision=0;
        scene.inputs.after_frame=[&](const Inputs& value,const Feedback&){
            if(stage==0 && scene.snapshot.frame>2){scene.activate=c==0?"Advanced inputs###Analysis inputs":"Edit analysis inputs";stage=1;return;}
            if(stage==1){if(value.revision!=0)return;scene.edit=[&](Inputs& v){v.white[0]=240;v.vectors[0]=.81;v.edited(Inputs::White|Inputs::Vectors);};stage=2;return;}
            if(stage==2){check(scene.commands.task==int(c==0?ui::WorkflowTask::Region:c==1?ui::WorkflowTask::Tissue:ui::WorkflowTask::Cells),"Edit inputs left its task");check(value.kind==0,"In-place stain editor changed Region Features");revision=value.revision;stage=3;}
        };
        scene.snapshot.after=[&](int){if(stage!=3)return;visible=scene.snapshot.text;const auto white=ui::i18n::text("White RGB"),stain=ui::i18n::text("Stain 1");
            if(visible.find(white)==std::string::npos || visible.find(stain)==std::string::npos)return;
            check(visible.find("240")!=std::string::npos,"In-place editor does not show request white");check(visible.find("0.81")!=std::string::npos,"Stain request vectors are hidden behind Region Features");
            if(c==1){const StainTransform expected({{.81,scene.options.analysis_vectors[1],scene.options.analysis_vectors[2]},{scene.options.analysis_vectors[3],scene.options.analysis_vectors[4],scene.options.analysis_vectors[5]}},{240,255,255});check(scene.inputs.tissue_request_stains && *scene.inputs.tissue_request_stains==expected.stains(),"Tissue request differs from in-place vectors while Region uses RGB OD");}
            edited=true;done=true;scene.commands.end_session=true;stage=4;
        };
        const auto report=scene.run(true);check(done && edited && revision==1 && report.ui_overflow_items==0,"In-place editor failed/reflow overflow: "+scene.name+" / "+report.ui_overflow_label);write(out/(scene.name+"-visible.txt"),visible);
    }
    std::cout<<"Region/Tissue/Cells inline editors, RGB-OD Region with stain consumers, normal/French 200% passed\n";
}
inline void edit_paths(const std::filesystem::path& out) {
    for(const std::string path:{"editor","estimate","model","profile"})for(bool running:{true,false})for(bool french:{false,true}) {
        Scene scene(out,"t51-path-"+path+(running?"-running":"-finished")+(french?"-fr200":"-normal"),0,french);scene.options.pixel_open=true;
        const StainTransform profile({{.65,.70,.29},{.07,.99,.11}},{245,250,255});
        scene.doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{.65,.70,.29},{.07,.99,.11}},{245,250,255},"H&E",{"Hematoxylin","Eosin"}));
        const auto model=out/(scene.name+".json");save_stain_model(profile,model.u8string());scene.options.stain_model_path=model.u8string();
        const StainTransform old({{1,0,0},{0,1,0}});AnalysisRequest kept(*scene.doc,2,old,AnalysisKind::StainMeans);const auto record=scene.doc->keep_analysis(kept,analyze(kept));const auto saved=scene.doc->analysis_record(record);
        int stage=0,next=0,round=0,capture_stage=0;bool done=false,capture_pending=false;std::uint64_t before=0,load_count=0;unsigned changed=0;std::array<bool,5> expected{};std::string visible;
        scene.describe=[&]{std::string text="stage="+std::to_string(stage)+" next="+std::to_string(next);for(const auto& j:scene.inputs.jobs)text+=" / "+j.state+":"+std::to_string(j.generation);return text;};
        const auto ready=[&](int c){const auto& j=scene.inputs.jobs[c];return running?j.busy:!j.busy && j.state=="ready";};
        const auto apply=[&]{if(path=="editor")scene.activate="Intensity statistics";if(path=="estimate")scene.pending=GuiInputKind::ApplyStainEstimateKey;if(path=="profile")scene.activate="Use saved stains for analysis";if(path=="model"){load_count=scene.inputs.model_loads;scene.activate="Load stain model";}};
        scene.inputs.after_frame=[&](const Inputs& value,const Feedback& feedback){
            if(stage==0 && scene.snapshot.frame>2){if(path=="estimate"){scene.start(4);stage=1;}else stage=2;return;}
            if(stage==1){if(scene.inputs.jobs[4].state!="ready" || scene.inputs.jobs[4].busy)return;stage=2;return;}
            if(stage==2){if(next<5){if(path=="estimate" && next==4){++next;return;}scene.start(next,running);stage=3;return;}stage=4;return;}
            if(stage==3){if(!ready(next))return;++next;stage=2;return;}
            if(stage==4){before=value.revision;apply();stage=5;return;}
            if(stage==5){if(value.revision==before)return;changed=value.changed_fields;
                for(int c=0;c<5;++c)if((changed&masks[c]) && (c!=3 || running) && scene.inputs.jobs[c].state!="cancelling" && scene.inputs.jobs[c].state!="cancelled")return;
                for(int c=0;c<5;++c){expected[c]=(changed&masks[c])!=0 && (c!=3 || running);check(feedback.last_edit[c]==expected[c],"Real edit path disagrees with consumer table: "+scene.name+" / "+std::to_string(c));const auto& j=scene.inputs.jobs[c];
                    if(expected[c])check(j.state=="cancelling" || j.state=="cancelled","Real edit path did not cancel native consumer");else check(j.busy || j.state=="ready","Real edit path dropped unrelated native work");}
                if(!running)check(scene.inputs.jobs[3].has_result && scene.inputs.training_trained && keep_training_available(scene),"Complete-stain assignment changed finished training data, trained flag or Keep");
                check(scene.doc->analysis_record(record).result.input_sha256==saved.result.input_sha256 && scene.doc->analysis_record_count()==1,"Shared edit changed kept record identity");
                if(path=="estimate")check(scene.inputs.estimate_applied,"Use estimate did not mark active vectors");
                scene.server->hold=false;stage=6;return;
            }
            if(stage==6){if(capture_stage<2)return;for(const auto& j:scene.inputs.jobs)if(j.busy)return;stage=7;return;}
            if(stage==7 && round==0){
                if(path=="editor"){before=value.revision;scene.edit=[](Inputs& v){v.edited(0);};stage=10;return;}
                // Restart every cancelled consumer before repeating the identical assignment.
                next=0;round=1;stage=8;return;
            }
            if(stage==8){while(next<5 && !expected[next])++next;if(next==5){before=value.revision;apply();stage=10;return;}scene.start(next,false);stage=9;return;}
            if(stage==9){if(scene.inputs.jobs[next].state!="ready" || scene.inputs.jobs[next].busy)return;++next;stage=8;return;}
            if(stage==10){
                if(path=="model" && scene.inputs.model_loads==load_count)return;
                check(value.revision==before,"Identical assignment reported another edit: "+scene.name);
                for(int c=0;c<5;++c)if(path!="editor" && expected[c])check(scene.inputs.jobs[c].state=="ready" && scene.inputs.jobs[c].has_result && feedback.message(static_cast<Consumer>(c),scene.inputs.jobs[c].generation).empty(),"No-op assignment cancelled a fresh result or revived its sentence");
                done=true;stage=11;scene.commands.end_session=true;
            }
        };
        scene.snapshot.after=[&](int){if(stage==6){visible=scene.snapshot.text;write(out/(scene.name+"-visible.txt"),visible);
            for(int c=0;c<5;++c)if(expected[c])check(visible.find(ui::i18n::text(detail::analysis_input_consumers[c].cancelled))!=std::string::npos,"Cancellation sentence absent from edit surface: "+scene.name);
            if(!running)check(visible.find(ui::i18n::text(detail::analysis_input_consumers[3].cancelled))==std::string::npos,"Finished training was reported cancelled by a real edit path");
            capture_pending=true;
        }};
        scene.inputs.after_render=[&]{if(capture_pending && capture_stage<2){capture_view(out/(scene.name+(capture_stage?"-cancelled-bottom.ppm":"-cancelled.ppm")));if(auto* pane=dispatch_checks::task_body())ImGui::SetScrollY(pane,capture_stage?0:pane->ScrollMax.y);++capture_stage;capture_pending=false;}};
        const auto result=scene.run();check(done && result.ui_overflow_items==0,"Real edit path did not complete or overflowed: "+scene.name+" / "+result.ui_overflow_label);if(!running)check(result.pixel_error.empty(),"Finished training received a cancellation in the replay report");std::cout<<scene.name<<" passed\n";
    }
}
inline void kept(const std::filesystem::path& out) {
    Scene scene(out,"t51-kept");const StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest request(*scene.doc,2,transform,AnalysisKind::StainMeans);const auto id=scene.doc->keep_analysis(request,analyze(request));const auto before=scene.doc->analysis_record(id);int field=0;bool done=false;
    scene.inputs.after_frame=[&](const Inputs&,const Feedback& feedback){check(scene.doc->analysis_record_count()==1 && scene.doc->analysis_record(id).result.input_sha256==before.result.input_sha256 && scene.doc->analysis_record(id).result.means==before.result.means,"Edit altered a kept record");for(bool cancelled:feedback.last_edit)check(!cancelled,"Idle consumer was reported cancelled");if(field==6){done=true;scene.commands.end_session=true;return;}const auto next=1u<<field++;scene.edit=[next](Inputs& v){change(v,next);};};
    scene.run();check(done,"Kept-record walk incomplete");std::cout<<"Kept record identity and values survive all six field edits; idle consumers stay silent\n";
}
inline void cancellation_views(const std::filesystem::path& out) {
    for(int c=0;c<5;++c)for(bool french:{false,true}) {
        Scene scene(out,"t51-cancelled-"+std::to_string(c)+(french?"-fr200":"-normal"),c,french);int stage=0;bool done=false;std::string visible,expected;
        scene.inputs.after_frame=[&](const Inputs&,const Feedback& feedback){const auto& job=scene.inputs.jobs[c];
            if(stage==0 && scene.snapshot.frame>2){scene.activate=c==1 || c==2?"Edit analysis inputs":"Advanced inputs###Analysis inputs";stage=1;return;}
            if(stage==1){if(scene.snapshot.text.find(ui::i18n::text("White RGB"))==std::string::npos)return;scene.start(c,true);stage=2;return;}
            if(stage==2){if(!job.busy || !scene.server->entered)return;scene.edit=[](Inputs& v){change(v,Inputs::White);};stage=3;return;}
            if(stage==3){if(feedback.message(static_cast<Consumer>(c),job.generation).empty())return;scene.server->hold=false;stage=4;return;}
            if(stage==4 && !job.busy){check(job.state=="cancelled","White edit failed to cancel consumer");done=true;scene.commands.end_session=true;stage=5;}
        };
        scene.snapshot.after=[&](int){if(stage==5){visible=scene.snapshot.text;expected=ui::i18n::text(detail::analysis_input_consumers[c].cancelled);check(visible.find(expected)!=std::string::npos,"Cancelled consumer panel lacks whole sentence");}};
        const auto result=scene.run(true);check(done && result.ui_overflow_items==0,"Cancellation view overflow/incomplete: "+scene.name+" / "+result.ui_overflow_label);check(report_message(result,c)==expected,"Cancellation report differs from panel");write(out/(scene.name+"-visible.txt"),visible);
    }
    std::cout<<"Five consumer windows/panels show cancellation sentences, normal/French 200%, zero overflow\n";
}
inline void applied_lifetime(const std::filesystem::path& out) {
    for(unsigned field:{1u,2u,4u,8u,16u,32u}){
        Scene scene(out,"t51-applied-"+std::to_string(field),4);int stage=0;bool done=false;
        scene.inputs.after_frame=[&](const Inputs&,const Feedback&){
            if(stage==0 && scene.snapshot.frame>2){scene.start(4);stage=1;return;}
            if(stage==1 && scene.inputs.jobs[4].state=="ready" && !scene.inputs.jobs[4].busy){scene.pending=GuiInputKind::ApplyStainEstimateKey;stage=2;return;}
            if(stage==2 && scene.inputs.estimate_applied){scene.edit=[field](Inputs& v){change(v,field);};stage=3;return;}
            if(stage==3){check(scene.inputs.estimate_applied==((field&(Inputs::Kind|Inputs::Vectors|Inputs::White))==0),"Applied estimate flag has the wrong dependency");done=true;scene.commands.end_session=true;stage=4;}
        };
        scene.run();check(done,"Applied estimate lifetime incomplete");
    }
    std::cout<<"Applied estimate flag follows Kind/Vectors/White only\n";
}
inline void training_edge(const std::filesystem::path& out) {
    Scene scene(out,"t51-training-first-frame",3);int stage=0;bool done=false;
    scene.inputs.after_frame=[&](const Inputs&,const Feedback& feedback){const auto& j=scene.inputs.jobs[3];
        if(stage==0 && scene.snapshot.frame>2){const auto found=scene.snapshot.scope.trace.actions.find("Train labelled areas");if(found==scene.snapshot.scope.trace.actions.end() || !found->second.enabled)return;scene.start(3,true);stage=1;return;}
        if(stage==1 && !scene.activate){scene.edit=[](Inputs& v){change(v,Inputs::White);};stage=2;return;}
        if(stage==2){check(j.generation!=0 && feedback.last_edit[3],"First-frame training edit lost the native generation/cancellation sentence");scene.server->hold=false;stage=3;return;}
        if(stage==3 && !j.busy){check(j.state=="cancelled","First-frame training edit failed to cancel native job");done=true;scene.commands.end_session=true;stage=4;}
    };
    const auto result=scene.run();check(done && result.pixel_error==detail::analysis_input_consumers[3].cancelled,"First-frame training cancellation missing from replay report");
    std::cout<<"First-frame native training cancellation and sentence passed\n";
}
inline void cached_white(const std::filesystem::path& out) {
    const StainTransform basis({{.65,.70,.29},{.07,.99,.11}});const ColorVector white={254,255,255};
    for(const std::string site:{"model","profile","estimate"})for(bool running:{false,true}) {
        if(site=="estimate" && running)continue; // Use estimate requires a finished estimate.
        Scene scene(out,"t51-cached-white-"+site+(running?"-running":"-finished"),4);
        scene.doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{.65,.70,.29},{.07,.99,.11}},white,"H&E",{"Hematoxylin","Eosin"}));
        const auto path=out/(scene.name+".json");save_stain_model(StainTransform::from_normalized(basis.stains(),white),path.u8string());scene.options.stain_model_path=path.u8string();
        scene.edit=[&](Inputs& value){value.use_stains(basis.stains(),basis.white());value.white[0]=254;value.edited(Inputs::White);};
        int stage=0;bool done=false;std::uint64_t revision=0,generation=0;
        scene.describe=[&]{return "stage="+std::to_string(stage)+" / "+scene.inputs.jobs[4].state;};
        scene.inputs.after_frame=[&](const Inputs& value,const Feedback& feedback){const auto& job=scene.inputs.jobs[4];
            if(stage==0 && scene.snapshot.frame>2){check(value.white==std::vector<double>({254,255,255}) && value.estimated_white==basis.white(),"Cache-white fixture did not establish distinct requested/cached references");scene.start(4,running);stage=1;return;}
            if(stage==1){if(running?(!job.busy || !scene.server->entered):(job.busy || job.state!="ready"))return;revision=value.revision;generation=job.generation;
                if(site=="model")scene.activate="Load stain model";else if(site=="profile")scene.activate="Use saved stains for analysis";else scene.pending=GuiInputKind::ApplyStainEstimateKey;stage=2;return;}
            if(stage==2){if(value.revision==revision)return;check(value.changed_fields==Inputs::Vectors && value.white==std::vector<double>({254,255,255}) && value.estimated_white==white,"Cache refresh changed/marked the requested white");
                check(job.generation==generation && (running?job.busy && job.state=="running":job.state=="ready" && job.has_result),"Cached reference refresh cancelled an estimator with unchanged request inputs");
                check(!feedback.last_edit[4] && feedback.message(Consumer::StainEstimation,generation).empty(),"Unchanged estimator received a cancellation sentence");scene.server->hold=false;stage=3;return;}
            if(stage==3 && !job.busy){check(job.state=="ready","Estimator did not finish after cache-only edit");done=true;scene.commands.end_session=true;stage=4;}
        };
        const auto result=scene.run();check(done && result.stain_estimate_state=="ready" && result.stain_estimate_error.empty() && result.stain_estimate_result && result.stain_estimate_result->white==white,"Cache-only edit changed the estimate/report white");
        std::cout<<scene.name<<" passed\n";
    }
}
inline void editor_reason(const std::filesystem::path& out) {
    for(int c:{1,2})for(bool french:{false,true}) {
        Scene scene(out,"t51-editor-reason-"+std::to_string(c)+(french?"-fr200":"-normal"),c,french);bool dialog=false,closing=false,done=false;int stage=0;
        const auto base=scene.commands.before_context;
        scene.commands.before_context=[&,base](int frame){
            if(dialog){if(!closing && !ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))ui::OpenDialog(ui::DialogKind::RemoveKeptResult);ui::DialogState state(true,false);ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,"Input editor fixture",{},[](ui::DialogControls&){ImGui::TextUnformatted("Input editor fixture");},[](ui::DialogAction){return ui::DialogOutcome::Success;});if(closing && !ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))dialog=false;}
            base(frame);
        };
        scene.inputs.after_frame=[&](const Inputs& value,const Feedback&){const auto found=scene.snapshot.scope.trace.actions.find("Edit analysis inputs");if(found==scene.snapshot.scope.trace.actions.end())return;const auto& action=found->second;
            if(stage==0 && scene.snapshot.frame>2){dialog=true;stage=1;return;}
            if(stage==1){if(!action.disabled)return;check(!action.enabled && action.reason==ui::i18n::text("Finish the current dialog first."),"Disabled Edit inputs has no matching catalogue reason");scene.activate="Edit analysis inputs";stage=2;return;}
            if(stage==2){check(value.revision==0 && scene.snapshot.text.find(ui::i18n::text("White RGB"))==std::string::npos,"Disabled editor action changed inputs/opened its section");closing=true;scene.pending=GuiInputKind::EscapeKey;stage=3;return;}
            if(stage==3){if(action.disabled || !action.enabled)return;scene.activate="Edit analysis inputs";stage=4;return;}
        };
        scene.snapshot.after=[&](int){if(stage==4 && scene.snapshot.text.find(ui::i18n::text("White RGB"))!=std::string::npos){done=true;scene.commands.end_session=true;stage=5;}};
        const auto report=scene.run();check(done && report.ui_overflow_items==0,"Dialog-close did not enable the inline editor");
    }
    std::cout<<"Edit inputs refusal reason and actual dialog-close/open chain, English/French, passed\n";
}
inline void already_cancelling(const std::filesystem::path& out) {
    for(int c:{2,3}) {
        Scene scene(out,"t51f1-already-cancelling-"+std::to_string(c),c);int stage=0;bool done=false;
        scene.inputs.after_frame=[&](const Inputs&,const Feedback& feedback){const auto& job=scene.inputs.jobs[c];
            if(stage==0 && scene.snapshot.frame>2){scene.start(c,true);stage=1;return;}
            if(stage==1){if(!job.busy || !scene.server->entered)return;if(c==2)scene.pending=GuiInputKind::CancelCellsKey;else scene.activate="Cancel pixel work";stage=2;return;}
            if(stage==2){if(job.state!="cancelling")return;check(job.busy && scene.server->hold,"Own Cancel did not leave a held cancelling worker");scene.edit=[](Inputs& value){change(value,Inputs::White);};stage=3;return;}
            if(stage==3){check(!feedback.last_edit[c] && feedback.message(static_cast<Consumer>(c),job.generation).empty(),"Input edit claimed to cancel already-stopping work");scene.server->hold=false;stage=4;return;}
            if(stage==4 && !job.busy){check(job.state=="cancelled","Own Cancel did not finish");stage=5;}
        };
        scene.snapshot.after=[&](int){if(stage!=5)return;check(scene.snapshot.text.find(ui::i18n::text(detail::analysis_input_consumers[c].cancelled))==std::string::npos,"Already-stopping consumer shows an input-change sentence");check(scene.snapshot.text.find(ui::i18n::text("Cancelled"))!=std::string::npos,"Already-stopping consumer lost its ordinary cancelled caption");done=true;scene.commands.end_session=true;stage=6;};
        const auto report=scene.run();check(done && report_message(report,c).empty(),"Already-stopping consumer reported an input-change error");std::cout<<scene.name<<" passed\n";
    }
}
inline void single_sentences(const std::filesystem::path& out) {
    for(int c:{0,1})for(bool french:{false,true}) {
        Scene scene(out,"t51f1-single-sentence-"+std::to_string(c)+(french?"-fr200":"-normal"),c,french);int stage=0;bool done=false;
        scene.inputs.after_frame=[&](const Inputs&,const Feedback& feedback){
            if(stage==0 && scene.snapshot.frame>2){scene.activate=c==0?"Advanced inputs###Analysis inputs":"Edit analysis inputs";stage=1;return;}
            if(stage==1){if(scene.snapshot.text.find(ui::i18n::text("White RGB"))==std::string::npos)return;scene.start(c==0?2:c,true);stage=2;return;}
            if(stage==2){if(!scene.inputs.jobs[c==0?2:c].busy || !scene.server->entered)return;if(c==0)scene.start(0,true);stage=3;return;}
            if(stage==3){if(!scene.inputs.jobs[c].busy || !scene.server->entered || scene.commands.task!=int(c==0?ui::WorkflowTask::Region:ui::WorkflowTask::Tissue))return;scene.edit=[](Inputs& value){change(value,Inputs::White);};stage=4;return;}
            if(stage==4){check(feedback.last_edit[c] && (c!=0 || feedback.last_edit[2]),"List filtering lost a cancelled consumer's edit record");scene.server->hold=false;if(scene.inputs.jobs[c].busy || (c==0 && scene.inputs.jobs[2].busy))return;stage=5;}
        };
        scene.snapshot.after=[&](int){if(stage!=5)return;const auto own=ui::i18n::text(detail::analysis_input_consumers[c].cancelled);write(out/(scene.name+"-visible.txt"),scene.snapshot.text);check(occurrences(scene.snapshot.text,own)==1,"Same-section cancellation sentence is duplicated or missing: "+scene.name);if(c==0)check(occurrences(scene.snapshot.text,ui::i18n::text(detail::analysis_input_consumers[2].cancelled))==1,"Hidden Cells cancellation sentence is duplicated or missing");done=true;scene.commands.end_session=true;stage=6;};
        const auto report=scene.run(true);check(done && report.ui_overflow_items==0,"Single-sentence scene incomplete/overflowed");std::cout<<scene.name<<" passed\n";
    }
}
inline void image_feedback_lifetime(const std::filesystem::path& out) {
    Scene scene(out,"t51f1-image-feedback",2);const auto first=scene.project->active_image();
    const auto source=out/"t51f1-second.ppm";write(source,"P6\n2 2\n255\n"+std::string(12,char(80)));
    const auto second=scene.project->add_document(std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,80))),"Second image",source.u8string());scene.project->set_active_image(first);
    int stage=0;bool done=false;
    scene.inputs.after_frame=[&](const Inputs&,const Feedback& feedback){const auto& cell=scene.inputs.jobs[2];
        if(stage==0 && scene.snapshot.frame>2){scene.activate="Edit analysis inputs";stage=1;return;}
        if(stage==1){if(scene.snapshot.text.find(ui::i18n::text("White RGB"))==std::string::npos)return;scene.start(2,true);stage=2;return;}
        if(stage==2){if(!cell.busy || !scene.server->entered)return;scene.edit=[](Inputs& value){change(value,Inputs::White);};stage=3;return;}
        if(stage==3){if(feedback.message(Consumer::Cells,cell.generation).empty())return;scene.server->hold=false;if(!cell.busy)stage=4;return;}
        if(stage==5){if(scene.inputs.active_image!=second)return;check(std::none_of(feedback.last_edit.begin(),feedback.last_edit.end(),[](bool value){return value;}),"Editor history survived an image change");for(const auto& entry:feedback.cancelled)check(!entry.active,"Consumer sentence survived an image change");stage=6;}
    };
    scene.snapshot.after=[&](int){const auto sentence=ui::i18n::text(detail::analysis_input_consumers[2].cancelled);
        if(stage==4){check(scene.snapshot.text.find(sentence)!=std::string::npos,"First image never showed its cancellation sentence");scene.pending=GuiInputKind::NextImageKey;stage=5;return;}
        if(stage==6){check(scene.snapshot.text.find(sentence)==std::string::npos,"Second image panel/editor names cancellation from the first");done=true;scene.commands.end_session=true;stage=7;}
    };
    const auto report=scene.run();check(done && scene.project->active_image()==second && report.cell_error.empty(),"Image-switch feedback/report did not clear");std::cout<<scene.name<<" public next-image switch passed\n";
}
inline void run(const std::filesystem::path& out,const std::string& part) {
    std::filesystem::create_directories(out);
    if(part=="all" || part=="editor")unit_editor(out);
    if(part=="all" || part=="matrix")matrix(out);
    if(part=="all" || part=="in-place")in_place(out);
    if(part=="all" || part=="paths")edit_paths(out);
    if(part=="all" || part=="kept")kept(out);
    if(part=="all" || part=="views")cancellation_views(out);
    if(part=="all" || part=="applied")applied_lifetime(out);
    if(part=="all" || part=="training-edge")training_edge(out);
    if(part=="all" || part=="cached-white")cached_white(out);
    if(part=="all" || part=="editor-reason")editor_reason(out);
    if(part=="all" || part=="fix1") {already_cancelling(out);single_sentences(out);image_feedback_lifetime(out);}
}
}
