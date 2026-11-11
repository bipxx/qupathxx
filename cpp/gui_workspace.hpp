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
#include "gui_workflow.hpp"
#include "gui_project_preferences.hpp"

namespace qpx {
// All ImGui state belongs to the caller's context. Layout bytes never enter a
// Project; an empty preference path gives an in-memory workspace.
class GuiWorkspace {
    static constexpr std::size_t limit=1024*1024;
    bool enabled_,reset_,loaded_=false,writable_=true,protected_=false,settings_requested_=false,blocked_=false,upgrade_=true;
    std::string path_,error_;
    ImGuiID dock_=0;
    int active_=0;
    bool shown_[4]={true,true,true,true};
    int focus_=-1;
    int pending_pane_=-1;
    enum class Placement {Left,Right,StackRight,Floating};
    Placement pending_placement_=Placement::Right;
    ui::WorkflowTask task_=ui::WorkflowTask::Measurements;
    static int leaf_count(const ImGuiDockNode* node) {
        if(!node)return 0;
        return node->IsLeafNode()?1:leaf_count(node->ChildNodes[0])+leaf_count(node->ChildNodes[1]);
    }
    static ImGuiDockNode* side_node(ImGuiDockNode* node,const ImGuiDockNode* center,bool right) {
        if(!node || node==center)return nullptr;
        if(node->IsLeafNode())return (right?node->Pos.x>=center->Pos.x+center->Size.x-1:node->Pos.x+node->Size.x<=center->Pos.x+1)?node:nullptr;
        if(auto* found=side_node(node->ChildNodes[right?1:0],center,right))return found;
        return side_node(node->ChildNodes[right?0:1],center,right);
    }
    bool has_room_for_sides(float width) const {return width>=980*ui::design::scale();}
    void default_right(ImGuiID& center,float width,float) {
        ImGuiID right=0;
        ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,std::min(ui::design::metrics.sidebar_width*ui::design::scale(),width*.38f)/std::max(1.f,width),&right,&center);
        ImGui::DockBuilderDockWindow(workflow_pane,right);
    }
    void place_panel(int index,Placement placement) {
        auto* window=ImGui::FindWindowByName(panes[index]);
        auto* slide=ImGui::FindWindowByName("Slide");
        if(!window || !slide || !slide->DockNode)return;
        if(placement==Placement::Floating) {
            ImGui::DockContextProcessUndockWindow(ImGui::GetCurrentContext(),window,true);
            const auto* viewport=ImGui::GetMainViewport();
            ImGui::SetWindowPos(window,{viewport->WorkPos.x+viewport->WorkSize.x*.58f,viewport->WorkPos.y+48*ui::design::scale()},ImGuiCond_Always);
            ImGui::SetWindowSize(window,{std::min(336*ui::design::scale(),viewport->WorkSize.x*.42f),0},ImGuiCond_Always);
            return;
        }
        auto* center=slide->DockNode;
        const bool right=placement!=Placement::Left;
        auto* destination=side_node(ImGui::DockBuilderGetNode(dock_),center,right);
        ImGuiID target=destination?destination->ID:0,remaining=0;
        if(!target) {
            const float width=std::min(ui::design::metrics.sidebar_width*ui::design::scale(),center->Size.x*.42f);
            ImGui::DockBuilderSplitNode(center->ID,right?ImGuiDir_Right:ImGuiDir_Left,width/std::max(1.f,center->Size.x),&target,&remaining);
        } else if(placement==Placement::StackRight) {
            // Do not leave an empty slot when this is the only panel on that side.
            if(destination==window->DockNode && destination->Windows.Size==1)return;
            ImGui::DockBuilderSplitNode(target,ImGuiDir_Down,.5f,&target,&remaining);
        }
        ImGui::DockBuilderDockWindow(panes[index],target);
        ImGui::DockBuilderFinish(dock_);
    }
    static void prepare_node(ImGuiDockNode* node,ImGuiID active) {
        if(!node) return;
        bool shell_owned=!node->Windows.empty(),has_slide=false;
        for(auto* window:node->Windows) {
            bool primary=window->ID==ImHashStr("Slide");
            has_slide=has_slide || primary;
            for(const auto* pane:panes)primary=primary || window->ID==ImHashStr(pane);
            shell_owned=shell_owned && primary;
        }
        // A primary panel may be dropped over the viewer. Keep both reachable;
        // the activity rail selects inspectors, never the Slide window.
        if(has_slide && node->Windows.Size>1)shell_owned=false;
        auto flags=node->LocalFlags & ~(ImGuiDockNodeFlags_HiddenTabBar|ImGuiDockNodeFlags_NoTabBar);
        flags|=ImGuiDockNodeFlags_NoWindowMenuButton;
        if(shell_owned)flags|=ImGuiDockNodeFlags_NoTabBar;
        node->SetLocalFlags(flags);
        if(shell_owned) {
        ImGuiID preferred=node->SelectedTabId;
        for(auto* window:node->Windows) if(window->ID==active) preferred=ImHashStr("#TAB",0,active);
        for(int i=0;i<node->Windows.Size;++i) if(ImHashStr("#TAB",0,node->Windows[i]->ID)==preferred) {
            if(i) std::swap(node->Windows[0],node->Windows[i]);break;
        }
        if(!node->Windows.empty()) node->SelectedTabId=ImHashStr("#TAB",0,node->Windows[0]->ID);
        }
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
    std::string gene_preference;
    ui::ProjectPreferences projects;
    bool objects_visible=true,annotations_only=false;
    explicit GuiWorkspace(const GuiOptions& options):enabled_(options.docked_workspace),reset_(options.reset_layout),path_(options.layout_path) {
        annotations_only=enabled_;
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
            upgrade_=header.find("; qupathxx panel-layout=2")==std::string::npos;
            projects.load(header);
            objects_visible=header.find("; qupathxx objects-visible=0") == std::string::npos;
            for(int i=0;i<4;++i)shown_[i]=reset_ || header.find(std::string("; qupathxx pane-hidden=")+keys[i]+"\n")==std::string::npos && header.find(std::string("; qupathxx pane-hidden=")+keys[i]+"\r\n")==std::string::npos;
            annotations_only=header.find("; qupathxx object-filter=annotations") != std::string::npos;
            for(int i=0;i<4;++i) if(header.find(std::string("; qupathxx sidebar=")+keys[i]+"\n")!=std::string::npos || header.find(std::string("; qupathxx sidebar=")+keys[i]+"\r\n")!=std::string::npos) active_=i;
            for(std::size_t i=0;i<ui::workflow_task_keys.size();++i) {
                const auto marker=std::string("; qupathxx workflow=")+ui::workflow_task_keys[i];
                if(header.find(marker+"\n")!=std::string::npos || header.find(marker+"\r\n")!=std::string::npos) task_=static_cast<ui::WorkflowTask>(i);
            }
            if(reset_) return;
            const std::string gene_marker="; qupathxx gene-view=";
            const auto gene_start=data.find(gene_marker);
            if(gene_start!=std::string::npos) {
                const auto start=gene_start+gene_marker.size(),end=data.find_first_of("\r\n",start);
                gene_preference=data.substr(start,end==std::string::npos?end:end-start);
            }
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
    bool take_settings_request() {const bool requested=settings_requested_;settings_requested_=false;return requested;}
    bool loaded() const {return loaded_;}
    const std::string& path() const {return path_;}
    int active_pane() const {return shown_[active_]?active_:-1;}
    bool visible(const char* name) const {const int i=pane_index(name);return i<0 || shown_[i];}
    static bool can_dock_right(const char* name) {return pane_index(name)!=1;}
    ui::WorkflowTask workflow_task() const {return task_;}
    void select_task(ui::WorkflowTask task) {task_=task;}
    void select(const char* name) {
        const int i=pane_index(name);if(i<0)return;
        active_=i;shown_[i]=true;focus_=i;
        ImGui::SetWindowFocus(name);
    }
    void hide(const char* name) {const int i=pane_index(name);if(i>=0)shown_[i]=false;}
    // Activity clicks toggle the whole left sidebar, whereas View/task commands
    // always reveal their destination. Other sides and floating panes stay open.
    void activate(const char* name) {
        const int i=pane_index(name);if(i<0)return;
        if(active_==i && shown_[i] && on_left(i)) {
            for(int j=0;j<4;++j)if(on_left(j))shown_[j]=false;
        } else select(name);
    }
    void dock_left(const char* name) {request_placement(name,Placement::Left);}
    void dock_right(const char* name,bool stack=false) {request_placement(name,stack?Placement::StackRight:Placement::Right);}
    void float_panel(const char* name) {request_placement(name,Placement::Floating);}
    template<class Actions> void title(const char* caption,int extra_actions,Actions draw_actions) {
        if(ImGui::GetCurrentWindow()->SkipItems)return;
        const auto* window=ImGui::GetCurrentWindow();const auto* name=window->Name;
        const bool docked=window->DockNode!=nullptr;
        ui::ActionRow row(extra_actions+1+(docked?1:0));
        ui::design::Heading heading;
        const float height=ui::design::metrics.icon_size*ui::design::scale();
        const auto at=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##Drag panel",{row.field_width(),height});
        const auto* text=ui::i18n::text(caption);
        const ImVec4 clip{at.x,at.y,at.x+row.field_width(),at.y+height};
        if(docked)ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{at.x,at.y+(height-ImGui::GetFontSize())*.5f},ImGui::GetColorU32(ImGuiCol_Text),text,nullptr,0,&clip);
        if(docked && !blocked_ && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            auto* window=ImGui::GetCurrentWindow();auto& context=*ImGui::GetCurrentContext();
            if(window->DockNode)ImGui::DockContextQueueUndockWindow(&context,window);
            ImGui::StartMouseMovingWindow(window);context.MovingWindow=window;
        }
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))ImGui::SetTooltip("%s",ui::i18n::text("Drag panel to dock or float"));
        row.next();ImGui::BeginDisabled(blocked_);
        if(ui::IconButton("Panel layout",ui::Icon::PanelLayout))ImGui::OpenPopup("Panel layout");
        if(ui::BeginPopupMenu("Panel layout")) {
            if(ui::MenuItem("Dock on left"))dock_left(name);
            if(can_dock_right(name)) {
                if(ui::MenuItem("Dock on right"))dock_right(name);
                if(ui::MenuItem("Stack on right"))dock_right(name,true);
            }
            ImGui::Separator();
            if(ui::MenuItem("Float panel"))float_panel(name);
            if(ui::MenuItem("Hide panel"))hide(name);
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();draw_actions(row);
        if(docked) {row.next();if(ui::IconButton("Close panel",ui::Icon::Close))hide(name);}
    }
    void title(const char* caption) {title(caption,0,[](ui::ActionRow&){});}
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
    void reset() {if(protected_) return;reset_=true;loaded_=false;writable_=true;error_.clear();for(auto& shown:shown_)shown=true;}
    void dockspace(bool blocked=false,ui::RmlShell* shell=nullptr) {
        if(!enabled_) return;
        blocked_=blocked;
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
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ui::design::color(ui::design::colors().bg_primary));
        ImGui::BeginChild("Activity rail",{rail,size.y},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::BeginDisabled(blocked);
        constexpr ui::Icon icons[]={ui::Icon::Project,ui::Icon::Image,ui::Icon::Annotations,ui::Icon::Workflow};
        constexpr const char* shortcuts[]={"Ctrl+1","Ctrl+2","Ctrl+3","Ctrl+4"};
        for(int i=0;i<4;++i) {
            ImGui::SetCursorPos({0,i*ui::design::metrics.activity_row*scale});
            if(ui::ActivityButton(panes[i],icons[i],active_pane()==i,shortcuts[i])) activate(panes[i]);
        }
        ImGui::SetCursorPos({0,std::max(4*ui::design::metrics.activity_row*scale,size.y-ui::design::metrics.activity_row*scale)});
        if(ui::ActivityButton("Workspace settings",ui::Icon::Gear,false)) settings_requested_=true;
        ImGui::EndDisabled();ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar();ImGui::SameLine(0,0);
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
            if(has_room_for_sides(size.x))default_right(center,size.x-sidebar,size.y);
            else ImGui::DockBuilderDockWindow(workflow_pane,left);
            ImGui::DockBuilderGetNode(left)->SelectedTabId=ImHashStr("#TAB",0,ImHashStr(project_pane));
            ImGui::DockBuilderDockWindow("Slide",center);
            ImGui::DockBuilderFinish(dock_);reset_=false;
            upgrade_=false;
        }
        if(upgrade_ && has_room_for_sides(size.x)) {
            const auto* project=ImGui::FindWindowByName(project_pane);
            const auto* slide=ImGui::FindWindowByName("Slide");
            bool old_default=project && project->DockNode && slide && slide->DockNode && leaf_count(ImGui::DockBuilderGetNode(dock_))==2 && project->Pos.x<slide->Pos.x;
            for(const auto* name:panes) {const auto* pane=ImGui::FindWindowByName(name);old_default=old_default && pane && pane->DockId==project->DockId;}
            if(old_default) {auto center=slide->DockId;default_right(center,slide->DockNode->Size.x,size.y);ImGui::DockBuilderFinish(dock_);upgrade_=false;}
        }
        if(pending_pane_>=0) {place_panel(pending_pane_,pending_placement_);pending_pane_=-1;}
        // Enforce the same rule after native drag/drop and when restoring older
        // preferences; disabling a menu alone leaves those routes unrestricted.
        const auto* image=ImGui::FindWindowByName(image_pane);const auto* slide=ImGui::FindWindowByName("Slide");
        if(image && slide && image->DockNode && slide->DockNode && image->DockNode!=slide->DockNode && image->Pos.x>=slide->Pos.x+slide->Size.x-1)
            place_panel(1,Placement::Left);
        // Also migrate detached tool groups saved with the old hidden-tab rule.
        for(const auto& entry:ImGui::GetCurrentContext()->DockContext.Nodes.Data) {
            auto* node=static_cast<ImGuiDockNode*>(entry.val_p);
            if(node && !node->ParentNode)prepare_node(node,ImHashStr(panes[active_]));
        }
        ImGui::DockSpace(dock_,size,ImGuiDockNodeFlags_NoWindowMenuButton);
    }
    bool begin(const char* name,Rect& bounds,ImGuiWindowFlags flags=0) {
        const int index=pane_index(name);
        if(index>=0 && !shown_[index]) {
            // Existing inspector bodies also poll native jobs. Keep that work
            // alive in a noninteractive, unsaved sink without submitting the
            // real pane: ImGui can reclaim its inactive dock's viewer space.
            const auto sink=std::string("##Inactive inspector ")+keys[index];
            ImGui::SetNextWindowPos({-10000,-10000});ImGui::SetNextWindowSize({320,240});
            ImGui::Begin(sink.c_str(),nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoDecoration);
            ImGui::SetWindowHiddenAndSkipItemsForCurrentFrame(ImGui::GetCurrentWindow());
            bounds={0,0,0,0};return false;
        }
        const auto localized=ui::i18n::label(name);
        ui::design::Heading title;
        const bool slide=ImHashStr(name)==ImHashStr("Slide");
        if(slide) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        bool open=true;
        const auto* existing=ImGui::FindWindowByName(name);
        const bool floating=index>=0 && existing && !existing->DockId;
        if(floating) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,ui::design::metrics.panel_radius*ui::design::scale());
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,ui::design::metrics.border*ui::design::scale());
        }
        if(index>=0) {
            const auto* viewport=ImGui::GetMainViewport();
            ImGui::SetNextWindowSizeConstraints({240*ui::design::scale(),160*ui::design::scale()},viewport->WorkSize);
            if(focus_==index) {ImGui::SetNextWindowFocus();focus_=-1;}
        }
        const bool visible=ImGui::Begin(localized.c_str(),index>=0?&open:nullptr,flags|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoFocusOnAppearing);
        if(floating)ImGui::PopStyleVar(2);
        if(!open && index>=0)shown_[index]=false;
        if(slide) ImGui::PopStyleVar();
        const auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();
        bounds={p.x,p.y,s.x,s.y};
        return visible;
    }
    bool save() {
        if(!enabled_ || path_.empty() || !writable_) return false;
        try {
            std::size_t size=0;const char* data=ImGui::SaveIniSettingsToMemory(&size);
            if(gene_preference.size()>16384 || gene_preference.find_first_of("\r\n")!=std::string::npos)throw std::invalid_argument("Invalid gene-view workspace preference");
            std::string hidden;for(int i=0;i<4;++i)if(!shown_[i])hidden+=std::string("; qupathxx pane-hidden=")+keys[i]+"\n";
            const std::string header=std::string("; qupathxx workspace layout v1\n; qupathxx language=")+ui::i18n::preference()+"\n; qupathxx panel-layout=2\n; qupathxx theme="+ui::design::name(ui::design::state.choice)+"\n; qupathxx sidebar="+keys[active_]+"\n"+hidden+"; qupathxx workflow="+ui::workflow_task_keys[static_cast<std::size_t>(task_)]+"\n"+
                (gene_preference.empty()?"":"; qupathxx gene-view="+gene_preference+"\n")+"; qupathxx objects-visible="+(objects_visible?"1":"0")+"\n; qupathxx object-filter="+(annotations_only?"annotations":"all")+"\n"+projects.header();
            if(size>limit-header.size()) throw std::length_error("workspace layout exceeds 1 MiB");
            const auto parent=std::filesystem::u8path(path_).parent_path();
            if(!parent.empty()) std::filesystem::create_directories(parent);
            detail::AtomicExportFile file(path_,limit);file.write(header.data(),header.size());file.write(data,size);file.commit();return true;
        } catch(const std::exception& e) {error_=e.what();return false;}
    }
private:
    static int pane_index(const char* name) {for(int i=0;i<4;++i)if(ImHashStr(name)==ImHashStr(panes[i]))return i;return -1;}
    bool on_left(int i) const {
        const auto* window=ImGui::FindWindowByName(panes[i]);const auto* slide=ImGui::FindWindowByName("Slide");
        return window && slide && window->DockId && window->Pos.x+window->Size.x<=slide->Pos.x+1;
    }
    void request_placement(const char* name,Placement placement) {
        if(!can_dock_right(name) && (placement==Placement::Right || placement==Placement::StackRight))return;
        for(int i=0;i<4;++i)if(ImHashStr(name)==ImHashStr(panes[i])) {pending_pane_=i;pending_placement_=placement;select(name);break;}
    }
};
}
