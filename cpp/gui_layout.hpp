#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <utility>
#include <string>

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
inline void SetNextItemWidth(float width) {ImGui::SetNextItemWidth(width>0?width*ImGui::GetFontSize()/13.f:width);}

// The hidden widget ID is identical in the stacked and inline presentations.
// Groups forward hover/active state and keep each field a single flow item.
template<class Draw> bool field(const char* label,Draw&& draw) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const auto* end=label_end(label);
    const float room=available(),desired=std::max(1.f,ImGui::CalcItemWidth());
    const float label_width=ImGui::CalcTextSize(label,end).x;
    const bool stacked=label_width>0 && desired+label_width+ImGui::GetStyle().ItemInnerSpacing.x>room;
    ImGui::BeginGroup();ImGui::PushID(label);
    if(stacked) {++layout_stats.stacked_fields;label_text(label,end);}
    ImGui::SetNextItemWidth(stacked?room:std::min(desired,std::max(1.f,room-(label_width>0?label_width+ImGui::GetStyle().ItemInnerSpacing.x:0))));
    const bool changed=draw("##value");
    if(!stacked && label_width>0) {ImGui::SameLine(0,ImGui::GetStyle().ItemInnerSpacing.x);ImGui::TextUnformatted(label,end);}
    ImGui::PopID();ImGui::EndGroup();observe(label);return changed;
}
#define QPX_UI_FIELD(Name) template<class... Args> bool Name(const char* label,Args&&... args) {return field(label,[&](const char* id){return ImGui::Name(id,std::forward<Args>(args)...);});}
QPX_UI_FIELD(InputText)
QPX_UI_FIELD(InputTextWithHint)
QPX_UI_FIELD(InputScalar)
QPX_UI_FIELD(Combo)
QPX_UI_FIELD(SliderFloat)
#undef QPX_UI_FIELD
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
    const float width=std::min(available(),std::max(1.f,ImGui::CalcItemWidth()));
    const auto* end=label_end(label);
    if(end!=label) {++layout_stats.stacked_fields;label_text(label,end);}
    ImGui::PushID(label);ImGui::SetNextItemWidth(width);
    if(ImGui::BeginCombo("##value",preview,flags)) return true;
    observe();ImGui::PopID();return false;
}
inline void EndCombo() {ImGui::EndCombo();observe();ImGui::PopID();}

inline bool Button(const char* label,ImVec2 size={0,0}) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
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
    auto padding=ImGui::GetStyle().FramePadding;padding.y=0;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);const bool pressed=Button(label);ImGui::PopStyleVar();return pressed;
}
inline bool Checkbox(const char* label,bool* value) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const auto* end=label_end(label);
    if(ImGui::GetCurrentWindow()->DC.IsSameLine && available()<ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+std::min(120.f,ImGui::CalcTextSize(label,end).x)) ImGui::NewLine();
    ImGui::BeginGroup();ImGui::PushID(label);const auto id=ImGui::GetID("##value");
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
    const float width=std::max(1.f,std::min(560.f*ImGui::GetFontSize()/13.f,size.x-2*margin));
    ImGui::SetNextWindowSize({width,0},ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({width,0},{width,std::max(1.f,size.y-2*margin)});
    return ImGui::BeginPopupModal(name,open,flags);
}
inline void TextDisabled(const char* format,...) {
    va_list args;va_start(args,format);ImGui::PushTextWrapPos(0);ImGui::TextDisabledV(format,args);ImGui::PopTextWrapPos();va_end(args);
}
inline void SeparatorText(const char* label) {
    if(ImGui::GetCurrentWindow()->SkipItems) return;
    if(ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().SeparatorTextPadding.x<=available()) ImGui::SeparatorText(label);
    else {ImGui::Separator();label_text(label,label_end(label));}
}
}
