#pragma once
#include "gui_dialogs.hpp"
#include "gui_help.hpp"
#include "qupathxx/core.hpp"
#include <array>
#include <functional>
#include <map>

namespace qpx::ui {
enum class HeaderCommand {OpenProject,AddImages,SaveProject,SaveProjectAs,Close,Undo,Redo,
    CopyObjects,PasteObjects,FitImage,ShowObjects,ProjectPanel,ImagePanel,AnnotationsPanel,
    WorkflowPanel,ResetLayout,ReloadUi,IncreaseUiSize,DecreaseUiSize,AutomaticUiSize,
    ThemeDark,ThemeLight,ThemeSystem,ChooseLanguage,QuickReference,Documentation,About};
struct HeaderSnapshot {
    bool editing_blocked=false,image=false,project=false,project_dirty=false,save_busy=false;
    bool can_undo=false,can_redo=false,can_copy=false,can_paste=false,objects_visible=true;
    bool overlay_gesture=false,workspace=false,reload_ui=false;
    double ui_size=1;
    design::Theme theme=design::Theme::System;
};
struct HeaderTrace {
    std::array<Rect,5> menus{};
    bool compact=false;
    std::map<HeaderCommand,Rect> commands;
    std::map<std::string,Rect> groups;
    std::map<std::string,ImGuiID> group_ids;
};
using HeaderDispatch=std::function<void(HeaderCommand)>;
inline void draw_header_settings(const HeaderSnapshot&,const HeaderDispatch&,HeaderTrace* trace=nullptr);
namespace header_detail {
inline Rect bounds() {const auto p=ImGui::GetItemRectMin(),s=ImGui::GetItemRectSize();return {p.x,p.y,s.x,s.y};}
inline bool menu(const char* label,bool enabled,HeaderTrace* trace) {
    const bool opened=BeginMenu(label,enabled);
    if(trace) {trace->groups[label]=bounds();trace->group_ids[label]=ImGui::GetItemID();}
    return opened;
}
inline void item(const char* label,HeaderCommand command,const HeaderDispatch& dispatch,
    HeaderTrace* trace,const char* shortcut=nullptr,bool selected=false,bool enabled=true) {
    const bool pressed=MenuItem(label,shortcut,selected,enabled);
    if(trace)trace->commands[command]=bounds();if(pressed)dispatch(command);
}
inline float menu_width(const char* caption) {
    return ImGui::CalcTextSize(i18n::text(caption)).x+2*ImGui::GetStyle().ItemSpacing.x;
}
inline void file(const HeaderSnapshot& s,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    item("Open project...",HeaderCommand::OpenProject,dispatch,trace);
    item("Add images...",HeaderCommand::AddImages,dispatch,trace);
    item("Save project",HeaderCommand::SaveProject,dispatch,trace,"Ctrl+S",false,s.project && s.project_dirty && !s.save_busy);
    item("Save project as...",HeaderCommand::SaveProjectAs,dispatch,trace,nullptr,false,s.project && !s.save_busy);
    ImGui::Separator();item("Close",HeaderCommand::Close,dispatch,trace);
}
inline void edit(const HeaderSnapshot& s,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    item("Undo",HeaderCommand::Undo,dispatch,trace,"Ctrl+Z",false,s.image && s.can_undo);
    item("Redo",HeaderCommand::Redo,dispatch,trace,"Ctrl+Y",false,s.image && s.can_redo);
    ImGui::Separator();
    item("Copy objects",HeaderCommand::CopyObjects,dispatch,trace,"Ctrl+C",false,s.can_copy);
    item("Paste objects",HeaderCommand::PasteObjects,dispatch,trace,"Ctrl+V",false,s.can_paste);
    // The non-docked API viewer has no activity-rail gear. Keep its existing
    // preferences reachable through the same settings owner.
    if(!s.workspace) {
        ImGui::Separator();
        if(menu("Workspace settings",true,trace)) {draw_header_settings(s,dispatch,trace);ImGui::EndMenu();}
    }
}
inline void view(const HeaderSnapshot& s,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    item("Fit image",HeaderCommand::FitImage,dispatch,trace,nullptr,false,s.image);
    item("Show objects",HeaderCommand::ShowObjects,dispatch,trace,"O",s.objects_visible,s.image && !s.overlay_gesture);
    ImGui::Separator();
    if(menu("Panels",s.workspace,trace)) {
        item("Project",HeaderCommand::ProjectPanel,dispatch,trace);
        item("Image / Brightness and contrast",HeaderCommand::ImagePanel,dispatch,trace);
        item("Annotations",HeaderCommand::AnnotationsPanel,dispatch,trace);
        item("Workflow / Measurements",HeaderCommand::WorkflowPanel,dispatch,trace);
        ImGui::EndMenu();
    }
    if(menu("Layout",s.workspace,trace)) {
        item("Reset layout",HeaderCommand::ResetLayout,dispatch,trace);ImGui::EndMenu();
    }
}
}
inline void draw_header(const HeaderSnapshot& snapshot,const HeaderDispatch& dispatch,
    const std::function<void()>& draw_analyze,HeaderTrace* trace=nullptr) {
    if(trace)*trace={};
    if(!ImGui::BeginMenuBar())return;
    constexpr std::array captions={"File","Edit","View","Analyze"};
    const auto start=ImGui::GetCursorScreenPos();const float available=ImGui::GetContentRegionAvail().x;
    const float help_width=header_detail::menu_width("Help");
    float needed=help_width+ImGui::GetStyle().ItemSpacing.x;
    for(const auto* caption:captions)needed+=header_detail::menu_width(caption);
    const bool compact=needed>available;if(trace)trace->compact=compact;
    ImGui::BeginDisabled(snapshot.editing_blocked);
    const bool left_open=!compact || header_detail::menu("Menu",true,trace);
    if(compact && trace)trace->menus[0]=header_detail::bounds();
    if(left_open)for(std::size_t i=0;i<captions.size();++i) {
        const bool opened=header_detail::menu(captions[i],true,trace);if(trace && !compact)trace->menus[i]=header_detail::bounds();
        if(!opened)continue;
        if(i==0)header_detail::file(snapshot,dispatch,trace);
        else if(i==1)header_detail::edit(snapshot,dispatch,trace);
        else if(i==2)header_detail::view(snapshot,dispatch,trace);
        else draw_analyze();
        ImGui::EndMenu();
    }
    if(compact && left_open)ImGui::EndMenu();
    ImGui::EndDisabled();
    const float help_x=std::max(ImGui::GetCursorScreenPos().x,start.x+available-help_width);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+help_x-ImGui::GetCursorScreenPos().x);
    const bool help=header_detail::menu("Help",true,trace);if(trace)trace->menus[4]=header_detail::bounds();
    if(help) {
        header_detail::item("Quick reference",HeaderCommand::QuickReference,dispatch,trace,"F1");
        header_detail::item("Documentation",HeaderCommand::Documentation,dispatch,trace);
        ImGui::Separator();header_detail::item("About qupathxx",HeaderCommand::About,dispatch,trace);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}
inline void draw_header_settings(const HeaderSnapshot& snapshot,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    if(header_detail::menu("General",true,trace)) {
        header_detail::item("Choose language",HeaderCommand::ChooseLanguage,dispatch,trace,"Ctrl+Shift+L");
        ImGui::Separator();ImGui::TextUnformatted(i18n::languages[i18n::state.effective].native_name);ImGui::EndMenu();
    }
    if(header_detail::menu("Appearance",true,trace)) {
        header_detail::item("Dark",HeaderCommand::ThemeDark,dispatch,trace,nullptr,snapshot.theme==design::Theme::Dark);
        header_detail::item("Light",HeaderCommand::ThemeLight,dispatch,trace,nullptr,snapshot.theme==design::Theme::Light);
        header_detail::item("System",HeaderCommand::ThemeSystem,dispatch,trace,nullptr,snapshot.theme==design::Theme::System);
        ImGui::Separator();
        header_detail::item("Increase UI size",HeaderCommand::IncreaseUiSize,dispatch,trace,nullptr,false,snapshot.ui_size<3);
        header_detail::item("Decrease UI size",HeaderCommand::DecreaseUiSize,dispatch,trace,nullptr,false,snapshot.ui_size>.5);
        header_detail::item("Automatic UI size",HeaderCommand::AutomaticUiSize,dispatch,trace);ImGui::EndMenu();
    }
    if(snapshot.reload_ui && header_detail::menu("Advanced",true,trace)) {
        header_detail::item("Reload UI resources",HeaderCommand::ReloadUi,dispatch,trace,"Ctrl+Shift+R");ImGui::EndMenu();
    }
}
struct HeaderShortcuts {bool language=false,help=false;};
inline HeaderShortcuts header_shortcuts(bool blocked) {
    if(blocked)return {};
    return {ImGui::Shortcut(ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_L,ImGuiInputFlags_RouteGlobal),
        ImGui::Shortcut(ImGuiKey_F1,ImGuiInputFlags_RouteGlobal)};
}
}

namespace qpx::detail {
class GuiHelpWindow {
    HelpPage page_=HelpPage::QuickReference;
    bool requested_=false;
    std::string link_error_;
public:
    void request(HelpPage page) {page_=page;requested_=true;link_error_.clear();}
    HelpPage page() const {return page_;}
    bool is_open() const {return ui::IsDialogOpen(ui::DialogKind::Help);}
    void draw(const BuildEnvironment& environment) {
        if(requested_) {ui::OpenDialog(ui::DialogKind::Help);requested_=false;}
        ui::DialogState state(true,false);
        constexpr std::array captions={"Quick reference","Documentation","About qupathxx"};
        ui::RenderDialog(ui::DialogKind::Help,state,ui::i18n::text(captions.at(static_cast<std::size_t>(page_))),{&link_error_},
            [&](ui::DialogControls&) {render_help_page(page_,environment,link_error_);},
            [](ui::DialogAction) {return ui::DialogOutcome::Success;});
    }
};
}
