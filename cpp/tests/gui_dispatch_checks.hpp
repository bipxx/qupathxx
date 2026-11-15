#pragma once
#include "gui_command_checks.hpp"
#include "gui_analysis_tasks_checks.hpp"
#include "../filesystem_access.hpp"
// Three stable SQLite C entry points from the already linked native SQLite.
// Test-only exclusive transactions hold retention work independently of disk speed.
extern "C" {
struct sqlite3;
int sqlite3_open(const char*,sqlite3**);
int sqlite3_exec(sqlite3*,const char*,int (*)(void*,int,char**,char**),void*,char**);
int sqlite3_close(sqlite3*);
}

namespace dispatch_checks {
using Id=ui::CommandId;
struct TemporaryHashInput {
    std::filesystem::path path;
    ~TemporaryHashInput(){if(!path.empty()){std::error_code ignored;std::filesystem::remove(path,ignored);}}
};
struct TraceScope {
    ui::CommandTrace trace;ui::CommandTrace* previous=ui::command_trace;
    ui::NotificationsTrace notification_trace;ui::NotificationsTrace* previous_notifications=ui::notifications_trace;ui::Notifications* notices=nullptr;
    simplification_checks::TraceScope workflow;
    TraceScope(){ui::command_trace=&trace;notification_trace.before_frame=[&](ui::Notifications& value){notices=&value;};ui::notifications_trace=&notification_trace;}
    ~TraceScope(){ui::command_trace=previous;ui::notifications_trace=previous_notifications;}
};
inline void physical(ImGuiKeyChord chord,bool down) {
    if(ImGui::GetFrameCount()==0)glfwSwapInterval(0);
    auto& io=ImGui::GetIO();io.ConfigInputTrickleEventQueue=false;
    for(auto mod:{ImGuiMod_Ctrl,ImGuiMod_Shift,ImGuiMod_Alt,ImGuiMod_Super})io.AddKeyEvent(mod,down && (chord&mod));
    io.AddKeyEvent(static_cast<ImGuiKey>(chord&~ImGuiMod_Mask_),down);
}
inline void point_at_canvas(bool focus=true) {
    if(ImGui::GetFrameCount()==0)glfwSwapInterval(0);
    if(auto* view=ImGui::FindWindowByName("Slide")) {
        const auto r=view->WorkRect;ImGui::GetIO().AddMousePosEvent((r.Min.x+r.Max.x)*.5f,(r.Min.y+r.Max.y)*.5f);
        if(focus)ImGui::FocusWindow(view);
    }
}
inline ImGuiWindow* task_body() {
    auto* parent=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);
    if(parent)for(auto* window:ImGui::GetCurrentContext()->Windows)
        if(window->ParentWindow==parent && window->ChildId==ImHashStr("Workflow task body",0,parent->ID))return window;
    return nullptr;
}
// Focused-button activation enters ImGui::ButtonBehavior at the real widget;
// it does not call a cancel handler or assign any native job state.
inline void activate(ImGuiWindow* window,const char* label) {
    if(!window)return;const auto id=window->GetID(ui::i18n::label(label).c_str());auto& g=*ImGui::GetCurrentContext();
    ImGui::FocusWindow(window);g.NavActivateId=g.NavActivateDownId=g.NavActivatePressedId=id;
}
inline void discard_fixture_close() {
    auto* modal=ImGui::GetTopMostPopupModal();if(!modal || modal->ID!=ImHashStr(ui::dialog_definition(ui::DialogKind::CloseProject).name))return;
    for(const auto& action:ui::dialog_actions(ui::DialogKind::CloseProject,ui::DialogVariant::Default))if(action.id==ui::DialogAction::Discard)activate(modal,action.label);
}
inline void table_rules() {
    std::set<ImGuiKeyChord> chords;
    const std::set<Id> typing={Id::Header_OpenProject,Id::Header_SaveProject,Id::Header_ProjectPanel,Id::Header_ImagePanel,Id::Header_AnnotationsPanel,Id::Header_WorkflowPanel,Id::Header_ChooseLanguage,Id::Header_QuickReference};
    const std::set<Id> canvas={Id::Key_SelectAll,Id::Key_Delete,Id::Key_EscapeCanvas,Id::Key_FinishDraft,Id::Key_InsertVertex,Id::Action_Apply_Boolean_B,Id::Action_Merge_annotations_M};
    std::set<Id> actual_typing,actual_canvas;
    for(const auto& row:ui::command_table) {
        if(row.chord && row.scope==ui::KeyScope::Global)actual_typing.insert(row.id);
        if(row.scope==ui::KeyScope::Canvas)actual_canvas.insert(row.id);
        check(row.claim_navigation==(row.id==Id::Key_NextImage || row.id==Id::Key_PreviousImage),"Unexpected navigation claim");
        if(row.chord)check(chords.insert(row.chord).second,"Two owning rows share a chord");
        if(row.chord_owner!=Id::Count)check(!row.chord && std::string(ui::command_chord(row.id))==ui::command_chord(row.chord_owner),"Alias copied a chord or lost its owner's text");
        if(std::strstr(row.legacy_id,"Cancel") || row.id==Id::Key_StopRecoveryMaintenance || row.id==Id::Key_CancelBatch || row.id==Id::Key_CancelFingerprint)
            check(!row.chord,"Job Cancel/Stop has a chord");
        check(!row.repeat,"Unexpected repeating command");
    }
    check(actual_typing==typing,"Global owning chords widened beyond the eight commands that already worked while typing");
    check(actual_canvas==canvas,"Canvas rows differ from the seven workspace object commands");
    for(const auto pair:{std::make_pair(Id::Action_Trace_contours_Ctrl_J,Id::Action_Add_annotations_Ctrl_Shift_J),std::make_pair(Id::Action_Detect_cells_Ctrl_G,Id::Action_Add_cells_Ctrl_Shift_G)})
        check(ui::command(pair.second).chord==(ui::command(pair.first).chord|ImGuiMod_Shift),"Sibling Add is not Run plus Shift");
    check(ui::command(Id::Header_Redo).chord==(ImGuiMod_Ctrl|ImGuiKey_Y) && ui::command(Id::Key_RedoAlternate).chord==(ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Z),"Redo lost one chord");
    std::cout<<"Table: unique owning chords, alias text, retired Cancel/Stop, sibling Shift and both Redo chords passed\n";
}
// Every row is driven through run_project_gui, GLFW/ImGui NewFrame and the
// installed sites. probe_all additionally observes the gate for unavailable
// commands; it does not dispatch or replace any handler.
inline void every_chord_respects_scope(const std::filesystem::path& dir) {
    for(const auto& row:ui::command_table)if(row.chord) {
        const auto name=std::to_string(static_cast<unsigned>(row.id));
        for(const std::string mode:{"dialog","text","tool","held"}) {
            const auto stem="scope-"+name+"-"+mode;
            auto project=fixture(dir/(stem+".qpxproj"),32,32);auto doc=project->document(1);
            const auto revision=doc->revision();auto o=options(dir,stem,"workflow","measurements");o.settle_tiles=false;
            TraceScope observed;observed.trace.probe_all=true;int frame=-1,count=0;bool first=false;char text[64]="Editing text";
            observed.workflow.trace.before_frame=[&](int f) {
                frame=f;if(mode=="held")point_at_canvas();physical(row.chord,f>=5 && f<15);
                if(mode=="text" || mode=="tool")ImGui::GetIO().AddMousePosEvent(80,120);
            };
            observed.trace.before_context=[&](int) {
                if(mode=="dialog") {
                    if(!ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))ui::OpenDialog(ui::DialogKind::RemoveKeptResult);
                    ui::DialogState state(true,false);
                    ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,"Dispatcher scope",{},[](ui::DialogControls&){ui::TextUnformatted("Dispatcher scope");},[](ui::DialogAction){return ui::DialogOutcome::Success;});
                } else if(mode=="text" || mode=="tool") {
                    ImGui::SetNextWindowPos({20,70});ImGui::SetNextWindowSize({310,210});
                    ImGui::Begin("Dispatcher tool",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);
                    ImGui::SetWindowFocus();
                    if(mode=="text") {if(frame==1)ImGui::SetKeyboardFocusHere();ImGui::InputText("Editor",text,sizeof(text));}
                    else ImGui::Button("Panel control");
                    ImGui::End();
                }
            };
            observed.trace.after_frame=[&](const ui::ChordContext& context,const auto& fired) {
                if(frame<5 || frame>=15)return;
                check(fired.count()<=1,"Physical chord reached multiple commands: "+stem);
                count+=fired[static_cast<std::size_t>(row.id)];
                if(frame!=5)return;
                first=true;
                if(mode=="dialog")check(context.dialog_open && fired.none(),"Chord escaped registered dialog: "+stem);
                if(mode=="text")check(context.text_input && fired[static_cast<std::size_t>(row.id)]==(row.scope==ui::KeyScope::Global),"Text scope disagrees: "+stem);
                if(mode=="tool")check(!context.workspace_focused && fired[static_cast<std::size_t>(row.id)]==(row.scope!=ui::KeyScope::Canvas),"Tool-window scope disagrees: "+stem);
                if(mode=="held")check(context.workspace_focused && fired[static_cast<std::size_t>(row.id)],"Canvas chord did not fire: "+stem);
            };
            const auto report=run_project_gui(project,17,"",o);
            check(first,"Scope did not observe its physical input: "+stem);
            if(mode=="held")check(count==1,"Nonrepeating chord did not fire once in ten held frames: "+stem);
            if(mode=="dialog" || (mode=="text" && row.scope!=ui::KeyScope::Global))check(count==0 && doc->revision()==revision,"Blocked physical chord changed objects: "+stem);
            check(report.replay_complete,"Scope replay incomplete");
        }
        std::cout<<"scope "<<name<<" "<<ui::command_chord(row.id)<<": registered dialog/text/tool/ten held frames passed\n";
    }
}
inline void wiring(const std::filesystem::path& dir) {
    for(const auto& row:ui::command_table)if(row.chord) {
        const auto stem="wire-"+std::to_string(static_cast<unsigned>(row.id));
        const bool cell_row=row.id==Id::Action_Detect_cells_Ctrl_G || row.id==Id::Action_Add_cells_Ctrl_Shift_G || row.id==Id::Action_Keep_cell_run_Ctrl_Shift_C || row.id==Id::Action_Recheck_cells_Ctrl_Alt_C || row.id==Id::Action_Export_image_cells_Ctrl_Alt_X || row.id==Id::Action_Export_project_cells_Ctrl_Shift_X;
        const bool estimate_row=row.id==Id::Action_Estimate_stains_F3 || row.id==Id::Action_Use_estimated_stains_Shift_F3;
        std::shared_ptr<Project> project;
        if(cell_row || estimate_row) {
            std::vector<std::uint8_t> rgb(128*96*3,255);const StainTransform truth({{.65,.70,.29},{.07,.99,.11}});
            for(int y=0;y<96;++y)for(int x=0;x<128;++x)for(int c=0;c<3;++c) {
                if(cell_row){if((x-25)*(x-25)+(y-32)*(y-32)<=100 || (x-39)*(x-39)+(y-32)*(y-32)<=100)rgb[(y*128+x)*3+c]=0;}
                else {const auto a=x%8==0?0.:.1+.6*x/127,b=x%8==1?0.:.1+.7*y/95;rgb[(y*128+x)*3+c]=std::uint8_t(std::round(255*std::pow(10.,-a*truth.stains()[0][c]-b*truth.stains()[1][c])));}
            }
            project=std::make_shared<Project>();project->add_document(std::make_shared<Document>(Image(128,96,std::move(rgb))));project->save_new((dir/(stem+".qpxproj")).u8string());
        } else if(row.id==Id::Action_Review_for_project_Ctrl_Alt_F) {
            const auto source=dir/(stem+"-source.ppm");write(source,"P6\n128 96\n255\n"+std::string(128*96*3,char(80)));
            project=std::make_shared<Project>();project->add_image(source.u8string());project->save_new((dir/(stem+".qpxproj")).u8string());
        } else project=fixture(dir/(stem+".qpxproj"),128,96);
        auto doc=project->document(1);
        if(row.id==Id::Key_NextImage || row.id==Id::Key_PreviousImage)project->add_document(std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80))));
        const auto id=doc->add_rectangle({8,8,44,60});
        if(row.id==Id::Action_Apply_Boolean_B || row.id==Id::Action_Merge_annotations_M)doc->add_rectangle({30,10,40,40});
        const StainTransform transform({{1,0,0},{0,1,0}});const AnalysisRequest request(*doc,id,transform,AnalysisKind::StainMeans);
        doc->keep_analysis(request,analyze(request));
        if(row.id==Id::Header_Redo || row.id==Id::Key_RedoAlternate)doc->undo();
        if(row.id==Id::Key_InsertVertex)doc->update_roi(id,Roi::polygon({{8,8},{52,8},{52,68},{8,68}}));
        const auto vertex_count_before=row.id==Id::Key_InsertVertex?doc->annotation(id).geometry.paths().front().size():0;
        auto o=options(dir,stem,"workflow","measurements");o.settle_tiles=false;
        o.inputs={key(2,GuiInputKind::SelectAllKey)};o.interchange_path=(dir/(stem+".geojson")).u8string();
        int fire_frame=5;
        if(row.id==Id::Action_Review_for_project_Ctrl_Alt_F){fire_frame=18;o.file_fingerprint_path=doc->server()->source_locator();o.file_fingerprint_associate_source=true;o.inputs.push_back(key(3,GuiInputKind::FingerprintFileKey));o.inputs.push_back(workflow_safety_checks::waited(5,GuiInputKind::Move,GuiInputWait::FileFingerprintReady));}
        if(row.id==Id::Key_Recover || row.id==Id::Key_ReviewCheckpoint){o.recovery_path=(dir/(stem+".qpxrecover")).u8string();ProjectAutosave checkpoint;check(checkpoint.request(*project,o.recovery_path) && checkpoint.wait().state=="ready","Prepare available recovery command");}
        if(row.id==Id::Key_Autosave)o.autosave_path=(dir/(stem+".qpxrecover")).u8string();
        if(cell_row){CellParameters parameters;parameters.sigma=0;o.cell_parameters=std::make_shared<const CellParameters>(parameters);}
        if(row.id==Id::Key_InsertVertex){o.inputs.push_back(key(3,GuiInputKind::VertexKey));auto move=key(4,GuiInputKind::Move);move.target=GuiInputTarget::Image;move.x=8;move.y=30;o.inputs.push_back(move);}
        if(row.id==Id::Action_Keep_result_Ctrl_K){fire_frame=18;o.inputs.push_back(key(3,GuiInputKind::AnalyzeKey));}
        if(row.id==Id::Action_Keep_tissue_mask_Ctrl_U || row.id==Id::Action_Trace_contours_Ctrl_J || row.id==Id::Action_Add_annotations_Ctrl_Shift_J){fire_frame=18;o.inputs.push_back(key(3,GuiInputKind::DetectTissueKey));o.inputs.push_back(workflow_safety_checks::waited(5,GuiInputKind::Move,GuiInputWait::TissueReady));}
        if(row.id==Id::Action_Add_annotations_Ctrl_Shift_J){o.inputs.push_back(key(7,GuiInputKind::TraceTissueKey));o.inputs.push_back(workflow_safety_checks::waited(8,GuiInputKind::Move,GuiInputWait::TissueContoursReady));}
        if(row.id==Id::Action_Add_cells_Ctrl_Shift_G || row.id==Id::Action_Keep_cell_run_Ctrl_Shift_C){fire_frame=18;o.inputs.push_back(key(3,GuiInputKind::DetectCellsKey));o.inputs.push_back(workflow_safety_checks::waited(5,GuiInputKind::Move,GuiInputWait::CellsReady));}
        if(row.id==Id::Action_Recheck_saved_mask_Ctrl_Shift_U){TissueRequest saved(*doc,id,transform,TissueParameters{});doc->keep_tissue(saved,detect_tissue(saved));}
        if(row.id==Id::Action_Recheck_cells_Ctrl_Alt_C || row.id==Id::Action_Export_image_cells_Ctrl_Alt_X || row.id==Id::Action_Export_project_cells_Ctrl_Shift_X){CellParameters parameters;parameters.sigma=0;CellRequest saved(*doc,id,transform,parameters);doc->keep_cells(saved,detect_cells(saved));}
        if(row.id==Id::Action_Use_estimated_stains_Shift_F3){fire_frame=18;o.inputs.push_back(key(3,GuiInputKind::EstimateStainsKey));o.inputs.push_back(workflow_safety_checks::waited(5,GuiInputKind::Move,GuiInputWait::StainsEstimated));}
        if(row.id==Id::Key_FinishDraft) {
            fire_frame=12;o.inputs.push_back(key(3,GuiInputKind::PolygonKey));int frame=4;
            for(const Point p:std::vector<Point>{{80,10},{100,20},{80,30}}){GuiInput down;down.frame=frame++;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Image;down.x=p.x;down.y=p.y;o.inputs.push_back(down);down.frame=frame++;down.kind=GuiInputKind::LeftUp;o.inputs.push_back(down);}
        }
        o.measurement_table_path=(dir/(stem+".csv")).u8string();o.cell_table_path=(dir/(stem+"-cells.csv")).u8string();
        // The physical Run starts native work; a frame cap cannot prove it is ready.
        if(row.id==Id::Action_Detect_cells_Ctrl_G)o.inputs.push_back(workflow_safety_checks::waited(fire_frame+1,GuiInputKind::Move,GuiInputWait::CellsReady));
        if(row.id==Id::Action_Hash_file_Ctrl_Shift_F){o.file_fingerprint_path=(dir/(stem+"-hash.bin")).u8string();write(std::filesystem::u8path(o.file_fingerprint_path),"Hash this named input");}
        std::unique_ptr<workflow_safety_checks::FeedbackTrace> import_feedback;
        if(row.id==Id::Action_Import_GeoJSON_F7){save_geojson(*doc,o.interchange_path);import_feedback=std::make_unique<workflow_safety_checks::FeedbackTrace>();}
        // The table drives the probe; the site's own reader must set exactly its
        // bit here. There is no table probe in this test.
        TraceScope observed;int frame=-1;bool fired_once=false;int selected_tool=-1,task=-1;bool objects=true,opened=false,dialog_seen=false;
        core_path_checks::TraceScope forms;detail::ProjectForm shown=detail::ProjectForm::None;
        forms.trace.before_frame=[&](detail::GuiProjectForms& current,const auto&,auto&){if(frame>fire_frame)shown=current.shown;};
        observed.workflow.trace.before_frame=[&](int f){frame=f;if(import_feedback)import_feedback->observed.trace.before_frame(f);if((row.id!=Id::Key_FinishDraft || f>=11) && row.id!=Id::Key_InsertVertex)point_at_canvas();if(f==0 && (row.id==Id::Header_PasteObjects || row.id==Id::Key_PasteCurrentPlane)){const auto text=copy_objects(*doc,{id}).text;glfwSetClipboardString(glfwGetCurrentContext(),text.c_str());}physical(row.chord,f==fire_frame);};
        observed.trace.after_frame=[&](const auto&,const auto& fired) {
            if(frame>=fire_frame){
                // This observer is outside the originating window's ID stack;
                // inspect the actual modal window, not a relative popup query.
                const auto modal_is=[](ui::DialogKind kind){const auto* modal=ImGui::GetTopMostPopupModal();return modal && modal->ID==ImHashStr(ui::dialog_definition(kind).name);};
                if(row.id==Id::Key_NewProject || row.id==Id::Key_Recover)dialog_seen=dialog_seen || modal_is(ui::DialogKind::ReplaceWorkspace);
                if(row.id==Id::Key_RemoveImage)dialog_seen=dialog_seen || modal_is(ui::DialogKind::RemoveImage);
                if(row.id==Id::Action_Review_for_project_Ctrl_Alt_F)dialog_seen=dialog_seen || modal_is(ui::DialogKind::SourceVerification);
                if(row.id==Id::Key_ReviewCheckpoint)dialog_seen=dialog_seen || modal_is(ui::DialogKind::DeleteCheckpoint);
            }
            if(frame!=fire_frame)return;
            check(fired.count()==1 && fired[static_cast<std::size_t>(row.id)],"Physical chord missed its real site or reached another: "+stem);
            fired_once=true;selected_tool=observed.trace.tool;task=observed.trace.task;objects=observed.trace.objects_visible;
            const char* title=row.id==Id::Tool_ObjectClassifier?ui::tool_window_label(ui::ToolId::ObjectClassifier):row.id==Id::Tool_PixelClassifier?ui::tool_window_label(ui::ToolId::PixelClassifier):row.id==Id::Tool_Batch?ui::tool_window_label(ui::ToolId::Batch):row.id==Id::Tool_Tma?ui::tool_window_label(ui::ToolId::Tma):row.id==Id::Tool_Channels?ui::tool_window_label(ui::ToolId::Channels):nullptr;
            if(title){const auto* window=ImGui::FindWindowByName(ui::i18n::label(title).c_str());opened=window && window->Active;}
        };
        const auto report=run_project_gui(project,fire_frame+5,"",o);check(fired_once && report.replay_complete,"Wiring input unobserved: "+stem);
        if(row.id==Id::Action_Detect_cells_Ctrl_G)std::cout<<"Cells wiring task="<<task<<" state="<<report.cell_state<<" error="<<report.cell_error<<" state-wait frames="<<report.replay_wait_frames<<"\n";
        bool effect=false;
        if(row.id==Id::Header_OpenProject || row.id==Id::Header_AddImages || row.id==Id::Key_RelinkImage){const auto expected=row.id==Id::Header_OpenProject?detail::ProjectForm::Open:row.id==Id::Header_AddImages?detail::ProjectForm::Add:detail::ProjectForm::Relink;check(shown==expected,"Chooser chord did not show its form: "+stem);effect=true;}
        if(row.id==Id::Key_NewProject || row.id==Id::Key_RemoveImage || row.id==Id::Action_Review_for_project_Ctrl_Alt_F){check(dialog_seen,"Chord did not show its own review dialog: "+stem+" / "+report.file_fingerprint_state+" / "+report.file_fingerprint_error+" / "+report.project_error);effect=true;}
        if(row.id==Id::Key_NextImage || row.id==Id::Key_PreviousImage){check(report.image_switches==1,"Navigation chord did not switch the available image: "+stem);effect=true;}
        if(row.id==Id::Key_Autosave){check(report.autosaves_completed==1 && std::filesystem::exists(std::filesystem::u8path(o.autosave_path)),"F10 did not publish the named recovery");effect=true;}
        if(row.id>=Id::Shell_Rectangle && row.id<=Id::Shell_Hole) {check(selected_tool==int(row.id)-int(Id::Shell_Rectangle)+1,"Bare tool chord selected wrong tool");effect=true;}
        if(row.id==Id::Header_ShowObjects){check(!objects,"O did not hide objects");effect=true;}
        if(row.id==Id::Key_SelectAll){check(report.primary_selection==id,"Select all missed objects");effect=true;}
        if(row.id==Id::Key_Delete){check(doc->object_count()==0,"Delete missed its handler");effect=true;}
        if(row.id==Id::Header_Undo){check(doc->object_count()==0,"Undo missed its history handler");effect=true;}
        if(row.id==Id::Header_Redo || row.id==Id::Key_RedoAlternate){check(doc->object_count()==1,"Redo missed its history handler");effect=true;}
        if(row.id==Id::Header_CopyObjects){check(report.clipboard_copies==1,"Copy missed its clipboard handler");effect=true;}
        if(row.id==Id::Header_PasteObjects || row.id==Id::Key_PasteCurrentPlane){check(report.clipboard_pastes==1 && doc->object_count()==2,"Paste missed its clipboard handler");effect=true;}
        if(row.id==Id::Header_SaveProject){check(report.saves_completed==1,"Save missed its native job");effect=true;}
        if(row.id==Id::Action_Keep_result_Ctrl_K){check(doc->analysis_record_count()==2,"Region Keep did not retain the ready preview");effect=true;}
        if(row.id==Id::Action_Recheck_saved_Ctrl_Shift_K){check(report.analysis_state=="ready" && doc->analysis_record_count()==1,"Region Recheck missed its own saved record");effect=true;}
        if(row.id==Id::Action_Keep_tissue_mask_Ctrl_U){check(doc->tissue_record_count()==1,"Tissue Keep did not retain the ready preview");effect=true;}
        if(row.id==Id::Action_Recheck_saved_mask_Ctrl_Shift_U){check(report.tissue_state=="ready" && doc->tissue_record_count()==1,"Tissue Recheck missed its own saved record");effect=true;}
        if(row.id==Id::Action_Trace_contours_Ctrl_J){check(report.tissue_contour_components>0,"Trace chord did not trace ready tissue");effect=true;}
        if(row.id==Id::Action_Add_annotations_Ctrl_Shift_J){check(report.tissue_contours_published && report.tissue_annotations_added>0,"Add annotations did not publish ready contours");effect=true;}
        if(row.id==Id::Action_Add_cells_Ctrl_Shift_G){check(report.cell_annotations_added>0,"Add cells did not publish ready detections");effect=true;}
        if(row.id==Id::Action_Keep_cell_run_Ctrl_Shift_C){check(doc->cell_record_count()==1,"Cells Keep did not retain the ready preview");effect=true;}
        if(row.id==Id::Action_Recheck_cells_Ctrl_Alt_C){check(report.cell_state=="ready" && doc->cell_record_count()==1,"Cells Recheck missed its own saved record");effect=true;}
        if(row.id==Id::Action_Estimate_stains_F3){check(report.stain_estimate_state=="ready","F3 did not estimate the supplied two-stain fixture");effect=true;}
        if(row.id==Id::Action_Use_estimated_stains_Shift_F3){check(report.stain_estimate_applied,"Shift+F3 did not apply the ready estimate");effect=true;}
        if(row.id==Id::Action_Hash_file_Ctrl_Shift_F){check(report.file_fingerprint_state=="succeeded","Hash chord did not hash its named input");effect=true;}
        if(row.id==Id::Key_FinishDraft){check(doc->object_count()==2,"Finish did not publish its drafted polygon");effect=true;}
        if(row.id==Id::Key_InsertVertex){check(doc->annotation(id).geometry.paths().front().size()==vertex_count_before+1,"Insert did not edit its selected polygon edge");effect=true;}
        if(row.id==Id::Action_Analyze_region_Ctrl_M){check(task==int(ui::WorkflowTask::Region) && report.analysis_state=="ready","Region chord did not reveal/start Region");effect=true;}
        if(row.id==Id::Action_Detect_tissue_Ctrl_D){check(task==int(ui::WorkflowTask::Tissue) && report.tissue_state=="ready","Tissue chord did not reveal/start Tissue");effect=true;}
        if(row.id==Id::Action_Detect_cells_Ctrl_G){check(task==int(ui::WorkflowTask::Cells) && report.cell_state=="ready","Cells chord did not reveal/start Cells");effect=true;}
        if(import_feedback){
            const std::regex notice("Imported 1 objects from ([^\\r\\n]+) \\(Undo available\\)");std::smatch match;
            check(std::regex_search(import_feedback->text,match,notice),"Physical F7 status omitted filename or Undo available");
            check(std::filesystem::equivalent(std::filesystem::u8path(match[1].str()),std::filesystem::u8path(o.interchange_path)),"Physical F7 status named another file");
            write(dir/(stem+"-status.txt"),import_feedback->text);effect=true;
        }
        if(row.id==Id::Tool_ObjectClassifier || row.id==Id::Tool_PixelClassifier || row.id==Id::Tool_Batch || row.id==Id::Tool_Tma || row.id==Id::Tool_Channels){check(opened,"Tool chord did not open its window: "+stem);effect=true;}
        std::cout<<"wiring "<<static_cast<unsigned>(row.id)<<" "<<(effect?"effect":"own-row trace")<<" passed\n";
    }
}
inline void unused_modifiers_and_retired(const std::filesystem::path& dir) {
    std::vector<ImGuiKeyChord> chords;
    for(const auto& row:ui::command_table)if(row.chord)chords.push_back(row.chord|ImGuiMod_Super);
    const ImGuiKeyChord retired[]={ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_M,ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_D,ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_G,ImGuiMod_Ctrl|ImGuiKey_F3,ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiMod_Shift|ImGuiKey_X,ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiMod_Shift|ImGuiKey_Q,ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiMod_Shift|ImGuiKey_F,ImGuiMod_Ctrl|ImGuiKey_F10};
    chords.insert(chords.end(),std::begin(retired),std::end(retired));
    auto project=fixture(dir/"unused-modifiers.qpxproj",32,32);auto doc=project->document(1);doc->add_rectangle({1,1,20,20});const auto revision=doc->revision();
    auto o=options(dir,"unused-modifiers","annotations");TraceScope observed;int frame=0;std::size_t tested=0;
    observed.workflow.trace.before_frame=[&](int f){frame=f;point_at_canvas();if(f<4)return;const auto index=std::size_t((f-4)/3);if(index<chords.size())physical(chords[index],(f-4)%3==0);};
    observed.trace.after_frame=[&](const auto&,const auto& fired){if(frame>=4 && (frame-4)%3==0 && std::size_t((frame-4)/3)<chords.size()){check(fired.none(),"Extra modifier/retired chord reached a real command");++tested;}};
    run_project_gui(project,int(chords.size()*3+5),"",o);check(tested==chords.size() && doc->revision()==revision,"Retired/extra-modifier key changed document or was unobserved");
    std::cout<<"Exact modifiers through real sites and every retired chord changed nothing passed\n";
}
inline void popup_scope(const std::filesystem::path& dir) {
    std::vector<ImGuiKeyChord> chords;for(const auto& row:ui::command_table)if(row.chord)chords.push_back(row.chord);
    for(const std::string kind:{"menu","combo","context"}) {
        auto project=fixture(dir/(kind+"-scope.qpxproj"),32,32);auto o=options(dir,kind+"-scope","workflow");TraceScope observed;observed.trace.probe_all=true;int frame=0;std::size_t tested=0;
        observed.workflow.trace.before_frame=[&](int f){frame=f;if(f==0)glfwSwapInterval(0);if(f>=4){const auto at=std::size_t((f-4)/3);if(at<chords.size())physical(chords[at],(f-4)%3==0);}};
        observed.trace.before_context=[&](int){
            ImGui::Begin("Popup scope owner",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_MenuBar);
            if(kind=="combo") {
                activate(ImGui::GetCurrentWindow(),"Combo scope");
                if(ImGui::BeginCombo("Combo scope","Choice")){ImGui::Selectable("Combo choice");ImGui::EndCombo();}
            } else if(kind=="menu") {
                if(ImGui::BeginMenuBar()) {if(!ImGui::IsPopupOpen("Menu scope"))activate(ImGui::GetCurrentWindow(),"Menu scope");if(ImGui::BeginMenu("Menu scope")){ImGui::MenuItem("Menu choice");ImGui::EndMenu();}ImGui::EndMenuBar();}
            } else {
                if(!ImGui::IsPopupOpen("Context scope"))ImGui::OpenPopup("Context scope");
                if(ImGui::BeginPopupContextWindow("Context scope")){ImGui::MenuItem("Context choice");ImGui::EndPopup();}
            }
            ImGui::End();
        };
        observed.trace.after_frame=[&](const auto& context,const auto& fired){if(frame>=4 && (frame-4)%3==0 && std::size_t((frame-4)/3)<chords.size()){check(context.dialog_open && fired.none(),"Chord escaped key-taking "+kind+" popup");++tested;}};
        run_project_gui(project,int(chords.size()*3+5),"",o);check(tested==chords.size(),"Popup chord coverage incomplete");
    }
    std::cout<<"Every owning chord blocked by menu/combo/context popups, including dismissal-frame Escape, passed\n";
}
inline void canvas_focus(const std::filesystem::path& dir) {
    for(bool over_canvas:{false,true}) {
        const auto stem=over_canvas?"canvas-delete":"panel-delete";auto project=fixture(dir/(std::string(stem)+".qpxproj"),64,64);auto doc=project->document(1);doc->add_rectangle({1,1,30,30});
        auto o=options(dir,stem,"workflow","region");o.inputs={key(2,GuiInputKind::SelectAllKey)};TraceScope observed;ImVec2 row_point{80,120};
        ui::RecordRows records;records.rows={ui::record_row(1,"Kept record","Region","current")};records.two_lines=true;
        observed.workflow.trace.before_frame=[&](int f){
            auto& io=ImGui::GetIO();
            if(f==4 || f==5){io.AddMousePosEvent(row_point.x,row_point.y);io.AddMouseButtonEvent(0,f==4);}
            else if(f>=6 && over_canvas)point_at_canvas(false);else io.AddMousePosEvent(row_point.x,row_point.y);
            physical(ImGuiKey_Delete,f==7);
        };
        observed.trace.before_context=[&](int){
            ImGui::SetNextWindowPos({20,70});ImGui::SetNextWindowSize({310,210});ImGui::Begin("Record list",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);
            std::vector<ui::RecordListItem> trace;const auto previous=ui::record_list_trace;ui::record_list_trace=&trace;
            ui::draw_record_list("Records",records,1);ui::record_list_trace=previous;
            if(!trace.empty()){const auto& b=trace.front().bounds;row_point={float(b.x+b.width/2),float(b.y+b.height/2)};}ImGui::End();
        };
        run_project_gui(project,10,"",o);check(doc->object_count()==(over_canvas?0:1),"Delete ignored canvas-versus-panel focus contract");
    }
    auto project=fixture(dir/"panel-then-tool.qpxproj",32,32);auto o=options(dir,"panel-then-tool","workflow","region");TraceScope observed;int tool=-1;ImVec2 button{80,120};
    observed.workflow.trace.before_frame=[&](int f){
        auto& io=ImGui::GetIO();if(f==3 || f==4){io.AddMousePosEvent(button.x,button.y);io.AddMouseButtonEvent(0,f==3);}else if(f>=5)point_at_canvas(false);
        physical(ImGuiKey_E,f==5);
    };
    observed.trace.before_context=[&](int){ImGui::SetNextWindowPos({20,70});ImGui::SetNextWindowSize({310,210});ImGui::Begin("Panel button",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);ImGui::Button("Panel action");const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();button={(a.x+b.x)*.5f,(a.y+b.y)*.5f};ImGui::End();};
    observed.trace.after_frame=[&](const auto&,const auto&){tool=observed.trace.tool;};run_project_gui(project,10,"",o);check(tool==2,"Pointer over view after panel focus did not select bare tool");
    std::cout<<"Delete remains inert over a focused record panel; canvas Delete and bare tool after panel focus passed\n";
}
inline void page_routing(const std::filesystem::path& dir) {
    for(const bool next:{true,false}) {
        const std::string stem=next?"page-down":"page-up";auto project=fixture(dir/(stem+".qpxproj"),32,32);
        project->add_document(std::make_shared<Document>(Image(32,32,std::vector<std::uint8_t>(32*32*3,80))));
        auto o=options(dir,stem,"workflow");o.settle_tiles=false;TraceScope observed;int frame=-1;char text[64]="Page navigation editor";
        ImGuiID nav=0,active=0;bool editing=false,seen=false;float scroll=0;
        observed.workflow.trace.before_frame=[&](int f){frame=f;physical(ImGuiMod_Ctrl|(next?ImGuiKey_PageDown:ImGuiKey_PageUp),f==8);};
        observed.trace.before_context=[&](int) {
            ImGui::SetNextWindowPos({20,80});ImGui::SetNextWindowSize({310,360});
            ImGui::Begin("Dispatcher page panel",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);
            if(frame==1)ImGui::SetWindowFocus();
            for(int i=0;i<40;++i) {
                ImGui::PushID(i);if(frame==1 && i==12)ImGui::SetKeyboardFocusHere();
                if(i==17)ImGui::InputText("Editor",text,sizeof(text));else ImGui::Button(("Page row "+std::to_string(i)).c_str());ImGui::PopID();
            }
            ImGui::End();
        };
        observed.trace.after_frame=[&](const auto&,const auto&) {
            auto& g=*ImGui::GetCurrentContext();const auto* panel=ImGui::FindWindowByName("Dispatcher page panel");
            if(frame==7){check(panel && g.NavWindow==panel && g.NavId!=0 && g.ActiveId==0 && !g.IO.WantTextInput,"Page test did not establish idle keyboard focus in its panel");nav=g.NavId;active=g.ActiveId;editing=g.IO.WantTextInput;scroll=panel->Scroll.y;}
            if(frame==11) {
                seen=true;std::cout<<stem<<" nav="<<nav<<"->"<<g.NavId<<" active="<<active<<"->"<<g.ActiveId<<" text="<<editing<<"->"<<g.IO.WantTextInput<<" scroll="<<scroll<<"->"<<(panel?panel->Scroll.y:-1)<<"\n";
                check(panel && g.NavId==nav && g.ActiveId==active && g.IO.WantTextInput==editing && panel->Scroll.y==scroll,"Image chord also changed ImGui panel navigation: "+stem);
            }
        };
        const auto report=run_project_gui(project,15,"",o);check(seen && report.image_switches==1,"Page chord did not switch exactly one image: "+stem);
    }
    std::cout<<"Physical Ctrl+PageDown/PageUp switch images without panel navigation passed\n";
}
inline void compact_canvas_focus(const std::filesystem::path& dir) {
    for(const std::string location:{"panel","canvas","tool"}) {
        const bool over_canvas=location=="canvas",in_workspace=location!="tool";
        const std::string stem="compact-"+location+"-delete";
        auto project=fixture(dir/(stem+".qpxproj"),64,64);auto doc=project->document(1);doc->add_rectangle({1,1,30,30});
        auto o=options(dir,stem,"annotations");o.docked_workspace=false;o.layout_path.clear();o.width=1440;o.height=1000;o.settle_tiles=false;
        o.interchange_path=(dir/(stem+".geojson")).u8string();
        const auto probe=run_project_gui(project,5,"",o);const auto view=probe.canvas_content_bounds;
        check(view.width>0 && view.height>0,"Compact probe lost its canvas region");
        o.inputs={key(2,GuiInputKind::SelectAllKey),key(4,GuiInputKind::ExportKey)};
        auto down=key(8,GuiInputKind::LeftDown);down.target=GuiInputTarget::CopyObjects;down.wait_for=GuiInputWait::TargetReady;
        auto up=down;up.frame=9;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs.push_back(down);o.inputs.push_back(up);
        TraceScope observed;int frame=-1,fire=-1;bool checked=false;
        observed.workflow.trace.before_frame=[&](int f) {
            frame=f;if(f==0)glfwSetClipboardString(glfwGetCurrentContext(),"");
            const auto* copied=glfwGetClipboardString(glfwGetCurrentContext());
            if(f>=10 && fire<0 && copied && *copied)fire=f+2;
            if(over_canvas && fire>=0 && f>=fire-1)ImGui::GetIO().AddMousePosEvent(float(view.x+view.width/2),float(view.y+view.height/2));
            if(location=="tool" && f>=10)ImGui::GetIO().AddMousePosEvent(80,140);
            physical(ImGuiKey_Delete,f==fire);
        };
        observed.trace.before_context=[&](int f){if(location=="tool" && f>=10){ImGui::SetNextWindowPos({20,100});ImGui::SetNextWindowSize({300,200});ImGui::Begin("Compact record tool",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);ImGui::SetWindowFocus();ImGui::Selectable("Kept record");ImGui::End();}};
        observed.trace.after_frame=[&](const auto& context,const auto& fired) {
            if(frame!=fire || fire<0)return;checked=true;
            check(!context.text_input && !context.dialog_open,"Compact panel check accidentally acquired a text/dialog owner");
            check(context.workspace_focused==in_workspace && fired[static_cast<std::size_t>(Id::Key_Delete)]==in_workspace,"Compact Delete ignored workspace versus floating-tool ownership");
        };
        const auto report=run_project_gui(project,40,"",o);
        check(checked && report.replay_complete && report.clipboard_copies==1,"Compact check did not consume the actual Copy control click");
        check(doc->object_count()==(in_workspace?0:1),"Compact Delete ignored main-window versus tool-window ownership");
    }
    std::cout<<"Compact actual panel Copy click: Delete active over main panel/view, inert in floating record tool passed\n";
}
inline void cancel_compute_jobs(const std::filesystem::path& dir) {
    for(int family=0;family<4;++family)for(bool replay:{false,true}) {
        const auto stem="cancel-compute-"+std::to_string(family)+(replay?"-replay":"-button");
        auto server=std::make_shared<analysis_tasks_checks::Server>();auto project=std::make_shared<Project>();
        auto doc=std::make_shared<Document>(server);const auto source=dir/(stem+"-source.ppm");
        write(source,"P6\n128 96\n255\n"+std::string(128*96*3,char(80)));project->add_document(doc,"Cancellation",source.u8string());doc->add_rectangle({8,8,44,60});
        auto o=options(dir,stem,"workflow",family==0 || family==3?"region":family==1?"tissue":"cells");
        const auto start=family==0?GuiInputKind::AnalyzeKey:family==1?GuiInputKind::DetectTissueKey:family==2?GuiInputKind::DetectCellsKey:GuiInputKind::EstimateStainsKey;
        const auto cancel=family==0?GuiInputKind::CancelAnalysisKey:family==1?GuiInputKind::CancelTissueKey:family==2?GuiInputKind::CancelCellsKey:GuiInputKind::CancelStainEstimateKey;
        const char* original=family==0?"Cancel analysis":family==1?"Cancel tissue":family==2?"Cancel cells (Ctrl+Alt+G)":"Cancel estimate (Ctrl+F3)";
        const auto retired=family==0?ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_M:family==1?ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_D:family==2?ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiKey_G:ImGuiMod_Ctrl|ImGuiKey_F3;
        o.inputs={key(2,GuiInputKind::SelectAllKey),key(5,start)};if(replay)o.inputs.push_back(key(10,cancel));
        TraceScope observed;int frame=-1;bool busy_stop=false,retired_inert=false,finished=false;auto released=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int f){
            frame=f;if(!finished)point_at_canvas();physical(retired,f==7);
            if(f==4)server->hold=true;
            if(f==7){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!server->entered && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(server->entered,"Cancellation fixture worker never entered held ROI");}
            if(f==12){server->hold=false;released=std::chrono::steady_clock::now();}
            if(f>12){check(std::chrono::steady_clock::now()-released<std::chrono::seconds(5),"Native cancellation did not reach its final state");if(finished)glfwSetWindowShouldClose(glfwGetCurrentContext(),true);else std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        };
        observed.trace.before_context=[&](int f){if(!replay && f==10)activate(task_body(),ui::command_label(ui::workflow_command(original).id));if(finished)discard_fixture_close();};
        observed.trace.after_frame=[&](const auto&,const auto& fired){
            if(frame==7){check(fired.none(),"Retired compute chord fired while job busy");retired_inert=true;}
            if(frame==9){const auto* action=simplification_checks::action(observed.workflow.trace,original);check(action && action->enabled,"Busy native job has no visible Stop");busy_stop=true;}
            if(frame>12 && !simplification_checks::action(observed.workflow.trace,original))finished=true;
        };
        const auto report=run_project_gui(project,200000,"",o);server->hold=false;
        const auto state=family==0?report.analysis_state:family==1?report.tissue_state:family==2?report.cell_state:report.stain_estimate_state;
        check(busy_stop && retired_inert && finished && state=="cancelled","Visible Stop/replay did not cancel held native job: "+stem+" / "+state);
        std::cout<<stem<<": retired inert, visible Stop and native cancelled state passed\n";
    }
}
inline void cancel_exports_and_hash(const std::filesystem::path& dir) {
    const StainTransform transform({{1,0,0},{0,1,0}});
    for(int family=0;family<3;++family)for(bool replay:{false,true}) {
        const auto stem="cancel-io-"+std::to_string(family)+(replay?"-replay":"-button");auto project=fixture(dir/(stem+".qpxproj"),32,32);auto doc=project->document(1);
        auto o=options(dir,stem,"workflow",family==2?"files":"tables");
        if(family==0){o.cell_table_published_objects=true;o.cell_table_path=(dir/(stem+".csv")).u8string();for(int i=0;i<50000;++i)doc->add_cell(Roi::rectangle({1,1,2,2}),Roi::rectangle({1,1,1,1}),"Cell");}
        if(family==1){o.measurement_table_path=(dir/(stem+".csv")).u8string();const auto id=doc->add_rectangle({1,1,20,20});const AnalysisRequest request(*doc,id,transform);const auto result=analyze(request);for(int i=0;i<20000;++i)doc->keep_analysis(request,result);}
        TemporaryHashInput input;
        if(family==2){input.path=dir/(stem+".bin");std::ofstream f(input.path,std::ios::binary);f.seekp(256*1024*1024-1);f.put(0);f.close();o.file_fingerprint_path=input.path.u8string();}
        const auto start=family==0?GuiInputKind::ExportCellTableKey:family==1?GuiInputKind::ExportMeasurementTableKey:GuiInputKind::FingerprintFileKey;
        const auto cancel=family==0?GuiInputKind::CancelCellTableKey:family==1?GuiInputKind::CancelMeasurementTableKey:GuiInputKind::CancelFingerprintFileKey;
        const auto cancel_id=family==0?Id::Action_Cancel_table_Ctrl_Alt_Shift_X:family==1?Id::Action_Cancel_export_Ctrl_Alt_Shift_Q:Id::Key_CancelFingerprint;
        const auto* label=family==2?"Cancel file hash":ui::command_label(cancel_id);
        o.inputs={key(5,start)};if(replay)o.inputs.push_back(key(6,cancel));
        write(std::filesystem::u8path(o.layout_path),read(std::filesystem::u8path(o.layout_path))+"; qupathxx objects-visible=0\n");
        TraceScope observed;int frame=-1;bool stop_seen=false;
        observed.workflow.trace.before_frame=[&](int f){frame=f;point_at_canvas();};
        observed.trace.before_context=[&](int f){if(!replay && f==6)activate(task_body(),label);};
        observed.trace.after_frame=[&](const auto&,const auto&){if(frame==5 || frame==6) {
            if(family<2){const auto* action=simplification_checks::action(observed.workflow.trace,ui::command(cancel_id).legacy_id);stop_seen=stop_seen || (action && action->enabled);}
            else {const auto* action=simplification_checks::action(observed.workflow.trace,"Cancel file hash");stop_seen=stop_seen || (action && action->enabled);}
        }};
        const auto report=run_project_gui(project,15,"",o);
        const auto state=family==0?report.cell_table_state:family==1?report.measurement_table_state:report.file_fingerprint_state;
        check(stop_seen && state=="cancelled","I/O Stop/replay did not cancel busy job: "+stem+" / "+state);
        std::cout<<stem<<": busy visible Stop and native cancelled state passed\n";
    }
}
inline void cancel_recovery_maintenance(const std::filesystem::path& dir) {
    // Keep native recovery session leases inside this writable fixture too.
    // A denied external cache lease skips retention after successful publication.
    const auto cache=dir/"maintenance-cache";std::filesystem::create_directories(cache);
    struct CacheEnvironment {
#ifdef _WIN32
        const char* name="LOCALAPPDATA";
#else
        const char* name="XDG_STATE_HOME";
#endif
        std::optional<std::string> previous;
        explicit CacheEnvironment(const std::string& path){if(const auto* value=std::getenv(name))previous=value;set(path.c_str());}
        void set(const char* value){
#ifdef _WIN32
            _putenv_s(name,value?value:"");
#else
            if(value)setenv(name,value,1);else unsetenv(name);
#endif
        }
        ~CacheEnvironment(){set(previous?previous->c_str():nullptr);}
    } environment(cache.u8string());
    for(bool replay:{false,true}) {
        const std::string stem=replay?"cancel-maintenance-replay":"cancel-maintenance-button";const auto pool=dir/stem;std::filesystem::create_directories(pool);
        auto project=fixture(pool/"base.qpxproj",32,32);project->document(1)->add_rectangle({1,1,20,20});
        write(pool/"base.qpxproj.recovery-bad.qpxrecover","Invalid header, retained as a discovery error");
        auto o=options(pool,stem,"project");o.autosave_path=(pool/"current.qpxrecover").u8string();o.recovery_retention_enabled=true;
        const auto seed=dir/(stem+"-seed.qpxrecover");{ProjectAutosave writer;check(writer.request(*project,seed.u8string()) && writer.wait().state=="ready","Prepare held maintenance checkpoint");}
        struct HeldHeaders {
            std::vector<sqlite3*> databases;
            void release(){for(auto* db:databases){sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);sqlite3_close(db);}databases.clear();}
            ~HeldHeaders(){release();}
            void hold(const std::filesystem::path& path){sqlite3* db=nullptr;check(sqlite3_open(detail::access_path(path).u8string().c_str(),&db)==0,"Open held recovery database");databases.push_back(db);check(sqlite3_exec(db,"PRAGMA journal_mode=DELETE; BEGIN EXCLUSIVE",nullptr,nullptr,nullptr)==0,"Hold recovery header with exclusive SQLite transaction");}
        } held;
        workflow_safety_checks::FeedbackTrace feedback;TraceScope observed;bool stop_seen=false,settings_seen=false,started=false,stopped=false,finished=false,discovery=false;
        const auto began=std::chrono::steady_clock::now();int frame=-1,publication_frame=-1,completion_frame=-1;
        auto diagnostic=[&]{const auto& s=observed.trace.recovery;return stem+" settings_seen="+std::to_string(settings_seen)+" stop_seen="+std::to_string(stop_seen)+" retention_cancelled="+std::to_string(s.cancelled)+" autosaves_completed="+std::to_string(s.completed)+" busy="+std::to_string(s.busy)+" published="+std::to_string(s.published)+" cancel_requested="+std::to_string(s.cancel_requested)+" phase="+s.phase+" frame="+std::to_string(frame);};
        // Drive public kinds through the very same replay_emit path, on observed
        // state rather than assumed logical frames. This is a private test seam.
        observed.trace.replay_input=[&](const auto& emit){
            if(!discovery && frame>=1){emit(key(0,GuiInputKind::FindRecoveriesKey));discovery=true;}
            if(settings_seen && !started) {
                // Each native header read waits up to one second on its lock.
                // 128 held headers cannot finish inside the 30-second deadline,
                // even on a fast disk. Locks stay held until Cancel is observed.
                for(int i=0;i<128;++i){const auto path=pool/("held-"+std::to_string(i)+".qpxrecover");std::filesystem::copy_file(seed,path);held.hold(path);}
                bool locked=false;try{Project::inspect_recovery((pool/"held-0.qpxrecover").u8string());}catch(const std::exception& error){locked=std::string(error.what()).find("locked")!=std::string::npos;}
                check(locked,"Held SQLite header did not block the native recovery reader");
                emit(key(0,GuiInputKind::AutosaveKey));started=true;
            }
            if(replay && stop_seen && !stopped){emit(key(0,GuiInputKind::StopRecoveryMaintenanceKey));stopped=true;}
        };
        observed.workflow.trace.before_frame=[&](int f){frame=f;feedback.observed.trace.before_frame(f);point_at_canvas();check(std::chrono::steady_clock::now()-began<std::chrono::seconds(30),"Recovery maintenance wall deadline: "+diagnostic());std::this_thread::sleep_for(std::chrono::milliseconds(1));};
        observed.trace.before_context=[&](int){if(stop_seen && !replay && !stopped){activate(ImGui::FindWindowByName("Project settings"),ui::presentation_label(Id::Key_StopRecoveryMaintenance,"Stop maintenance (Ctrl+F10)"));stopped=true;}};
        observed.trace.after_frame=[&](const auto&,const auto&){
            const auto* action=simplification_checks::action(observed.workflow.trace,"Stop maintenance (Ctrl+F10)");stop_seen=stop_seen || (action && action->enabled);
            if(auto* settings=ImGui::FindWindowByName("Project settings"))settings_seen=settings_seen || settings->Active;
            const auto& s=observed.trace.recovery;
            if(s.published && publication_frame<0)publication_frame=frame;
            if(s.cancel_requested)held.release();
            if(started && !s.busy && s.completed==1){finished=true;completion_frame=frame;observed.trace.end_session=true;}
        };
        const auto report=run_project_gui(project,200000,"",o);
        check(settings_seen && stop_seen && report.retention_cancelled && report.autosaves_completed==1,"Recovery maintenance Stop/replay did not cancel after publication: "+diagnostic()+" retention_error="+report.retention_error);
        check(std::filesystem::equivalent(std::filesystem::u8path(report.recovery_path),std::filesystem::u8path(o.autosave_path)),"Cancelled maintenance lost the published checkpoint");
        std::cout<<diagnostic()<<" publication_frame="<<publication_frame<<" completion_frame="<<completion_frame<<": actual visible Stop, native retention cancelled and published file retained passed\n";
    }
}
inline void replay_control_keys(const std::filesystem::path& dir) {
    auto draft=[](GuiOptions& o){
        o.inputs.push_back(key(3,GuiInputKind::PolygonKey));int frame=4;
        for(const Point p:{Point{400,300},Point{480,300},Point{480,360}}){auto down=key(frame++,GuiInputKind::LeftDown);down.target=GuiInputTarget::Image;down.x=p.x;down.y=p.y;o.inputs.push_back(down);down.frame=frame++;down.kind=GuiInputKind::LeftUp;o.inputs.push_back(down);}
    };
    for(bool shared:{false,true}) {
        const std::string stem=shared?"escape-release":"escape-alone";auto project=fixture(dir/(stem+".qpxproj"));auto doc=project->document(1);const auto id=doc->add_rectangle({20,20,40,40});
        auto o=options(dir,stem,"project");o.inputs.push_back(key(2,GuiInputKind::SelectAllKey));draft(o);o.inputs.push_back(key(shared?9:11,GuiInputKind::EscapeKey));
        TraceScope observed;int fired=0;bool had_draft=false;observed.workflow.trace.before_frame=[](int f){if(f==0)glfwSwapInterval(0);};
        observed.trace.after_frame=[&](const auto&,const auto& commands){had_draft=had_draft || observed.trace.draft_points==3;fired+=commands[static_cast<std::size_t>(Id::Key_EscapeCanvas)];};
        const auto r=run_project_gui(project,20,"",o);
        check(r.replay_complete && had_draft && !observed.trace.draft_points && fired==1 && doc->object_count()==1 && r.selected_ids==std::vector<std::uint64_t>{id},"One public Escape must drop only the draft, including a trickled press: "+stem+" fired="+std::to_string(fired)+" draft_seen="+std::to_string(had_draft));
    }
    {
        const std::string stem="browser-enter-release";auto project=fixture(dir/(stem+".qpxproj"));auto o=options(dir,stem,"project");draft(o);
        auto down=key(10,GuiInputKind::LeftDown);down.target=GuiInputTarget::ProjectList;down.x=.4;down.y=.2;auto up=down;up.frame=11;up.kind=GuiInputKind::LeftUp;
        auto enter=key(11,GuiInputKind::FinishKey);enter.target=down.target;enter.x=down.x;enter.y=down.y;o.inputs.insert(o.inputs.end(),{down,up,enter});
        TraceScope observed;bool had_draft=false,activated=false;int finished=0;
        observed.workflow.trace.before_frame=[](int f){if(f==0)glfwSwapInterval(0);};
        observed.trace.after_frame=[&](const auto&,const auto& fired){had_draft=had_draft || observed.trace.draft_points==3;activated=activated || observed.trace.project_browser_enter;finished+=fired[static_cast<std::size_t>(Id::Key_FinishDraft)];};
        const auto r=run_project_gui(project,22,"",o);
        check(r.replay_complete && had_draft && activated && finished==0 && project->document(1)->object_count()==0,"Public Enter sharing row release must activate the row and preserve the polygon draft");
    }
    {
        const std::string stem="menu-escape";auto project=fixture(dir/(stem+".qpxproj"));auto doc=project->document(1);const auto id=doc->add_rectangle({20,20,40,40});auto o=options(dir,stem,"project");o.inputs.push_back(key(2,GuiInputKind::SelectAllKey));draft(o);
        auto down=key(10,GuiInputKind::LeftDown);down.target=GuiInputTarget::ProjectList;down.x=.95;down.y=.2;auto up=down;up.frame=11;up.kind=GuiInputKind::LeftUp;o.inputs.insert(o.inputs.end(),{down,up,key(14,GuiInputKind::EscapeKey)});
        TraceScope observed;bool menu_seen=false,closed=false;int frame=-1,fired=0;std::size_t points=0;
        observed.workflow.trace.before_frame=[&](int f){frame=f;if(f==0)glfwSwapInterval(0);};
        observed.trace.after_frame=[&](const auto&,const auto& commands){if(frame==13){menu_seen=!ImGui::GetCurrentContext()->OpenPopupStack.empty();points=observed.trace.draft_points;}if(frame>=16)closed=ImGui::GetCurrentContext()->OpenPopupStack.empty();fired+=commands[static_cast<std::size_t>(Id::Key_EscapeCanvas)];};
        const auto r=run_project_gui(project,22,"",o);
        check(r.replay_complete && menu_seen && closed && points==3 && observed.trace.draft_points==3 && fired==0 && doc->object_count()==1 && r.selected_ids==std::vector<std::uint64_t>{id},"Public Escape must close the row menu without changing the draft or selection");
    }
    {
        const std::string stem="key-pointer-pin";auto project=fixture(dir/(stem+".qpxproj"));auto o=options(dir,stem,"project");auto event=key(5,GuiInputKind::RectangleKey);event.target=GuiInputTarget::Window;event.x=.73;event.y=.39;event.ctrl=event.shift=true;o.inputs={event};
        TraceScope observed;int frame=-1;bool checked=false;observed.workflow.trace.before_frame=[&](int f){frame=f;if(f==0)glfwSwapInterval(0);};
        observed.trace.after_frame=[&](const auto&,const auto&){if(frame==6){const auto& io=ImGui::GetIO();check(std::abs(io.MousePos.x-io.DisplaySize.x*.73f)<1 && std::abs(io.MousePos.y-io.DisplaySize.y*.39f)<1 && io.KeyMods==0,"Key input failed to position/pin its target or injected modifiers");checked=true;}};
        const auto r=run_project_gui(project,12,"",o);check(r.replay_complete && checked,"Pointer pin observation missing");
    }
    std::cout<<"Public control-key replay: Escape alone/release, Enter on row/release, menu Escape and target pointer pin passed\n";
}
inline void workspace_object_keys(const std::filesystem::path& dir) {
    for(const std::string mode:{"delete","select-all","merge"}) {
        auto project=fixture(dir/("workspace-"+mode+".qpxproj"),128,96);auto doc=project->document(1);
        const auto first=doc->add_rectangle({8,8,44,60},"First"),second=doc->add_rectangle({30,8,44,60},"Second");
        auto o=options(dir,"workspace-"+mode,"annotations");o.height=1100;o.settle_tiles=false;
        auto down=key(4,GuiInputKind::LeftDown);down.target=GuiInputTarget::ObjectList;down.x=.4;down.y=.1;auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;
        o.inputs={down,up};if(mode=="merge")o.inputs={key(2,GuiInputKind::SelectAllKey)};
        TraceScope observed;int frame=-1,fire=mode=="merge"?-1:9;bool checked=false;ImVec2 button{};
        const auto chord=mode=="delete"?ImGuiKey_Delete:mode=="select-all"?ImGuiMod_Ctrl|ImGuiKey_A:ImGuiMod_Shift|ImGuiKey_M;
        const auto command=mode=="delete"?Id::Key_Delete:mode=="select-all"?Id::Key_SelectAll:Id::Action_Merge_annotations_M;
        observed.workflow.trace.before_frame=[&](int f){frame=f;if(mode=="merge" && fire>=0){ImGui::GetIO().AddMousePosEvent(button.x,button.y);if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane))ImGui::FocusWindow(pane);}physical(chord,f==fire);};
        observed.trace.before_context=[&](int){if(mode=="merge")if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane))pane->StateStorage.SetInt(pane->GetID(ui::i18n::label("Combine annotations").c_str()),1);};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){
            if(mode=="merge" && frame>=5 && fire<0){if(const auto* action=simplification_checks::action(observed.workflow.trace,"Merge annotations (M)");action && action->enabled){button={float(action->bounds.x+action->bounds.width/2),float(action->bounds.y+action->bounds.height/2)};fire=frame+3;}}
            if(frame!=fire || fire<0)return;
            const auto& g=*ImGui::GetCurrentContext();const auto belongs=[](const ImGuiWindow* window){return window && window->RootWindow->ID==ImHashStr(GuiWorkspace::objects_pane);};
            check(belongs(g.HoveredWindow) && belongs(g.NavWindow) && context.workspace_focused && !context.control_active && fired.count()==1 && fired[static_cast<std::size_t>(command)],"Pressed object chord missed Annotations ownership: "+mode);checked=true;
        };
        const auto report=run_project_gui(project,28,"",o);check(checked && report.replay_complete,"Workspace object key was not observed: "+mode);
        if(mode=="delete")check(doc->object_count()==1 && !doc->objects().count(first) && doc->objects().count(second),"Delete after clicking the Annotations row did not delete exactly that object");
        if(mode=="select-all")check(doc->object_count()==2 && report.selected_ids==std::vector<std::uint64_t>{first,second},"Physical Ctrl+A in Annotations did not select all objects");
        if(mode=="merge")check(doc->object_count()==1 && !doc->objects().count(first) && !doc->objects().count(second),"Shift+M over Merge did not merge the two selected objects");
        std::cout<<"Annotations "<<mode<<": physical chord and native effect passed\n";
    }
    for(bool typing:{false,true}) {
        const std::string stem=typing?"floating-tool-typing":"floating-tool-rectangle";auto project=fixture(dir/(stem+".qpxproj"),32,32);auto o=options(dir,stem,"workflow");o.settle_tiles=false;
        TraceScope observed;int frame=-1;bool checked=false;char text[64]="Class name";
        observed.workflow.trace.before_frame=[&](int f){frame=f;ImGui::GetIO().AddMousePosEvent(80,120);physical(ImGuiKey_R,f==6);};
        observed.trace.before_context=[&](int f){ImGui::SetNextWindowPos({20,70});ImGui::SetNextWindowSize({310,210});ImGui::Begin("Floating class tool",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);ImGui::SetWindowFocus();if(typing){if(f==1)ImGui::SetKeyboardFocusHere();ImGui::InputText("Class",text,sizeof(text));}else ImGui::Button("Choose class");ImGui::End();};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){if(frame==6){check(!context.workspace_focused && context.text_input==typing && fired[static_cast<std::size_t>(Id::Shell_Rectangle)]==!typing,"R in a floating class tool ignored typing scope");check(observed.trace.tool==(typing?0:1),"R in floating tool selected the wrong actual tool");checked=true;}};
        run_project_gui(project,12,"",o);check(checked,"Floating tool R was not observed");
    }
    std::cout<<"R selects Rectangle in a floating tool, and remains blocked in its active text field\n";
}
inline void never_drawn_popup(const std::filesystem::path& dir) {
    auto project=fixture(dir/"never-drawn-popup.qpxproj",32,32);auto o=options(dir,"never-drawn-popup","project");o.settle_tiles=false;
    TraceScope observed;int frame=-1,global=0,ordinary=0;bool orphan=false,panel=false,tool=false;
    observed.workflow.trace.before_frame=[&](int f){frame=f;physical(f==12?ImGuiKey_F4:ImGuiMod_Ctrl|ImGuiKey_4,f==8 || f==12);};
    observed.trace.before_context=[&](int f){if(f==2)ImGui::OpenPopup("Never submitted dispatcher popup");};
    observed.trace.after_frame=[&](const auto&,const auto& fired){
        for(const auto& popup:ImGui::GetCurrentContext()->OpenPopupStack)orphan=orphan || !popup.Window;
        global+=fired[static_cast<std::size_t>(Id::Header_WorkflowPanel)];ordinary+=fired[static_cast<std::size_t>(Id::Tool_ObjectClassifier)];
        if(frame>=9 && frame<12){const auto* window=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);const auto* nav=ImGui::GetCurrentContext()->NavWindow;panel=panel || (window && window->Active && !window->Hidden && nav && nav->RootWindow==window->RootWindow);}
        if(frame>=13){const auto* window=ImGui::FindWindowByName(ui::i18n::label(ui::tool_window_label(ui::ToolId::ObjectClassifier)).c_str());tool=tool || (window && window->Active && !window->Hidden);}
    };
    const auto report=run_project_gui(project,20,"",o);
    std::cout<<"Never-begun popup: null-window entry="<<orphan<<" Global fired="<<global<<" Workflow visible="<<panel<<" NotInText fired="<<ordinary<<" classifier visible="<<tool<<"\n";
    check(report.replay_complete && orphan && global==1 && ordinary==1 && panel && tool,"Undrawn popup silenced real Global/NotInText command sites");
}
inline void run(const std::filesystem::path& directory,const std::string& part) {
    const auto dir=std::filesystem::absolute(directory);std::filesystem::create_directories(dir);table_rules();
    if(part=="scope" || part=="all")every_chord_respects_scope(dir);
    if(part=="wiring" || part=="all")wiring(dir);
    if(part=="focus" || part=="all"){unused_modifiers_and_retired(dir);popup_scope(dir);canvas_focus(dir);}
    if(part=="compact" || part=="all")compact_canvas_focus(dir);
    if(part=="page-routing")page_routing(dir);
    if(part=="orphan-popup")never_drawn_popup(dir);
    if(part=="workspace")workspace_object_keys(dir);
    if(part=="replay-controls")replay_control_keys(dir);
    if(part=="cancel" || part=="all"){cancel_compute_jobs(dir);cancel_exports_and_hash(dir);cancel_recovery_maintenance(dir);refusal_checks::batch_dialog(dir);}
    if(part=="cancel-compute")cancel_compute_jobs(dir);
    if(part=="cancel-io")cancel_exports_and_hash(dir);
    if(part=="cancel-recovery")cancel_recovery_maintenance(dir);
    if(part=="feedback")workflow_safety_checks::export_feedback(dir);
}
}
