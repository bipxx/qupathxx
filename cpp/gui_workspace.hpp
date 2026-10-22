#pragma once
#include "atomic_export.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace qpx {
// All ImGui state belongs to the caller's context. Layout bytes never enter a
// Project; an empty preference path gives an in-memory workspace.
class GuiWorkspace {
    static constexpr std::size_t limit=1024*1024;
    bool enabled_,reset_,loaded_=false,writable_=true,protected_=false;
    std::string path_,error_;
    ImGuiID dock_=0;
public:
    Rect browser{0,0,1,1},display{0,0,1,1},measurements{0,0,1,1},canvas{0,0,1,1};
    explicit GuiWorkspace(const GuiOptions& options):enabled_(options.docked_workspace),reset_(options.reset_layout),path_(options.layout_path) {
        if(!enabled_ && (!path_.empty() || reset_)) throw std::invalid_argument("layout preferences require a docked workspace");
        if(path_.empty()) return;
        if(path_.size()>32768 || path_.find('\0')!=std::string::npos || std::filesystem::u8path(path_).extension()!=std::filesystem::path(".ini"))
            throw std::invalid_argument("workspace layout path must be an .ini file");
        protect(options.startup_path);
        if(options.project) {protect(options.project->path());protect(options.project->recovery_source());}
        if(protected_) return;
        // ImGui does no automatic I/O. Read and write errors are recoverable
        // preference errors, never a reason to lose the open research project.
        if(reset_) return;
        try {
            const auto file=std::filesystem::u8path(path_);
            if(!std::filesystem::exists(file)) return;
            if(std::filesystem::file_size(file)>limit) throw std::length_error("workspace layout exceeds 1 MiB");
            std::ifstream input(file,std::ios::binary);
            if(!input) throw std::runtime_error("cannot read workspace layout");
            std::string data(limit+1,'\0');input.read(data.data(),static_cast<std::streamsize>(data.size()));data.resize(static_cast<std::size_t>(input.gcount()));
            if(input.bad() || data.size()>limit) throw std::runtime_error("cannot read bounded workspace layout");
            if(data.rfind("; qupathxx workspace layout v1\n",0)!=0 && data.rfind("; qupathxx workspace layout v1\r\n",0)!=0) throw std::runtime_error("unrecognized workspace layout; reset layout to replace it");
            ImGui::LoadIniSettingsFromMemory(data.data(),data.size());loaded_=true;
        } catch(const std::exception& e) {error_=e.what();writable_=false;}
    }
    bool enabled() const {return enabled_;}
    bool loaded() const {return loaded_;}
    const std::string& error() const {return error_;}
    void protect(const std::string& source) {
        if(path_.empty() || source.empty()) return;
        std::error_code error;
        const auto destination=std::filesystem::absolute(std::filesystem::u8path(path_)).lexically_normal();
        const auto candidate=std::filesystem::absolute(std::filesystem::u8path(source)).lexically_normal();
        if(destination==candidate || std::filesystem::equivalent(destination,candidate,error)) {
            protected_=true;writable_=false;error_="Workspace layout cannot replace a project or image source";
        }
    }
    void reset() {if(protected_) return;reset_=true;loaded_=false;writable_=true;error_.clear();}
    void dockspace() {
        if(!enabled_) return;
        dock_=ImGui::GetID("Research workspace dockspace");
        auto size=ImGui::GetContentRegionAvail();size.x=std::max(1.f,size.x);size.y=std::max(1.f,size.y);
        if(reset_ || !ImGui::DockBuilderGetNode(dock_)) {
            ImGui::DockBuilderRemoveNode(dock_);
            ImGui::DockBuilderAddNode(dock_,ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodePos(dock_,ImGui::GetCursorScreenPos());
            ImGui::DockBuilderSetNodeSize(dock_,size);
            ImGuiID center=dock_,left=0,right=0,bottom=0;
            ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,.23f,&left,&center);
            ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.30f,&right,&center);
            ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.30f,&bottom,&center);
            ImGui::DockBuilderDockWindow("Project and objects",left);
            ImGui::DockBuilderDockWindow("Image and display",right);
            ImGui::DockBuilderDockWindow("Measurements and jobs",bottom);
            ImGui::DockBuilderDockWindow("Slide",center);
            ImGui::DockBuilderFinish(dock_);reset_=false;
        }
        ImGui::DockSpace(dock_,size);
    }
    bool begin(const char* name,Rect& bounds,ImGuiWindowFlags flags=0) {
        const bool visible=ImGui::Begin(name,nullptr,flags|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoFocusOnAppearing);
        const auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();
        bounds={p.x,p.y,s.x,s.y};
        return visible;
    }
    bool save() {
        if(!enabled_ || path_.empty() || !writable_) return false;
        try {
            std::size_t size=0;const char* data=ImGui::SaveIniSettingsToMemory(&size);
            const std::string header="; qupathxx workspace layout v1\n";
            if(size>limit-header.size()) throw std::length_error("workspace layout exceeds 1 MiB");
            const auto parent=std::filesystem::u8path(path_).parent_path();
            if(!parent.empty()) std::filesystem::create_directories(parent);
            detail::AtomicExportFile file(path_,limit);file.write(header.data(),header.size());file.write(data,size);file.commit();return true;
        } catch(const std::exception& e) {error_=e.what();return false;}
    }
};
}
