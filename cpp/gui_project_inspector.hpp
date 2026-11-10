#pragma once
#include "gui_layout.hpp"
#include "qupathxx/project.hpp"

namespace qpx::ui {
// Presentation over an owned ProjectImage snapshot. Editing stays with Project
// and EntryEditor; inspecting an entry never activates it or reads slide pixels.
inline void ImageMetadata(const ProjectImage& image) {
    if(ImGui::BeginTable("##Image metadata",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("Key",ImGuiTableColumnFlags_WidthStretch,.38f);
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,.62f);
        auto row=[](const std::string& key,const std::string& value) {
            ImGui::TableNextRow();ImGui::TableNextColumn();
            {design::Caption caption;ImGui::PushStyleColor(ImGuiCol_Text,design::color(design::colors().text_secondary));
                ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(key.empty()?"\xe2\x80\x94":key.c_str());ImGui::PopTextWrapPos();ImGui::PopStyleColor();}
            ImGui::TableNextColumn();ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(value.empty()?"\xe2\x80\x94":value.c_str());ImGui::PopTextWrapPos();
        };
        for(const auto& [key,value]:image.metadata)row(key,value);
        row(i18n::text("Source"),image.embedded?i18n::text("Embedded RGB"):image.source_path);
        if(!image.embedded && !image.reader.empty())row(i18n::text("Reader"),image.reader);
        ImGui::EndTable();
    }
    if(image.description && !image.description->empty()) {
        ImGui::Spacing();TextDisabled("Entry description");ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(image.description->c_str());ImGui::PopTextWrapPos();
    }
    if(!image.tags.empty()) {
        ImGui::Spacing();TextDisabled("Entry tags");
        for(const auto& tag:image.tags) {ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(tag.c_str());ImGui::PopTextWrapPos();}
    }
}
}
