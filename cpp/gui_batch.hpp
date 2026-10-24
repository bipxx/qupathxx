#pragma once
#include "qupathxx/project_batch.hpp"
#include "gui_layout.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace qpx::detail {
inline void reset_cell_detector(CellParameters& p) {p.algorithm=CellAlgorithm::DistanceWatershed;p.log_prominence=.5;p.context_padding=32;}
inline bool cell_detector_controls(CellParameters& p) {
    int algorithm=static_cast<int>(p.algorithm);bool changed=ui::Combo("Cell detector",&algorithm,"Distance watershed\0LoG watershed\0");
    if(changed) {p.algorithm=static_cast<CellAlgorithm>(algorithm);if(p.algorithm==CellAlgorithm::DistanceWatershed) reset_cell_detector(p);}
    if(p.algorithm==CellAlgorithm::LogWatershed) {
        changed=ui::InputDouble("Peak prominence",&p.log_prominence,0,0,"%.3g") || changed;
        changed=ui::InputScalar("Context (sample px)",ImGuiDataType_U32,&p.context_padding) || changed;
    }
    return changed;
}
inline bool cell_area_controls(std::optional<double>& minimum,std::optional<double>& maximum) {
    bool changed=false;
    if(ImGui::CollapsingHeader("Physical nucleus area",(minimum || maximum)?ImGuiTreeNodeFlags_DefaultOpen:0)) {
        auto bound=[&](const char* toggle,const char* label,std::optional<double>& value) {
            bool enabled=bool(value);
            if(ui::Checkbox(toggle,&enabled)) {value=enabled?std::optional<double>(0):std::nullopt;changed=true;}
            if(value) {double number=*value;if(ui::InputDouble(label,&number,0,0,"%.8g")) {value=number;changed=true;}}
        };
        bound("Minimum area (um^2)","Area minimum >=",minimum);
        bound("Maximum area (um^2)","Area maximum <=",maximum);
        if(maximum && *maximum==0)
            ImGui::TextWrapped("Maximum zero keeps no nuclei. Disable the maximum to remove the upper bound.");
        ImGui::TextWrapped("Uses image calibration and the selected sampling level. Both sampled and physical size limits apply.");
    }
    return changed;
}
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
    void open_tma() {open_=true;options_.tma_only=true;options_.include_missing_tma=false;options_.nucleus=false;selected_only_=true;}
    bool busy() const {return job_.busy();}
    bool finished() const {return !job_.busy() && stats_.state!="idle";}
    void finish() {if(job_.busy()) job_.cancel();stats_=job_.wait();}
    const ProjectBatchStats& stats() const {return stats_;}
    const std::string& error() const {return error_.empty()?stats_.error:error_;}
    std::shared_ptr<const ProjectBatchReport> result() const {return job_.result();}
    template<class Transform>
    void draw(const Project* project,std::uint64_t image_id,const ObjectSelection* selection,bool blocked,const TissueParameters& tissue,const CellParameters& cells,
        std::optional<double> cell_min_area,std::optional<double> cell_max_area,AnalysisKind roi_kind,RoiSamplingPolicy roi_sampling_policy,bool nucleus,std::shared_ptr<const PixelClassifier> pixel_model,Transform&& transform) {
        const auto& io=ImGui::GetIO();
        const bool shortcut=!io.WantTextInput && io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_B,false);
        bool start=shortcut && !io.KeyShift && !blocked;
        bool cancel=shortcut && io.KeyShift;
        if(shortcut) open_=true;
        if(open_) {
            const float scale=ImGui::GetFontSize()/13.f;
            const ImVec2 initial_size{std::min(460.f*scale,io.DisplaySize.x-20),std::min(720.f*scale,io.DisplaySize.y-40)};
            ImGui::SetNextWindowSize(initial_size,ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos({std::max(10.f,io.DisplaySize.x-initial_size.x-20),std::min(88.f*scale,std::max(10.f,io.DisplaySize.y-initial_size.y-10))},ImGuiCond_FirstUseEver);
            if(ImGui::Begin("Project batch",&open_)) {
            ImGui::PushID("project-batch");
            ImGui::BeginDisabled(job_.busy() || blocked);
            ui::SetNextItemWidth(240);
            ui::InputTextWithHint("##output","Output directory",output_.data(),output_.size());
            int kind=static_cast<int>(options_.kind);
            if(ui::Combo("Workflow",&kind,"ROI means\0Tissue\0Cells\0Pixel streams\0Raw channel statistics\0")) options_.kind=static_cast<ProjectBatchKind>(kind);
            ui::Checkbox("TMA cores only",&options_.tma_only);
            if(options_.tma_only) {ui::Checkbox("Include missing cores",&options_.include_missing_tma);options_.nucleus=false;}else options_.include_missing_tma=false;
            ui::Checkbox("Selected objects only",&selected_only_);
            ImGui::TextWrapped(selected_only_?"Selected objects in the active image.":"Matching area objects in every project image.");
            if(!selected_only_ && !options_.tma_only) {
                int object_kind=static_cast<int>(options_.object_kind);
                if(ui::Combo("Object kind",&object_kind,"Annotations\0Detections\0Cells\0")) options_.object_kind=static_cast<ObjectKind>(object_kind);
            }
            if(options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw) ui::Checkbox("Use image analysis settings",&use_image_settings_);
            if(use_image_settings_ && options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw) ImGui::TextWrapped("Uses the Region analysis, Tissue and Cells settings in the image inspector.");
            ImGui::BeginDisabled(use_image_settings_ && options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw);
            if(options_.kind!=ProjectBatchKind::Roi) options_.roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
            if(options_.kind!=ProjectBatchKind::Tissue) options_.tissue.sampling_policy=RoiSamplingPolicy::GeometryCoverage;
            if(options_.kind!=ProjectBatchKind::Cells) reset_cell_detector(options_.cells);
            if(options_.kind!=ProjectBatchKind::Cells) options_.publish_cells=false;
            if(options_.kind!=ProjectBatchKind::Cells) {options_.cell_min_nucleus_area_um2.reset();options_.cell_max_nucleus_area_um2.reset();}
            if(options_.kind==ProjectBatchKind::Cells || options_.kind==ProjectBatchKind::Pixels) options_.nucleus=false;
            if(options_.kind==ProjectBatchKind::Pixels) {
                ImGui::BeginDisabled(!pixel_model);if(ui::Button("Use active pixel model") && pixel_model) options_.pixel_model=*pixel_model;ImGui::EndDisabled();
                if(options_.pixel_model) ImGui::TextWrapped("Pinned model: %s",options_.pixel_model->sha256().c_str());
                else ImGui::TextWrapped(pixel_model?"Run captures the active pixel model.":"Train or load a pixel model in the image inspector first.");
                int block=static_cast<int>(options_.pixels.block_size),tile=static_cast<int>(options_.pixels.prediction.tile_size),mib=static_cast<int>(options_.pixels.prediction.work_bytes/(1024*1024));
                if(ui::InputInt("Block size",&block)) options_.pixels.block_size=static_cast<std::size_t>(block);
                if(ui::InputInt("Prediction tile size",&tile)) options_.pixels.prediction.tile_size=static_cast<std::size_t>(tile);
                if(ui::InputInt("Prediction memory (MiB)",&mib)) options_.pixels.prediction.work_bytes=static_cast<std::size_t>(std::clamp(mib,1,1024))*1024*1024;
                ui::InputDouble("Minimum vote",&options_.pixels.prediction.min_vote_fraction,0,0,"%.5g");
            } else if(options_.kind==ProjectBatchKind::Raw) {
                ui::Checkbox("Use each image's displayed Z/time",&options_.raw_follow_display);
                auto axis=[&](const char* label,std::uint32_t& value,int minimum=0) {int n=static_cast<int>(value);if(ui::InputInt(label,&n)) value=static_cast<std::uint32_t>(std::clamp(n,minimum,minimum?65536:65535));};
                ui::InputInt("Raw pyramid level",&options_.raw.region.level);axis("First channel (zero-based)",options_.raw.first_channel);axis("Channel count",options_.raw.channels,1);
                if(!options_.raw_follow_display) {axis("Z (zero-based)",options_.raw.z);axis("Time (zero-based)",options_.raw.t);}
                if(!options_.tma_only) ui::Checkbox("Use nucleus",&options_.nucleus);
            } else if(options_.kind==ProjectBatchKind::Roi) {
                int sampling=static_cast<int>(options_.roi_sampling_policy);
                if(ui::Combo("ROI polygon samples",&sampling,"Geometry coverage\0Polygon winding\0")) options_.roi_sampling_policy=static_cast<RoiSamplingPolicy>(sampling);
                int feature=static_cast<int>(options_.roi_kind);
                if(ui::Combo("Features",&feature,"RGB optical density\0Supplied stains\0")) options_.roi_kind=static_cast<AnalysisKind>(feature);
                if(!options_.tma_only) ui::Checkbox("Use nucleus",&options_.nucleus);
            } else {
                if(options_.kind==ProjectBatchKind::Tissue) {
                    int sampling=static_cast<int>(options_.tissue.sampling_policy);
                    if(ui::Combo("Tissue polygon samples",&sampling,"Geometry coverage\0Polygon winding\0")) options_.tissue.sampling_policy=static_cast<RoiSamplingPolicy>(sampling);
                }
                auto& feature=options_.kind==ProjectBatchKind::Tissue?options_.tissue.feature:options_.cells.feature;
                int selected_feature=static_cast<int>(feature);
                if(ui::Combo("Feature",&selected_feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0")) feature=static_cast<TissueFeature>(selected_feature);
                auto& threshold=options_.kind==ProjectBatchKind::Tissue?options_.tissue.threshold:options_.cells.threshold;
                ui::InputDouble("Threshold >=",&threshold,0,0,"%.5g");
                auto& level=options_.kind==ProjectBatchKind::Tissue?options_.tissue.level:options_.cells.level;
                ui::InputInt("Level (-1=auto)",&level);
                if(options_.kind==ProjectBatchKind::Cells) cell_detector_controls(options_.cells);
                if(options_.kind==ProjectBatchKind::Cells)
                    cell_area_controls(options_.cell_min_nucleus_area_um2,options_.cell_max_nucleus_area_um2);

            }
            ImGui::EndDisabled();
            if(options_.kind==ProjectBatchKind::Cells) ui::Checkbox("Publish cell objects",&options_.publish_cells);
            int failure=static_cast<int>(options_.failure);
            if(ui::Combo("On image error",&failure,"Stop\0Continue\0")) options_.failure=static_cast<ProjectBatchFailure>(failure);
            ui::Checkbox("Reuse matching checkpoints",&options_.resume);
            ImGui::TextWrapped(options_.kind==ProjectBatchKind::Raw?"Measures original TIFF samples at the selected axes. Each ROI retains its input hash and original labels in an independent checkpoint.":options_.kind==ProjectBatchKind::Pixels?"Uses the pinned pixel model. Each target produces a verified QPS file; only complete image bundles are reused. The open project is unchanged.":"Uses the current white/stain vectors. Results go to independent checkpoints; the open project is unchanged.");
            ImGui::BeginDisabled(!project || !project->active_image());
            start=ui::Button("Run batch (Ctrl+Alt+B)") || start;
            ImGui::EndDisabled();ImGui::EndDisabled();
            if(job_.busy()) cancel=ui::Button("Cancel batch") || cancel;
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
                if(report->images.size()>20) ui::TextDisabled("More images in the run report.");
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
                if(run_options.kind==ProjectBatchKind::Pixels) {if(!run_options.pixel_model && pixel_model) run_options.pixel_model=*pixel_model;run_options.nucleus=false;}
                else if(use_image_settings_ && run_options.kind!=ProjectBatchKind::Raw) {
                    run_options.tissue=tissue;run_options.cells=cells;run_options.roi_kind=roi_kind;run_options.roi_sampling_policy=roi_sampling_policy;
                    run_options.nucleus=(run_options.kind==ProjectBatchKind::Cells || run_options.tma_only)?false:nucleus;
                    if(run_options.kind==ProjectBatchKind::Cells) {run_options.cell_min_nucleus_area_um2=cell_min_area;run_options.cell_max_nucleus_area_um2=cell_max_area;}
                }
                if(run_options.kind!=ProjectBatchKind::Roi) run_options.roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
                if(run_options.kind!=ProjectBatchKind::Tissue) run_options.tissue.sampling_policy=RoiSamplingPolicy::GeometryCoverage;
                if(run_options.kind!=ProjectBatchKind::Cells) reset_cell_detector(run_options.cells);
                const bool stains=run_options.kind!=ProjectBatchKind::Raw && run_options.kind!=ProjectBatchKind::Pixels && (run_options.kind==ProjectBatchKind::Roi?run_options.roi_kind==AnalysisKind::StainMeans:
                    (run_options.kind==ProjectBatchKind::Tissue?run_options.tissue.feature:run_options.cells.feature)!=TissueFeature::OpticalDensitySum);
                ProjectBatchRequest request(*project,transform(stains),chosen,run_options);
                job_.request(request,output_.data());error_.clear();poll();
            } catch(const std::exception& e) {error_=e.what();}
        }
    }
};
}
