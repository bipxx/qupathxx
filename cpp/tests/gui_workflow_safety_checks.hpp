#pragma once
#include "gui_dialog_contract_checks.hpp"
#include "../gui_raw.hpp"
#include "../gui_batch.hpp"
#include "qupathxx/pixel_stream_file.hpp"
#include "object_forest_fixture.hpp"
#include <thread>
#include "gui_workflow_display_checks.hpp"
#include "gui_review_fix1_checks.hpp"
namespace refusal_checks {inline void batch_dialog(const std::filesystem::path&);}
namespace workflow_safety_checks {
inline GuiInput waited(int frame,GuiInputKind kind,GuiInputWait wait) {auto e=key(frame,kind);e.wait_for=wait;return e;}
inline void blocked_batch() {
    simplification_tools_checks::NativeUi host;GuiOptions options;detail::GuiBatch panel(options);panel.open();
    bool blocked=false;
    Project project;auto doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));auto id=project.add_document(doc);ObjectSelection selected(*doc);
    auto draw=[&]{panel.draw(&project,id,&selected,blocked,{TissueParameters{},nullptr,{},{},CellParameters{},{},{},{},{},GridCellParameters{},AnalysisKind::StainMeans,RoiSamplingPolicy{},false,nullptr,false,std::vector<double>(6),std::vector<double>(3,255)},[](bool){return StainTransform({{1,0,0},{0,1,0}});});};
    host.fit("Project batch",700,1100,draw);
    ImGui::NewFrame();ImGui::LogToBuffer();draw();const std::string text=host.context->LogBuffer.c_str();ImGui::LogFinish();ImGui::Render();
    check(text.find("Enter an output folder.")!=std::string::npos && !host.action("Run batch (Ctrl+Alt+B)").enabled && !panel.busy(),"Batch empty-output reason/disabled Run missing");
    auto submit=[&]{ui::CommandFrame frame;frame.request(ui::CommandId::Action_Run_batch_Ctrl_Alt_B);ui::CommandFrameScope scope(frame);host.frame(draw);};
    blocked=true;submit();check(!panel.busy() && panel.error().empty(),"Blocked replay Run was dispatched");
    blocked=false;submit();check(!panel.busy() && !panel.error().empty(),"Replay Run without output folder started work or hid its error");
    for(int owner=0;owner<3;++owner) {
        ui::CommandFrame frame;frame.request(ui::CommandId::Action_Run_batch_Ctrl_Alt_B);frame.request(ui::CommandId::Key_CancelBatch);
        frame.context.dialog_open=owner!=2;frame.context.text_input=owner==2;
        ui::CommandFrameScope scope(frame);
        const auto requests=panel.take_requests(owner==0);
        check(!requests.first && requests.second,"Blocked frame consumed/dropped replay Cancel");
    }
    std::cout<<"Batch empty-output reason and idle Run passed\n";
}
inline void headless() {dialog_contract_checks::record_removal();workflow_display_checks::headless();blocked_batch();review_fix1_checks::headless();}

