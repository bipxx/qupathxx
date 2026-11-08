#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <utility>
#include <optional>
#include <string>
#include "gui_i18n.hpp"
#include "gui_design_system.hpp"
#include "gui_icons.hpp"

namespace qpx::ui {
struct LayoutStats {
    std::uint64_t stacked_fields=0,wrapped_buttons=0,overflow_items=0;
    double max_overflow=0;
    std::string first_overflow;
};
inline thread_local LayoutStats layout_stats;
inline void begin_frame() {layout_stats={};}
inline float available() {return std::max(1.f,ImGui::GetContentRegionAvail().x);}
inline void observe(const char* label="<combo>") {
    const auto* window=ImGui::GetCurrentWindow();
    if(window->SkipItems) return;
    const float overflow=std::max(0.f,ImGui::GetItemRectMax().x-window->WorkRect.Max.x);
    if(overflow>.75f) {++layout_stats.overflow_items;layout_stats.max_overflow=std::max(layout_stats.max_overflow,double(overflow));if(layout_stats.first_overflow.empty()) layout_stats.first_overflow=std::string(label);}
}
inline const char* label_end(const char* label) {const auto* end=std::strstr(label,"##");return end?end:label+std::strlen(label);}
inline void label_text(const char* label,const char* end) {
    ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(label,end);ImGui::PopTextWrapPos();
}
inline void SetNextItemWidth(float width) {ImGui::SetNextItemWidth(width>0?width*design::scale():width);}

// The hidden widget ID is identical in the stacked and inline presentations.
// Groups forward hover/active state and keep each field a single flow item.
template<class Draw> bool field(const char* label,Draw&& draw) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const auto* original=label;const auto localized=i18n::label(label);label=localized.c_str();
    const auto* end=label_end(label);
    const float room=available(),desired=std::max(1.f,ImGui::CalcItemWidth());
    const float label_width=ImGui::CalcTextSize(label,end).x;
    const bool stacked=label_width>0 && desired+label_width+ImGui::GetStyle().ItemInnerSpacing.x>room;
    ImGui::BeginGroup();ImGui::PushID(original);
    if(stacked) {++layout_stats.stacked_fields;label_text(label,end);}
    ImGui::SetNextItemWidth(stacked?room:std::min(desired,std::max(1.f,room-(label_width>0?label_width+ImGui::GetStyle().ItemInnerSpacing.x:0))));
    const bool changed=draw("##value");
    if(!stacked && label_width>0) {ImGui::SameLine(0,ImGui::GetStyle().ItemInnerSpacing.x);ImGui::TextUnformatted(label,end);}
    ImGui::PopID();ImGui::EndGroup();observe(label);return changed;
}
#define QPX_UI_FIELD(Name) template<class... Args> bool Name(const char* label,Args&&... args) {return field(label,[&](const char* id){return ImGui::Name(id,std::forward<Args>(args)...);});}
QPX_UI_FIELD(InputText)
QPX_UI_FIELD(InputScalar)
QPX_UI_FIELD(Combo)
QPX_UI_FIELD(SliderFloat)
#undef QPX_UI_FIELD
template<class... Args> bool InputTextWithHint(const char* label,const char* hint,Args&&... args) {
    return field(label,[&](const char* id){return ImGui::InputTextWithHint(id,i18n::text(hint),std::forward<Args>(args)...);});
}
inline bool Combo(const char* label,int* current,const char* items,int height=-1) {
    std::string localized;
    for(const char* item=items;*item;item+=std::strlen(item)+1) {localized+=i18n::text(item);localized+='\0';}
    localized+='\0';
    return field(label,[&](const char* id){return ImGui::Combo(id,current,localized.c_str(),height);});
}
inline bool InputScalarN(const char* label,ImGuiDataType type,void* data,int components,const void* step=nullptr,const void* fast=nullptr,const char* format=nullptr,ImGuiInputTextFlags flags=0) {
    return field(label,[&](const char* id) {
        const float width=ImGui::CalcItemWidth();
        if(components!=3 || width/3>=6*ImGui::GetFontSize()+2*ImGui::GetStyle().FramePadding.x)
            return ImGui::InputScalarN(id,type,data,components,step,fast,format,flags);
        ++layout_stats.stacked_fields;bool changed=false;
        ImGui::PushID(id);ImGui::BeginGroup();
        const char* labels[]={"R","G","B"};
        for(int component=0;component<components;++component) {
            ImGui::PushID(component);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(labels[component]);
            ImGui::SameLine(0,ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::SetNextItemWidth(std::max(1.f,width-ImGui::CalcTextSize(labels[component]).x-ImGui::GetStyle().ItemInnerSpacing.x));
            changed=ImGui::InputScalar("##v",type,static_cast<char*>(data)+component*ImGui::DataTypeGetInfo(type)->Size,step,fast,format,flags) || changed;
            ImGui::PopID();
        }
        ImGui::EndGroup();ImGui::PopID();return changed;
    });
}
inline bool ColorEdit3(const char* label,float* values,ImGuiColorEditFlags flags=0) {
    return field(label,[&](const char* id) {
        const float rgb_width=3*(ImGui::CalcTextSize("000").x+2*ImGui::GetStyle().FramePadding.x)+ImGui::GetFrameHeight()+3*ImGui::GetStyle().ItemInnerSpacing.x;
        if(!(flags&ImGuiColorEditFlags_DisplayMask_) && ImGui::CalcItemWidth()<rgb_width) flags|=ImGuiColorEditFlags_DisplayHex;
        return ImGui::ColorEdit3(id,values,flags);
    });
}
inline bool InputDouble(const char* label,double* value,double step=0,double fast=0,const char* format="%.6f",ImGuiInputTextFlags flags=0) {
    return field(label,[&](const char* id){return ImGui::InputDouble(id,value,ImGui::CalcItemWidth()<3*ImGui::GetFrameHeight()?0:step,fast,format,flags);});
}
inline bool InputInt(const char* label,int* value,int step=1,int fast=100,ImGuiInputTextFlags flags=0) {
    return field(label,[&](const char* id){return ImGui::InputInt(id,value,ImGui::CalcItemWidth()<3*ImGui::GetFrameHeight()?0:step,fast,flags);});
}
inline bool InputInt64(const char* label,std::int64_t* value) {
    return field(label,[&](const char* id){return ImGui::InputScalar(id,ImGuiDataType_S64,value);});
}
// BeginCombo changes the current window when open; retain the parent ID scope
// until EndCombo restores that window, rather than popping the popup's stack.
inline bool BeginCombo(const char* label,const char* preview,ImGuiComboFlags flags=0) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const auto* original=label;const auto localized=i18n::label(label);label=localized.c_str();
    const float width=std::min(available(),std::max(1.f,ImGui::CalcItemWidth()));
    const auto* end=label_end(label);
    if(end!=label) {++layout_stats.stacked_fields;label_text(label,end);}
    ImGui::PushID(original);ImGui::SetNextItemWidth(width);
    if(ImGui::BeginCombo("##value",preview,flags)) return true;
    observe();ImGui::PopID();return false;
}
inline void EndCombo() {ImGui::EndCombo();observe();ImGui::PopID();}

