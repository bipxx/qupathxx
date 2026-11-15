#pragma once
#include "gui_model_safety_checks.hpp"
#include "../gui_class_editor.hpp"
#include "../gui_project_density.hpp"
namespace contract_enforcement_checks {
using namespace qpx;
inline void combo_scope_contracts(const std::filesystem::path& out) {
    std::filesystem::create_directories(out);
    check(glfwInit(),"contract capture GLFW init");
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Contract controls",nullptr,nullptr);check(window,"contract capture window");glfwMakeContextCurrent(window);
    {
        model_safety_checks::GlHost host(window);
        // Exercise every converted caption with real pointer input. The native
        // workflow matrix separately exercises their owning panels and jobs.
        const char* labels[]={"##Task","##Project switcher","Batch class","Class","Parent ID","Tissue level","Cell level","Kept normalization###Saved normalization","Expression source","Project stream links","Prediction area","Expression source","Image"};
        for(int locale:{0,2})for(std::size_t i=0;i<std::size(labels);++i) {
            ui::i18n::choose(locale);ImGui::GetStyle().FontSizeBase=locale==2?32:16;
            Rect combo_bounds{},choice_bounds{};bool opened=false,selected=false;int closed=0;
            auto draw=[&]{
                ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({1400,1300});ImGui::Begin("Contract controls");
                auto* parent=ImGui::GetCurrentWindow();auto* context=ImGui::GetCurrentContext();
                const auto ids=parent->IDStack.Size,windows=context->CurrentWindowStack.Size,popups=context->BeginPopupStack.Size;
                ui::SetNextItemWidth(1000);
                {
                    ui::ComboScope combo(labels[i],selected?"Selected":"Choice");
                    if(combo) {
                        opened=true;
                        check(context->BeginPopupStack.Size==popups+1,"open combo has no popup scope");
                        if(ImGui::Selectable(ui::data_label("Class 50%###literal","choice").c_str(),selected))selected=true;
                        const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();choice_bounds={a.x,a.y,b.x-a.x,b.y-a.y};
                    } else ++closed;
                }
                check(ImGui::GetCurrentWindow()==parent && parent->IDStack.Size==ids && context->CurrentWindowStack.Size==windows && context->BeginPopupStack.Size==popups,"ComboScope did not restore open/closed stacks");
                const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();combo_bounds={a.x,a.y,b.x-a.x,b.y-a.y};
                bool value=false;
                const auto data=ui::data_label("Save project 50%###literal","class-control");
                check(data=="Save project 50%###literal###class-control","data label translated or formatted");
                ImGui::Checkbox(data.c_str(),&value);
                const auto id=context->LastItemData.ID;
                check(id==parent->GetID("###class-control") && id==parent->GetID(ui::data_label("Different %s","class-control").c_str()),"data text changed the control identity");
                check(ui::label_end(data.c_str())==data.c_str()+16,"data ## drawing boundary changed");
                ImGui::End();
            };
            host.settle(draw);check(closed>0 && !opened,"closed combo unexpectedly opened");
            host.click(combo_bounds,draw);check(opened && choice_bounds.width>0,"converted combo did not open");
            if(i<2)host.capture(out/("t63-combo-"+std::to_string(i)+(locale==2?"-fr200.ppm":"-normal.ppm")),draw);
            host.click(choice_bounds,draw);check(selected,"converted combo did not select");
        }
        ui::i18n::choose(0);ImGui::GetStyle().FontSizeBase=16;
        auto document=std::make_shared<Document>(Image::demo());
        document->set_class_definition(ObjectClass::definition("Class 50%###literal",std::nullopt));
        auto project=std::make_shared<Project>();project->add_document(document);project->save((out/"labels.qpxproj").u8string());
        auto catalog=project->images();catalog.front().name="Image 50%###literal";
        detail::GuiProjectDensity density;density.open();
        auto density_draw=[&]{density.draw(project.get(),catalog,false);};
        host.capture(out/"t63-data-image.ppm",density_draw);
        detail::ClassEditor editor;Rect registry;
        auto class_draw=[&]{
            ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({1100,1200});ImGui::Begin("Class data label");
            auto* parent=ImGui::GetCurrentWindow();const auto ids=parent->IDStack.Size;
            const auto windows=host.context->CurrentWindowStack.Size,popups=host.context->BeginPopupStack.Size;
            const auto at=ImGui::GetCursorScreenPos();registry={at.x,at.y,ImGui::CalcItemWidth(),ImGui::GetFrameHeight()};
            editor.draw(project,document.get(),1);
            check(ImGui::GetCurrentWindow()==parent && parent->IDStack.Size==ids && host.context->CurrentWindowStack.Size==windows && host.context->BeginPopupStack.Size==popups,"Raw class Registry combo did not restore stacks");
            ImGui::End();
        };
        host.settle(class_draw);host.click(registry,class_draw);host.capture(out/"t63-data-class.ppm",class_draw);
        // The two unchanged raw class combos also had a source pair-count
        // guard. Test their real scopes before retiring that old assertion.
        ImGui::ClosePopupsOverWindow(nullptr,false);
        detail::ClassEditor palette(true);bool default_open=false;int default_closed=0;
        auto palette_draw=[&]{
            ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({1100,1500});ImGui::Begin("Class palette scopes");
            auto* parent=ImGui::GetCurrentWindow();const auto ids=parent->IDStack.Size;
            const auto windows=host.context->CurrentWindowStack.Size,popups=host.context->BeginPopupStack.Size;
            palette.draw(project,document.get(),1);
            const auto id=ImHashStr("Default class",0,ImHashStr("Project class editor",0,parent->ID));
            if(ImGui::IsPopupOpen(ImHashStr("##ComboPopup",0,id),ImGuiPopupFlags_None))default_open=true;else ++default_closed;
            check(ImGui::GetCurrentWindow()==parent && parent->IDStack.Size==ids && host.context->CurrentWindowStack.Size==windows && host.context->BeginPopupStack.Size==popups,"Raw Default class combo did not restore stacks");
            ImGui::End();
        };
        host.settle(palette_draw);
        auto* parent=ImGui::FindWindowByName("Class palette scopes");
        const auto id=ImHashStr("Default class",0,ImHashStr("Project class editor",0,parent->ID));
        ImGui::ActivateItemByID(id);host.frame(palette_draw);host.settle(palette_draw);
        check(default_closed>0 && default_open,"Raw Default class combo was not checked both closed and open");
    }
    glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"ComboScope: 13 converted captions open/select in English and French 200%; open/closed stacks and literal ###/% identities pass\n";
}
}
