#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>
#include <optional>
#include <string>
#include "gui_i18n.hpp"
#include "gui_units.hpp"
#include "gui_design_system.hpp"
#include "gui_icons.hpp"
#include "gui_layout_trace.hpp"
#include "gui_commands.hpp"

namespace qpx::ui {
// Formatting visible units separately keeps translations and stable widget IDs
// independent of the unit's spelling.
inline std::string unit_text(const char* key,const char* unit) {
    char caption[1024];std::snprintf(caption,sizeof(caption),i18n::text(key),unit);
    return caption;
}
inline std::string unit_caption(const char* key,const char* unit,const char* original) {
    return unit_text(key,unit)+"###"+original;
}
// Data is literal visible text, never a translation key or a widget identity.
// ImGui still hides the visible suffix starting at the first ## in the data.
inline std::string data_label(std::string_view visible,std::string_view id) {
    return std::string(visible)+"###"+std::string(id);
}
struct LayoutStats {
    std::uint64_t stacked_fields=0,wrapped_buttons=0,overflow_items=0;
    double max_overflow=0;
    std::string first_overflow;
};
inline thread_local LayoutStats layout_stats;
inline thread_local Availability icon_availability;
struct IconReason {
    Availability previous=icon_availability;
    explicit IconReason(const char* reason){icon_availability={false,reason};}
    ~IconReason(){icon_availability=previous;}
};
inline void begin_frame() {finish_layout_frame();if(layout_trace && layout_trace->before_frame)layout_trace->before_frame();layout_stats={};}
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
    trace_layout(LayoutKind::Caption,label);
}
inline void SetNextItemWidth(float width) {ImGui::SetNextItemWidth(width>0?width*design::scale():width);}

// A growing field and a fixed trailing command group share one control height.
// Keep compact commands with their context even when its label wraps.
inline thread_local int action_row_depth=0;
class ActionRow {
    float start_,room_,field_,gap_,side_,destructive_extra_;
    int actions_,drawn_=0;
    bool stacked_;
public:
    explicit ActionRow(int actions,float trailing_inset=0,bool destructive_group=false):start_(ImGui::GetCursorPosX()),room_(std::max(1.f,available()-trailing_inset)),
        gap_(design::spacing.sm*design::scale()),side_(design::metrics.icon_size*design::scale()),actions_(actions),
        stacked_(false) {
        destructive_extra_=destructive_group?(design::group_separator_width-design::spacing.sm)*design::scale():0;
        field_=stacked_?room_:std::max(1.f,room_-actions_*(side_+gap_)-destructive_extra_);
        auto padding=ImGui::GetStyle().FramePadding;padding.y=std::max(0.f,(side_-ImGui::GetFontSize())*.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);
        ImGui::SetNextItemWidth(field_);
        ++action_row_depth;
    }
    ~ActionRow(){--action_row_depth;ImGui::PopStyleVar();}
    ActionRow(const ActionRow&)=delete;ActionRow& operator=(const ActionRow&)=delete;
    float field_width() const {return field_;}
    void next() {
        if(drawn_++) ImGui::SameLine(0,gap_);
        else if(stacked_) ImGui::SetCursorPosX(start_+std::max(0.f,room_-actions_*side_-(actions_-1)*gap_));
        // SameLine's explicit offset also adds the current group's offset.
        // Cursor coordinates already include it, so set x after joining y.
        else {ImGui::SameLine(0,0);ImGui::SetCursorPosX(start_+field_+gap_);}
    }
    void destructive() {
        IM_ASSERT(destructive_extra_>0);
        next();ImGui::SetCursorPosX(ImGui::GetCursorPosX()+destructive_extra_);
        const auto p=ImGui::GetCursorScreenPos();const float x=p.x-design::group_separator_width*design::scale()*.5f;
        const float inset=(design::metrics.icon_size-design::metrics.font_size)*.5f*design::scale();
        ImGui::GetWindowDrawList()->AddLine({x,p.y+inset},{x,p.y+side_-inset},ImGui::ColorConvertFloat4ToU32(design::color(design::colors().border_light)),design::metrics.border*design::scale());
        const float half=design::metrics.border*design::scale()*.5f;const ImRect rect{{x-half,p.y+inset},{x+half,p.y+side_-inset}};
        trace_layout(LayoutKind::Separator,"",Icon::Info,&rect,false);
    }
};

