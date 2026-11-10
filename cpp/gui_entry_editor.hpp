#pragma once

#include "qupathxx/project.hpp"
#include <imgui.h>
#include "gui_dialogs.hpp"
#include <filesystem>
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
    bool has_description_{},open_requested_{},project_mode_{},details_tab_{};
    std::string context_;
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
        auto metadata=project_mode_?project->metadata():project->image_metadata(image);
        auto description=project_mode_?std::optional<std::string>{}:project->image_description(image);
        auto tags=project_mode_?std::vector<std::string>{}:project->image_tags(image);
        owner_=project;image_=image;
        if(project_mode_)context_=project->path().empty()?ui::i18n::text("Untitled project"):std::filesystem::u8path(project->path()).filename().u8string();
        else {const auto images=project->images();const auto item=std::find_if(images.begin(),images.end(),[&](const auto& entry){return entry.id==image;});context_=item==images.end()?std::string{}:item->name;}
        baseline_metadata_=std::move(metadata);baseline_description_=description;baseline_tags_=tags;
        metadata_.assign(baseline_metadata_.begin(),baseline_metadata_.end());tags_=std::move(tags);
        description_.assign(project_mode_?1:1024*1024+1,0);has_description_=description.has_value();
        if(description)std::memcpy(description_.data(),description->data(),description->size());
        select_metadata(metadata_.empty()?-1:0);select_tag(tags_.empty()?-1:0);error_.clear();
    }
    static std::string preview(std::string value) {for(auto& c:value)if(c=='\n' || c=='\r' || c=='\t')c=' ';return value;}
    void metadata_fields() {
        ImGui::BeginDisabled(metadata_.size()>=4096);
        if(ui::IconButton("Add metadata",ui::Icon::Plus)) {metadata_.emplace_back();select_metadata(static_cast<int>(metadata_.size()-1));}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(metadata_row_<0);
        if(ui::IconButton("Remove metadata",ui::Icon::Minus)) {metadata_.erase(metadata_.begin()+metadata_row_);select_metadata(metadata_.empty()?-1:0);}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(metadata_.empty());
        if(ui::IconButton("Clear metadata",ui::Icon::Trash)) {metadata_.clear();select_metadata(-1);}
        ImGui::EndDisabled();
        if(ImGui::BeginChild("Metadata rows",{0,140*ui::design::scale()},ImGuiChildFlags_Borders)) {
            if(metadata_.empty())ui::TextDisabled("No metadata fields yet.");
            ImGuiListClipper clip;clip.Begin(static_cast<int>(metadata_.size()));
            while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
                ImGui::PushID(row);const auto label=preview(metadata_[row].first);
                if(ImGui::Selectable(label.empty()?"<empty key>":label.c_str(),row==metadata_row_))select_metadata(row);
                if(ImGui::IsItemHovered()) {ImGui::BeginTooltip();ImGui::TextUnformatted(metadata_[row].first.c_str());ImGui::Separator();ImGui::TextUnformatted(metadata_[row].second.c_str());ImGui::EndTooltip();}
                ImGui::PopID();
            }
        }ImGui::EndChild();
        ImGui::BeginDisabled(metadata_row_<0);
        if(ui::DialogInput("Key",key_.data(),key_.size(),{0,48}) && metadata_row_>=0)metadata_[metadata_row_].first=key_.data();
        if(ui::DialogInput("Value",value_.data(),value_.size(),{0,112}) && metadata_row_>=0)metadata_[metadata_row_].second=value_.data();
        ImGui::EndDisabled();
    }
    void detail_fields() {
        ui::Checkbox("Include description###Description present",&has_description_);
        if(has_description_)ui::DialogInput("Description",description_.data(),description_.size(),{0,112});
        ImGui::Spacing();ui::TextDisabled("Tags");
        ImGui::BeginDisabled(tags_.size()>=4096);
        if(ui::IconButton("Add tag",ui::Icon::Plus)) {tags_.emplace_back();select_tag(static_cast<int>(tags_.size()-1));}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(tag_row_<0);
        if(ui::IconButton("Remove tag",ui::Icon::Minus)) {tags_.erase(tags_.begin()+tag_row_);select_tag(tags_.empty()?-1:0);}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(tags_.empty());
        if(ui::IconButton("Clear tags",ui::Icon::Trash)) {tags_.clear();select_tag(-1);}
        ImGui::EndDisabled();
        if(ImGui::BeginChild("Tag rows",{0,100*ui::design::scale()},ImGuiChildFlags_Borders)) {
            if(tags_.empty())ui::TextDisabled("No tags yet.");
            ImGuiListClipper clip;clip.Begin(static_cast<int>(tags_.size()));
            while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
                ImGui::PushID(row);const auto label=preview(tags_[row]);
                if(ImGui::Selectable(label.empty()?"<empty tag>":label.c_str(),row==tag_row_))select_tag(row);
                ImGui::PopID();
            }
        }ImGui::EndChild();
        if(tag_row_>=0)ui::DialogInput("Tag",tag_.data(),tag_.size(),{0,48});
        if(tag_row_>=0)tags_[tag_row_]=tag_.data();
    }
    void apply_metadata(const std::shared_ptr<Project>& project) {
        if((project_mode_?project->metadata():project->image_metadata(image_))!=baseline_metadata_)throw std::runtime_error("Metadata changed outside this draft. Reload before applying.");
        std::map<std::string,std::string> values;
        for(const auto& item:metadata_)if(!values.emplace(item).second)throw std::runtime_error("Duplicate metadata key. Edit or remove the duplicate before applying.");
        if(project_mode_)project->set_metadata(values);else project->set_image_metadata(image_,values);
        baseline_metadata_=std::move(values);
    }
    void apply_details(const std::shared_ptr<Project>& project) {
        if(project->image_description(image_)!=baseline_description_ || project->image_tags(image_)!=baseline_tags_)throw std::runtime_error("Description or tags changed outside this draft. Reload before applying.");
        auto description=has_description_?std::optional<std::string>(description_.data()):std::nullopt;
        project->set_image_details(image_,description,tags_);
        baseline_description_=std::move(description);baseline_tags_=project->image_tags(image_);
        tags_=baseline_tags_;select_tag(tags_.empty()?-1:0);
    }
