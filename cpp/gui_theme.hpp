#pragma once
#include <imgui.h>
#include <initializer_list>
#include "gui_i18n.hpp"

namespace qpx::ui {
// Desktop chrome only. Image pixels, class colors and scientific overlays keep
// their own display contracts. GuiScale scales this baseline once per change.
inline void apply_desktop_theme() {
    ImGui::StyleColorsLight();
    auto& s=ImGui::GetStyle();
    s.WindowPadding={10,10};s.FramePadding={7,4};s.ItemSpacing={8,6};
    s.WindowRounding=0;s.ChildRounding=0;s.FrameRounding=2;s.PopupRounding=3;
    s.TabRounding=2;s.ScrollbarRounding=2;s.WindowBorderSize=1;s.FrameBorderSize=1;
    s.DockingSeparatorSize=4;
    auto* c=s.Colors;
    c[ImGuiCol_Text]={.12f,.14f,.16f,1};c[ImGuiCol_TextDisabled]={.38f,.40f,.43f,1};
    c[ImGuiCol_WindowBg]={.96f,.96f,.96f,1};c[ImGuiCol_ChildBg]={.98f,.98f,.98f,1};
    c[ImGuiCol_PopupBg]={.99f,.99f,.99f,1};c[ImGuiCol_Border]={.74f,.76f,.78f,1};
    c[ImGuiCol_FrameBg]={1,1,1,1};c[ImGuiCol_FrameBgHovered]={.90f,.94f,.98f,1};
    c[ImGuiCol_FrameBgActive]={.83f,.90f,.97f,1};
    c[ImGuiCol_TitleBg]={.87f,.88f,.89f,1};c[ImGuiCol_TitleBgActive]={.83f,.86f,.89f,1};
    c[ImGuiCol_MenuBarBg]={.94f,.94f,.94f,1};
    c[ImGuiCol_Button]={.93f,.93f,.93f,1};c[ImGuiCol_ButtonHovered]={.82f,.89f,.96f,1};
    c[ImGuiCol_ButtonActive]={.67f,.80f,.93f,1};
    c[ImGuiCol_Header]={.85f,.90f,.95f,1};c[ImGuiCol_HeaderHovered]={.77f,.86f,.95f,1};
    c[ImGuiCol_HeaderActive]={.66f,.79f,.93f,1};
    c[ImGuiCol_Tab]={.88f,.89f,.90f,1};c[ImGuiCol_TabHovered]={.78f,.86f,.95f,1};
    c[ImGuiCol_TabSelected]={.98f,.98f,.98f,1};
    c[ImGuiCol_TabSelectedOverline]={.20f,.43f,.70f,1};
    c[ImGuiCol_TabDimmed]={.88f,.89f,.90f,1};c[ImGuiCol_TabDimmedSelected]={.96f,.96f,.96f,1};
    c[ImGuiCol_TabDimmedSelectedOverline]=c[ImGuiCol_TabSelectedOverline];
    c[ImGuiCol_CheckMark]={.16f,.37f,.63f,1};c[ImGuiCol_SliderGrab]={.28f,.47f,.68f,1};
    c[ImGuiCol_Separator]={.76f,.78f,.80f,1};
    c[ImGuiCol_DockingEmptyBg]={.85f,.85f,.85f,1};
}

// Vector glyphs stay crisp at each supported DPI without another font asset.
inline bool tool_button(int index,int selected,const char* name,const char* shortcut) {
    const float side=ImGui::GetFrameHeight()+4;
    ImGui::PushID(index);
    if(index==selected) ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
    const bool pressed=ImGui::Button("##tool",{side,side});
    if(index==selected) ImGui::PopStyleColor();
    const auto p=ImGui::GetItemRectMin();const float k=side/32;
    auto at=[&](float x,float y){return ImVec2{p.x+x*k,p.y+y*k};};
    auto* d=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(ImGuiCol_Text);
    const float stroke=1.5f*k;
    if(index==0) {d->AddLine(at(10,6),at(10,25),color,stroke);d->AddLine(at(10,6),at(24,20),color,stroke);d->AddLine(at(10,25),at(15,20),color,stroke);d->AddLine(at(15,20),at(24,20),color,stroke);}
    else if(index==1) d->AddRect(at(7,9),at(25,23),color,0,0,stroke);
    else if(index==2) d->AddEllipse(at(16,16),{9*k,7*k},color,0,0,stroke);
    else if(index==3 || index==4) {ImVec2 points[]={at(7,23),at(11,8),at(25,12),at(22,24)};d->AddPolyline(points,4,color,stroke,index==3?ImDrawFlags_Closed:0);}
    else if(index==5) {for(auto q:{at(10,11),at(23,14),at(14,24)}) {d->AddLine({q.x-3*k,q.y},{q.x+3*k,q.y},color,stroke);d->AddLine({q.x,q.y-3*k},{q.x,q.y+3*k},color,stroke);}}
    else if(index==6) {d->AddRect(at(8,8),at(24,24),color,0,0,stroke);for(auto q:{at(8,8),at(24,8),at(8,24),at(24,24)}) d->AddRectFilled({q.x-2*k,q.y-2*k},{q.x+2*k,q.y+2*k},color);}
    else {d->AddCircle(at(16,16),9*k,color,0,stroke);d->AddCircle(at(16,16),4*k,color,0,stroke);}
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s (%s)",i18n::text(name),shortcut);
    ImGui::PopID();return pressed;
}
}
