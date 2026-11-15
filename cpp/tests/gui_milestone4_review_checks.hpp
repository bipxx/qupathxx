#pragma once
#include "gui_reasons_help_checks.hpp"
#include "gui_milestone4_refusal_checks.hpp"
#include "gui_refusal_lifetime_checks.hpp"
namespace milestone4_review_checks {
using Id=ui::CommandId;
inline void deadline(std::chrono::steady_clock::time_point start,const std::string& scene) {
    check(std::chrono::steady_clock::now()-start<std::chrono::seconds(30),"Review check timed out: "+scene);
}
inline void raw_chains(const std::filesystem::path& dir,GLFWwindow* window) {
    model_safety_checks::GlHost host(window);auto tiff=record_fixture::source(dir);
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    auto zarr=std::make_shared<Document>(open_ome_zarr((root/"tests/fixtures/ome-zarr/plain").u8string()));
    Document* current=zarr.get();Project project;project.add_document(tiff);ObjectSelection selection(*tiff);
    GuiOptions o;o.raw_open=true;o.raw_tab=0;o.raw_csv_path=(dir/"raw-chain.csv").u8string();detail::GuiRaw panel(o);
    std::function<void()> draw=[&]{panel.draw(*current,selection,&project,false,[](GuiInputTarget){},nullptr,0,nullptr,{dir.u8string()},true);};
    host.fit("Quantitative channels",1100,1500,draw);host.until(draw,[&]{return panel.ready(*current);},"Zarr metadata not ready");host.settle(draw);
    check(host.action("Open series as a new image").reason==ui::i18n::text("Open an OME-TIFF image to open another series."),"Zarr series reason is false");
    current=tiff.get();host.until(draw,[&]{return panel.ready(*current);},"TIFF metadata not ready");host.settle(draw);
    check(host.action("Open series as a new image").enabled,"Opening TIFF did not enable Open series");
    host.click(panel.bounds(GuiInputTarget::RawRecordsTab),draw);
    constexpr const char* export_action="Export kept raw measurements###Export saved raw measurements";
    check(host.action(export_action).reason==ui::i18n::text("Keep a result first."),"Empty raw records suggest selecting a nonexistent result");
    host.click(panel.bounds(GuiInputTarget::RawDataTab),draw);reasons_help_checks::press(host,"Measure raw channels",draw);
    host.until(draw,[&]{return panel.analysis_state()=="ready";},"Raw measurement did not finish");
    host.click(panel.bounds(GuiInputTarget::RawResultsTab),draw);reasons_help_checks::press(host,"Keep raw measurement",draw);
    host.click(panel.bounds(GuiInputTarget::RawRecordsTab),draw);
    check(tiff->raw_analysis_record_count()==1 && host.action(export_action).enabled,"Keeping raw result did not enable export");
    std::cout<<"Raw reason chains: OME-Zarr -> TIFF series; empty records -> measure/keep -> export enabled passed\n";
}
inline ImGuiWindow* inspector() {
    for(auto* w:ImGui::GetCurrentContext()->Windows)if(w->Active && w->ParentWindow && w->ChildId==ImHashStr("Inspector",0,w->ParentWindow->ID))return w;
    return nullptr;
}
inline std::string compact_tables(const std::filesystem::path& dir,bool keyboard=false) {
    std::vector<std::uint8_t> rgb(64*64*3,255);for(int y=0;y<64;++y)for(int x=0;x<64;++x)if((x-25)*(x-25)+(y-32)*(y-32)<=100 || (x-39)*(x-39)+(y-32)*(y-32)<=100)for(int c=0;c<3;++c)rgb[(y*64+x)*3+c]=0;
    auto doc=std::make_shared<Document>(Image(64,64,std::move(rgb)));const auto roi=doc->add_rectangle({0,0,64,64});CellParameters parameters;parameters.sigma=0;
    const CellRequest request(*doc,roi,StainTransform({{1,0,0},{0,1,0}}),parameters);const auto result=detect_cells(request);check(result->features().size()==2,"Compact cells fixture changed");
    doc->keep_cells(request,result);doc->publish_cells(request,result);
    // Select all makes the last object primary; keep an annotation last so
    // the Cells panel shows its retained-run actions beside the live source.
    const auto selected_roi=doc->add_rectangle({0,0,64,64});
    const CellRequest selected_request(*doc,selected_roi,StainTransform({{1,0,0},{0,1,0}}),parameters);
    doc->keep_cells(selected_request,detect_cells(selected_request));
    auto project=std::make_shared<Project>();project->add_document(doc);project->save_new((dir/(keyboard?"compact-key.qpxproj":"compact-button.qpxproj")).u8string());
    CellTableOptions table;table.published_objects=true;const auto expected=cell_measurements_csv(CellTableRequest(*doc,{},table));
    auto o=options(dir,keyboard?"compact-key":"compact-button","workflow","cells");o.docked_workspace=false;o.layout_path.clear();o.width=1440;o.height=1400;o.cell_table_published_objects=true;o.settle_tiles=false;o.cell_parameters=std::make_shared<CellParameters>(parameters);
    const auto path=dir/(keyboard?"compact-key.csv":"compact-button.csv");o.cell_table_path=path.u8string();o.inputs={key(2,GuiInputKind::SelectAllKey),key(4,GuiInputKind::DetectCellsKey)};
    dispatch_checks::TraceScope observed;observed.trace.observe_task_jobs=true;int stage=-2,mouse=0;bool done=false,selected=false,cells_refused=false,timed_out=false;Rect click_bounds;std::optional<ImGuiKeyChord> pending,down;const auto start=std::chrono::steady_clock::now();
    struct ComboObservation {decltype(ui::combo_observer) previous=ui::combo_observer;~ComboObservation(){ui::combo_observer=previous;}} combos;
    float scope_y=0;ImGuiID scope_id=0;int scope_value=-1;
    ui::combo_observer=[&](const char* label,int current,int){if(std::string_view(label)=="Scope" && ImGui::GetCurrentWindow()==inspector()){scope_y=ImGui::GetCursorScreenPos().y;scope_value=current;scope_id=ImHashStr("##value",0,ImGui::GetCurrentWindow()->GetID(label));}};
    observed.workflow.trace.before_frame=[&](int){
        if(std::chrono::steady_clock::now()-start>=std::chrono::seconds(30)){timed_out=true;observed.trace.end_session=true;return;}
        if(down){dispatch_checks::physical(*down,false);down.reset();}if(pending){dispatch_checks::point_at_canvas();dispatch_checks::physical(*pending,true);down=pending;pending.reset();}
        if(mouse){auto& io=ImGui::GetIO();io.AddMousePosEvent(float(click_bounds.x+click_bounds.width/2),float(click_bounds.y+click_bounds.height/2));if(mouse==2)io.AddMouseButtonEvent(0,true);if(mouse==3)io.AddMouseButtonEvent(0,false);mouse=mouse==3?0:mouse+1;}
    };
    observed.trace.after_frame=[&](const auto& context,const auto& fired){
        if(timed_out)return;
        auto* body=inspector();if(!body)return;
        selected=selected || fired[std::size_t(Id::Key_SelectAll)];
        if(stage==-2){if(selected && !context.dialog_open && !context.text_input){stage=-1;}return;}
        if(stage==-1){if(observed.trace.task_jobs[2].state!="ready")return;const auto* cells=simplification_checks::action(observed.workflow.trace,"Export cell table (Ctrl+Alt+X)");check(cells && cells->disabled && cells->reason==ui::i18n::text("In Tables, choose kept runs in Cell source."),"Cells button lost its live-source refusal");cells_refused=true;stage=0;return;}
        if(stage==0){if(!scope_id)return;if(scope_y<body->InnerClipRect.Min.y || scope_y+70>body->InnerClipRect.Max.y){ImGui::SetScrollY(body,scope_y-body->Pos.y+body->Scroll.y-body->Size.y*.5f);return;}ImGui::ActivateItemByID(scope_id);stage=1;return;}
        if(stage==1){auto* popup=ImGui::FindWindowByName("##Combo_00");if(popup && popup->Active){const int index=0;const auto seed=ImHashData(&index,sizeof(index),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text("Current image"),0,seed));stage=2;}return;}
        if(stage==2){const auto* a=simplification_checks::action(observed.workflow.trace,"Export image cells (Ctrl+Alt+X)");if(!a)return;
            check(!a->disabled,"Compact Tables inherited the Cells rule");
            if(a->bounds.y<body->InnerClipRect.Min.y || a->bounds.y+a->bounds.height>body->InnerClipRect.Max.y){ImGui::SetScrollY(body,float(a->bounds.y-body->Pos.y+body->Scroll.y-body->Size.y*.5));return;}
            check(a->enabled,"Compact Tables export is not usable");if(context.dialog_open || context.text_input)return;if(keyboard)pending=ui::command(Id::Action_Export_image_cells_Ctrl_Alt_X).chord;else {click_bounds=a->bounds;mouse=1;}stage=3;return;
        }
        if(stage==3 && mouse){const auto* a=simplification_checks::action(observed.workflow.trace,"Export image cells (Ctrl+Alt+X)");check(a && a->enabled,"Compact button disappeared during its press");
            // Scope changes can clamp the inspector's scroll after submission.
            // Move again until the rendered rectangle is stable before pressing.
            if(mouse==2 && (a->bounds.x!=click_bounds.x || a->bounds.y!=click_bounds.y || a->bounds.width!=click_bounds.width || a->bounds.height!=click_bounds.height))mouse=1;
            click_bounds=a->bounds;
        }
        if(stage==3 && std::filesystem::exists(path)){done=true;observed.trace.end_session=true;}
    };
    const auto report=run_project_gui(project,100000,(dir/"rf1-compact.ppm").u8string(),o);
    check(done && report.cell_table_state=="succeeded" && read(path)==expected,"Compact live Tables changed export bytes: stage "+std::to_string(stage)+" scope "+std::to_string(scope_value)+" state "+report.cell_table_state+" error "+report.cell_table_error+" path "+report.cell_table_path);
    check(cells_refused,"Cells live-source refusal was not observed on its button");
    if(!keyboard)check(observed.trace.task==int(ui::WorkflowTask::Cells),"Compact export changed the Cells task");
    else check(observed.trace.task==int(ui::WorkflowTask::Tables),"Compact export key did not ask for Tables");
    std::cout<<"Compact live Tables with task Cells: "+std::string(keyboard?"key":"button")+", exact CSV bytes; Cells remains refused passed\n";return read(path);
}
inline void files_and_profile(const std::filesystem::path& dir) {
    auto project=fixture(dir/"files.qpxproj",32,32);auto doc=project->document(1);const auto input=dir/"hash-input.txt";write(input,"Review hash fixture\n");
    auto o=options(dir,"files","workflow","files");o.width=1440;o.height=1200;o.settle_tiles=false;o.file_fingerprint_path=input.u8string();o.file_fingerprint_associate_source=false;
    dispatch_checks::TraceScope observed;int stage=0;bool activate=false,done=false;std::optional<ImGuiKeyChord> pending,down;const auto start=std::chrono::steady_clock::now();
    observed.workflow.trace.before_frame=[&](int){deadline(start,"Files availability");dispatch_checks::point_at_canvas();if(down){dispatch_checks::physical(*down,false);down.reset();}if(pending){dispatch_checks::physical(*pending,true);down=pending;pending.reset();}};
    observed.trace.before_context=[&](int){if(activate){dispatch_checks::activate(dispatch_checks::task_body(),ui::command_label(Id::Action_Hash_file_Ctrl_Shift_F));activate=false;}};
    observed.trace.after_frame=[&](const auto& context,const auto& fired){
        const auto* hash=simplification_checks::action(observed.workflow.trace,"Hash file (Ctrl+Shift+F)");const auto* review=simplification_checks::action(observed.workflow.trace,"Review for project (Ctrl+Alt+F)");if(!hash || !review)return;
        if(stage==0 && hash->enabled && !context.text_input){check(review->disabled && review->reason==ui::i18n::text("Hash the file first."),"Review has no accurate reason before hashing");pending=ui::command(Id::Action_Review_for_project_Ctrl_Alt_F).chord;stage=1;return;}
        if(stage==1 && down){check(fired[std::size_t(Id::Action_Review_for_project_Ctrl_Alt_F)] && review->disabled,"Refused Files key must report intent without starting a review");activate=true;stage=2;return;}
        if(stage==2 && review->enabled){done=true;observed.trace.end_session=true;}
    };
    const auto report=run_project_gui(project,100000,"",o);check(done && report.file_fingerprint_state=="succeeded","Hashing did not enable Review for project");
    doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{.65,.70,.29},{.07,.99,.11}},{1000,1000,1000},"Wide white",{"A","B"}));
    check(doc->image_analysis_settings() && !doc->image_analysis_settings()->rgb8_supported(),"Profile fixture must exceed RGB8 white range");
    auto profile=options(dir,"profile","workflow","region");profile.width=1440;profile.height=1200;bool seen=false;
    observed.trace.before_context={};observed.trace.end_session=false;observed.trace.after_frame=[&](const auto&,const auto&){auto* body=dispatch_checks::task_body();if(!body || !body->WasActive || body->Hidden || body->SkipItems)return;if(const auto* a=simplification_checks::action(observed.workflow.trace,"Use saved stains for analysis")){check(a->disabled && !a->reason.empty() && a->reason==a->inline_reason,"Use profile has no matching disabled/inline reason: disabled "+std::to_string(a->disabled)+" reason "+a->reason+" inline "+a->inline_reason);seen=true;observed.trace.end_session=true;}};
    run_project_gui(project,1000,"",profile);check(seen,"Use profile unavailable action not drawn");
    doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{.65,.70,.29},{.07,.99,.11}},{255,255,255},"Supported white",{"A","B"}));
    check(doc->image_analysis_settings()->rgb8_supported(),"Blocked profile fixture must otherwise be supported");
    auto blocked_profile=options(dir,"profile-blocked","workflow","region");blocked_profile.width=1440;blocked_profile.height=1200;seen=false;observed.trace.end_session=false;
    const auto blocked_start=std::chrono::steady_clock::now();observed.workflow.trace.before_frame=[&](int){deadline(blocked_start,"blocked supported profile");};
    observed.trace.before_context=[&](int){if(!ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))ui::OpenDialog(ui::DialogKind::RemoveKeptResult);ui::DialogState state(true,false);ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,"Blocked profile",{},[](ui::DialogControls&){ui::TextUnformatted("Blocked profile");},[](ui::DialogAction){return ui::DialogOutcome::Success;});};
    observed.trace.after_frame=[&](const auto& context,const auto&){auto* body=dispatch_checks::task_body();if(!context.dialog_open || !body || !body->WasActive || body->Hidden || body->SkipItems)return;if(const auto* a=simplification_checks::action(observed.workflow.trace,"Use saved stains for analysis")){check(a->disabled && a->reason==ui::i18n::text("Finish the current dialog first.") && a->inline_reason==a->reason,"Supported profile lost its dialog-block reason: disabled "+std::to_string(a->disabled)+" reason "+a->reason+" inline "+a->inline_reason);seen=true;observed.trace.end_session=true;}};
    run_project_gui(project,1000,"",blocked_profile);check(seen,"Blocked supported profile was not drawn");
    std::cout<<"Files shared Review gate and Hash-first chain; Use profile disabled tooltip/inline passed\n";
}
inline void blocked_keys(const std::filesystem::path& dir) {
    for(const auto id:{Id::Action_Export_image_cells_Ctrl_Alt_X,Id::Action_Export_project_cells_Ctrl_Shift_X,Id::Action_Export_image_Ctrl_Alt_Q,Id::Action_Export_project_Ctrl_Shift_Q,Id::Action_Hash_file_Ctrl_Shift_F,Id::Action_Review_for_project_Ctrl_Alt_F}) {
        const bool files=id==Id::Action_Hash_file_Ctrl_Shift_F || id==Id::Action_Review_for_project_Ctrl_Alt_F;
        const auto stem="rf1-blocked-"+std::to_string(int(id));auto project=fixture(dir/(stem+".qpxproj"),32,32);
        auto o=options(dir,stem,"workflow",files?"files":"region");o.width=1440;o.height=1200;o.settle_tiles=false;
        const auto path=dir/(stem+".csv");o.cell_table_path=o.measurement_table_path=path.u8string();
        dispatch_checks::TraceScope observed;bool pending=false,down=false,done=false;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){deadline(start,stem);if(pending){dispatch_checks::physical(ui::command(id).chord,true);pending=false;down=true;}};
        observed.trace.before_context=[&](int){
            if(!ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))ui::OpenDialog(ui::DialogKind::RemoveKeptResult);
            ui::DialogState state(true,false);ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,"Review blocked key",{},[](ui::DialogControls&){ui::TextUnformatted("Review blocked key");},[](ui::DialogAction){return ui::DialogOutcome::Success;});
            if(down && files)dispatch_checks::activate(dispatch_checks::task_body(),ui::command_label(id));
        };
        observed.trace.after_frame=[&](const auto& context,const auto& fired){
            if(!context.dialog_open)return;
            if(!down){pending=true;return;}
            check(!fired[std::size_t(id)],"Dialog allowed a refused key: "+stem);
            check(observed.trace.task==int(files?ui::WorkflowTask::Files:ui::WorkflowTask::Region),"Dialog key revealed another task");
            if(files){const auto* action=simplification_checks::action(observed.workflow.trace,ui::command(id).legacy_id);check(action && action->disabled && !action->reason.empty(),"Files disabled widget has no reason");}
            done=true;observed.trace.end_session=true;
        };
        const auto r=run_project_gui(project,100000,"",o);
        check(done && r.cell_table_error.empty() && r.measurement_table_error.empty() && r.cell_table_state=="idle" && r.measurement_table_state=="idle" && r.file_fingerprint_state=="idle" && !std::filesystem::exists(path),"Blocked key started work or reported a refusal");
    }
    std::cout<<"Four export keys silent under dialog; both disabled Files buttons/keys refuse together passed\n";
}
inline void run(const std::filesystem::path& out) {
    const auto dir=std::filesystem::absolute(out);std::filesystem::create_directories(dir);
    refusal_lifetime_checks::run(dir);
    refusal_checks::cache();refusal_checks::exports(dir);refusal_checks::exports(dir,true);refusal_checks::cells(dir);refusal_checks::files(dir);refusal_checks::files_dialog(dir);blocked_keys(dir);const auto button=compact_tables(dir);const auto pressed=compact_tables(dir,true);check(button==pressed,"Compact key and Tables button exported different bytes");files_and_profile(dir);
    check(glfwInit()!=0,"Review GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Milestone 4 review",nullptr,nullptr);check(window,"Review window");glfwMakeContextCurrent(window);raw_chains(dir,window);glfwDestroyWindow(window);glfwTerminate();
}
}
