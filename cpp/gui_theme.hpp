#pragma once
#include <imgui.h>
#include <initializer_list>
#include "gui_i18n.hpp"
#include "gui_design_system.hpp"
#include "gui_layout.hpp"

namespace qpx::ui {
// Desktop chrome only. Image pixels, class colors and scientific overlays keep
// their own display contracts. GuiScale scales this baseline once per change.
inline void apply_desktop_theme() {design::apply(ImGui::GetStyle(),design::current());}

enum class ActionIcon {Fit,Undo,Redo,Spatial,Reset};
inline bool icon_action(const char* id,ActionIcon icon,const char* caption,const char* shortcut="") {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    design::CompactActions compact;const float side=design::metrics.icon_size*design::scale();
    const bool pressed=Button(id,{side,side},design::Action::Ghost);
    const auto p=ImGui::GetItemRectMin();const float k=side/32;
    auto at=[&](float x,float y){return ImVec2{p.x+x*k,p.y+y*k};};
    auto* draw=ImGui::GetWindowDrawList();const auto ink=ImGui::GetColorU32(ImGuiCol_Text);const float stroke=1.5f*k;
    auto line=[&](float x,float y,float u,float v){draw->AddLine(at(x,y),at(u,v),ink,stroke);};
    if(icon==ActionIcon::Fit) {
        for(float x:{7.f,25.f}) for(float y:{7.f,25.f}) {line(x,y,x+(x<16?6:-6),y);line(x,y,x,y+(y<16?6:-6));}
    } else if(icon==ActionIcon::Undo || icon==ActionIcon::Redo) {
        auto point=[&](float x,float y){return at(icon==ActionIcon::Redo?32-x:x,y);};
        draw->AddBezierCubic(point(25,24),point(28,9),point(12,7),point(7,15),ink,stroke);
        const ImVec2 arrow[]={point(7,8),point(7,15),point(14,15)};draw->AddPolyline(arrow,3,ink,stroke,0);
    } else if(icon==ActionIcon::Spatial) {
        line(9,23,15,8);line(15,8,25,22);line(25,22,9,23);
        for(auto point:{at(9,23),at(15,8),at(25,22)}) draw->AddCircleFilled(point,3*k,ink);
    } else {
        draw->AddCircle(at(16,16),9*k,ink,0,stroke);line(7,7,7,15);line(7,15,15,15);
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused()) {
        if(*shortcut) ImGui::SetTooltip("%s (%s)",i18n::text(caption),shortcut);else ImGui::SetTooltip("%s",i18n::text(caption));
    }
    return pressed;
}

// Vector glyphs stay crisp at each supported DPI without another font asset.
inline bool tool_button(int index,int selected,const char* name,const char* shortcut) {
    const float side=design::metrics.icon_size*design::scale();
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
    if(index==selected) d->AddRect(p,{p.x+side,p.y+side},ImGui::ColorConvertFloat4ToU32(design::color(design::colors().accent)),design::metrics.control_radius*design::scale(),0,2*design::scale());
    ImGui::PopID();return pressed;
}
}
