#pragma once
#include "gui_milestone4_refusal_checks.hpp"
namespace refusal_lifetime_checks {
using namespace refusal_checks;
inline void tables(const std::filesystem::path& dir) {
    for(int locale:{0,2})for(bool project_key:{false,true}) {
        const auto stem="rf3-tables-life-"+std::to_string(locale)+(project_key?"-project":"-image");
        auto doc=std::make_shared<Document>(Image::demo());auto project=std::make_shared<Project>();project->add_document(doc);project->save_new((dir/(stem+".qpxproj")).u8string());
        auto o=settings(dir,stem,"tables",locale);o.cell_table_path=(dir/(stem+".csv")).u8string();dispatch_checks::TraceScope observed;
        int stage=0,samples=0;std::optional<GuiInput> pending;std::string expected;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){limit(start,stem);dispatch_checks::point_at_canvas();};
        observed.trace.replay_input=[&](const auto& emit){if(pending){emit(*pending);pending.reset();}};
        observed.trace.before_context=[&](int){log_begin();};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){const auto text=log_end();if(context.dialog_open || context.text_input)return;
            if(stage==0){auto input=key(0,GuiInputKind::ExportCellTableKey);input.shift=project_key;pending=input;stage=1;return;}
            const auto id=project_key?Id::Action_Export_project_cells_Ctrl_Shift_X:Id::Action_Export_image_cells_Ctrl_Alt_X;
            if(stage==1 && fired[std::size_t(id)]){expected=ui::i18n::text(project_key?"Open a project first.":"Select an annotation with a kept cell run.");check(observed.trace.task==int(ui::WorkflowTask::Tables),"Tables refusal lost its task");pending=key(0,GuiInputKind::KeepCellRunKey);stage=2;return;}
            if(stage==2 && observed.trace.task==int(ui::WorkflowTask::Cells)){
                if(!model_safety_checks::occurrences(text,ui::i18n::text("Select an annotation to use this task.")))return;
                const auto available=detail::analysis_tasks::kept_cells_export_availability(*doc,0,CellTableOptions{},false,false);
                check(model_safety_checks::occurrences(text,expected)==0 && model_safety_checks::occurrences(text,ui::i18n::text(available.reason))==0,"Tables refusal leaked onto the Cells line");
                if(++samples==4)observed.trace.end_session=true;
            }
        };
        const auto r=project_key?run_gui(*doc,100000,"",o):run_project_gui(project,100000,"",o);
        check(samples==4 && r.cell_table_error==expected && r.cell_table_state=="idle" && !std::filesystem::exists(std::filesystem::u8path(o.cell_table_path)),"Tables refusal history changed after Cells navigation");
    }
    std::cout<<"Tables refusal lifetime: Cells navigation stays silent, original report retained, four observed frames passed\n";
}
inline std::shared_ptr<Project> kept_project(const std::filesystem::path& dir,const std::string& stem) {
    auto doc=std::make_shared<Document>(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));const auto roi=doc->add_rectangle({0,0,32,32});const StainTransform stain({{1,0,0},{0,1,0}});CellParameters p;p.sigma=0;
    const CellRequest request(*doc,roi,stain,p);doc->keep_cells(request,detect_cells(request));auto project=std::make_shared<Project>();project->add_document(doc);project->save_new((dir/(stem+".qpxproj")).u8string());return project;
}
inline void cells(const std::filesystem::path& dir) {
    for(int locale:{0,2})for(bool export_after:{false,true}) {
        const auto stem="rf3-cells-life-"+std::to_string(locale)+(export_after?"-export":"-history");auto project=kept_project(dir,stem);auto o=settings(dir,stem,"cells",locale);o.cell_table_published_objects=false;const auto path=dir/(stem+".csv");o.cell_table_path=path.u8string();
        dispatch_checks::TraceScope observed;int stage=0,samples=0;bool release_escape=false;std::optional<GuiInput> pending;std::string expected;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){limit(start,stem+" stage "+std::to_string(stage));dispatch_checks::point_at_canvas();if(release_escape){ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);release_escape=false;}};observed.trace.replay_input=[&](const auto& emit){if(pending){emit(*pending);pending.reset();}};observed.trace.before_context=[&](int){log_begin();};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){const auto text=log_end();if(context.dialog_open || context.text_input)return;const auto* a=simplification_checks::action(observed.workflow.trace,"Export cell table (Ctrl+Alt+X)");
            if(fired[std::size_t(Id::Key_EscapeCanvas)])release_escape=true;
            const bool no_selection=!a && model_safety_checks::occurrences(text,ui::i18n::text("Select an annotation to use this task."));
            expected=ui::i18n::text("Select an annotation with a kept cell run.");
            if(stage==0 && no_selection){pending=key(0,GuiInputKind::ExportCellTableKey);stage=1;return;}
            if(stage==1 && fired[std::size_t(Id::Action_Export_image_cells_Ctrl_Alt_X)]){check(model_safety_checks::occurrences(text,expected)==0 && observed.notices && observed.notices->current() && observed.notices->current()->notice.severity==ui::Severity::Warning && observed.notices->current()->notice.source==ui::workflow_task_labels[3],"Refusal moved from panel line to Activity/status notice");pending=key(0,GuiInputKind::SelectAllKey);stage=2;return;}
            if(stage==2 && a && a->enabled){check(model_safety_checks::occurrences(text,expected)==0,"Available Cells action kept its refusal line");pending=key(0,GuiInputKind::EscapeKey);stage=3;return;}
            if(stage==3 && no_selection){check(model_safety_checks::occurrences(text,expected)==0,"Ended Cells refusal returned after clearing selection");if(++samples==4){if(export_after){pending=key(0,GuiInputKind::SelectAllKey);stage=4;}else observed.trace.end_session=true;}return;}
            if(stage==4 && a && a->enabled){pending=key(0,GuiInputKind::ExportCellTableKey);stage=5;return;}
            if(stage==5 && fired[std::size_t(Id::Action_Export_image_cells_Ctrl_Alt_X)])stage=6;
            if(stage==6 && a && a->enabled && std::filesystem::exists(path)){pending=key(0,GuiInputKind::EscapeKey);stage=7;samples=0;return;}
            if(stage==7 && no_selection){check(model_safety_checks::occurrences(text,expected)==0,"Export revived the old Cells refusal");if(++samples==4)observed.trace.end_session=true;}
        };
        const auto r=run_project_gui(project,100000,"",o);
        check(samples==4 && r.cell_table_error==(export_after?std::string{}:expected),"Cells report did not preserve refusal until export start");
        check(export_after?(r.cell_table_state=="succeeded" && std::filesystem::exists(path)):(r.cell_table_state=="idle" && !std::filesystem::exists(path)),"Cells lifetime scene changed native export state");
    }
    std::cout<<"Cells refusal ends on availability; selection clear cannot revive it; successful export clears report passed\n";
}
inline void dialog(const std::filesystem::path& dir) {
    for(int locale:{0,2})for(bool close_dialog:{false,true}) {
        const auto stem="rf3-dialog-life-"+std::to_string(locale)+(close_dialog?"-closed":"-open");auto project=kept_project(dir,stem);auto o=settings(dir,stem,"cells",locale);o.cell_table_published_objects=false;dispatch_checks::TraceScope observed;
        int stage=0,samples=0;bool show=false,dismiss=false;std::optional<GuiInput> pending;std::string expected;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){limit(start,stem);dispatch_checks::point_at_canvas();};observed.trace.replay_input=[&](const auto& emit){if(pending){emit(*pending);pending.reset();}};
        observed.trace.before_context=[&](int){log_begin();if(show){if(!ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))ui::OpenDialog(ui::DialogKind::RemoveKeptResult);ui::DialogState state(true,false);state.dismiss_requested=dismiss;ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,"Refusal lifetime",{},[](ui::DialogControls&){ImGui::TextUnformatted("Refusal lifetime");},[](ui::DialogAction){return ui::DialogOutcome::Success;});if(dismiss)show=false;}};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){const auto text=log_end();expected=ui::i18n::text("Select an annotation with a kept cell run.");
            if(stage==0 && !context.dialog_open && !context.text_input){pending=key(0,GuiInputKind::ExportCellTableKey);stage=1;return;}
            if(stage==1 && fired[std::size_t(Id::Action_Export_image_cells_Ctrl_Alt_X)]){check(model_safety_checks::occurrences(text,expected)==0 && observed.notices && observed.notices->current() && observed.notices->current()->notice.severity==ui::Severity::Warning && observed.notices->current()->notice.source==ui::workflow_task_labels[3],"Refusal moved from panel line to Activity/status notice");show=true;stage=2;return;}
            if(stage==2 && context.dialog_open){check(model_safety_checks::occurrences(text,expected)==0,"Dialog did not hide standing Cells refusal");if(++samples==4){if(close_dialog){dismiss=true;stage=3;}else observed.trace.end_session=true;}return;}
            if(stage==3 && !context.dialog_open){check(model_safety_checks::occurrences(text,expected)==0 && observed.notices && observed.notices->current() && observed.notices->current()->notice.severity==ui::Severity::Warning && observed.notices->current()->notice.source==ui::workflow_task_labels[3],"Refusal moved from panel line to Activity/status notice");observed.trace.end_session=true;stage=4;}
        };
        const auto r=run_project_gui(project,100000,"",o);check(samples==4 && (!close_dialog || stage==4) && r.cell_table_error==expected && r.cell_table_state=="idle","Dialog changed refusal history or started export");
    }
    std::cout<<"Standing Cells refusal hidden during dialog, original report preserved, line restored after close passed\n";
}
inline void run(const std::filesystem::path& dir) {
    const auto file=dir/"rf3-guard-unwind.bin";try{dispatch_checks::TemporaryHashInput input;input.path=file;write(file,"temporary");throw std::runtime_error("unwind");}catch(const std::runtime_error&){}check(!std::filesystem::exists(file),"Hash input guard did not clean up on failure");
    tables(dir);cells(dir);dialog(dir);
}
}
