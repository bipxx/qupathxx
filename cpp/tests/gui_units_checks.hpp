#pragma once
#include "gui_model_safety_checks.hpp"
#include "../gui_calibration_editor.hpp"
namespace units_checks {
using namespace qpx;
inline void glyphs(const std::filesystem::path& out,GLFWwindow* window) {
    const auto old=ui::i18n::state;
    std::size_t frames=0;
    // Text sizes are continuous; exercise every half-pixel base size throughout
    // the supported logical range, including the user and DPI extrema.
    for(int locale=0;locale<ui::i18n::language_count;++locale)for(int face=0;face<2;++face) {
      for(int group=16;group<=192;group+=16) {
        // A fresh atlas bounds each size sweep and avoids conflating unrelated
        // historical sizes with the glyph support of the current text face.
        model_safety_checks::GlHost host(window);
        ui::i18n::choose(locale);
        for(int half=group;half<=std::min(group+15,192);++half) {
            const float size=half*.5f;
            std::function<void()> draw=[&]{
                ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({2100,1400});
                ImGui::Begin("Composed text faces");ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[face],size);
                ui::Text("%s",ui::i18n::languages[locale].native_name);
                ui::Text("Pixel width (%s)",ui::unit::um);ui::Text("Minimum area (%s)",ui::unit::um2);
                auto* baked=ImGui::GetFontBaked();
                for(const ImWchar code:{ImWchar(0xB5),ImWchar(0xB2)}) {
                    const auto* glyph=baked->FindGlyphNoFallback(code);
                    check(glyph && glyph->Codepoint==code && glyph->Visible && glyph->AdvanceX>0,"STOP: composed text face uses a fallback unit glyph");
                    check(glyph->U1>glyph->U0 && glyph->V1>glyph->V0,"STOP: unit glyph has no rasterized image");
                    bool drawn=false;
                    for(const auto& vertex:ImGui::GetWindowDrawList()->VtxBuffer)
                        if(vertex.uv.x==glyph->U0 && vertex.uv.y==glyph->V0)drawn=true;
                    check(drawn,"STOP: real unit glyph was not submitted on the rendered frame");
                }
                ImGui::PopFont();ImGui::End();
            };
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();host.frame(draw);
            glViewport(0,0,2200,1600);glClearColor(.04f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if(half==32 || half==64)dialog_checks::capture(out/("t62-glyph-"+std::string(ui::i18n::languages[locale].code)+"-face"+std::to_string(face)+"-size"+std::to_string(int(size))+".ppm"),window);
            glfwSwapBuffers(window);++frames;
        }
      }
    }
    ui::i18n::state=old;
    std::cout<<"Composed units: "<<frames<<" OpenGL rendered frames, all 16 languages, both merged text faces, half-pixel sizes 8..96; exact glyph codepoints/UVs, no fallback\n";
}
inline void calibration(const std::filesystem::path& out,GLFWwindow* window) {
    for(int locale:{0,2}) {
        model_safety_checks::GlHost host(window);ui::i18n::choose(locale);
        if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        Document doc(Image::demo());doc.set_calibration(.375,.5);detail::CalibrationEditor editor;editor.open(doc,1,1);
        std::function<void()> draw=[&]{ImGui::Begin("Calibration host");editor.draw(&doc,1,1);ImGui::End();};
        host.capture(out/(locale==2?"t62-calibration-fr200.ppm":"t62-calibration-normal.ppm"),draw);
    }
}
inline void tools(const std::filesystem::path& out,GLFWwindow* window) {
    for(int locale:{0,2}) {
        model_safety_checks::GlHost host(window);ui::i18n::choose(locale);
        if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        auto doc=std::make_shared<Document>(Image::demo());doc->set_calibration(.375,.5);
        Project project;project.add_document(doc);ObjectSelection selection(*doc);auto mark=[](GuiInputTarget){};
        GuiOptions initial;initial.tma_open=true;initial.tma_tab=0;
        auto dearray=std::make_shared<TmaDearrayOptions>();dearray->core_units=TmaCoreUnits::Micrometers;dearray->core_width=dearray->core_height=100;initial.tma_dearray_options=dearray;
        detail::GuiTma tma(initial);
        auto spatial_options=std::make_shared<GuiSpatialOptions>();spatial_options->open=true;spatial_options->operation=SpatialOperation::TissueDensity;initial.spatial=spatial_options;
        detail::GuiSpatial spatial(initial);
        auto batch_options=std::make_shared<ProjectBatchOptions>();batch_options->kind=ProjectBatchKind::Tissue;batch_options->tissue_min_component_area_um2=.5;initial.batch_options=batch_options;
        detail::GuiBatch batch(initial);batch.open();batch_recipe_checks::Values values;
        std::vector<std::pair<std::string,std::function<void()>>> draws{
            {"tma",[&]{tma.draw(*doc,selection,&project,false,true,mark);}},
            {"spatial",[&]{spatial.draw(*doc,selection,&project,nullptr,false,true,mark);}},
            {"batch",[&]{batch.draw(&project,1,&selection,false,values.snapshot(),[&](bool stains){return values.transform(stains);});}}
        };
        for(auto& item:draws) {
            auto& draw=item.second;host.settle(draw);
            for(auto* win:host.context->Windows)if(win->Active && !win->ParentWindow){ImGui::SetWindowPos(win,{20,20});ImGui::SetWindowSize(win,{1550,1500});}
            host.capture(out/("t62-"+item.first+(locale==2?"-fr200.ppm":"-normal.ppm")),draw);
            // Capture the lower controls too when the natural body scrolls.
            for(auto* win:host.context->Windows)if(win->Active && !win->ParentWindow)ImGui::SetScrollY(win,win->ScrollMax.y);
            host.capture(out/("t62-"+item.first+(locale==2?"-fr200-lower.ppm":"-normal-lower.ppm")),draw);
        }
    }
}
inline void view(const std::filesystem::path& out) {
    for(int locale:{0,2}) {
        const auto stem=std::string("t62-view")+(locale==2?"-fr200":"-normal");
        auto project=fixture(out/(stem+".qpxproj"),128,96);auto doc=project->document(1);doc->set_calibration(.375,.5);
        auto initial=::options(out,stem,"image","region",locale==2?2:1);initial.width=1800;initial.height=1400;
        if(locale==2){auto ini=read(initial.layout_path);ini.replace(ini.find("language=en"),11,"language=fr");write(initial.layout_path,ini);}
        const auto report=run_project_gui(project,22,(out/(stem+".ppm")).u8string(),initial);
        check(report.ui_overflow_items==0,"Units image/status capture overflow");
    }
}
inline void guidance(const std::filesystem::path& out) {
    std::filesystem::create_directories(out);
    for(int locale:{0,2}) {
        auto project=refusal_checks::live_project(out,"t62-guidance-"+std::to_string(locale));
        auto initial=::options(out,"t62-guidance-"+std::to_string(locale),"workflow","cells");initial.width=1800;initial.height=1400;initial.cell_table_published_objects=true;initial.inputs={key(2,GuiInputKind::SelectAllKey)};
        if(locale==2){auto ini=read(initial.layout_path);ini.replace(ini.find("language=en"),11,"language=fr");write(initial.layout_path,ini);}
        dispatch_checks::TraceScope observed;int stage=0,source_value=-1,reported_stage=-1;ImGuiID source_id=0;bool done=false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
        struct Restore {decltype(ui::combo_observer) old=ui::combo_observer;~Restore(){ui::combo_observer=old;}} restore;
        ui::combo_observer=[&](const char* label,int current,int){if(std::string_view(label)=="Cell source"){source_value=current;source_id=ui::field_input_id(label);}};
        observed.trace.after_frame=[&](const auto&,const auto&) {
            if(reported_stage!=stage){std::cout<<"Guidance stage "<<stage<<" task "<<observed.trace.task<<" source "<<source_value<<'\n';reported_stage=stage;}
            check(std::chrono::steady_clock::now()<deadline,"Cross-task guidance did not complete at stage "+std::to_string(stage));
            auto* body=dispatch_checks::task_body();if(!body)return;
            auto open_task=[&]{auto* parent=body->ParentWindow;check(parent,"Task body has no navigation owner");ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("##Task",0,parent->ID)));};
            auto* popup=ImGui::FindWindowByName("##Combo_00");
            if(stage==0) {
                const auto* action=simplification_checks::action(observed.workflow.trace,"Export cell table (Ctrl+Alt+X)");
                if(!action || !action->disabled || action->reason!=ui::i18n::text("In Tables, choose kept runs in Cell source."))return;
                open_task();stage=1;return;
            }
            if(stage==1 && popup && popup->Active){ImGui::ActivateItemByID(popup->GetID(ui::i18n::label("Tables").c_str()));stage=2;return;}
            if(stage==2 && observed.trace.task==int(ui::WorkflowTask::Tables) && source_id){check(source_value==1,"Tables did not show the refused live source");ImGui::ActivateItemByID(source_id);stage=3;return;}
            if(stage==3 && popup && popup->Active){const int index=0;const auto seed=ImHashData(&index,sizeof(index),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text("Kept project runs"),0,seed));stage=4;return;}
            if(stage==4 && source_value==0){open_task();stage=5;return;}
            if(stage==5 && popup && popup->Active){ImGui::ActivateItemByID(popup->GetID(ui::i18n::label("Watershed cells").c_str()));stage=6;return;}
            if(stage==6 && observed.trace.task==int(ui::WorkflowTask::Cells)) {
                const auto* action=simplification_checks::action(observed.workflow.trace,"Export cell table (Ctrl+Alt+X)");
                check(action && action->enabled && action->reason.empty(),"Following the Tables guidance did not enable Cells export");done=true;observed.trace.end_session=true;
            }
        };
        const auto report=run_project_gui(project,100000,"",initial);check(done && report.replay_complete,"Cross-task guidance replay incomplete");
    }
    std::cout<<"Cells refusal -> Tables navigation -> actual Cell source choice -> Cells export enabled, English/French passed\n";
    simplification_tools_checks::NativeUi host;
    for(int locale=0;locale<ui::i18n::language_count;++locale) {
        ui::i18n::choose(locale);ProjectBatchOptions batch;batch.kind=ProjectBatchKind::Tissue;batch.tissue.level=-1;batch.tissue.max_pixels=0;batch.tissue.threshold=.3;
        std::function<void()> draw=[&]{ImGui::Begin("Batch summary words");detail::GuiBatch::draw_summary(batch);ImGui::End();};
        host.settle(draw);const auto text=model_safety_checks::logged(host,draw);
        check(text.find(ui::i18n::text("auto"))!=std::string::npos && text.find(ui::i18n::text("all"))!=std::string::npos && text.find(">= 0.3")!=std::string::npos,"Batch summary lost user words for sentinels or comparison");
        check(text.find("-1")==std::string::npos && text.find("0=all")==std::string::npos,"Batch summary exposed sentinel numbers");
        batch.tissue.level=-2;const auto invalid=model_safety_checks::logged(host,draw);
        check(invalid.find("-2")!=std::string::npos && invalid.find(ui::i18n::text("auto"))==std::string::npos,"Invalid negative level was presented as automatic");
    }
    std::cout<<"Batch summary: auto/all/comparison and Feature in all 16 languages passed\n";
}
inline void run(const std::filesystem::path& out) {
    std::filesystem::create_directories(out);check(glfwInit(),"Units GLFW initialization");
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(2200,1600,"Units checks",nullptr,nullptr);
    check(window,"Units GL window");glfwMakeContextCurrent(window);
    glyphs(out,window);calibration(out,window);tools(out,window);glfwDestroyWindow(window);glfwTerminate();view(out);
}
}
