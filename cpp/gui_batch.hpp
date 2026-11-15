#pragma once
#include "gui_tool_catalog.hpp"
#include "gui_shell_open.hpp"
#include "gui_result_state.hpp"
#include "gui_record_list.hpp"
#include "gui_path_field.hpp"
#include <utility>
#include "qupathxx/project_batch.hpp"
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
#include "gui_raw_grid_controls.hpp"
#include "gui_raw_axis.hpp"
#include "gui_sampling.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <tuple>

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
        changed=ui::InputScalar("Context (sampled px)###Context (sample px)",ImGuiDataType_U32,&p.context_padding) || changed;
    }
    return changed;
}
inline bool tissue_sampling_controls(std::shared_ptr<const TissueGridSampling>& grid) {
    bool enabled=bool(grid),changed=ui::Checkbox("Explicit tissue grid",&enabled);
    if(changed) grid=enabled?std::make_shared<const TissueGridSampling>():nullptr;
    if(!enabled) return changed;
    auto value=*grid;
    changed=ui::InputScalar("Tissue samples X",ImGuiDataType_U32,&value.samples_x) || changed;
    changed=ui::InputScalar("Tissue samples Y",ImGuiDataType_U32,&value.samples_y) || changed;
    changed=ui::InputScalar("Tissue context X",ImGuiDataType_U32,&value.context_x) || changed;
    changed=ui::InputScalar("Tissue context Y",ImGuiDataType_U32,&value.context_y) || changed;
    ui::DataTextWrapped("Counts cover ROI bounds. Context adds sampled pixels around each edge; output stays inside the ROI.");
    if(changed) grid=std::make_shared<const TissueGridSampling>(value);
    return changed;
}
inline bool cell_area_controls(std::optional<double>& minimum,std::optional<double>& maximum,bool tissue=false) {
    bool changed=false;
    if(ImGui::CollapsingHeader(tissue?"Physical component area":"Physical nucleus area",(minimum || maximum)?ImGuiTreeNodeFlags_DefaultOpen:0)) {
        auto bound=[&](const char* toggle,const char* label,std::optional<double>& value) {
            bool enabled=bool(value);
            const char* original=toggle==std::string("Minimum area (%s)")?"Minimum area (um^2)":"Maximum area (um^2)";
            if(ui::Checkbox(ui::unit_caption(toggle,ui::unit::um2,original).c_str(),&enabled)) {value=enabled?std::optional<double>(0):std::nullopt;changed=true;}
            if(value) {double number=*value;if(ui::InputDouble(label,&number,0,0,"%.8g")) {value=number;changed=true;}}
        };
        bound("Minimum area (%s)","Area minimum >=",minimum);
        bound("Maximum area (%s)","Area maximum <=",maximum);
        if(maximum && *maximum==0)
            ui::DataTextWrapped("Maximum zero keeps no %s. Disable the maximum to remove the upper bound.",tissue?"components":"nuclei");
        ui::DataTextWrapped("Uses image calibration and the selected sampling level. Both sampled and physical size limits apply.");
    }
    return changed;
}
inline bool grid_cell_parameter_controls(GridCellParameters& p) {
    bool changed=false;
    int units=static_cast<int>(p.units),feature=static_cast<int>(p.feature),policy=static_cast<int>(p.sampling_policy);
    if(ui::Combo("Distance units",&units,"Level-zero pixels\0Micrometres\0")) {p.units=static_cast<GridCellUnits>(units);changed=true;}
    if(ui::Combo("Grid feature",&feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0")) {p.feature=static_cast<TissueFeature>(feature);changed=true;}
    if(ui::Combo("Grid ROI samples",&policy,"Geometry coverage\0Polygon winding\0")) {p.sampling_policy=static_cast<RoiSamplingPolicy>(policy);changed=true;}
    auto unit_label=[&](const char* label,bool area=false) {char caption[1024];std::snprintf(caption,sizeof(caption),ui::i18n::text("%s (%s)"),ui::i18n::text(label),p.units==GridCellUnits::Micrometres?(area?ui::unit::um2:ui::unit::um):(area?"level-0 px^2":"level-0 px"));return std::string(caption)+"###"+label;};
    changed=ui::InputDouble("Grid threshold",&p.threshold,0,0,"%.5g") || changed;
    changed=ui::InputDouble(unit_label("Grid sigma",false).c_str(),&p.sigma,0,0,"%.5g") || changed;
    changed=ui::InputDouble("Grid peak prominence",&p.log_prominence,0,0,"%.5g") || changed;
    changed=ui::InputDouble(unit_label("Grid peak radius >=",false).c_str(),&p.min_peak_radius,0,0,"%.5g") || changed;
    changed=ui::InputDouble(unit_label("Grid seed spacing",false).c_str(),&p.min_seed_distance,0,0,"%.5g") || changed;
    changed=ui::InputDouble(unit_label("Grid body expansion",false).c_str(),&p.cell_expansion,0,0,"%.5g") || changed;
    changed=ui::InputDouble(unit_label("Grid nucleus area >=",true).c_str(),&p.min_nucleus_area,0,0,"%.8g") || changed;
    bool maximum=bool(p.max_nucleus_area);
    if(ui::Checkbox("Limit grid nucleus area",&maximum)) {p.max_nucleus_area=maximum?std::optional<double>(p.min_nucleus_area):std::nullopt;changed=true;}
    if(p.max_nucleus_area) changed=ui::InputDouble(unit_label("Grid nucleus area <=",true).c_str(),&*p.max_nucleus_area,0,0,"%.8g") || changed;
    int memory=static_cast<int>(p.work_bytes/(1024*1024));
    if(ui::InputInt("Grid memory (MiB)",&memory)) {p.work_bytes=std::size_t(std::clamp(memory,1,512))*1024*1024;changed=true;}
    ui::DataTextWrapped("Distances use the selected units; areas use those units squared. Micrometres require both calibration axes.");
    return changed;
}
inline bool grid_cell_controls(GridCellSampling& grid,GridCellParameters& p) {
    bool changed=false;
    auto count=[&](const char* label,std::uint32_t& value,int minimum,int maximum) {
        int n=static_cast<int>(value);if(ui::InputInt(label,&n)) {value=static_cast<std::uint32_t>(std::clamp(n,minimum,maximum));changed=true;}
    };
    count("Grid samples X",grid.samples_x,1,1000000);count("Grid samples Y",grid.samples_y,1,1000000);
    count("Grid context X",grid.context_x,0,4096);count("Grid context Y",grid.context_y,0,4096);
    ui::DataTextWrapped("Counts cover the integer enclosure of each ROI.");
    return grid_cell_parameter_controls(p) || changed;
}
inline bool nominal_cell_controls(NominalCellRoiSampling& sampling,GridCellParameters& p) {
    bool changed=ui::InputDouble("Nominal downsample (level-0 px per sample)###Nominal downsample",&sampling.downsample,0,0,"%.8g");
    int coordinates=static_cast<int>(sampling.sampling);
    if(ui::Combo("RGB sampling",&coordinates,"Integer origin\0Affine inverse\0")) {sampling.sampling=static_cast<RgbNominalSampling>(coordinates);changed=true;}
    int processing=static_cast<int>(p.processing);
    if(ui::Combo("Cell processing",&processing,"Quantitative\0ImageJ 1.54p\0QuPath 0.7.0 v1\0QuPath 0.7.0 v2\0")) {p.processing=static_cast<CellProcessingPolicy>(processing);changed=true;}
    auto context=[&](const char* label,std::uint32_t& value) {
        int n=static_cast<int>(value);if(ui::InputInt(label,&n)) {value=static_cast<std::uint32_t>(std::clamp(n,0,4096));changed=true;}
    };
    context("Nominal context X (output samples)###Nominal context X",sampling.context_x);context("Nominal context Y (output samples)###Nominal context Y",sampling.context_y);
    ui::TextWrapped("Samples the original image at the requested nominal pixel step. Context is measured in output samples; the original ROI is kept.");
    return grid_cell_parameter_controls(p) || changed;
}
inline bool cell_sampling_controls(std::optional<GridCellSampling>& grid,std::optional<NominalCellRoiSampling>& nominal,GridCellParameters& parameters) {
    bool enabled=bool(grid),changed=ui::Checkbox("Explicit cell grid",&enabled);
    if(changed) {grid=enabled?std::optional<GridCellSampling>(GridCellSampling{}):std::nullopt;if(enabled) nominal.reset();}
    enabled=bool(nominal);
    if(ui::Checkbox("Nominal RGB cell grid",&enabled)) {
        nominal=enabled?std::optional<NominalCellRoiSampling>(NominalCellRoiSampling{}):std::nullopt;
        if(enabled) grid.reset();changed=true;
    }
    if(changed && !nominal) parameters.processing=CellProcessingPolicy::Quantitative;
    return changed;
}
// References borrow the live Workflow only during drawing/request capture.
// ProjectBatchRequest owns the independent options used by a running job.
struct WorkflowSnapshot {
    const TissueParameters& tissue;
    const std::shared_ptr<const TissueGridSampling>& tissue_grid;
    std::optional<double> tissue_min_area,tissue_max_area;
    const CellParameters& cells;
    std::optional<double> cell_min_area,cell_max_area;
    const std::optional<GridCellSampling>& cell_grid;
    const std::optional<NominalCellRoiSampling>& cell_nominal;
    const GridCellParameters& grid_cells;
    AnalysisKind roi_kind;RoiSamplingPolicy roi_sampling_policy;bool nucleus;
    std::shared_ptr<const PixelClassifier> pixel_model;
    bool intensity_statistics=false;
    const std::vector<double>& vectors;
    const std::vector<double>& white;
};
// The owner captures once. Workers process an independent project snapshot;
// no completion callback publishes into the currently displayed workspace.
class GuiBatch {
    ui::PathField output_{{"##output",ui::PathRole::NewFolder,{},"","Output directory"}};
    ui::PathField raw_model_path_{{"Raw model",ui::PathRole::OpenFile,{".qrm"},"","Model .qrm path"}};
    friend struct GuiBatchTestAccess;
    ProjectBatchJob job_;
    std::uint64_t notice_generation_=0;
    ProjectBatchOptions options_;
    std::array<char,128> raw_x_{},raw_y_{};
    bool selected_only_=false,use_image_settings_=true,open_=false;
    std::string error_;
    ProjectBatchStats stats_;
    SamplingSummaryCache sampling_;
    using SamplingTargetKey=std::tuple<const Document*,std::uint64_t,const ObjectSelection*,std::uint64_t,bool,ObjectKind>;
    std::optional<SamplingTargetKey> sampling_target_key_;
    std::uint64_t sampling_target_=0;
    std::size_t sampling_target_walks_=0,sampling_target_visits_=0;
    mutable std::size_t effective_calls_=0;
public:
    std::vector<ui::PathField*> paths(){return {&output_,&raw_model_path_};}
    explicit GuiBatch(const GuiOptions& initial):options_(initial.batch_options?*initial.batch_options:ProjectBatchOptions{}),selected_only_(initial.batch_selected_objects),use_image_settings_(!initial.batch_options) {
        options_.validate();
        auto axis=[](const std::optional<ScientificGridAxis>& a,auto& text) {if(a) {const auto value=std::to_string(a->origin())+","+std::to_string(a->extent())+","+std::to_string(a->samples())+","+std::to_string(a->context());std::copy(value.begin(),value.end(),text.begin());}};
        axis(options_.raw_pixel_x,raw_x_);axis(options_.raw_pixel_y,raw_y_);
        const auto& path=initial.batch_output_directory;
        if(path.size()>32768 || path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid batch output directory");
        output_.set_option(path);
    }
    std::uint64_t notice_generation() const {return notice_generation_;}
    void poll() {stats_=job_.poll();}
    void open() {open_=true;}
    void open_tma() {open_=true;options_.tma_only=true;options_.include_missing_tma=false;options_.nucleus=false;selected_only_=true;}
    bool busy() const {return job_.busy();}
    bool finished() const {return !job_.busy() && stats_.state!="idle";}
    void finish() {if(job_.busy()) job_.cancel();stats_=job_.wait();}
    const ProjectBatchStats& stats() const {return stats_;}
    const std::string& error() const {return error_.empty()?stats_.error:error_;}
    std::shared_ptr<const ProjectBatchReport> result() const {return job_.result();}
    std::pair<bool,bool> take_requests(bool blocked) {
        auto& frame=ui::frame_commands();
        const bool run=!blocked && ui::chord_pressed(ui::CommandId::Action_Run_batch_Ctrl_Alt_B,frame);
        const bool cancel=ui::batch_cancel_requested(frame);
        return {run,cancel};
    }
    ui::Availability availability(const Project* project,bool blocked,std::string* requested_error=nullptr) const {
        if(blocked)return ui::WorkflowBlocked();
        if(job_.busy())return {false,"Wait for the current work to finish."};
        // Replay clients retain validation feedback even when no work is submitted.
        const auto unavailable=[&](const char* reason,const char* error){if(requested_error)*requested_error=error;return ui::Availability{false,reason};};
        if(!project)return unavailable("Open a project to run a batch.","batch requires a nonempty project");
        if(!project->active_image())return unavailable("Open an image to run a batch.","batch requires a nonempty project");
        if(!output_.text()[0])return unavailable("Enter an output folder.","enter a batch output directory");
        return {};
    }
    ProjectBatchOptions effective(const WorkflowSnapshot& workflow) const {
        ++effective_calls_;
        auto run_options=options_;
        if(run_options.kind==ProjectBatchKind::RawPixels || run_options.kind==ProjectBatchKind::RawPixelContours) {
            if(!*raw_x_.data() && !*raw_y_.data()) {run_options.raw_pixel_x.reset();run_options.raw_pixel_y.reset();}
            else {if(!*raw_x_.data() || !*raw_y_.data()) throw std::invalid_argument("enter both raw axes");run_options.raw_pixel_x=gui_raw_axis(raw_x_.data());run_options.raw_pixel_y=gui_raw_axis(raw_y_.data());}
            run_options.nucleus=false;
        }
        else if(run_options.kind==ProjectBatchKind::Pixels) {if(!run_options.pixel_model && workflow.pixel_model) run_options.pixel_model=*workflow.pixel_model;run_options.nucleus=false;}
        else if(use_image_settings_ && run_options.kind!=ProjectBatchKind::Raw) {
            run_options.tissue=workflow.tissue;
            if(run_options.kind==ProjectBatchKind::Tissue) {run_options.tissue_grid=workflow.tissue_grid;run_options.tissue_min_component_area_um2=workflow.tissue_min_area;run_options.tissue_max_component_area_um2=workflow.tissue_max_area;}run_options.cells=workflow.cells;run_options.roi_kind=workflow.roi_kind;run_options.roi_sampling_policy=workflow.roi_sampling_policy;
            if(run_options.kind==ProjectBatchKind::Roi)run_options.roi_intensity_statistics=workflow.intensity_statistics;
            run_options.nucleus=(run_options.kind==ProjectBatchKind::Cells || run_options.tma_only)?false:workflow.nucleus;
            if(run_options.kind==ProjectBatchKind::Cells) {
                run_options.cell_grid=workflow.cell_grid;run_options.cell_nominal=workflow.cell_nominal;run_options.grid_cells=workflow.grid_cells;
                run_options.cell_min_nucleus_area_um2=(workflow.cell_grid || workflow.cell_nominal)?std::nullopt:workflow.cell_min_area;run_options.cell_max_nucleus_area_um2=(workflow.cell_grid || workflow.cell_nominal)?std::nullopt:workflow.cell_max_area;
            }
        }
        if(run_options.kind!=ProjectBatchKind::Raw) {run_options.raw_grid.reset();run_options.raw_grid_work_bytes=256u*1024*1024;}
        if(run_options.kind!=ProjectBatchKind::Roi) run_options.roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
        if(run_options.kind!=ProjectBatchKind::Tissue) {run_options.tissue.sampling_policy=RoiSamplingPolicy::GeometryCoverage;run_options.tissue.smoothing_sigma=0;run_options.tissue.median_cleanup=false;run_options.tissue.canonical_pyramid=false;run_options.tissue_grid.reset();run_options.tissue_min_component_area_um2.reset();run_options.tissue_max_component_area_um2.reset();}
        if(run_options.kind!=ProjectBatchKind::Cells) {reset_cell_detector(run_options.cells);run_options.cell_grid.reset();run_options.cell_nominal.reset();}
        if(run_options.cell_nominal) run_options.cells=CellParameters{};
        return run_options;
    }
    static bool separates_stains(const ProjectBatchOptions& o) {
        return o.kind!=ProjectBatchKind::RawPixels && o.kind!=ProjectBatchKind::RawPixelContours && o.kind!=ProjectBatchKind::Raw && o.kind!=ProjectBatchKind::Pixels && (o.kind==ProjectBatchKind::Roi?o.roi_kind==AnalysisKind::StainMeans:
                    (o.kind==ProjectBatchKind::Tissue?o.tissue.feature:(o.cell_grid || o.cell_nominal)?o.grid_cells.feature:o.cells.feature)!=TissueFeature::OpticalDensitySum);
    }
    static void vector_summary(const char* label,const double* value) {
        ui::TextWrapped("%s: %.5g, %.5g, %.5g",ui::i18n::text(label),value[0],value[1],value[2]);
    }
    static void draw_summary(const ProjectBatchOptions& o) {
        const auto value=[](const char* label,double number){ui::TextWrapped("%s: %.5g",ui::i18n::text(label),number);};
        const auto flag=[](const char* label,bool on){ui::TextWrapped("%s: %s",ui::i18n::text(label),ui::i18n::text(on?"Yes":"No"));};
        const auto feature=[](TissueFeature f){constexpr const char* names[]={"OD sum","Stain 1","Stain 2","Stain 3"};ui::TextWrapped("%s: %s",ui::i18n::text("Feature"),ui::i18n::text(names[static_cast<int>(f)]));};
        const auto area=[&](const char* label,const std::optional<double>& bound){const auto caption=std::strstr(label,"%s")?ui::unit_text(label,ui::unit::um2):std::string(ui::i18n::text(label));if(bound)ui::TextWrapped("%s: %.5g",caption.c_str(),*bound);else ui::TextWrapped("%s: %s",caption.c_str(),ui::i18n::text("Off"));};
        const auto level=[&](int number){if(number==-1)ui::TextWrapped("Level: %s",ui::i18n::text("auto"));else value("Level",number);};
        const auto maximum=[&](const char* label,std::uint64_t number){if(number==0)ui::TextWrapped("%s: %s",ui::i18n::text(label),ui::i18n::text("all"));else value(label,double(number));};
        if(o.kind==ProjectBatchKind::Roi) {
            ui::TextWrapped("%s: %s",ui::i18n::text("Features"),ui::i18n::text(o.roi_kind==AnalysisKind::StainMeans?"Supplied stains":"RGB optical density"));
            ui::TextWrapped("%s: %s",ui::i18n::text("ROI polygon samples"),ui::i18n::text(o.roi_sampling_policy==RoiSamplingPolicy::GeometryCoverage?"Geometry coverage":"Polygon winding"));
            flag("Use nucleus",o.nucleus);flag("Intensity statistics",o.roi_intensity_statistics);
        } else if(o.kind==ProjectBatchKind::Tissue) {
            feature(o.tissue.feature);ui::TextWrapped("Threshold: >= %.5g",o.tissue.threshold);value("Sigma (sampled px)",o.tissue.smoothing_sigma);
            if(o.tissue_grid){value("Tissue samples X",o.tissue_grid->samples_x);value("Tissue samples Y",o.tissue_grid->samples_y);}
            else level(o.tissue.level);
            value("Minimum pixels",o.tissue.min_pixels);maximum("Maximum pixels",o.tissue.max_pixels);
            area("Minimum area (%s)",o.tissue_min_component_area_um2);area("Maximum area (%s)",o.tissue_max_component_area_um2);
        } else if(o.kind==ProjectBatchKind::Cells) {
            const bool grid=o.cell_grid || o.cell_nominal;feature(grid?o.grid_cells.feature:o.cells.feature);
            value("Cell threshold",grid?o.grid_cells.threshold:o.cells.threshold);
            value(grid?"Sigma":"Blur sigma (sampled px)",grid?o.grid_cells.sigma:o.cells.sigma);
            value(grid?"Cell expansion":"Cell expansion (sampled px)",grid?o.grid_cells.cell_expansion:o.cells.cell_expansion);
            if(grid)ui::TextWrapped("%s: %s",ui::i18n::text("Distance units"),ui::i18n::text(o.grid_cells.units==GridCellUnits::Micrometres?"Micrometres":"Level-zero pixels"));
            if(o.cell_nominal)value("Nominal downsample (level-0 px per sample)",o.cell_nominal->downsample);
            else if(o.cell_grid){value("Grid samples X",o.cell_grid->samples_x);value("Grid samples Y",o.cell_grid->samples_y);}
            else level(o.cells.level);
            if(!grid){value("Minimum nucleus pixels",o.cells.min_nucleus_pixels);maximum("Maximum nucleus pixels",o.cells.max_nucleus_pixels);}
            if(grid){value("Minimum nucleus area",o.grid_cells.min_nucleus_area);area("Maximum nucleus area",o.grid_cells.max_nucleus_area);}
            else {area("Minimum area (%s)",o.cell_min_nucleus_area_um2);area("Maximum area (%s)",o.cell_max_nucleus_area_um2);}
        }
    }
    template<class Transform>void draw_sampling(const Project* project,std::uint64_t image_id,const ObjectSelection* selection,const ProjectBatchOptions& o,const WorkflowSnapshot& workflow,Transform&& transform) {
        if(!project || !image_id || o.tma_only)return;
        const auto doc=project->document(image_id);if(!doc)return;
        const SamplingTargetKey target_key{doc.get(),doc->revision(),selection,selection?selection->revision():0,selected_only_,o.object_kind};
        // Cache absence too: an idle window must not repeatedly search every object.
        if(!sampling_target_key_ || *sampling_target_key_!=target_key) {
            sampling_target_key_=target_key;sampling_target_=0;++sampling_target_walks_;
            for(const auto& [id,a]:doc->objects()) {
                ++sampling_target_visits_;
                if(a.geometry.is_area() && (selected_only_?(selection && selection->contains(id)):a.kind==o.object_kind)){sampling_target_=id;break;}
            }
        }
        const auto target=sampling_target_;
        if(!target)return;
        SamplingInputKey key;key.append(reinterpret_cast<std::uintptr_t>(doc.get()),doc->revision(),target,o.kind,o.nucleus);
        // A corrected transform must clear a cached sampling-validation error.
        for(double value:workflow.vectors)key.add(value);for(double value:workflow.white)key.add(value);
        key.parameters(o.tissue);key.parameters(o.cells);key.parameters(o.grid_cells);
        key.append(o.tissue_min_component_area_um2,o.tissue_max_component_area_um2,o.cell_min_nucleus_area_um2,o.cell_max_nucleus_area_um2,bool(o.tissue_grid),bool(o.cell_grid),bool(o.cell_nominal));
        if(o.tissue_grid)key.append(o.tissue_grid->samples_x,o.tissue_grid->samples_y,o.tissue_grid->context_x,o.tissue_grid->context_y);
        if(o.cell_grid)key.append(o.cell_grid->samples_x,o.cell_grid->samples_y,o.cell_grid->context_x,o.cell_grid->context_y);
        if(o.cell_nominal)key.append(o.cell_nominal->downsample,o.cell_nominal->context_x,o.cell_nominal->context_y,o.cell_nominal->sampling);
        sampling_.update(std::move(key),[&]{
            const auto t=transform(separates_stains(o));
            if(o.kind==ProjectBatchKind::Tissue)return sampling_summary(o.tissue_grid?TissueRequest::for_grid_roi(*doc,target,t,*o.tissue_grid,o.tissue,o.nucleus,o.tissue_min_component_area_um2,o.tissue_max_component_area_um2):TissueRequest::with_component_area_limits(*doc,target,t,o.tissue_min_component_area_um2,o.tissue_max_component_area_um2,o.tissue,o.nucleus));
            const auto r=o.cell_nominal?CellRequest::for_nominal_roi(*doc,target,t,o.cell_nominal->downsample,o.cell_nominal->context_x,o.cell_nominal->context_y,o.grid_cells,o.cell_nominal->sampling):o.cell_grid?CellRequest::for_grid_roi(*doc,target,t,*o.cell_grid,o.grid_cells):CellRequest::with_nucleus_area_limits(*doc,target,t,o.cell_min_nucleus_area_um2,o.cell_max_nucleus_area_um2,o.cells);
            return sampling_summary(r,doc->pixel_width_um(),doc->pixel_height_um());
        });
        ui::TextWrapped("Sampling example: active image, area #%llu",static_cast<unsigned long long>(target));
        draw_sampling_summary(sampling_);
    }
    template<class Transform>
    void draw(const Project* project,std::uint64_t image_id,const ObjectSelection* selection,bool blocked,const WorkflowSnapshot& workflow,Transform&& transform,const ui::PathContext& context={}) {
    ui::i18n::SurfaceTrace translation_surface("tool/Batch");
        const auto& io=ImGui::GetIO();
        const bool shortcut=!blocked && ui::chord_pressed(ui::CommandId::Tool_Batch,ui::frame_commands());
        auto [start,cancel]=take_requests(blocked);
        if(start)open_=true;
        if(shortcut) open_=true;
        if(open_) {
            ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::Batch),{460,720});
            if(ImGui::Begin(ui::tool_window_label(ui::ToolId::Batch),&open_)) {
            ImGui::PushID("project-batch");
            if(job_.busy()) cancel=ui::Button(ui::presentation_label(ui::CommandId::Key_CancelBatch,"Cancel batch")) || cancel;
            ui::DataTextWrapped("Batch: %s / %s",ui::caption(ui::parse_run_state(stats_.state),stats_.state).c_str(),stats_.phase.c_str());
            if(stats_.total_images) {
                ImGui::ProgressBar(float(double(stats_.completed_images)/stats_.total_images));
                ui::DataTextWrapped("Images: %zu / %zu   Objects: %zu / %zu",stats_.completed_images,stats_.total_images,stats_.completed_objects,stats_.total_objects);
            }
            if(const auto report=job_.result()) ui::TextWrapped("Saved: %zu   Reused: %zu   Failed: %zu",report->saved,report->reused,report->failed);
            if(!error().empty()) ui::TextWrapped("%s",error().c_str());
            const float footer=ui::design::metrics.action_height*ui::design::scale()+ImGui::GetTextLineHeightWithSpacing()*3;
            ImGui::BeginChild("Batch recipe",{0,std::max(1.f,ImGui::GetContentRegionAvail().y-footer)},ImGuiChildFlags_None);
            ImGui::BeginDisabled(job_.busy() || blocked);
            output_.draw(context,false,{},{},"Output directory");
            int kind=static_cast<int>(options_.kind);
            if(ui::Combo("Workflow",&kind,"ROI means\0Tissue\0Cells\0Pixel streams\0Raw channel statistics\0Raw prediction files\0Raw streamed contours\0")) options_.kind=static_cast<ProjectBatchKind>(kind);
            const bool raw_classifier=options_.kind==ProjectBatchKind::RawPixels || options_.kind==ProjectBatchKind::RawPixelContours;
            if(!raw_classifier) {options_.raw_pixel_model.reset();options_.raw_pixel_x.reset();options_.raw_pixel_y.reset();}
            ui::Checkbox("TMA cores only",&options_.tma_only);
            if(options_.tma_only) {ui::Checkbox("Include missing cores",&options_.include_missing_tma);options_.nucleus=false;}else options_.include_missing_tma=false;
            ui::Checkbox("Selected objects only",&selected_only_);
            ui::DataTextWrapped(selected_only_?"Selected objects in the active image.":"Matching area objects in every project image.");
            if(!selected_only_ && !options_.tma_only) {
                int object_kind=static_cast<int>(options_.object_kind);
                if(ui::Combo("Object kind",&object_kind,"Annotations\0Detections\0Cells\0")) options_.object_kind=static_cast<ObjectKind>(object_kind);
            }
            if(!raw_classifier && options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw) ui::Checkbox("Use image analysis settings",&use_image_settings_);
            const bool summary=use_image_settings_ && !raw_classifier && options_.kind!=ProjectBatchKind::Pixels && options_.kind!=ProjectBatchKind::Raw;
            if(options_.kind!=ProjectBatchKind::Roi) {options_.roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;options_.roi_intensity_statistics=false;}
            if(options_.kind!=ProjectBatchKind::Tissue) {options_.tissue_grid.reset();options_.tissue.sampling_policy=RoiSamplingPolicy::GeometryCoverage;options_.tissue.smoothing_sigma=0;options_.tissue.median_cleanup=false;}
            if(options_.kind!=ProjectBatchKind::Cells) reset_cell_detector(options_.cells);
            if(options_.kind!=ProjectBatchKind::Raw) {options_.raw_grid.reset();options_.raw_grid_work_bytes=256u*1024*1024;}
            if(options_.kind!=ProjectBatchKind::Tissue) {options_.tissue_min_component_area_um2.reset();options_.tissue_max_component_area_um2.reset();}
            if(options_.kind!=ProjectBatchKind::Cells) options_.publish_cells=false;
            if(options_.kind!=ProjectBatchKind::Cells) {options_.cell_min_nucleus_area_um2.reset();options_.cell_max_nucleus_area_um2.reset();}
            if(raw_classifier || options_.kind==ProjectBatchKind::Cells || options_.kind==ProjectBatchKind::Pixels) options_.nucleus=false;
            std::optional<ProjectBatchOptions> shown_options;
            if(summary) {shown_options=effective(workflow);draw_summary(*shown_options);if(shown_options->kind!=ProjectBatchKind::Roi)draw_sampling(project,image_id,selection,*shown_options,workflow,transform);}
            else if(raw_classifier) {
                raw_model_path_.draw(context);
                if(ui::Button("Load batch raw model")) {try {options_.raw_pixel_model=load_raw_pixel_classifier(raw_model_path_.state().resolved);error_.clear();}catch(const std::exception& e) {error_=e.what();}}
                if(options_.raw_pixel_model) {ui::TextWrapped("Pinned raw model: %s",ui::short_record_id(options_.raw_pixel_model->sha256()).c_str());if(ui::CollapsingHeader("Details"))ui::TextWrapped("%s",options_.raw_pixel_model->sha256().c_str());}
                ui::InputTextWithHint("Raw X axis","origin,extent,samples,context",raw_x_.data(),raw_x_.size());
                ui::InputTextWithHint("Raw Y axis","origin,extent,samples,context",raw_y_.data(),raw_y_.size());
                ui::InputScalar("Raw tile width",ImGuiDataType_S64,&options_.raw_pixels.tile_width);
                ui::InputScalar("Raw tile height",ImGuiDataType_S64,&options_.raw_pixels.tile_height);
                ui::InputDouble("Raw minimum vote",&options_.raw_pixels.min_vote_fraction,0,0,"%.5g");
                if(ui::CollapsingHeader("Raw batch limits")) {
                    auto memory=[](const char* label,std::size_t& value,int maximum) {int mib=static_cast<int>(value/(1024*1024));if(ui::InputInt(label,&mib)) value=static_cast<std::size_t>(std::clamp(mib,1,maximum))*1024*1024;};
                    memory("Producer memory (MiB)",options_.raw_pixels.work_bytes,1024);
                    if(options_.kind==ProjectBatchKind::RawPixelContours) {
                        memory("Boundary memory (MiB)",options_.raw_contours.boundary_work_bytes,512);
                        memory("Geometry memory (MiB)",options_.raw_contours.contours.work_bytes,512);
                        memory("Total contour memory (MiB)",options_.raw_contours.work_bytes,1024);
                        memory("Add objects memory (MiB)###Publication memory (MiB)",options_.raw_contour_publication_bytes,512);
                    }
                }
                if(options_.kind==ProjectBatchKind::RawPixelContours) {
                    ui::Checkbox("Clip contours to source area",&options_.raw_contours.contours.clip_to_roi);
                    ui::InputScalar("Contour components limit",ImGuiDataType_U64,&options_.raw_contours.contours.max_components);
                    ui::DataTextWrapped("Contours are saved in independent per-image projects. The open project is unchanged.");
                }
            } else if(options_.kind==ProjectBatchKind::Pixels) {
                ImGui::BeginDisabled(!workflow.pixel_model);if(ui::Button("Use active pixel model") && workflow.pixel_model) options_.pixel_model=*workflow.pixel_model;ImGui::EndDisabled();
                if(options_.pixel_model) {
                    ui::TextWrapped("Pinned model: %s",ui::short_record_id(options_.pixel_model->sha256()).c_str());
                    for(const auto& name:options_.pixel_model->classes())ui::TextWrapped("%s",name.c_str());
                    if(ui::CollapsingHeader("Details"))ui::TextWrapped("%s",options_.pixel_model->sha256().c_str());
                    if(ui::Button("Unpin"))options_.pixel_model.reset();
                }
                else ui::DataTextWrapped(workflow.pixel_model?"Run captures the active pixel model.":"Train or load a pixel model in Pixel classifiers first.");
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
                ui::Checkbox("Intensity statistics",&options_.roi_intensity_statistics);
                int sampling=static_cast<int>(options_.roi_sampling_policy);
                if(ui::Combo("ROI polygon samples",&sampling,"Geometry coverage\0Polygon winding\0")) options_.roi_sampling_policy=static_cast<RoiSamplingPolicy>(sampling);
                int feature=static_cast<int>(options_.roi_kind);
                if(ui::Combo("Features",&feature,"RGB optical density\0Supplied stains\0")) options_.roi_kind=static_cast<AnalysisKind>(feature);
                if(!options_.tma_only) ui::Checkbox("Use nucleus",&options_.nucleus);
            } else {
                if(options_.kind==ProjectBatchKind::Cells) {
                    if(cell_sampling_controls(options_.cell_grid,options_.cell_nominal,options_.grid_cells) && (options_.cell_grid || options_.cell_nominal)) {
                        options_.cell_min_nucleus_area_um2.reset();options_.cell_max_nucleus_area_um2.reset();
                    }
                }
                if(options_.kind==ProjectBatchKind::Cells && options_.cell_nominal) nominal_cell_controls(*options_.cell_nominal,options_.grid_cells);
                else if(options_.kind==ProjectBatchKind::Cells && options_.cell_grid) grid_cell_controls(*options_.cell_grid,options_.grid_cells);
                else {
                if(options_.kind==ProjectBatchKind::Tissue) {
                    tissue_sampling_controls(options_.tissue_grid);if(options_.tissue_grid) options_.tissue.level=0;
                    ui::InputDouble("Sigma (sampled px)###Sigma (px)",&options_.tissue.smoothing_sigma,0,0,"%.5g");
                    ui::Checkbox("Median cleanup (3x3)",&options_.tissue.median_cleanup);
                    cell_area_controls(options_.tissue_min_component_area_um2,options_.tissue_max_component_area_um2,true);
                    int sampling=static_cast<int>(options_.tissue.sampling_policy);
                    if(ui::Combo("Tissue polygon samples",&sampling,"Geometry coverage\0Polygon winding\0")) options_.tissue.sampling_policy=static_cast<RoiSamplingPolicy>(sampling);
                }
                auto& feature=options_.kind==ProjectBatchKind::Tissue?options_.tissue.feature:options_.cells.feature;
                int selected_feature=static_cast<int>(feature);
                if(ui::Combo("Feature",&selected_feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0")) feature=static_cast<TissueFeature>(selected_feature);
                auto& threshold=options_.kind==ProjectBatchKind::Tissue?options_.tissue.threshold:options_.cells.threshold;
                ui::InputDouble("Threshold >=",&threshold,0,0,"%.5g");
                auto& level=options_.kind==ProjectBatchKind::Tissue?options_.tissue.level:options_.cells.level;
                if(options_.kind!=ProjectBatchKind::Tissue || !options_.tissue_grid) ui::InputInt("Level (-1=auto)",&level);
                if(options_.kind==ProjectBatchKind::Cells) cell_detector_controls(options_.cells);
                if(options_.kind==ProjectBatchKind::Cells)
                    cell_area_controls(options_.cell_min_nucleus_area_um2,options_.cell_max_nucleus_area_um2);
                }
            }
            if(options_.kind==ProjectBatchKind::Roi || options_.kind==ProjectBatchKind::Tissue || options_.kind==ProjectBatchKind::Cells) {
                if(!shown_options)shown_options=effective(workflow);
                vector_summary("White RGB",workflow.white.data());
                if(separates_stains(*shown_options)){vector_summary("Stain 1",workflow.vectors.data());vector_summary("Stain 2",workflow.vectors.data()+3);}
            }
            if(options_.kind==ProjectBatchKind::Cells) ui::Checkbox("Add cell objects (in checkpoint projects)###Publish cell objects",&options_.publish_cells);
            int failure=static_cast<int>(options_.failure);
            if(ui::Combo("On image error",&failure,"Stop\0Continue\0")) options_.failure=static_cast<ProjectBatchFailure>(failure);
            ui::Checkbox("Reuse matching checkpoints",&options_.resume);
            ui::TextWrapped(raw_classifier?"Uses the pinned raw model and exact sampling axes. Complete per-image projects are sealed before checkpoint reuse.":options_.kind==ProjectBatchKind::Raw?"Measures original TIFF samples at the selected axes. Each ROI keeps its input hash and original labels in an independent checkpoint.":options_.kind==ProjectBatchKind::Pixels?"Each target produces a verified QPS file; only complete image bundles are reused. The open project is unchanged.":"Each run keeps its starting settings. Results go to independent checkpoints; the open project is unchanged.");
            ImGui::EndDisabled();
            if(const auto report=job_.result()) {
                ui::DataTextWrapped("Saved: %zu   Reused: %zu   Failed: %zu",report->saved,report->reused,report->failed);
                if(!report->report_path.empty()) {
                    ui::DataTextWrapped("Run report: %s",report->report_path.c_str());
                    const auto folder=std::filesystem::u8path(report->report_path).parent_path().u8string();
                    ui::TextWrapped("Output folder: %s",folder.c_str());
                    if(ui::WorkflowAction("Open folder###Batch output folder"))try {ui::open_in_shell(folder);}catch(const std::exception& e){error_=e.what();}
                }
                // Keep a large project report from creating unbounded widgets.
                for(std::size_t i=0;i<std::min<std::size_t>(report->images.size(),20);++i) {
                    const auto& image=report->images[i];
                    ui::DataTextWrapped("%s: %s%s%s",image.image_name.c_str(),ui::caption(ui::parse_run_state(image.state),image.state).c_str(),image.error.empty()?"":" / ",image.error.c_str());
                }
                if(report->images.size()>20) ui::TextDisabled("More images in the run report.");
            }
            ImGui::EndChild();
            const auto available=availability(project,blocked);
            start=ui::WorkflowAction("Run batch (Ctrl+Alt+B)",ui::design::Action::Primary,available) || start;
            ui::WorkflowReason("Run batch (Ctrl+Alt+B)",available);
            ImGui::PopID();
            }
            ImGui::End();
        }
        if(cancel) job_.cancel();
        if(start && availability(project,blocked,&error_).enabled) {
            try {
                std::vector<ProjectBatchSelection> chosen;
                if(selected_only_) {
                    if(!selection || !image_id || selection->size()==0) throw std::invalid_argument("select at least one area object for this batch");
                    chosen.push_back({image_id,selection->ids()});
                }
                auto run_options=effective(workflow);
                const bool stains=separates_stains(run_options);
                ProjectBatchRequest request(*project,transform(stains),chosen,run_options);
                if(job_.request(request,output_.state().resolved))++notice_generation_;error_.clear();poll();
            } catch(const std::exception& e) {error_=e.what();}
        }
    }
};
}
