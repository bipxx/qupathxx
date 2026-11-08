#pragma once
#include "gui_design_tokens.hpp"
#include <imgui.h>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace qpx::ui::design {
inline ImVec4 color(std::uint32_t rgb,float alpha=1) {return {((rgb>>16)&255)/255.f,((rgb>>8)&255)/255.f,(rgb&255)/255.f,alpha};}
inline const char* name(Theme theme) {return theme==Theme::Light?"light":theme==Theme::System?"system":"dark";}
inline std::optional<Theme> parse(std::string_view value) {
    if(value=="dark") return Theme::Dark;if(value=="light") return Theme::Light;if(value=="system") return Theme::System;return std::nullopt;
}
inline bool system_light() {
#ifdef _WIN32
    DWORD value=0,size=sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS && value!=0;
#elif defined(__APPLE__)
    const auto value=CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"),kCFPreferencesCurrentApplication);
    const bool is_dark=value && CFGetTypeID(value)==CFStringGetTypeID() && CFStringCompare(static_cast<CFStringRef>(value),CFSTR("Dark"),kCFCompareCaseInsensitive)==kCFCompareEqualTo;
    if(value) CFRelease(value);return !is_dark;
#else
    const char* value=std::getenv("GTK_THEME");if(!value) return false;
    const std::string_view theme(value);return theme.find("light")!=std::string_view::npos || theme.find("Light")!=std::string_view::npos;
#endif
}
struct State {Theme choice=Theme::Dark;bool forced=false;};
inline thread_local State state;
struct Session {
    State previous=state;
    Session(){state={};if(const auto* value=std::getenv("QUPATHXX_UI_THEME"))if(auto theme=parse(value)) {state.choice=*theme;state.forced=true;}}
    ~Session(){state=previous;}
    Session(const Session&)=delete;Session& operator=(const Session&)=delete;
};
inline Theme current() {
    if(state.choice!=Theme::System) return state.choice;
    // Follow OS changes without querying the registry on every rendered item.
    static thread_local auto sampled=std::chrono::steady_clock::time_point{};
    static thread_local bool prefers_light=false;
    const auto now=std::chrono::steady_clock::now();
    if(now-sampled>std::chrono::seconds(1)) {prefers_light=system_light();sampled=now;}
    return resolve(state.choice,prefers_light);
}
inline const Palette& colors(){return palette(current());}
inline void load_preference(std::string_view text) {
    if(state.forced) return;
    while(!text.empty() && text.front()==';') {
        const auto end=text.find('\n');auto line=text.substr(0,end);if(!line.empty() && line.back()=='\r') line.remove_suffix(1);
        constexpr std::string_view prefix="; qupathxx theme=";
        if(line.substr(0,prefix.size())==prefix) if(auto choice=parse(line.substr(prefix.size()))) state.choice=*choice;
        if(end==std::string_view::npos) break;text.remove_prefix(end+1);
    }
}
inline float scale(){return ImGui::GetFontSize()/metrics.font_size;}
inline void apply(ImGuiStyle& s,Theme theme) {
    s=ImGuiStyle{};ImGui::StyleColorsDark(&s);const auto& p=palette(theme);auto* c=s.Colors;
    s.FontSizeBase=metrics.font_size;s.WindowPadding={metrics.panel_inset_x,metrics.panel_inset_y};s.FramePadding={spacing.md,5};
    s.ItemSpacing={spacing.md,spacing.sm};s.ItemInnerSpacing={spacing.md,spacing.sm};
    s.WindowRounding=s.PopupRounding=s.ChildRounding=metrics.panel_radius;s.FrameRounding=s.GrabRounding=metrics.control_radius;
    s.TabRounding=metrics.control_radius;s.ScrollbarRounding=metrics.control_radius;s.ScrollbarSize=12;
    s.WindowBorderSize=s.ChildBorderSize=s.FrameBorderSize=s.PopupBorderSize=metrics.border;
    s.DockingSeparatorSize=spacing.sm;s.TabBorderSize=0;s.SeparatorTextBorderSize=0;s.SeparatorTextPadding={0,spacing.sm};
    s.WindowMenuButtonPosition=ImGuiDir_None;
    s.DisabledAlpha=.5f;
    c[ImGuiCol_Text]=color(p.text_primary);c[ImGuiCol_TextDisabled]=color(p.text_muted);
    c[ImGuiCol_WindowBg]=color(p.bg_secondary);c[ImGuiCol_ChildBg]=color(p.bg_primary);c[ImGuiCol_PopupBg]=color(p.bg_secondary);
    c[ImGuiCol_Border]=color(p.border);c[ImGuiCol_BorderShadow]={0,0,0,0};
    c[ImGuiCol_FrameBg]=color(p.bg_input);c[ImGuiCol_FrameBgHovered]=color(p.bg_hover);c[ImGuiCol_FrameBgActive]=color(p.bg_elevated);
    c[ImGuiCol_TitleBg]=c[ImGuiCol_TitleBgActive]=c[ImGuiCol_TitleBgCollapsed]=color(p.bg_secondary);
    c[ImGuiCol_MenuBarBg]=color(p.bg_primary);
    c[ImGuiCol_Button]=color(p.bg_hover);c[ImGuiCol_ButtonHovered]=color(p.bg_elevated);c[ImGuiCol_ButtonActive]=color(p.border_light);
    c[ImGuiCol_Header]=color(p.bg_tertiary);c[ImGuiCol_HeaderHovered]=color(p.bg_hover);c[ImGuiCol_HeaderActive]=color(p.bg_elevated);
    c[ImGuiCol_Tab]=c[ImGuiCol_TabDimmed]=color(p.bg_secondary);c[ImGuiCol_TabHovered]=color(p.bg_hover);
    c[ImGuiCol_TabSelected]=c[ImGuiCol_TabDimmedSelected]=color(p.bg_tertiary);
    c[ImGuiCol_TabSelectedOverline]=c[ImGuiCol_TabDimmedSelectedOverline]=color(p.accent);
    c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=color(p.accent);c[ImGuiCol_SliderGrabActive]=color(p.accent_hover);
    c[ImGuiCol_Separator]=color(p.border_subtle);c[ImGuiCol_SeparatorHovered]=c[ImGuiCol_SeparatorActive]=color(p.border_light);
    c[ImGuiCol_ScrollbarBg]=color(p.bg_primary);c[ImGuiCol_ScrollbarGrab]=color(p.border_light);
    c[ImGuiCol_ScrollbarGrabHovered]=c[ImGuiCol_ScrollbarGrabActive]=color(p.text_muted);
    c[ImGuiCol_ResizeGrip]=color(p.border_light,.4f);c[ImGuiCol_ResizeGripHovered]=c[ImGuiCol_ResizeGripActive]=color(p.accent);
    c[ImGuiCol_DockingEmptyBg]=color(p.bg_primary);c[ImGuiCol_DockingPreview]=color(p.accent,.25f);
    c[ImGuiCol_TextSelectedBg]=color(p.accent,.25f);c[ImGuiCol_NavCursor]=color(p.accent_hover);
    c[ImGuiCol_TableHeaderBg]=color(p.bg_tertiary);c[ImGuiCol_TableBorderStrong]=color(p.border);c[ImGuiCol_TableBorderLight]=color(p.border_subtle);
    c[ImGuiCol_TableRowBg]={0,0,0,0};c[ImGuiCol_TableRowBgAlt]=color(p.text_primary,.025f);
    c[ImGuiCol_ModalWindowDimBg]=color(p.bg_void,.5f);
}
inline thread_local bool compact_actions=false;
struct CompactActions {
    bool previous=compact_actions;CompactActions(){compact_actions=true;}~CompactActions(){compact_actions=previous;}
};
struct Heading {
    Heading(){auto* atlas=ImGui::GetIO().Fonts;ImGui::PushFont(atlas->Fonts.Size>1?atlas->Fonts[1]:ImGui::GetFont(),ImGui::GetFontSize());}
    ~Heading(){ImGui::PopFont();}
};
struct ActionStyle {
    explicit ActionStyle(Action action) {
        const auto& p=colors();const auto primary=action==Action::Primary,danger=action==Action::Danger,ghost=action==Action::Ghost;
        const auto base=primary?p.accent:danger?p.error:p.bg_hover;
        ImGui::PushStyleColor(ImGuiCol_Button,color(base,ghost?0:1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,color(primary?p.accent_hover:danger?p.error:p.bg_elevated));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,color(primary?p.accent_hover:danger?p.error:p.border_light));
        ImGui::PushStyleColor(ImGuiCol_Text,color(primary?p.bg_void:danger?0xffffff:ghost?p.text_secondary:p.text_primary));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    }
    ~ActionStyle(){ImGui::PopStyleVar();ImGui::PopStyleColor(4);}
};
}
