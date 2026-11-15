#pragma once
#include "gui_record_fixture.hpp"
namespace record_list_checks {
using namespace qpx;
inline void no_invented_states(const std::vector<ui::RecordListItem>& rows,const char* family) {
    check(!rows.empty(),std::string(family)+" fixture has no rendered row");
    for(const auto& row:rows)check(!row.row.state && row.second_line.find("Needs recheck")==std::string::npos,std::string(family)+" invented a native status");
}
inline void presentation(const std::filesystem::path&) {
    simplification_tools_checks::NativeUi host;ui::RecordRows cache;std::vector<ui::RecordListItem> rows;ui::RecordListPresentation presentation;ui::record_list_trace=&rows;ui::record_list_presentation=&presentation;
    struct Reset{~Reset(){ui::record_list_trace=nullptr;ui::record_list_presentation=nullptr;}}reset;
    std::uint64_t selected=0;std::string text;int copies=0;
    struct Callback {int* copies;Callback(int* c):copies(c){}Callback(const Callback& other):copies(other.copies){++*copies;}void operator()(std::uint64_t)const{}};
    ui::RecordListActions actions;actions.use=Callback(&copies);const int initial_copies=copies;
    auto draw=[&]{rows.clear();ImGui::Begin("List presentation");ImGui::LogToBuffer();selected=ui::draw_record_list("Records",cache,selected,actions);text=host.context->LogBuffer.c_str();ImGui::LogFinish();ImGui::End();};
    float one=0,two=0;
    for(std::uint64_t count:{0ULL,1ULL,2ULL,4ULL}) {
        cache.refresh(&host,count,0,[&](auto& records){for(std::uint64_t i=1;i<=count;++i)records.push_back(ui::record_row(i,"File","Raw pixel file"));});host.fit("List presentation",650,450,draw);
        check(presentation.box==(count!=0) && !cache.two_lines,"No-state list is not one-line/empty presentation");
        auto* window=ImGui::FindWindowByName("List presentation");ImGuiWindow* child=nullptr;for(auto* c:window->DC.ChildWindows)if(c->Active)child=c;
        if(!count)check(!child && text.find("No kept records")!=std::string::npos,"Empty list is a blank box or missing its message");
        else {check(child && std::abs(child->Size.y-presentation.box_height)<1.f,"List box did not use cached row count/height");if(count==1)one=child->Size.y;if(count==2)two=child->Size.y;if(count==4)check(child->ScrollMax.y>0,"Four-row list cannot scroll");}
    }
    check(two>one && two<2*one+ImGui::GetStyle().ItemSpacing.y,"Short list retains empty vertical space");
    cache.refresh(&host,100,0,[&](auto& r){auto a=ui::record_row(1,"Classifier","Object classifier");a.kind="Prediction";r.push_back(a);r.push_back(ui::record_row(2,"Pixel","Pixel classifier"));r.push_back(ui::record_row(3,"Raw","Raw pixel file"));});host.settle(draw);
    check(cache.two_lines,"Kind row did not establish uniform two-line layout");no_invented_states(rows,"classifier/pixel/raw");check(rows.front().second_line=="Prediction","Kind text acquired an invented state");
    cache.refresh(&host,101,0,[&](auto& r){r.push_back(ui::record_row(9,"Unexpected","Cells","future %s token"));});host.settle(draw);check(rows.size()==1 && rows[0].second_line=="Unknown status (future %s token)","Unexpected native status was hidden as no-state");
    check(copies==initial_copies,"draw_record_list copied an action std::function");
    std::cout<<"Fix1 rows: explicit no-state, unknown, kind, uniform cached height, empty message/no box, 1/2 tight rows, 4 scroll, no callback copies passed\n";
}
inline void recipe_choices(const std::filesystem::path& dir) {
    simplification_tools_checks::NativeUi host;auto doc=record_fixture::source(dir);const auto ids=record_fixture::seed(*doc,dir);
    std::vector<ui::RecordListItem> rows;std::vector<ui::RecordListPresentation> lists;ui::record_list_trace=&rows;ui::record_list_presentations=&lists;
    struct Reset{~Reset(){ui::record_list_trace=nullptr;ui::record_list_presentations=nullptr;}}reset;
    detail::GuiLinearUnmixing unmix;unmix.open();auto draw=[&]{rows.clear();lists.clear();unmix.draw(*doc);};host.fit("Linear unmixing",1100,1300,draw);
    const auto parent=[&]()->const ui::RecordListPresentation&{const auto it=std::find_if(lists.begin(),lists.end(),[](const auto& l){return l.heading=="Input correction (explicit)";});check(it!=lists.end(),"Parent heading/list missing");return *it;};
    check(parent().none_current,"Original input does not read as current");no_invented_states(rows,"recipes");const auto bounds=unmix.bounds(detail::GuiLinearUnmixing::Action::Parent);check(bounds.width>0 && bounds.height>0,"Parent replay mark absent");host.click(bounds,draw);check(!parent().none_current,"Original input still reads current after choosing a correction");
    auto* window=ImGui::FindWindowByName("Linear unmixing");ImGui::ActivateItemByID(ImHashStr(ui::i18n::label("None / original input").c_str(),0,window->ID));host.settle(draw);check(parent().none_current,"None choice cannot restore original input");
    // Both public setters must retain an unresolved parent and let native validation refuse it.
    const std::string missing(64,'f');const auto retained=dark_flat_recipe_ids(*doc);check(std::find(retained.begin(),retained.end(),missing)==retained.end(),"Missing-parent fixture unexpectedly retained");
    std::string expected_error;try{load_dark_flat_recipe(*doc,missing);}catch(const std::out_of_range& error){expected_error=error.what();}check(!expected_error.empty(),"Native missing-parent oracle did not refuse");
    for(int setter:{0,1}) {
        if(setter==0)unmix.set_matrix(load_linear_unmix_recipe(*doc,ids.unmix).matrix,missing);
        else unmix.set_matrix_file((dir/"not-opened-before-parent-validation.json").u8string(),missing);
        for(int frame=0;frame<6;++frame)host.frame(draw);
        check(!parent().none_current,"Unlisted parent silently became original input");
        for(const auto& row:rows)check(!row.highlighted || row.row.identifier!=ids.correction,"Unlisted parent highlighted a listed correction");
        const auto missing_scope=ImHashStr(missing.c_str(),0,window->ID);window->StateStorage.SetInt(ImHashStr("Details",0,missing_scope),1);
        std::string parent_text;auto logged=[&]{ImGui::LogToBuffer();host.context->LogWindow=nullptr;draw();parent_text=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.settle(logged);
        check(parent_text.find(missing)!=std::string::npos,"Unlisted parent full identity disappeared from Details");
        host.click(unmix.bounds(setter==0?detail::GuiLinearUnmixing::Action::Apply:detail::GuiLinearUnmixing::Action::LoadFile),draw);
        check(unmix.error()==expected_error && !unmix.busy() && !unmix.request(),"Unlisted parent did not preserve native refusal");
        ImGui::ActivateItemByID(ImHashStr(ui::i18n::label("None / original input").c_str(),0,window->ID));host.settle(draw);check(parent().none_current,"Explicit None did not clear unresolved parent");
    }
    // Directly check the selection-return contract independently of window internals.
    ui::StringRecordRows selector;std::string selection=missing;const std::vector<std::string> known{ids.correction};
    auto select_draw=[&]{ImGui::Begin("Unresolved identity");selection=selector.draw("Kept correction",known,selection,doc.get(),doc->revision(),"Dark/flat correction",{},"None / original input");ImGui::End();};host.settle(select_draw);check(selection==missing,"String list return silently cleared an unlisted identifier");
    std::cout<<"Fix2 missing parent: both setters preserve full identity/no None highlight/native error; explicit None clears; string selection remains exact\n";
    detail::GuiMarkerRules marker;MarkerRule rule=load_marker_rule(*doc,ids.marker);marker.set_rule(rule);marker.open();std::string text;
    auto marker_draw=[&]{rows.clear();ImGui::LogToBuffer();host.context->LogWindow=nullptr;marker.draw(*doc,0);text=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.fit("Marker rules",1100,1350,marker_draw);auto* mwindow=ImGui::FindWindowByName("Marker rules");mwindow->StateStorage.SetInt(ImHashStr(ui::i18n::label("Edit marker rule").c_str(),0,mwindow->ID),1);host.settle(marker_draw);
    ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("Measurement domain",0,mwindow->ID)));host.settle(marker_draw);auto* popup=ImGui::FindWindowByName("##Combo_00");check(popup && popup->Active,"Marker domain combo absent");const int item=2;const auto item_scope=ImHashData(&item,sizeof(item),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text("Unmixed components/residuals"),0,item_scope));host.settle(marker_draw);check(marker.rule().domain=="qpx.linear-unmixed-samples/1" && marker.rule().unmix_record.empty(),"Marker empty scope was not reached through actual domain control");
    check(text.find("Choose kept recipe")!=std::string::npos,"Missing marker recipe-selection hint");check(marker.bounds(detail::GuiMarkerRules::Action::Scope).width>0,"Marker scope mark missing");
    // Open selected correction Details and activate its actual Copy control.
    detail::GuiFluorescence fluo;fluo.open();auto fluo_draw=[&]{rows.clear();ImGui::LogToBuffer();host.context->LogWindow=nullptr;fluo.draw(*doc,0);text=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.fit("Fluorescence cells",1100,1300,fluo_draw);fluo.set_correction(replay_dark_flat_recipe(*doc,ids.correction));host.settle(fluo_draw);
    auto* fwindow=ImGui::FindWindowByName("Fluorescence cells");const auto scope=ImHashStr(ids.correction.c_str(),0,fwindow->ID);fwindow->StateStorage.SetInt(ImHashStr("Details",0,scope),1);host.settle(fluo_draw);check(text.find(ids.correction)!=std::string::npos,"Selected correction full ID inaccessible in Details");ImGui::ActivateItemByID(ImHashStr(ui::i18n::label("Copy identifier").c_str(),0,scope));host.settle(fluo_draw);check(std::string(ImGui::GetClipboardText())==ids.correction,"Correction Copy did not preserve full exact identifier");no_invented_states(rows,"fluorescence");
    detail::GuiDarkFlat dark;dark.open();auto dark_draw=[&]{rows.clear();dark.draw(*doc);};host.fit("Dark/flat correction",1100,1300,dark_draw);no_invented_states(rows,"dark/flat");
    std::cout<<"Fix1 recipe rows/Parent replay/explicit current None/marker hint/selected correction Details and exact Copy passed\n";
}
inline void raw_planes(const std::filesystem::path&) {
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";Document doc(open_ome_zarr(source.u8string()));const auto roi=doc.add_rectangle({0,0,4,4});doc.rename_annotation(roi,"Same object");
    RawRegionRequest r;r.region={0,0,0,4,4};r.channels=2;RawAnalysisRequest a(doc,r,roi);const auto first=doc.keep_raw_analysis(a,analyze_raw(a));r.z=1;r.t=1;doc.set_object_plane(roi,{-1,1,1});RawAnalysisRequest b(doc,r,roi);const auto second=doc.keep_raw_analysis(b,analyze_raw(b));
    GuiOptions o;o.raw_open=true;o.raw_tab=3;detail::GuiRaw panel(o);ObjectSelection selection(doc);simplification_tools_checks::NativeUi host;std::vector<ui::RecordListItem> rows;ui::record_list_trace=&rows;struct Reset{~Reset(){ui::record_list_trace=nullptr;}}reset;
    auto draw=[&]{rows.clear();panel.draw(doc,selection,nullptr,false,[](GuiInputTarget){});};host.fit("Quantitative channels",1100,1300,draw);host.until(draw,[&]{return panel.ready(doc);},"Raw plane metadata did not finish");
    check(rows.size()==2 && rows[0].row.id==first && rows[1].row.id==second,"Raw plane rows absent");check(rows[0].row.detail=="Z 0 / time 0 / rectangle" && rows[1].row.detail=="Z 1 / time 1 / rectangle","Two planes of one object are indistinguishable");
    for(const auto& row:rows){const auto raw=doc.raw_analysis_status(row.row.id);check(row.second_line==row.row.detail+" / "+ui::caption(ui::parse_record_state(raw),raw),"Raw plane or live native status missing from drawn second line");}
    std::cout<<"Fix1 same-object different Z/time rows and native live status passed\n";
}
}
