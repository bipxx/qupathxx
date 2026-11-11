#pragma once
#include "../gui_workflow_ui.hpp"
#include "../gui_measurements.hpp"
#include "qupathxx/analysis.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/object_properties.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace simplification_checks {
struct TraceScope {
    ui::WorkflowTrace trace;
    ui::WorkflowTrace* previous=ui::workflow_trace;
    TraceScope(){ui::workflow_trace=&trace;}
    ~TraceScope(){ui::workflow_trace=previous;}
};
inline bool exact_double(double left,double right) {
    return std::memcmp(&left,&right,sizeof(double))==0;
}
struct NativeControls {
    ImGuiContext* previous_context=ImGui::GetCurrentContext();
    ui::i18n::State previous_language=ui::i18n::state;
    ui::design::State previous_theme=ui::design::state;
    ImGuiContext* context=ImGui::CreateContext();
    TraceScope trace;
    Document document{Image(128,96,std::vector<std::uint8_t>(128*96*3,80))};
    std::uint64_t object=0;
    double number=std::nextafter(.05,1.);
    float width=336,scale=1,measurement_height=0;
    Rect input_bounds{},action_bounds{};
    ImGuiID action_id=0,expected_action_id=0,expected_input_id=0;
    int calls=0;
    bool numeric_mode=false,measurement_mode=false,means_mode=false;
    AnalysisResult native_means;std::uint64_t native_means_id=0;
    NativeControls() {
        ImGui::SetCurrentContext(context);auto& io=ImGui::GetIO();
        io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1600,1200};io.DeltaTime=1.f/60;
        io.ConfigInputTrickleEventQueue=false;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);
        object=document.add_cell(Roi::rectangle({8,8,44,60}),Roi::ellipse({18,20,10,12}),"Cell###literal");
        document.set_calibration(.5,.25);
        settle();
    }
    ~NativeControls() {
        ImGui::DestroyContext(context);ImGui::SetCurrentContext(previous_context);
        ui::i18n::state=previous_language;ui::design::state=previous_theme;
    }
    void frame() {
        ImGui::NewFrame();ui::begin_frame();ui::reset_workflow_trace();
        ImGui::GetStyle().FontSizeBase=16*scale;
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({width,1000});
        ImGui::Begin("Workflow controls fixture",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDecoration);
        auto* window=ImGui::GetCurrentWindow();
        if(numeric_mode) {
            expected_input_id=ImHashStr("##value",0,ImHashStr("Missing tissue fraction",0,window->IDStack.back()));
            ui::CompactInputDouble("Minimum tissue coverage###Missing tissue fraction",&number);
            const auto p=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();input_bounds={p.x,p.y,size.x,size.y};
        } else if(means_mode) {
            ui::WorkflowMeans("kept","Kept result",native_means.kind,native_means.means,native_means_id,native_means.nucleus);
            check(window->DC.CursorMaxPos.x<=window->WorkRect.Max.x+1 && !ui::layout_stats.overflow_items,
                "localized result means extend horizontally outside the native panel");
            check(ImGui::GetCursorPosY()<800,"localized result means exceed the bounded panel height");
        } else if(measurement_mode) {
            const auto before=ImGui::GetCursorPosY();detail::draw_shape_measurements(document,object,1);
            measurement_height=ImGui::GetCursorPosY()-before;
            check(window->DC.CursorMaxPos.x<=window->WorkRect.Max.x+1,"measurement rows extend outside the native panel");
            check(measurement_height<600,"compact measurements exceed the enlarged panel height");
        } else {
            constexpr auto original="Analyze region (Ctrl+M)";
            expected_action_id=window->GetID(original);
            if(ui::WorkflowAction(original,"Run analysis","Ctrl+M",ui::design::Action::Primary))++calls;
            action_id=ImGui::GetItemID();
            const auto p=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();action_bounds={p.x,p.y,size.x,size.y};
            const auto explicit_expected=window->GetID("Go to tables###ROI tables");
            ui::WorkflowAction("Go to tables###ROI tables","View kept results");
            check(ImGui::GetItemID()==explicit_expected,"caption migration changed an explicit historical action ID");
        }
        ImGui::End();ImGui::Render();
    }
    void settle(){for(int i=0;i<4;++i)frame();}
    void click(Rect rect,bool field=false) {
        auto& io=ImGui::GetIO();
        io.AddMousePosEvent(float(field?rect.x+rect.width-24:rect.x+rect.width/2),float(rect.y+rect.height/2));settle();
        io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);settle();
    }
};
inline void native_close_aliases() {
    for(const bool docked:{false,true})for(const float scale:{1.f,2.f}) {
        auto* previous_context=ImGui::GetCurrentContext();
        const auto previous_language=ui::i18n::state;const auto previous_theme=ui::design::state;
        auto* context=ImGui::CreateContext();ImGui::SetCurrentContext(context);
        struct Restore {
            ImGuiContext* created;ImGuiContext* previous;ui::i18n::State language;ui::design::State theme;
            ~Restore(){ImGui::DestroyContext(created);ImGui::SetCurrentContext(previous);ui::i18n::state=language;ui::design::state=theme;}
        } restore{context,previous_context,previous_language,previous_theme};
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1200,900};io.DeltaTime=1.f/60;
        io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.ConfigInputTrickleEventQueue=false;
        io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);ImGui::GetStyle().ScaleAllSizes(scale);ImGui::GetStyle().FontSizeBase=16*scale;
        TraceScope observed;bool target_open=true,sibling_open=true;int cancelled=0,alias_callbacks=0;
        std::optional<Rect> close;Rect cancel;ImGuiID dock_id=0,cancel_id=0;
        auto frame=[&] {
            ImGui::NewFrame();ui::begin_frame();ui::reset_workflow_trace();close.reset();
            ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Native alias host",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking);
            dock_id=ImGui::GetID("Native alias dockspace");
            if(docked) {
                if(!ImGui::DockBuilderGetNode(dock_id)) {
                    ImGui::DockBuilderAddNode(dock_id,ImGuiDockNodeFlags_DockSpace);
                    ImGui::DockBuilderSetNodePos(dock_id,ImGui::GetCursorScreenPos());
                    ImGui::DockBuilderSetNodeSize(dock_id,ImGui::GetContentRegionAvail());
                    ImGui::DockBuilderDockWindow("Alias target",dock_id);ImGui::DockBuilderDockWindow("Alias sibling",dock_id);ImGui::DockBuilderFinish(dock_id);
                }
                ImGui::DockSpace(dock_id,ImGui::GetContentRegionAvail(),ImGuiDockNodeFlags_NoWindowMenuButton);
            }
            ImGui::End();
            if(target_open) {
                if(!docked){ImGui::SetNextWindowPos({50,55});ImGui::SetNextWindowSize({340*scale,250*scale});}
                if(ImGui::Begin("Alias target",&target_open,ImGuiWindowFlags_NoCollapse)) {
                    cancel_id=ImGui::GetCurrentWindow()->GetID("Cancel operation");
                    if(ui::WorkflowIcon("Cancel operation","Cancel",ui::Icon::Stop))++cancelled;
                    const auto p=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();cancel={p.x,p.y,size.x,size.y};
                    const auto before=context->LastItemData;
                    ui::NativeToolCloseTarget("Hide panel",[&](Rect value){close=value;++alias_callbacks;});
                    check(context->LastItemData.ID==before.ID && context->LastItemData.Rect.Min.x==before.Rect.Min.x &&
                        context->LastItemData.Rect.Min.y==before.Rect.Min.y,"native close alias changed the last body widget/hit target");
                    ImGui::TextUnformatted("A running job remains independent of pane visibility.");
                }
                ImGui::End();
            }
            if(sibling_open) {
                if(!docked){ImGui::SetNextWindowPos({780,55});ImGui::SetNextWindowSize({300,250});}
                ImGui::Begin("Alias sibling",&sibling_open,ImGuiWindowFlags_NoCollapse);
                ImGui::TextUnformatted("Sibling settings");ImGui::End();
            }
            ImGui::Render();
        };
        for(int i=0;i<4;++i)frame();
        if(docked){ImGui::SetWindowFocus("Alias target");for(int i=0;i<4;++i)frame();}
        check(close && alias_callbacks>0,"native tool close alias has no floating/tab title target");
        const auto found=observed.trace.actions.find("Hide panel");
        check(found!=observed.trace.actions.end() && found->second.enabled &&
            found->second.bounds.x==close->x && found->second.bounds.y==close->y,"native close alias did not retain the legacy reachable target");
        // Exercise the nullable-shortcut utility path while actually hovering it.
        io.AddMousePosEvent(float(cancel.x+cancel.width/2),float(cancel.y+cancel.height/2));for(int i=0;i<40;++i)frame();
        check(context->HoveredId==cancel_id && target_open && sibling_open && !cancelled,"hovering a utility without a shortcut changed a tool/job state");
        if(docked) {const auto* target=ImGui::FindWindowByName("Alias target");const auto* sibling=ImGui::FindWindowByName("Alias sibling");check(target && sibling && target->DockId==dock_id && sibling->DockId==dock_id,"native close fixture did not actually dock both tools");}
        check(close.has_value(),"close alias disappeared while the tool remained open");const auto hit=*close;
        io.AddMousePosEvent(float(hit.x+hit.width/2),float(hit.y+hit.height/2));for(int i=0;i<4;++i)frame();
        io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);for(int i=0;i<4;++i)frame();
        check(!target_open && sibling_open && !cancelled,"native close alias failed to close only its tool or cancelled work");
        const auto* sibling=ImGui::FindWindowByName("Alias sibling");
        check(sibling && sibling->Active && (!docked || (sibling->DockId==dock_id && sibling->DockTabIsVisible)),"closing one tab closed or orphaned its sibling: docked="+std::to_string(docked)+" scale="+std::to_string(scale)+" sibling="+std::to_string(bool(sibling))+" active="+std::to_string(sibling?sibling->Active:false)+" tab="+std::to_string(sibling?sibling->DockTabIsVisible:false)+" dock="+std::to_string(sibling?sibling->DockId:0)+" host="+std::to_string(dock_id));
        const auto previous_alias_callbacks=alias_callbacks;target_open=true;
        if(docked) {ImGui::DockBuilderDockWindow("Alias target",dock_id);ImGui::DockBuilderFinish(dock_id);}
        for(int i=0;i<4;++i)frame();
        ImGui::SetWindowFocus("Alias target");for(int i=0;i<4;++i)frame();
        const auto* reopened=ImGui::FindWindowByName("Alias target");sibling=ImGui::FindWindowByName("Alias sibling");
        check(target_open && sibling_open && reopened && reopened->Active && reopened->HasCloseButton && close &&
            alias_callbacks>previous_alias_callbacks && sibling && sibling->Active &&
            (!docked || (reopened->DockId==dock_id && reopened->DockTabIsVisible && sibling->DockId==dock_id)),
            "reopening the tool lost its native close target, dock or sibling");
        check(!cancelled,"reopening a hidden tool cancelled work");
        io.AddMousePosEvent(float(cancel.x+cancel.width/2),float(cancel.y+cancel.height/2));for(int i=0;i<4;++i)frame();
        check(context->HoveredId==cancel_id,"reopened Cancel glyph has no actual native hit target");
        io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);for(int i=0;i<4;++i)frame();
        check(cancelled==1 && target_open && sibling_open && close && ImGui::FindWindowByName("Alias target")->Active &&
            ImGui::FindWindowByName("Alias sibling")->Active,"Cancel did not dispatch exactly once independently of both open tools");
    }
    std::cout<<"Native close aliases: floating/tab X, reopen, sibling retention, nullable-shortcut hover and independent Cancel passed\n";
}
inline void headless() {
    const std::array<double,11> numbers={0.,-0.,std::nextafter(.05,0.),.05,std::nextafter(.05,1.),1.2345678901234567,
        -987654.3210123456,std::numeric_limits<double>::min(),std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::max(),-std::numeric_limits<double>::max()};
    for(const auto value:numbers) {
        const auto format=ui::compact_double_format(value);std::array<char,96> text{};
        const auto length=std::snprintf(text.data(),text.size(),format.data(),value);
        check(length>0 && std::size_t(length)<text.size(),"compact numeric format is unbounded");
        char* end=nullptr;const auto parsed=std::strtod(text.data(),&end);
        check(end==text.data()+length && exact_double(value,parsed),"compact numeric format changes an editable native value");
    }
    NativeControls f;
    for(const char* language:{"en","fr"}) {
        ui::i18n::choose(*ui::i18n::exact_language(language));f.settle();
        check(f.action_id==f.expected_action_id,"localized short button changed its historical ImGui ID");
        const auto before=f.calls;f.click(f.action_bounds);check(f.calls==before+1,"shortened native action did not dispatch exactly once");
    }
    f.numeric_mode=true;
    for(const auto value:numbers) {
        f.number=value;f.settle();f.click(f.input_bounds,true);
        check(ImGui::GetCurrentContext()->ActiveId==f.expected_input_id,"renamed numeric field changed its historical PushID/value identity");
        auto& io=ImGui::GetIO();io.AddKeyEvent(ImGuiKey_Tab,true);f.frame();io.AddKeyEvent(ImGuiKey_Tab,false);f.settle();
        check(exact_double(f.number,value),"focusing/blurring an unedited numeric field changed its exact native value");
    }
    f.numeric_mode=false;f.measurement_mode=true;
    for(int language=0;language<ui::i18n::language_count;++language)for(const auto size:{std::pair{336.f,1.f},std::pair{400.f,2.f}}) {
        ui::i18n::choose(language);f.width=size.first;f.scale=size.second;
        const auto before=f.document.shape_measurements(f.object);const auto revision=f.document.revision();f.settle();
        const auto after=f.document.shape_measurements(f.object);
        check(after.roi.area_px2==before.roi.area_px2 && after.roi.area_um2==before.roi.area_um2 &&
            after.nucleus && before.nucleus && after.nucleus->area_px2==before.nucleus->area_px2 &&
            after.nucleus->length_um==before.nucleus->length_um && f.document.revision()==revision,
            "compact localized measurement rendering changed native facts");
    }
    f.measurement_mode=false;f.means_mode=true;
    const StainTransform native_transform({{1,0,0},{0,1,0}});
    const AnalysisRequest native_request(f.document,f.object,native_transform,AnalysisKind::StainMeans);
    f.native_means=analyze(native_request);f.native_means_id=f.document.keep_analysis(native_request,f.native_means);
    const auto means_revision=f.document.revision();
    for(int language=0;language<ui::i18n::language_count;++language)for(const auto size:{std::pair{336.f,1.f},std::pair{336.f,2.f},std::pair{400.f,2.f}}) {
        ui::i18n::choose(language);f.width=size.first;f.scale=size.second;f.settle();
        const auto rendered=f.trace.trace.results.find("kept");const auto stored=f.document.analysis_record(f.native_means_id);
        check(rendered!=f.trace.trace.results.end() && rendered->second.record_id==f.native_means_id &&
            rendered->second.kind==AnalysisKind::StainMeans && rendered->second.means==stored.result.means &&
            stored.result.input_sha256==f.native_means.input_sha256 && f.document.revision()==means_revision,
            "localized result rendering changed the captured native values or retained identity");
    }
    f.means_mode=false;f.measurement_mode=true;
    f.document.clear_calibration();f.width=336;f.scale=1;f.settle();
    check(!f.document.shape_measurements(f.object).roi.area_um2,"uncalibrated measurement fixture acquired physical units");
    native_close_aliases();
    std::cout<<"Simplification controls: stable localized IDs, exact numeric focus/blur, all16-locale compact native measurements passed\n";
}

