#pragma once
#include "gui_path_field.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/qupath_project.hpp"
#include "ascii_case.hpp"
#include <deque>

namespace qpx::detail {
enum class ProjectForm {None,Open,Save,Add,Relink};
struct ProjectFormRequests {bool open=false,refresh_review=false,save=false,add=false,relink=false;};
struct GuiPathDrop {std::vector<std::string> paths;double x=0,y=0;};
// Presentation and requests only. Native project/reader jobs remain with run_gui.
class GuiProjectForms {
    std::deque<std::string> queue_;
    std::size_t completed_=0,total_=0;
    std::vector<std::pair<std::string,std::string>> failures_;
    void heading(const char* caption) {
        ui::ActionRow row(1);
        {ui::design::Heading heading;ImGui::AlignTextToFramePadding();ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row.field_width());ui::TextUnformatted(caption);ImGui::PopTextWrapPos();}
        row.next();if(ui::IconButton("Close",ui::Icon::Close)){select(ProjectForm::None);focus_open=focus_save=focus_image=false;}
    }
public:
    ProjectForm shown=ProjectForm::None;
    bool focus_open=false,focus_save=false,focus_image=false;
    ui::PathField destination{{"##Project path",ui::PathRole::NewFile,{".qpxproj"},"","Destination .qpxproj file",true}};
    ui::PathField open_path{{"##Open project path",ui::PathRole::OpenFile,{".qpxproj",".qpproj"},"","Project file to open",true}};
    ui::PathField image{{"##Image source",ui::PathRole::OpenFile,{},"","Image file to add or relink",true}};
    ui::PathField qupath_destination{{"##QuPath destination",ui::PathRole::NewFile,{".qpxproj"},"","New native project destination"}};
    std::string saved_destination;
    explicit GuiProjectForms(const GuiOptions& options) {
        open_path.set_option(options.project_open_path);image.set_option(options.project_image_path);
        if(options.project){saved_destination=options.project->path();destination.set(saved_destination);}
        shown=!options.project_open_path.empty()?ProjectForm::Open:!options.project_image_path.empty()?ProjectForm::Add:ProjectForm::None;
    }
    void select(ProjectForm next) {
        if(shown==ProjectForm::Save && next!=ProjectForm::Save)destination.set(saved_destination);
        shown=next;
    }
    std::vector<ui::PathField*> paths(){return {&destination,&open_path,&image,&qupath_destination};}
    ProjectFormRequests draw_save(const ui::PathContext& context,bool busy,const std::function<void()>& mark) {
        ProjectFormRequests request;if(shown!=ProjectForm::Save)return request;heading("Save project as...");
        if(focus_save){destination.request_focus();focus_save=false;}
        destination.draw(context,busy,{},[&](bool entered){ImGui::BeginDisabled(!*destination.text());if(ui::IconButton("Save project",ui::Icon::Save,ui::Availability{*destination.text() && !busy,busy?"Wait for the current work to finish.":"Choose a path"},ui::command_chord(ui::CommandId::Header_SaveProject)) || (entered && *destination.text()))request.save=true;mark();ImGui::EndDisabled();},"File");return request;
    }
    ProjectFormRequests draw_open(const ui::PathContext& context,bool busy,bool shortcut,bool reviewing,const std::shared_ptr<const QuPathProjectPreview>& preview,bool& losses) {
        ProjectFormRequests request;if(shown!=ProjectForm::Open)return request;heading("Open project...");
        if(focus_open){open_path.request_focus();focus_open=false;}
        auto extension=std::filesystem::u8path(open_path.text()).extension().u8string();ascii_lower_in_place(extension);const bool qupath=extension==".qpproj";
        const char* label=qupath?(preview?"Import reviewed project (Ctrl+O)":"Review QuPath project (Ctrl+O)"):"Open project (Ctrl+O)";
        open_path.draw(context,busy,{},[&](bool entered){ImGui::BeginDisabled(!*open_path.text());if(ui::IconButton(label,qupath?ui::Icon::Import:ui::Icon::Open,ui::Availability{*open_path.text() && !busy,busy?"Wait for the current work to finish.":"Choose a path"}) || (entered && *open_path.text() && !busy))request.open=true;ImGui::EndDisabled();},"File");
        if(shortcut && *open_path.text() && !busy)request.open=true;
        if(reviewing)ui::TextDisabled("Reviewing QuPath project...");
        if(qupath && preview) {
            ui::Text("Reviewed %zu images",preview->images().size());
            ImGui::BeginChild("QuPath import report",ImVec2(0,180),true,ImGuiWindowFlags_HorizontalScrollbar);
            ImGuiListClipper rows;const auto count=preview->images().size();rows.Begin(int(count+preview->issues().size()));
            while(rows.Step())for(int row=rows.DisplayStart;row<rows.DisplayEnd;++row) {
                if(std::size_t(row)<count){const auto& item=preview->images()[row];ui::Text("%s: %s, %zu objects",item.name.c_str(),item.available?"available":"unavailable",item.objects);}
                else {const auto& issue=preview->issues()[row-count];ui::Text("%s (%zu)",issue.message.c_str(),issue.count);}
            }ImGui::EndChild();
            if(!preview->issues().empty())ui::Checkbox("Accept reported project import losses",&losses);
            qupath_destination.draw(context,busy,{},[&](bool){if(ui::IconButton("Refresh QuPath review",ui::Icon::Refresh))request.refresh_review=true;},"Export destination");
            ui::TextDisabled("Existing destination files are preserved.");
        }return request;
    }
    ProjectFormRequests draw_image(const ui::PathContext& context,bool busy,bool has_image,const std::vector<ProjectImage>& images,int& reader,const std::string& reader_labels) {
        ProjectFormRequests request;if(shown!=ProjectForm::Add && shown!=ProjectForm::Relink)return request;
        const bool relinking=shown==ProjectForm::Relink;heading(relinking?"Relink image":"Add images");
        if(focus_image){image.request_focus();focus_image=false;}
        for(const auto& entry:images)if(*image.text() && entry.source_path==image.state().resolved){ui::TextWrapped("This file is already in the project as %s.",entry.name.c_str());break;}
        image.draw(context,busy,{},[&](bool entered){ImGui::BeginDisabled(!*image.text() || (relinking && !has_image));if(ui::IconButton(relinking?ui::presentation_label(ui::CommandId::Key_RelinkImage,"Relink image (F9)"):"Add image###Add image (F8)",relinking?ui::Icon::Link:ui::Icon::Plus,ui::Availability{*image.text() && (!relinking || has_image) && !busy,busy?"Wait for the current work to finish.":relinking && !has_image?"Open an image first.":"Choose a path"},ui::command_chord(relinking?ui::CommandId::Key_RelinkImage:ui::CommandId::Header_AddImages)) || (entered && *image.text())){request.relink=relinking;request.add=!relinking;}ImGui::EndDisabled();},"File");
        if(!relinking){ui::SetNextItemWidth(-1);ui::Combo("Reader",&reader,reader_labels.c_str());}
        draw_progress();return request;
    }
    void enqueue(const std::vector<std::string>& paths) {
        if(queue_.empty()){completed_=total_=0;failures_.clear();}
        for(const auto& path:paths){queue_.push_back(path);++total_;}shown=ProjectForm::Add;
    }
    bool adding() const{return !queue_.empty();}
    std::size_t completed() const{return completed_;}
    std::size_t total() const{return total_;}
    const std::vector<std::pair<std::string,std::string>>& failures() const{return failures_;}
    void finish_one(const std::string& error={}) {
        if(queue_.empty())return;
        if(!error.empty())failures_.emplace_back(std::filesystem::u8path(queue_.front()).filename().u8string(),error);
        queue_.pop_front();++completed_;
    }
    void process_one(const std::function<bool(const std::string&)>& add) {
        if(queue_.empty())return;
        try{if(add(queue_.front()))finish_one();}catch(const std::exception& e){finish_one(e.what());}
    }
    void draw_progress() const {
        if(adding())ui::TextWrapped("Adding %zu of %zu",completed_+1,total_);
        for(const auto& failure:failures_)ui::TextWrapped("%s: %s",failure.first.c_str(),ui::i18n::text(failure.second.c_str()));
    }
};
// Position is captured by GLFW, never inferred from ImGui hover during OS drag.
inline bool route_path_drop(const GuiPathDrop& drop,GuiProjectForms& forms,const std::vector<ui::PathField*>& fields,bool blocked,ImGuiID modal=0,bool has_project=true,std::string* blocked_error=nullptr,const char** refusal_key=nullptr) {
    const auto refused=[&](const char* key){if(blocked_error)*blocked_error=ui::i18n::text(key);if(refusal_key)*refusal_key=key;};
    if(drop.paths.empty())return false;
    if(blocked && !modal){refused("Wait for the current work to finish.");return false;}
    for(auto* field:fields)if(field->hit(drop.x,drop.y,modal)){field->drop(drop.paths.front(),drop.paths.size()>1);return true;}
    if(blocked || modal){refused("Finish the current dialog first.");return false;}
    if(!has_project){refused("Open a project to add images.");return true;}
    auto extension=std::filesystem::u8path(drop.paths.front()).extension().u8string();ascii_lower_in_place(extension);
    if(drop.paths.size()==1 && (extension==".qpxproj" || extension==".qpproj")){forms.open_path.drop(drop.paths.front());forms.select(ProjectForm::Open);}
    else forms.enqueue(drop.paths);
    return true;
}
struct GuiProjectFormsTrace {
    std::function<void(GuiProjectForms&,const std::vector<ui::PathField*>&,std::vector<GuiPathDrop>&)> before_frame;
};
inline thread_local GuiProjectFormsTrace* project_forms_trace=nullptr;
}
