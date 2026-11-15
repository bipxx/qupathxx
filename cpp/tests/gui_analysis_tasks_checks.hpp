#pragma once
#include "gui_workflow_safety_checks.hpp"
#include <regex>
#include <atomic>
#include "../gui_result_panel.hpp"
#include "../gui_stain_model.hpp"

// Characterize the product, including real native jobs; never assign a job state.
namespace analysis_tasks_checks {
inline void source_paths() {
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    check(root.is_absolute() && root.root_name().string()!="Q:","Frontend source path still requires Q:");
    std::size_t sources=0;for(const auto& entry:std::filesystem::directory_iterator(root/"cpp"))if(entry.is_regular_file())++sources;
    check(sources>0 && std::filesystem::exists(root/"cpp/gui_result_panel.hpp"),"Cannot inspect real cpp source directory");
    const auto server=open_ome_zarr((root/"tests/fixtures/ome-zarr/plain").u8string());
    check(server->metadata().width>0 && server->metadata().height>0,"Cannot inspect real OME-Zarr fixture");
    std::cout<<"Real-source probe: "<<root.u8string()<<" / cpp files="<<sources<<" / OME-Zarr="<<server->metadata().width<<"x"<<server->metadata().height<<" passed\n";
}
struct Server final:ImageServer {
    std::shared_ptr<ImageServer> inner=dense_image_server(Image(128,96,std::vector<std::uint8_t>(128*96*3,80)));
    std::thread::id ui_thread=std::this_thread::get_id();
    mutable std::atomic<bool> hold{false},fail{false},entered{false};
    const ImageMetadata& metadata() const override{return inner->metadata();}
    PixelBuffer read_region(const RegionRequest& request) const override {
        if(std::this_thread::get_id()!=ui_thread) {
            // Only the native ROI job owns this gate; viewer/overview reads cannot satisfy entered.
            if(hold.load() && request.x==8 && request.y==8 && request.width==44 && request.height==60) {
                entered=true;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
                while(hold.load()) {
                    if(std::chrono::steady_clock::now()>deadline) {hold=false;throw std::runtime_error("Held ROI reader timed out");}
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            if(fail.load())throw std::runtime_error("t32 fixture read failure");
        }
        return inner->read_region(request);
    }
    void close() override{inner->close();}
    bool closed() const override{return inner->closed();}
};
inline std::string normalized(std::string text) {
    // Wall clock values are nondeterministic; the raw capture remains beside the comparison.
    return std::regex_replace(text,std::regex("[0-9]+\\.[0-9]+ ms"),"<elapsed> ms");
}
struct Snapshot {
    simplification_checks::TraceScope scope;std::string text,actions,results,first_label="Features",visible_first_label;bool installed=false;
    std::function<void(int)> before,after;int frame=0;
    Snapshot(){scope.trace.before_frame=[&](int f){frame=f;if(before)before(f);if(installed)return;installed=true;
        ImGuiContextHook begin;begin.Type=ImGuiContextHookType_NewFramePost;
        begin.Callback=[](ImGuiContext* ctx,ImGuiContextHook*){ImGui::LogToBuffer();ctx->LogWindow=nullptr;};ImGui::AddContextHook(ImGui::GetCurrentContext(),&begin);
        ImGuiContextHook end;end.Type=ImGuiContextHookType_EndFramePre;end.UserData=this;
        end.Callback=[](ImGuiContext* ctx,ImGuiContextHook* hook){auto& self=*static_cast<Snapshot*>(hook->UserData);
            std::string all=ctx->LogBuffer.c_str();ImGui::LogFinish();
            // The actual submitted Workflow pane text is bounded by these two windows.
            auto start=all.find("Task");if(start!=std::string::npos)all.erase(0,start);
            self.text=all;self.visible_first_label=ui::i18n::text(self.first_label.c_str());
            std::vector<std::pair<std::string,ui::WorkflowActionTrace>> ordered(self.scope.trace.actions.begin(),self.scope.trace.actions.end());
            std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.second.order<b.second.order;});
            self.actions.clear();std::size_t index=0;
            for(const auto& [key,a]:ordered) {
                if(key.find("Analyze region")==0 || key.find("Detect ")==0 || key.find("Keep ")==0 || key.find("Recheck ")==0 || key.find("Go to tables")==0 || key.find("Cancel analysis")==0 || key.find("Cancel tissue")==0 || key.find("Cancel cells")==0 || key.find("Trace contours")==0 || key.find("Add annotations")==0 || key.find("Add cells")==0 || key.find("Export cell table")==0 || key.find("Remove kept result")==0)
                    self.actions+=std::to_string(index++)+" | "+key+" | "+ui::i18n::text(a.caption.c_str())+" | "+(a.enabled?"enabled":"disabled")+" | "+(a.primary?"primary":"secondary")+(a.disabled?" | reason: "+a.reason:"")+"\n";
            }
            for(const auto& [key,a]:self.scope.trace.actions)if(a.disabled &&
                (key.find("Analyze region")==0 || key.find("Detect ")==0 || key.find("Keep ")==0 || key.find("Recheck ")==0 || key.find("Trace contours")==0 || key.find("Add annotations")==0 || key.find("Add cells")==0 || key.find("Export cell table")==0 || key.find("Remove kept result")==0)) {
                check(!a.reason.empty(),"Task disabled action has no reason: "+key);
                if(!a.inline_reason.empty())check(a.inline_reason==a.reason,"Task inline/tooltip reasons differ: "+key);
            }
            self.results.clear();for(const auto& [key,r]:self.scope.trace.results) {
                self.results+=key+" | "+r.caption+" | "+std::to_string(r.record_id)+" | "+std::to_string(int(r.kind))+" | "+std::to_string(r.nucleus);
                for(auto mean:r.means){char value[64];std::snprintf(value,sizeof(value)," | %.17g",mean);self.results+=value;}self.results+='\n';
            }
            if(self.after)self.after(self.frame);
        };ImGui::AddContextHook(ImGui::GetCurrentContext(),&end);
    };}
    std::string comparison() const {
        const auto start=text.find("\n"+visible_first_label+" ");
        check(start!=std::string::npos,"Task text boundary missing: "+first_label);
        const auto end=text.rfind("--------------------------------");check(end>start,"Task footer boundary missing");
        std::string lines;std::istringstream input(text.substr(start,end-start));std::string line;
        // Compare every task line; only measured wall time is normalized. Raw lines are retained too.
        while(std::getline(input,line)) {line=std::regex_replace(line,std::regex("^[ \\t\\r]+|[ \\t\\r]+$"),"");if(!line.empty())lines+=normalized(line)+'\n';}
        return "ACTIONS\n"+actions+"RESULTS\n"+results+"LINES\n"+lines;
    }
};
inline void presenter_contracts(const std::filesystem::path& out) {
    simplification_checks::NativeControls host;
    auto rendered=[&](auto draw) {
        ImGui::NewFrame();ImGui::Begin("Presenter contract fixture");ImGui::LogToBuffer();draw();
        const std::string text=host.context->LogBuffer.c_str();ImGui::LogFinish();ImGui::End();ImGui::Render();return text;
    };
    ui::ResultModel model;ui::ResultActions actions;
    for(const auto& raw:{"running","cancelling","idle","ready","failed","future %s token"}) {
        model.raw_run=raw;model.busy=true;
        const auto expected=std::string(raw)=="cancelling"?"Cancelling":std::string(raw)=="future %s token"?"Unknown status (future %s token)":"Running";
        check(ui::run_caption(model)==expected,"Busy caption lost polled Running/Cancelling/Unknown rule");
    }
    model.raw_run="running";model.phase="literal %s%n step";model.done=model.total=0;
    const auto zero=rendered([&]{ui::draw_run_status(model,actions);});
    check(zero.find("literal %s%n step")!=std::string::npos && zero.find("Tiles:")==std::string::npos,"Step was formatted or zero-total tiles were shown");
    model.total=2;check(rendered([&]{ui::draw_run_status(model,actions);}).find("Tiles: 0 / 2")!=std::string::npos,"Nonzero total lost tile progress");
    model.kept=ui::KeptView{1,"literal %s%n label",ui::RecordState::Unknown,false,"future record"};
    ui::ResultIds ids{"Keep result (Ctrl+K)","Go to tables###ROI tables","Recheck saved (Ctrl+Shift+K)",nullptr,"remove"};
    check(rendered([&]{ui::draw_kept_block(model,ids,actions,{});}).find("literal %s%n label")!=std::string::npos,"Display label became a printf format");
    const std::pair<const char*,const char*> tissue[]={{"smoothing","Smoothing tissue feature..."},{"contours","Tracing tissue contours..."},{"cleanup","Cleaning tissue mask..."},{"components","Finding components..."},{"thresholding","Reading and thresholding..."}};
    for(const auto& pair:tissue)check(ui::tissue_step_sentence(pair.first)==pair.second,"Tissue phase sentence changed");
    const std::pair<const char*,const char*> cells[]={{"reading","Reading pixels..."},{"smoothing","Smoothing cell feature..."},{"distance","Measuring distances..."},{"watershed","Separating nuclei..."},{"expansion","Expanding cell bodies..."},{"contours","Tracing cell contours..."}};
    for(const auto& pair:cells)check(ui::cell_step_sentence(pair.first)==pair.second,"Cell phase sentence changed");
    for(const auto* phase:{"idle","complete","future phase"})check(ui::cell_step_sentence(phase).empty(),"Non-working cell phase displayed a step");
    detail::GuiStainModel io;
    auto finish=[&]{const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(io.busy()) {io.poll();check(std::chrono::steady_clock::now()<deadline,"Model operation did not finish");std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
    io.load((out/"missing-model.json").u8string());finish();
    check(io.state()=="failed" && io.operation()==detail::GuiStainModel::Operation::Load,"Failed load lost its operation");
    check(rendered([&]{ui::TextWrapped("Model file: %s",io.caption().c_str());}).find("Model file: Work failed")!=std::string::npos,"Load failure shown as export failure");
    io.fail("Owner changed","stale");check(io.operation()==detail::GuiStainModel::Operation::Load && io.caption()=="Inputs changed","Stale load lost run vocabulary");
    io.save(StainTransform({{1,0,0},{0,1,0}}),(out/"absent-parent"/"model.json").u8string(),{},ExportWritePolicy::CreateNew);finish();
    check(io.state()=="failed" && io.operation()==detail::GuiStainModel::Operation::Save,"Failed save lost its operation");
    check(rendered([&]{ui::TextWrapped("Model file: %s",io.caption().c_str());}).find("Model file: Export failed")!=std::string::npos,"Save failure lost export vocabulary");
    std::cout<<"Presenter contracts: step sentences, literal label/step, busy captions, zero totals and native load/save failures passed\n";
}
inline void run(const std::filesystem::path& out,const std::string& mode) {
    std::filesystem::create_directories(out);
    const auto baseline=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/gui-analysis-tasks";
    check(mode=="record-presenter" || mode=="record-dispatcher" || mode=="presenter" || mode=="presenter-fr" || mode=="presenter-details","Unsupported analysis task mode");
    const bool french=mode=="presenter-fr",expanded=mode=="presenter-details";
    const bool record_presenter=mode=="record-presenter",pinned=mode=="presenter";
    if(record_presenter || pinned)for(const auto* task:{"region","tissue","cells"})for(const auto* situation:{"no-result","running","preview","kept","stale","cancelled","failed","recheck"}) {
        const auto path=baseline/(std::string(task)+"-"+situation+".txt");
        check(record_presenter?!std::filesystem::exists(path):std::filesystem::exists(path),record_presenter?"Refusing to overwrite accepted presenter traces":"Missing accepted presenter trace: "+path.u8string());
    }
    {
        presenter_contracts(out);
        ui::ResultModel model;check(ui::primary_action(model)==ui::ResultPrimary::Run,"Idle primary rule");
        model.run=ui::RunState::Ready;model.can_keep=true;check(ui::primary_action(model)==ui::ResultPrimary::Keep,"Fresh primary rule");
        model.can_keep=false;model.preview_is_kept=true;check(ui::primary_action(model)==ui::ResultPrimary::View,"Kept primary rule");
        model.preview_is_kept=false;model.preview_is_recheck=true;check(ui::primary_action(model)==ui::ResultPrimary::View,"Recheck primary rule");
        model.busy=true;check(ui::primary_action(model)==ui::ResultPrimary::None,"Running primary rule");
        simplification_checks::NativeControls host;ImGui::NewFrame();ImGui::Begin("Unknown presenter fixture");ImGui::LogToBuffer();
        model=ui::ResultModel{};model.raw_run="future %s token";model.run=ui::parse_run_state(model.raw_run);ui::ResultActions actions;
        ui::draw_run_status(model,actions);model.kept=ui::KeptView{1,"",ui::RecordState::Unknown,false,"future record"};
        ui::ResultIds ids{"Keep result (Ctrl+K)","Go to tables###ROI tables","Recheck saved (Ctrl+Shift+K)",nullptr,"remove"};ui::draw_kept_block(model,ids,actions,{});
        const std::string text=host.context->LogBuffer.c_str();ImGui::LogFinish();ImGui::End();ImGui::Render();
        check(text.find("Unknown status (future %s token)")!=std::string::npos && text.find("Unknown status (future record)")!=std::string::npos && text.find("Recheck failed")==std::string::npos,"Presenter lost safe unknown captions");
    }
    for(int family=0;family<3;++family)for(const std::string situation:{"no-result","running","preview","kept","stale","cancelled","failed","recheck"}) {
        const std::string task=family==0?"region":family==1?"tissue":"cells",name=task+"-"+situation;
        auto server=std::make_shared<Server>();auto doc=std::make_shared<Document>(server);auto project=std::make_shared<Project>();
        const auto source=out/("t32-"+name+".ppm-source.ppm");write(source,"P6\n128 96\n255\n"+std::string(128*96*3,char(80)));project->add_document(doc,"Characterisation",source.u8string());
        const auto id=doc->add_rectangle({8,8,44,60});doc->set_calibration(.5,.25);
        auto o=options(out,"t32-"+name,"workflow",task.c_str());o.width=1440;o.height=1200;o.settle_tiles=false;
        if(french){o.ui_scale=2;o.width=2200;o.height=2000;auto settings=read(o.layout_path);settings.replace(settings.find("language=en"),11,"language=fr");write(o.layout_path,settings);}
        const auto start=family==0?GuiInputKind::AnalyzeKey:family==1?GuiInputKind::DetectTissueKey:GuiInputKind::DetectCellsKey;
        const auto keep=family==0?GuiInputKind::KeepAnalysisKey:family==1?GuiInputKind::KeepTissueKey:GuiInputKind::KeepCellRunKey;
        const auto cancel=family==0?GuiInputKind::CancelAnalysisKey:family==1?GuiInputKind::CancelTissueKey:GuiInputKind::CancelCellsKey;
        const auto recheck=family==0?GuiInputKind::RecheckAnalysisKey:family==1?GuiInputKind::RecheckTissueKey:GuiInputKind::RecheckCellRunKey;
        o.inputs={key(3,GuiInputKind::SelectAllKey)};if(situation!="no-result")o.inputs.push_back(key(5,start));
        if(situation=="kept" || situation=="recheck")o.inputs.push_back(key(16,keep));
        if(situation=="recheck")o.inputs.push_back(key(25,recheck));
        if(situation=="cancelled")o.inputs.push_back(key(8,cancel));
        const int frames=situation=="running"?7:40;
        Snapshot observed;std::string collapsed_text;int cancelling_frames=0;bool saw_cancelled=false;
        observed.first_label=family==0?"Features":family==1?"Feature":"Cell feature";observed.before=[&](int f){
            if(f==4 && (situation=="running" || situation=="cancelled")) {server->entered=false;server->hold=true;}
            if((situation=="running" && f==frames-1) || (situation=="cancelled" && f==8)) {
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                while(!server->entered.load() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if(!server->entered.load()) {server->hold=false;throw std::runtime_error("Native worker did not enter held ROI reader: "+name);}
            }
            if(f==4 && situation=="failed")server->fail=true;
            if(f==12)server->hold=false;
            if(f==24 && situation=="stale")doc->update_rectangle(id,{8,8,40,56});
            if(expanded && (situation=="preview" || situation=="kept" || situation=="recheck") && (f==22 || f==30)) {
                auto* parent=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);
                for(auto* pane:ImGui::GetCurrentContext()->Windows)if(pane->ParentWindow==parent && pane->ChildId==ImHashStr("Workflow task body",0,parent->ID)) {
                    const std::string label=f==30 && family==0 && situation!="preview"?"Details###Region kept timing":"Details###"+(family==0?std::string("Region"):family==1?std::string("Tissue"):std::string("Cells"))+" preview timing";
                    if(f==22 || (family==0 && situation!="preview"))ImGui::ActivateItemByID(pane->GetID(label.c_str()));
                }
            }
        };
        observed.after=[&](int f){
            if(f==20)collapsed_text=observed.text;
            if(family==0 && situation=="cancelled") {
                if(f>=9 && f<12) {check(observed.text.find(ui::i18n::text("Cancelling"))!=std::string::npos,"Held Region stopped showing Cancelling");++cancelling_frames;}
                if(f>=12 && observed.text.find(ui::i18n::text("Cancelled"))!=std::string::npos)saw_cancelled=true;
            }
            // EndFramePre has captured this frame's text/actions/results; release only now.
            if(situation=="running" && f==frames-1)server->hold=false;
        };
        const auto report=run_project_gui(project,frames,(out/("t32-"+name+".ppm")).u8string(),o);server->hold=false;
        const auto state=family==0?report.analysis_state:family==1?report.tissue_state:report.cell_state;
        const std::string expected=situation=="no-result"?"idle":situation=="running"?"cancelled":situation=="stale"?"stale":situation=="cancelled"?"cancelled":situation=="failed"?"failed":"ready";
        check(report.replay_complete,"Characterisation replay incomplete: "+name);
        if(situation!="running")check(state==expected,"Characterisation state "+name+": "+state+", expected "+expected);
        if(situation=="running")check(observed.actions.find("Cancel")!=std::string::npos,"Running frame did not show Cancel: "+name);
        if(family==0 && situation=="cancelled")check(cancelling_frames==3 && saw_cancelled,"Region did not show held Cancelling then Cancelled");
        if(situation=="kept" || situation=="recheck")check((family==0?doc->analysis_record_count():family==1?doc->tissue_record_count():doc->cell_record_count())==1,"Characterisation did not keep exactly once: "+name);
        check(report.ui_overflow_items==0,"Characterisation overflow: "+name);
        write(out/("t32-visible-"+name+".txt"),observed.text);write(out/("t32-actual-"+name+".txt"),observed.comparison());
        const auto accepted=baseline/(name+".txt");
        if(record_presenter) {const auto recorded=out/(name+".txt");check(!std::filesystem::exists(accepted) && !std::filesystem::exists(recorded),"Refusing to overwrite accepted presenter trace");write(recorded,observed.comparison());}
        if(mode=="record-dispatcher") {const auto recorded=out/(name+".txt");check(!std::filesystem::exists(recorded),"Refusing to overwrite dispatcher recording");write(recorded,observed.comparison());}
        if(pinned)check(read(accepted)==observed.comparison(),"Accepted presenter trace differs: "+name);
        if(situation=="preview" || situation=="kept" || situation=="recheck") {
            char count[128]{};
            if(family==0)std::snprintf(count,sizeof(count),"Samples: %llu",static_cast<unsigned long long>(situation=="preview"?report.analysis_samples:doc->latest_analysis(id)->result.samples));
            if(family==1)std::snprintf(count,sizeof(count),"Samples: %llu",static_cast<unsigned long long>(report.tissue_samples));
            if(family==2)std::snprintf(count,sizeof(count),"Markers: %zu",report.cell_result->markers().size());
            if(!french)check(collapsed_text.find(count)!=std::string::npos && collapsed_text.find("Run time:")==std::string::npos,"Collapsed Details hid a count or exposed time: "+name);
        }
        if(expanded && (situation=="preview" || situation=="kept" || situation=="recheck")) {
            check(observed.text.find("Run time:")!=std::string::npos,"Expanded Details lost timing: "+name);
            char timing[128]{};
            if(family==0 && situation!="preview") {const auto saved=doc->latest_analysis(id);std::snprintf(timing,sizeof(timing),"Run time: %.3f ms",saved->result.elapsed_ms);}
            if(family==1 && situation=="kept") {const auto saved=doc->latest_tissue(id);std::snprintf(timing,sizeof(timing),"Run time: %.3f ms",saved->result->elapsed_ms());}
            if(family==2)std::snprintf(timing,sizeof(timing),"Run time: %.3f ms",report.cell_result->elapsed_ms());
            if(*timing)check(observed.text.find(timing)!=std::string::npos,"Details changed native timing/count values: "+name);
        }
        std::cout<<name<<": "<<state<<" / zero overflow / baseline "<<mode<<" passed\n";
    }
}
}
