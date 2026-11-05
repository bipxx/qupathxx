#pragma once

#include "qupathxx/project.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <array>
#include <cstring>
#include <stdexcept>

namespace qpx::detail {
// Transient UI state only. Project remains the validation and publication authority.
class EntryEditor {
    std::shared_ptr<Project> owner_;
    std::uint64_t image_{};
    std::map<std::string,std::string> baseline_metadata_;
    std::optional<std::string> baseline_description_;
    std::vector<std::string> baseline_tags_;
    std::vector<std::pair<std::string,std::string>> metadata_;
    std::vector<std::string> tags_;
    std::vector<char> description_;
    bool has_description_{},open_requested_{};
    int metadata_row_{-1},tag_row_{-1};
    std::array<char,4097> key_{},value_{},tag_{};
    std::string error_;

    static void copy(std::array<char,4097>& buffer,const std::string& value) {
        buffer.fill(0);std::memcpy(buffer.data(),value.data(),value.size());
    }
    void select_metadata(int row) {
        // InputText caches active contents independently of our rebound buffer.
        ImGui::ClearActiveID();metadata_row_=row;
        if(row>=0) {copy(key_,metadata_[row].first);copy(value_,metadata_[row].second);}
    }
    void select_tag(int row) {ImGui::ClearActiveID();tag_row_=row;if(row>=0)copy(tag_,tags_[row]);}
    void load(const std::shared_ptr<Project>& project,std::uint64_t image) {
        // Read before replacing a previous draft, so a removed image cannot clear it.
        auto metadata=project->image_metadata(image);
        auto description=project->image_description(image);
        auto tags=project->image_tags(image);
        owner_=project;image_=image;
        baseline_metadata_=std::move(metadata);baseline_description_=description;baseline_tags_=tags;
        metadata_.assign(baseline_metadata_.begin(),baseline_metadata_.end());tags_=std::move(tags);
        description_.assign(1024*1024+1,0);has_description_=description.has_value();
        if(description)std::memcpy(description_.data(),description->data(),description->size());
        select_metadata(metadata_.empty()?-1:0);select_tag(tags_.empty()?-1:0);error_.clear();
    }
public:
    void open(const std::shared_ptr<Project>& project,std::uint64_t image) {
        load(project,image);open_requested_=true;
    }
    // Return true only when a native setter succeeded (including an exact no-op).
    bool draw(const std::shared_ptr<Project>& project,std::uint64_t image) {
        bool applied=false;
        // Resolve popup IDs in the draw scope, independent of the sidebar tree.
        if(open_requested_) {ImGui::OpenPopup("Edit project entry");open_requested_=false;}
        ImGui::SetNextWindowSize({640,660},ImGuiCond_FirstUseEver);
        if(!ImGui::BeginPopupModal("Edit project entry",nullptr,ImGuiWindowFlags_NoSavedSettings))return false;
        const bool same=project==owner_ && image==image_;
        ImGui::TextUnformatted("Changes stay in this draft until Apply. Save the project to persist them.");
        if(!same)ImGui::TextWrapped("The active project or image changed. Reload the current entry or cancel.");
        if(ImGui::Button("Reload current entry")) {
            try {load(project,image);}catch(const std::exception& e){error_=e.what();}
        }
        ImGui::SameLine();if(ImGui::Button("Cancel / close")) {ImGui::CloseCurrentPopup();owner_.reset();}
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
        ImGui::BeginDisabled(!same);
        if(ImGui::BeginTabBar("Entry fields")) {
            if(ImGui::BeginTabItem("Metadata")) {
                ImGui::TextUnformatted("Keys and values preserve exact text. Empty strings are allowed.");
                if(ImGui::BeginChild("Metadata rows",{0,140},ImGuiChildFlags_Borders)) {
                    ImGuiListClipper clip;clip.Begin(static_cast<int>(metadata_.size()));
                    while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
                        ImGui::PushID(row);
                        const auto& key=metadata_[row].first;
                        if(ImGui::Selectable(key.empty()?"<empty key>":key.c_str(),row==metadata_row_))select_metadata(row);
                        ImGui::PopID();
                    }
                }ImGui::EndChild();
                ImGui::BeginDisabled(metadata_.size()>=4096);
                if(ImGui::Button("Add metadata")) {metadata_.emplace_back();select_metadata(static_cast<int>(metadata_.size()-1));}
                ImGui::EndDisabled();ImGui::SameLine();
                ImGui::BeginDisabled(metadata_row_<0);
                if(ImGui::Button("Remove metadata")) {metadata_.erase(metadata_.begin()+metadata_row_);select_metadata(metadata_.empty()?-1:0);}
                if(metadata_row_>=0) {
                    if(ImGui::InputTextMultiline("Key",key_.data(),key_.size(),{0,55},ImGuiInputTextFlags_AllowTabInput))metadata_[metadata_row_].first=key_.data();
                    if(ImGui::InputTextMultiline("Value",value_.data(),value_.size(),{0,80},ImGuiInputTextFlags_AllowTabInput))metadata_[metadata_row_].second=value_.data();
                }
                ImGui::EndDisabled();
                if(ImGui::Button("Clear metadata")) {metadata_.clear();select_metadata(-1);}
                if(ImGui::Button("Apply metadata")) {
                    try {
                        if(project->image_metadata(image_)!=baseline_metadata_)throw std::runtime_error("Metadata changed outside this draft. Reload before applying.");
                        std::map<std::string,std::string> values;
                        for(const auto& item:metadata_)if(!values.emplace(item).second)throw std::runtime_error("Duplicate metadata key. Edit or remove the duplicate before applying.");
                        project->set_image_metadata(image_,values);baseline_metadata_=std::move(values);error_.clear();applied=true;
                    }catch(const std::exception& e){error_=e.what();}
                }
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Description and tags")) {
                ImGui::Checkbox("Description present",&has_description_);
                ImGui::BeginDisabled(!has_description_);
                ImGui::InputTextMultiline("Description",description_.data(),description_.size(),{0,130},ImGuiInputTextFlags_AllowTabInput);
                ImGui::EndDisabled();
                if(ImGui::BeginChild("Tag rows",{0,100},ImGuiChildFlags_Borders)) {
                    ImGuiListClipper clip;clip.Begin(static_cast<int>(tags_.size()));
                    while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
                        ImGui::PushID(row);
                        if(ImGui::Selectable(tags_[row].empty()?"<empty tag>":tags_[row].c_str(),row==tag_row_))select_tag(row);
                        ImGui::PopID();
                    }
                }ImGui::EndChild();
                ImGui::BeginDisabled(tags_.size()>=4096);
                if(ImGui::Button("Add tag")) {tags_.emplace_back();select_tag(static_cast<int>(tags_.size()-1));}
                ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(tag_row_<0);
                if(ImGui::Button("Remove tag")) {tags_.erase(tags_.begin()+tag_row_);select_tag(tags_.empty()?-1:0);}
                if(tag_row_>=0 && ImGui::InputTextMultiline("Tag",tag_.data(),tag_.size(),{0,55},ImGuiInputTextFlags_AllowTabInput))tags_[tag_row_]=tag_.data();
                ImGui::EndDisabled();
                if(ImGui::Button("Clear tags")) {tags_.clear();select_tag(-1);}
                if(ImGui::Button("Apply description and tags")) {
                    try {
                        if(project->image_description(image_)!=baseline_description_ || project->image_tags(image_)!=baseline_tags_)throw std::runtime_error("Description or tags changed outside this draft. Reload before applying.");
                        auto description=has_description_?std::optional<std::string>(description_.data()):std::nullopt;
                        project->set_image_details(image_,description,tags_);
                        baseline_description_=std::move(description);baseline_tags_=project->image_tags(image_);
                        tags_=baseline_tags_;select_tag(tags_.empty()?-1:0);error_.clear();applied=true;
                    }catch(const std::exception& e){error_=e.what();}
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndDisabled();ImGui::EndPopup();return applied;
    }
};
}
