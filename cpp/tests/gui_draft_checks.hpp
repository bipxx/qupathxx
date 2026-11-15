#pragma once
#include "../gui_draft.hpp"
#include "gui_analysis_inputs_checks.hpp"

namespace draft_checks {
using namespace qpx;
using Geometry=detail::Draft<detail::GeometryDraft>;
using Appearance=detail::Draft<detail::AppearanceDraft>;
template<class Host,class Draw>inline void type_text(Host& host,const char* key,const char* value,Draw& draw) {
    const auto bounds=host.action(key).bounds;
    host.click(Rect{bounds.x+bounds.width-25,bounds.y+std::min(bounds.height/2,14.),2,2},draw);
    auto& io=ImGui::GetIO();auto& g=*ImGui::GetCurrentContext();
    check(g.ActiveId && g.ActiveId==g.InputTextState.ID,"Draft text gesture did not focus the value");const auto id=g.ActiveId;
    io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);host.frame(draw);
    io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);host.frame(draw);
    io.AddInputCharactersUTF8(value);host.until(draw,[&]{return g.InputTextState.TextA.Data && std::string(g.InputTextState.TextA.Data)==value;},"Draft text was not consumed");
    io.AddKeyEvent(ImGuiKey_Tab,true);host.frame(draw);io.AddKeyEvent(ImGuiKey_Tab,false);host.until(draw,[&]{return g.ActiveId!=id;},"Draft field did not lose focus");
}
struct Scene {
    std::filesystem::path out;std::string name;
    std::shared_ptr<Project> project;std::shared_ptr<Document> doc;GuiOptions options;
    analysis_tasks_checks::Snapshot snapshot;ui::CommandTrace commands;detail::DraftEditorTrace editor;detail::AnalysisInputsTrace inputs;
    ui::CommandTrace* old_commands=ui::command_trace;detail::DraftEditorTrace* old_editor=detail::draft_editor_trace;detail::AnalysisInputsTrace* old_inputs=detail::analysis_inputs_trace;
    std::function<void()> before;std::optional<GuiInputKind> pending;std::string activate,capture;
    std::optional<ImVec2> mouse;std::function<std::string()> describe;bool capture_bottom=false;
    std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    Scene(const std::filesystem::path& dir,std::string stem,bool french=false):out(dir),name(std::move(stem)) {
        project=fixture(out/(name+".qpxproj"),128,96);doc=project->document(1);
        options=::options(out,name,"annotations");options.width=french?2400:1600;options.height=french?2200:1400;options.settle_tiles=false;
        if(french){options.ui_scale=2;auto ini=read(std::filesystem::u8path(options.layout_path));ini.replace(ini.find("language=en"),11,"language=fr");write(std::filesystem::u8path(options.layout_path),ini);}
        ui::command_trace=&commands;detail::draft_editor_trace=&editor;detail::analysis_inputs_trace=&inputs;
        snapshot.before=[&](int){check(std::chrono::steady_clock::now()<deadline,"Draft scene deadline: "+name+(describe?describe():""));ImGui::GetIO().ConfigInputTrickleEventQueue=false;if(before)before();if(mouse)ImGui::GetIO().AddMousePosEvent(mouse->x,mouse->y);};
        commands.replay_input=[&](const auto& emit){if(pending){emit(key(0,*pending));pending.reset();}};
        commands.before_context=[&](int){if(!activate.empty()) {auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane);if(pane) {dispatch_checks::activate(pane,ui::command_label(ui::workflow_command(activate.c_str()).id));activate.clear();}}};
        inputs.after_render=[&]{if(!capture.empty()){
            analysis_inputs_checks::capture_view(out/(name+"-"+capture+(capture_bottom?"-bottom":"")+".ppm"));
            if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane))ImGui::SetScrollY(pane,capture_bottom?0:pane->ScrollMax.y);
            if(capture_bottom)capture.clear();capture_bottom=!capture_bottom;
        }};
    }
    ~Scene(){ui::command_trace=old_commands;detail::draft_editor_trace=old_editor;detail::analysis_inputs_trace=old_inputs;}
    GuiReport run(){auto r=run_project_gui(project,20000,"",options);check(commands.end_session && r.ui_overflow_items==0,"Draft scene incomplete/overflow: "+name+" / "+r.ui_overflow_label+(describe?describe():""));return r;}
};
// Actual private pointer/text events, as in the retained dirty-drafts fixture.
// Each transition observes ImGui's consumed state; no frame-count waits.
struct Typing {
    int stage=0;std::string value,key="Object name";ImVec2 point{};
    bool step(const ui::WorkflowTrace& trace) {
        auto& g=*ImGui::GetCurrentContext();auto& io=ImGui::GetIO();
        auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane);
        const auto expected=pane?ImHashStr("##value",0,ImHashStr(key=="Object name"?"Name":key.c_str(),0,pane->ID)):0;
        if(stage==0) {const auto it=trace.actions.find(key);if(it==trace.actions.end() || !it->second.enabled)return false;const auto b=it->second.bounds;point={float(b.x+b.width-24),float(b.y+std::min(b.height/2,14.))};io.AddMousePosEvent(point.x,point.y);stage=1;return false;}
        io.AddMousePosEvent(point.x,point.y);
        switch(stage) {
        case 1:if(!g.HoveredWindow)return false;io.AddMouseButtonEvent(0,true);stage=2;break;
        case 2:if(!g.InputTextState.ID || g.ActiveId!=g.InputTextState.ID || g.ActiveId!=expected)return false;io.AddMouseButtonEvent(0,false);stage=3;break;
        case 3:if(io.MouseDown[0])return false;io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);stage=4;break;
        case 4:if(!g.InputTextState.HasSelection())return false;io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);stage=5;break;
        case 5:if(io.KeyCtrl)return false;io.AddInputCharactersUTF8(value.c_str());stage=6;break;
        case 6:if(g.InputTextState.TextA.Data && std::string(g.InputTextState.TextA.Data)==value)return true;break;
        }return false;
    }
};
inline void names(const std::filesystem::path& dir) {
    for(const std::string mode:{"enter","escape","same","selection"}) {
        Scene scene(dir,"name-"+mode);const auto first=scene.doc->add_rectangle({8,8,30,40},"First"),second=scene.doc->add_rectangle({70,8,30,40},"Second");scene.doc->clear_history();
        const bool drawing=mode=="enter" || mode=="escape";
        if(drawing) {scene.options.inputs.push_back(key(3,GuiInputKind::PolygonKey));int frame=4;
            for(const Point p:{Point{12,12},Point{60,12},Point{60,60}}) {auto input=key(frame++,GuiInputKind::LeftDown);input.target=GuiInputTarget::Image;input.x=p.x;input.y=p.y;scene.options.inputs.push_back(input);input.frame=frame++;input.kind=GuiInputKind::LeftUp;scene.options.inputs.push_back(input);}}
        int stage=0;bool checked=false;Typing typing;typing.value=mode=="same"?"First":"Renamed";ObjectSelection* selection=nullptr;std::uint64_t observed_primary=0;
        scene.describe=[&]{return " stage="+std::to_string(stage)+" typing="+std::to_string(typing.stage)+" first="+scene.doc->annotation(first).name+" second="+scene.doc->annotation(second).name+" primary="+std::to_string(observed_primary);};
        scene.editor.after_editor=[&](Geometry&,Appearance&,ObjectSelection& s,const char* text,bool,bool){selection=&s;observed_primary=s.primary();
            if(stage==0){s.replace({first},first);stage=1;}
            if(stage==3) {
                if(drawing)check(scene.commands.draft_points==3 && s.primary()==first && scene.doc->object_count()==2,"Text Enter/Escape escaped to drawing/selection");
                if(mode=="escape" || mode=="same") {if(ImGui::GetCurrentContext()->ActiveId)return;check(scene.doc->annotation(first).name=="First" && !scene.doc->can_undo(),"Escape/no-op created an edit");}
                else {if(scene.doc->annotation(first).name!="Renamed")return;check(scene.doc->annotation(second).name=="Second" && scene.doc->undo_label()=="Rename annotation","Rename target/label wrong");check(scene.doc->undo() && !scene.doc->can_undo(),"Rename was not exactly one undo step");stage=4;return;}
                checked=true;scene.commands.end_session=true;
            }
            if(stage==4 && std::string(text)==(mode=="selection"?"Second":"First")){checked=true;scene.commands.end_session=true;}
        };
        scene.before=[&]{
            if(stage==1 && (!drawing || scene.commands.draft_points==3) && typing.step(scene.snapshot.scope.trace)){stage=2;return;}
            if(stage==2){
                if(mode=="selection") {auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane);if(!pane)return;ImGuiWindow* list=nullptr;for(auto* child:pane->DC.ChildWindows)if(std::string(child->Name).find("Object list")!=std::string::npos)list=child;check(list,"Name selection list missing");
                    scene.mouse=ImVec2{list->DC.CursorStartPos.x+40,list->DC.CursorStartPos.y+1.5f*ImGui::GetTextLineHeightWithSpacing()};stage=7;
                }else {scene.pending=mode=="escape"?GuiInputKind::EscapeKey:GuiInputKind::FinishKey;stage=3;}
                return;
            }
            if(stage==7){ImGui::GetIO().AddMouseButtonEvent(0,true);stage=8;return;}
            if(stage==8 && ImGui::GetIO().MouseDown[0]){ImGui::GetIO().AddMouseButtonEvent(0,false);stage=9;return;}
            if(stage==9 && selection->primary()==second)stage=3;
        };
        const auto report=scene.run();check(checked,"Name scene did not check: "+mode);
        if(drawing)check(scene.commands.draft_points==3 && report.selected_ids==std::vector<std::uint64_t>{first} && scene.doc->object_count()==2,"Text key changed the final canvas draft or selection");
        std::cout<<"Name "<<mode<<" passed\n";
    }
}
inline void geometry(const std::filesystem::path& dir,bool rectangle) {
    Scene scene(dir,rectangle?"rectangle-current-shape":"polygon-current-shape");
    const auto original=rectangle?Roi::rectangle({8,8,30,40}):Roi::polygon({{8,8},{38,8},{35,48},{8,40}});
    const auto reshaped=rectangle?Roi::rectangle({12,10,36,32}):Roi::polygon({{12,10},{48,12},{44,42},{20,40},{12,28}});
    const auto id=scene.doc->add_roi(original,"Shape"),other=scene.doc->add_rectangle({70,8,20,30},"Other");scene.doc->set_class("Tumor",0xff0000);scene.doc->clear_history();
    int stage=0;std::uint64_t revision=0;Typing typing;typing.key="X";typing.value="25";double observed_x=0,observed_baseline=0;
    scene.describe=[&]{const auto bounds=scene.doc->annotation(id).geometry.bounds();return " stage="+std::to_string(stage)+" typing="+std::to_string(typing.stage)+" x="+std::to_string(observed_x)+" baseline="+std::to_string(observed_baseline)+" native="+std::to_string(bounds.x)+","+std::to_string(bounds.y)+","+std::to_string(bounds.width)+","+std::to_string(bounds.height);};
    scene.commands.before_context=[&](int){if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane))pane->StateStorage.SetInt(pane->GetID(ui::i18n::label("Geometry").c_str()),1);
        if(!scene.activate.empty()){dispatch_checks::activate(ImGui::FindWindowByName(GuiWorkspace::objects_pane),ui::command_label(ui::workflow_command(scene.activate.c_str()).id));scene.activate.clear();}};
    scene.before=[&]{if(stage==1 && typing.step(scene.snapshot.scope.trace)){ImGui::GetIO().AddKeyEvent(ImGuiKey_Tab,true);stage=2;}else if(stage>=2)ImGui::GetIO().AddKeyEvent(ImGuiKey_Tab,false);};
    scene.editor.after_editor=[&](Geometry& g,Appearance& a,ObjectSelection& s,const char*,bool gd,bool ad){
        observed_x=g.value.bounds.x;observed_baseline=g.baseline.bounds.x;
        if(stage==0){s.replace({id},id);stage=1;return;}
        if(stage==2){if(g.value.bounds.x!=25 || !g.dirty())return;a.value.probability=true;a.value.reported=.7;scene.doc->rename_annotation(other,"Unrelated");scene.doc->classify_annotation(id,"Tumor");revision=scene.doc->revision();stage=3;return;}
        if(stage==3){check(g.value.bounds.x==25 && g.dirty() && scene.doc->annotation(id).class_name=="Tumor","Unrelated/class edit discarded geometry");check(scene.doc->undo() && scene.doc->annotation(id).class_name.empty(),"Class is not its own undo step");scene.doc->update_roi(id,reshaped);stage=4;return;}
        if(stage==4){check(g.value.bounds.x==25 && g.value.bounds.y==reshaped.bounds().y && g.value.bounds.width==reshaped.bounds().width,"Reshape discarded typed X or retained stale untouched bounds");scene.activate="Apply object edit";stage=5;return;}
        if(stage==5){const auto expected=reshaped.translated(25-reshaped.bounds().x,0);if(!scene.doc->annotation(id).geometry.equals(expected))return;
            check(!g.dirty() && a.dirty() && scene.snapshot.scope.trace.actions.at("Apply###Apply object display state").enabled && !scene.doc->annotation(id).class_probability(),"Geometry Apply committed/discarded independent appearance");
            check(scene.doc->undo() && scene.doc->annotation(id).geometry.equals(reshaped),"Apply not one undo step");g.value.bounds.x=66;scene.activate="Apply###Apply object display state";stage=8;return;}
        if(stage==8 && scene.doc->annotation(id).class_probability()==std::optional<double>(.7)) {
            check(g.dirty() && g.value.bounds.x==66 && scene.doc->annotation(id).geometry.equals(reshaped),"Appearance Apply committed/discarded independent geometry");
            check(scene.doc->undo() && !scene.doc->annotation(id).class_probability() && scene.doc->annotation(id).geometry.equals(reshaped),"Appearance Apply not one Undo");
            a.value.probability=true;a.value.reported=.8;s.replace({other},other);stage=6;return;
        }
        if(stage==6){check(gd && ad,"Dirty selection change has no discard notices");scene.doc->rename_annotation(other,"Another edit");stage=7;return;}
        if(stage==7){check(gd && ad,"Discard notice vanished after document edit");scene.commands.end_session=true;}
    };
    scene.run();std::cout<<(rectangle?"Rectangle":"Polygon")<<" current-shape Apply/rebase/discard passed\n";
}
inline void class_choice(const std::filesystem::path& dir) {
    Scene scene(dir,"class-choice");const auto id=scene.doc->add_rectangle({8,8,30,40},"First");scene.doc->set_class("Tumor",0xff0000);scene.doc->clear_history();int stage=0;
    scene.editor.after_editor=[&](Geometry& g,Appearance&,ObjectSelection& s,const char*,bool,bool){
        if(stage==0){s.replace({id},id);stage=1;return;}
        if(stage==1){g.value.bounds.x=25;stage=2;return;}
        if(stage==4 && scene.doc->annotation(id).class_name=="Tumor") {check(g.dirty() && g.value.bounds.x==25,"Class choice discarded geometry");check(scene.doc->undo_label()=="Classify annotation" && scene.doc->undo() && !scene.doc->can_undo(),"Class choice not exactly one Undo");stage=5;return;}
        if(stage==5 && g.dirty() && scene.doc->annotation(id).class_name.empty())scene.commands.end_session=true;
    };
    scene.commands.before_context=[&](int){
        if(stage==2){if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane)){const auto id=ImHashStr("##value",0,ImHashStr("Class",0,pane->ID));auto& ctx=*ImGui::GetCurrentContext();ImGui::FocusWindow(pane);ctx.NavActivateId=ctx.NavActivateDownId=ctx.NavActivatePressedId=id;stage=3;}}
        else if(stage==3){for(const auto& popup:ImGui::GetCurrentContext()->OpenPopupStack)if(popup.Window && popup.Window->WasActive){const auto& definition=scene.doc->classes().at("Tumor");dispatch_checks::activate(popup.Window,ui::data_label("Tumor",std::to_string(reinterpret_cast<std::uintptr_t>(&definition))).c_str());stage=4;break;}}
    };scene.run();std::cout<<"Actual Class choice creates one Undo and preserves dirty geometry\n";
}
inline void parent_measurement(const std::filesystem::path& dir) {
    Document doc(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));doc.set_history_limits(0,0);std::uint64_t id=0;
    for(int i=0;i<10000;++i)id=doc.add_rectangle({0,0,4,4},"Area "+std::to_string(i));
    detail::ParentChoices choices;const auto start=std::chrono::steady_clock::now();choices.rebuild(doc,id);const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    check(choices.rows.size()==9999 && std::none_of(choices.rows.begin(),choices.rows.end(),[&](const auto& row){return row.first==id;}),"Parent list count/self exclusion");
    write(dir/"parent-measurement.txt","10000 area annotations; rebuild ms="+std::to_string(ms)+"; rows=9999\n");
    check(ms<1000,"Parent cache rebuild exceeded one second");
    {
        simplification_tools_checks::NativeUi host;std::size_t visited=0;std::uint64_t choice=0;
        auto draw=[&]{ImGui::SetNextWindowSize({460,500});ImGui::Begin("Parent performance");ImGui::BeginChild("Rows",{420,300});visited=choices.draw(choice);ImGui::EndChild();ImGui::End();};
        host.settle(draw);const auto began=std::chrono::steady_clock::now();std::size_t max_rows=0;
        for(int i=0;i<100;++i){host.frame(draw);max_rows=std::max(max_rows,visited);}
        const auto per_frame=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()/100;
        check(max_rows<100 && per_frame<50,"Parent list did not remain clipped/responsive");
        write(dir/"parent-render-measurement.txt","100 idle renders; ms/frame="+std::to_string(per_frame)+"; max rows="+std::to_string(max_rows)+"\n");
    }
    Scene scene(dir,"parent-apply");const auto parent=scene.doc->add_rectangle({0,0,100,90},"Parent###literal"),child=scene.doc->add_rectangle({10,10,20,20},"Child");int stage=0;
    scene.describe=[&]{return " stage="+std::to_string(stage);};
    scene.commands.before_context=[&](int){
        auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane);if(!pane)return;
        pane->StateStorage.SetInt(pane->GetID(ui::i18n::label("Geometry").c_str()),1);
        if(!scene.activate.empty()){dispatch_checks::activate(pane,ui::command_label(ui::workflow_command(scene.activate.c_str()).id));scene.activate.clear();}
        if(stage==1 || stage==4){auto& ctx=*ImGui::GetCurrentContext();ImGui::FocusWindow(pane);const auto id=ImHashStr("##value",0,ImHashStr("Parent ID",0,pane->ID));ctx.NavActivateId=ctx.NavActivateDownId=ctx.NavActivatePressedId=id;stage=stage==1?10:12;}
        else if(stage==10 || stage==12)for(const auto& popup:ImGui::GetCurrentContext()->OpenPopupStack)if(popup.Window && popup.Window->WasActive){
            if(stage==10){const int index=0;const auto id=ImHashStr("##parent",0,ImHashData(&index,sizeof(index),popup.Window->ID));auto& ctx=*ImGui::GetCurrentContext();ImGui::FocusWindow(popup.Window);ctx.NavActivateId=ctx.NavActivateDownId=ctx.NavActivatePressedId=id;stage=11;}
            else {dispatch_checks::activate(popup.Window,"No parent");stage=13;}break;
        }
    };
    scene.editor.after_editor=[&](Geometry& g,Appearance&,ObjectSelection& s,const char*,bool,bool){
        if(stage==0){s.replace({child},child);stage=1;return;}
        if(stage==11 && g.value.parent==parent){scene.activate="Apply object edit";stage=2;return;}
        if(stage==2){if(scene.doc->annotation(child).parent_id!=parent)return;
            check(std::string(ImGui::GetCurrentContext()->LogBuffer.c_str()).find("Parent###literal (#"+std::to_string(parent)+")")!=std::string::npos,"Parent preview hid literal name or native ID");
            check(scene.doc->undo() && !scene.doc->annotation(child).parent_id,"Parent Undo failed");stage=3;return;}
        if(stage==3){check(!g.value.parent,"No parent did not rebase after Undo");scene.doc->reparent_annotation(child,parent);stage=4;return;}
        if(stage==13 && !g.value.parent){scene.activate="Apply object edit";stage=5;return;}
        if(stage==5 && !scene.doc->annotation(child).parent_id)scene.commands.end_session=true;
    };scene.run();std::cout<<"Parent Apply/Undo/No parent and 10000-row rebuild "<<ms<<" ms passed\n";
}
inline void tma(const std::filesystem::path& dir) {
    auto project=fixture(dir/"tma-source.qpxproj",128,96);auto doc=project->document(1);TmaGridOptions grid;grid.rows=1;grid.columns=3;grid.origin={20,30};grid.row_step={0,30};grid.column_step={35,0};grid.width=grid.height=18;const auto ids=doc->create_tma_grid(grid);doc->set_tma_case_ids({{ids[0],"Case"},{ids[1],"Case"}});
    ObjectSelection selection(*doc);selection.replace(ids,ids[0]);GuiOptions initial;initial.tma_open=true;detail::GuiTma panel(initial);simplification_tools_checks::NativeUi host;
    const auto draw=[&]{panel.draw(*doc,selection,project.get(),false,true,[](GuiInputTarget){});};host.fit("Tissue microarray",600,1200,draw);
    auto& cases=detail::GuiTmaTestAccess::case_draft(panel);check(std::string(cases.value.data())=="Case","TMA case setup");
    check(host.action("Apply case ID to selected cores").enabled,"Case Apply ignored third selected core");doc->clear_history();host.click("Apply case ID to selected cores",draw);check(doc->annotation(ids[2]).tma_core->case_id()=="Case" && doc->undo() && !doc->can_undo(),"Case Apply not one native undo step");host.settle(draw);
    type_text(host,"Case ID","Typed",draw);host.click(panel.bounds(GuiInputTarget::ToggleTmaMissing),draw);check(std::string(cases.value.data())=="Typed" && cases.dirty(),"Missing dropped typed case ID");
    host.click("Apply case ID to selected cores",draw);check(!host.action("Apply case ID to selected cores").enabled && !host.action("Apply core geometry").enabled,"Clean TMA Apply enabled");
    cases.value[0]='X';selection.replace({ids[1]},ids[1]);host.settle(draw);check(detail::GuiTmaTestAccess::discarded(panel),"TMA selection lacks discard");
    doc->set_tma_missing({ids[1]},false);host.settle(draw);check(detail::GuiTmaTestAccess::discarded(panel),"TMA document edit cleared notice");
    type_text(host,"X (px)###Core minimum X (px)","47",draw);check(detail::GuiTmaTestAccess::core_draft(panel).dirty(),"Typed core geometry did not become dirty");
    host.click(panel.bounds(GuiInputTarget::ToggleTmaMissing),draw);check(detail::GuiTmaTestAccess::core_draft(panel).value.bounds.x==47,"Missing discarded typed core geometry");
    doc->clear_history();host.click("Apply core geometry",draw);check(doc->annotation(ids[1]).geometry.bounds().x==47 && doc->undo() && !doc->can_undo(),"Core Apply not one Undo");host.settle(draw);
    host.click(panel.bounds(GuiInputTarget::TmaGridTab),draw);check(!host.action("Apply labels").enabled,"Clean labels Apply enabled");
    type_text(host,"Row label","Typed row",draw);doc->set_tma_missing({ids[0]},false);host.settle(draw);check(std::string(detail::GuiTmaTestAccess::labels_draft(panel).value.row.data())=="Typed row","Missing discarded dirty labels");
    doc->clear_history();host.click("Apply labels",draw);check(doc->tma_grid()->info()->row_labels()[0]=="Typed row" && doc->undo() && !doc->can_undo(),"Labels Apply not one Undo");
    host.settle(draw);type_text(host,"Row label","Pending row",draw);doc->clear_tma_grid(true);host.settle(draw);
    check(model_safety_checks::logged(host,draw).find("Unapplied changes were discarded.")!=std::string::npos,"Removing the grid hid the discard notice");
    std::cout<<"TMA Missing preserves case; three-core no-op availability; Apply Undo; discard lifetime passed\n";
}
inline void captures(const std::filesystem::path& dir) {
    for(bool french:{false,true}) {
        Scene scene(dir,french?"annotations-fr200":"annotations-normal",french);const auto id=scene.doc->add_rectangle({8,8,30,40},"First"),other=scene.doc->add_rectangle({70,8,20,30},"Second");int stage=0;Typing typing;typing.key="X";typing.value="30";
        scene.options.width=french?2200:1100;scene.options.height=french?1800:900;
        scene.commands.before_context=[&](int){if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane))pane->StateStorage.SetInt(pane->GetID(ui::i18n::label("Geometry").c_str()),1);};
        scene.before=[&]{if(stage==7 && typing.step(scene.snapshot.scope.trace))stage=8;};
        scene.editor.after_editor=[&](Geometry& g,Appearance& a,ObjectSelection& s,const char*,bool gd,bool ad){
            if(stage==0){s.replace({id},id);stage=1;return;}
            if(stage==1){if(scene.snapshot.scope.trace.actions.find("Apply object edit")==scene.snapshot.scope.trace.actions.end())return;check(!scene.snapshot.scope.trace.actions.at("Apply object edit").enabled,"Clean geometry Apply enabled");scene.capture="clean";stage=2;return;}
            if(stage==2 && scene.capture.empty()){g.value.bounds.x=20;a.value.probability=true;a.value.reported=.7;stage=3;return;}
            if(stage==3){scene.capture="dirty";stage=4;return;}
            if(stage==4 && scene.capture.empty()){s.replace({other},other);stage=5;return;}
            if(stage==5){check(gd && ad,"Capture lacks discard notices");scene.capture="discard";stage=6;return;}
            if(stage==6 && scene.capture.empty())stage=7;
            if(stage==8 && g.value.bounds.x==30){check(!gd && !ad,"Next Annotations pane edit did not clear discard notices");scene.commands.end_session=true;}
        };scene.run();
    }
}
inline void removal(const std::filesystem::path& dir) {
    Scene scene(dir,"removed-object");const auto id=scene.doc->add_rectangle({8,8,30,40},"First");int stage=0;
    scene.before=[&]{if(stage==2){scene.doc->remove_annotation(id);stage=3;}};
    scene.editor.after_editor=[&](Geometry& g,Appearance& a,ObjectSelection& s,const char*,bool gd,bool ad){
        if(stage==0){s.replace({id},id);stage=1;return;}
        if(stage==1){g.value.bounds.x=25;a.value.probability=true;a.value.reported=.7;stage=2;return;}
        if(stage==3){check(!s.primary() && gd && ad,"Removed object hid dirty-draft discard notices");scene.commands.end_session=true;}
    };scene.run();std::cout<<"Removing the dirty draft's object shows both discard notices\n";
}
inline void active_numeric_discard(const std::filesystem::path& dir) {
    Scene scene(dir,"active-numeric-discard");const auto first=scene.doc->add_rectangle({8,8,30,40},"First"),second=scene.doc->add_rectangle({70,8,20,30},"Second");int stage=0;Typing typing;typing.key="X";typing.value="25";
    scene.commands.before_context=[&](int){if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane))pane->StateStorage.SetInt(pane->GetID(ui::i18n::label("Geometry").c_str()),1);};
    scene.before=[&]{if(stage==1 && typing.step(scene.snapshot.scope.trace))stage=2;};
    scene.editor.after_editor=[&](Geometry& g,Appearance&,ObjectSelection& s,const char*,bool gd,bool){
        if(stage==0){s.replace({first},first);stage=1;return;}
        if(stage==2){check(ImGui::GetActiveID()==ImGui::GetCurrentContext()->InputTextState.ID && g.dirty() && g.value.bounds.x==25,"Active numeric text did not reach the draft");s.replace({second},second);stage=3;return;}
        if(stage==3){check(gd && scene.doc->annotation(first).geometry.bounds().x==8 && scene.doc->annotation(second).geometry.bounds().x==70,"Active numeric retirement lost its notice or applied a draft");scene.commands.end_session=true;}
    };scene.run();std::cout<<"Active numeric draft is dirty before focus loss and selection discards it with a notice\n";
}
inline void tma_captures(const std::filesystem::path& dir) {
    check(glfwInit()!=0,"Draft TMA GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Draft TMA",nullptr,nullptr);check(window,"Draft TMA capture window");glfwMakeContextCurrent(window);
    {
        model_safety_checks::GlHost host(window);const auto style=ImGui::GetStyle();
        for(int locale:{0,2}) {
            ui::i18n::choose(locale);ImGui::GetStyle()=style;if(locale==2)ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=locale==2?32:16;
            auto project=fixture(dir/("tma-capture-"+std::to_string(locale)+".qpxproj"),128,96);auto doc=project->document(1);TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={30,30};grid.column_step={40,0};grid.row_step={0,30};grid.width=grid.height=18;const auto ids=doc->create_tma_grid(grid);
            ObjectSelection selection(*doc);selection.replace({ids[0]},ids[0]);GuiOptions options;options.tma_open=true;detail::GuiTma panel(options);
            auto draw=[&]{panel.draw(*doc,selection,project.get(),false,true,[](GuiInputTarget){});};host.fit("Tissue microarray",locale==2?1060:530,locale==2?1500:780,draw);
            const auto stem=locale==2?"tma-fr200-":"tma-normal-";
            host.capture(dir/(std::string(stem)+"clean.ppm"),draw);
            auto& cases=detail::GuiTmaTestAccess::case_draft(panel);std::strcpy(cases.value.data(),"Typed case");host.capture(dir/(std::string(stem)+"dirty.ppm"),draw);
            selection.replace({ids[1]},ids[1]);host.settle(draw);check(detail::GuiTmaTestAccess::discarded(panel),"TMA capture discard absent");
            const auto text=model_safety_checks::logged(host,draw);check(text.find(ui::i18n::text("Unapplied changes were discarded."))!=std::string::npos,"TMA discard sentence not rendered");
            host.capture(dir/(std::string(stem)+"discard.ppm"),draw);
            type_text(host,"Case ID","Next case",draw);check(!detail::GuiTmaTestAccess::discarded(panel),"Next TMA pane edit did not clear discard notice");
        }
    }glfwDestroyWindow(window);glfwTerminate();
}
inline void run(const std::filesystem::path& input) {
    const auto dir=std::filesystem::absolute(input).lexically_normal();std::filesystem::create_directories(dir);
    detail::Draft<detail::DraftText<8>> text;std::strcpy(text.value.data(),"A");text.reset(text.value);text.value[5]='x';check(!text.dirty(),"Scratch bytes after NUL changed text draft identity");
    names(dir);class_choice(dir);geometry(dir,false);geometry(dir,true);parent_measurement(dir);tma(dir);removal(dir);active_numeric_discard(dir);captures(dir);tma_captures(dir);
}
}
