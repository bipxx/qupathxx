#pragma once
#include "qupathxx/project_batch.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace qpx::detail {
// The owner captures once. Workers process an independent project snapshot;
// no completion callback publishes into the currently displayed workspace.
class GuiBatch {
    ProjectBatchJob job_;
    ProjectBatchOptions options_;
    std::array<char,32769> output_{};
    bool selected_only_=false,use_image_settings_=true,open_=false;
    std::string error_;
    ProjectBatchStats stats_;
public:
    explicit GuiBatch(const GuiOptions& initial):options_(initial.batch_options?*initial.batch_options:ProjectBatchOptions{}),selected_only_(initial.batch_selected_objects),use_image_settings_(!initial.batch_options) {
        options_.validate();
        const auto& path=initial.batch_output_directory;
        if(path.size()>32768 || path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid batch output directory");
        std::copy(path.begin(),path.end(),output_.begin());
    }
    void poll() {stats_=job_.poll();}
    void open() {open_=true;}
    bool busy() const {return job_.busy();}
    bool finished() const {return !job_.busy() && stats_.state!="idle";}
    void finish() {if(job_.busy()) job_.cancel();stats_=job_.wait();}
    const ProjectBatchStats& stats() const {return stats_;}
    const std::string& error() const {return error_.empty()?stats_.error:error_;}
    std::shared_ptr<const ProjectBatchReport> result() const {return job_.result();}
    template<class Transform>
    void draw(const Project* project,std::uint64_t image_id,const ObjectSelection* selection,bool blocked,const TissueParameters& tissue,const CellParameters& cells,AnalysisKind roi_kind,bool nucleus,Transform&& transform) {
        const auto& io=ImGui::GetIO();
        const bool shortcut=!io.WantTextInput && io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_B,false);
        bool start=shortcut && !io.KeyShift && !blocked;
        bool cancel=shortcut && io.KeyShift;
        if(shortcut) open_=true;
        if(open_) {
            ImGui::SetNextWindowSize({std::min(460.f,io.DisplaySize.x-20),std::min(720.f,io.DisplaySize.y-100)},ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos({std::max(10.f,io.DisplaySize.x-480),88},ImGuiCond_FirstUseEver);
            if(ImGui::Begin("Project batch",&open_)) {
            ImGui::PushID("project-batch");
            ImGui::BeginDisabled(job_.busy() || blocked);
            ImGui::SetNextItemWidth(240);
            ImGui::InputTextWithHint("##output","Output directory",output_.data(),output_.size());
            int kind=static_cast<int>(options_.kind);
            if(ImGui::Combo("Workflow",&kind,"ROI means\0Tissue\0Cells\0")) options_.kind=static_cast<ProjectBatchKind>(kind);
            ImGui::Checkbox("Selected objects only",&selected_only_);
            ImGui::TextWrapped(selected_only_?"Selected objects in the active image.":"Matching area objects in every project image.");
            if(!selected_only_) {
                int object_kind=static_cast<int>(options_.object_kind);
                if(ImGui::Combo("Object kind",&object_kind,"Annotations\0Detections\0Cells\0")) options_.object_kind=static_cast<ObjectKind>(object_kind);
            }
            ImGui::Checkbox("Use image analysis settings",&use_image_settings_);
            if(use_image_settings_) ImGui::TextWrapped("Uses the Region analysis, Tissue and Cells settings in the image inspector.");
            ImGui::BeginDisabled(use_image_settings_);
            if(options_.kind!=ProjectBatchKind::Cells) options_.publish_cells=false;
            if(options_.kind==ProjectBatchKind::Cells) options_.nucleus=false;
            if(options_.kind==ProjectBatchKind::Roi) {
                int feature=static_cast<int>(options_.roi_kind);
                if(ImGui::Combo("Features",&feature,"RGB optical density\0Supplied stains\0")) options_.roi_kind=static_cast<AnalysisKind>(feature);
                ImGui::Checkbox("Use nucleus",&options_.nucleus);
            } else {
                auto& feature=options_.kind==ProjectBatchKind::Tissue?options_.tissue.feature:options_.cells.feature;
                int selected_feature=static_cast<int>(feature);
                if(ImGui::Combo("Feature",&selected_feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0")) feature=static_cast<TissueFeature>(selected_feature);
                auto& threshold=options_.kind==ProjectBatchKind::Tissue?options_.tissue.threshold:options_.cells.threshold;
                ImGui::InputDouble("Threshold >=",&threshold,0,0,"%.5g");
                auto& level=options_.kind==ProjectBatchKind::Tissue?options_.tissue.level:options_.cells.level;
                ImGui::InputInt("Level (-1=auto)",&level);

            }
            ImGui::EndDisabled();
            if(options_.kind==ProjectBatchKind::Cells) ImGui::Checkbox("Publish cell objects",&options_.publish_cells);
            int failure=static_cast<int>(options_.failure);
            if(ImGui::Combo("On image error",&failure,"Stop\0Continue\0")) options_.failure=static_cast<ProjectBatchFailure>(failure);
            ImGui::Checkbox("Reuse matching checkpoints",&options_.resume);
            ImGui::TextWrapped("Uses the current white/stain vectors. Results go to independent checkpoints; the open project is unchanged.");
            ImGui::BeginDisabled(!project || !project->active_image());
            start=ImGui::Button("Run batch (Ctrl+Alt+B)") || start;
            ImGui::EndDisabled();ImGui::EndDisabled();
            if(job_.busy()) cancel=ImGui::Button("Cancel batch") || cancel;
            ImGui::TextWrapped("Batch: %s / %s",stats_.state.c_str(),stats_.phase.c_str());
            if(stats_.total_images) {
                ImGui::ProgressBar(float(double(stats_.completed_images)/stats_.total_images));
                ImGui::TextWrapped("Images: %zu / %zu   Objects: %zu / %zu",stats_.completed_images,stats_.total_images,stats_.completed_objects,stats_.total_objects);
            }
            if(const auto report=job_.result()) {
                ImGui::TextWrapped("Saved: %zu   Reused: %zu   Failed: %zu",report->saved,report->reused,report->failed);
                if(!report->report_path.empty()) ImGui::TextWrapped("Run report: %s",report->report_path.c_str());
                // Keep a large project report from creating unbounded widgets.
                for(std::size_t i=0;i<std::min<std::size_t>(report->images.size(),20);++i) {
                    const auto& image=report->images[i];
                    ImGui::TextWrapped("%s: %s%s%s",image.image_name.c_str(),image.state.c_str(),image.error.empty()?"":" / ",image.error.c_str());
                }
                if(report->images.size()>20) ImGui::TextDisabled("More images in the run report.");
            }
            if(!error().empty()) ImGui::TextWrapped("%s",error().c_str());
            ImGui::PopID();
            }
            ImGui::End();
        }
        if(cancel) job_.cancel();
        if(start && !blocked && !job_.busy()) {
            try {
                if(!project || !project->active_image()) throw std::invalid_argument("batch requires a nonempty project");
                if(!*output_.data()) throw std::invalid_argument("enter a batch output directory");
                std::vector<ProjectBatchSelection> chosen;
                if(selected_only_) {
                    if(!selection || !image_id || selection->size()==0) throw std::invalid_argument("select at least one area object for this batch");
                    chosen.push_back({image_id,selection->ids()});
                }
                auto run_options=options_;
                if(use_image_settings_) {run_options.tissue=tissue;run_options.cells=cells;run_options.roi_kind=roi_kind;run_options.nucleus=run_options.kind==ProjectBatchKind::Cells?false:nucleus;}
                const bool stains=run_options.kind==ProjectBatchKind::Roi?run_options.roi_kind==AnalysisKind::StainMeans:
                    (run_options.kind==ProjectBatchKind::Tissue?run_options.tissue.feature:run_options.cells.feature)!=TissueFeature::OpticalDensitySum;
                ProjectBatchRequest request(*project,transform(stains),chosen,run_options);
                job_.request(request,output_.data());error_.clear();poll();
            } catch(const std::exception& e) {error_=e.what();}
        }
    }
};
}
