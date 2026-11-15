#pragma once
#include "gui_draft_checks.hpp"
namespace draft_fix1_checks {
using namespace qpx;
using Access=detail::GuiTmaTestAccess;
struct ActiveTyping {
    std::string key,value;int stage=0;ImVec2 point{};
    bool step(const ui::WorkflowTrace& trace) {
        auto& io=ImGui::GetIO();auto& g=*ImGui::GetCurrentContext();
        if(stage==0){auto found=trace.actions.find(key);if(found==trace.actions.end() || !found->second.enabled)return false;const auto b=found->second.bounds;point={float(b.x+b.width-24),float(b.y+std::min(b.height/2,14.))};io.AddMousePosEvent(point.x,point.y);stage=1;return false;}
        io.AddMousePosEvent(point.x,point.y);
        if(stage==1){if(!g.HoveredWindow)return false;io.AddMouseButtonEvent(0,true);stage=2;}
        else if(stage==2){if(!g.ActiveId || g.ActiveId!=g.InputTextState.ID)return false;io.AddMouseButtonEvent(0,false);stage=3;}
        else if(stage==3){if(io.MouseDown[0])return false;io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);stage=4;}
        else if(stage==4){if(!g.InputTextState.HasSelection())return false;io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);stage=5;}
        else if(stage==5){if(io.KeyCtrl)return false;io.AddInputCharactersUTF8(value.c_str());stage=6;}
        else if(stage==6 && g.InputTextState.TextA.Data && std::string(g.InputTextState.TextA.Data)==value)return true;
        return false;
    }
};
template<class Host,class Draw>inline void type_active(Host& host,const char* key,const char* value,Draw& draw) {
    ActiveTyping typing{key,value};host.until(draw,[&]{return typing.step(host.trace);},"Active draft typing did not finish");
    check(ImGui::GetActiveID()==ImGui::GetCurrentContext()->InputTextState.ID,"Typed field did not remain active");
}
inline TmaGridOptions grid_options(){TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={20,30};grid.column_step={35,0};grid.row_step={0,30};grid.width=grid.height=18;return grid;}
inline void d1_canvas(const std::filesystem::path& out) {
    draft_checks::Scene scene(out,"d1-canvas");scene.options.width=1800;scene.options.height=1200;scene.options.tma_open=true;
    const auto ids=scene.doc->create_tma_grid(grid_options());scene.doc->set_tma_case_ids({{ids[0],"A-case"},{ids[1],"B-case"}});
    ActiveTyping typing{"Case ID","Typed for A"};int stage=0;ObjectSelection* selected=nullptr;int observations=0;
    scene.editor.after_editor=[&](auto&,auto&,ObjectSelection& selection,const char*,bool,bool){selected=&selection;if(stage==0){selection.replace({ids[0]},ids[0]);stage=1;}};
    scene.commands.before_context=[&](int){if(auto* window=ImGui::FindWindowByName(ui::tool_window_label(ui::ToolId::Tma))){ImGui::SetWindowPos(window,{20,60});ImGui::SetWindowSize(window,{600,1000});}};
    scene.before=[&]{if(stage==1){auto* window=ImGui::FindWindowByName(ui::tool_window_label(ui::ToolId::Tma));const auto it=scene.snapshot.scope.trace.actions.find("Case ID");if(!window || it==scene.snapshot.scope.trace.actions.end() || it->second.bounds.x>620)return;if(typing.step(scene.snapshot.scope.trace))stage=2;}};
    scene.commands.replay_input=[&](const auto& emit){if(stage==2 || stage==4){auto input=key(0,stage==2?GuiInputKind::LeftDown:GuiInputKind::LeftUp);input.target=GuiInputTarget::Image;input.x=55;input.y=30;emit(input);stage=stage==2?3:5;}};
    scene.snapshot.after=[&](int){if((stage!=3 && stage!=5) || !selected || selected->primary()!=ids[1])return;
        const auto& text=scene.snapshot.text;const auto action=scene.snapshot.scope.trace.actions.at("Apply case ID to selected cores");
        const bool own=text.find("B-case")!=std::string::npos,typed=text.find("Typed for A")!=std::string::npos,notice=text.find("Unapplied changes were discarded.")!=std::string::npos;
        std::cout<<"D1 canvas observation "<<observations<<": case_apply_enabled="<<action.enabled<<" own_case_visible="<<own<<" old_typed_visible="<<typed<<" notice="<<notice<<" first_document_case="<<scene.doc->annotation(ids[0]).tma_core->case_id()<<'\n';
        write(out/("d1-canvas-observation-"+std::to_string(observations)+".txt"),text);
        check(!action.enabled && own && !typed && notice && scene.doc->annotation(ids[0]).tma_core->case_id()=="A-case","D1 canvas press carried the first core's case draft into the second core");
        if(++observations==2)scene.commands.end_session=true;else stage=4;
    };scene.run();check(observations==2,"D1 did not observe both frames");
}
inline void d2_add(const std::filesystem::path& out) {
    auto project=fixture(out/"d2-source.qpxproj",128,96);auto doc=project->document(1);const auto ids=doc->create_tma_grid(grid_options());doc->remove_annotation(ids[0]);
    ObjectSelection selection(*doc);GuiOptions initial;initial.tma_open=true;detail::GuiTma panel(initial);simplification_tools_checks::NativeUi host;
    auto draw=[&]{panel.draw(*doc,selection,project.get(),false,true,[](GuiInputTarget){});};host.fit("Tissue microarray",600,1100,draw);
    draft_checks::type_text(host,"Case ID","Vacant case",draw);draft_checks::type_text(host,"Width (px)###Core width (px)","23",draw);doc->clear_history();
    host.click("Add core in vacant slot",draw);const auto added=doc->tma_grid()->core(0,0);check(bool(added),"D2 failed to add the vacant core");
    const auto text=model_safety_checks::logged(host,draw);write(out/"d2-after-add.txt",text);
    const bool notice=text.find("Unapplied changes were discarded.")!=std::string::npos;
    std::cout<<"D2 added case="<<added->tma_core->case_id()<<" width="<<added->geometry.bounds().width<<" case_dirty="<<Access::case_draft(panel).dirty()<<" core_dirty="<<Access::core_draft(panel).dirty()<<" notice="<<notice<<'\n';
    check(added->tma_core->case_id()=="Vacant case" && added->geometry.bounds().width==23 && !notice && !Access::case_draft(panel).dirty() && !Access::core_draft(panel).dirty() && !host.action("Apply case ID to selected cores").enabled && !host.action("Apply core geometry").enabled,"D2 Add core falsely reported discarded changes");
    check(doc->undo() && !doc->can_undo() && !doc->tma_grid()->core(0,0),"D2 Add core was not one undo step");
    host.settle(draw);draft_checks::type_text(host,"Case ID","Another pending case",draw);
    auto choose_slot=[&](int row){ImGuiWindow* catalog=nullptr;for(auto* window:host.context->Windows)if(std::strstr(window->Name,"core catalog"))catalog=window;check(catalog,"D2 core catalog missing");const auto p=catalog->DC.CursorStartPos;const auto h=ImGui::GetTextLineHeightWithSpacing();host.click(Rect{p.x,p.y+row*h,150,h},draw);};
    choose_slot(0);check(Access::case_draft(panel).dirty() && std::string(Access::case_draft(panel).value.data())=="Another pending case" && !Access::discarded(panel),"D2 reselecting the same vacant slot discarded its draft");
    choose_slot(1);check(selection.primary()==ids[1] && Access::discarded(panel),"D2 choosing another slot did not report the discarded case draft");
}
inline void measure(const std::filesystem::path& out,const char* tag) {
    Document doc(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));doc.set_history_limits(0,0);std::uint64_t selected=0;
    for(int i=0;i<10000;++i)selected=doc.add_rectangle({0,0,4,4},"Area "+std::to_string(i));
    for(int i=0;i<200000;++i)doc.add_detection(Roi::rectangle({0,0,1,1}));
    detail::ParentChoices choices;const auto start=std::chrono::steady_clock::now();choices.rebuild(doc,selected);
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();check(choices.rows.size()==9999,"D3 mixed-document parent count changed");
    write(out/(std::string("parent-")+tag+".txt"),"10000 areas + 200000 detections; eligible=9999; rebuild_ms="+std::to_string(elapsed)+"\n");std::cout<<"D3 "<<tag<<" mixed-document rebuild_ms="<<elapsed<<'\n';
}
inline void d1_host(const std::filesystem::path& out) {
    for(const std::string field:{"Case ID","X (px)###Core minimum X (px)","Row label","Move X (px)"}) {
        auto project=fixture(out/("host-"+std::to_string(field.size())+".qpxproj"),128,96);auto doc=project->document(1);const auto ids=doc->create_tma_grid(grid_options());doc->set_tma_case_ids({{ids[0],"A-case"},{ids[1],"B-case"}});
        ObjectSelection selection(*doc);selection.replace({ids[0]},ids[0]);GuiOptions options;options.tma_open=true;options.tma_tab=(field=="Row label" || field=="Move X (px)")?1:0;detail::GuiTma panel(options);simplification_tools_checks::NativeUi host;
        Rect press{};std::string text;int presses=0;
        auto draw=[&]{ImGui::SetNextWindowPos({10,10});ImGui::SetNextWindowSize({260,90});ImGui::Begin("Canvas press stand-in");ImGui::InvisibleButton("take focus on press",{220,35});const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();press={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)){selection.replace({ids[1]},ids[1]);++presses;}ImGui::End();
            ImGui::LogToBuffer();host.context->LogWindow=nullptr;panel.draw(*doc,selection,project.get(),false,true,[](GuiInputTarget){});text=host.context->LogBuffer.c_str();ImGui::LogFinish();};
        host.fit("Tissue microarray",700,1300,draw);auto* window=ImGui::FindWindowByName(ui::tool_window_label(ui::ToolId::Tma));ImGui::SetWindowPos(window,{350,10});host.settle(draw);
        const char* typed=field=="Case ID"?"Typed for A":field=="Row label"?"Pending row":field=="Move X (px)"?"12":"27";
        type_active(host,field.c_str(),typed,draw);
        if(field=="Move X (px)") {
            const auto active=ImGui::GetActiveID();selection.replace({ids[1]},ids[1]);host.frame(draw);
            check(ImGui::GetActiveID()==active && std::string(host.context->InputTextState.TextA.Data)=="12","D1 direct selection retired a non-draft numeric field");
            selection.replace({ids[0]},ids[0]);host.frame(draw);
            check(ImGui::GetActiveID()==active && std::string(host.context->InputTextState.TextA.Data)=="12","D1 returning to the first core retired the non-draft field");
        }
        auto& io=ImGui::GetIO();io.AddMousePosEvent(float(press.x+press.width/2),float(press.y+press.height/2));host.frame(draw);io.AddMouseButtonEvent(0,true);host.frame(draw);
        auto verify=[&]{check(presses==1 && selection.primary()==ids[1],"D1 stand-in did not select on press");
            if(field=="Move X (px)"){check(Access::move(panel).x==12,"D1 non-draft final numeric write was lost");return;}
            const bool own=field=="Case ID"?std::string(Access::case_draft(panel).value.data())=="B-case":field=="Row label"?std::string(Access::labels_draft(panel).value.row.data())=="A":Access::core_draft(panel).value.bounds.x==doc->objects().at(ids[1]).geometry.bounds().x;
            const bool clean=field=="Case ID"?!Access::case_draft(panel).dirty():field=="Row label"?!Access::labels_draft(panel).dirty():!Access::core_draft(panel).dirty();
            check(own && clean && text.find("Unapplied changes were discarded.")!=std::string::npos,"D1 host carried a draft through a press: "+field);
            check(doc->objects().at(ids[0]).tma_core->case_id()=="A-case" && doc->objects().at(ids[0]).geometry.bounds().x==11 && doc->tma_grid()->info()->row_labels()[0]=="A","D1 retirement modified the first core/grid");};
        verify();host.frame(draw);verify();io.AddMouseButtonEvent(0,false);host.frame(draw);std::cout<<"D1 host "<<field<<" passed after press and next frame\n";
    }
}
inline void parent_counter(const std::filesystem::path& out) {
    draft_checks::Scene scene(out,"parent-counter");const auto first=scene.doc->add_rectangle({8,8,30,40},"First"),other=scene.doc->add_rectangle({70,8,20,30},"Other");int stage=0,edits=0;std::uint64_t rebuilds=0;
    scene.editor.parent_choices=[&](const detail::ParentChoices& choices){rebuilds=choices.rebuilds;};
    scene.commands.before_context=[&](int){if(auto* pane=ImGui::FindWindowByName(GuiWorkspace::objects_pane)){pane->StateStorage.SetInt(pane->GetID(ui::i18n::label("Geometry").c_str()),1);if(stage==3){const auto id=ImHashStr("##value",0,ImHashStr("Parent ID",0,pane->ID));auto& g=*ImGui::GetCurrentContext();ImGui::FocusWindow(pane);g.NavActivateId=g.NavActivateDownId=g.NavActivatePressedId=id;stage=4;}}};
    scene.editor.after_editor=[&](auto&,auto&,ObjectSelection& selection,const char*,bool,bool){
        if(stage==0){selection.replace({first},first);stage=1;return;}
        if(stage==1){check(rebuilds==0,"D3 closed combo rebuilt during edits");if(edits<20){scene.doc->rename_annotation(other,"Edit "+std::to_string(++edits));return;}selection.replace({other},other);stage=2;return;}
        if(stage==2){check(rebuilds==0,"D3 closed combo rebuilt on selection");stage=3;return;}
        if(stage==4 && rebuilds==1){scene.doc->rename_annotation(first,"Changed while open");stage=5;return;}
        if(stage==5){check(rebuilds==2,"D3 open combo did not rebuild exactly once after revision");scene.commands.end_session=true;}
    };scene.run();std::cout<<"D3 closed edits=20 plus selection: 0 rebuilds; open: 1; revision while open: 2\n";
}
inline void reasons(const std::filesystem::path& out) {
    draft_checks::Scene scene(out,"clean-reasons");const auto id=scene.doc->add_rectangle({8,8,30,40},"First");int stage=0;
    scene.editor.after_editor=[&](auto&,auto&,ObjectSelection& selection,const char*,bool,bool){if(stage==0){selection.replace({id},id);stage=1;return;}const auto& actions=scene.snapshot.scope.trace.actions;
        for(const auto* key:{"Apply object edit","Apply###Apply object display state","Clear properties and unlock (Undo)"}){auto it=actions.find(key);if(it==actions.end())return;const auto expected=std::string(key)=="Clear properties and unlock (Undo)"?"This object has no properties.":"There is nothing to apply.";check(!it->second.enabled && it->second.reason==expected,"D5 annotation disabled reason mismatch: "+std::string(key));if(std::string(key)=="Apply object edit")check(it->second.inline_reason==expected,"D5 primary reason was not inline");}scene.commands.end_session=true;};scene.run();
    auto project=fixture(out/"clean-tma.qpxproj",128,96);auto doc=project->document(1);const auto ids=doc->create_tma_grid(grid_options());ObjectSelection selection(*doc);selection.replace({ids[0]},ids[0]);GuiOptions options;options.tma_open=true;detail::GuiTma panel(options);simplification_tools_checks::NativeUi host;auto draw=[&]{panel.draw(*doc,selection,project.get(),false,true,[](GuiInputTarget){});};host.fit("Tissue microarray",700,1200,draw);
    for(const auto* key:{"Apply case ID to selected cores","Apply core geometry"}){const auto a=host.action(key);check(!a.enabled && a.reason=="There is nothing to apply.","D5 clean TMA reason mismatch");}
    host.click(panel.bounds(GuiInputTarget::TmaGridTab),draw);check(!host.action("Apply labels").enabled && host.action("Apply labels").reason=="There is nothing to apply.","D5 label reason mismatch");std::cout<<"D5 clean annotation/TMA actions expose exact reasons\n";
}
inline void image_notices(const std::filesystem::path& out) {
    for(bool direct:{false,true}) {
        draft_checks::Scene scene(out,direct?"direct-image-notice":"stale-image-notice");const auto first=scene.doc->add_rectangle({8,8,30,40},"First"),other=scene.doc->add_rectangle({70,8,20,30},"Other");
        const auto original=scene.project->active_image();const auto source=out/(direct?"direct-second.ppm":"stale-second.ppm");write(source,"P6\n2 2\n255\n"+std::string(12,char(80)));const auto second=scene.project->add_document(std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,80))),"Second image",source.u8string());scene.project->set_active_image(original);int stage=0;
        scene.editor.after_editor=[&](auto& geometry,auto& appearance,ObjectSelection& selection,const char*,bool gd,bool ad){
            if(stage==0){selection.replace({first},first);stage=1;return;}
            if(stage==1){geometry.value.bounds.x=25;appearance.value.probability=true;appearance.value.reported=.7;if(direct){scene.pending=GuiInputKind::NextImageKey;stage=3;}else {selection.replace({other},other);stage=2;}return;}
            if(stage==2){check(gd && ad,"D6 initial selection notice absent");scene.pending=GuiInputKind::NextImageKey;stage=3;return;}
            if(stage==3 && scene.inputs.active_image==second){check(gd==direct && ad==direct,"D6 image notice inherited history instead of describing this switch");const std::string text=ImGui::GetCurrentContext()->LogBuffer.c_str();check((text.find("Unapplied geometry changes were discarded.")!=std::string::npos)==direct && (text.find("Unapplied changes were discarded.")!=std::string::npos)==direct,"D6 rendered annotation notice disagrees with this image switch");scene.commands.end_session=true;}
        };scene.run();std::cout<<"D6 annotations "<<(direct?"direct dirty switch":"old notice then switch")<<" passed\n";
    }
    auto first=std::make_shared<Document>(Image(128,96,std::vector<std::uint8_t>(128*96*3,80))),second=std::make_shared<Document>(Image(128,96,std::vector<std::uint8_t>(128*96*3,80))),third=std::make_shared<Document>(Image(128,96,std::vector<std::uint8_t>(128*96*3,80)));
    const auto a=first->create_tma_grid(grid_options()),b=second->create_tma_grid(grid_options()),c=third->create_tma_grid(grid_options());ObjectSelection sa(*first),sb(*second),sc(*third);sa.replace({a[0]},a[0]);sb.replace({b[0]},b[0]);sc.replace({c[0]},c[0]);Document* current=first.get();ObjectSelection* selected=&sa;
    GuiOptions options;options.tma_open=true;detail::GuiTma panel(options);simplification_tools_checks::NativeUi host;auto draw=[&]{panel.draw(*current,*selected,nullptr,false,true,[](GuiInputTarget){});};host.fit("Tissue microarray",700,1200,draw);
    draft_checks::type_text(host,"Case ID","First pending",draw);sa.replace({a[1]},a[1]);host.settle(draw);check(Access::discarded(panel),"D6 TMA initial notice missing");current=second.get();selected=&sb;host.settle(draw);check(!Access::discarded(panel),"D6 TMA carried old notice into another document");
    draft_checks::type_text(host,"Case ID","Second pending",draw);current=third.get();selected=&sc;host.settle(draw);check(Access::discarded(panel) && model_safety_checks::logged(host,draw).find("Unapplied changes were discarded.")!=std::string::npos,"D6 TMA direct dirty switch lost notice");std::cout<<"D6 TMA old-notice/direct-dirty document switches passed\n";
}
inline void run(const std::filesystem::path& input,const std::string& part) {
    const auto out=std::filesystem::absolute(input).lexically_normal();std::filesystem::create_directories(out);
    if(part=="d1")d1_canvas(out);
    if(part=="d2")d2_add(out);
    if(part=="before-measure")measure(out,"before");
    if(part=="all"){d1_canvas(out);d1_host(out);d2_add(out);measure(out,"after");parent_counter(out);reasons(out);image_notices(out);}
}
}
