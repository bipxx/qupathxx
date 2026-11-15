#pragma once
#include "qupathxx/core.hpp"
#include <array>
#include <algorithm>
#include <functional>
#include <imgui_internal.h>
#include <string_view>

namespace qpx::detail {
template<class T> struct Draft {T value{},baseline{};
    bool dirty() const {return !(value==baseline);}
    void rebase(const T& current) {if(!dirty())value=current;baseline=current;}
    void reset(const T& current) {value=baseline=current;}};

template<std::size_t N> struct DraftText {
    std::array<char,N> bytes{};
    char* data(){return bytes.data();}const char* data() const{return bytes.data();}
    std::size_t size() const{return N;}void fill(char c){bytes.fill(c);}
    char& operator[](std::size_t i){return bytes[i];}
    bool operator==(const DraftText& b) const {return std::string_view(data())==b.data();}
};
struct DraftTextOwner {
    std::vector<ImGuiID> inputs;
    void observe() {
        const auto id=ImGui::GetActiveID();
        if(ImGui::IsItemActive() && ImGui::GetInputTextState(id) && std::find(inputs.begin(),inputs.end(),id)==inputs.end())inputs.push_back(id);
    }
    bool owns(ImGuiID id) const {return id && std::find(inputs.begin(),inputs.end(),id)!=inputs.end();}
};
inline void retire_draft_text(DraftTextOwner& owner) {
    auto& g=*ImGui::GetCurrentContext();
    // A press may already have moved focus before this editor retires. Keep
    // ownership through deactivation, and leave other fields' final writes alone.
    if(owner.owns(g.ActiveId))ImGui::ClearActiveID();
    if(owner.owns(g.InputTextDeactivatedState.ID))g.InputTextDeactivatedState.ID=0;
    owner.inputs.clear();
}

inline bool same_bounds(const Rect& a,const Rect& b) {
    return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height;
}
struct GeometryDraft {
    Rect bounds{};std::uint64_t parent=0;
    bool operator==(const GeometryDraft& b) const {return same_bounds(bounds,b.bounds) && parent==b.parent;}
    Roi apply(const Roi& current) const {
        const auto at=current.bounds();
        return current.kind()==RoiKind::Rectangle?Roi::rectangle(bounds):
            current.kind()==RoiKind::Ellipse?Roi::ellipse(bounds):current.translated(bounds.x-at.x,bounds.y-at.y);
    }
};
inline void rebase_geometry(Draft<GeometryDraft>& draft,const GeometryDraft& current) {
    // Only typed components are pending. Canvas changes to other components
    // remain current when a position-only edit is applied.
    auto& v=draft.value;const auto& b=draft.baseline;
    if(v.bounds.x==b.bounds.x)v.bounds.x=current.bounds.x;
    if(v.bounds.y==b.bounds.y)v.bounds.y=current.bounds.y;
    if(v.bounds.width==b.bounds.width)v.bounds.width=current.bounds.width;
    if(v.bounds.height==b.bounds.height)v.bounds.height=current.bounds.height;
    if(v.parent==b.parent)v.parent=current.parent;
    draft.rebase(current);
}
struct AppearanceDraft {
    bool color=false,probability=false;std::array<float,3> rgb{};double reported=0;
    std::optional<std::uint32_t> packed() const {
        const auto byte=[](float v){return std::uint32_t(std::clamp(v,0.f,1.f)*255+.5f);};
        return color?std::optional<std::uint32_t>((byte(rgb[0])<<16)|(byte(rgb[1])<<8)|byte(rgb[2])):std::nullopt;
    }
    std::optional<double> score() const {return probability?std::optional<double>(reported):std::nullopt;}
    bool operator==(const AppearanceDraft& b) const {return packed()==b.packed() && score()==b.score();}
    static AppearanceDraft from(const Annotation& a) {
        const auto rgb=a.color_rgb().value_or(0x41e1be);
        return {bool(a.color_rgb()),bool(a.class_probability()),{((rgb>>16)&255)/255.f,((rgb>>8)&255)/255.f,(rgb&255)/255.f},a.class_probability().value_or(0)};
    }
};
inline void rebase_appearance(Draft<AppearanceDraft>& draft,const AppearanceDraft& current) {
    auto& value=draft.value;const auto& baseline=draft.baseline;
    if(value.packed()==baseline.packed()) {value.color=current.color;value.rgb=current.rgb;}
    if(value.score()==baseline.score()) {value.probability=current.probability;value.reported=current.reported;}
    draft.rebase(current);
}
// Cache labels as well as IDs: opening and revisions are the only O(n) work.
struct ParentChoices {
    std::uint64_t revision=~std::uint64_t(0),object=0;
    std::uint64_t rebuilds=0;
    std::vector<std::pair<std::uint64_t,std::string>> rows;
    void rebuild(const Document& doc,std::uint64_t selected) {
        ++rebuilds;
        rows.clear();for(const auto& [id,a]:doc.objects())if(id!=selected && a.kind==ObjectKind::Annotation && a.geometry.is_area())
            rows.emplace_back(a.id,a.name+" (#"+std::to_string(a.id)+")");
        revision=doc.revision();object=selected;
    }
    std::size_t draw(std::uint64_t& selected) const {
        std::size_t visited=0;ImGuiListClipper clip;clip.Begin(static_cast<int>(rows.size()));
        while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
            ++visited;const auto& [id,label]=rows[i];ImGui::PushID(static_cast<int>(i));
            if(ui::Selectable("##parent",selected==id))selected=id;
            const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();auto* draw=ImGui::GetWindowDrawList();
            // Names are data: ## and ### must not hide a name or its native ID.
            draw->PushClipRect(lo,hi,true);draw->AddText(lo,ImGui::GetColorU32(ImGuiCol_Text),label.c_str(),label.c_str()+label.size());draw->PopClipRect();
            ImGui::PopID();
        }return visited;
    }
};
// Private frontend observations, following the workflow/command trace seams.
// No public replay option or native ABI depends on this session-only hook.
struct DraftEditorTrace {
    std::function<void(const ParentChoices&)> parent_choices;
    std::function<void(Draft<GeometryDraft>&,Draft<AppearanceDraft>&,ObjectSelection&,const char*,bool,bool)> after_editor;
};
inline thread_local DraftEditorTrace* draft_editor_trace=nullptr;
}