// Capture actual rendered text, stopping before the GUI destroys its context.
struct FeedbackTrace {
    simplification_checks::TraceScope observed;std::string text;
    bool installed=false;
    FeedbackTrace() {observed.trace.before_frame=[&](int){if(installed)return;installed=true;
        ImGuiContextHook begin;begin.Type=ImGuiContextHookType_NewFramePost;begin.UserData=this;
        begin.Callback=[](ImGuiContext* ctx,ImGuiContextHook*){ImGui::LogToBuffer();ctx->LogWindow=nullptr;};ImGui::AddContextHook(ImGui::GetCurrentContext(),&begin);
        ImGuiContextHook end;end.Type=ImGuiContextHookType_EndFramePre;end.UserData=this;
        end.Callback=[](ImGuiContext* ctx,ImGuiContextHook* hook){static_cast<FeedbackTrace*>(hook->UserData)->text+=ctx->LogBuffer.c_str();ImGui::LogFinish();};ImGui::AddContextHook(ImGui::GetCurrentContext(),&end);
    };}
};
inline void recheck_never_duplicates(const std::filesystem::path& dir) {
    for(int family=0;family<3;++family) {
        const auto name="recheck-"+std::to_string(family);auto project=fixture(dir/(name+".qpxproj"),128,96);auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60});auto o=options(dir,name,"workflow",family==0?"region":family==1?"tissue":"cells");
        o.inputs={key(3,GuiInputKind::SelectAllKey)};
        if(family==0) {o.inputs.push_back(key(5,GuiInputKind::AnalyzeKey));o.inputs.push_back(key(12,GuiInputKind::KeepAnalysisKey));o.inputs.push_back(key(18,GuiInputKind::RecheckAnalysisKey));o.inputs.push_back(key(28,GuiInputKind::KeepAnalysisKey));}
        else if(family==1) {o.inputs.push_back(key(5,GuiInputKind::DetectTissueKey));o.inputs.push_back(waited(12,GuiInputKind::KeepTissueKey,GuiInputWait::TissueReady));o.inputs.push_back(waited(18,GuiInputKind::RecheckTissueKey,GuiInputWait::TissueArchived));o.inputs.push_back(waited(28,GuiInputKind::KeepTissueKey,GuiInputWait::TissueVerified));}
        else {o.inputs.push_back(key(5,GuiInputKind::DetectCellsKey));o.inputs.push_back(waited(12,GuiInputKind::KeepCellRunKey,GuiInputWait::CellsReady));o.inputs.push_back(waited(18,GuiInputKind::RecheckCellRunKey,GuiInputWait::CellsArchived));o.inputs.push_back(waited(28,GuiInputKind::KeepCellRunKey,GuiInputWait::CellsVerified));}
        FeedbackTrace notice;bool remove_busy_guard=false;const auto before=notice.observed.trace.before_frame;
        notice.observed.trace.before_frame=[&](int frame){const auto* item=simplification_checks::action(notice.observed.trace,family==0?"Remove kept result###Remove region result":"Remove kept result###Remove cell result");if(item && !item->enabled)remove_busy_guard=true;before(frame);};
        const auto report=run_project_gui(project,40,(dir/(name+".ppm")).u8string(),o);check(report.replay_complete,"Recheck replay incomplete");
        if(family==0 || family==2)check(remove_busy_guard,"Region/Cells removal remained enabled during work");
        if(family==0)check(notice.text.find("This run is a recheck of kept result ")!=std::string::npos,"Region recheck notice is hidden");
        if(family==0)check(doc->analysis_record_count()==1 && doc->analysis_status(doc->latest_analysis(id)->id)=="current","Region Recheck duplicated a record or failed verification");
        if(family==1)check(doc->tissue_record_count()==1 && doc->tissue_status(doc->latest_tissue(id)->id)=="current","Tissue Recheck duplicated a record or failed verification");
        if(family==2)check(doc->cell_record_count()==1 && doc->cell_status(doc->latest_cells(id)->id)=="current","Cells Recheck duplicated a record or failed verification");
    }
    std::cout<<"recheck_never_duplicates: region/tissue/cells passed\n";
}
inline void recheck_keeps_inputs(const std::filesystem::path& dir) {
    const auto path=dir/"recheck-inputs.qpxproj";auto project=fixture(path,128,96);auto doc=project->document(1);const auto id=doc->add_rectangle({8,8,44,60});
    auto o=options(dir,"inputs-first","workflow","region");o.analysis_kind=1;o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::AnalyzeKey),key(12,GuiInputKind::KeepAnalysisKey)};
    check(run_project_gui(project,24,"",o).replay_complete && doc->analysis_record_count()==1,"First input session failed");project->save(path.u8string());
    project=Project::open(path.u8string());doc=project->document(1);o=options(dir,"inputs-second","workflow","region");o.analysis_kind=0;
    o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::RecheckAnalysisKey),key(18,GuiInputKind::AnalyzeKey),key(28,GuiInputKind::KeepAnalysisKey)};
    check(run_project_gui(project,40,(dir/"inputs-second.ppm").u8string(),o).replay_complete && doc->analysis_record_count()==2 && doc->latest_analysis(id)->result.kind==AnalysisKind::OpticalDensityMeans,"Recheck rewrote analysis form inputs");
    std::cout<<"recheck_keeps_inputs: save/reopen and fresh optical-density run passed\n";
}
inline void click(std::vector<GuiInput>& inputs,int frame,GuiInputTarget target,GuiInputWait wait=GuiInputWait::Immediate) {
    GuiInput e;e.frame=frame;e.kind=GuiInputKind::LeftDown;e.target=target;e.wait_for=wait;inputs.push_back(e);e.frame++;e.kind=GuiInputKind::LeftUp;e.wait_for=GuiInputWait::Immediate;inputs.push_back(e);
}
inline void keep_once(const std::filesystem::path& dir) {
    // Raw panel uses the same native OME-Zarr fixture as gui_zarr.
    auto project=std::make_shared<Project>();const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    const auto image=project->add_document(std::make_shared<Document>(open_ome_zarr(source.u8string())));auto doc=project->document(image);
    GuiOptions o;o.raw_open=true;o.raw_tab=1;
    RawRegionRequest raw;raw.region={0,0,0,7,5};raw.channels=3;o.raw_request=std::make_shared<const RawRegionRequest>(raw);
    {
        // Use the native panel fixture directly, as gui_zarr does. This avoids
        // a timed tab click through a concurrently laid-out outer workspace.
        simplification_checks::NativeControls host;detail::GuiRaw panel(o);ObjectSelection selection(*doc);
        auto frame=[&] {ImGui::NewFrame();panel.draw(*doc,selection,project.get(),false,[](GuiInputTarget){});ImGui::Render();};
        auto settle=[&](auto ready) {for(int i=0;i<3000;++i){frame();if(ready())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Raw Keep fixture did not settle: "+panel.error());};
        auto press=[&](GuiInputTarget target) {frame();const auto b=panel.bounds(target);check(b.width>0 && b.height>0,"Raw Keep fixture target absent");auto& io=ImGui::GetIO();io.AddMousePosEvent(float(b.x+b.width/2),float(b.y+b.height/2));frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();frame();};
        settle([&]{return panel.state()=="ready";});press(GuiInputTarget::MeasureRawRegion);settle([&]{return panel.analysis_state()=="ready";});press(GuiInputTarget::RawResultsTab);
        check(panel.enabled(GuiInputTarget::KeepRawAnalysis),"First raw Keep unavailable");press(GuiInputTarget::KeepRawAnalysis);
        check(!panel.enabled(GuiInputTarget::KeepRawAnalysis),"Repeated raw Keep still enabled");press(GuiInputTarget::KeepRawAnalysis);
        check(doc->raw_analysis_record_count()==1,"Repeated raw Keep duplicated a record");
        const auto kept=panel.record_id();doc->remove_raw_analysis(kept);frame();
        check(panel.enabled(GuiInputTarget::KeepRawAnalysis),"Removed raw record still disables Keep");press(GuiInputTarget::KeepRawAnalysis);
        check(doc->raw_analysis_record_count()==1 && panel.record_id()!=kept,"Raw result could not be kept after removal");panel.finish(doc.get(),true);
    }
    // Author a bounded historical stream with the independent backend-free forest fixture.
    project=fixture(dir/"stream-project.qpxproj",32,32);doc=project->document(1);const auto roi=doc->add_rectangle({0,0,32,32});
    PixelFeatureOptions features;features.sigmas={0};StainTransform transform({{1,0,0},{0,1,0}});std::vector<std::pair<std::string,std::string>> schema;
    for(const auto& f:pixel_feature_schema(features,transform))schema.emplace_back(f.name,f.unit);
    PixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),features,transform,PixelResolution{});
    PixelStreamOptions settings;settings.block_size=32;PixelPredictionStream stream(*doc,roi,model,settings);const auto file=dir/"keep-once.qpxpixels";save_pixel_stream(stream,file.u8string());
    o=GuiOptions{};o.width=1400;o.height=1000;o.pixel_stream_browser_open=true;o.pixel_stream_read_path=file.u8string();o.pixel_show_overlay=false;
    click(o.inputs,3,GuiInputTarget::OpenPixelStreamFile,GuiInputWait::TargetReady);click(o.inputs,8,GuiInputTarget::VerifyPixelStreamFile,GuiInputWait::PixelStreamReadReady);
    click(o.inputs,14,GuiInputTarget::KeepPixelStreamLink,GuiInputWait::PixelStreamVerified);click(o.inputs,20,GuiInputTarget::KeepPixelStreamLink,GuiInputWait::PixelStreamLinked);
    const auto report=run_project_gui(project,30,(dir/"stream-keep-once.ppm").u8string(),o);check(report.replay_complete && doc->pixel_stream_records().size()==1,"Repeated stream Keep duplicated a record");
    std::cout<<"keep_once: repeated actual raw/stream Keep clicks passed\n";
}
inline std::string image_file(const std::filesystem::path& dir) {const auto path=dir/"add-image.ppm";write(path,"P6\n2 2\n255\n"+std::string(12,'x'));return path.u8string();}
inline void chords_do_not_mutate(const std::filesystem::path& dir) {
    const auto source=image_file(dir);
    for(bool dialog:{true,false}) {
        const auto name=dialog?"modal-chords":"text-chords";auto project=fixture(dir/(std::string(name)+".qpxproj"),128,96);auto doc=project->document(1);
        doc->add_rectangle({1,1,30,30});doc->add_rectangle({50,1,30,30});const auto revision=doc->revision(),count=project->images().size();
        auto o=options(dir,name,"annotations");o.project_image_path=source;o.batch_output_directory=(dir/(std::string(name)+"-batch")).u8string();
        o.inputs={key(3,GuiInputKind::SelectAllKey)};if(dialog)o.inputs.push_back(key(5,GuiInputKind::RemoveImageKey));
        int frame=14;for(auto kind:{GuiInputKind::MergeKey,GuiInputKind::BooleanKey,GuiInputKind::DeleteKey,GuiInputKind::AddImageKey}){o.inputs.push_back(key(frame,kind));frame+=4;}if(dialog)o.inputs.push_back(key(frame,GuiInputKind::RunBatchKey));
        simplification_checks::TraceScope observed;bool owner_seen=false;int stage=0;ImVec2 mouse{};
        observed.trace.before_frame=[&](int f) {
            if(dialog) {if(f>=12 && ui::DialogOwnsInput())owner_seen=true;return;}
            if(f>=6 && stage<3) {
                if(stage==0){const auto* target=simplification_checks::action(observed.trace,"Object name");if(!target)return;const auto b=target->bounds;mouse={float(b.x+b.width/2),float(b.y+b.height/2)};}
                auto& io=ImGui::GetIO();io.AddMousePosEvent(mouse.x,mouse.y);if(stage)io.AddMouseButtonEvent(0,stage==1);++stage;
            }
            if(f>=12 && ImGui::GetIO().WantTextInput)owner_seen=true;
        };
        const auto result=run_project_gui(project,40,(dir/(std::string(name)+".ppm")).u8string(),o);observed.trace.before_frame={};
        check(owner_seen && result.replay_complete,"Hazard replay never established the intended input owner");
        check(doc->revision()==revision && project->images().size()==count && result.batch_state=="idle","Blocked chord mutated document, project or batch");
    }
    std::cout<<"chords_do_not_mutate: registered modal and actual text editor passed\n";
}
inline void add_image_key_never_repeats(const std::filesystem::path& dir) {
    auto project=fixture(dir/"add-key.qpxproj",32,32);const auto count=project->images().size();auto o=options(dir,"add-key","project");o.project_image_path=image_file(dir);
    o.inputs={key(3,GuiInputKind::AddImageKey),key(12,GuiInputKind::AddImageKey)};
    simplification_checks::TraceScope trace;bool add_form=false;
    trace.trace.before_frame=[&](int frame){if(frame>=15 && ImGui::GetIO().WantTextInput)add_form=true;};auto result=run_project_gui(project,24,(dir/"add-key.ppm").u8string(),o);
    check(result.replay_complete && result.project_error.empty() && project->images().size()==count+1,"F8 re-added remembered source");check(add_form,"Second F8 did not reveal/focus the Add form");
    std::cout<<"add_image_key_never_repeats: one add across two F8 presses passed\n";
}
inline bool feedback_names_file(const std::string& text,const std::string& prefix,const std::string& suffix,const std::filesystem::path& expected) {
    auto at=text.find(prefix);if(at==std::string::npos)return false;at+=prefix.size();
    const auto end=suffix.empty()?text.find_first_of("\r\n",at):text.find(suffix,at);
    if(end==std::string::npos)return false;
    const auto same_file=[&](const std::string& value) {return value==std::filesystem::absolute(expected).lexically_normal().u8string();};
    const auto shown=text.substr(at,end-at);if(same_file(shown))return true;
    // ImGui's log may join the next window heading to this text line:
    // "...feedback.geojson { Measurements }". The heading is not a path.
    const auto heading=shown.rfind(" { ");
    return suffix.empty() && heading!=std::string::npos && shown.back()=='}' && same_file(shown.substr(0,heading));
}
inline void export_feedback(const std::filesystem::path& dir) {
    auto project=fixture(dir/"feedback.qpxproj",128,96);auto doc=project->document(1);const auto id=doc->add_rectangle({8,8,44,60});
    auto other=std::make_shared<Document>(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));project->add_document(other);
    const StainTransform transform({{1,0,0},{0,1,0}});
    const AnalysisRequest request(*doc,id,transform,AnalysisKind::StainMeans);doc->keep_analysis(request,analyze(request));
    const auto second=other->add_rectangle({1,1,12,12});const AnalysisRequest request2(*other,second,transform,AnalysisKind::StainMeans);other->keep_analysis(request2,analyze(request2));
    project->set_active_image(1);const auto csv=dir/"feedback-ROI.csv";
    auto o=options(dir,"feedback","workflow","tables");o.measurement_table_path=csv.u8string();
    o.inputs={key(5,GuiInputKind::ExportMeasurementTableKey),waited(12,GuiInputKind::Move,GuiInputWait::MeasurementTableSaved)};
    FeedbackTrace trace;const auto result=run_project_gui(project,34,(dir/"pass-c-feedback.ppm").u8string(),o);
    write(dir/"pass-c-feedback-text.log",trace.text);
    check(trace.text.find("Project: 2 ROI / 0 tissue / 0 cell runs kept")!=std::string::npos && trace.text.find("Current image: 1 ROI / 0 tissue / 0 cell runs kept")!=std::string::npos,"Tables Project count does not sum all images");
    std::cout<<"Project/current-image kept counts passed\n";
    check(result.replay_complete && result.measurement_table_state=="succeeded" && std::filesystem::exists(csv),"Feedback export did not complete: "+result.measurement_table_state+" / "+result.measurement_table_error);
    check(feedback_names_file(trace.text,"Exported 1 rows to ","",csv),"Export feedback omitted row count or absolute dispatch path");
    const auto geo=dir/"feedback.geojson";o=options(dir,"feedback-geo","annotations");o.interchange_path=geo.u8string();o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::ExportKey),key(12,GuiInputKind::ImportKey)};
    FeedbackTrace geo_trace;const auto imported=run_project_gui(project,34,(dir/"pass-c-feedback-geo.ppm").u8string(),o);
    write(dir/"pass-c-feedback-geo-text.log",geo_trace.text);
    check(imported.replay_complete && feedback_names_file(geo_trace.text,"Exported 1 objects to ","",geo) && feedback_names_file(geo_trace.text,"Imported 1 objects from "," (Undo available)",geo),"GeoJSON absolute export/import feedback missing");
    std::cout<<"Table project/current counts, CSV/GeoJSON destinations and import Undo feedback passed\n";
}
inline void cells_empty_export(const std::filesystem::path& dir) {
    auto project=fixture(dir/"cells-empty-export.qpxproj",128,96);auto doc=project->document(1);doc->add_rectangle({8,8,44,60});
    auto o=options(dir,"cells-empty-export","workflow","cells");o.width=1440;o.height=1000;
    o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::DetectCellsKey),waited(12,GuiInputKind::KeepCellRunKey,GuiInputWait::CellsReady)};
    check(run_project_gui(project,24,"",o).replay_complete && doc->cell_record_count()==1,"Empty Cells export fixture needs kept cells");
    o.inputs={key(3,GuiInputKind::SelectAllKey)};
    simplification_checks::TraceScope observed;run_project_gui(project,8,"",o);
    const auto* target=simplification_checks::action(observed.trace,"Export cell table (Ctrl+Alt+X)");check(target && target->enabled,"Cells Export CSV unreachable");const auto bounds=target->bounds;
    simplification_checks::window_click(o.inputs,5,bounds,o.width,o.height);bool focused=false;
    observed.trace.before_frame=[&](int frame){if(frame>=10 && ImGui::GetTopMostPopupModal() && ImGui::GetTopMostPopupModal()->ID==ImHashStr("Choose import file"))focused=true;};
    const auto report=run_project_gui(project,20,(dir/"pass-c-cells-empty-export.ppm").u8string(),o);observed.trace.before_frame={};
    check(report.replay_complete && focused && report.cell_table_state=="idle" && report.cell_table_error.empty(),"Empty Cells export did not open the new-file picker without writing");
    std::cout<<"Cells Export CSV with empty path opens the new-file picker and writes nothing passed\n";
}
inline void polygon_resize(const std::filesystem::path& dir) {
    auto project=fixture(dir/"resize-draft.qpxproj",128,96);auto doc=project->document(1);auto o=options(dir,"resize-draft","annotations");
    o.inputs={key(3,GuiInputKind::PolygonKey)};
    for(const auto item:std::vector<std::pair<int,Point>>{{5,{20,20}},{9,{60,20}},{18,{60,60}}}) {
        auto e=key(item.first,GuiInputKind::LeftDown);e.target=GuiInputTarget::Image;e.x=item.second.x;e.y=item.second.y;o.inputs.push_back(e);e.frame++;e.kind=GuiInputKind::LeftUp;o.inputs.push_back(e);
    }
    auto resize=key(13,GuiInputKind::ResizeWindow);resize.x=1600;resize.y=1100;o.inputs.insert(o.inputs.begin()+5,resize);
    o.inputs.push_back(key(22,GuiInputKind::FinishKey));const auto result=run_project_gui(project,30,(dir/"pass-c-resize-draft.ppm").u8string(),o);
    check(result.replay_complete && doc->object_count()==1 && doc->annotations().front().geometry.area()>0,"Canvas resize discarded polygon draft");
    std::cout<<"Polygon draft survives actual window resize passed\n";
}
inline void workflows(const std::filesystem::path& dir) {
    cells_empty_export(dir);polygon_resize(dir);workflow_display_checks::french(dir);recheck_never_duplicates(dir);recheck_keeps_inputs(dir);keep_once(dir);chords_do_not_mutate(dir);add_image_key_never_repeats(dir);export_feedback(dir);
    refusal_checks::batch_dialog(dir);
}
}
