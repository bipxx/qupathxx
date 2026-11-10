#pragma once
#include "gui_dialogs.hpp"
#include "ascii_case.hpp"
#include <filesystem>
#include <array>
#include <vector>
#include <optional>

namespace qpx::detail {
// Local path choice only; no decoding, downloads or scientific interpretation.
class GuiImportPicker {
    std::array<char,32769> folder_{},filename_{};
    std::filesystem::directory_iterator iterator_,end_;
    std::vector<std::filesystem::directory_entry> entries_;
    std::vector<std::string> extensions_;
    std::string error_;bool save_=false,requested_=false;
    void page() {
        entries_.clear();unsigned scanned=0;
        while(iterator_!=end_ && scanned++<256 && entries_.size()<64) {
            const auto entry=*iterator_;++iterator_;auto extension=entry.path().extension().u8string();ascii_lower_in_place(extension);
            if(entry.is_directory() || extensions_.empty() || std::find(extensions_.begin(),extensions_.end(),extension)!=extensions_.end())entries_.push_back(entry);
        }
    }
    void directory(const std::filesystem::path& path) {
        const auto text=std::filesystem::absolute(path).lexically_normal().u8string();if(text.size()>32768)throw std::invalid_argument("Directory path is too long");
        std::copy(text.begin(),text.end(),folder_.begin());folder_[text.size()]=0;
        iterator_=std::filesystem::directory_iterator(std::filesystem::u8path(text),std::filesystem::directory_options::skip_permission_denied);page();error_.clear();
    }
public:
    void open(const std::string& current,std::vector<std::string> extensions,bool save=false) {
        extensions_=std::move(extensions);save_=save;requested_=true;filename_[0]=0;
        try {auto path=std::filesystem::u8path(current);if(path.empty())path=std::filesystem::current_path();
            if(!std::filesystem::is_directory(path)) {const auto name=path.filename().u8string();if(name.size()<filename_.size())std::copy(name.c_str(),name.c_str()+name.size()+1,filename_.begin());path=path.parent_path();}
            if(path.empty())path=".";directory(path);
        }catch(const std::exception& e){error_=e.what();entries_.clear();iterator_=end_;}
    }
    std::optional<std::string> draw() {
        if(requested_) {ui::OpenDialog(ui::DialogKind::ImportFile);requested_=false;}
        std::optional<std::string> selected;
        ui::DialogState state(true,false,save_?ui::DialogVariant::Output:ui::DialogVariant::Default);
        ui::RenderDialog(ui::DialogKind::ImportFile,state,folder_.data(),{&error_},[&](ui::DialogControls&) {
            const auto invoke=[&](auto action){try{action();}catch(const std::exception& e){error_=e.what();}};
            ui::DialogInput("Folder",folder_.data(),folder_.size());
            if(ui::Button("Read folder"))invoke([&]{directory(std::filesystem::u8path(folder_.data()));});ImGui::SameLine();
            if(ui::Button("Parent folder"))invoke([&]{directory(std::filesystem::u8path(folder_.data()).parent_path());});
            if(ImGui::BeginChild("files",{0,std::max(80.f,ImGui::GetContentRegionAvail().y-96*ui::design::scale())},ImGuiChildFlags_Borders)) {
                for(std::size_t i=0;i<entries_.size();++i) {
                    ImGui::PushID(int(i));const auto name=entries_[i].path().filename().u8string();
                    std::error_code status;const bool is_directory=entries_[i].is_directory(status);bool navigated=false;
                    const auto position=ImGui::GetCursorScreenPos();
                    if(ImGui::Selectable("##entry"))invoke([&]{
                        if(is_directory) {directory(entries_[i].path());navigated=true;}
                        else {std::copy(name.begin(),name.end(),filename_.begin());filename_[name.size()]=0;}
                    });const auto label=is_directory?"[+] "+name:name;ImGui::GetWindowDrawList()->AddText(position,ImGui::GetColorU32(ImGuiCol_Text),label.c_str());ImGui::PopID();
                    if(navigated)break;
                }
            }ImGui::EndChild();
            ImGui::BeginDisabled(iterator_==end_);if(ui::Button("Next files"))invoke([&]{page();});ImGui::EndDisabled();
            ui::DialogInput(save_?"New filename":"Filename",filename_.data(),filename_.size());
        },[&](ui::DialogAction action) {
            if(action==ui::DialogAction::Cancel)return ui::DialogOutcome::Success;
            try {
                const auto path=(std::filesystem::u8path(folder_.data())/std::filesystem::u8path(filename_.data())).lexically_normal();
                if(!filename_[0] || (save_?std::filesystem::exists(path):!std::filesystem::is_regular_file(path)))throw std::invalid_argument(save_?"Choose an unused output filename":"Choose an existing regular file");
                selected=path.u8string();error_.clear();return ui::DialogOutcome::Success;
            }catch(const std::exception& e){error_=e.what();return ui::DialogOutcome::Refused;}
        });
        return selected;
    }
};
}
