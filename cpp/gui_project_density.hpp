#pragma once
#include "gui_tool_catalog.hpp"
#include "gui_path_field.hpp"
#include "qupathxx/density_workflow.hpp"
#include "qupathxx/project.hpp"
#include "gui_layout.hpp"
#include "gui_result_state.hpp"
#include "gui_workflow_ui.hpp"
#include "gui_shell_open.hpp"
#include "gui_notifications.hpp"
#include <imgui.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif
namespace qpx::detail {
class GuiProjectDensity {
    ui::PathField output_{{"##density-output",ui::PathRole::NewFolder,{},"","New output folder",false,true}};
    ui::PathField previous_{{"##density-previous",ui::PathRole::OpenFolder,{},"","Previous results folder (optional)",false,true}};
    friend struct GuiProjectDensityTestAccess;
    ProjectDensityJob job_;ui::NoticeJob notice_job_;std::uint64_t notice_generation_=0;
    bool open_=false,all_cells_=true,prefer_nucleus_=true,curves_=false;
    std::array<std::array<char,4097>,4> classes_{};
    std::set<std::uint64_t> images_;
    std::string project_id_,source_,task_source_,error_;
    std::shared_ptr<const ProjectDensityReport> preview_,observed_;
    template<class F>void attempt(F action) {try{action();error_.clear();}catch(const std::exception& e){error_=e.what();}}
    static std::vector<std::string> lines(const char* text) {
        std::vector<std::string> out;std::istringstream input(text);std::string line;
        while(std::getline(input,line)) {if(!line.empty() && line.back()=='\r')line.pop_back();if(!line.empty())out.push_back(line);}return out;
    }
    ProjectDensityRecipe recipe() const {
        ProjectDensityRecipe r;r.images.assign(images_.begin(),images_.end());
        for(std::size_t i=0;i<4;++i)r.classes[i]=lines(classes_[i].data());if(all_cells_)r.classes[3].clear();
        r.all_cells=all_cells_;r.options.prefer_nucleus=prefer_nucleus_;r.options.polygonize_curves=curves_;return r;
    }
public:
    std::vector<ui::PathField*> paths(){return {&output_,&previous_};}
    void finish(){job_.finish();}
    void open(){open_=true;}
    void open_button(){if(ui::Button(ui::command(ui::CommandId::Tool_Density).caption))open_=true;}
    void draw(const Project* project,const std::vector<ProjectImage>& catalog,bool blocked,const ui::PathContext& context={},ui::Notifications* notices=nullptr) {
    ui::i18n::SurfaceTrace translation_surface("tool/Project density");
        job_.poll();auto result=job_.result();
        if(result && result!=observed_) {observed_=result;if(result->state=="preview")preview_=result;else preview_.reset();}
        if(notices) {
            if(result)notice_job_.observe(*notices,ui::command(ui::CommandId::Tool_Density).caption,result->state,result->state=="completed",result->state=="failed" || result->state=="partial",result->error,result->output,notice_generation_);
            else if(!job_.busy() && !job_.error().empty())notice_job_.observe(*notices,ui::command(ui::CommandId::Tool_Density).caption,"failed",false,true,job_.error(),{},notice_generation_);
        }
        if(!open_)return;
        ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::Density),{760,720});
        if(!ImGui::Begin(ui::tool_window_label(ui::ToolId::Density),&open_)){ImGui::End();return;}
        try {
            const auto path=project && !project->path().empty()?std::filesystem::weakly_canonical(std::filesystem::u8path(project->path())).u8string():std::string{};
            if((project?project->identity():std::string{})!=project_id_ || path!=source_) {
                project_id_=project?project->identity():"";source_=path;images_.clear();for(const auto& image:catalog)images_.insert(image.id);preview_.reset();
            }
            for(auto i=images_.begin();i!=images_.end();) {
                if(std::none_of(catalog.begin(),catalog.end(),[&](const auto& image){return image.id==*i;})){i=images_.erase(i);preview_.reset();}else ++i;
            }
            if(preview_ && preview_->source!=source_)preview_.reset();
            const bool busy=job_.busy(),saved=project && !source_.empty() && !project->dirty(),enabled=saved && !blocked && !busy;
            ui::TextWrapped("Uses the saved project. Results are written to a new folder.");
            if(!saved)ui::TextWrapped("Save project before checking inputs or running.");
            if(!source_.empty())ui::DataTextWrapped("Source: %s",source_.c_str());
            ImGui::BeginDisabled(busy || blocked);
            bool changed=false;
            {ui::LayoutRegion images_region("Project images");
            {ui::CaptionedStrip images("Project images",2);images.next();
            if(ui::IconButton("Select all images###All images",ui::Icon::SelectAll,ui::Availability{!busy && !blocked,blocked?ui::WorkflowBlocked().reason:"Wait for the current work to finish."})){images_.clear();for(const auto& image:catalog)images_.insert(image.id);changed=true;}images.next();
            if(ui::IconButton("Clear image selection###Clear images",ui::Icon::Deselect,ui::Availability{!busy && !blocked,blocked?ui::WorkflowBlocked().reason:"Wait for the current work to finish."})){images_.clear();changed=true;}}
            if(ui::BeginCollection("Density images",catalog.size())) {
                if(catalog.empty())ui::TextDisabled("No images. Use Add images to fill this list.");
                for(const auto& image:catalog) {
                    ImGui::PushID(std::to_string(image.id).c_str());bool selected=images_.count(image.id)!=0;
                    if(ui::RowCheckbox(ui::data_label(image.name,"image").c_str(),&selected)){if(selected)images_.insert(image.id);else images_.erase(image.id);changed=true;}
                    ImGui::SameLine();ui::DataTextDisabled("ID %llu",static_cast<unsigned long long>(image.id));ImGui::PopID();
                }
            }ImGui::EndChild();}
            ui::TextWrapped("Enter exact class names, one per line. Subclasses are not included. Each class must occur in every selected image.");
            constexpr const char* labels[]={"Window classes","Tissue classes","Exclusion classes (optional)","Cell classes"};
            for(std::size_t i=0;i<4;++i) {
                if(i==3){changed=ui::Checkbox("All Cell objects (including unclassified)",&all_cells_) || changed;ImGui::BeginDisabled(all_cells_);}
                ImGui::SetNextItemWidth(-1);const auto original="##density-class-"+std::to_string(i);
                changed=ui::legacy_field(labels[i],original.c_str(),[&](const char* id){const bool edited=ImGui::InputTextMultiline(id,classes_[i].data(),classes_[i].size(),{0,std::max(42.f,ImGui::GetFrameHeight())});ui::trace_workflow_action(("density-class-"+std::to_string(i)).c_str(),labels[i]);return edited;},true) || changed;
                if(i==3)ImGui::EndDisabled();
            }
            changed=ui::Checkbox("Prefer nucleus centroids",&prefer_nucleus_) || changed;
            changed=ui::Checkbox("Accept 512-segment curve approximation",&curves_) || changed;
            if(changed){preview_.reset();error_.clear();}
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!enabled);
            if(ui::WorkflowAction("Preview cohorts and calibration",preview_ && preview_->valid()?ui::design::Action::Secondary:ui::design::Action::Primary))attempt([&]{preview_.reset();task_source_=source_;if(job_.preview(source_,recipe()))++notice_generation_;});
            ImGui::EndDisabled();
            ui::TextDisabled("Checks image classes and calibration.");
            const auto shown=preview_?preview_:(result && result->state!="preview"?result:nullptr);
            if(shown && ImGui::BeginTable("Density preview",8,ImGuiTableFlags_Borders|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,ui::CollectionHeight(shown->images.size()+1)})) {
                for(const auto* label:{"Image ID","Windows","Tissue","Excluded","Cells"})ImGui::TableSetupColumn(label);ImGui::TableSetupColumn(ui::unit_text("%s X,Y",ui::unit::um_per_px).c_str());for(const auto* label:{"State","Issue / reuse"})ImGui::TableSetupColumn(label);
                ImGui::TableHeadersRow();
                for(const auto& row:shown->images) {
                    ImGui::TableNextRow();ImGui::TableNextColumn();ui::DataTextf("%llu",static_cast<unsigned long long>(row.id));if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",row.name.c_str());
                    for(const auto& ids:row.ids){ImGui::TableNextColumn();ui::DataTextf("%zu",ids.size());}
                    ImGui::TableNextColumn();if(row.x && row.y)ui::DataTextf("%.6g, %.6g",*row.x,*row.y);else ui::DataText("Missing");
                    ImGui::TableNextColumn();ui::DataText(ui::caption(ui::parse_run_state(row.state),row.state).c_str());ImGui::TableNextColumn();
                    if(!row.error.empty())ui::DataTextWrapped("%s",row.error.c_str());
                    else if(row.state!="preview"){ui::DataText(row.reuse=="reused"?"Unchanged":"Fresh");if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",row.reuse_reason.c_str());}
                }ImGui::EndTable();
            }
            ImGui::BeginDisabled(busy || blocked);
            previous_.draw(context,false,[]{ui::trace_workflow_action("density-previous","Previous results folder (optional)");},{},"Previous results folder (optional)");
            ui::TextWrapped("Completed results can be reused. Changed or missing results are recomputed; cache errors are reported.");
            output_.draw(context,false,[]{ui::trace_workflow_action("density-output","New output folder");},{},"New output folder");ImGui::EndDisabled();
            const bool ready=enabled && preview_ && preview_->valid() && preview_->source==source_ && output_.text()[0];
            ImGui::BeginDisabled(!ready);
            if(ui::WorkflowAction("Run project density",preview_ && preview_->valid()?ui::design::Action::Primary:ui::design::Action::Secondary))attempt([&]{auto reviewed=*preview_;task_source_=reviewed.source;if(job_.run(reviewed,output_.state().resolved,previous_.state().resolved))++notice_generation_;});
            ImGui::EndDisabled();
            if(busy) {ImGui::SameLine();if(ui::WorkflowIcon("Cancel density",ui::Icon::Stop))job_.cancel();}
            if(busy) {
                ui::DataTextWrapped("Task source: %s",task_source_.c_str());
                const auto& p=job_.progress();const auto total=p.total.load(),completed=p.completed.load();
                ImGui::ProgressBar(total?float(completed)/float(total):0);
                constexpr const char* phases[]={"Opening saved project","Previewing","Computing","Saving image result","Writing CSV/report","Checking saved results"};
                ui::DataTextf("%s; image %llu; %zu/%zu",ui::i18n::text(phases[std::clamp(p.phase.load(),0,5)]),static_cast<unsigned long long>(p.image.load()),completed,total);
                ui::DataTextf("Computations started: %zu; reused: %zu",p.kernel_calls.load(),p.reused.load());
                if(job_.cancelling())ui::TextWrapped("Stopping after the current operation. Completed results are kept.");
            }
            if(result && result->state!="preview") {
                ui::DataTextWrapped("Run source: %s",result->source.c_str());
                ui::DataTextf("%s: %zu complete, %zu computed, %zu reused, %zu failed, %zu rows",ui::caption(ui::parse_run_state(result->state),result->state).c_str(),result->completed,result->computed,result->reused,result->failed,result->rows);
                if(!result->error.empty())ui::DataTextWrapped("%s",result->error.c_str());
                ui::DataTextWrapped("CSV: %s",result->csv.c_str());ui::DataTextWrapped("Report: %s",result->report.c_str());
                if(ui::Button("Open CSV"))ui::open_in_shell(result->csv);ImGui::SameLine();if(ui::Button("Open results folder"))ui::open_in_shell(result->output);
            }
            if(!job_.error().empty())ui::DataTextWrapped("%s",job_.error().c_str());
            if(!error_.empty())ui::DataTextWrapped("%s",error_.c_str());
            if(ui::CollapsingHeader("Details###Density details")) {
                ui::TextWrapped("Default analysis limits apply. Existing output folders are not replaced.");
                ui::TextWrapped("Reuse requires the same engine and captured inputs. Reused results are not rechecked numerically.");
                ui::TextWrapped("Completed image results and reports are kept after errors or cancellation.");
            }
        }catch(const std::exception& e){error_=e.what();}
        ImGui::End();
    }
};
}
