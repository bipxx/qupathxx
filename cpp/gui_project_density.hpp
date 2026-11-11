#pragma once
#include "qupathxx/density_workflow.hpp"
#include "qupathxx/project.hpp"
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
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
    friend struct GuiProjectDensityTestAccess;
    ProjectDensityJob job_;
    bool open_=false,all_cells_=true,prefer_nucleus_=true,curves_=false;
    std::array<std::array<char,4097>,4> classes_{};
    std::array<char,32769> output_{},previous_{};
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
    static void open_file(const std::string& path) {
        if(path.empty() || !std::filesystem::exists(std::filesystem::u8path(path)))throw std::runtime_error("Result path is unavailable");
#ifdef _WIN32
        if(reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr,L"open",std::filesystem::u8path(path).c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
            throw std::runtime_error("Windows could not open the result path");
#else
        throw std::runtime_error("Open the displayed result path in your file manager");
#endif
    }
public:
    void finish(){job_.finish();}
    void open(){open_=true;}
    void open_button(){if(ui::Button("Project density..."))open_=true;}
    void draw(const Project* project,const std::vector<ProjectImage>& catalog,bool blocked) {
        job_.poll();auto result=job_.result();
        if(result && result!=observed_) {observed_=result;if(result->state=="preview")preview_=result;}
        if(!open_)return;
        ui::SetNextToolWindow("Project-wide density",{760,720});
        if(!ImGui::Begin(ui::i18n::label("Project-wide density").c_str(),&open_)){ImGui::End();return;}
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
            if(!source_.empty())ImGui::TextWrapped("Source: %s",source_.c_str());
            ImGui::BeginDisabled(busy || blocked);
            bool changed=false;
            if(ui::IconButton("All images",ui::Icon::SelectAll)){images_.clear();for(const auto& image:catalog)images_.insert(image.id);changed=true;}ImGui::SameLine();
            if(ui::IconButton("Clear images",ui::Icon::Deselect)){images_.clear();changed=true;}
            if(ui::BeginCollection("Density images",catalog.size())) {
                for(const auto& image:catalog) {
                    ImGui::PushID(std::to_string(image.id).c_str());bool selected=images_.count(image.id)!=0;
                    if(ImGui::Checkbox(image.name.c_str(),&selected)){if(selected)images_.insert(image.id);else images_.erase(image.id);changed=true;}
                    ImGui::SameLine();ImGui::TextDisabled("ID %llu",static_cast<unsigned long long>(image.id));ImGui::PopID();
                }
            }ImGui::EndChild();
            ui::TextWrapped("Enter exact class names, one per line. Subclasses are not included. Each class must occur in every selected image.");
            constexpr const char* labels[]={"Window classes","Tissue classes","Exclusion classes (optional)","Cell classes"};
            for(std::size_t i=0;i<4;++i) {
                if(i==3){changed=ui::Checkbox("All Cell objects (including unclassified)",&all_cells_) || changed;ImGui::BeginDisabled(all_cells_);}
                ImGui::SetNextItemWidth(-1);ImGui::TextUnformatted(labels[i]);
                changed=ImGui::InputTextMultiline(("##density-class-"+std::to_string(i)).c_str(),classes_[i].data(),classes_[i].size(),{0,42}) || changed;
                ui::trace_workflow_action(("density-class-"+std::to_string(i)).c_str(),labels[i]);
                if(i==3)ImGui::EndDisabled();
            }
            changed=ui::Checkbox("Prefer nucleus centroids",&prefer_nucleus_) || changed;
            changed=ui::Checkbox("Accept 512-segment curve approximation",&curves_) || changed;
            if(changed){preview_.reset();error_.clear();}
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!enabled);
            if(ui::WorkflowAction("Preview cohorts and calibration","Check inputs",nullptr,preview_ && preview_->valid()?ui::design::Action::Secondary:ui::design::Action::Primary))attempt([&]{preview_.reset();task_source_=source_;job_.preview(source_,recipe());});
            ImGui::EndDisabled();
            ui::TextDisabled("Checks image classes and calibration.");
            const auto shown=preview_?preview_:(result && result->state!="preview"?result:nullptr);
            if(shown && ImGui::BeginTable("Density preview",8,ImGuiTableFlags_Borders|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,ui::CollectionHeight(shown->images.size()+1)})) {
                for(const auto* label:{"Image ID","Windows","Tissue","Excluded","Cells","um/px X,Y","State","Issue / reuse"})ImGui::TableSetupColumn(label);
                ImGui::TableHeadersRow();
                for(const auto& row:shown->images) {
                    ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(row.id));if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",row.name.c_str());
                    for(const auto& ids:row.ids){ImGui::TableNextColumn();ImGui::Text("%zu",ids.size());}
                    ImGui::TableNextColumn();if(row.x && row.y)ImGui::Text("%.6g, %.6g",*row.x,*row.y);else ImGui::TextUnformatted("Missing");
                    ImGui::TableNextColumn();ImGui::TextUnformatted(row.state.c_str());ImGui::TableNextColumn();
                    if(!row.error.empty())ImGui::TextWrapped("%s",row.error.c_str());
                    else if(row.state!="preview"){ImGui::TextUnformatted(row.reuse=="reused"?"Unchanged":"Fresh");if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",row.reuse_reason.c_str());}
                }ImGui::EndTable();
            }
            ImGui::BeginDisabled(busy || blocked);ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##density-previous",ui::i18n::text("Previous results folder (optional)"),previous_.data(),previous_.size());
            ui::trace_workflow_action("density-previous","Previous results folder (optional)");
            ui::TextWrapped("Completed results can be reused. Changed or missing results are recomputed; cache errors are reported.");
            ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##density-output",ui::i18n::text("New output folder"),output_.data(),output_.size());ui::trace_workflow_action("density-output","New output folder");ImGui::EndDisabled();
            const bool ready=enabled && preview_ && preview_->valid() && preview_->source==source_ && output_[0];
            ImGui::BeginDisabled(!ready);
            if(ui::WorkflowAction("Run project density","Run density",nullptr,preview_ && preview_->valid()?ui::design::Action::Primary:ui::design::Action::Secondary))attempt([&]{auto reviewed=*preview_;task_source_=reviewed.source;job_.run(reviewed,output_.data(),previous_.data());preview_.reset();});
            ImGui::EndDisabled();
            if(busy) {ImGui::SameLine();if(ui::WorkflowIcon("Cancel density","Cancel density",ui::Icon::Stop))job_.cancel();}
            if(busy) {
                ImGui::TextWrapped("Task source: %s",task_source_.c_str());
                const auto& p=job_.progress();const auto total=p.total.load(),completed=p.completed.load();
                ImGui::ProgressBar(total?float(completed)/float(total):0);
                constexpr const char* phases[]={"Opening saved project","Previewing","Computing","Saving image result","Publishing CSV/report","Checking saved results"};
                ImGui::Text("%s; image %llu; %zu/%zu",phases[std::clamp(p.phase.load(),0,5)],static_cast<unsigned long long>(p.image.load()),completed,total);
                ImGui::Text("Computations started: %zu; reused: %zu",p.kernel_calls.load(),p.reused.load());
                if(job_.cancelling())ui::TextWrapped("Stopping after the current operation. Completed results are kept.");
            }
            if(result && result->state!="preview") {
                ImGui::TextWrapped("Run source: %s",result->source.c_str());
                ImGui::Text("%s: %zu complete, %zu computed, %zu reused, %zu failed, %zu rows",result->state.c_str(),result->completed,result->computed,result->reused,result->failed,result->rows);
                if(!result->error.empty())ImGui::TextWrapped("%s",result->error.c_str());
                ImGui::TextWrapped("CSV: %s",result->csv.c_str());ImGui::TextWrapped("Report: %s",result->report.c_str());
                if(ui::Button("Open CSV"))open_file(result->csv);ImGui::SameLine();if(ui::Button("Open results folder"))open_file(result->output);
            }
            if(!job_.error().empty())ImGui::TextWrapped("%s",job_.error().c_str());
            if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
            if(ui::CollapsingHeader("Details###Density details")) {
                ui::TextWrapped("Default analysis limits apply. Existing output folders are not replaced.");
                ui::TextWrapped("Reuse requires the same engine and captured inputs. Reused results are not rechecked numerically.");
                ui::TextWrapped("Completed image results and reports are retained after errors or cancellation.");
            }
        }catch(const std::exception& e){error_=e.what();}
        ImGui::End();
    }
};
}
