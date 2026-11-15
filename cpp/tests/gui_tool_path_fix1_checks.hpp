#pragma once
#include "gui_tool_path_checks.hpp"
namespace tool_path_fix1_checks {
using namespace qpx;
using A=ui::PathFieldTestAccess;
inline void controls(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    auto survivor=std::make_unique<ui::PathField>(ui::PathSpec{"Survivor",ui::PathRole::NewFile,{".csv"}});
    {
        simplification_tools_checks::NativeUi previous;
        previous.settle([&]{ImGui::Begin("Expired path window");survivor->draw({dir.u8string()});ImGui::End();});
    }
    auto* restored=ImGui::GetCurrentContext();ImGui::SetCurrentContext(nullptr);
    check(!survivor->hit(10,10),"Path field hit after its context was destroyed");ImGui::SetCurrentContext(restored);
    simplification_tools_checks::NativeUi host;GuiOptions options;
    check(!survivor->hit(10,10),"Path field from destroyed context hit in a new context");
    auto forms=std::make_unique<detail::GuiProjectForms>(options);
    auto back=std::make_unique<ui::PathField>(ui::PathSpec{"Behind",ui::PathRole::NewFile,{".csv"}});
    auto front=std::make_unique<ui::PathField>(ui::PathSpec{"Front",ui::PathRole::NewFile,{".csv"}});
    ui::PathContext context{dir.u8string()};bool cover=false;
    auto overlap=[&] {
        for(auto pair:{std::pair{"Behind window",back.get()},std::pair{"Front window",front.get()}}) {
            ImGui::SetNextWindowPos({30,30});ImGui::SetNextWindowSize({500,240});ImGui::Begin(pair.first);pair.second->draw(context);ImGui::End();
        }
        if(cover){ImGui::SetNextWindowPos({30,30});ImGui::SetNextWindowSize({500,240});ImGui::Begin("Cover window");ImGui::TextUnformatted("Cover");ImGui::End();ImGui::BringWindowToDisplayFront(ImGui::FindWindowByName("Cover window"));}
        else ImGui::BringWindowToDisplayFront(ImGui::FindWindowByName("Front window"));
    };
    host.settle(overlap);const auto point=A::center(*front);std::vector<ui::PathField*> fields{back.get(),front.get()};
    check(detail::route_path_drop({{"front.csv"},point.x,point.y},*forms,fields,false),"Front drop not routed");
    check(std::string(front->text())=="front.csv" && !*back->text() && !forms->adding(),"Drop went behind front window");
    cover=true;host.settle(overlap);std::string error;
    detail::route_path_drop({{"hidden.csv"},point.x,point.y},*forms,fields,false,0,false,&error);
    check(std::string(front->text())=="front.csv" && !*back->text(),"Covered field received a drop");
    check(!forms->adding() && error==ui::i18n::text("Open a project to add images."),"Standalone drop queued or hid blocked-action reason");
    cover=false;host.settle(overlap);error.clear();
    detail::route_path_drop({{"standalone.csv"},point.x,point.y},*forms,fields,false,0,false,&error);
    check(std::string(front->text())=="standalone.csv" && error.empty() && !forms->adding(),"Standalone path-field drop stopped working");

    // Unchanged syncs neither refresh fields nor repeat the process lookup.
    front->set_option("missing-refresh.csv");back->set_option("another.csv");
    auto quiet=[&]{front->sync(context);back->sync(context);front->sync(context);back->sync(context);};
    host.settle(quiet);const auto first=A::refreshes(*front),second=A::refreshes(*back),reads=ui::path_process_reads;
    for(int i=0;i<4;++i)host.frame(quiet);
    check(A::refreshes(*front)==first && A::refreshes(*back)==second,"Unchanged frame refreshed a field");
    check(ui::path_process_reads-reads==4,"Process directory was not shared once per frame");

    auto refresh=std::make_unique<ui::PathField>(ui::PathSpec{"Refresh input",ui::PathRole::OpenFile,{".csv"}});
    const auto file=dir/"appeared.csv";refresh->set(file.u8string());
    auto draw=[&]{ImGui::SetNextWindowPos({600,30});ImGui::SetNextWindowSize({500,240});ImGui::Begin("Refresh window");refresh->draw(context);ImGui::End();};
    host.settle(draw);check(!refresh->state().valid,"Refresh fixture already exists");
    {std::ofstream output(file);output<<"new";}
    auto at=A::center(*refresh);ImGui::GetIO().AddMousePosEvent(at.x,at.y);host.settle(draw);
    check(refresh->state().valid && refresh->state().problem.empty(),"Pointer entry left stale missing-file problem");
    const auto stable=A::refreshes(*refresh);host.settle(draw);check(A::refreshes(*refresh)==stable,"Hover performed per-frame filesystem refresh");
    std::filesystem::remove(file);ImGui::GetIO().AddMouseButtonEvent(0,true);host.frame(draw);ImGui::GetIO().AddMouseButtonEvent(0,false);host.frame(draw);
    check(!refresh->state().valid && A::refreshes(*refresh)>stable,"Activation left stale existing-file state");
    forms->select(detail::ProjectForm::Open);forms->open_path.set_option("shortcut.qpxproj");bool requested=false;
    auto collapsed=[&]{ImGui::SetNextWindowCollapsed(true,ImGuiCond_Always);ImGui::Begin("Collapsed Open form");bool losses=false;requested=forms->draw_open(context,false,true,false,nullptr,losses).open;ImGui::End();};
    host.settle(collapsed);check(requested,"Collapsed path row swallowed Ctrl+O/scripted switch request");
    auto pixel=std::make_unique<detail::GuiPixelClassifier>(options);pixel->open();Document doc(Image(8,8,std::vector<std::uint8_t>(192,80)));ObjectSelection selected(doc);
    auto pixel_draw=[&]{pixel->draw(doc,selected,false,true,[](GuiInputTarget){},[](bool){return StainTransform({{1,0,0},{0,1,0}});},{0,0,8,8});for(auto* field:pixel->paths())check(A::draws(*field)<=1,"Actual Pixel window drew a path twice in one frame");};
    host.settle(pixel_draw);
    std::cout<<"Fix1 paths: topmost drop, standalone rejection/field acceptance, unchanged sync0 refresh/process1, hover/activation refresh and collapsed shortcut passed\n";
}
}