enum class FieldPlacement {Column,Above};
inline float prepare_field(const char* label,const char* end,float desired,FieldPlacement placement) {
    const float room=available(),gap=design::spacing.sm*design::scale();
    if(end==label) return std::min(desired,room);
    const float column=std::min(design::metrics.label_width*design::scale(),room*.4f);
    const bool above=placement==FieldPlacement::Above || room-column-gap<design::metrics.field_min_width*design::scale();
    ImGui::PushStyleColor(ImGuiCol_Text,design::color(design::colors().text_secondary));
    if(above) {++layout_stats.stacked_fields;label_text(label,end);}
    else {
        const float start=ImGui::GetCursorPosX();
        if(ImGui::CalcTextSize(label,end,false,column).y<=ImGui::GetFontSize()+.5f) ImGui::AlignTextToFramePadding();
        ImGui::PushTextWrapPos(start+column);ImGui::TextUnformatted(label,end);ImGui::PopTextWrapPos();
        trace_layout(LayoutKind::Caption,label);
        ImGui::SameLine(0,0);ImGui::SetCursorPosX(start+column+gap);
    }
    ImGui::PopStyleColor();return placement==FieldPlacement::Above?room:std::min(desired,above?room:std::max(1.f,room-column-gap));
}
// Label columns, narrow stacking and vector fields share one recipe and IDs.
inline ImGuiID field_input_id(const char* label) {
    return ImHashStr("##value",0,ImHashStr(label,0,ImGui::GetCurrentWindow()->IDStack.back()));
}
template<class Draw> bool field(const char* label,Draw&& draw,FieldPlacement placement=FieldPlacement::Column,bool literal=false,float requested_width=0) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const auto* original=label;const auto localized=literal?std::string(label):i18n::label(label);label=localized.c_str();
    const auto* end=label_end(label);
    const float desired=std::max(1.f,requested_width>0?requested_width:ImGui::CalcItemWidth());
    ImGui::BeginGroup();ImGui::PushID(original);
    ImGui::SetNextItemWidth(prepare_field(label,end,desired,placement));
    const bool changed=draw("##value");
    trace_layout(LayoutKind::Input,original);
    ImGui::PopID();ImGui::EndGroup();observe(label);return changed;
}
template<class Draw,class Actions> bool field_with_actions(const char* label,int count,Draw&& draw,Actions&& actions) {
    if(ImGui::GetCurrentWindow()->SkipItems)return false;
    const auto localized=i18n::label(label);ImGui::BeginGroup();
    prepare_field(localized.c_str(),label_end(localized.c_str()),available(),FieldPlacement::Column);
    ImGui::PushID(label);bool changed=false;
    {ActionRow row(count);changed=draw("##value");trace_layout(LayoutKind::Input,label);actions(row);}
    ImGui::PopID();ImGui::EndGroup();observe(label);return changed;
}
// Raw legacy fields had no label scope. An empty native label under the old
// ID preserves their input and step-button identities while moving the caption.
template<class Draw> bool legacy_field(const char* caption,const char* original,Draw&& draw,bool literal=false) {
    if(ImGui::GetCurrentWindow()->SkipItems)return false;
    const auto id=ImGui::GetID(original);const auto localized=literal?std::string(caption):i18n::label(caption);
    ImGui::BeginGroup();const float desired=available();
    ImGui::SetNextItemWidth(prepare_field(localized.c_str(),label_end(localized.c_str()),desired,FieldPlacement::Column));
    ImGui::PushOverrideID(id);const bool changed=draw("");trace_layout(LayoutKind::Input,caption);ImGui::PopID();
    ImGui::EndGroup();observe(caption);return changed;
}
#define QPX_UI_FIELD(Name) template<class... Args> bool Name(const char* label,Args&&... args) {return field(label,[&](const char* id){return ImGui::Name(id,std::forward<Args>(args)...);});}
QPX_UI_FIELD(InputText)
QPX_UI_FIELD(Combo)
QPX_UI_FIELD(SliderFloat)
#undef QPX_UI_FIELD
template<class... Args> bool scalar_value(const char* id,Args&&... args) {return ImGui::InputScalar(id,std::forward<Args>(args)...);}
inline thread_local float number_width=design::metrics.number_width;
// Rare longer formats opt in locally; a generic full-width field setting does
// not turn every number in the section into a window-sized control.
struct NumberWidth {
    float previous=number_width;
    explicit NumberWidth(float logical){number_width=std::max(1.f,logical);}
    ~NumberWidth(){number_width=previous;}
    NumberWidth(const NumberWidth&)=delete;NumberWidth& operator=(const NumberWidth&)=delete;
};
inline float prepare_number(const char* id) {
    const auto* end=label_end(id);const float label=end==id?0:ImGui::CalcTextSize(id,end).x+ImGui::GetStyle().ItemInnerSpacing.x;
    const float width=std::max(1.f,std::min({number_width*design::scale(),available()-label,ImGui::CalcItemWidth()}));
    ImGui::SetNextItemWidth(width);return width;
}
inline bool integer_value(const char* id,int* value,int step=1,int fast=100,ImGuiInputTextFlags flags=0) {
    const float width=prepare_number(id);return ImGui::InputInt(id,value,width<3*ImGui::GetFrameHeight()?0:step,fast,flags);
}
inline bool double_value(const char* id,double* value,double step=0,double fast=0,const char* format="%.6f",ImGuiInputTextFlags flags=0) {
    const float width=prepare_number(id);return ImGui::InputDouble(id,value,width<3*ImGui::GetFrameHeight()?0:step,fast,format,flags);
}
template<class... Args> bool number_value(const char* id,Args&&... args) {
    prepare_number(id);return scalar_value(id,std::forward<Args>(args)...);
}
template<class... Args> bool InputScalar(const char* label,Args&&... args) {
    return field(label,[&](const char* id){return number_value(id,std::forward<Args>(args)...);},FieldPlacement::Column,false,number_width*design::scale());
}
template<class... Args> bool InputTextWithHint(const char* label,const char* hint,Args&&... args) {
    return field(label,[&](const char* id){return ImGui::InputTextWithHint(id,i18n::text(hint),std::forward<Args>(args)...);});
}
// Optional observer of the index actually handed to the native combo.
inline thread_local std::function<void(const char*,int,int)> combo_observer;
inline bool Combo(const char* label,int* current,const char* items,int height=-1) {
    std::string localized;int count=0;
    for(const char* item=items;*item;item+=std::strlen(item)+1) {++count;localized+=i18n::text(item);localized+='\0';}
    if(combo_observer)combo_observer(label,*current,count);
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
    },FieldPlacement::Column);
}
inline bool color_value3(const char* id,float* values,ImGuiColorEditFlags flags=0) {
    const float rgb_width=3*(ImGui::CalcTextSize("000").x+2*ImGui::GetStyle().FramePadding.x)+ImGui::GetFrameHeight()+3*ImGui::GetStyle().ItemInnerSpacing.x;
    if(!(flags&ImGuiColorEditFlags_DisplayMask_) && ImGui::CalcItemWidth()<rgb_width)flags|=ImGuiColorEditFlags_DisplayHex;
    return ImGui::ColorEdit3(id,values,flags);
}
inline bool ColorEdit3(const char* label,float* values,ImGuiColorEditFlags flags=0,bool literal=false) {
    return field(label,[&](const char* id){return color_value3(id,values,flags);},FieldPlacement::Column,literal);
}
inline bool InputDouble(const char* label,double* value,double step=0,double fast=0,const char* format="%.6f",ImGuiInputTextFlags flags=0,bool literal=false) {
    return field(label,[&](const char* id){return double_value(id,value,step,fast,format,flags);},FieldPlacement::Column,literal,number_width*design::scale());
}
inline bool InputInt(const char* label,int* value,int step=1,int fast=100,ImGuiInputTextFlags flags=0) {
    return field(label,[&](const char* id){return integer_value(id,value,step,fast,flags);},FieldPlacement::Column,false,number_width*design::scale());
}
inline bool InputInt64(const char* label,std::int64_t* value) {
    return field(label,[&](const char* id){return number_value(id,ImGuiDataType_S64,value);},FieldPlacement::Column,false,number_width*design::scale());
}
// An open combo owns the parent ID scope until its popup window is restored.
class ComboScope {
    bool open_=false;
    std::string original_;
public:
    ComboScope(const char* label,const char* preview,ImGuiComboFlags flags=0) {
    if(ImGui::GetCurrentWindow()->SkipItems) return;
    const auto* original=label;if(layout_trace)original_=label;
    const auto localized=i18n::label(label);label=localized.c_str();
    const float desired=std::max(1.f,ImGui::CalcItemWidth());
    const auto* end=label_end(label);
    const float width=prepare_field(label,end,desired,FieldPlacement::Column);
    ImGui::PushID(original);ImGui::SetNextItemWidth(width);
    open_=ImGui::BeginCombo("##value",preview,flags);
    if(!open_) {observe();trace_layout(LayoutKind::Input,original);ImGui::PopID();}
    }
    ~ComboScope() {if(open_) {ImGui::EndCombo();observe();trace_layout(LayoutKind::Input,original_.c_str());ImGui::PopID();}}
    ComboScope(const ComboScope&)=delete;
    ComboScope& operator=(const ComboScope&)=delete;
    explicit operator bool() const {return open_;}
};
class LegacyComboScope {
    bool open_=false;
    std::string caption_;
public:
    LegacyComboScope(const char* caption,const char* original,const char* preview,bool literal=false) {
        if(ImGui::GetCurrentWindow()->SkipItems)return;
        if(layout_trace)caption_=caption;const auto id=ImGui::GetID(original);const auto localized=literal?std::string(caption):i18n::label(caption);
        ImGui::SetNextItemWidth(prepare_field(localized.c_str(),label_end(localized.c_str()),available(),FieldPlacement::Column));
        ImGui::PushOverrideID(id);open_=ImGui::BeginCombo("",preview);
        if(!open_){trace_layout(LayoutKind::Input,caption);ImGui::PopID();}
    }
    ~LegacyComboScope(){if(open_){ImGui::EndCombo();trace_layout(LayoutKind::Input,caption_.c_str());ImGui::PopID();}}
    explicit operator bool() const{return open_;}
    LegacyComboScope(const LegacyComboScope&)=delete;
    LegacyComboScope& operator=(const LegacyComboScope&)=delete;
};

