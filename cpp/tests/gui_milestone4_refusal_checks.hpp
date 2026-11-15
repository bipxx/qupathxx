#pragma once
#include "gui_reasons_help_checks.hpp"
#include "../gui_analysis_tasks.hpp"
namespace refusal_checks {
using Id=ui::CommandId;
inline GuiOptions settings(const std::filesystem::path& dir,const std::string& name,const char* task,int locale) {
    auto o=options(dir,name,"workflow",task);o.width=1440;o.height=1200;o.settle_tiles=false;
    if(locale==2){auto s=read(std::filesystem::u8path(o.layout_path));const auto p=s.find("language=en");check(p!=std::string::npos,"Fixture language header missing");s.replace(p,11,"language=fr");write(std::filesystem::u8path(o.layout_path),s);}
    return o;
}
inline void limit(std::chrono::steady_clock::time_point start,const std::string& scene) {check(std::chrono::steady_clock::now()-start<std::chrono::seconds(30),"Refusal check timed out: "+scene);}
inline void log_begin(){ImGui::LogToBuffer();ImGui::GetCurrentContext()->LogWindow=nullptr;}
inline std::string log_end(){std::string text=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();return text;}
inline void exports(const std::filesystem::path& dir,bool selected_only=false) {
    for(int locale:{0,2})for(const auto id:{Id::Action_Export_image_cells_Ctrl_Alt_X,Id::Action_Export_project_cells_Ctrl_Shift_X,Id::Action_Export_image_Ctrl_Alt_Q,Id::Action_Export_project_Ctrl_Shift_Q})for(bool replay:{false,true}) {
        const bool cells=id==Id::Action_Export_image_cells_Ctrl_Alt_X || id==Id::Action_Export_project_cells_Ctrl_Shift_X;
        const bool project_scope=id==Id::Action_Export_project_cells_Ctrl_Shift_X || id==Id::Action_Export_project_Ctrl_Shift_Q;
        if(selected_only && id!=Id::Action_Export_image_cells_Ctrl_Alt_X)continue;
        const auto stem="rf2-refused-"+std::to_string(int(id))+"-"+std::to_string(locale)+(replay?"-replay":"-key")+(selected_only?"-selected":"");
        auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image::demo());
        if(selected_only){doc->add_rectangle({0,0,32,32});project->add_document(doc);project->save_new((dir/(stem+".qpxproj")).u8string());}
        auto o=settings(dir,stem,"region",locale);const auto path=dir/(stem+".csv");o.cell_table_path=o.measurement_table_path=path.u8string();if(selected_only)o.inputs={key(2,GuiInputKind::SelectAllKey)};
        dispatch_checks::TraceScope observed;bool request=false,sent=false,done=false,release=false,selected=!selected_only;std::string expected;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){limit(start,stem);dispatch_checks::point_at_canvas();if(release){dispatch_checks::physical(ui::command(id).chord,false);release=false;}if(request && !replay){dispatch_checks::physical(ui::command(id).chord,true);request=false;sent=true;release=true;}};
        observed.trace.replay_input=[&](const auto& emit){if(request && replay){auto input=key(0,cells?GuiInputKind::ExportCellTableKey:GuiInputKind::ExportMeasurementTableKey);input.shift=project_scope;emit(input);request=false;sent=true;}};
        observed.trace.before_context=[&](int){log_begin();};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){
            const auto text=log_end();selected=selected || fired[std::size_t(Id::Key_SelectAll)];
            if(!sent){if(ImGui::GetFrameCount()>2 && selected && !context.dialog_open && !context.text_input)request=true;return;}
            check(observed.trace.task==int(ui::WorkflowTask::Tables),"Refused export did not reveal Tables: "+stem);
            const auto* a=simplification_checks::action(observed.workflow.trace,ui::command(id).legacy_id);if(!a || !a->disabled || a->reason.empty())return;
            expected=ui::i18n::text(selected_only?"Keep a cell run for the selected annotation before exporting kept cells.":project_scope?"Open a project first.":"Open an image first.");
            check(a->reason==expected && a->inline_reason==expected && model_safety_checks::occurrences(text,expected)==1,"Refused export must show its translated availability sentence exactly once: "+stem+"\n"+text);
            done=true;observed.trace.end_session=true;
        };
        const auto r=project_scope?run_gui(*doc,100000,"",o):run_project_gui(project,100000,"",o);
        check(done && (cells?r.cell_table_error:r.measurement_table_error)==expected,"Refused report differs from the one inline sentence: "+stem);
        check(r.cell_table_state=="idle" && r.measurement_table_state=="idle" && !std::filesystem::exists(path),"Refused export started work: "+stem);
    }
    std::cout<<"Export refusals: English/French physical/replay, one inline sentence equals report, no file/job passed\n";
}
inline std::shared_ptr<Project> live_project(const std::filesystem::path& dir,const std::string& stem) {
    std::vector<std::uint8_t> rgb(64*64*3,255);for(int y=0;y<64;++y)for(int x=0;x<64;++x)if((x-25)*(x-25)+(y-32)*(y-32)<=100 || (x-39)*(x-39)+(y-32)*(y-32)<=100)for(int c=0;c<3;++c)rgb[(y*64+x)*3+c]=0;
    auto doc=std::make_shared<Document>(Image(64,64,std::move(rgb)));CellParameters p;p.sigma=0;const auto roi=doc->add_rectangle({0,0,64,64});const StainTransform stain({{1,0,0},{0,1,0}});
    const CellRequest request(*doc,roi,stain,p);const auto result=detect_cells(request);check(result->features().size()==2,"Live cells fixture changed");doc->publish_cells(request,result);
    const auto selected=doc->add_rectangle({0,0,64,64});const CellRequest kept(*doc,selected,stain,p);doc->keep_cells(kept,detect_cells(kept));auto project=std::make_shared<Project>();project->add_document(doc);project->save_new((dir/(stem+".qpxproj")).u8string());return project;
}
inline void cells(const std::filesystem::path& dir) {
    for(int locale:{0,2}) {
        const auto stem="rf2-cells-refused-"+std::to_string(locale);auto project=live_project(dir,stem);auto o=settings(dir,stem,"cells",locale);o.cell_table_published_objects=true;o.inputs={key(2,GuiInputKind::SelectAllKey)};const auto path=dir/(stem+".csv");o.cell_table_path=path.u8string();
        dispatch_checks::TraceScope observed;bool selected=false,sent=false,done=false,release=false;std::string expected;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){limit(start,stem);dispatch_checks::point_at_canvas();if(release){dispatch_checks::physical(ui::command(Id::Action_Export_image_cells_Ctrl_Alt_X).chord,false);release=false;}if(selected && !sent){dispatch_checks::physical(ui::command(Id::Action_Export_image_cells_Ctrl_Alt_X).chord,true);sent=true;release=true;}};
        observed.trace.before_context=[&](int){log_begin();};observed.trace.after_frame=[&](const auto&,const auto& fired){const auto text=log_end();selected=selected || fired[std::size_t(Id::Key_SelectAll)];if(!sent)return;const auto* a=simplification_checks::action(observed.workflow.trace,"Export cell table (Ctrl+Alt+X)");if(!a || !a->disabled)return;expected=ui::i18n::text("In Tables, choose kept runs in Cell source.");check(observed.trace.task==int(ui::WorkflowTask::Cells) && a->reason==expected && a->inline_reason.empty() && model_safety_checks::occurrences(text,expected)==0 && observed.notices && observed.notices->current() && observed.notices->current()->notice.severity==ui::Severity::Warning && observed.notices->current()->notice.source==ui::workflow_task_labels[3],"Refusal moved from panel line to Activity/status notice");done=true;observed.trace.end_session=true;};
        const auto r=run_project_gui(project,100000,"",o);check(done && r.cell_table_error==expected && r.cell_table_state=="idle" && !std::filesystem::exists(path),"Docked Cells refusal changed export or report");
    }
    std::cout<<"Docked Cells live-source key: one translated message/report, task unchanged, no export passed\n";
}
inline void files(const std::filesystem::path& dir) {
    for(int locale:{0,2})for(bool hashing:{false,true}) {
        const auto stem="rf2-files-"+std::to_string(locale)+(hashing?"-busy":"-review");auto project=fixture(dir/(stem+".qpxproj"),32,32);auto o=settings(dir,stem,"region",locale);o.file_fingerprint_associate_source=false;
        dispatch_checks::TemporaryHashInput input;
        if(hashing){input.path=dir/(stem+".bin");std::ofstream f(input.path,std::ios::binary);f.seekp(256*1024*1024-1);f.put(0);f.close();o.file_fingerprint_path=input.path.u8string();}
        dispatch_checks::TraceScope observed;int stage=0;bool done=false;std::optional<ImGuiKeyChord> pending,down;std::string expected;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){limit(start,stem);dispatch_checks::point_at_canvas();if(down){dispatch_checks::physical(*down,false);down.reset();}if(pending){dispatch_checks::physical(*pending,true);down=pending;pending.reset();}};
        observed.trace.before_context=[&](int){log_begin();};observed.trace.replay_input=[&](const auto& emit){if(stage==2 && hashing){emit(key(0,GuiInputKind::AnalyzeKey));stage=3;}};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){const auto text=log_end();
            if(stage==0 && ImGui::GetFrameCount()>2 && !context.dialog_open && !context.text_input){pending=ui::command(hashing?Id::Action_Hash_file_Ctrl_Shift_F:Id::Action_Review_for_project_Ctrl_Alt_F).chord;stage=1;return;}
            if(!hashing && stage==1 && fired[std::size_t(Id::Action_Review_for_project_Ctrl_Alt_F)]){expected=ui::i18n::text("Hash the file first.");check(observed.trace.task==int(ui::WorkflowTask::Files) && model_safety_checks::occurrences(text,expected)==0 && observed.notices && observed.notices->current() && observed.notices->current()->notice.source==ui::workflow_task_labels[6] && observed.notices->current()->notice.severity==ui::Severity::Warning && observed.notices->current()->notice.message=="Hash the file first." && !context.dialog_open,"Refused Review did not post one Files warning without a panel line");done=true;observed.trace.end_session=true;}
            if(hashing && stage==1 && observed.trace.fingerprint_busy){stage=2;return;}
            if(hashing && stage==3 && observed.trace.task==int(ui::WorkflowTask::Region) && observed.trace.fingerprint_busy){pending=ui::command(Id::Action_Hash_file_Ctrl_Shift_F).chord;stage=4;return;}
            if(hashing && stage==4 && fired[std::size_t(Id::Action_Hash_file_Ctrl_Shift_F)]){const auto* a=simplification_checks::action(observed.workflow.trace,"Hash file (Ctrl+Shift+F)");expected=ui::i18n::text("Wait for the current work to finish.");check(observed.trace.task==int(ui::WorkflowTask::Files) && a && a->disabled && a->reason==expected && a->inline_reason==expected && model_safety_checks::occurrences(text,expected)==1,"Busy Hash must reveal Files with only its inline reason");done=true;stage=5;return;}
            if(hashing && stage==5 && !observed.trace.fingerprint_busy)observed.trace.end_session=true;
        };
        const auto r=run_project_gui(project,100000,"",o);check(done && r.file_fingerprint_error==(hashing?std::string{}:expected),"Files refusal report/message changed");
    }
    std::cout<<"Files refused Review/busy Hash from Region: reveal task, translated message or inline only passed\n";
}
inline void cache() {
    detail::analysis_tasks::AvailabilityAnswers answers;std::array<std::string,17> names;int calls=0;
    const auto query=[&](const char*){++calls;return ui::Availability{false,"Wait for the current work to finish."};};
    for(int i=0;i<17;++i){names[i]="action-"+std::to_string(i);const auto a=answers.get(names[i].c_str(),query);check(!a.enabled && std::string(a.reason)=="Wait for the current work to finish.","Full cache lost the 17th answer");}
    answers.get(names[16].c_str(),query);check(answers.count==16 && calls==18,"Full cache must compute uncached answers without throwing");
}
inline void files_dialog(const std::filesystem::path& dir) {
    for(const auto id:{Id::Action_Hash_file_Ctrl_Shift_F,Id::Action_Review_for_project_Ctrl_Alt_F}) {
        const auto stem="rf2-files-dialog-"+std::to_string(int(id));auto project=fixture(dir/(stem+".qpxproj"),32,32);auto o=settings(dir,stem,"region",0);dispatch_checks::TraceScope observed;bool pending=false,sent=false,done=false;const auto start=std::chrono::steady_clock::now();
        observed.workflow.trace.before_frame=[&](int){limit(start,stem);if(pending){dispatch_checks::physical(ui::command(id).chord,true);pending=false;sent=true;}};
        observed.trace.before_context=[&](int){if(!ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))ui::OpenDialog(ui::DialogKind::RemoveKeptResult);ui::DialogState state(true,false);ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,"Files blocked",{},[](ui::DialogControls&){ui::TextUnformatted("Files blocked");},[](ui::DialogAction){return ui::DialogOutcome::Success;});};
        observed.trace.after_frame=[&](const auto& context,const auto& fired){if(!context.dialog_open)return;if(!sent){pending=true;return;}check(!fired[std::size_t(id)] && observed.trace.task==int(ui::WorkflowTask::Region),"Dialog Files key changed task or fired");done=true;observed.trace.end_session=true;};
        const auto r=run_project_gui(project,100000,"",o);check(done && r.file_fingerprint_error.empty() && r.file_fingerprint_state=="idle","Dialog Files key added feedback or work");
    }
    std::cout<<"Both Files keys under a registered dialog: Region unchanged, silent and idle passed\n";
}
inline void batch_dialog(const std::filesystem::path& dir) {
    auto project=std::make_shared<Project>();for(int i=0;i<1000;++i){auto doc=std::make_shared<Document>(Image(16,16,std::vector<std::uint8_t>(16*16*3,80)));doc->add_rectangle({0,0,16,16});project->add_document(doc,"Batch "+std::to_string(i));}
    auto o=settings(dir,"rf2-batch-dialog","region",0);o.batch_output_directory=(dir/"rf2-batch-output").u8string();dispatch_checks::TraceScope observed;int stage=0;bool dialog=false,cancel=false,done=false;const auto start=std::chrono::steady_clock::now();
    observed.workflow.trace.before_frame=[&](int){limit(start,"public batch Cancel under dialog");};
    observed.trace.replay_input=[&](const auto& emit){if(stage==0){emit(key(0,GuiInputKind::RunBatchKey));stage=1;}else if(stage==3 && !cancel){emit(key(0,GuiInputKind::RunBatchKey));emit(key(0,GuiInputKind::CancelBatchKey));cancel=true;}};
    observed.trace.before_context=[&](int){if(dialog){if(!ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult))ui::OpenDialog(ui::DialogKind::RemoveKeptResult);ui::DialogState state(true,false);ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,"Batch stop",{},[](ui::DialogControls&){ui::TextUnformatted("Batch stop");},[](ui::DialogAction){return ui::DialogOutcome::Success;});}};
    observed.trace.after_frame=[&](const auto& context,const auto& fired){if(stage==1 && observed.trace.batch_busy && observed.trace.batch_state=="running"){dialog=true;stage=2;}else if(stage==2 && context.dialog_open && observed.trace.batch_busy)stage=3;
        if(cancel && fired[std::size_t(Id::Key_CancelBatch)])check(context.dialog_open && !fired[std::size_t(Id::Action_Run_batch_Ctrl_Alt_B)],"Dialog must deliver public Cancel and refuse Run together");
        if(cancel && !observed.trace.batch_busy){check(observed.trace.batch_state=="cancelled","Public Cancel did not stop running batch: "+observed.trace.batch_state);done=true;observed.trace.end_session=true;}
    };
    const auto r=run_project_gui(project,100000,"",o);check(done && cancel && r.batch_state=="cancelled","Public dialog batch cancellation was not observed");std::cout<<"Public RunBatchKey/CancelBatchKey: running -> registered dialog -> cancelled, blocked Run inert passed\n";
}
}