public:
    void open(const std::shared_ptr<Project>& project,std::uint64_t image) {
        project_mode_=false;load(project,image);open_requested_=true;
    }
    void open_project(const std::shared_ptr<Project>& project) {project_mode_=true;load(project,0);open_requested_=true;}
    // Return true only when a native setter succeeded (including an exact no-op).
    bool draw(const std::shared_ptr<Project>& project,std::uint64_t image,bool busy=false) {
        const auto kind=project_mode_?ui::DialogKind::ProjectMetadata:ui::DialogKind::ImageMetadata;
        if(open_requested_) {ui::OpenDialog(kind);open_requested_=false;}
        bool same=project && project==owner_ && (project_mode_ || image==image_);
        if(same && !project_mode_)try {project->document(image_);}catch(const std::exception&){same=false;}
        ui::DialogState state(same,busy,details_tab_ && !project_mode_?ui::DialogVariant::Details:ui::DialogVariant::Metadata);
        bool applied=false;
        ui::RenderDialog(kind,state,context_,{&error_},[&](ui::DialogControls& controls) {
            if(project_mode_) {
                details_tab_=false;controls.variant(ui::DialogVariant::Metadata);ImGui::PushID("Entry fields");ImGui::PushID("Metadata");metadata_fields();ImGui::PopID();ImGui::PopID();
            } else if(ImGui::BeginTabBar("Entry fields")) {
                if(ImGui::BeginTabItem(ui::i18n::label("Metadata").c_str())) {details_tab_=false;controls.variant(ui::DialogVariant::Metadata);metadata_fields();ImGui::EndTabItem();}
                if(ImGui::BeginTabItem(ui::i18n::label("Description and tags").c_str())) {details_tab_=true;controls.variant(ui::DialogVariant::Details);detail_fields();ImGui::EndTabItem();}
                ImGui::EndTabBar();
            }
            ImGui::Spacing();ui::TextDisabled("Save the project to keep applied changes.");
        },[&](ui::DialogAction action) {
            if(action==ui::DialogAction::Cancel) {owner_.reset();return ui::DialogOutcome::Success;}
            try {
                if(action==ui::DialogAction::Reload)load(project,image);
                else {if(action==ui::DialogAction::ApplyDetails)apply_details(project);else apply_metadata(project);applied=true;}
                error_.clear();return ui::DialogOutcome::Success;
            }catch(const std::exception& e){error_=e.what();return ui::DialogOutcome::Refused;}
        });
        return applied;
    }
};
}
