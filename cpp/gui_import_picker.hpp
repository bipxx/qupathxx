#pragma once
#include "gui_dialogs.hpp"
#include "ascii_case.hpp"
#include <filesystem>
#include <array>
#include <vector>
#include <optional>

namespace qpx::detail {
enum class PickerMode {OpenFile,NewFile,OpenFolder,NewFolder};
struct GuiImportPickerTestAccess;
// Local path choice only; no decoding, downloads or scientific interpretation.
class GuiImportPicker {
    std::array<char,32769> folder_{},filename_{};
    std::filesystem::directory_iterator iterator_,end_;
    std::vector<std::filesystem::directory_entry> entries_;
    std::vector<std::string> extensions_;
    std::string error_;bool save_=false,requested_=false,legacy_=false,all_files_=false;
    PickerMode mode_=PickerMode::OpenFile;
    std::vector<std::vector<std::filesystem::directory_entry>> pages_;
    std::size_t page_index_=0;
    friend struct GuiImportPickerTestAccess;
    bool folder_role() const {return mode_==PickerMode::OpenFolder || mode_==PickerMode::NewFolder;}
    void previous() {if(page_index_)entries_=pages_[--page_index_];}
    void all_files(bool value) {all_files_=value;directory(std::filesystem::u8path(folder_.data()));}
    std::string choose() const {
        const auto path=(mode_==PickerMode::OpenFolder?std::filesystem::u8path(folder_.data()):std::filesystem::u8path(folder_.data())/std::filesystem::u8path(filename_.data())).lexically_normal();
        if(mode_==PickerMode::OpenFolder) {
            if(!std::filesystem::is_directory(path))throw std::invalid_argument("Choose an existing folder");
        } else {
            if(!filename_[0])throw std::invalid_argument(folder_role()?"Enter a folder name":"Enter a filename");
            if(mode_==PickerMode::OpenFile && !std::filesystem::is_regular_file(path))throw std::invalid_argument("Choose an existing regular file");
            if(mode_==PickerMode::NewFile && ((legacy_ && std::filesystem::exists(path)) || (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path))))throw std::invalid_argument(legacy_?"Choose an unused output filename":"Choose a file path");
            if(mode_==PickerMode::NewFolder && std::filesystem::exists(path) && !std::filesystem::is_directory(path))throw std::invalid_argument("Choose a folder path");
        }
        if(!legacy_ && save_ && !std::filesystem::is_directory(path.parent_path()))throw std::invalid_argument("Parent folder does not exist");
        return path.u8string();
    }
    void page() {
        if(!pages_.empty() && page_index_+1<pages_.size()) {entries_=pages_[++page_index_];return;}
        entries_.clear();unsigned scanned=0;
        while(iterator_!=end_ && scanned++<16384 && entries_.size()<64) {
            const auto entry=*iterator_;++iterator_;auto extension=entry.path().extension().u8string();ascii_lower_in_place(extension);
            const bool folder=entry.is_directory();
            if(folder || (!folder_role() && (all_files_ || extensions_.empty() || std::find(extensions_.begin(),extensions_.end(),extension)!=extensions_.end())))entries_.push_back(entry);
        }
        std::sort(entries_.begin(),entries_.end(),[](const auto& a,const auto& b) {
            const bool ad=a.is_directory(),bd=b.is_directory();if(ad!=bd)return ad;
            auto an=a.path().filename().u8string(),bn=b.path().filename().u8string();ascii_lower_in_place(an);ascii_lower_in_place(bn);
            return an==bn?a.path().filename().u8string()<b.path().filename().u8string():an<bn;
        });
        pages_.push_back(entries_);page_index_=pages_.size()-1;
    }
    void directory(const std::filesystem::path& path) {
        const auto text=std::filesystem::absolute(path).lexically_normal().u8string();if(text.size()>32768)throw std::invalid_argument("Directory path is too long");
        std::copy(text.begin(),text.end(),folder_.begin());folder_[text.size()]=0;
        pages_.clear();page_index_=0;iterator_=std::filesystem::directory_iterator(std::filesystem::u8path(text),std::filesystem::directory_options::skip_permission_denied);page();error_.clear();
    }
public:
    bool active() const {return requested_ || ui::IsDialogOpen(ui::DialogKind::ImportFile);}
    void open(const std::string& current,std::vector<std::string> extensions,bool save=false) {
        open(current,std::move(extensions),save?PickerMode::NewFile:PickerMode::OpenFile);legacy_=true;
    }
    void open(const std::string& current,std::vector<std::string> extensions,PickerMode mode) {
        extensions_=std::move(extensions);for(auto& extension:extensions_)ascii_lower_in_place(extension);
        mode_=mode;save_=mode==PickerMode::NewFile || mode==PickerMode::NewFolder;legacy_=false;all_files_=false;requested_=true;filename_[0]=0;
        try {auto path=std::filesystem::u8path(current);if(path.empty())path=std::filesystem::current_path();
            if(!std::filesystem::is_directory(path)) {const auto name=path.filename().u8string();if(name.size()<filename_.size())std::copy(name.c_str(),name.c_str()+name.size()+1,filename_.begin());path=path.parent_path();}
            if(path.empty())path=".";if(mode_==PickerMode::OpenFolder)filename_[0]=0;directory(path);
        }catch(const std::exception& e){error_=e.what();entries_.clear();iterator_=end_;}
    }
    std::optional<std::string> draw() {
        if(requested_) {ui::OpenDialog(ui::DialogKind::ImportFile);requested_=false;}
        std::optional<std::string> selected;
        ui::DialogState state(true,false,mode_==PickerMode::NewFolder?ui::DialogVariant::NewFolder:mode_==PickerMode::OpenFolder?ui::DialogVariant::Folder:save_?ui::DialogVariant::Output:ui::DialogVariant::Default);
        const std::string localized_error=ui::i18n::text(error_.c_str());
        ui::RenderDialog(ui::DialogKind::ImportFile,state,folder_.data(),{&localized_error},[&](ui::DialogControls&) {
            const auto invoke=[&](auto action){try{action();}catch(const std::exception& e){error_=e.what();}};
            ui::DialogInput("Folder",folder_.data(),folder_.size());
            if(ui::Button("Read folder"))invoke([&]{directory(std::filesystem::u8path(folder_.data()));});ImGui::SameLine();
            if(ui::Button("Parent folder"))invoke([&]{directory(std::filesystem::u8path(folder_.data()).parent_path());});
            ui::LayoutRegion files_region("Files");ui::PaneTitle("Files");
            if(entries_.empty())ui::TextDisabled("No files. Use %s.",ui::i18n::text("Read folder"));
            if(ImGui::BeginChild("files",{0,std::max(80.f,ImGui::GetContentRegionAvail().y-5*ImGui::GetFrameHeightWithSpacing())},ImGuiChildFlags_Borders)) {
                for(std::size_t i=0;i<entries_.size();++i) {
                    ImGui::PushID(int(i));const auto name=entries_[i].path().filename().u8string();
                    std::error_code status;const bool is_directory=entries_[i].is_directory(status);bool navigated=false;
                    const auto position=ImGui::GetCursorScreenPos();
                    if(ui::Selectable("##entry"))invoke([&]{
                        if(is_directory) {directory(entries_[i].path());navigated=true;}
                        else {std::copy(name.begin(),name.end(),filename_.begin());filename_[name.size()]=0;}
                    });const auto label=is_directory?"[+] "+name:name;ImGui::GetWindowDrawList()->AddText(position,ImGui::GetColorU32(ImGuiCol_Text),label.c_str());ImGui::PopID();
                    if(navigated)break;
                }
            }ImGui::EndChild();
            ImGui::BeginDisabled(page_index_==0);if(ui::Button("Previous files"))previous();ImGui::EndDisabled();
            ImGui::BeginDisabled(iterator_==end_ && page_index_+1==pages_.size());if(ui::Button("Next files"))invoke([&]{page();});ImGui::EndDisabled();
            if(!folder_role()) {bool all=all_files_;if(ui::Checkbox("All files",&all))invoke([&]{all_files(all);});}
            if(mode_!=PickerMode::OpenFolder)ui::DialogInput(mode_==PickerMode::NewFolder?"New folder name":save_?"New filename":"Filename",filename_.data(),filename_.size());
        },[&](ui::DialogAction action) {
            if(action==ui::DialogAction::Cancel)return ui::DialogOutcome::Success;
            try {
                selected=choose();error_.clear();return ui::DialogOutcome::Success;
            }catch(const std::exception& e){error_=e.what();return ui::DialogOutcome::Refused;}
        });
        return selected;
    }
};
}
