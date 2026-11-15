#pragma once
#include "gui_dialogs.hpp"
#include "gui_commands.hpp"
#include "gui_desktop_context.hpp"
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
inline constexpr std::array header_commands={CommandId::Header_OpenProject,CommandId::Header_AddImages,CommandId::Header_SaveProject,CommandId::Header_SaveProjectAs,CommandId::Header_Close,CommandId::Header_Undo,CommandId::Header_Redo,CommandId::Header_CopyObjects,CommandId::Header_PasteObjects,CommandId::Header_FitImage,CommandId::Header_ShowObjects,CommandId::Header_ProjectPanel,CommandId::Header_ImagePanel,CommandId::Header_AnnotationsPanel,CommandId::Header_WorkflowPanel,CommandId::Header_ResetLayout,CommandId::Header_ReloadUi,CommandId::Header_IncreaseUiSize,CommandId::Header_DecreaseUiSize,CommandId::Header_AutomaticUiSize,CommandId::Header_ThemeDark,CommandId::Header_ThemeLight,CommandId::Header_ThemeSystem,CommandId::Header_ChooseLanguage,CommandId::Header_QuickReference,CommandId::Header_Documentation,CommandId::Header_About};
static_assert(header_commands.size()==static_cast<std::size_t>(HeaderCommand::About)+1);
inline constexpr CommandId command_id(HeaderCommand id) {return header_commands.at(static_cast<std::size_t>(id));}
struct HeaderSnapshot {
    bool editing_blocked=false,image=false,project=false,project_dirty=false,save_busy=false;
    bool can_undo=false,can_redo=false,can_copy=false,can_paste=false,objects_visible=true;
    bool overlay_gesture=false,workspace=false,reload_ui=false,desktop=false;
    double ui_size=1;
    design::Theme theme=design::Theme::System;
};
struct HeaderTrace {
    std::array<Rect,5> menus{};
    bool compact=false;
    std::map<HeaderCommand,Rect> commands;
    std::map<std::string,Rect> groups;
    std::map<std::string,ImGuiID> group_ids;
    std::map<HeaderCommand,std::pair<std::string,std::string>> shown;
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
inline void item(HeaderCommand command,const HeaderDispatch& dispatch,
    HeaderTrace* trace,bool selected=false,bool enabled=true,bool desktop_exit=false) {
    const auto id=desktop_exit?CommandId::DesktopExit:command_id(command);
    const auto* caption=command_table.at(static_cast<std::size_t>(id)).caption;
    const bool pressed=MenuItem(desktop_exit?command_label(id):caption,command_chord(id),selected,enabled);
    if(trace) {trace->commands[command]=bounds();trace->shown[command]={i18n::text(caption),command_chord(id)};}if(pressed)dispatch(command);
}
inline float menu_width(const char* caption) {
    return ImGui::CalcTextSize(i18n::text(caption)).x+2*ImGui::GetStyle().ItemSpacing.x;
}
inline void file(const HeaderSnapshot& s,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    item(HeaderCommand::OpenProject,dispatch,trace);
    item(HeaderCommand::AddImages,dispatch,trace);
    item(HeaderCommand::SaveProject,dispatch,trace,false,s.project && s.project_dirty && !s.save_busy);
    item(HeaderCommand::SaveProjectAs,dispatch,trace,false,s.project && !s.save_busy);
    ui::Separator();item(HeaderCommand::Close,dispatch,trace,false,true,s.desktop);
}
inline void edit(const HeaderSnapshot& s,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    item(HeaderCommand::Undo,dispatch,trace,false,s.image && s.can_undo);
    item(HeaderCommand::Redo,dispatch,trace,false,s.image && s.can_redo);
    ui::Separator();
    item(HeaderCommand::CopyObjects,dispatch,trace,false,s.can_copy);
    item(HeaderCommand::PasteObjects,dispatch,trace,false,s.can_paste);
    // The non-docked API viewer has no activity-rail gear. Keep its existing
    // preferences reachable through the same settings owner.
    if(!s.workspace) {
        ui::Separator();
        if(menu("Workspace settings",true,trace)) {draw_header_settings(s,dispatch,trace);ImGui::EndMenu();}
    }
}
inline void view(const HeaderSnapshot& s,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    item(HeaderCommand::FitImage,dispatch,trace,false,s.image);
    item(HeaderCommand::ShowObjects,dispatch,trace,s.objects_visible,s.image && !s.overlay_gesture);
    ui::Separator();
    if(menu("Panels",s.workspace,trace)) {
        item(HeaderCommand::ProjectPanel,dispatch,trace);
        item(HeaderCommand::ImagePanel,dispatch,trace);
        item(HeaderCommand::AnnotationsPanel,dispatch,trace);
        item(HeaderCommand::WorkflowPanel,dispatch,trace);
        ImGui::EndMenu();
    }
    if(menu("Layout",s.workspace,trace)) {
        item(HeaderCommand::ResetLayout,dispatch,trace);ImGui::EndMenu();
    }
}
}
inline void draw_header(const HeaderSnapshot& snapshot,const HeaderDispatch& dispatch,
    const std::function<void()>& draw_analyze,HeaderTrace* trace=nullptr) {
    ui::i18n::SurfaceTrace translation_surface("header menus");
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
        header_detail::item(HeaderCommand::QuickReference,dispatch,trace);
        header_detail::item(HeaderCommand::Documentation,dispatch,trace);
        ui::Separator();header_detail::item(HeaderCommand::About,dispatch,trace);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}
inline void draw_header_settings(const HeaderSnapshot& snapshot,const HeaderDispatch& dispatch,HeaderTrace* trace) {
    ui::i18n::SurfaceTrace translation_surface("header menus");
    if(header_detail::menu("General",true,trace)) {
        header_detail::item(HeaderCommand::ChooseLanguage,dispatch,trace);
        ui::Separator();ui::DataText(i18n::languages[i18n::state.effective].native_name);ImGui::EndMenu();
    }
    if(header_detail::menu("Appearance",true,trace)) {
        header_detail::item(HeaderCommand::ThemeDark,dispatch,trace,snapshot.theme==design::Theme::Dark);
        header_detail::item(HeaderCommand::ThemeLight,dispatch,trace,snapshot.theme==design::Theme::Light);
        header_detail::item(HeaderCommand::ThemeSystem,dispatch,trace,snapshot.theme==design::Theme::System);
        ui::Separator();
        header_detail::item(HeaderCommand::IncreaseUiSize,dispatch,trace,false,snapshot.ui_size<3);
        header_detail::item(HeaderCommand::DecreaseUiSize,dispatch,trace,false,snapshot.ui_size>.5);
        header_detail::item(HeaderCommand::AutomaticUiSize,dispatch,trace);ImGui::EndMenu();
    }
    if(snapshot.reload_ui && header_detail::menu("Advanced",true,trace)) {
        header_detail::item(HeaderCommand::ReloadUi,dispatch,trace);ImGui::EndMenu();
    }
}
struct HeaderShortcuts {bool language=false,help=false;};
inline HeaderShortcuts header_shortcuts(bool blocked) {
    if(blocked)return {};
    return {ui::chord_pressed(ui::CommandId::Header_ChooseLanguage,ui::frame_commands()),
        ui::chord_pressed(ui::CommandId::Header_QuickReference,ui::frame_commands())};
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