inline bool Button(const char* label,ImVec2 size={0,0},design::Action action=design::Action::Secondary) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    design::ActionStyle recipe(action);
    const float height=design::compact_actions?design::metrics.compact_height:
        (ImGui::GetCurrentWindow()->Flags&ImGuiWindowFlags_Modal)?design::metrics.dialog_action_height:design::metrics.action_height;
    size.y=std::max(size.y,height*design::scale());
    const auto localized=i18n::label(label);label=localized.c_str();
    const auto* end=label_end(label);const auto padding=ImGui::GetStyle().FramePadding;
    const float minimum=action==design::Action::Ghost || design::compact_actions?0:design::metrics.button_min_width*design::scale();
    const float caption_width=ImGui::CalcTextSize(label,end).x+2*padding.x;
    const float natural=std::max(minimum,caption_width);
    if(ImGui::GetCurrentWindow()->DC.IsSameLine && (size.x>0?size.x:natural)>available()) ImGui::NewLine();
    const float width=std::min(size.x>0?size.x:natural,available());
    if(caption_width<=width+.1f) {const bool pressed=ImGui::Button(label,{width,size.y});observe(label);trace_layout(LayoutKind::TextButton,label);return pressed;}
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
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{lo.x+padding.x,lo.y+(hi.y-lo.y-text_size.y)/2},ImGui::GetColorU32(ImGuiCol_Text),label,end,text_width,&clip);
    observe(label);trace_layout(LayoutKind::TextButton,label);return pressed;
}
inline bool SmallButton(const char* label) {
    design::CompactActions compact;
    auto padding=ImGui::GetStyle().FramePadding;padding.y=0;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);const bool pressed=Button(label);ImGui::PopStyleVar();return pressed;
}
inline bool PrimaryButton(const char* label,ImVec2 size={0,0}) {return Button(label,size,design::Action::Primary);}
inline bool ToggleButton(const char* label,ImVec2 size,design::Action role) {
    const bool hit=Button(label,size,role);
    // A labelled boolean field may use a chip instead of a checkbox.
    if(layout_trace && !layout_trace->items.empty())layout_trace->items.back().kind=LayoutKind::Input;
    return hit;
}
inline bool DangerButton(const char* label,ImVec2 size={0,0}) {return Button(label,size,design::Action::Danger);}
inline bool GhostButton(const char* label,ImVec2 size={0,0}) {return Button(label,size,design::Action::Ghost);}
// The hidden ID hashes to the original English caption, as the text button did.
// Language, tooltip and presentation changes therefore preserve focus/replay IDs.
inline bool icon_button(const char* label,Icon icon,design::Action action,const char* shortcut,bool selected,bool reveal,float row_height=0) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const float side=design::metrics.icon_size*design::scale();
    if(ImGui::GetCurrentWindow()->DC.IsSameLine && available()<side) ImGui::NewLine();
    const auto* stable=std::strstr(label,"###");const std::string id=std::string("###")+(stable?stable+3:label);
    design::ActionStyle recipe(action);
    if(selected) {ImGui::PushStyleColor(ImGuiCol_Button,design::color(design::colors().bg_hover));ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,design::metrics.border*design::scale());}
    // SameLine inherits the catalog's row height; center the square target in
    // that line without creating a tall, separately shaded action strip.
    if(row_height>side)ImGui::SetCursorPosY(ImGui::GetCursorPosY()+(row_height-side)*.5f);
    const bool pressed=ImGui::Button(id.c_str(),{std::min(side,available()),side});
    if(selected) {ImGui::PopStyleVar();ImGui::PopStyleColor();}
    const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
    const bool emphasized=selected || ImGui::IsItemHovered() || ImGui::IsItemFocused() || ImGui::IsItemActive();
    const auto ink=action==design::Action::Ghost && emphasized?ImGui::GetColorU32(design::color(design::colors().text_primary)):ImGui::GetColorU32(ImGuiCol_Text);
    const float inset=(design::metrics.icon_size-design::metrics.icon_canvas)*.5f*design::scale();
    if(reveal || emphasized)draw_icon(icon,{lo.x+inset,lo.y+inset},{hi.x-inset,hi.y-inset},ink);
    observe(label);
    trace_layout(LayoutKind::Icon,label,icon);
    trace_layout_command(stable?stable+3:label);
    const bool focused=ImGui::IsItemFocused();
    if(focused || ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip|ImGuiHoveredFlags_AllowWhenDisabled)) {
        const std::string caption(label,label_end(label));
        std::string hint=i18n::text(caption.c_str());if(*shortcut) hint+=" ("+std::string(shortcut)+")";
        if((ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled) && icon_availability.reason)hint+="\n"+std::string(i18n::text(icon_availability.reason));
        if(focused) {
            const auto* vp=ImGui::GetMainViewport();const auto text=ImGui::CalcTextSize(hint.c_str()),padding=ImGui::GetStyle().WindowPadding;
            ImGui::SetNextWindowPos({std::clamp(lo.x,vp->WorkPos.x,std::max(vp->WorkPos.x,vp->WorkPos.x+vp->WorkSize.x-text.x-2*padding.x)),
                std::clamp(hi.y+4*design::scale(),vp->WorkPos.y,std::max(vp->WorkPos.y,vp->WorkPos.y+vp->WorkSize.y-text.y-2*padding.y))});
        }
        ImGui::SetTooltip("%s",hint.c_str());
    }
    return pressed;
}
inline bool IconButton(const char* label,Icon icon,design::Action action=design::Action::Ghost,const char* shortcut="",bool selected=false) {
    return icon_button(label,icon,action,shortcut,selected,true);
}
inline bool IconButton(const char* label,Icon icon,Availability availability,const char* shortcut="") {
    ImGui::BeginDisabled(!availability.enabled);
    const bool hit=icon_button(label,icon,design::Action::Ghost,shortcut,false,true);
    if(!availability.enabled && availability.reason && (ImGui::IsItemFocused() || ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip|ImGuiHoveredFlags_AllowWhenDisabled))) {
        const std::string caption(label,label_end(label));std::string hint=i18n::text(caption.c_str());
        if(*shortcut)hint+=" ("+std::string(shortcut)+")";
        hint+="\n";hint+=i18n::text(availability.reason);ImGui::SetTooltip("%s",hint.c_str());
    }
    ImGui::EndDisabled();return hit;
}
inline bool RowIconButton(const char* label,Icon icon,bool reveal,float height) {
    return icon_button(label,icon,design::Action::Ghost,"",false,reveal,height);
}
inline bool ActivityButton(const char* label,Icon icon,bool selected,const char* shortcut="") {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const float side=design::metrics.activity_row*design::scale(),box=design::metrics.activity_icon_box*design::scale();
    const auto* stable=std::strstr(label,"###");const std::string id=std::string("###")+(stable?stable+3:label);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    for(auto color:{ImGuiCol_Button,ImGuiCol_ButtonHovered,ImGuiCol_ButtonActive}) ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
    const bool pressed=ImGui::Button(id.c_str(),{design::metrics.activity_width*design::scale(),side});
    ImGui::PopStyleColor(3);ImGui::PopStyleVar(2);
    const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
    const bool hovered=ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    const auto ink=ImGui::GetColorU32(design::color(selected || hovered || ImGui::IsItemFocused()?design::colors().text_primary:design::colors().text_muted));
    draw_icon_to(ImGui::GetWindowDrawList(),icon,{lo.x+(hi.x-lo.x-box)/2,lo.y+(side-box)/2},{lo.x+(hi.x-lo.x+box)/2,lo.y+(side+box)/2},ink);
    if(selected) ImGui::GetWindowDrawList()->AddRectFilled(lo,{lo.x+design::metrics.activity_indicator*design::scale(),hi.y},ImGui::GetColorU32(design::color(design::colors().text_primary)));
    if(ImGui::IsItemFocused() || ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip|ImGuiHoveredFlags_AllowWhenDisabled)) {
        const std::string caption(label,label_end(label));std::string hint=i18n::text(caption.c_str());if(*shortcut)hint+=" ("+std::string(shortcut)+")";
        if((ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled) && icon_availability.reason)hint+="\n"+std::string(i18n::text(icon_availability.reason));
        ImGui::SetTooltip("%s",hint.c_str());
    }
    observe(label);trace_layout(LayoutKind::Palette,label,icon);return pressed;
}
inline bool Checkbox(const char* label,bool* value) {
    if(ImGui::GetCurrentWindow()->SkipItems) return false;
    const auto* original=label;const auto localized=i18n::label(label);label=localized.c_str();
    const auto* end=label_end(label);
    if(ImGui::GetCurrentWindow()->DC.IsSameLine && available()<ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+std::min(120.f,ImGui::CalcTextSize(label,end).x)) ImGui::NewLine();
    auto padding=ImGui::GetStyle().FramePadding;padding.y=std::max(0.f,(design::metrics.checkbox_size*design::scale()-ImGui::GetFontSize())/2);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);
    ImGui::BeginGroup();ImGui::PushID(original);const auto id=ImGui::GetID("##value");
    bool changed=ImGui::Checkbox("##value",value);
    if(end!=label) {
        ImGui::SameLine(0,ImGui::GetStyle().ItemInnerSpacing.x);
        if(ImGui::CalcTextSize(label,end).x>available()) ++layout_stats.stacked_fields;
        label_text(label,end);
        if(!changed && ImGui::IsItemClicked()) {*value=!*value;changed=true;ImGui::MarkItemEdited(id);}
    }
    ImGui::PopID();ImGui::EndGroup();ImGui::PopStyleVar();observe(label);trace_layout(LayoutKind::Input,original);return changed;
}
inline float CollectionHeight(std::size_t rows) {
    const float row=ImGui::GetTextLineHeightWithSpacing();
    const float visible=rows?std::clamp(float(rows),1.f,8.f):2.f;
    const float desired=visible*row+2*ImGui::GetStyle().FramePadding.y;
    return std::min(desired,std::max(std::min(visible,3.f)*row,ImGui::GetContentRegionAvail().y*.35f));
}
inline bool BeginCollection(const char* id,std::size_t rows) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
    const bool visible=ImGui::BeginChild(id,{0,CollectionHeight(rows)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);return visible;
}
inline bool CatalogEntry(const char* label,bool selected,const char* status_caption=nullptr,ImGuiSelectableFlags flags=0,float actions_width=0,float leading_width=0,float row_height=0) {
    if(ImGui::GetCurrentWindow()->SkipItems)return false;
    const auto at=ImGui::GetCursorScreenPos();const float width=std::max(1.f,available()-actions_width),height=std::max(design::metrics.control_height*design::scale(),row_height);
    if(actions_width>0) {
        const bool hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(at,{at.x+available(),at.y+height});
        if(selected || hovered) {
            const auto color=hovered?(ImGui::IsMouseDown(ImGuiMouseButton_Left)?ImGuiCol_HeaderActive:ImGuiCol_HeaderHovered):ImGuiCol_Header;
            ImGui::GetWindowDrawList()->AddRectFilled(at,{at.x+available(),at.y+height},ImGui::GetColorU32(color));
        }
        // Keep the row's action slots inside its background, with independent
        // hit targets. The Selectable still owns keyboard focus/navigation.
        for(const auto color:{ImGuiCol_Header,ImGuiCol_HeaderHovered,ImGuiCol_HeaderActive})ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
    }
    const bool pressed=ImGui::Selectable("##Catalog entry",selected,flags|ImGuiSelectableFlags_NoPadWithHalfSpacing,{width,height});
    if(actions_width>0)ImGui::PopStyleColor(3);
    const float trailing=status_caption?ImGui::CalcTextSize(status_caption).x+design::spacing.md*design::scale():0;
    const float y=at.y+(height-ImGui::GetFontSize())*.5f;
    const ImVec4 clip{at.x,at.y,at.x+std::max(1.f,width-trailing),at.y+height};
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{at.x+leading_width,y},ImGui::GetColorU32(ImGuiCol_Text),label,nullptr,0,&clip);
    if(status_caption)ImGui::GetWindowDrawList()->AddText({at.x+width-trailing+design::spacing.sm*design::scale(),y},ImGui::GetColorU32(ImGuiCol_TextDisabled),status_caption);
    observe(label);trace_layout(LayoutKind::Row,label);return pressed;
}
enum class ToolHeight {Preferred,Content};
inline void SetNextToolWindow(const char* name,ImVec2 preferred,ToolHeight height=ToolHeight::Preferred) {
    const auto* vp=ImGui::GetMainViewport();const float scale=design::scale();
    const float margin=std::min(12.f*scale,std::min(vp->WorkSize.x,vp->WorkSize.y)*.05f);
    const ImVec2 origin{vp->WorkPos.x+margin,vp->WorkPos.y+margin};
    const ImVec2 extent{std::max(1.f,vp->WorkSize.x-2*margin),std::max(1.f,vp->WorkSize.y-2*margin)};
    const ImVec2 minimum{std::min(260.f*scale,extent.x),std::min(160.f*scale,extent.y)};
    // Zero height asks ImGui to fit the first two frames; it does not force
    // auto-resize later or replace persisted/manual/docked dimensions.
    ImGui::SetNextWindowSize({std::min(preferred.x*scale,extent.x),height==ToolHeight::Content?0:std::min(preferred.y*scale,extent.y)},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({origin.x+extent.x*.5f,origin.y+extent.y*.5f},ImGuiCond_FirstUseEver,{.5f,.5f});
    ImGui::SetNextWindowSizeConstraints(minimum,extent);
    // Localized titles already carry the same stable ImGui ID as their source.
    if(auto* window=ImGui::FindWindowByName(name);window && !window->DockNode) {
        const ImVec2 size{std::clamp(window->SizeFull.x,minimum.x,extent.x),std::clamp(window->SizeFull.y,minimum.y,extent.y)};
        const ImVec2 pos{std::clamp(window->Pos.x,origin.x,origin.x+extent.x-size.x),std::clamp(window->Pos.y,origin.y,origin.y+extent.y-size.y)};
        if(pos.x!=window->Pos.x || pos.y!=window->Pos.y) ImGui::SetNextWindowPos(pos);
    }
}
inline void TextDisabled(const char* format,...) {
    design::Caption caption;
    va_list args;va_start(args,format);ImGui::PushTextWrapPos(0);ImGui::TextDisabledV(i18n::text(format),args);ImGui::PopTextWrapPos();va_end(args);trace_layout(LayoutKind::Caption,format);
}
inline void FieldHelp(const char* format,...) {
    const float room=available(),gap=design::spacing.sm*design::scale();
    const float column=std::min(design::metrics.label_width*design::scale(),room*.4f);
    const float indent=room-column-gap<design::metrics.field_min_width*design::scale()?0:column+gap;
    if(indent)ImGui::Indent(indent);
    {design::Caption caption;va_list args;va_start(args,format);ImGui::PushTextWrapPos(0);ImGui::TextDisabledV(i18n::text(format),args);ImGui::PopTextWrapPos();va_end(args);trace_layout(LayoutKind::Caption,format);}
    if(indent)ImGui::Unindent(indent);
}
inline void SeparatorText(const char* label) {
    if(ImGui::GetCurrentWindow()->SkipItems) return;
    design::Heading title;
    label=i18n::text(label);
    if(ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().SeparatorTextPadding.x<=available()) ImGui::SeparatorText(label);
    else {ImGui::Separator();label_text(label,label_end(label));}
    trace_layout(LayoutKind::Separator,label);
}
inline void Separator() {ImGui::Separator();trace_layout(LayoutKind::Separator);}
inline void PaneTitle(const char* label) {
    if(ImGui::GetCurrentWindow()->SkipItems) return;
    design::Heading title;ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(i18n::text(label));trace_layout(LayoutKind::Caption,label);
}
// One caption and one right-aligned command strip keep collection scope visible.
class CaptionedStrip {
    ActionRow row_;
public:
    CaptionedStrip(const char* caption,int actions,float trailing_inset=0,bool destructive_group=false):row_(actions,trailing_inset,destructive_group) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row_.field_width());PaneTitle(caption);ImGui::PopTextWrapPos();
    }
    void next(){row_.next();}
    void destructive(){row_.destructive();}
};
// A horizontal separator with a generous resize target; fraction owns the
// lower pane's share. Both panes keep their own scrolling and content padding.
inline bool ResizeDivider(const char* id,float& fraction,float total,float minimum_lower,float minimum_upper) {
    const float height=design::spacing.sm*design::scale();const auto at=ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id,{available(),height});
    const bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();
    if(hovered || active)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if(active && total>0)fraction=std::clamp(fraction-ImGui::GetIO().MouseDelta.y/total,std::max(.2f,minimum_lower/total),std::min(.98f,1-minimum_upper/total));
    const auto color=active?ImGuiCol_SeparatorActive:hovered?ImGuiCol_SeparatorHovered:ImGuiCol_Separator;
    ImGui::GetWindowDrawList()->AddLine({at.x,at.y+height*.5f},{at.x+ImGui::GetItemRectSize().x,at.y+height*.5f},ImGui::GetColorU32(color),design::scale());
    return active;
}
struct PopupMenuRow {
    bool vertical=ImGui::GetCurrentWindow()->DC.LayoutType==ImGuiLayoutType_Vertical;
    PopupMenuRow() {if(vertical)ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{ImGui::GetStyle().ItemSpacing.x,std::max(ImGui::GetStyle().ItemSpacing.y,design::metrics.menu_row_height*design::scale()-ImGui::GetFontSize())});}
    ~PopupMenuRow(){if(vertical)ImGui::PopStyleVar();}
};
inline bool BeginMenu(const char* label,bool enabled=true) {PopupMenuRow row;const bool open=ImGui::BeginMenu(i18n::label(label).c_str(),enabled);if(!open)trace_layout(LayoutKind::Caption,label);return open;}
inline bool MenuItem(const char* label,const char* shortcut=nullptr,bool selected=false,bool enabled=true,bool literal=false) {
    PopupMenuRow row;
    auto caption=literal?std::string(label):i18n::label(label);std::string inferred;
    // Legacy captions include " (F9)" or " (Ctrl+O)". Put those shortcuts in
    // the shared trailing column while retaining the original command ID.
    if(!literal && (!shortcut || !*shortcut)) {
        const std::string original(label,label_end(label));const auto at=original.rfind(" (");
        if(at!=std::string::npos && original.back()==')') {
            const auto key=original.substr(at+2,original.size()-at-3);
            const bool function=key.size()>1 && key[0]=='F' && std::all_of(key.begin()+1,key.end(),[](char c){return c>='0' && c<='9';});
            if(function || key.rfind("Ctrl+",0)==0 || key.rfind("Alt+",0)==0 || key.rfind("Shift+",0)==0) {
                const std::string visible(caption.c_str(),label_end(caption.c_str())),suffix=" ("+key+")";
                if(visible.size()>=suffix.size() && visible.compare(visible.size()-suffix.size(),suffix.size(),suffix)==0) {
                    const auto* stable=std::strstr(label,"###");caption=visible.substr(0,visible.size()-suffix.size())+"###"+(stable?stable+3:label);inferred=key;shortcut=inferred.c_str();
                }
            }
        }
    }
    const bool hit=ImGui::MenuItem(caption.c_str(),shortcut,selected,enabled);trace_layout(LayoutKind::TextButton,label);return hit;
}
inline void PreparePopupMenu() {
    const auto* vp=ImGui::GetMainViewport();const float s=design::scale();
    ImGui::SetNextWindowSizeConstraints({std::min(180*s,std::max(1.f,vp->WorkSize.x-32*s)),0},{std::max(1.f,vp->WorkSize.x-32*s),std::max(1.f,vp->WorkSize.y-32*s)});
}
inline bool BeginPopupMenu(const char* id) {PreparePopupMenu();return ImGui::BeginPopup(id);}
inline bool BeginContextMenu(const char* id) {PreparePopupMenu();return ImGui::BeginPopupContextItem(id);}
inline bool CollapsingHeader(const char* label,ImGuiTreeNodeFlags flags=0) {
    design::Heading title;auto padding=ImGui::GetStyle().FramePadding;
    padding.y=std::max(0.f,(design::metrics.section_height*design::scale()-ImGui::GetFontSize())/2);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,padding);ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0,0,0,0));
    const bool open=ImGui::CollapsingHeader(i18n::label(label).c_str(),flags);
    ImGui::PopStyleColor();ImGui::PopStyleVar(3);trace_layout(LayoutKind::Separator,label);return open;
}
// Use these only for application-owned captions/formats. User-supplied strings
// continue to use TextUnformatted and never pass through the catalogue.
inline void Text(const char* format,...) {va_list args;va_start(args,format);ImGui::TextV(i18n::text(format),args);va_end(args);trace_layout(LayoutKind::Caption,format);}
inline void TextWrapped(const char* format,...) {va_list args;va_start(args,format);ImGui::TextWrappedV(i18n::text(format),args);va_end(args);trace_layout(LayoutKind::Caption,format);}
inline void TextUnformatted(const char* caption) {ImGui::TextUnformatted(i18n::text(caption));trace_layout(LayoutKind::Caption,caption);}
// Literal data stays outside the catalogue while retaining its drawn geometry.
inline void DataText(const char* caption) {ImGui::TextUnformatted(caption);trace_layout(LayoutKind::Caption,caption);}
inline void DataTextf(const char* format,...) {va_list args;va_start(args,format);ImGui::TextV(format,args);va_end(args);trace_layout(LayoutKind::Caption,format);}
inline void DataTextWrapped(const char* format,...) {va_list args;va_start(args,format);ImGui::TextWrappedV(format,args);va_end(args);trace_layout(LayoutKind::Caption,format);}
inline void DataTextDisabled(const char* format,...) {va_list args;va_start(args,format);ImGui::TextDisabledV(format,args);va_end(args);trace_layout(LayoutKind::Caption,format);}
inline bool RowCheckbox(const char* label,bool* value) {const bool hit=ImGui::Checkbox(label,value);trace_layout(LayoutKind::Row,label);observe(label);return hit;}
inline bool Selectable(const char* label,bool selected=false,ImGuiSelectableFlags flags=0,const ImVec2& size={0,0}) {
    const bool hit=ImGui::Selectable(label,selected,flags,size);trace_layout(LayoutKind::Row,label);return hit;
}
inline bool Selectable(const char* label,bool* selected,ImGuiSelectableFlags flags=0,const ImVec2& size={0,0}) {
    const bool hit=ImGui::Selectable(label,selected,flags,size);trace_layout(LayoutKind::Row,label);return hit;
}
}
