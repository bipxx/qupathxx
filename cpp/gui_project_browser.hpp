#pragma once
#include "qupathxx/project.hpp"
#include "ascii_case.hpp"
#include "gui_layout.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace qpx::ui {
// Presentation over an owned catalog snapshot. Browsing never opens a document,
// changes the project's active image, or infers clinical metadata/status.
struct ProjectBrowser {
    static constexpr auto group_row=std::numeric_limits<std::size_t>::max();
    struct Row {std::size_t image=group_row;std::string group;std::size_t count=0;};
    std::uint64_t selected=0;
    std::string group_key;
    bool missing_only=false;
    std::set<std::string> collapsed;
    std::string previous_group;
    std::vector<Row> rows;
    std::vector<std::string> keys;
    std::size_t matched=0,missing=0;
    static std::string folded(std::string text) {detail::ascii_lower_in_place(text);return text;}
    void rebuild(const std::vector<ProjectImage>& catalog,const std::string& query) {
        missing=std::count_if(catalog.begin(),catalog.end(),[](const auto& image){return !image.available;});
        if(!missing)missing_only=false;
        std::set<std::string> unique_keys;
        for(const auto& image:catalog)for(const auto& entry:image.metadata)unique_keys.insert(entry.first);
        keys.assign(unique_keys.begin(),unique_keys.end());
        if(!group_key.empty() && !unique_keys.count(group_key))group_key.clear();
        if(previous_group!=group_key) {collapsed.clear();previous_group=group_key;}
        if(std::none_of(catalog.begin(),catalog.end(),[&](const auto& image){return image.id==selected;}))selected=catalog.empty()?0:catalog.front().id;
        const auto needle=folded(query);std::vector<std::size_t> matches;
        for(std::size_t i=0;i<catalog.size();++i) {
            const auto& image=catalog[i];if(missing_only && image.available)continue;
            std::string text=image.name+"\n"+image.source_path;
            for(const auto& entry:image.metadata)text+="\n"+entry.first+"\n"+entry.second;
            for(const auto& tag:image.tags)text+="\n"+tag;
            if(image.description)text+="\n"+*image.description;
            if(needle.empty() || folded(std::move(text)).find(needle)!=std::string::npos)matches.push_back(i);
        }
        matched=matches.size();rows.clear();
        if(group_key.empty()) {for(auto i:matches)rows.push_back({i,{}});return;}
        std::map<std::string,std::vector<std::size_t>> groups;std::vector<std::size_t> unassigned;
        for(auto i:matches) {const auto it=catalog[i].metadata.find(group_key);if(it==catalog[i].metadata.end() || it->second.empty())unassigned.push_back(i);else groups[it->second].push_back(i);}
        auto append=[&](const std::string& name,const std::vector<std::size_t>& images) {
            rows.push_back({group_row,name,images.size()});
            if(!needle.empty() || !collapsed.count(name))for(auto i:images)rows.push_back({i,{}});
        };
        for(const auto& group:groups)append(group.first,group.second);
        if(!unassigned.empty())append({},unassigned);
    }
    void toggle_group(const std::string& name) {if(!collapsed.erase(name))collapsed.insert(name);}
};

// Optional organization belongs in the Project menu; the primary flow is search
// and browsing. Only active organization or a source problem needs a status row.
inline bool project_browser_menu(ProjectBrowser& browser) {
    bool changed=false;
    if(!browser.keys.empty() && BeginMenu("Group images by")) {
        if(MenuItem("No grouping",nullptr,browser.group_key.empty())) {browser.group_key.clear();changed=true;}
        for(const auto& key:browser.keys)if(MenuItem(key.c_str(),nullptr,key==browser.group_key)) {browser.group_key=key;changed=true;}
        ImGui::EndMenu();
    }
    return changed;
}
inline bool project_browser_filters(ProjectBrowser& browser) {
    bool changed=false;ImGui::PushID("Project filters");
    if(!browser.group_key.empty()) {
        ActionRow row(1);
        ImGui::AlignTextToFramePadding();ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row.field_width());
        Text("%s: %s",i18n::text("Group images by"),browser.group_key.c_str());ImGui::PopTextWrapPos();
        row.next();if(IconButton("No grouping",Icon::Close)) {browser.group_key.clear();changed=true;}
    }
    if(browser.missing) {
        const bool active=browser.missing_only;ActionRow row(active?1:0);
        const auto caption=std::string(i18n::text("Missing files"))+" ("+std::to_string(browser.missing)+")###Missing files";
        if(Button(caption.c_str(),{row.field_width(),0},active?design::Action::Secondary:design::Action::Ghost)) {browser.missing_only=!active;changed=true;}
        if(active) {row.next();if(IconButton("All images",Icon::Close)) {browser.missing_only=false;changed=true;}}
    }
    ImGui::PopID();return changed;
}
}
