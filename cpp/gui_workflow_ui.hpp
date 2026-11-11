#pragma once
#include "gui_layout.hpp"
#include "qupathxx/core.hpp"
#include <array>
#include <charconv>
#include <cstdio>
#include <system_error>
#include <map>
#include <functional>

namespace qpx::ui {
struct WorkflowActionTrace {Rect bounds;std::string caption;bool enabled=false;};
struct WorkflowResultTrace {AnalysisKind kind;std::array<double,3> means;std::uint64_t record_id;bool nucleus;std::string caption;};
struct WorkflowTrace {
    std::map<std::string,WorkflowActionTrace> actions;
    std::map<std::string,WorkflowResultTrace> results;
    std::function<void(int)> before_frame;
};
inline thread_local WorkflowTrace* workflow_trace=nullptr;
inline void reset_workflow_trace() {if(workflow_trace) {workflow_trace->actions.clear();workflow_trace->results.clear();}}
inline void WorkflowMeans(const char* key,const char* caption,AnalysisKind kind,
    const std::array<double,3>& means,std::uint64_t record_id,bool nucleus) {
    SeparatorText(caption);
    constexpr const char* od[]={"Mean OD R","Mean OD G","Mean OD B"};
    constexpr const char* stains[]={"Mean stain 1","Mean stain 2","Mean stain 3"};
    for(std::size_t i=0;i<means.size();++i) {
        const auto* label=kind==AnalysisKind::StainMeans?stains[i]:od[i];
        ImGui::TextWrapped("%s: %.6g",i18n::text(label),means[i]);observe(label);
    }
    if(workflow_trace)workflow_trace->results[key]={kind,means,record_id,nucleus,caption};
}
inline void trace_workflow_action(const char* original,const char* caption) {
    if(!workflow_trace)return;
    const auto lo=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();
    const bool enabled=!ImGui::GetCurrentWindow()->SkipItems && ImGui::IsItemVisible() &&
        !(ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled);
    workflow_trace->actions[original]={{lo.x,lo.y,size.x,size.y},caption,enabled};
}
// Caption changes never change the identity of an existing control. Explicit
// English suffixes also survive localization and the private replay contracts.
inline std::string workflow_label(const char* original,const char* caption) {
    const auto* stable=std::strstr(original,"###");
    return std::string(caption)+"###"+(stable?stable+3:original);
}
inline bool WorkflowAction(const char* original,const char* caption,const char* shortcut=nullptr,
    design::Action role=design::Action::Secondary) {
    const auto label=workflow_label(original,caption);
    const bool pressed=Button(label.c_str(),{},role);
    trace_workflow_action(original,caption);
    if(shortcut && *shortcut && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip|ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s (%s)",i18n::text(caption),shortcut);
    return pressed;
}
inline bool WorkflowIcon(const char* original,const char* caption,Icon icon,const char* shortcut=nullptr) {
    const auto label=workflow_label(original,caption);
    const bool pressed=IconButton(label.c_str(),icon,design::Action::Ghost,shortcut?shortcut:"");
    trace_workflow_action(original,caption);return pressed;
}

// Alias an existing replay target to the real native title/tab close control;
// this submits no second button. Tab placement comes from the pinned ImGui
// authority; the final icon inset matches RenderWindowTitleBarContents /
// TabItemLabelAndCloseButton and is exercised by native close tests.
inline std::optional<Rect> native_tool_close_bounds() {
    auto* window=ImGui::GetCurrentWindow();auto& context=*ImGui::GetCurrentContext();
    if(!window->HasCloseButton || window->SkipItems)return std::nullopt;
    auto bar=window->TitleBarRect();auto padding=ImGui::GetStyle().FramePadding;
    if(window->DockIsActive) {
        auto* node=window->DockNode;
        if(!node || !node->TabBar || node->IsNoTabBar())return std::nullopt;
        auto* tab=ImGui::TabBarFindTabByID(node->TabBar,window->TabId);
        if(!tab || !window->DockTabIsVisible)return std::nullopt;
        const auto pos=ImGui::TabBarGetTabPos(node->TabBar,tab);
        bar=ImRect(pos,{pos.x+tab->Width,node->TabBar->BarRect.Max.y});padding=node->TabBar->FramePadding;
    } else if(window->Flags&ImGuiWindowFlags_NoTitleBar)return std::nullopt;
    const float side=context.FontSize;
    return Rect{std::max(bar.Min.x,bar.Max.x-padding.x-side),bar.Min.y+padding.y,side,side};
}
template<class Mark>inline bool NativeToolCloseTarget(const char* original,Mark mark) {
    const auto rect=native_tool_close_bounds();if(!rect)return false;
    auto* window=ImGui::GetCurrentWindow();auto& context=*ImGui::GetCurrentContext();
    const auto last=context.LastItemData;const auto clip=window->ClipRect;
    window->ClipRect=window->Rect();
    ImGui::SetLastItemData(window->GetID("#CLOSE"),ImGuiItemFlags_None,ImGuiItemStatusFlags_Visible,
        ImRect({float(rect->x),float(rect->y)},{float(rect->x+rect->width),float(rect->y+rect->height)}));
    trace_workflow_action(original,"Hide panel");mark(*rect);
    context.LastItemData=last;window->ClipRect=clip;return true;
}

// Derive the significant-digit count from the shortest round-trip form, rather
// than rounding editable scientific inputs to an arbitrary display precision.
// InputDouble still owns parsing, stepping and validation at the native boundary.
inline std::array<char,12> compact_double_format(double value) {
    std::array<char,64> text{};
    const auto converted=std::to_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
    int digits=0;bool significant=false;
    if(converted.ec==std::errc{})for(auto* p=text.data();p!=converted.ptr && *p!='e' && *p!='E';++p) {
        if(*p>='0' && *p<='9') {significant=significant || *p!='0';if(significant)++digits;}
    }
    std::array<char,12> format{};
    std::snprintf(format.data(),format.size(),"%%.%dg",std::max(1,std::min(17,digits)));
    return format;
}
inline bool CompactInputDouble(const char* label,double* value,double step=0,double fast=0,
    ImGuiInputTextFlags flags=0) {
    const auto format=compact_double_format(*value);
    const bool changed=InputDouble(label,value,step,fast,format.data(),flags);
    trace_workflow_action(label,label);return changed;
}
}
