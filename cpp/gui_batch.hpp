#pragma once
#include "qupathxx/project_batch.hpp"
#include "gui_layout.hpp"
#include "gui_raw_grid_controls.hpp"
#include "gui_raw_axis.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace qpx::detail {
inline void reset_cell_detector(CellParameters& p) {p.algorithm=CellAlgorithm::DistanceWatershed;p.log_prominence=.5;p.context_padding=32;}
inline bool cell_detector_controls(CellParameters& p) {
    int algorithm=static_cast<int>(p.algorithm);bool changed=ui::Combo("Cell detector",&algorithm,"Distance watershed\0LoG watershed\0LoG with watershed bodies\0LoG sampled shape\0");
    if(changed) {
        p.algorithm=static_cast<CellAlgorithm>(algorithm);if(p.algorithm==CellAlgorithm::DistanceWatershed) reset_cell_detector(p);
        if(p.algorithm==CellAlgorithm::SampledLogWatershedBodies) {p.min_peak_radius=0;p.min_seed_distance=0;p.connectivity=8;}
    }
    if(p.algorithm!=CellAlgorithm::DistanceWatershed) {
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
inline bool grid_cell_controls(GridCellSampling& grid,GridCellParameters& p) {
    bool changed=false;
    auto count=[&](const char* label,std::uint32_t& value,int minimum,int maximum) {
        int n=static_cast<int>(value);if(ui::InputInt(label,&n)) {value=static_cast<std::uint32_t>(std::clamp(n,minimum,maximum));changed=true;}
    };
    count("Grid samples X",grid.samples_x,1,1000000);count("Grid samples Y",grid.samples_y,1,1000000);
    count("Grid context X",grid.context_x,0,4096);count("Grid context Y",grid.context_y,0,4096);
    int units=static_cast<int>(p.units),feature=static_cast<int>(p.feature),policy=static_cast<int>(p.sampling_policy);
    if(ui::Combo("Distance units",&units,"Level-zero pixels\0Micrometres\0")) {p.units=static_cast<GridCellUnits>(units);changed=true;}
    if(ui::Combo("Grid feature",&feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0")) {p.feature=static_cast<TissueFeature>(feature);changed=true;}
    if(ui::Combo("Grid ROI samples",&policy,"Geometry coverage\0Polygon winding\0")) {p.sampling_policy=static_cast<RoiSamplingPolicy>(policy);changed=true;}
    changed=ui::InputDouble("Grid threshold",&p.threshold,0,0,"%.5g") || changed;
    changed=ui::InputDouble("Grid sigma",&p.sigma,0,0,"%.5g") || changed;
    changed=ui::InputDouble("Grid peak prominence",&p.log_prominence,0,0,"%.5g") || changed;
    changed=ui::InputDouble("Grid peak radius >=",&p.min_peak_radius,0,0,"%.5g") || changed;
    changed=ui::InputDouble("Grid seed spacing",&p.min_seed_distance,0,0,"%.5g") || changed;
    changed=ui::InputDouble("Grid body expansion",&p.cell_expansion,0,0,"%.5g") || changed;
    changed=ui::InputDouble("Grid nucleus area >=",&p.min_nucleus_area,0,0,"%.8g") || changed;
    bool maximum=bool(p.max_nucleus_area);
    if(ui::Checkbox("Limit grid nucleus area",&maximum)) {p.max_nucleus_area=maximum?std::optional<double>(p.min_nucleus_area):std::nullopt;changed=true;}
    if(p.max_nucleus_area) changed=ui::InputDouble("Grid nucleus area <=",&*p.max_nucleus_area,0,0,"%.8g") || changed;
    int memory=static_cast<int>(p.work_bytes/(1024*1024));
    if(ui::InputInt("Grid memory (MiB)",&memory)) {p.work_bytes=std::size_t(std::clamp(memory,1,512))*1024*1024;changed=true;}
    ImGui::TextWrapped("Counts cover the integer enclosure of each ROI. Distances use the selected units; areas use those units squared. Micrometres require both calibration axes.");
    return changed;
}
// The owner captures once. Workers process an independent project snapshot;
// no completion callback publishes into the currently displayed workspace.
class GuiBatch {
    ProjectBatchJob job_;
    ProjectBatchOptions options_;
    std::array<char,32769> output_{},raw_model_path_{};
    std::array<char,128> raw_x_{},raw_y_{};
    bool selected_only_=false,use_image_settings_=true,open_=false;
    std::string error_;
    ProjectBatchStats stats_;
public:
    explicit GuiBatch(const GuiOptions& initial):options_(initial.batch_options?*initial.batch_options:ProjectBatchOptions{}),selected_only_(initial.batch_selected_objects),use_image_settings_(!initial.batch_options) {
        options_.validate();
        auto axis=[](const std::optional<ScientificGridAxis>& a,auto& text) {if(a) {const auto value=std::to_string(a->origin())+","+std::to_string(a->extent())+","+std::to_string(a->samples())+","+std::to_string(a->context());std::copy(value.begin(),value.end(),text.begin());}};
        axis(options_.raw_pixel_x,raw_x_);axis(options_.raw_pixel_y,raw_y_);
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
        std::optional<double> cell_min_area,std::optional<double> cell_max_area,const std::optional<GridCellSampling>& cell_grid,const GridCellParameters& grid_cells,AnalysisKind roi_kind,RoiSamplingPolicy roi_sampling_policy,bool nucleus,std::shared_ptr<const PixelClassifier> pixel_model,Transform&& transform) {
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
            if(ui::Combo("Workflow",&kind,"ROI means\0Tissue\0Cells\0Pixel streams\0Raw channel statistics\0Raw prediction files\0Raw streamed contours\0")) options_.kind=static_cast<ProjectBatchKind>(kind);
            const bool raw_classifier=options_.kind==ProjectBatchKind::RawPixels || options_.kind==ProjectBatchKind::RawPixelContours;
            if(!raw_classifier) {options_.raw_pixel_model.reset();options_.raw_pixel_x.reset();options_.raw_pixel_y.reset();}
            ui::Checkbox("TMA cores only",&options_.tma_only);
            if(options_.tma_only) {ui::Checkbox("Include missing cores",&options_.include_missing_tma);options_.nucleus=false;}else options_.include_missing_tma=false;
            ui::Checkbox("Selected objects only",&selected_only_);
            ImGui::TextWrapped(selected_only_?"Selected objects in the active image.":"Matching area objects in every project image.");
            if(!selected_only_ && !options_.tma_only) {
                int object_kind=static_cast<int>(options_.object_kind);
                if(ui::Combo("Object kind",&object_kind,"Annotations\0Detections\0Cells\0")) options_.object_kind=static_cast<ObjectKind>(object_kind);
            }
            if(!raw_classifier && options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw) ui::Checkbox("Use image analysis settings",&use_image_settings_);
            if(use_image_settings_ && !raw_classifier && options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw) ImGui::TextWrapped("Uses the Region analysis, Tissue and Cells settings in the image inspector.");
            ImGui::BeginDisabled(use_image_settings_ && !raw_classifier && options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw);
            if(options_.kind!=ProjectBatchKind::Roi) options_.roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
            if(options_.kind!=ProjectBatchKind::Tissue) options_.tissue.sampling_policy=RoiSamplingPolicy::GeometryCoverage;
            if(options_.kind!=ProjectBatchKind::Cells) reset_cell_detector(options_.cells);
            if(options_.kind!=ProjectBatchKind::Raw) {options_.raw_grid.reset();options_.raw_grid_work_bytes=256u*1024*1024;}
            if(options_.kind!=ProjectBatchKind::Cells) options_.publish_cells=false;
            if(options_.kind!=ProjectBatchKind::Cells) {options_.cell_min_nucleus_area_um2.reset();options_.cell_max_nucleus_area_um2.reset();}
            if(raw_classifier || options_.kind==ProjectBatchKind::Cells || options_.kind==ProjectBatchKind::Pixels) options_.nucleus=false;
            if(raw_classifier) {
                ui::InputTextWithHint("Raw model","Model .qrm path",raw_model_path_.data(),raw_model_path_.size());
                if(ui::Button("Load batch raw model")) {try {options_.raw_pixel_model=load_raw_pixel_classifier(raw_model_path_.data());error_.clear();}catch(const std::exception& e) {error_=e.what();}}
                if(options_.raw_pixel_model) ImGui::TextWrapped("Pinned raw model: %s",options_.raw_pixel_model->sha256().c_str());
                ui::InputTextWithHint("Raw X axis","origin,extent,samples,context",raw_x_.data(),raw_x_.size());
                ui::InputTextWithHint("Raw Y axis","origin,extent,samples,context",raw_y_.data(),raw_y_.size());
                ui::InputScalar("Raw tile width",ImGuiDataType_S64,&options_.raw_pixels.tile_width);
                ui::InputScalar("Raw tile height",ImGuiDataType_S64,&options_.raw_pixels.tile_height);
                ui::InputDouble("Raw minimum vote",&options_.raw_pixels.min_vote_fraction,0,0,"%.5g");
                if(options_.kind==ProjectBatchKind::RawPixelContours) {
                    ui::Checkbox("Clip contours to source area",&options_.raw_contours.contours.clip_to_roi);
                    ui::InputScalar("Contour components limit",ImGuiDataType_U64,&options_.raw_contours.contours.max_components);
                    ImGui::TextWrapped("Contours are saved in independent per-image projects. The open project is unchanged.");
                }
            } else if(options_.kind==ProjectBatchKind::Pixels) {
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
                bool grid=bool(options_.raw_grid);
                if(ui::Checkbox("Explicit raw grid",&grid)) {
                    options_.raw_grid=grid?std::optional<RawGridSampling>(RawGridSampling{}):std::nullopt;
                    options_.raw_grid_work_bytes=256u*1024*1024;
                    if(grid) {options_.raw.region.level=0;options_.roi_tile_size=256;}
                }
                if(options_.raw_grid) raw_grid_controls(*options_.raw_grid,options_.raw_grid_work_bytes);
                else ui::InputInt("Raw pyramid level",&options_.raw.region.level);
                axis("First channel (zero-based)",options_.raw.first_channel);axis("Channel count",options_.raw.channels,1);
                if(!options_.raw_follow_display) {axis("Z (zero-based)",options_.raw.z);axis("Time (zero-based)",options_.raw.t);}
                if(!options_.tma_only) ui::Checkbox("Use nucleus",&options_.nucleus);
            } else if(options_.kind==ProjectBatchKind::Roi) {
                int sampling=static_cast<int>(options_.roi_sampling_policy);
                if(ui::Combo("ROI polygon samples",&sampling,"Geometry coverage\0Polygon winding\0")) options_.roi_sampling_policy=static_cast<RoiSamplingPolicy>(sampling);
                int feature=static_cast<int>(options_.roi_kind);
                if(ui::Combo("Features",&feature,"RGB optical density\0Supplied stains\0")) options_.roi_kind=static_cast<AnalysisKind>(feature);
                if(!options_.tma_only) ui::Checkbox("Use nucleus",&options_.nucleus);
            } else {
                if(options_.kind==ProjectBatchKind::Cells) {
                    bool grid=bool(options_.cell_grid);
                    if(ui::Checkbox("Explicit cell grid",&grid)) {
                        options_.cell_grid=grid?std::optional<GridCellSampling>(GridCellSampling{}):std::nullopt;
                        if(grid) {options_.cell_min_nucleus_area_um2.reset();options_.cell_max_nucleus_area_um2.reset();}
                    }
                }
                if(options_.kind==ProjectBatchKind::Cells && options_.cell_grid) grid_cell_controls(*options_.cell_grid,options_.grid_cells);
                else {
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
                if(run_options.kind==ProjectBatchKind::RawPixels || run_options.kind==ProjectBatchKind::RawPixelContours) {
                    if(!*raw_x_.data() && !*raw_y_.data()) {run_options.raw_pixel_x.reset();run_options.raw_pixel_y.reset();}
                    else {if(!*raw_x_.data() || !*raw_y_.data()) throw std::invalid_argument("enter both raw axes");run_options.raw_pixel_x=gui_raw_axis(raw_x_.data());run_options.raw_pixel_y=gui_raw_axis(raw_y_.data());}
                    run_options.nucleus=false;
                }
                else if(run_options.kind==ProjectBatchKind::Pixels) {if(!run_options.pixel_model && pixel_model) run_options.pixel_model=*pixel_model;run_options.nucleus=false;}
                else if(use_image_settings_ && run_options.kind!=ProjectBatchKind::Raw) {
                    run_options.tissue=tissue;run_options.cells=cells;run_options.roi_kind=roi_kind;run_options.roi_sampling_policy=roi_sampling_policy;
                    run_options.nucleus=(run_options.kind==ProjectBatchKind::Cells || run_options.tma_only)?false:nucleus;
                    if(run_options.kind==ProjectBatchKind::Cells) {
                        run_options.cell_grid=cell_grid;run_options.grid_cells=grid_cells;
                        run_options.cell_min_nucleus_area_um2=cell_grid?std::nullopt:cell_min_area;run_options.cell_max_nucleus_area_um2=cell_grid?std::nullopt:cell_max_area;
                    }
                }
                if(run_options.kind!=ProjectBatchKind::Raw) {run_options.raw_grid.reset();run_options.raw_grid_work_bytes=256u*1024*1024;}
                if(run_options.kind!=ProjectBatchKind::Roi) run_options.roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
                if(run_options.kind!=ProjectBatchKind::Tissue) run_options.tissue.sampling_policy=RoiSamplingPolicy::GeometryCoverage;
                if(run_options.kind!=ProjectBatchKind::Cells) {reset_cell_detector(run_options.cells);run_options.cell_grid.reset();}
                const bool stains=run_options.kind!=ProjectBatchKind::RawPixels && run_options.kind!=ProjectBatchKind::RawPixelContours && run_options.kind!=ProjectBatchKind::Raw && run_options.kind!=ProjectBatchKind::Pixels && (run_options.kind==ProjectBatchKind::Roi?run_options.roi_kind==AnalysisKind::StainMeans:
                    (run_options.kind==ProjectBatchKind::Tissue?run_options.tissue.feature:run_options.cell_grid?run_options.grid_cells.feature:run_options.cells.feature)!=TissueFeature::OpticalDensitySum);
                ProjectBatchRequest request(*project,transform(stains),chosen,run_options);
                job_.request(request,output_.data());error_.clear();poll();
            } catch(const std::exception& e) {error_=e.what();}
        }
    }
};
}
