#pragma once
#include "../gui_class_palette.hpp"
#include "../gui_overlay.hpp"
#include "../gui_shell.hpp"
#include "../gui_object_classifier.hpp"
#include <regex>

namespace workflow_display_checks {
inline void palette_and_labels() {
    Document doc(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));
    doc.set_class("A",0x41e1be);doc.set_class("B",0x41e1be);doc.set_class("C",0x123456);
    const auto palette=detail::class_palette(doc,{"A","B","C"});
    check(palette[0]!=palette[1] && palette[2]==0xff563412u,"Default classes collapsed or explicit colour lost");
    for(float scale:{1.f,1.5f,2.f}) {const float text=16*scale;const auto bar=scale_bar_layout(text,scale);
        check(bar.label_offset_y>=text+bar.thickness && bar.outline>0,"Scale label overlaps bar");}
    const std::regex doubled(R"(\([^()]*\)\s*\([^()]*\)$)");
    for(std::size_t i=0;i<ui::shell_command_count;++i) {
        std::string label=ui::shell_names[i];if(*ui::shell_chords[i])label+=" ("+std::string(ui::shell_chords[i])+")";
        check(!std::regex_search(label,doubled),"Doubled shell chord: "+label);
    }
    // Inspect the literal IconButton callers, including their optional hints.
    const std::regex icon(R"rx(ui::IconButton\("([^"]+)"\s*,\s*ui::Icon::\w+(?:\s*,\s*ui::design::Action::\w+\s*,\s*"([^"]*)")?)rx");
    for(const auto& file:std::filesystem::directory_iterator(std::filesystem::path(__FILE__).parent_path().parent_path())) {
        if(file.path().filename().u8string().rfind("gui",0)!=0 || !file.is_regular_file())continue;
        const auto source=read(file.path());
        for(std::sregex_iterator it(source.begin(),source.end(),icon),end;it!=end;++it) {
            auto label=(*it)[1].str();label=label.substr(0,label.find("##"));
            const auto hint=(*it)[2].str();if(!hint.empty())label+=" ("+hint+")";
            check(!std::regex_search(label,doubled),"Doubled icon hint: "+label);
        }
    }
    std::cout<<"Pass B palette, scale layout, shell labels and icon hints passed\n";
}
inline void classifier_scope() {
    simplification_tools_checks::NativeUi host;
    Document doc(Image(128,96,std::vector<std::uint8_t>(128*96*3,80)));
    const std::vector<std::uint64_t> ids={doc.add_detection(Roi::rectangle({2,2,2,2})),doc.add_detection(Roi::rectangle({20,2,20,20}))};
    const auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture());
    const auto expected=classify_objects(ObjectFeatureRequest(doc,ids),model);
    std::vector<std::pair<std::uint64_t,std::string>> labels;
    for(std::size_t i=0;i<ids.size();++i)labels.emplace_back(ids[i],model.classes()[expected->predictions()[i].class_index]);
    doc.apply_classifications(labels);
    ObjectSelection selection(doc);selection.replace(ids);
    GuiOptions options;options.classifier_open=true;options.classifier_selected_only=true;options.classifier_unclassified_only=false;options.classifier_model=std::make_shared<const ForestModel>(model);
    detail::GuiObjectClassifier panel(options);
    auto draw=[&]{panel.update(doc,selection);panel.draw(doc,selection,false,true,[](GuiInputTarget){});};
    host.fit("Object classifiers",700,1100,draw);host.click("Predict object classes",draw);
    host.until(draw,[&]{return panel.predicted() || !panel.error().empty();},"Classifier preview did not finish");
    check(panel.predicted(),"Classifier preview failed: "+panel.error());const auto preview=panel.result();
    selection.replace({ids.back()});host.settle(draw);
    check(panel.result()==preview && panel.predicted(),"Completed classifier preview lost on selection change");
    host.click("Apply predictions (one Undo)",draw);
    check(panel.applied() && panel.changed()==0,"Zero-change Apply fixture did not apply");
    ImGui::NewFrame();ImGui::LogToBuffer();draw();const std::string text=host.context->LogBuffer.c_str();ImGui::LogFinish();ImGui::Render();
    check(text.find("No objects changed.")!=std::string::npos && text.find("Undo available")==std::string::npos,"Zero-change Apply claims Undo");
    std::cout<<"Pass B classifier completed preview and zero-change feedback passed\n";
}
inline void tma_scope() {
    simplification_tools_checks::NativeUi host;
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    auto doc=std::make_shared<Document>(open_ome_zarr(source.u8string()));Project project;project.add_document(doc);
    TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={1,1};grid.column_step={3,0};grid.width=grid.height=1;
    const auto cores=doc->create_tma_grid(grid);ObjectSelection selection(*doc);
    GuiOptions options;options.tma_open=true;detail::GuiTma panel(options);bool batch=false;
    auto draw=[&]{batch=panel.draw(*doc,selection,&project,false,true,[](GuiInputTarget){});};
    host.fit("Tissue microarray",700,1100,draw);
    // The catalogue contains a core on a channel outside the displayed view.
    doc->set_object_plane(cores.front(),{0,0,0});auto view=*doc->server()->raw_view();view.channels={1,1,1};doc->set_raw_view(view);host.settle(draw);
    host.click("Per-core batch...",draw);
    check(!batch && !panel.error().empty() && selection.ids().empty(),"Invisible TMA batch escaped guarded selection");
    ImGuiWindow* catalog=nullptr;for(auto* window:host.context->Windows)if(std::string(window->Name).find("core catalog")!=std::string::npos)catalog=window;
    check(catalog,"TMA core catalogue missing");
    const auto at=catalog->DC.CursorStartPos;
    host.click(Rect{at.x,at.y,100,ImGui::GetTextLineHeightWithSpacing()},draw);
    check(!panel.error().empty() && selection.ids().empty(),"Invisible catalogue core escaped guarded selection");
    view.channels={0,1,2};doc->set_raw_view(view);selection.replace(cores);host.settle(draw);
    ImGui::NewFrame();ImGui::LogToBuffer();draw();const std::string text=host.context->LogBuffer.c_str();ImGui::LogFinish();ImGui::Render();
    check(text.find("Applies to 2 cores.")!=std::string::npos && text.find("Core ")<text.find("Cores"),"TMA target count/current-core feedback missing");
    host.click(panel.bounds(GuiInputTarget::TmaGridTab),draw);host.settle(draw);
    check(host.action("Remove row").caption.find("(2 cores)")!=std::string::npos && host.action("Remove column").caption.find("(1 cores)")!=std::string::npos,"TMA axis counts are not actual core counts");
    std::cout<<"Pass B TMA scope/counts and invisible-core errors passed\n";
}
inline void headless() {palette_and_labels();classifier_scope();tma_scope();}
inline void french(const std::filesystem::path& dir) {
    const auto project=fixture(dir/"display-fr.qpxproj",128,96);project->document(1)->set_calibration(.5,.5);project->document(1)->add_rectangle({8,8,44,60});
    auto o=options(dir,"display-fr","annotations","region",2);o.width=1800;o.height=1200;o.pixel_open=true;
    auto ini=read(std::filesystem::u8path(o.layout_path));const auto at=ini.find("language=en");ini.replace(at,11,"language=fr");write(std::filesystem::u8path(o.layout_path),ini);
    o.inputs={key(3,GuiInputKind::SelectAllKey)};
    const auto report=run_project_gui(project,18,(dir/"pass-b-display-fr-200.ppm").u8string(),o);
    check(report.replay_complete && report.ui_overflow_items==0,"Pass B French 200% overflow: "+report.ui_overflow_label);
    check(read(std::filesystem::u8path(o.layout_path)).find("language=fr")!=std::string::npos,"French display fixture did not retain locale");
    std::cout<<"Pass B French 200% GL display: zero overflow\n";
}
}
