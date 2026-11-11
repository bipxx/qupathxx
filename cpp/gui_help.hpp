#pragma once
#include "gui_layout.hpp"
#include "qupathxx/provenance.hpp"
#include <string>

namespace qpx::detail {
enum class HelpPage {QuickReference,Documentation,About};
namespace help_detail {
inline constexpr const char* user_guide_url="https://github.com/bipxx/qupathxx/blob/main/README.md#build-and-launch";
inline constexpr const char* repository_url="https://github.com/bipxx/qupathxx";
inline constexpr const char* license_url="https://github.com/bipxx/qupathxx/blob/main/LICENSE";

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
    ImGui::TextUnformatted(text.empty()?ui::i18n::text("Not available"):text.c_str());
    ImGui::PopTextWrapPos();ui::observe(caption);
}
}

// Body only: the caller owns registered-modal lifetime, close, page and error.
// Navigation and layout never invoke a platform callback; links require a click.
inline void render_help_page(HelpPage page,const BuildEnvironment& environment,std::string& link_error) {
    switch(page) {
    case HelpPage::QuickReference:
        ui::SeparatorText("Navigation");
        ui::TextWrapped("Right drag: pan");
        ui::TextWrapped("Scroll: zoom");
        ui::SeparatorText("Selection");
        ui::TextWrapped("Ctrl+click: toggle selection");
        ui::TextWrapped("Shift+drag: select box");
        ui::TextWrapped("Ctrl+A: select all");
        ui::SeparatorText("Drawing");
        ui::TextWrapped("R: rectangle");
        ui::TextWrapped("E: ellipse");
        ui::TextWrapped("P: polygon");
        ui::TextWrapped("L: polyline");
        ui::TextWrapped("T: points");
        ui::TextWrapped("V: vertices");
        ui::TextWrapped("H: hole");
        ui::TextWrapped("Enter or double-click: finish shape");
        ui::TextWrapped("Escape: cancel");
        break;
    case HelpPage::Documentation:
        ui::TextWrapped("Open the user guide for setup and workflow instructions.");
        help_detail::link("User guide###HelpUserGuide",help_detail::user_guide_url,link_error);
        ImGui::Spacing();
        ui::TextWrapped("Find source code, issues and project updates on GitHub.");
        help_detail::link("GitHub repository###HelpRepository",help_detail::repository_url,link_error);
        break;
    case HelpPage::About:
        ImGui::TextUnformatted("qupathxx");
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
