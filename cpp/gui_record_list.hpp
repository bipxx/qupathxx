#pragma once
#include "gui_result_state.hpp"
#include "gui_layout.hpp"
#include "qupathxx/geometry.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <limits>
#include <optional>
#include <vector>

namespace qpx::ui {
// IDs are native numeric IDs, or one-based indices into an owned string-ID
// snapshot. The latter are UI selection handles, never persisted identities.
struct RecordRow {
    std::uint64_t id=0;std::string label,kind;std::optional<RecordState> state;
    std::string raw,identifier,detail;
};
struct RecordListActions {
    std::function<void(std::uint64_t)> use,preview,recheck,remove;
    // Existing action IDs, enabled predicates and replay marks stay window-owned.
    std::function<void()> mark;
    std::function<void(std::uint64_t)> mark_row;
    std::function<std::string(std::uint64_t)> status;
};
struct RecordListItem {RecordRow row;Rect bounds;ImGuiID widget=0;std::string second_line;bool highlighted=false;};
struct RecordListPresentation {bool box=false;std::size_t count=0;float row_height=0,box_height=0;bool none_current=false;std::string heading;};
inline thread_local RecordListPresentation* record_list_presentation=nullptr;
inline thread_local std::vector<RecordListPresentation>* record_list_presentations=nullptr;
inline thread_local std::vector<RecordListItem>* record_list_trace=nullptr;
inline std::string short_record_id(const std::string& id) {
    return id.size()==64 && std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');})?id.substr(0,8):id;
}
// All record enumeration/status lookup happens only on native invalidation.
class RecordRows {
    const void* owner_=nullptr;const void* source_=nullptr;std::uint64_t revision_=~0ULL,extra_=~0ULL;int language_=-1;
public:
    std::vector<RecordRow> rows;
    bool two_lines=false;
    template<class Build>void refresh(const void* owner,std::uint64_t revision,std::uint64_t extra,Build build,const void* source=nullptr) {
        if(owner_==owner && revision_==revision && extra_==extra && source_==source && language_==i18n::state.effective)return;
        owner_=owner;revision_=revision;extra_=extra;source_=source;language_=i18n::state.effective;rows.clear();build(rows);
        two_lines=std::any_of(rows.begin(),rows.end(),[](const auto& row){return row.state.has_value() || !row.kind.empty() || !row.detail.empty();});
    }
};
inline std::uint64_t draw_record_list(const char* id,const RecordRows& records,std::uint64_t selected,const RecordListActions& actions={},bool informational=false,const char* none_choice=nullptr) {
    LayoutRegion region(id);
    const auto& rows=records.rows;
    std::string heading=id;const auto end=heading.find("##");if(end!=std::string::npos)heading.resize(end);
    if(heading.empty())heading="Kept results";
    TextWrapped("%s",i18n::text(heading.c_str()));
    const auto hidden=id[0]=='#'?std::string(id):"##"+std::string(id);
    const auto height=records.two_lines?2*ImGui::GetTextLineHeight()+ImGui::GetStyle().ItemSpacing.y:ImGui::GetTextLineHeight();
    const auto visible=std::min<std::size_t>(3,rows.size());
    const auto list_height=visible*height+(visible?visible-1:0)*ImGui::GetStyle().ItemSpacing.y+2*ImGui::GetStyle().FramePadding.y;
    if(informational)selected=0;
    if(none_choice && ui::Selectable(i18n::label(none_choice).c_str(),selected==0))selected=0;
    if(record_list_presentation || record_list_presentations) {
        const RecordListPresentation presentation{!rows.empty(),rows.size(),height,list_height,none_choice && selected==0,heading};
        if(record_list_presentation)*record_list_presentation=presentation;
        if(record_list_presentations)record_list_presentations->push_back(presentation);
    }
    if(rows.empty())TextWrapped("No kept records. Run, then Keep or Apply.");
    else if(ImGui::BeginListBox(hidden.c_str(),{-1,list_height})) {
        ImGuiListClipper clip;clip.Begin(static_cast<int>(rows.size()),height+ImGui::GetStyle().ItemSpacing.y);
        while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
            const auto& row=rows[std::size_t(i)];ImGui::PushID(std::to_string(row.id).c_str());
            const auto raw=actions.status?actions.status(row.id):row.raw;
            const auto status=actions.status?caption(parse_record_state(raw),raw):row.state?caption(*row.state,raw):std::string{};
            const auto short_id=row.identifier.empty()?std::to_string(row.id):short_record_id(row.identifier);
            const auto suffix=" #"+short_id;std::string name=row.label;
            if(name.size()>=suffix.size() && name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0)name.resize(name.size()-suffix.size());
            std::string state_line=row.detail;
            if(!row.kind.empty()){if(!state_line.empty())state_line+=" / ";state_line+=i18n::text(row.kind.c_str());}
            if(!status.empty()){if(!state_line.empty())state_line+=" / ";state_line+=status;}
            const auto at=ImGui::GetCursorScreenPos();const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
            if(ui::Selectable("##Catalog entry",!informational && row.id==selected,ImGuiSelectableFlags_NoPadWithHalfSpacing,{width,height}) && !informational)selected=row.id;
            const auto id_width=ImGui::CalcTextSize(suffix.c_str()).x;
            const ImVec4 name_clip{at.x,at.y,at.x+std::max(1.f,width-id_width-ImGui::GetStyle().ItemInnerSpacing.x),at.y+ImGui::GetTextLineHeight()};
            const ImVec4 row_clip{at.x,at.y,at.x+width,at.y+height};auto* draw=ImGui::GetWindowDrawList();
            draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),at,ImGui::GetColorU32(ImGuiCol_Text),name.c_str(),nullptr,0,&name_clip);
            draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{at.x+width-id_width,at.y},ImGui::GetColorU32(ImGuiCol_Text),suffix.c_str(),nullptr,0,&row_clip);
            if(records.two_lines)draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{at.x,at.y+ImGui::GetTextLineHeightWithSpacing()},ImGui::GetColorU32(ImGuiCol_TextDisabled),state_line.c_str(),nullptr,0,&row_clip);
            observe(row.label.c_str());
            if(record_list_trace){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();record_list_trace->push_back({row,{a.x,a.y,b.x-a.x,b.y-a.y},ImGui::GetItemID(),state_line,!informational && row.id==selected});}
            if(actions.mark_row)actions.mark_row(row.id);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s",row.label.c_str(),state_line.c_str());
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    if(actions.mark)actions.mark();
    constexpr const char* labels[]={"Use","Preview","Recheck","Remove"};
    const std::function<void(std::uint64_t)>* callbacks[]={&actions.use,&actions.preview,&actions.recheck,&actions.remove};
    for(int i=0;i<4;++i)if(*callbacks[i] && Button(labels[i]))(*callbacks[i])(selected);
    return selected;
}
inline RecordRow record_row(std::uint64_t id,const std::string& name,const char* family) {
    return {id,(name.empty()?std::string(i18n::text(family)):name)+" #"+std::to_string(id),"",std::nullopt,{},std::to_string(id),{}};
}
inline RecordRow record_row(std::uint64_t id,const std::string& name,const char* family,const std::string& raw) {
    auto row=record_row(id,name,family);row.state=parse_record_state(raw);row.raw=raw;return row;
}
// Called by the family's existing event-driven snapshot refresh.
inline void string_record_rows(std::vector<RecordRow>& rows,const std::vector<std::string>& ids,const char* family) {
    rows.clear();for(const auto& id:ids)rows.push_back({rows.size()+1,std::string(i18n::text(family))+" #"+short_record_id(id),"",std::nullopt,{},id,{}});
}
inline void record_identifier_details(const std::string& identifier) {
    ImGui::PushID(identifier.c_str());
    if(!identifier.empty() && CollapsingHeader("Details")) {
        TextWrapped("%s",identifier.c_str());if(Button("Copy identifier"))ImGui::SetClipboardText(identifier.c_str());
    }
    ImGui::PopID();
}
class StringRecordRows {
    RecordRows cache_;std::map<std::string,std::uint64_t> handles_;
public:
    std::string draw(const char* label,const std::vector<std::string>& ids,const std::string& selected,const void* owner,std::uint64_t revision,const char* family,const RecordListActions& actions={},const char* none_choice=nullptr) {
        cache_.refresh(owner,revision,ids.size(),[&](auto& rows){string_record_rows(rows,ids,family);handles_.clear();for(const auto& row:rows)handles_.emplace(row.identifier,row.id);});
        const auto found=handles_.find(selected);
        // An unlisted nonempty identity is unresolved, never the explicit None choice.
        const auto handle=selected.empty()?0:found==handles_.end()?std::numeric_limits<std::uint64_t>::max():found->second;
        const auto next=draw_record_list(label,cache_,handle,actions,false,none_choice);
        const auto value=next && next<=cache_.rows.size()?cache_.rows[std::size_t(next-1)].identifier:next==0 && none_choice?std::string{}:selected;
        record_identifier_details(value);return value;
    }
};
}
