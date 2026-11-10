#pragma once
#include "../gui_export.hpp"
#include "qupathxx/interchange.hpp"
#include "qupathxx/analysis.hpp"
#include "qupathxx/cells.hpp"
static void export_sampling_workflows(const std::filesystem::path& directory) {
    auto click=[](std::vector<GuiInput>& events,int frame,GuiInputTarget target) {
        GuiInput down;down.frame=frame;down.kind=GuiInputKind::LeftDown;down.target=target;down.wait_for=GuiInputWait::TargetReady;
        auto up=down;up.frame=frame+1;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;events.push_back(down);events.push_back(up);
    };
    for(const std::string mode:{"new","cancel","confirm","escape","seed-import","scaled-fr"}) {
        const auto project=fixture(directory/(mode+".qpxproj"),128,96);const auto doc=project->document(1);
        doc->add_rectangle({8,8,100,72},"Export ROI");project->save(project->path());
        const auto saved=read(std::filesystem::u8path(project->path()));const auto expected=export_geojson(*doc).text;
        auto o=options(directory,mode,"project");const auto destination=directory/(mode+"-reviewed-export.geojson");o.interchange_path=destination.u8string();
        const bool existing=mode!="new" && mode!="seed-import";if(existing)write(destination,"sentinel");
        o.inputs={key(5,GuiInputKind::ExportKey)};
        if(mode=="cancel")click(o.inputs,12,GuiInputTarget::CancelReplaceExport);
        if(mode=="confirm")click(o.inputs,12,GuiInputTarget::ConfirmReplaceExport);
        if(mode=="escape")o.inputs.push_back(key(12,GuiInputKind::EscapeKey));
        if(mode=="seed-import")o.inputs.push_back(key(12,GuiInputKind::ImportKey));
        if(mode=="scaled-fr") {o.width=800;o.height=900;o.ui_scale=2;auto ini=read(std::filesystem::u8path(o.layout_path));const auto p=ini.find("language=en");ini.replace(p,11,"language=fr");write(std::filesystem::u8path(o.layout_path),ini);}
        const auto r=run_project_gui(project,28,(directory/("export-"+mode+".ppm")).u8string(),o);
        check(r.replay_complete && r.project_error.empty(),"Export replay did not complete: "+mode);
        if(mode=="scaled-fr")check(read(std::filesystem::u8path(o.layout_path)).find("; qupathxx language=fr")!=std::string::npos,"Translated capture must actually use French");
        check(read(destination)==(existing && mode!="confirm"?"sentinel":expected),"Export confirmation changed wrong bytes: "+mode);
        check(read(std::filesystem::u8path(project->path()))==saved,"Export changed project bytes: "+mode);
        check(doc->object_count()==(mode=="seed-import"?2:1),"Import/export option compatibility changed: "+mode);
        check(r.ui_overflow_items==0,"Export UI overflow: "+mode+" / "+r.ui_overflow_label);
    }
    for(const bool recovery:{false,true}) {
        const std::string name=recovery?"guard-owned-recovery":"guard-project";const auto project=fixture(directory/(name+".qpxproj"),128,96);
        project->document(1)->add_rectangle({8,8,100,72});auto o=options(directory,name,"project");
        const auto destination=recovery?directory/"owned.qpxrecover":std::filesystem::u8path(project->path());if(recovery) {write(destination,"owned-recovery");o.autosave_path=destination.u8string();}
        const auto before=read(destination);o.interchange_path=destination.u8string();o.inputs={key(5,GuiInputKind::ExportKey)};
        const auto r=run_project_gui(project,20,(directory/(name+".ppm")).u8string(),o);check(r.replay_complete && read(destination)==before,"GUI export bypassed native project/recovery guard");
    }
    // Run each table shortcut from a different pane, through the same modal.
    for(const std::string kind:{"saved-cells","live-cells","project-cells","image-roi","project-roi"}) {
        const auto project=fixture(directory/(kind+".qpxproj"),64,48);const auto doc=project->document(1);const auto id=doc->add_rectangle({4,4,48,32});
        const StainTransform transform({{1,0,0},{0,1,0}});const CellRequest cell(*doc,id,transform);doc->keep_cells(cell,detect_cells(cell));
        const CellRequest second_cell(*doc,id,transform);const auto latest_cell=doc->keep_cells(second_cell,detect_cells(second_cell));
        check(doc->cell_record_count()==2,"Two-run export fixture did not keep distinct native records");
        const AnalysisRequest analysis(*doc,id,transform);doc->keep_analysis(analysis,analyze(analysis));project->save(project->path());
        auto o=options(directory,kind,"project");const auto destination=directory/(kind+".csv");write(destination,"table-sentinel");
        o.cell_table_path=o.measurement_table_path=destination.u8string();o.cell_table_published_objects=kind=="live-cells";
        const bool measurement=kind=="image-roi" || kind=="project-roi";auto export_key=key(7,measurement?GuiInputKind::ExportMeasurementTableKey:GuiInputKind::ExportCellTableKey);
        export_key.shift=kind=="project-cells" || kind=="project-roi";o.inputs={key(3,GuiInputKind::SelectAllKey),export_key};click(o.inputs,14,GuiInputTarget::ConfirmReplaceExport);
        const auto r=run_project_gui(project,45,(directory/(kind+".ppm")).u8string(),o);
        check(r.replay_complete && (measurement?r.measurement_table_state:r.cell_table_state)=="succeeded","Table confirmation failed: "+kind);
        if(measurement) {const auto expected=export_key.shift?measurements_csv(MeasurementTableRequest(*project,MeasurementTableKind::Roi)):measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Roi));check(read(destination)==expected,"Measurement GUI/native bytes differ");}
        else {CellTableOptions table;table.published_objects=o.cell_table_published_objects;const auto expected=export_key.shift?cell_measurements_csv(CellTableRequest(*project,{},table)):cell_measurements_csv(CellTableRequest(*doc,table.published_objects?std::vector<std::uint64_t>{}:std::vector<std::uint64_t>{latest_cell},table));check(read(destination)==expected,"Cell GUI/native bytes differ");
            if(kind=="saved-cells")check(expected!=cell_measurements_csv(CellTableRequest(*doc,{},table)),"Selected latest-run test cannot distinguish all kept runs");}
    }
    for(const bool calibrated:{false,true})for(const char* task:{"tissue","cells"}) {
        const auto name=std::string("sampling-")+task+(calibrated?"-calibrated":"-unknown");const auto project=fixture(directory/(name+".qpxproj"),128,96);
        const auto doc=project->document(1);doc->add_rectangle({8,8,100,72});if(calibrated)doc->set_calibration(.5,.25);
        auto o=options(directory,name,"workflow",task);o.width=800;o.height=900;o.ui_scale=2;o.inputs={key(3,GuiInputKind::SelectAllKey)};
        if(std::string(task)=="cells") {GuiInput move;move.frame=6;move.kind=GuiInputKind::Move;move.target=GuiInputTarget::Window;move.x=.27;move.y=.65;auto wheel=move;wheel.frame=9;wheel.kind=GuiInputKind::Wheel;wheel.wheel=-4;o.inputs.push_back(move);o.inputs.push_back(wheel);}
        const auto r=run_project_gui(project,18,(directory/(name+".ppm")).u8string(),o);
        check(r.replay_complete && r.selected_ids.size()==1 && r.ui_overflow_items==0,"Sampling caption unreadable: "+name+" / "+r.ui_overflow_label);
    }
    detail::ExportOwnerStamp captured,current;captured.members.emplace_back(7,nullptr,3,2,nullptr);current=captured;
    check(captured==current,"Identical export owners differ");std::get<2>(current.members[0])=4;check(!(captured==current),"Project member edit did not stale export");
    current=captured;++current.selection;check(!(captured==current),"Selection did not stale subset export");
    FileVersion before,after;before.path=after.path="destination";before.identity=after.identity="file";check(detail::same_export_destination(before,after),"Same destination token differs");
    after.modified_token="changed";check(!detail::same_export_destination(before,after),"Changed destination token was accepted");
    std::cout<<"Export/sampling workflow: create, cancel/confirm/Escape, cross-pane table routes, protected recovery/project, seeded import and scaled captions passed\n";
}