inline bool Button(const char* label,ImVec2 size={0,0},design::Action action=design::Action::Secondary) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    design::ActionStyle recipe(action);
    size.y=std::max(size.y,(design::compact_actions?design::metrics.compact_height:design::metrics.action_height)*design::scale());
    const auto localized=i18n::label(label);label=localized.c_str();
    const auto* end=label_end(label);const auto padding=ImGui::GetStyle().FramePadding;
    const float natural=ImGui::CalcTextSize(label,end).x+2*padding.x;
    if(ImGui::GetCurrentWindow()->DC.IsSameLine && (size.x>0?size.x:natural)>available()) ImGui::NewLine();
    const float width=std::min(size.x>0?size.x:natural,available());
    if(natural<=width+.1f) {const bool pressed=ImGui::Button(label,{width,size.y});observe(label);return pressed;}
    ++layout_stats.wrapped_buttons;
    const float text_width=std::max(1.f,width-2*padding.x);
    const auto text_size=ImGui::CalcTextSize(label,end,false,text_width);
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
    const bool pressed=ImGui::Button(label,{width,std::max(size.y,text_size.y+2*padding.y)});
    ImGui::PopStyleColor();
    const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
    // Fine-clip glyphs without introducing a distinct GPU scissor/draw command
    // for every wrapped label. This is the same path as RenderTextClipped.
    const ImVec4 clip{lo.x,lo.y,hi.x,hi.y};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{lo.x+padding.x,lo.y+padding.y},ImGui::GetColorU32(ImGuiCol_Text),label,end,text_width,&clip);
    observe(label);return pressed;
}
inline bool SmallButton(const char* label) {
    design::CompactActions compact;
    auto padding=ImGui::GetStyle().FramePadding;padding.y=0;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);const bool pressed=Button(label);ImGui::PopStyleVar();return pressed;
}
inline bool PrimaryButton(const char* label,ImVec2 size={0,0}) {return Button(label,size,design::Action::Primary);}
inline bool DangerButton(const char* label,ImVec2 size={0,0}) {return Button(label,size,design::Action::Danger);}
inline bool GhostButton(const char* label,ImVec2 size={0,0}) {return Button(label,size,design::Action::Ghost);}
// The hidden ID hashes to the original English caption, as the text button did.
// Language, tooltip and presentation changes therefore preserve focus/replay IDs.
inline bool IconButton(const char* label,Icon icon,design::Action action=design::Action::Ghost,const char* shortcut="",bool selected=false) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const float side=design::metrics.icon_size*design::scale();
    if(ImGui::GetCurrentWindow()->DC.IsSameLine && available()<side) ImGui::NewLine();
    const auto* stable=std::strstr(label,"###");const std::string id=std::string("###")+(stable?stable+3:label);
    design::ActionStyle recipe(action);
    if(selected) ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
    const bool pressed=ImGui::Button(id.c_str(),{std::min(side,available()),side});
    if(selected) ImGui::PopStyleColor();
    const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
    draw_icon(icon,lo,hi,ImGui::GetColorU32(ImGuiCol_Text));
    if(selected) ImGui::GetWindowDrawList()->AddRect(lo,hi,ImGui::GetColorU32(design::color(design::colors().accent)),design::metrics.control_radius*design::scale(),0,2*design::scale());
    observe(label);
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip|ImGuiHoveredFlags_AllowWhenDisabled)) {
        const std::string caption(label,label_end(label));
        if(*shortcut) ImGui::SetTooltip("%s (%s)",i18n::text(caption.c_str()),shortcut);
        else ImGui::SetTooltip("%s",i18n::text(caption.c_str()));
    }
    return pressed;
}
inline bool Checkbox(const char* label,bool* value) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const auto* original=label;const auto localized=i18n::label(label);label=localized.c_str();
    const auto* end=label_end(label);
    if(ImGui::GetCurrentWindow()->DC.IsSameLine && available()<ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+std::min(120.f,ImGui::CalcTextSize(label,end).x)) ImGui::NewLine();
    ImGui::BeginGroup();ImGui::PushID(original);const auto id=ImGui::GetID("##value");
    bool changed=ImGui::Checkbox("##value",value);
    if(end!=label) {
        ImGui::SameLine(0,ImGui::GetStyle().ItemInnerSpacing.x);
        if(ImGui::CalcTextSize(label,end).x>available()) ++layout_stats.stacked_fields;
        label_text(label,end);
        if(!changed && ImGui::IsItemClicked()) {*value=!*value;changed=true;ImGui::MarkItemEdited(id);}
    }
    ImGui::PopID();ImGui::EndGroup();observe(label);return changed;
}
inline bool BeginPopupModal(const char* name,bool* open=nullptr,ImGuiWindowFlags flags=0) {
    const auto size=ImGui::GetIO().DisplaySize;
    const float margin=std::max(16.f,ImGui::GetFontSize());
    const float width=std::max(1.f,std::min(design::metrics.dialog_width*design::scale(),size.x-2*margin));
    ImGui::SetNextWindowSize({width,0},ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({width,0},{width,std::max(1.f,size.y-2*margin)});
    design::Heading title;
    return ImGui::BeginPopupModal(i18n::label(name).c_str(),open,flags);
}
inline float CollectionHeight(std::size_t rows) {
    const float row=ImGui::GetTextLineHeightWithSpacing();
    const float desired=(rows?std::clamp(float(rows),4.f,8.f):2.f)*row+2*ImGui::GetStyle().FramePadding.y;
    return std::min(desired,std::max((rows?3.f:2.f)*row,ImGui::GetContentRegionAvail().y*.35f));
}
inline void SetNextToolWindow(const char* name,ImVec2 preferred) {
    const auto* vp=ImGui::GetMainViewport();const float scale=design::scale();
    const float margin=std::min(12.f*scale,std::min(vp->WorkSize.x,vp->WorkSize.y)*.05f);
    const ImVec2 origin{vp->WorkPos.x+margin,vp->WorkPos.y+margin};
    const ImVec2 extent{std::max(1.f,vp->WorkSize.x-2*margin),std::max(1.f,vp->WorkSize.y-2*margin)};
    const ImVec2 minimum{std::min(260.f*scale,extent.x),std::min(160.f*scale,extent.y)};
    ImGui::SetNextWindowSize({std::min(preferred.x*scale,extent.x),std::min(preferred.y*scale,extent.y)},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({origin.x+extent.x*.5f,origin.y+extent.y*.5f},ImGuiCond_FirstUseEver,{.5f,.5f});
    ImGui::SetNextWindowSizeConstraints(minimum,extent);
    if(auto* window=ImGui::FindWindowByName(i18n::label(name).c_str());window && !window->DockNode) {
        const ImVec2 size{std::clamp(window->SizeFull.x,minimum.x,extent.x),std::clamp(window->SizeFull.y,minimum.y,extent.y)};
        const ImVec2 pos{std::clamp(window->Pos.x,origin.x,origin.x+extent.x-size.x),std::clamp(window->Pos.y,origin.y,origin.y+extent.y-size.y)};
        if(pos.x!=window->Pos.x || pos.y!=window->Pos.y) ImGui::SetNextWindowPos(pos);
    }
}
inline void TextDisabled(const char* format,...) {
    va_list args;va_start(args,format);ImGui::PushTextWrapPos(0);ImGui::TextDisabledV(i18n::text(format),args);ImGui::PopTextWrapPos();va_end(args);
}
inline void SeparatorText(const char* label) {
    if(ImGui::GetCurrentWindow()->SkipItems) return;
    design::Heading title;
    label=i18n::text(label);
    if(ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().SeparatorTextPadding.x<=available()) ImGui::SeparatorText(label);
    else {ImGui::Separator();label_text(label,label_end(label));}
}
inline bool BeginMenu(const char* label,bool enabled=true) {return ImGui::BeginMenu(i18n::label(label).c_str(),enabled);}
inline bool MenuItem(const char* label,const char* shortcut=nullptr,bool selected=false,bool enabled=true) {
    return ImGui::MenuItem(i18n::label(label).c_str(),shortcut,selected,enabled);
}
inline bool CollapsingHeader(const char* label,ImGuiTreeNodeFlags flags=0) {design::Heading title;return ImGui::CollapsingHeader(i18n::label(label).c_str(),flags);}
// Use these only for application-owned captions/formats. User-supplied strings
// continue to use TextUnformatted and never pass through the catalogue.
inline void Text(const char* format,...) {va_list args;va_start(args,format);ImGui::TextV(i18n::text(format),args);va_end(args);}
inline void TextWrapped(const char* format,...) {va_list args;va_start(args,format);ImGui::TextWrappedV(i18n::text(format),args);va_end(args);}
inline void TextUnformatted(const char* caption) {ImGui::TextUnformatted(i18n::text(caption));}
}
