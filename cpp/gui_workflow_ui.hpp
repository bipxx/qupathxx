#pragma once
#include "gui_layout.hpp"
#include "gui_commands.hpp"
#include "qupathxx/core.hpp"
#include <array>
#include <charconv>
#include <cstdio>
#include <system_error>
#include <map>
#include <functional>

namespace qpx::ui {
struct WorkflowActionTrace {Rect bounds;std::string caption;bool enabled=false;bool primary=false;std::size_t order=0;std::string reason,inline_reason;bool disabled=false;};
struct WorkflowResultTrace {AnalysisKind kind;std::array<double,3> means;std::uint64_t record_id;bool nucleus;std::string caption;};
struct WorkflowTrace {
    std::map<std::string,WorkflowActionTrace> actions;
    std::map<std::string,WorkflowResultTrace> results;
    std::function<void(int)> before_frame;
};
inline thread_local WorkflowTrace* workflow_trace=nullptr;
inline void reset_workflow_trace() {if(workflow_trace) {workflow_trace->actions.clear();workflow_trace->results.clear();}}
inline void WorkflowMeans(const char* key,const char* caption,AnalysisKind kind,
    const std::array<double,3>& means,std::uint64_t record_id,bool nucleus,bool heading=true) {
    if(heading)SeparatorText(caption);
    constexpr const char* od[]={"Mean OD R","Mean OD G","Mean OD B"};
    constexpr const char* stains[]={"Mean stain 1","Mean stain 2","Mean stain 3"};
    for(std::size_t i=0;i<means.size();++i) {
        const auto* label=kind==AnalysisKind::StainMeans?stains[i]:od[i];
        ui::DataTextWrapped("%s: %.6g",i18n::text(label),means[i]);observe(label);
    }
    if(workflow_trace)workflow_trace->results[key]={kind,means,record_id,nucleus,caption};
}
inline void trace_workflow_action(const char* original,const char* caption,design::Action role=design::Action::Secondary,Availability availability={}) {
    if(!workflow_trace)return;
    const auto lo=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();
    const bool enabled=!ImGui::GetCurrentWindow()->SkipItems && ImGui::IsItemVisible() &&
        !(ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled);
    const auto order=workflow_trace->actions.size();
    workflow_trace->actions[original]={{lo.x,lo.y,size.x,size.y},caption,enabled,role==design::Action::Primary,order,
        !enabled && availability.reason?i18n::text(availability.reason):"",{},
        bool(ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled)};
}
// Caption changes never change the identity of an existing control. Explicit
// English suffixes also survive localization and the private replay contracts.
inline std::string workflow_label(const char* original,const char* caption) {
    const auto* stable=std::strstr(original,"###");
    return std::string(caption)+"###"+(stable?stable+3:original);
}
inline void WorkflowReason(const char* original,Availability availability) {
    if(availability.enabled || !availability.reason)return;
    TextWrapped("%s",i18n::text(availability.reason));
    if(workflow_trace)workflow_trace->actions[original].inline_reason=i18n::text(availability.reason);
}
inline Availability WorkflowBlocked() {
    return {false,ImGui::GetTopMostPopupModal()?"Finish the current dialog first.":"Wait for the current work to finish."};
}
inline Availability DraftAvailability(bool changes) {
    if(!changes)return {false,"There is nothing to apply."};
    return {};
}
inline Availability ObjectPropertiesAvailability(bool present) {
    if(!present)return {false,"This object has no properties."};
    return {};
}
inline void WorkflowTooltip(const char* caption,const char* shortcut,Availability availability) {
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip|ImGuiHoveredFlags_AllowWhenDisabled))return;
    if(!availability.enabled && availability.reason) {
        ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious,ImGuiWindowFlags_None);
        const float width=std::max(1.f,std::min(36*ImGui::GetFontSize(),ImGui::GetMainViewport()->WorkSize.x-2*ImGui::GetStyle().WindowPadding.x));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width);
        ui::DataText(i18n::text(caption));
        if(shortcut && *shortcut)ui::DataText(shortcut);
        ui::DataText(i18n::text(availability.reason));
        ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    else if(shortcut && *shortcut)ImGui::SetTooltip("%s (%s)",i18n::text(caption),shortcut);
}
inline bool WorkflowAction(const char* original,design::Action role,Availability availability,const char* formatted_caption=nullptr) {
    const auto& info=workflow_command(original);
    const auto* caption=formatted_caption?formatted_caption:info.caption;const auto* shortcut=command_chord(info.id);
    const auto* label=formatted_caption?nullptr:command_label(info.id);
    const auto dynamic_label=formatted_caption?workflow_label(original,caption):std::string{};
    ImGui::BeginDisabled(!availability.enabled);
    const bool pressed=Button(formatted_caption?dynamic_label.c_str():label,{},role);
    trace_workflow_action(original,caption,role,availability);
    WorkflowTooltip(caption,shortcut,availability);ImGui::EndDisabled();
    return pressed;
}
inline bool WorkflowAction(const char* original,design::Action role=design::Action::Secondary,const char* formatted_caption=nullptr) {
    return WorkflowAction(original,role,{},formatted_caption);
}
inline bool WorkflowIcon(const char* original,Icon icon,Availability availability={}) {
    const auto& info=workflow_command(original);
    const auto* caption=info.caption;const auto* shortcut=command_chord(info.id);
    const auto* label=command_label(info.id);
    std::optional<CaptionedStrip> status;
    if(icon==Icon::Stop && !action_row_depth) {if(ImGui::GetCurrentWindow()->DC.IsSameLine)ImGui::NewLine();status.emplace("Run status",1);status->next();}
    ImGui::BeginDisabled(!availability.enabled);
    const bool pressed=IconButton(label,icon,design::Action::Ghost,shortcut?shortcut:"");
    trace_layout_command(info.legacy_id);
    trace_workflow_action(original,caption,design::Action::Secondary,availability);
    WorkflowTooltip(caption,shortcut,availability);ImGui::EndDisabled();return pressed;
}
inline bool AvailableButton(const char* original,Availability availability,design::Action role=design::Action::Secondary,const char* trace_caption=nullptr) {
    ImGui::BeginDisabled(!availability.enabled);
    const bool pressed=Button(original,{},role);
    trace_workflow_action(original,trace_caption?trace_caption:original,role,availability);
    WorkflowTooltip(original,nullptr,availability);ImGui::EndDisabled();return pressed;
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
inline bool DraftInputDouble(const char* label,double* value,std::optional<double>& previous) {
    const auto id=field_input_id(label);
    // A clean component can follow a document edit while its field owns focus.
    // Reload that changed value, rather than letting ImGui's old text undo it.
    if(previous && *previous!=*value && ImGui::GetActiveID()==id)
        if(auto* state=ImGui::GetInputTextState(id))state->ReloadUserBufAndKeepSelection();
    ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar,true);
    const bool changed=CompactInputDouble(label,value);ImGui::PopItemFlag();previous=*value;return changed;
}
}
