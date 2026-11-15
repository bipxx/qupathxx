#pragma once
#include "gui_layout.hpp"
#include "gui_commands.hpp"

namespace qpx::ui {
// Desktop chrome only. Scientific rendering owns its data colors and geometry.
inline void apply_desktop_theme() {design::apply(ImGui::GetStyle(),design::current());}
using ActionIcon=Icon;
inline int toolbar_rows(float width,int utility_controls) {
    const float scale=design::scale(),side=design::metrics.icon_size*scale,gap=design::spacing.xs*scale;
    const float separator=design::group_separator_width*scale;
    int rows=1;float used=0;
    const int groups[]={8,2,utility_controls};
    for(int group=0;group<3;++group) {
        const float group_width=groups[group]*side+(groups[group]-1)*gap;
        if(group) {if(used+separator+group_width>width) {++rows;used=0;}else used+=separator;}
        for(int i=0;i<groups[group];++i) {
            if(i) {if(used+gap+side>width) {++rows;used=0;}else used+=gap;}
            used+=side;
        }
    }
    return rows;
}
// Called after SameLine. Keep a group together when it fits on a fresh row.
inline void toolbar_separator(int following_controls) {
    const float scale=design::scale(),side=design::metrics.icon_size*scale,gap=design::spacing.xs*scale;
    const float span=design::group_separator_width*scale;
    if(available()<span-gap+following_controls*side+(following_controls-1)*gap) {ImGui::NewLine();return;}
    const auto p=ImGui::GetCursorScreenPos();
    ImGui::Dummy({span-gap,side});
    const float x=p.x+(design::spacing.sm-design::spacing.xs+design::metrics.border*.5f)*scale;
    const float inset=(design::metrics.icon_size-design::metrics.font_size)*.5f*scale;
    ImGui::GetWindowDrawList()->AddLine({x,p.y+inset},{x,p.y+side-inset},ImGui::ColorConvertFloat4ToU32(design::color(design::colors().border_light)),design::metrics.border*scale);
    const float half=design::metrics.border*scale*.5f;const ImRect rect{{x-half,p.y+inset},{x+half,p.y+side-inset}};
    trace_layout(LayoutKind::Separator,"",Icon::Info,&rect,false);
    ImGui::SameLine(0,0);
}
inline bool icon_action(const char* id,ActionIcon icon,CommandId command,Availability availability={}) {
    const bool hit=IconButton(presentation_label(command,id),icon,availability,command_chord(command));
    trace_layout_command(qpx::ui::command(command).legacy_id);return hit;
}
inline bool tool_button(int index,int selected,CommandId command) {
    constexpr Icon icons[]={Icon::Select,Icon::Rectangle,Icon::Ellipse,Icon::Polygon,Icon::Polyline,Icon::Points,Icon::Vertices,Icon::Hole};
    ImGui::PushID(index);
    const bool pressed=IconButton(presentation_label(command,"##tool"),icons[index],design::Action::Ghost,command_chord(command),index==selected);
    if(layout_trace && !layout_trace->items.empty())layout_trace->items.back().kind=LayoutKind::Palette;
    trace_layout_command(qpx::ui::command(command).legacy_id);
    ImGui::PopID();return pressed;
}
}
