#pragma once
#include "gui_layout.hpp"
#include "gui_commands.hpp"
#include "qupathxx/provenance.hpp"
#include <string>

namespace qpx::detail {
enum class HelpPage {QuickReference,Documentation,About};
namespace help_detail {
inline constexpr const char* user_guide_url="https://github.com/bipxx/qupathxx/blob/main/README.md#build-and-launch";
inline constexpr const char* repository_url="https://github.com/bipxx/qupathxx";
inline constexpr const char* license_url="https://github.com/bipxx/qupathxx/blob/main/LICENSE";
inline constexpr std::array shortcut_headings={
    "Anywhere, also while typing",
    "Anywhere except while typing",
    "On the image and its panels, outside tool windows"};
struct ShortcutLine {ui::CommandId command;std::string text;};
// Help has no surrounding task panel to qualify a short button caption.
inline std::string shortcut_caption(const ui::CommandInfo& row) {
    using Id=ui::CommandId;
    const char* context=nullptr;const char* scope=nullptr;const char* caption=row.caption;
    switch(row.id) {
    case Id::Action_Analyze_region_Ctrl_M:case Id::Action_Keep_result_Ctrl_K:case Id::Action_Recheck_saved_Ctrl_Shift_K:
    case Id::Action_Estimate_stains_F3:case Id::Action_Use_estimated_stains_Shift_F3:context="Region analysis";break;
    case Id::Action_Detect_tissue_Ctrl_D:case Id::Action_Keep_tissue_mask_Ctrl_U:case Id::Action_Recheck_saved_mask_Ctrl_Shift_U:
    case Id::Action_Trace_contours_Ctrl_J:case Id::Action_Add_annotations_Ctrl_Shift_J:context="Tissue";break;
    case Id::Action_Detect_cells_Ctrl_G:case Id::Action_Keep_cell_run_Ctrl_Shift_C:case Id::Action_Recheck_cells_Ctrl_Alt_C:
    case Id::Action_Add_cells_Ctrl_Shift_G:context="Cells";break;
    case Id::Action_Export_image_cells_Ctrl_Alt_X:context="Cells";scope="Current image";break;
    case Id::Action_Export_project_cells_Ctrl_Shift_X:context="Cells";scope="Project";break;
    case Id::Action_Export_image_Ctrl_Alt_Q:context="Measurements";scope="Current image";break;
    case Id::Action_Export_project_Ctrl_Shift_Q:context="Measurements";scope="Project";break;
    case Id::Action_Hash_file_Ctrl_Shift_F:case Id::Action_Review_for_project_Ctrl_Alt_F:context="File checksum";break;
    case Id::Action_Apply_Boolean_B:caption="Apply Boolean operation";break;
    default:break;
    }
    std::string result;
    if(context){result=ui::i18n::text(context);result+=" / ";}
    if(scope){result+=ui::i18n::text(scope);result+=" / ";}
    result+=ui::i18n::text(caption);return result;
}
inline const std::array<std::vector<ShortcutLine>,3>& shortcut_lines() {
    static thread_local int language=-1;
    static thread_local std::array<std::vector<ShortcutLine>,3> groups;
    if(language!=ui::i18n::state.effective) {
        language=ui::i18n::state.effective;
        for(auto& group:groups)group.clear();
        for(const auto& row:ui::command_table)if(row.chord && row.chord_owner==ui::CommandId::Count)
            groups.at(static_cast<std::size_t>(row.scope)).push_back({row.id,
                ui::chord_text(row.chord)+": "+shortcut_caption(row)});
    }
    return groups;
}

inline void link(const char* label,const char* url,std::string& error) {
    const auto translated=ui::i18n::label(label);
    const auto* end=ui::label_end(translated.c_str());
    // TextLinkOpenURL discards the platform callback's success value. Keep its
    // native link behavior while reporting failure through the owned dialog.
    const bool pressed=ImGui::CalcTextSize(translated.c_str(),end).x<=ui::available()
        ?ImGui::TextLink(translated.c_str()):ui::GhostButton(label);
    ui::observe(label);ImGui::SetItemTooltip("%s",url);
    if(ui::BeginContextMenu(nullptr)) {
        if(ui::MenuItem("Copy link"))ImGui::SetClipboardText(url);
        ImGui::EndPopup();
    }
    if(pressed) {
        bool opened=false;
        try {
            const auto open=ImGui::GetPlatformIO().Platform_OpenInShellFn;
            opened=open && open(ImGui::GetCurrentContext(),url);
        }catch(...) {}
        if(opened)error.clear();
        else {error=ui::i18n::text("Could not open the link. Copy the URL and open it in your browser.");error+='\n';error+=url;}
    }
}
inline void value(const char* caption,const std::string& text) {
    ui::TextDisabled("%s",ui::i18n::text(caption));
    ImGui::PushTextWrapPos(0);
    ui::DataText(text.empty()?ui::i18n::text("Not available"):text.c_str());
    ImGui::PopTextWrapPos();ui::observe(caption);
}
}

// Body only: the caller owns registered-modal lifetime, close, page and error.
// Navigation and layout never invoke a platform callback; links require a click.
inline void render_help_page(HelpPage page,const BuildEnvironment& environment,std::string& link_error) {
    switch(page) {
    case HelpPage::QuickReference:
        ui::TextWrapped("No shortcut acts while a dialog, a menu or a list is open.");
        for(std::size_t i=0;i<help_detail::shortcut_headings.size();++i) {
            ImGui::Spacing();
            ui::SeparatorText(help_detail::shortcut_headings[i]);
            for(const auto& line:help_detail::shortcut_lines()[i]) {
                ui::DataTextWrapped("%s",line.text.c_str());ui::observe("Shortcut");
            }
        }
        ui::SeparatorText("Mouse gestures");
        ui::TextWrapped("Right drag: pan");
        ui::TextWrapped("Scroll: zoom");
        ui::TextWrapped("Ctrl+click: toggle selection");
        ui::TextWrapped("Shift+drag: select box");
        ui::TextWrapped("Double-click: finish shape");
        break;
    case HelpPage::Documentation:
        ui::TextWrapped("Open the user guide for setup and workflow instructions.");
        help_detail::link("User guide###HelpUserGuide",help_detail::user_guide_url,link_error);
        ImGui::Spacing();
        ui::TextWrapped("Find source code, issues and project updates on GitHub.");
        help_detail::link("GitHub repository###HelpRepository",help_detail::repository_url,link_error);
        break;
    case HelpPage::About:
        ui::DataText("qupathxx");
        ImGui::Spacing();
        help_detail::value("Version",environment.app_version);
        ImGui::Spacing();
        help_detail::value("Engine source SHA-256",environment.source_tree_sha256);
        ImGui::Spacing();
        ui::TextWrapped("Licensed under AGPL-3.0-only.");
        help_detail::link("Source and license###HelpLicense",help_detail::license_url,link_error);
        break;
    }
}
}
