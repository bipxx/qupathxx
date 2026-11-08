#pragma once
#include "atomic_export.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "gui_i18n.hpp"
#include "gui_design_system.hpp"
#include "gui_layout.hpp"
#include "gui_shell.hpp"

namespace qpx {
// All ImGui state belongs to the caller's context. Layout bytes never enter a
// Project; an empty preference path gives an in-memory workspace.
class GuiWorkspace {
    static constexpr std::size_t limit=1024*1024;
    bool enabled_,reset_,loaded_=false,writable_=true,protected_=false;
    std::string path_,error_;
    ImGuiID dock_=0;
    int active_=0;
    static void prepare_node(ImGuiDockNode* node,ImGuiID active) {
        if(!node) return;
        node->SetLocalFlags((node->LocalFlags & ~ImGuiDockNodeFlags_HiddenTabBar)|ImGuiDockNodeFlags_NoTabBar|ImGuiDockNodeFlags_NoWindowMenuButton);
        ImGuiID preferred=node->SelectedTabId;
        for(auto* window:node->Windows) if(window->ID==active) preferred=ImHashStr("#TAB",0,active);
        for(int i=0;i<node->Windows.Size;++i) if(ImHashStr("#TAB",0,node->Windows[i]->ID)==preferred) {
            if(i) std::swap(node->Windows[0],node->Windows[i]);break;
        }
        if(!node->Windows.empty()) node->SelectedTabId=ImHashStr("#TAB",0,node->Windows[0]->ID);
        prepare_node(node->ChildNodes[0],active);prepare_node(node->ChildNodes[1],active);
    }
public:
    static constexpr const char* project_pane="Project###Project and objects";
    static constexpr const char* image_pane="Image###Image and display";
    static constexpr const char* objects_pane="Annotations###Annotations";
    static constexpr const char* workflow_pane="Workflow###Measurements and jobs";
    inline static constexpr const char* panes[]={project_pane,image_pane,objects_pane,workflow_pane};
    inline static constexpr const char* keys[]={"project","image","annotations","workflow"};
    Rect browser{0,0,1,1},objects{0,0,1,1},display{0,0,1,1},measurements{0,0,1,1},canvas{0,0,1,1};
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
        try {
            const auto file=std::filesystem::u8path(path_);
            if(!std::filesystem::exists(file)) return;
            if(std::filesystem::file_size(file)>limit) {if(reset_) return;throw std::length_error("workspace layout exceeds 1 MiB");}
            std::ifstream input(file,std::ios::binary);
            if(!input) throw std::runtime_error("cannot read workspace layout");
            std::string data(limit+1,'\0');input.read(data.data(),static_cast<std::streamsize>(data.size()));data.resize(static_cast<std::size_t>(input.gcount()));
            if(input.bad() || data.size()>limit) throw std::runtime_error("cannot read bounded workspace layout");
            if(data.rfind("; qupathxx workspace layout v1\n",0)!=0 && data.rfind("; qupathxx workspace layout v1\r\n",0)!=0) {if(reset_) return;throw std::runtime_error("unrecognized workspace layout; reset layout to replace it");}
            ui::i18n::load_preference(data);ui::design::load_preference(data);
            const auto header_end=data.find("[Window]");const auto header=data.substr(0,header_end);
            for(int i=0;i<4;++i) if(header.find(std::string("; qupathxx sidebar=")+keys[i]+"\n")!=std::string::npos || header.find(std::string("; qupathxx sidebar=")+keys[i]+"\r\n")!=std::string::npos) active_=i;
            if(reset_) return;
            // Retain the user's splits when opening a pre-tabbed workspace.
            for(const auto& names:{std::pair{"Project and objects",project_pane},std::pair{"Image and display",image_pane},std::pair{"Measurements and jobs",workflow_pane}}) {
                const std::string old_header=std::string("[Window][")+names.first+"]";
                const auto at=data.find(old_header);
                if(at!=std::string::npos) data.replace(at,old_header.size(),std::string("[Window][")+names.second+"]");
            }
            ImGui::LoadIniSettingsFromMemory(data.data(),data.size());loaded_=true;
            if(!ImGui::FindWindowSettingsByID(ImHashStr(objects_pane))) {
                const auto* project=ImGui::FindWindowSettingsByID(ImHashStr(project_pane));
                if(project && project->DockId) ImGui::DockBuilderDockWindow(objects_pane,project->DockId);
            }
        } catch(const std::exception& e) {error_=e.what();writable_=false;}
    }
    bool enabled() const {return enabled_;}
    bool loaded() const {return loaded_;}
    int active_pane() const {return active_;}
    void select(const char* name) {
        for(int i=0;i<4;++i) if(ImHashStr(name)==ImHashStr(panes[i])) {active_=i;ImGui::SetWindowFocus(name);break;}
    }
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
    void dockspace(bool blocked=false,ui::RmlShell* shell=nullptr) {
        if(!enabled_) return;
        dock_=ImGui::GetID("Research workspace dockspace");
        auto size=ImGui::GetContentRegionAvail();size.x=std::max(1.f,size.x);
        const int footer_lines=size.x<60*ImGui::GetFontSize()?2:1;
        size.y=std::max(1.f,size.y-footer_lines*ImGui::GetTextLineHeightWithSpacing()-2*ui::design::spacing.sm*ui::design::scale());
        const float scale=ui::design::scale(),rail=shell?shell->rail_width():ui::design::metrics.activity_width*scale;
        const auto origin=ImGui::GetCursorScreenPos();
        if(!blocked) for(int i=0;i<4;++i) if(ImGui::Shortcut(ImGuiMod_Ctrl|static_cast<ImGuiKey>(ImGuiKey_1+i),ImGuiInputFlags_RouteGlobal)) select(panes[i]);
        if(shell) {
            shell->place_rail(origin.x,origin.y,size.y);ImGui::Dummy({rail,size.y});ImGui::SameLine(0,0);
        } else {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::BeginChild("Activity rail",{rail,size.y},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        constexpr ui::Icon icons[]={ui::Icon::Project,ui::Icon::Image,ui::Icon::Annotations,ui::Icon::Workflow};
        constexpr const char* shortcuts[]={"Ctrl+1","Ctrl+2","Ctrl+3","Ctrl+4"};
        for(int i=0;i<4;++i) {
            ImGui::SetCursorPos({(rail-ui::design::metrics.icon_size*scale)/2,(ui::design::spacing.sm+i*ui::design::metrics.activity_row)*scale});
            if(ui::IconButton(panes[i],icons[i],ui::design::Action::Ghost,shortcuts[i])) select(panes[i]);
            if(active_==i) {
                const auto p=ImGui::GetItemRectMin(),s=ImGui::GetItemRectSize();
                ImGui::GetWindowDrawList()->AddRectFilled({origin.x,p.y},{origin.x+2*scale,p.y+s.y},ImGui::GetColorU32(ui::design::color(ui::design::colors().accent)));
            }
        }
        ImGui::EndChild();ImGui::PopStyleVar();ImGui::SameLine(0,0);
        }
        size.x=std::max(1.f,size.x-rail);
        if(reset_ || !ImGui::DockBuilderGetNode(dock_)) {
            ImGui::DockBuilderRemoveNode(dock_);
            ImGui::DockBuilderAddNode(dock_,ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodePos(dock_,ImGui::GetCursorScreenPos());
            ImGui::DockBuilderSetNodeSize(dock_,size);
            ImGuiID center=dock_,left=0;
            const float sidebar=std::min(size.x*.42f,ui::design::metrics.sidebar_width*ui::design::scale());
            ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,sidebar/size.x,&left,&center);
            ImGui::DockBuilderDockWindow(project_pane,left);
            ImGui::DockBuilderDockWindow(image_pane,left);
            ImGui::DockBuilderDockWindow(objects_pane,left);
            ImGui::DockBuilderDockWindow(workflow_pane,left);
            ImGui::DockBuilderGetNode(left)->SelectedTabId=ImHashStr("#TAB",0,ImHashStr(project_pane));
            ImGui::DockBuilderDockWindow("Slide",center);
            ImGui::DockBuilderFinish(dock_);reset_=false;
        }
        prepare_node(ImGui::DockBuilderGetNode(dock_),ImHashStr(panes[active_]));
        ImGui::DockSpace(dock_,size,ImGuiDockNodeFlags_NoTabBar|ImGuiDockNodeFlags_NoWindowMenuButton);
    }
    bool begin(const char* name,Rect& bounds,ImGuiWindowFlags flags=0) {
        const auto localized=ui::i18n::label(name);
        ui::design::Heading title;
        const bool slide=ImHashStr(name)==ImHashStr("Slide");
        if(slide) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        const bool visible=ImGui::Begin(localized.c_str(),nullptr,flags|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoFocusOnAppearing);
        if(slide) ImGui::PopStyleVar();
        const auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();
        bounds={p.x,p.y,s.x,s.y};
        return visible;
    }
    bool save() {
        if(!enabled_ || path_.empty() || !writable_) return false;
        try {
            std::size_t size=0;const char* data=ImGui::SaveIniSettingsToMemory(&size);
            const std::string header=std::string("; qupathxx workspace layout v1\n; qupathxx language=")+ui::i18n::preference()+"\n; qupathxx theme="+ui::design::name(ui::design::state.choice)+"\n; qupathxx sidebar="+keys[active_]+"\n";
            if(size>limit-header.size()) throw std::length_error("workspace layout exceeds 1 MiB");
            const auto parent=std::filesystem::u8path(path_).parent_path();
            if(!parent.empty()) std::filesystem::create_directories(parent);
            detail::AtomicExportFile file(path_,limit);file.write(header.data(),header.size());file.write(data,size);file.commit();return true;
        } catch(const std::exception& e) {error_=e.what();return false;}
    }
};
}
