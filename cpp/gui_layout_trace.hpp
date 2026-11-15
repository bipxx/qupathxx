#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include "gui_icons.hpp"
#include "gui_i18n.hpp"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace qpx::ui {
// Private rendered observation. Only the observing test owns strings/storage.
enum class LayoutKind {Icon,TextButton,Input,Row,Caption,Separator,Palette};
struct LayoutItem {
    std::string window,surface,caption,command,section;
    ImGuiID region=0;
    ImRect rect;
    LayoutKind kind=LayoutKind::Caption;
    Icon icon=Icon::Info;
    float line=0,scale=1,right=0;
    int language=0;
};
struct LayoutTrace {
    std::vector<LayoutItem> items;
    std::map<ImGuiID,std::string> sections;
    std::function<void(const std::vector<LayoutItem>&)> frame;
    std::function<void(bool&,int&)> project_settings;
    bool rendered_only=false;
    std::function<void()> before_frame;
    std::function<void()> after_settings;
};
inline thread_local LayoutTrace* layout_trace=nullptr;
inline thread_local int tool_palette_depth=0;
struct ToolPalette {ToolPalette(){++tool_palette_depth;}~ToolPalette(){--tool_palette_depth;}};
inline thread_local ImGuiID layout_parent_region=0;
inline thread_local const char* layout_surface=nullptr;
struct LayoutSurface {
    const char* previous=layout_surface;
    explicit LayoutSurface(const char* surface){layout_surface=surface;}
    ~LayoutSurface(){layout_surface=previous;}
};
class LayoutRegion {
    ImGuiID previous_=layout_parent_region;
public:
    explicit LayoutRegion(const char* id){layout_parent_region=ImGui::GetID(id);}
    ~LayoutRegion(){layout_parent_region=previous_;}
};
inline void finish_layout_frame() {
    if(!layout_trace)return;
    if(layout_trace->frame && !layout_trace->items.empty())layout_trace->frame(layout_trace->items);
    layout_trace->items.clear();layout_trace->sections.clear();
}
inline void trace_layout(LayoutKind kind,const char* caption="",Icon icon=Icon::Info,const ImRect* bounds=nullptr,bool section_break=true) {
    if(!layout_trace)return;
    if(layout_trace->rendered_only && !ImGui::GetIO().BackendRendererName)return;
    auto* window=ImGui::GetCurrentWindow();
    if(window->SkipItems)return;
    const auto* table=ImGui::GetCurrentTable();
    // A named region joins a list's external caption and scrolling body;
    // unrelated children keep distinct parents even when their rows overlap.
    const auto region=layout_parent_region?layout_parent_region:table?table->ID:window->ID;
    if(kind==LayoutKind::Separator && section_break)layout_trace->sections[region]=std::to_string(layout_trace->items.size())+":"+caption;
    if(kind==LayoutKind::Icon && tool_palette_depth)kind=LayoutKind::Palette;
    layout_trace->items.push_back({window->RootWindow->Name,layout_surface?layout_surface:i18n::surface_name,caption,caption,layout_trace->sections[region],region,
        bounds?*bounds:ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax()),kind,icon,ImGui::GetTextLineHeightWithSpacing(),design::scale(),window->WorkRect.Max.x,i18n::effective_language()});
}
inline void trace_layout_command(const char* command) {
    if(!layout_trace || layout_trace->items.empty())return;
    auto& item=layout_trace->items.back();const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
    if(item.window==ImGui::GetCurrentWindow()->RootWindow->Name && item.rect.Min.x==lo.x && item.rect.Min.y==lo.y && item.rect.Max.x==hi.x && item.rect.Max.y==hi.y)item.command=command;
}
struct LayoutViolation {std::string principle,message;LayoutItem item;};
using IconVocabulary=std::set<std::pair<int,std::string>>;
inline std::vector<LayoutViolation> lint_layout(const std::vector<LayoutItem>& items,const IconVocabulary& vocabulary) {
    std::vector<LayoutViolation> errors;
    const auto same=[](const LayoutItem& a,const LayoutItem& b){return a.window==b.window && a.region==b.region;};
    const auto line=[](const LayoutItem& a,const LayoutItem& b){return std::min(a.rect.Max.y,b.rect.Max.y)-std::max(a.rect.Min.y,b.rect.Min.y)>.5f;};
    const auto anchor=[](const LayoutItem& item){return item.rect.GetWidth()>.5f && (item.kind==LayoutKind::Input || item.kind==LayoutKind::Row || ((item.kind==LayoutKind::Caption || item.kind==LayoutKind::Separator) && !item.caption.empty()));};
    for(std::size_t n=0;n<items.size();++n) {
        const auto& a=items[n];
        if(a.kind==LayoutKind::Icon) {
            bool attached=false;
            for(const auto& b:items)if(same(a,b) && line(a,b) && anchor(b)){attached=true;break;}
            if(!attached)errors.push_back({"P2","orphan-icon",a});
        }
        if((a.kind==LayoutKind::Icon || a.kind==LayoutKind::Palette) && !vocabulary.count({int(a.icon),a.command}))
            errors.push_back({"P8","icon-meaning",a});
        if(a.kind==LayoutKind::Row) {
            // Combo choices are the open field's options, not a collection.
            if(a.window.find("##Combo")!=std::string::npos)continue;
            const LayoutItem* previous=nullptr;
            for(std::size_t k=n;k-->0;)if(same(a,items[k]) && (items[k].kind==LayoutKind::Row || ((items[k].kind==LayoutKind::Caption || items[k].kind==LayoutKind::Separator) && !items[k].caption.empty())) && items[k].rect.Max.y<=a.rect.Min.y+.5f) {
                if(!previous || items[k].rect.Max.y>previous->rect.Max.y)previous=&items[k];
            }
            if(previous && previous->kind==LayoutKind::Row && a.rect.Min.y-previous->rect.Max.y<=a.line)continue;
            if(!previous || (previous->kind!=LayoutKind::Caption && previous->kind!=LayoutKind::Separator) || a.rect.Min.y-previous->rect.Max.y>a.line)
                errors.push_back({"P6","uncaptioned-list",a});
        }
    }
    // Compare the drawn right edge of adjacent strips within one section.
    std::vector<const LayoutItem*> strips;
    for(const auto& a:items)if(a.kind==LayoutKind::Icon) {
        bool rightmost=true,attached=false;
        for(const auto& b:items)if(same(a,b) && line(a,b)) {
            if(b.kind==LayoutKind::Icon && b.rect.Max.x>a.rect.Max.x+.5f)rightmost=false;
            if(anchor(b) && b.rect.Min.x<a.rect.Min.x)attached=true;
        }
        if(rightmost && attached)strips.push_back(&a);
    }
    for(std::size_t n=1;n<strips.size();++n) {
        const auto& a=*strips[n-1];const auto& b=*strips[n];
        if(same(a,b) && a.section==b.section && b.rect.Min.y>=a.rect.Max.y && b.rect.Min.y-a.rect.Max.y<=b.line && std::abs(a.rect.Max.x-b.rect.Max.x)>1.f)
            errors.push_back({"P9","ragged-icon-strip",b});
    }
    return errors;
}
}