inline void window_click(std::vector<GuiInput>& inputs,int frame,Rect bounds,int width,int height) {
    check(bounds.width>0 && bounds.height>0,"workflow action has no native click bounds");
    GuiInput down;down.frame=frame;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;
    down.x=(bounds.x+bounds.width/2)/width;down.y=(bounds.y+bounds.height/2)/height;
    auto up=down;up.frame=frame+1;up.kind=GuiInputKind::LeftUp;inputs.push_back(down);inputs.push_back(up);
}
inline const ui::WorkflowActionTrace* action(const ui::WorkflowTrace& trace,const char* original) {
    const auto found=trace.actions.find(original);return found==trace.actions.end()?nullptr:&found->second;
}
inline void workflows(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    for(const bool scaled:{false,true}) {
        const auto name=std::string("measurement-view")+(scaled?"-fr-2x":"");
        const auto project=fixture(directory/(name+".qpxproj"),128,96);const auto doc=project->document(1);
        const auto id=doc->add_cell(Roi::rectangle({8,8,44,60}),Roi::ellipse({18,20,10,12}),"Cell###literal");
        doc->set_calibration(.5,.25);project->save(project->path());const auto bytes=read(project->path());
        const auto revision=doc->revision();const auto native=doc->shape_measurements(id);
        auto o=options(directory,name,"annotations","measurements",scaled?2:1);
        if(scaled){o.width=1000;o.height=1100;auto ini=read(o.layout_path);ini.replace(ini.find("language=en"),11,"language=fr");write(o.layout_path,ini);}
        o.inputs={key(3,GuiInputKind::SelectAllKey)};
        const auto report=run_project_gui(project,16,(directory/(name+".ppm")).u8string(),o);
        check(report.replay_complete && !report.ui_overflow_items,"compact measurement panel is not usable at normal/French200 size");
        check(report.primary_selection==id && doc->shape_measurements(id).roi.area_um2==native.roi.area_um2 &&
            doc->revision()==revision && read(project->path())==bytes,"measurement presentation changed selection, native values or saved bytes");
    }
    {
        const auto project=fixture(directory/"lifecycle.qpxproj",128,96);const auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60},"ROI###literal");doc->set_calibration(.5,.25);project->save(project->path());
        const auto bytes=read(project->path());TraceScope observed;
        auto o=options(directory,"lifecycle","workflow","region");o.analysis_white={201,212,223};
        o.inputs={key(3,GuiInputKind::SelectAllKey)};
        auto empty=run_project_gui(project,12,(directory/"no-result.ppm").u8string(),o);
        const auto* unavailable=action(observed.trace,"Go to tables###ROI tables");
        check(empty.replay_complete && (!unavailable || !unavailable->enabled) && !doc->analysis_record_count(),"no-result view offers a misleading ready result-table action");
        o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::AnalyzeKey),key(9,GuiInputKind::KeepAnalysisKey)};
        const auto ready=run_project_gui(project,28,(directory/"kept-result.ppm").u8string(),o);
        const auto kept=doc->latest_analysis(id);check(ready.replay_complete && ready.analysis_state=="ready" && kept && doc->analysis_record_count()==1,"Run/Keep failed through the simplified result panel");
        check(kept->result.white==std::array<double,3>{201,212,223} && kept->result.kind==AnalysisKind::OpticalDensityMeans,"simplified run changed native inputs");
        check(!observed.trace.results.count("preview") && observed.trace.results.count("kept") &&
            observed.trace.results.at("kept").record_id==kept->id && observed.trace.results.at("kept").means==kept->result.means,
            "the exact native Keep result must have one summary without duplicate preview values");
        const auto* navigation=action(observed.trace,"Go to tables###ROI tables");
        check(navigation && navigation->enabled,"kept result has no usable table navigation");
        const auto output=directory/"lifecycle.csv";o.measurement_table_path=output.u8string();
        int navigation_stage=0;ImVec2 navigation_mouse{};
        observed.trace.before_frame=[&](int frame) {
            if(frame<6 || navigation_stage==3)return;
            auto& io=ImGui::GetIO();
            if(navigation_stage==0) {
                const auto* target=action(observed.trace,"Go to tables###ROI tables");if(!target || !target->enabled)return;
                const auto hit=target->bounds;navigation_mouse={float(hit.x+hit.width/2),float(hit.y+hit.height/2)};
            }
            io.AddMousePosEvent(navigation_mouse.x,navigation_mouse.y);
            if(navigation_stage>0)io.AddMouseButtonEvent(0,navigation_stage==1);
            ++navigation_stage;
        };
        o.inputs={key(3,GuiInputKind::SelectAllKey)};
        const auto navigated=run_project_gui(project,16,(directory/"kept-tables.ppm").u8string(),o);
        observed.trace.before_frame={};check(navigation_stage==3,"kept-result navigation click did not complete");
        check(navigated.replay_complete && read(o.layout_path).find("; qupathxx workflow=tables")!=std::string::npos,"View kept results did not navigate to Tables");
        const auto* export_action=action(observed.trace,"Export image (Ctrl+Alt+Q)");
        check(export_action && export_action->enabled,"kept-result navigation lost the ROI/current-image table export route");
        o.inputs={key(3,GuiInputKind::SelectAllKey),key(6,GuiInputKind::ExportMeasurementTableKey)};
        const auto exported=run_project_gui(project,28,(directory/"kept-export.ppm").u8string(),o);
        check(exported.replay_complete && exported.measurement_table_state=="succeeded" &&
            read(output)==measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Roi)),"simplified table route changed exact native CSV output");
        check(doc->analysis_record_count()==1 && doc->latest_analysis(id)->id==kept->id && read(project->path())==bytes,"navigation/export kept an extra result or silently saved the project");
    }
    {
        const auto project=fixture(directory/"stale-kept.qpxproj",128,96);const auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60},"Changed ROI");
        const StainTransform transform({{1,0,0},{0,1,0}});const AnalysisRequest request(*doc,id,transform);
        const auto result=analyze(request);const auto retained=doc->keep_analysis(request,result);
        project->save(project->path());const auto bytes=read(project->path());
        doc->execute({ObjectEdit::replace_geometry(id,Roi::rectangle({9,8,44,60}),"Changed ROI")},"Move review ROI");
        check(doc->analysis_status(retained)=="object_changed","stale kept fixture must invalidate its captured geometry");
        TraceScope observed;auto o=options(directory,"stale-kept","workflow","region");o.inputs={key(3,GuiInputKind::SelectAllKey)};
        const auto report=run_project_gui(project,16,(directory/"stale-kept.ppm").u8string(),o);
        const auto* view=action(observed.trace,"Go to tables###ROI tables");
        const auto* recheck=action(observed.trace,"Recheck saved (Ctrl+Shift+K)");
        check(report.replay_complete && report.saved_analysis_status=="object_changed" && view && view->enabled && recheck && !recheck->enabled &&
            observed.trace.results.count("kept") && observed.trace.results.at("kept").record_id==retained &&
            observed.trace.results.at("kept").means==result.means && doc->analysis_record_count()==1 && read(project->path())==bytes,
            "stale kept values must remain inspectable without enabling invalid Recheck or changing their identity");
    }
    {
        const auto project=fixture(directory/"captured-domain.qpxproj",128,96);const auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60},"Domain ROI");
        const StainTransform transform({{1,0,0},{0,1,0}},{190,200,210});
        const AnalysisRequest request(*doc,id,transform,AnalysisKind::StainMeans);const auto expected=analyze(request);
        const auto retained=doc->keep_analysis(request,expected);project->save(project->path());const auto bytes=read(project->path());
        TraceScope observed;auto o=options(directory,"captured-domain","workflow","region");o.analysis_kind=0;o.analysis_white={201,212,223};
        o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::AnalyzeKey)};
        const auto report=run_project_gui(project,28,(directory/"captured-domain.ppm").u8string(),o);
        check(report.replay_complete && observed.trace.results.count("preview") && observed.trace.results.count("kept"),"different preview and kept results must both remain visible");
        const auto& preview=observed.trace.results.at("preview");const auto& kept=observed.trace.results.at("kept");
        check(preview.kind==AnalysisKind::OpticalDensityMeans && kept.kind==AnalysisKind::StainMeans && kept.record_id==retained &&
            kept.means==expected.means && !kept.nucleus,"kept result labels or values followed the active draft instead of captured native identity");
        check(doc->analysis_record_count()==1 && doc->latest_analysis(id)->id==retained && read(project->path())==bytes,"preview implicitly kept or persisted a result");
        auto enlarged=options(directory,"captured-domain-fr-2x","workflow","region",2);enlarged.width=1000;enlarged.height=1100;
        auto ini=read(enlarged.layout_path);ini.replace(ini.find("language=en"),11,"language=fr");write(enlarged.layout_path,ini);
        enlarged.analysis_kind=o.analysis_kind;enlarged.analysis_white=o.analysis_white;enlarged.inputs=o.inputs;
        Rect body_clip{};Point scroll_pointer{};double scroll_step=0,scroll_distance=0;bool placement=false;
        observed.trace.before_frame=[&](int frame) {
            if(frame<10)return;auto* context=ImGui::GetCurrentContext();auto* parent=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);
            if(!context || !parent)return;
            for(auto* window:context->Windows)if(window->ParentWindow==parent &&
                window->ChildId==ImHashStr("Workflow task body",0,parent->ID) && window->LastFrameActive==context->FrameCount) {
                const auto* recheck=action(observed.trace,"Recheck saved (Ctrl+Shift+K)");if(!recheck || recheck->bounds.width<=0 || recheck->bounds.height<=0)break;
                const auto clip=window->InnerClipRect;body_clip={clip.Min.x,clip.Min.y,clip.GetWidth(),clip.GetHeight()};
                const auto display=ImGui::GetIO().DisplaySize;
                scroll_pointer={(clip.Min.x+clip.Max.x)/2/display.x,(clip.Min.y+clip.Max.y)/2/display.y};
                // Match pinned ImGui's wheel authority, using actual native font
                // and inner height. Put Recheck near the visible body's bottom,
                // with room above for all captured-domain kept result rows.
                scroll_step=std::trunc(std::min(5*window->FontRefSize,window->InnerRect.GetHeight()*.67f));
                const double target=clip.Max.y-recheck->bounds.height-2*ImGui::GetFontSize();
                scroll_distance=std::min(double(window->ScrollMax.y),std::max(1.,recheck->bounds.y-target));
                placement=scroll_step>0 && body_clip.height>0;break;
            }
        };
        const auto placement_report=run_project_gui(project,12,"",enlarged);
        check(placement_report.replay_complete && placement && read(project->path())==bytes && doc->analysis_record_count()==1,
            "French200 read-only placement probe lost body/recheck placement or changed retained state");
        GuiInput hover;hover.frame=10;hover.kind=GuiInputKind::Move;hover.target=GuiInputTarget::Window;hover.x=scroll_pointer.x;hover.y=scroll_pointer.y;
        auto wheel=hover;wheel.frame=12;wheel.kind=GuiInputKind::Wheel;wheel.wheel=-scroll_distance/scroll_step;
        enlarged.inputs.push_back(hover);enlarged.inputs.push_back(wheel);
        bool kept_scrolled_into_view=false;int scroll_matches=0;
        observed.trace.before_frame=[&](int frame) {
            if(frame<10)return;auto* context=ImGui::GetCurrentContext();auto* parent=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);
            if(!context || !parent)return;
            for(auto* window:context->Windows)if(window->ParentWindow==parent &&
                window->ChildId==ImHashStr("Workflow task body",0,parent->ID) && window->LastFrameActive==context->FrameCount) {
                ++scroll_matches;const auto clip=window->InnerClipRect;const auto* recheck=action(observed.trace,"Recheck saved (Ctrl+Shift+K)");
                if(window->Scroll.y>0 && recheck && recheck->enabled && recheck->bounds.y>=clip.Min.y+8*ImGui::GetFontSize() &&
                    recheck->bounds.y+recheck->bounds.height<=clip.Max.y)
                    kept_scrolled_into_view=true;
                if(frame==22)std::fprintf(stderr,"French200 public wheel=%.17g step=%.17g requested-scroll=%.17g actual-scroll=%.17g/%.17g recheck-y=%.17g clip=(%.17g %.17g) kept-visible=%d\n",
                    wheel.wheel,scroll_step,scroll_distance,double(window->Scroll.y),double(window->ScrollMax.y),recheck?recheck->bounds.y:0,double(clip.Min.y),double(clip.Max.y),int(kept_scrolled_into_view));
                break;
            }
        };
        const auto large=run_project_gui(project,28,(directory/"captured-domain-fr-2x.ppm").u8string(),enlarged);
        observed.trace.before_frame={};
        check(large.replay_complete && !large.ui_overflow_items && kept_scrolled_into_view && observed.trace.results.count("preview") && observed.trace.results.count("kept") &&
            observed.trace.results.at("kept").record_id==retained && observed.trace.results.at("kept").means==expected.means && read(project->path())==bytes,
            "French200 preview/kept display changed identity, values or usable layout: matched="+std::to_string(scroll_matches)+" wheel="+std::to_string(wheel.wheel)+" kept-visible="+std::to_string(kept_scrolled_into_view));
    }
    for(const bool apply:{false,true}) {
        const auto name=std::string("profile-")+(apply?"explicit":"not-applied");
        const auto project=fixture(directory/(name+".qpxproj"),128,96);const auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60},"Profile ROI");
        doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{1,0,0},{0,1,0}},
            {190,200,210},"Stored profile",{"First stain","Second stain"}));
        const auto profile=doc->image_analysis_settings();project->save(project->path());const auto bytes=read(project->path());
        auto o=options(directory,name,"workflow","region");o.analysis_kind=0;o.analysis_white={201,212,223};
        o.inputs={key(3,GuiInputKind::SelectAllKey)};
        if(apply) {
            GuiInput down;down.frame=6;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::UseImageStains;down.wait_for=GuiInputWait::TargetReady;
            auto up=down;up.frame=7;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs.insert(o.inputs.end(),{down,up});
        }
        o.inputs.push_back(key(11,GuiInputKind::AnalyzeKey));o.inputs.push_back(key(16,GuiInputKind::KeepAnalysisKey));
        const auto report=run_project_gui(project,32,(directory/(name+".ppm")).u8string(),o);const auto kept=doc->latest_analysis(id);
        check(report.replay_complete && kept && kept->result.kind==(apply?AnalysisKind::StainMeans:AnalysisKind::OpticalDensityMeans),"saved profile changed measurement mode without its explicit Use action");
        check(kept->result.white==(apply?std::array<double,3>{190,200,210}:std::array<double,3>{201,212,223}) &&
            doc->image_analysis_settings() && doc->image_analysis_settings()->equals(*profile) && read(project->path())==bytes,"profile presentation changed the saved profile, scientific inputs or project bytes");
    }
    {
        const auto project=fixture(directory/"apply-scope.qpxproj",128,96);const auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60},"Object###literal");
        doc->set_object_edit_state(id,false,0x123456,.8123456789012345);project->save(project->path());
        const auto original=doc->annotation(id);const auto bytes=read(project->path());TraceScope observed;
        auto o=options(directory,"apply-scope","annotations");o.inputs={key(3,GuiInputKind::SelectAllKey)};
        const auto unchanged=run_project_gui(project,12,(directory/"apply-pristine.ppm").u8string(),o);
        const auto* object_apply=action(observed.trace,"Apply object edit");const auto* metadata_apply=action(observed.trace,"Apply###Apply object display state");
        check(unchanged.replay_complete && object_apply && metadata_apply && !object_apply->enabled && !metadata_apply->enabled,"pristine independent Apply drafts must both be disabled");
        GuiInput down;down.frame=6;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::ToggleObjectLock;down.wait_for=GuiInputWait::TargetReady;
        auto up=down;up.frame=7;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs.insert(o.inputs.end(),{down,up});
        const auto locked=run_project_gui(project,18,(directory/"apply-independent-lock.ppm").u8string(),o);const auto after=doc->annotation(id);
        check(locked.replay_complete && after.locked() && after.name==original.name && after.geometry.equals(original.geometry) &&
            after.color_rgb()==original.color_rgb() && after.class_probability()==original.class_probability(),"immediate lock action committed an unrelated object/color/probability change");
        check(doc->undo() && !doc->annotation(id).locked() && doc->annotation(id).class_probability()==original.class_probability() &&
            read(project->path())==bytes,"scoped immediate edit lost native Undo or persisted unexpectedly");
    }
    for(const bool metadata_first:{true,false}) {
        const auto name=std::string("dirty-drafts-")+(metadata_first?"metadata-first":"name-first");
        const auto project=fixture(directory/(name+".qpxproj"),128,96);const auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60},"Original###literal");
        doc->set_object_edit_state(id,false,0x123456,.25);project->save(project->path());
        const auto original=doc->annotation(id);const auto bytes=read(project->path());
        constexpr double desired=.8123456789012345;constexpr const char* desired_name="Edited###literal";
        TraceScope observed;int stage=0;std::optional<ImVec2> draft_mouse;std::string armed_target;
        const char* first=metadata_first?"Apply###Apply object display state":"Apply object edit";
        const char* second=metadata_first?"Apply object edit":"Apply###Apply object display state";
        observed.trace.before_frame=[&](int frame) {
            if(frame<6 || stage==21)return;
            auto& io=ImGui::GetIO();
            if(draft_mouse)io.AddMousePosEvent(draft_mouse->x,draft_mouse->y);
            auto press=[&](const char* key,bool field) {
                const auto* target=action(observed.trace,key);
                if(!target || !target->enabled)return false;
                const auto rect=target->bounds;
                draft_mouse=ImVec2{float(field?rect.x+rect.width-24:rect.x+rect.width/2),float(rect.y+std::min(rect.height/2,14.))};
                io.AddMousePosEvent(draft_mouse->x,draft_mouse->y);
                if(armed_target!=key) {armed_target=key;return false;}
                armed_target.clear();io.AddMouseButtonEvent(0,true);return true;
            };
            switch(stage) {
            case 0:if(press("Object name",true))++stage;break;
            case 1:io.AddMouseButtonEvent(0,false);++stage;break;
            case 2:io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);++stage;break;
            case 3:io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);++stage;break;
            case 4:io.AddInputCharactersUTF8(desired_name);++stage;break;
            case 5:io.AddKeyEvent(ImGuiKey_Tab,true);++stage;break;
            case 6:io.AddKeyEvent(ImGuiKey_Tab,false);++stage;break;
            case 7:if(press("Reported probability",true))++stage;break;
            case 8:io.AddMouseButtonEvent(0,false);++stage;break;
            case 9:io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);++stage;break;
            case 10:io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);++stage;break;
            case 11:io.AddInputCharactersUTF8("0.8123456789012345");++stage;break;
            case 12:io.AddKeyEvent(ImGuiKey_Tab,true);++stage;break;
            case 13:io.AddKeyEvent(ImGuiKey_Tab,false);++stage;break;
            case 14:if(press(first,false))++stage;break;
            case 15:io.AddMouseButtonEvent(0,false);++stage;break;
            case 16:++stage;break; // Let the editor reconcile the native revision.
            case 17: {
                const auto value=doc->annotation(id);const auto* pending=action(observed.trace,second);
                check(value.name==(metadata_first?original.name:desired_name) && value.class_probability()==
                    std::optional<double>(metadata_first?desired:.25),"first Apply committed the other dirty draft: "+name+" actual-name="+value.name+" probability="+std::to_string(value.class_probability().value_or(-1)));
                check(value.color_rgb()==original.color_rgb() && value.geometry.equals(original.geometry) && pending && pending->enabled,
                    "first Apply discarded the other dirty draft or changed unrelated native state: "+name);
                if(press(second,false))++stage;break;
            }
            case 18:io.AddMouseButtonEvent(0,false);++stage;break;
            case 19:++stage;break;
            case 20: {
                const auto value=doc->annotation(id);
                check(value.name==desired_name && value.class_probability()==std::optional<double>(desired) &&
                    value.color_rgb()==original.color_rgb() && value.geometry.equals(original.geometry),"second Apply lost the independently committed draft: "+name);
                ++stage;break;
            }
            }
            // Public key replay pins its own mouse each frame; retain the
            // actual private text/click gesture through the release frame.
            if(draft_mouse)io.AddMousePosEvent(draft_mouse->x,draft_mouse->y);
        };
        auto o=options(directory,name,"annotations");o.height=1100;o.inputs={key(3,GuiInputKind::SelectAllKey)};
        const auto report=run_project_gui(project,80,(directory/(name+".ppm")).u8string(),o);
        check(report.replay_complete && !report.ui_overflow_items && stage==21,"dirty draft input sequence did not complete: "+name+" / stage "+std::to_string(stage));
        check(doc->undo(),"second independent Apply has no native Undo: "+name);
        const auto first_only=doc->annotation(id);
        check(first_only.name==(metadata_first?original.name:desired_name) && first_only.class_probability()==
            std::optional<double>(metadata_first?desired:.25),"Undo did not reverse only the second Apply: "+name);
        check(doc->undo(),"first independent Apply has no native Undo: "+name);
        const auto restored=doc->annotation(id);
        check(restored.name==original.name && restored.class_probability()==original.class_probability() &&
            restored.color_rgb()==original.color_rgb() && restored.geometry.equals(original.geometry) && read(project->path())==bytes,
            "independent Apply Undo did not restore exact native fields or project bytes: "+name);
    }
    std::cout<<"Simplification UI: compact measurements, no-result/Run/Keep/View/export and captured-domain separation passed\n";
}
inline void complex_review(const std::filesystem::path& directory,const std::filesystem::path& source) {
    std::filesystem::create_directories(directory);
    const auto original=read(source);check(!original.empty(),"complex review project missing");
    const auto copy=directory/"Research review.qpxproj";write(copy,original);
    const auto project=Project::open(copy.u8string());const auto catalog=project->images();
    check(catalog.size()>=6,"complex review must retain the six-image project catalog");
    auto chosen=catalog.end();for(auto it=catalog.begin();it!=catalog.end();++it)
        if(it->name.find("NuCLS")!=std::string::npos && it->available) {chosen=it;break;}
    check(chosen!=catalog.end(),"complex review requires the existing NuCLS field");project->set_active_image(chosen->id);
    const auto doc=project->document(chosen->id);const auto revision=doc->revision();const auto objects=doc->object_count();
    const auto x=doc->pixel_width_um(),y=doc->pixel_height_um();
    for(const std::string pane:{"project","image","annotations","workflow"}) {
        auto o=options(directory,"complex-"+pane,pane.c_str(),"tools");o.width=1440;o.height=960;o.settle_tiles=true;
        const auto report=run_project_gui(project,14,(directory/("complex-"+pane+".ppm")).u8string(),o);
        check(report.replay_complete && report.canvas_visible && !report.ui_overflow_items && report.active_image==chosen->id,
            "complex review lost layout/control bounds or selected image: "+pane+" / "+report.ui_overflow_label);
        check(doc->revision()==revision && doc->object_count()==objects && doc->pixel_width_um()==x && doc->pixel_height_um()==y &&
            project->images().size()==catalog.size() && read(copy)==original && read(source)==original,
            "complex review changed objects/calibration/catalog or original/copied project bytes");
    }
    const auto metadata=doc->server()->metadata();check(metadata.width>70 && metadata.height>70,"complex NuCLS field is too small for the bounded review ROI");
    const RegionRequest sample{8,8,0,32,32};
    const auto pixels=doc->server()->read_region(sample).bytes();
    const auto previous_objects=doc->objects();const auto previous_classes=doc->classes();
    const auto previous_roi=doc->analysis_records();const auto previous_tissue=doc->tissue_records();const auto previous_cells=doc->cell_records();
    auto preserve_existing=[&](const Document& current) {
        check(current.pixel_width_um()==x && current.pixel_height_um()==y && current.classes()==previous_classes &&
            current.server()->read_region(sample).bytes()==pixels,"complex workflow changed calibration, classes or original raw samples");
        for(const auto& [id,before]:previous_objects) {
            const auto after=current.annotation(id);
            check(after.id==before.id && after.revision==before.revision && after.name==before.name && after.class_name==before.class_name &&
                after.parent_id==before.parent_id && after.kind==before.kind && after.geometry.equals(before.geometry) &&
                bool(after.nucleus)==bool(before.nucleus) && (!before.nucleus || after.nucleus->equals(*before.nucleus)) &&
                bool(after.properties)==bool(before.properties) && (!before.properties || after.properties->equals(*before.properties)),
                "complex workflow changed a preexisting object identity, geometry or imported property");
        }
        for(const auto& before:previous_roi) {
            const auto after=current.analysis_record(before.id);
            check(after.id==before.id && after.result.input_sha256==before.result.input_sha256 && after.result.kind==before.result.kind &&
                after.result.means==before.result.means && after.result.white==before.result.white && after.roi.equals(before.roi),
                "complex workflow changed a preexisting ROI result identity");
        }
        check(current.tissue_record_count()==previous_tissue.size() && current.cell_record_count()==previous_cells.size(),"complex workflow changed unrelated retained result counts");
        for(const auto& before:previous_tissue) {const auto after=current.tissue_record(before->id);check(after && after->id==before->id &&
            after->result->input_sha256()==before->result->input_sha256(),"complex workflow changed a retained tissue identity");}
        for(const auto& before:previous_cells) {const auto after=current.cell_record(before->id);check(after && after->id==before->id &&
            after->result->nuclei()->input_sha256()==before->result->nuclei()->input_sha256() &&
            after->result->cells()->input_sha256()==before->result->cells()->input_sha256(),"complex workflow changed a retained cell identity");}
    };
    project->set_view(chosen->id,{{metadata.width/2.,metadata.height/2.},1,{}});
    const auto csv=directory/"Research ROI.csv";TraceScope observed;int stage=0;std::uint64_t region=0,record=0;
    auto o=options(directory,"complex-workflow","workflow","region");o.width=1440;o.height=960;o.settle_tiles=true;o.measurement_table_path=csv.u8string();
    const auto placement_report=run_project_gui(project,12,"",o);
    check(placement_report.replay_complete && placement_report.canvas_visible && placement_report.final_zoom>0 &&
        placement_report.canvas_content_bounds.width>0 && placement_report.canvas_content_bounds.height>0 &&
        doc->object_count()==objects && read(copy)==original,"complex read-only viewport probe changed state or lost the native canvas");
    const Viewport replay_view{placement_report.final_origin,placement_report.final_zoom};o.start_view=replay_view;
    const auto replay_canvas=placement_report.canvas_content_bounds;
    auto mapped_position=[&](Point image) {
        const auto relative=replay_view.image_to_screen(image);
        // Same float conversion/addition as the public Image replay target.
        return Point{float(replay_canvas.x)+float(relative.x),float(replay_canvas.y)+float(relative.y)};
    };
    const auto precise_down=mapped_position({8,8}),precise_up=mapped_position({60,70});
    auto expected_image=[&](Point mapped) {
        return replay_view.screen_to_image({std::floor(mapped.x)-replay_canvas.x,std::floor(mapped.y)-replay_canvas.y});
    };
    const auto expected_down=expected_image(precise_down),expected_up=expected_image(precise_up);
    std::optional<Point> gesture;std::string armed_action,pending_press;
    std::optional<Point> native_down,native_up;
    observed.trace.before_frame=[&](int frame) {
        auto& io=ImGui::GetIO();auto* context=ImGui::GetCurrentContext();
        // Keep this actual native-input session free of public replay pointer
        // pinning. Every injected key/button is explicitly released.
        if(frame==2)io.AddKeyEvent(ImGuiKey_R,true);
        if(frame==3)io.AddKeyEvent(ImGuiKey_R,false);
        if(frame==3 || frame==4 || frame==5)io.AddMousePosEvent(float(precise_down.x),float(precise_down.y));
        if(frame==4)io.AddMouseButtonEvent(0,true);
        if(frame==6 || frame==7 || frame==8)io.AddMousePosEvent(float(precise_up.x),float(precise_up.y));
        if(frame==6)io.AddMouseButtonEvent(0,false);
        if(frame==5)native_down=Point{io.MousePos.x,io.MousePos.y};
        if(frame==7)native_up=Point{io.MousePos.x,io.MousePos.y};
        if(frame<9 || stage==12)return;
        if(gesture)io.AddMousePosEvent(float(gesture->x),float(gesture->y));
        if(frame<=18) {
            std::fprintf(stderr,"complex gesture frame=%d native=%d stage=%d mouse=(%.9g %.9g) down=%d clicked=%d released=%d active=%08X active-window=%s hovered=%08X hover-window=%s nav=%s nav-id=%08X moving=%s focus-lost=%d capture=%d queued=%d armed=%s pending=%s\n",
                frame,context->FrameCount,stage,io.MousePos.x,io.MousePos.y,int(io.MouseDown[0]),int(io.MouseClicked[0]),int(io.MouseReleased[0]),
                context->ActiveId,context->ActiveIdWindow?context->ActiveIdWindow->Name:"<none>",context->HoveredId,
                context->HoveredWindow?context->HoveredWindow->Name:"<none>",context->NavWindow?context->NavWindow->Name:"<none>",context->NavId,
                context->MovingWindow?context->MovingWindow->Name:"<none>",int(io.AppFocusLost),int(io.WantCaptureMouse),context->InputEventsQueue.Size,armed_action.c_str(),pending_press.c_str());
            for(const auto& event:context->InputEventsQueue) {
                if(event.Type==ImGuiInputEventType_MousePos)std::fprintf(stderr,"  queue pos id=%u (%.9g %.9g)\n",event.EventId,event.MousePos.PosX,event.MousePos.PosY);
                else if(event.Type==ImGuiInputEventType_MouseButton)std::fprintf(stderr,"  queue button id=%u button=%d down=%d\n",event.EventId,event.MouseButton.Button,int(event.MouseButton.Down));
                else std::fprintf(stderr,"  queue type=%d id=%u\n",int(event.Type),event.EventId);
            }
        }
        auto press=[&](const char* key) {
            const auto* target=action(observed.trace,key);
            if(frame<=18)std::fprintf(stderr,"  target=%s present=%d enabled=%d bounds=(%.17g %.17g %.17g %.17g)\n",key,int(target!=nullptr),target?int(target->enabled):0,
                target?target->bounds.x:0,target?target->bounds.y:0,target?target->bounds.width:0,target?target->bounds.height:0);
            if(!target || !target->enabled){armed_action.clear();return false;}
            const auto rect=target->bounds;const Point next{rect.x+rect.width/2,rect.y+rect.height/2};
            const bool hovered=armed_action==key && gesture && gesture->x==next.x && gesture->y==next.y;gesture=next;
            io.AddMousePosEvent(float(gesture->x),float(gesture->y));
            if(pending_press==key) {
                // Do not release until ImGui consumed this actual down event and
                // activated the hovered native item; no blind click retries.
                if(io.MouseDown[0] && context->ActiveId!=0 && context->ActiveId==context->HoveredId &&
                    io.MousePos.x>=rect.x && io.MousePos.x<rect.x+rect.width && io.MousePos.y>=rect.y && io.MousePos.y<rect.y+rect.height) {
                    pending_press.clear();armed_action.clear();return true;
                }
                return false;
            }
            if(!hovered){armed_action=key;return false;}
            pending_press=key;io.AddMouseButtonEvent(0,true);return false;
        };
        switch(stage) {
        case 0:
            if(doc->object_count()==objects+1) {
                for(const auto& [id,value]:doc->objects())if(!previous_objects.count(id))region=id;
                check(region!=0,"native rectangle gesture did not create a new object");const auto bounds=doc->annotation(region).geometry.bounds();
                check(native_down && native_up,"complex rectangle lost native gesture evidence");
                const auto quantized=[](Point mapped,Point native) {
                    return native.x==std::floor(mapped.x) && native.y==std::floor(mapped.y);
                };
                check(quantized(precise_down,*native_down) && quantized(precise_up,*native_up),
                    "complex rectangle native input differs from the measured viewport's pinned ImGui screen quantization");
                const Rect expected{expected_down.x,expected_down.y,expected_up.x-expected_down.x,expected_up.y-expected_down.y};
                char diagnostic[1600];std::snprintf(diagnostic,sizeof(diagnostic),
                    "complex GUI rectangle actual=(%.17g %.17g %.17g %.17g) expected=(%.17g %.17g %.17g %.17g) mapped=(%.17g %.17g)->(%.17g %.17g) native=(%.17g %.17g)->(%.17g %.17g) scale=(%.17g %.17g) frame=%d native-frame=%d image=%dx%d mouse=(%.17g %.17g) origin=(%.17g %.17g) canvas=(%.17g %.17g %.17g %.17g)",
                    bounds.x,bounds.y,bounds.width,bounds.height,expected.x,expected.y,expected.width,expected.height,
                    precise_down.x,precise_down.y,precise_up.x,precise_up.y,native_down->x,native_down->y,native_up->x,native_up->y,replay_view.zoom,replay_view.zoom,
                    frame,context->FrameCount,int(metadata.width),int(metadata.height),double(io.MousePos.x),double(io.MousePos.y),replay_view.origin.x,replay_view.origin.y,
                    replay_canvas.x,replay_canvas.y,replay_canvas.width,replay_canvas.height);
                std::fprintf(stderr,"%s\n",diagnostic);
                check(std::abs(bounds.x-expected.x)<1e-4 && std::abs(bounds.y-expected.y)<1e-4 &&
                    std::abs(bounds.width-expected.width)<1e-4 && std::abs(bounds.height-expected.height)<1e-4,diagnostic);++stage;
            }break;
        case 1:if(press("Analyze region (Ctrl+M)"))++stage;break;
        case 2:io.AddMouseButtonEvent(0,false);++stage;break;
        case 3:if(press("Keep result (Ctrl+K)"))++stage;break;
        case 4:io.AddMouseButtonEvent(0,false);++stage;break;
        case 5:
            if(const auto kept=doc->latest_analysis(region);kept && doc->analysis_record_count()==previous_roi.size()+1) {
                record=kept->id;if(press("Go to tables###ROI tables"))++stage;
            }break;
        case 6:io.AddMouseButtonEvent(0,false);++stage;break;
        case 7:if(press("Export image (Ctrl+Alt+Q)"))++stage;break;
        case 8:io.AddMouseButtonEvent(0,false);++stage;break;
        case 9:
            if(const auto* ready=action(observed.trace,"Export image (Ctrl+Alt+Q)");ready && ready->enabled && std::filesystem::exists(csv)) {
                check(read(csv)==measurements_csv(MeasurementTableRequest(*doc,MeasurementTableKind::Roi)),"complex GUI table export differs from the exact native table");
                io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_S,true);++stage;
            }break;
        case 10:io.AddKeyEvent(ImGuiKey_S,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);++stage;break;
        case 11:if(!project->dirty() && read(copy)!=original)++stage;break;
        }
        if(gesture)io.AddMousePosEvent(float(gesture->x),float(gesture->y));
    };
    GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Image;down.x=8;down.y=8;
    auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;up.x=60;up.y=70;
    o.inputs.clear();
    const auto completed=run_project_gui(project,100,(directory/"complex-workflow.ppm").u8string(),o);
    check(completed.replay_complete && !completed.ui_overflow_items && completed.tiles_settled && !completed.pending_tiles && stage==12 &&
        completed.saves_completed==1 && completed.measurement_table_state=="succeeded" && completed.project_error.empty(),
        "complex Draw/Run/Keep/View/Export/Save workflow did not finish: stage="+std::to_string(stage)+" / "+completed.project_error+
        " / analysis="+completed.analysis_state+" / error="+completed.analysis_error+" / samples="+std::to_string(completed.analysis_samples)+
        " / selection="+std::to_string(completed.primary_selection));
    preserve_existing(*doc);check(read(source)==original,"complex workflow overwrote its original project");
    const auto saved=read(copy);check(saved!=original,"explicit GUI Save did not persist the copied project");
    const auto reopened=Project::open(copy.u8string());const auto restored=reopened->document(chosen->id);preserve_existing(*restored);
    check(reopened->images().size()==catalog.size() && restored->object_count()==objects+1 && restored->analysis_record_count()==previous_roi.size()+1 &&
        restored->annotation(region).geometry.equals(doc->annotation(region).geometry),"complex GUI Save/reopen lost new or preexisting project data");
    const auto kept=restored->analysis_record(record);const auto before=doc->analysis_record(record);
    check(kept.id==before.id && kept.result.input_sha256==before.result.input_sha256 && kept.result.means==before.result.means &&
        kept.result.kind==before.result.kind && kept.result.white==before.result.white,"saved result identity changed across native reopen");
    std::optional<Point> pick;for(int py=10;py<69 && !pick;py+=2)for(int px=10;px<59 && !pick;px+=2) {
        const auto hits=restored->pick({double(px),double(py)},4);if(!hits.empty() && hits.back()==region)pick=Point{double(px),double(py)};
    }
    check(pick.has_value(),"complex review cannot pick its new ROI without selecting an existing object");
    auto verify=options(directory,"complex-recheck","workflow","region");verify.width=1440;verify.height=960;verify.settle_tiles=true;
    down.frame=4;down.x=pick->x;down.y=pick->y;up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;
    verify.inputs={down,up,key(9,GuiInputKind::RecheckAnalysisKey)};observed.trace.before_frame={};
    const auto rechecked=run_project_gui(reopened,45,(directory/"complex-recheck.ppm").u8string(),verify);
    check(rechecked.replay_complete && !rechecked.ui_overflow_items && rechecked.primary_selection==region && rechecked.analysis_state=="ready" &&
        rechecked.saved_analysis_status=="current" && restored->analysis_record_count()==previous_roi.size()+1 &&
        restored->analysis_record(record).result.input_sha256==kept.result.input_sha256 && restored->analysis_record(record).result.means==kept.result.means,
        "explicit GUI Recheck changed identity, selected the wrong object or failed native verification");
    preserve_existing(*restored);check(read(copy)==saved && read(source)==original,"Recheck unexpectedly saved or changed the original project");
    std::cout<<"Complex project: six-image catalog, native NuCLS Draw/Run/Keep/View/exact CSV/GUI Save/reopen/Recheck and preexisting identity/calibration/raw-data preservation passed\n";
}

}
