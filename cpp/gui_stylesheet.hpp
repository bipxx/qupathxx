#pragma once
#include "gui_design_tokens.hpp"
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace qpx::ui::design {
// Native controls and retained RCSS consume the same semantic values. Expansion
// happens only on resource load/reload, before RmlUi parses the stylesheet.
inline std::string resolve_stylesheet(std::string source) {
    auto replace=[&](const std::string& name,const std::string& value) {
        const auto key="${"+name+"}";std::size_t at=0;
        while((at=source.find(key,at))!=std::string::npos) {source.replace(at,key.size(),value);at+=value.size();}
    };
    const std::pair<const char*,float> dimensions[]={
        {"font-size",metrics.font_size},{"activity-width",metrics.activity_width},
        {"activity-row",metrics.activity_row},{"activity-icon-box",metrics.activity_icon_box},
        {"activity-icon-inset",(metrics.activity_width-metrics.activity_icon_box)/2},
        {"activity-indicator",metrics.activity_indicator},{"icon-size",metrics.icon_size},
        {"icon-canvas",metrics.icon_canvas},{"icon-inset",(metrics.icon_size-metrics.icon_canvas)/2},
        {"control-radius",metrics.control_radius},{"space-sm",spacing.sm},{"space-md",spacing.md},
        {"button-radius",metrics.button_radius},
        {"toolbar-group-gap",spacing.sm},{"toolbar-group-inset",2*spacing.sm+metrics.border},
        {"toolbar-separator-height",metrics.font_size},{"toolbar-separator-top",(metrics.icon_size-metrics.font_size)/2},
        {"space-xs",spacing.xs},{"panel-radius",metrics.panel_radius},{"border",metrics.border},
        {"disabled-opacity",metrics.disabled_opacity}};
    for(const auto& value:dimensions) replace(value.first,std::to_string(value.second));
    for(const auto& theme:std::array<std::pair<const char*,const Palette*>,2>{{{"dark",&dark},{"light",&light}}}) {
        const auto& p=*theme.second;
        const std::pair<const char*,std::uint32_t> colors[]={
            {"activity-background",p.bg_primary},{"panel-background",p.bg_secondary},
            {"hover-background",p.bg_hover},{"tooltip-background",p.bg_elevated},
            {"pressed-background",p.bg_elevated},{"selected-border",p.border_light},
            {"toolbar-separator",p.border_light},
            {"foreground",p.text_primary},{"secondary-foreground",p.text_secondary},
            {"inactive-foreground",p.text_muted},{"border",p.border},
            {"focus-border",p.accent_hover}};
        for(const auto& value:colors) {std::ostringstream hex;hex<<'#'<<std::hex<<std::setw(6)<<std::setfill('0')<<value.second;
            replace(std::string(theme.first)+"."+value.first,hex.str());}
    }
    if(source.find("${")!=std::string::npos) throw std::runtime_error("Unknown or incomplete workspace style token");
    return source;
}
}
