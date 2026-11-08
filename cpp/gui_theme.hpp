#pragma once
#include "gui_layout.hpp"

namespace qpx::ui {
// Desktop chrome only. Scientific rendering owns its data colors and geometry.
inline void apply_desktop_theme() {design::apply(ImGui::GetStyle(),design::current());}
using ActionIcon=Icon;
inline bool icon_action(const char* id,ActionIcon icon,const char* caption,const char* shortcut="") {
    const auto label=std::string(caption)+"###"+id;
    return IconButton(label.c_str(),icon,design::Action::Ghost,shortcut);
}
inline bool tool_button(int index,int selected,const char* name,const char* shortcut) {
    constexpr Icon icons[]={Icon::Select,Icon::Rectangle,Icon::Ellipse,Icon::Polygon,Icon::Polyline,Icon::Points,Icon::Vertices,Icon::Hole};
    ImGui::PushID(index);
    const auto label=std::string(name)+"###"+"##tool";
    const bool pressed=IconButton(label.c_str(),icons[index],design::Action::Ghost,shortcut,index==selected);
    ImGui::PopID();return pressed;
}
}
