#pragma once
#include "gui_workspace.hpp"
#include "gui_stain_model.hpp"
#include "gui_core_paths.hpp"
#include "gui_output_gate.hpp"
#include "gui_pixel_classifier.hpp"
#include "gui_record_removal.hpp"
#include "gui_sampling.hpp"
#include "gui_workflow_ui.hpp"
#include "gui_result_panel.hpp"
#include "gui_tissue.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/interchange.hpp"
#include "gui_batch.hpp"
#include "gui_measurements.hpp"
#include "gui_analysis_inputs.hpp"

namespace qpx::detail::analysis_tasks {
struct AvailabilityAnswers {
    std::array<std::pair<std::string_view,ui::Availability>,16> values{};std::size_t count=0;
    template<class Query>ui::Availability get(const char* action,Query&& query) {
        for(std::size_t i=0;i<count;++i)if(values[i].first==action)return values[i].second;
        if(count==values.size())return query(action);
        const auto answer=query(action);values[count++]={action,answer};return answer;
    }
};
inline ui::Availability kept_cells_export_availability(const Document& document,std::uint64_t selected,const CellTableOptions& options,bool busy,bool blocked,bool for_cells=true) {
    if(blocked)return ui::WorkflowBlocked();
    if(!for_cells && options.published_objects)return {};
    const auto saved=selected?document.latest_cells(selected):nullptr;
    if(!saved)return {false,selected?"Keep a cell run for the selected annotation before exporting kept cells.":"Select an annotation with a kept cell run."};
    if(options.published_objects)return {false,"In Tables, choose kept runs in Cell source."};
    if(busy)return {false,"Wait for the current work to finish."};
    if(options.require_current && document.cell_status(saved->id)!="current")return {false,"Recheck the kept cell run before exporting current measurements."};
    return {};
}
struct State {
    StainEstimateParameters stain_estimate_parameters;
    SamplingSummaryCache tissue_sampling_cache,cell_sampling_cache;
    bool tissue_inputs_open=false,cell_inputs_open=false;
    explicit State(const GuiOptions& options)
        :stain_estimate_parameters(options.stain_estimate_parameters?*options.stain_estimate_parameters:StainEstimateParameters{}) {stain_estimate_parameters.validate();}
};
struct TaskContext {
    State& state;
    GuiWorkspace& workspace;
    const ui::WorkflowTask& workflow_task;
    const GuiOptions& options;
    Document& document;
    const std::uint64_t& selected;
    const ImGuiIO& io;
    const bool& editing_blocked;
    const ImageMetadata& image;
    AnalysisJob& analysis;
    AnalysisStats& analysis_status;
    TissueJob& tissue;
    CellJob& cell_job;
    StainEstimateJob& stain_estimate;
    bool& stain_estimate_applied;
    std::string& stain_estimate_error;
    StainEstimateParameters& stain_estimate_parameters;
    AnalysisInputs& inputs;
    AnalysisInputFeedback& input_feedback;
    GuiStainModel& stain_model_io;
    Document*& stain_model_document;
    std::uint64_t& stain_model_revision;
    std::uint64_t& stain_model_project;
    std::uint64_t& stain_model_image;
    int& stain_model_kind;
    std::vector<double>& stain_model_vectors;
    std::vector<double>& stain_model_white;
    std::optional<std::array<ColorVector,3>>& stain_model_basis;
    ColorVector& stain_model_basis_white;
    std::array<Rect,2>& stain_model_bounds;
    const std::uint64_t& project_replacements;
    const std::uint64_t& active_image;
    GuiCorePaths& paths;
    ui::PathContext& path_context;
    OutputGate& output_gate;
    GuiPixelClassifier& pixel_classifier;
    ui::Notifications& notifications;
    TissueParameters& tissue_parameters;
    std::shared_ptr<const TissueGridSampling>& tissue_grid;
    std::optional<double>& tissue_min_area;
    std::optional<double>& tissue_max_area;
    int& tissue_feature;
    int& tissue_connectivity;
    bool& show_tissue;
    TissuePreview& tissue_preview;
    std::size_t& tissue_annotations_added;
    CellParameters& cell_parameters;
    std::optional<GridCellSampling>& cell_grid;
    std::optional<NominalCellRoiSampling>& cell_nominal;
    GridCellParameters& grid_cells;
    std::optional<double>& cell_min_area;
    std::optional<double>& cell_max_area;
    int& cell_feature;
    int& cell_connectivity;
    int& cell_preview_body;
    bool& show_cells;
    TissuePreview& cell_preview;
    std::size_t& cell_annotations_added;
    std::uint64_t& kept_analysis_generation;
    std::uint64_t& kept_analysis_id;
    std::uint64_t& pending_analysis_recheck;
    std::uint64_t& kept_tissue_generation;
    std::uint64_t& verifying_tissue;
    std::uint64_t& kept_cell_generation;
    std::uint64_t& verifying_cells;
    std::uint64_t& recheck_analysis_generation;
    std::uint64_t& recheck_analysis_id;
    std::uint64_t& recheck_tissue_generation;
    std::uint64_t& recheck_tissue_id;
    std::uint64_t& recheck_cell_generation;
    std::uint64_t& recheck_cell_id;
    RecordRemovalDialog& record_removal;
    const std::uint64_t& workspace_epoch;
    CellTableOptions& cell_table_options;
    CellTableExportJob& cell_table_job;
    const std::function<StainTransform(bool)>& make_transform;
    const std::function<void(GuiInputTarget)>& mark_target;
    const std::function<void(int)>& focus_tables;
    const std::function<TissueRequest()>& build_tissue_request;
    const std::function<CellRequest()>& build_cell_request;
    const std::function<SamplingInputKey()>& sampling_key;
    const std::function<void(GuiExportOperation,std::vector<std::uint64_t>)>& begin_export;
    bool measurement_export_busy=false;
    ui::Availability region_availability(const char* original) const {
        const std::string_view action=original;
        if(editing_blocked)return ui::WorkflowBlocked();
        if(action=="Cancel analysis" || action=="Cancel estimate (Ctrl+F3)")return {};
        if(action=="Estimate stains (F3)")return stain_estimate.busy()?ui::Availability{false,"Wait for the current work to finish."}:ui::Availability{};
        if(action=="Use estimated stains (Shift+F3)")return stain_estimate.result()?ui::Availability{}:ui::Availability{false,"Run a preview first."};
        if(action=="Go to tables###ROI tables")return {};
        if(analysis.busy())return {false,"Wait for the current work to finish."};
        if(action=="Analyze region (Ctrl+M)")return {};
        if(action=="Keep result (Ctrl+K)") {
            const auto status=analysis_status;
            if(status.state!="ready")return {false,"Run a preview first."};
            if(kept_analysis_generation==status.generation || (recheck_analysis_generation==status.generation && recheck_analysis_id))return {false,"Run again for a new result."};
            return {};
        }
        const auto saved=selected?document.latest_analysis(selected):std::nullopt;
        if(action=="Recheck saved (Ctrl+Shift+K)") {
            if(!saved)return {false,"Select a kept result first."};
            const auto record_state=document.analysis_status(saved->id);
            if(record_state=="object_changed")return {false,"Restore the annotation before rechecking."};
            if(record_state=="source_closed")return {false,"The kept result's image source is unavailable."};
        }
        return {};
    }

    ui::Availability tissue_availability(const char* original) const {
        const std::string_view action=original;
        if(editing_blocked)return ui::WorkflowBlocked();
        if(action=="Cancel tissue")return {};
        if(action=="Go to tables###Tissue tables")return {};
        if(tissue.busy())return {false,"Wait for the current work to finish."};
        if(action=="Detect tissue (Ctrl+D)")return {};
        if(action=="Trace contours (Ctrl+J)") {
            if(!has_geos())return {false,"Use a build with this feature enabled."};
            return tissue.result()?ui::Availability{}:ui::Availability{false,"Run a preview first."};
        }
        if(action=="Add annotations (Ctrl+Shift+J)") {
            const auto contours=tissue.contours();
            if(!contours)return {false,"Trace contours"};
            if(contours->components().empty())return {false,"Run again for a new result."};
            if(tissue.contours_published())return {false,"Run again for a new result."};
            return {};
        }
        if(action=="Keep tissue mask (Ctrl+U)") {
            const auto status=tissue.poll(document,selected);
            if(status.state!="ready" || !tissue.result())return {false,"Run a preview first."};
            if(kept_tissue_generation==status.generation || (recheck_tissue_generation==status.generation && recheck_tissue_id))return {false,"Run again for a new result."};
            return {};
        }
        const auto saved=selected?document.latest_tissue(selected):nullptr;
        if(action=="Recheck saved mask (Ctrl+Shift+U)") {
            if(!saved)return {false,"Select a kept result first."};
            const auto record_state=document.tissue_status(saved->id);
            if(record_state=="object_changed")return {false,"Restore the annotation before rechecking."};
            if(record_state=="source_closed")return {false,"The kept result's image source is unavailable."};
        }
        return {};
    }

    ui::Availability cells_availability(const char* original) const {
        const std::string_view action=original;
        if(action=="Export cell table (Ctrl+Alt+X)")return kept_cells_export_availability(document,selected,cell_table_options,cell_table_job.busy() || measurement_export_busy,editing_blocked);
        if(editing_blocked)return ui::WorkflowBlocked();
        if(action=="Cancel cells (Ctrl+Alt+G)")return {};
        if(action=="Go to tables###Cell tables")return {};
        if(action!="Add kept cells###Add saved cells" && cell_job.busy())return {false,"Wait for the current work to finish."};
        if(action=="Detect cells (Ctrl+G)")return {};
        if(action=="Add cells (Ctrl+Shift+G)") {
            if(!has_geos())return {false,"Use a build with this feature enabled."};
            if(!cell_job.result() || cell_job.result()->features().empty())return {false,"Run a preview first."};
            if(cell_job.published())return {false,"Run again for a new result."};
            return {};
        }
        if(action=="Keep cell run (Ctrl+Shift+C)") {
            const auto status=cell_job.poll(document,selected);
            if(status.state!="ready" || !cell_job.result())return {false,"Run a preview first."};
            if(kept_cell_generation==status.generation || (recheck_cell_generation==status.generation && recheck_cell_id))return {false,"Run again for a new result."};
            return {};
        }
        const auto saved=selected?document.latest_cells(selected):nullptr;
        if(action=="Recheck cells (Ctrl+Alt+C)" || action=="Add kept cells###Add saved cells") {
            if(!saved)return {false,"Select a kept result first."};
            const auto record_state=document.cell_status(saved->id);
            if(action=="Add kept cells###Add saved cells") {
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                if(record_state!="current")return {false,"Recheck the kept result first."};
                return {};
            }
            if(record_state=="object_changed")return {false,"Restore the annotation before rechecking."};
            if(record_state=="source_closed")return {false,"The kept result's image source is unavailable."};
        }
        return {};
    }

};
inline void provenance_info(const std::shared_ptr<const AnalysisProvenance>& p) {
        if(!p) {ui::TextWrapped("Source/calibration history unavailable");return;}
        ui::TextWrapped("Recorded with qupathxx %s",p->environment->app_version.c_str());
        if(p->effective_pixel_width_um && p->effective_pixel_height_um) ui::TextWrapped("Captured calibration: %.5g / %.5g %s",*p->effective_pixel_width_um,*p->effective_pixel_height_um,ui::unit::um_per_px);
        else ui::TextWrapped("Captured calibration: unknown");
}
inline void draw(TaskContext& context) {
    // A frame uses one answer per action for its presenter, widget and key.
    AvailabilityAnswers region_answers;
    const auto region_available=[&](const char* action) {
        return region_answers.get(action,[&](const char* name){return context.region_availability(name);});
    };
    AvailabilityAnswers tissue_answers;
    const auto tissue_available=[&](const char* action) {
        return tissue_answers.get(action,[&](const char* name){return context.tissue_availability(name);});
    };
    AvailabilityAnswers cells_answers;
    const auto cells_available=[&](const char* action) {
        return cells_answers.get(action,[&](const char* name){return context.cells_availability(name);});
    };
    using Task=ui::WorkflowTask;using ExportOp=GuiExportOperation;
    auto& tissue_sampling_cache=context.state.tissue_sampling_cache;
    auto& cell_sampling_cache=context.state.cell_sampling_cache;
    auto& workspace=context.workspace;
    auto& workflow_task=context.workflow_task;
    auto& options=context.options;
    auto& document=context.document;
    auto& selected=context.selected;
    auto& io=context.io;
    auto& editing_blocked=context.editing_blocked;
    auto& image=context.image;
    auto& analysis=context.analysis;
    auto& analysis_status=context.analysis_status;
    auto& tissue=context.tissue;
    auto& cell_job=context.cell_job;
    auto& stain_estimate=context.stain_estimate;
    auto& stain_estimate_applied=context.stain_estimate_applied;
    auto& stain_estimate_error=context.stain_estimate_error;
    auto& stain_estimate_parameters=context.stain_estimate_parameters;
    auto& inputs=context.inputs;
    auto& input_feedback=context.input_feedback;
    auto& stain_model_io=context.stain_model_io;
    auto& stain_model_document=context.stain_model_document;
    auto& stain_model_revision=context.stain_model_revision;
    auto& stain_model_project=context.stain_model_project;
    auto& stain_model_image=context.stain_model_image;
    auto& stain_model_kind=context.stain_model_kind;
    auto& stain_model_vectors=context.stain_model_vectors;
    auto& stain_model_white=context.stain_model_white;
    auto& stain_model_basis=context.stain_model_basis;
    auto& stain_model_basis_white=context.stain_model_basis_white;
    auto& stain_model_bounds=context.stain_model_bounds;
    auto& project_replacements=context.project_replacements;
    auto& active_image=context.active_image;
    auto& paths=context.paths;
    auto& path_context=context.path_context;
    auto& output_gate=context.output_gate;
    auto& pixel_classifier=context.pixel_classifier;
    auto& notifications=context.notifications;
    auto& tissue_parameters=context.tissue_parameters;
    auto& tissue_grid=context.tissue_grid;
    auto& tissue_min_area=context.tissue_min_area;
    auto& tissue_max_area=context.tissue_max_area;
    auto& tissue_feature=context.tissue_feature;
    auto& tissue_connectivity=context.tissue_connectivity;
    auto& show_tissue=context.show_tissue;
    auto& tissue_preview=context.tissue_preview;
    auto& tissue_annotations_added=context.tissue_annotations_added;
    auto& cell_parameters=context.cell_parameters;
    auto& cell_grid=context.cell_grid;
    auto& cell_nominal=context.cell_nominal;
    auto& grid_cells=context.grid_cells;
    auto& cell_min_area=context.cell_min_area;
    auto& cell_max_area=context.cell_max_area;
    auto& cell_feature=context.cell_feature;
    auto& cell_connectivity=context.cell_connectivity;
    auto& cell_preview_body=context.cell_preview_body;
    auto& show_cells=context.show_cells;
    auto& cell_preview=context.cell_preview;
    auto& cell_annotations_added=context.cell_annotations_added;
    auto& kept_analysis_generation=context.kept_analysis_generation;
    auto& kept_analysis_id=context.kept_analysis_id;
    auto& pending_analysis_recheck=context.pending_analysis_recheck;
    auto& kept_tissue_generation=context.kept_tissue_generation;
    auto& verifying_tissue=context.verifying_tissue;
    auto& kept_cell_generation=context.kept_cell_generation;
    auto& verifying_cells=context.verifying_cells;
    auto& recheck_analysis_generation=context.recheck_analysis_generation;
    auto& recheck_analysis_id=context.recheck_analysis_id;
    auto& recheck_tissue_generation=context.recheck_tissue_generation;
    auto& recheck_tissue_id=context.recheck_tissue_id;
    auto& recheck_cell_generation=context.recheck_cell_generation;
    auto& recheck_cell_id=context.recheck_cell_id;
    auto& record_removal=context.record_removal;
    auto& workspace_epoch=context.workspace_epoch;
    auto& cell_table_options=context.cell_table_options;
    auto& cell_table_job=context.cell_table_job;
    auto& make_transform=context.make_transform;
    auto& mark_target=context.mark_target;
    auto& focus_tables=context.focus_tables;
    auto& build_tissue_request=context.build_tissue_request;
    auto& build_cell_request=context.build_cell_request;
    auto& sampling_key=context.sampling_key;
    auto& begin_export=context.begin_export;
            if(!workspace.enabled() || workflow_task==Task::Region) {
    ui::i18n::SurfaceTrace translation_surface("task/region");
            if(!workspace.enabled()) ui::SeparatorText("Region analysis");
            input_feedback.next_editor=AnalysisInputFeedback::Editor::Region;
            draw_analysis_inputs_editor(inputs,AnalysisInputs::Kind|AnalysisInputs::Nucleus|AnalysisInputs::IntensityStatistics);
            if(!workspace.enabled() || ui::CollapsingHeader("Advanced inputs###Analysis inputs",!options.stain_model_path.empty()?ImGuiTreeNodeFlags_DefaultOpen:0)) {
            draw_analysis_inputs_editor(inputs,AnalysisInputs::RoiSampling|AnalysisInputs::White|AnalysisInputs::Vectors);
            if(inputs.kind==0)ui::TextWrapped("Stain 1 and Stain 2 are used by Tissue and Cells stain features and pixel-classifier training.");
            if(inputs.kind==1) {
                ui::TextWrapped("Supply validated OD directions. An imported complete basis is kept until a vector/white edit; otherwise channel 3 is a signed residual.");
            }
            ui::SeparatorText("Stain model");
            ImGui::BeginDisabled(stain_model_io.busy());
            paths.stain_load.draw(path_context,false,{},[&](bool){
            ImGui::BeginDisabled(!has_interchange());
            if(ui::IconButton("Load stain model",ui::Icon::Open,ui::Availability{has_interchange() && !stain_model_io.busy(),!has_interchange()?"File interchange is unavailable in this build.":"Wait for the current work to finish."})) {
                stain_model_document=&document;stain_model_revision=document.revision();stain_model_project=project_replacements;stain_model_image=active_image;stain_model_kind=inputs.kind;
                stain_model_vectors=inputs.vectors;stain_model_white=inputs.white;stain_model_basis=inputs.estimated_basis;stain_model_basis_white=inputs.estimated_white;
                stain_model_io.load(paths.stain_load.state().resolved);
            }
            {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();stain_model_bounds[0]={a.x,a.y,b.x-a.x,b.y-a.y};mark_target(GuiInputTarget::LoadStainModel);}
            ImGui::EndDisabled();});
            paths.stain_save.draw(path_context,false,{},[&](bool){
            if(ui::IconButton("Save stain model",ui::Icon::Save,ui::Availability{!stain_model_io.busy(),"Wait for the current work to finish."})) {
                try {
                    std::vector<std::pair<std::string,std::string>> sources;
                    const auto source=document.server()->reader_source();
                    if(source) {if(source->primary)sources.emplace_back(source->primary->path,source->primary->identity);for(const auto& file:source->companions)sources.emplace_back(file.path,file.identity);}
                    if(options.project && !options.project->path().empty())sources.emplace_back(options.project->path(),std::string{});
                    const auto locator=document.server()->source_locator();
                    if(!locator.empty() && std::none_of(sources.begin(),sources.end(),[&](const auto& file){return file.first==locator;}))sources.emplace_back(locator,source && source->primary?source->primary->identity:std::string{});
                    const auto model=make_transform(true);
                    std::vector<std::string> source_paths;for(const auto& source:sources)source_paths.push_back(source.first);
                    output_gate.submit(paths.stain_save.state().resolved,detail::output_scope("Stain model",false,"Stain model"),
                        [&,model,sources=std::move(sources)](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& protected_paths) {
                            if(!stain_model_io.save(model,path,sources,policy,protected_paths))throw ui::NoticeException<std::runtime_error>("Wait for the current export to finish.");
                            output_gate.watch(path,[&]()->std::optional<bool>{if(stain_model_io.busy())return {};return stain_model_io.state()=="saved";},[&]{return stain_model_io.state()=="failed"?stain_model_io.error():std::string{};});
                        },[&](const std::string& message){stain_model_io.fail(message,"failed",GuiStainModel::Operation::Save);},false,{},std::move(source_paths));
                } catch(const std::exception& e) {stain_model_io.fail(e.what(),"failed",GuiStainModel::Operation::Save);notifications.post(ui::exception_notice("Stain model",e,paths.stain_save.state().resolved));}
            }
            {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();stain_model_bounds[1]={a.x,a.y,b.x-a.x,b.y-a.y};mark_target(GuiInputTarget::SaveStainModel);}
            });ImGui::EndDisabled();
            if(!has_interchange())ui::TextWrapped("Model import is unavailable in this build.");
            if(stain_model_io.state()!="idle") {
                ui::TextWrapped("Model file: %s",stain_model_io.caption().c_str());
            }
            if(!stain_model_io.digest().empty())ui::TextWrapped("File model SHA256: %s",stain_model_io.digest().c_str());
            ui::SeparatorText("Stain estimation");
                bool changed=false;
                ui::SetNextItemWidth(100);changed=ui::InputDouble("Minimum OD",&stain_estimate_parameters.min_od,0,0,"%.5g") || changed;
                ui::SetNextItemWidth(100);changed=ui::InputDouble("Trim fraction",&stain_estimate_parameters.trim_quantile,0,0,"%.5g") || changed;
                ui::SetNextItemWidth(100);changed=ui::InputDouble("Min angle (deg)",&stain_estimate_parameters.min_angle_degrees,0,0,"%.5g") || changed;
                ui::SetNextItemWidth(100);changed=ui::InputDouble("Max residual",&stain_estimate_parameters.max_residual_fraction,0,0,"%.5g") || changed;
                ui::SetNextItemWidth(100);changed=ui::InputScalar("Read pixels",ImGuiDataType_U64,&stain_estimate_parameters.max_read_pixels) || changed;
                std::uint64_t limit=stain_estimate_parameters.sample_limit;
                ui::SetNextItemWidth(100);if(ui::InputScalar("Kept samples",ImGuiDataType_U64,&limit)) {stain_estimate_parameters.sample_limit=static_cast<std::size_t>(limit);changed=true;}
                ui::SetNextItemWidth(80);changed=ui::InputInt("Level (-1 auto)",&stain_estimate_parameters.level) || changed;
                if(changed) {stain_estimate.cancel();stain_estimate_applied=false;stain_estimate_error.clear();}
            }
            if(!stain_model_io.error().empty())ui::TextWrapped("%s",stain_model_io.error().c_str());
            input_feedback.draw(AnalysisInputFeedback::Editor::Model);
            const bool estimate_key=!editing_blocked && ui::chord_pressed(ui::CommandId::Action_Estimate_stains_F3,ui::frame_commands());
            if(inputs.kind==1 || stain_estimate.busy() || stain_estimate.result() || estimate_key || !editing_blocked && ui::chord_pressed(ui::CommandId::Action_Use_estimated_stains_Shift_F3,ui::frame_commands()) || !editing_blocked && ui::chord_pressed(ui::CommandId::Action_Cancel_estimate_Ctrl_F3,ui::frame_commands())) {
            const auto estimate_available=region_available("Estimate stains (F3)");
            const bool estimate_clicked=ui::WorkflowAction("Estimate stains (F3)",ui::design::Action::Secondary,estimate_available) || (estimate_available.enabled && estimate_key);
            if(estimate_clicked) {try {stain_estimate.request(StainEstimateRequest(document,selected,stain_estimate_parameters,{inputs.white[0],inputs.white[1],inputs.white[2]},inputs.nucleus));stain_estimate_applied=false;stain_estimate_error.clear();} catch(const std::exception& e) {{stain_estimate_error=e.what();if(!stain_estimate_error.empty())notifications.post(ui::exception_notice("Stain estimation",e,{}));}}}
            const auto cancel_estimate_available=region_available("Cancel estimate (Ctrl+F3)");
            if((cancel_estimate_available.enabled && ui::chord_pressed(ui::CommandId::Action_Cancel_estimate_Ctrl_F3,ui::frame_commands())) || (stain_estimate.busy() && ui::WorkflowIcon("Cancel estimate (Ctrl+F3)",ui::Icon::Stop,cancel_estimate_available))) {stain_estimate.cancel();stain_estimate_applied=false;}
            }
            const auto estimate_status=stain_estimate.poll(document,selected);
            if(stain_estimate.busy()) ui::Text("Stain tiles: %llu / %llu",static_cast<unsigned long long>(estimate_status.completed_tiles),static_cast<unsigned long long>(estimate_status.total_tiles));
            if(const auto estimate=stain_estimate.result()) {
                ui::Text("Stain separation: %.2f deg",estimate->angle_degrees);ui::Text("Plane residual: %.4g",estimate->residual_fraction);
                const bool use=ui::WorkflowAction("Use estimated stains (Shift+F3)",ui::design::Action::Secondary,region_available("Use estimated stains (Shift+F3)")) || (region_available("Use estimated stains (Shift+F3)").enabled && ui::chord_pressed(ui::CommandId::Action_Use_estimated_stains_Shift_F3,ui::frame_commands()));
                if(use) {
                    input_feedback.next_editor=AnalysisInputFeedback::Editor::Region;
                    inputs.use_stains(estimate->stains,estimate->white);stain_estimate_applied=true;
                }
                if(stain_estimate_applied) ui::TextWrapped("Estimated vectors are active for subsequent analysis and detection.");
            }
            if(!stain_estimate_error.empty()) ui::TextWrapped("%s",stain_estimate_error.c_str());
            if(!estimate_status.error.empty()) ui::TextWrapped("Stain estimate: %s",estimate_status.error.c_str());
            if(estimate_status.state=="stale") ui::TextWrapped("Stain estimation inputs changed; estimate again.");
            const auto estimate_cancelled=input_feedback.message(InputConsumer::StainEstimation,estimate_status.generation);
            if(!estimate_cancelled.empty())ui::TextWrapped("%s",estimate_cancelled.c_str());
            input_feedback.draw(AnalysisInputFeedback::Editor::Region);
            ui::ResultModel region_model;
            region_model.availability=[&](const char* action){return region_available(action);};
            region_model.raw_run=analysis_status.state;region_model.run=ui::parse_run_state(analysis_status.state);region_model.busy=analysis.busy();
            region_model.cancellation=input_feedback.message(InputConsumer::Region,analysis_status.generation);
            region_model.has_preview=bool(analysis.result());region_model.project=bool(options.project);
            region_model.preview_is_recheck=recheck_analysis_generation==analysis_status.generation && recheck_analysis_id;
            region_model.preview_is_kept=kept_analysis_generation==analysis_status.generation;
            region_model.can_keep=region_available("Keep result (Ctrl+K)").enabled;
            region_model.done=analysis_status.completed_tiles;region_model.total=analysis_status.total_tiles;
            region_model.cancel_original="Cancel analysis";region_model.error_format="Analysis: %s";
            region_model.is_busy=[&]{return analysis.busy();};
            ui::ResultActions region_model_actions;region_model_actions.cancel=[&]{analysis.cancel();};
            const auto run_available=region_available("Analyze region (Ctrl+M)");
            const bool analysis_key=run_available.enabled && ui::chord_pressed(ui::CommandId::Action_Analyze_region_Ctrl_M,ui::frame_commands());
            const bool start_analysis=ui::WorkflowAction("Analyze region (Ctrl+M)",ui::result_role(region_model,ui::ResultPrimary::Run),run_available) || analysis_key;
            ui::WorkflowReason("Analyze region (Ctrl+M)",run_available);
            if(start_analysis) {
                try {const auto transform=make_transform(inputs.kind==1);
                    const auto kind=inputs.kind==0?AnalysisKind::OpticalDensityMeans:AnalysisKind::StainMeans;
                    analysis.request(inputs.intensity_statistics
                        ?AnalysisRequest::with_intensity_statistics(document,selected,transform,kind,inputs.nucleus,256,inputs.roi_sampling)
                        :AnalysisRequest(document,selected,transform,kind,inputs.nucleus,256,inputs.roi_sampling));notifications.post({ui::Severity::Info,ui::workflow_task_labels[1],"The operation started.",{},true});}
                catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[1],e,{}));}
            }
            region_model.busy=analysis.busy();
            region_model.cancel_requested=region_available("Cancel analysis").enabled && ui::chord_pressed(ui::CommandId::Action_Cancel_analysis,ui::frame_commands());
            region_model.error=analysis_status.error;
            ui::draw_run_status(region_model,region_model_actions);
            auto intensity_summary=[](const std::shared_ptr<const CompartmentIntensityStatistics>& statistics) {
                if(!statistics) return;
                for(int domain=0;domain<2;++domain) {
                    const auto& values=domain?statistics->channels:statistics->rgb;
                    const char* label=domain?"Channel":"RGB";
                    for(std::size_t c=0;c<3;++c) {
                        ui::Text("%s %d sum: %.6g",label,int(c+1),values.sum[c]);
                        ui::Text("%s %d minimum: %.6g",label,int(c+1),values.minimum[c]);
                        ui::Text("%s %d maximum: %.6g",label,int(c+1),values.maximum[c]);
                        if(values.sample_standard_deviation) ui::Text("%s %d sample SD: %.6g",label,int(c+1),(*values.sample_standard_deviation)[c]);
                        else ui::Text("%s %d sample SD: undefined (one sample)",label,int(c+1));
                    }
                }
            };
            const auto kept_preview_record=document.latest_analysis(selected);
            const bool preview_is_recheck=recheck_analysis_generation==analysis_status.generation && recheck_analysis_id;
            const bool preview_is_kept=analysis_status.state=="ready" && kept_analysis_generation==analysis_status.generation &&
                kept_preview_record && kept_analysis_id && kept_preview_record->id==kept_analysis_id;
            if(const auto result=analysis.result();result && !preview_is_kept && !preview_is_recheck) {
                if(result->intensity_statistics && ImGui::TreeNode("Intensity summary")) {intensity_summary(result->intensity_statistics);ImGui::TreePop();}
                ui::WorkflowMeans("preview","Preview result",result->kind,result->means,0,result->nucleus);
                ui::Text("Samples: %llu",static_cast<unsigned long long>(result->samples));
                if(ui::CollapsingHeader("Details###Region preview timing"))ui::Text("Run time: %.3f ms",result->elapsed_ms);
            }

            const bool can_keep=region_available("Keep result (Ctrl+K)").enabled;
            if(preview_is_recheck) ui::TextWrapped("This run is a recheck of kept result %llu.",static_cast<unsigned long long>(recheck_analysis_id));
            if(analysis.result() && !preview_is_kept && !preview_is_recheck) {
            detail::retained_result_hint(bool(options.project));
            if(!document.analysis_record_count())ui::TextDisabled("Keep this result to include it in tables.");
            }
            const ui::ResultIds region_ids{"Keep result (Ctrl+K)","Go to tables###ROI tables","Recheck saved (Ctrl+Shift+K)",nullptr,"Remove kept result###Remove region result","Details###Region kept timing"};
            region_model.can_keep=region_available("Keep result (Ctrl+K)").enabled;region_model.show_keep=analysis.result() && !preview_is_kept && !preview_is_recheck;
            region_model.preview_is_kept=preview_is_kept;region_model.preview_is_recheck=preview_is_recheck;
            region_model.kept_count=document.analysis_record_count();region_model.keep_requested=region_available("Keep result (Ctrl+K)").enabled && ui::chord_pressed(ui::CommandId::Action_Keep_result_Ctrl_K,ui::frame_commands());
            region_model_actions.keep=[&] {
                try {kept_analysis_id=analysis.keep_result(document,selected);kept_analysis_generation=analysis_status.generation;notifications.resolve(ui::workflow_task_labels[1]);}
                catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[1],e,{}));}
                region_model.kept_count=document.analysis_record_count();
            };
            region_model_actions.view=[&]{focus_tables(1);};
            ui::draw_keep_row(region_model,region_ids,region_model_actions);
            if(const auto saved=document.latest_analysis(selected)) {
                const auto state=document.analysis_status(saved->id);
                region_model.kept=ui::KeptView{saved->id,"",ui::parse_record_state(state),false,state};
                region_model.provenance=[&]{provenance_info(saved->result.provenance);};
                region_model.leading_content=[&]{ui::WorkflowMeans("kept","Kept result",saved->result.kind,saved->result.means,saved->id,saved->result.nucleus,false);ui::Text("Samples: %llu",static_cast<unsigned long long>(saved->result.samples));};
                region_model.trailing_content=[&]{
                    ui::Text("%s / %s",saved->result.kind==AnalysisKind::StainMeans?"Stains":"Optical density",saved->result.nucleus?"Nucleus":"Body");
                    ui::Text("Result ID: %llu",static_cast<unsigned long long>(saved->id));
                    if(saved->result.intensity_statistics && ImGui::TreeNode(ui::i18n::label("Kept intensity summary###Saved intensity summary").c_str())) {intensity_summary(saved->result.intensity_statistics);ImGui::TreePop();}
                };
                region_model.can_recheck=region_available("Recheck saved (Ctrl+Shift+K)").enabled;
                region_model.recheck_requested=region_available("Recheck saved (Ctrl+Shift+K)").enabled && ui::chord_pressed(ui::CommandId::Action_Recheck_saved_Ctrl_Shift_K,ui::frame_commands());
                region_model_actions.recheck=[&] {
                    try {
                        auto replay=AnalysisRequest::replay(document,saved->id);
                        analysis.request(replay);pending_analysis_recheck=saved->id;
                        notifications.post({ui::Severity::Info,ui::workflow_task_labels[1],"The operation started.",{},true});
                    } catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[1],e,{}));}
                };
                region_model_actions.remove=[&]{record_removal.request_record(document,workspace_epoch,"Region",saved->id,&Document::analysis_record,&Document::remove_analysis);};
                region_model.after_actions=[&]{
                    if(ImGui::TreeNode(ui::i18n::label("Kept provenance###Saved provenance").c_str())) {
                        ui::Text("Record: %llu",static_cast<unsigned long long>(saved->id));
                        ui::TextWrapped("Algorithm: %s",saved->result.algorithm.c_str());
                        ui::TextWrapped("Input SHA-256: %s",saved->result.input_sha256.c_str());ImGui::TreePop();
                    }
                };
                ui::draw_kept_block(region_model,region_ids,region_model_actions,[&]{ui::Text("Run time: %.3f ms",saved->result.elapsed_ms);});
            }
            }
            if(!workspace.enabled() || workflow_task==Task::Tissue) {
    ui::i18n::SurfaceTrace translation_surface("task/tissue");
            if(!workspace.enabled()) ui::SeparatorText("Threshold tissue mask");
            const auto tissue_status=tissue.poll(document,selected);
            ImGui::PushItemWidth(-1);
            bool tissue_changed=ui::Combo("Feature###Tissue feature",&tissue_feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0");
            tissue_changed=ui::InputDouble("Threshold >=",&tissue_parameters.threshold,0,0,"%.5g") || tissue_changed;
            tissue_changed=ui::InputDouble("Sigma (sampled px)###Sigma (px)",&tissue_parameters.smoothing_sigma,0,0,"%.5g") || tissue_changed;

            {
                auto key=sampling_key();key.parameters(tissue_parameters);key.append(tissue_feature,tissue_connectivity,tissue_min_area,tissue_max_area,bool(tissue_grid));
                if(tissue_grid)key.append(tissue_grid->samples_x,tissue_grid->samples_y,tissue_grid->context_x,tissue_grid->context_y);
                tissue_sampling_cache.update(std::move(key),[&]{return detail::sampling_summary(build_tissue_request());});
                detail::draw_sampling_summary(tissue_sampling_cache);
                ui::TextDisabled("Threshold uses the selected feature's units.");
            }
            if(!workspace.enabled() || ui::CollapsingHeader("Advanced settings###Tissue settings")) {
            tissue_changed=detail::tissue_sampling_controls(tissue_grid) || tissue_changed;
            int tissue_sampling=static_cast<int>(tissue_parameters.sampling_policy);
            if(ui::Combo("Tissue polygon samples",&tissue_sampling,"Geometry coverage\0Polygon winding\0")) {tissue_parameters.sampling_policy=static_cast<RoiSamplingPolicy>(tissue_sampling);tissue_changed=true;}
            if(!tissue_grid) {
            const auto level_label=tissue_parameters.level<0?std::string("Auto (budget)"):"Level "+std::to_string(tissue_parameters.level);
            if(ui::ComboScope combo{"Tissue level",level_label.c_str()};combo) {
                if(ui::Selectable("Auto (work budget)",tissue_parameters.level<0)) {tissue_parameters.level=-1;tissue_changed=true;}
                for(std::size_t i=0;i<image.levels.size();++i) {
                    const auto label="Level "+std::to_string(i)+" / "+std::to_string(image.levels[i].downsample)+"x";
                    if(ui::Selectable(ui::data_label(label,std::to_string(i)).c_str(),tissue_parameters.level==static_cast<std::int32_t>(i))) {tissue_parameters.level=static_cast<std::int32_t>(i);tissue_changed=true;}
                }
            }
            } else ui::TextDisabled("Area means from source level zero");
            tissue_changed=ui::Combo("Connectivity",&tissue_connectivity,"4 neighbors\0 8 neighbors\0") || tissue_changed;
            tissue_changed=ui::InputScalar("Minimum (sampled pixel count)###Min pixels",ImGuiDataType_U64,&tissue_parameters.min_pixels) || tissue_changed;
            tissue_changed=ui::InputScalar("Maximum (sampled pixel count; 0=off)###Max (0=off)",ImGuiDataType_U64,&tissue_parameters.max_pixels) || tissue_changed;
            tissue_changed=ui::Checkbox("Median cleanup (3x3)",&tissue_parameters.median_cleanup) || tissue_changed;
            tissue_changed=ui::Checkbox("Stable pyramid snapshot",&tissue_parameters.canonical_pyramid) || tissue_changed;
            if(tissue_parameters.canonical_pyramid) ui::TextWrapped("Choose a level above zero. Reads that pyramid once for consistent samples.");
            std::uint64_t tissue_work_mib=tissue_parameters.work_bytes/(1024u*1024u);
            if(ui::InputScalar("Tissue memory (MiB)",ImGuiDataType_U64,&tissue_work_mib)) {
                tissue_parameters.work_bytes=static_cast<std::size_t>(std::clamp<std::uint64_t>(tissue_work_mib,1,512)*1024u*1024u);tissue_changed=true;
            }
            tissue_changed=ui::InputInt("Opening radius (sampled px)###Opening radius",&tissue_parameters.open_radius) || tissue_changed;
            tissue_changed=ui::InputInt("Closing radius (sampled px)###Closing radius",&tissue_parameters.close_radius) || tissue_changed;
            tissue_changed=ui::Checkbox("Fill enclosed holes",&tissue_parameters.fill_holes) || tissue_changed;
            if(tissue_parameters.fill_holes) tissue_changed=ui::InputScalar("Hole maximum (sampled pixel count; 0=all)###Hole max (0=all)",ImGuiDataType_U64,&tissue_parameters.max_hole_pixels) || tissue_changed;
            ui::TextDisabled("Sigma/radii/sizes: sampled px");
            ImGui::PushID("tissue_physical_area");
            tissue_changed=detail::cell_area_controls(tissue_min_area,tissue_max_area,true) || tissue_changed;
            ImGui::PopID();
            }
            ImGui::PopItemWidth();
            if(tissue_changed) tissue.cancel();
            ui::ResultModel tissue_model;
            tissue_model.availability=[&](const char* action){return tissue_available(action);};
            tissue_model.raw_run=tissue_status.state;tissue_model.run=ui::parse_run_state(tissue_status.state);tissue_model.busy=tissue.busy();
            tissue_model.cancellation=input_feedback.message(InputConsumer::Tissue,tissue_status.generation);
            tissue_model.has_preview=bool(tissue.result());tissue_model.project=bool(options.project);
            tissue_model.preview_is_recheck=recheck_tissue_generation==tissue_status.generation && recheck_tissue_id;
            tissue_model.preview_is_kept=kept_tissue_generation==tissue_status.generation;
            tissue_model.can_keep=tissue_available("Keep tissue mask (Ctrl+U)").enabled;
            tissue_model.done=tissue_status.completed_tiles;tissue_model.total=tissue_status.total_tiles;
            tissue_model.cancel_original="Cancel tissue";tissue_model.error_format="Tissue: %s";
            tissue_model.is_busy=[&]{return tissue.busy();};
            ui::ResultActions tissue_model_actions;tissue_model_actions.cancel=[&]{tissue.cancel();};
            const auto run_available=tissue_available("Detect tissue (Ctrl+D)");
            const bool tissue_key=run_available.enabled && ui::chord_pressed(ui::CommandId::Action_Detect_tissue_Ctrl_D,ui::frame_commands());
            const bool detect_clicked=ui::WorkflowAction("Detect tissue (Ctrl+D)",ui::result_role(tissue_model,ui::ResultPrimary::Run),run_available) || tissue_key;
            ui::WorkflowReason("Detect tissue (Ctrl+D)",run_available);
            if(detect_clicked) {
                try {
                    tissue_parameters.feature=static_cast<TissueFeature>(tissue_feature);tissue_parameters.connectivity=tissue_connectivity?8:4;
                    tissue.request(build_tissue_request());tissue_annotations_added=0;notifications.post({ui::Severity::Info,ui::workflow_task_labels[2],"The operation started.",{},true});
                } catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[2],e,{}));}
            }
            tissue_model.busy=tissue.busy();
            tissue_model.phase=ui::tissue_step_sentence(tissue_status.phase);tissue_model.cancel_requested=tissue_available("Cancel tissue").enabled && ui::chord_pressed(ui::CommandId::Action_Cancel_tissue,ui::frame_commands());
            tissue_model.error=tissue_status.error;ui::draw_run_status(tissue_model,tissue_model_actions);
            if(tissue.result()) {
            ui::Checkbox("Show tissue mask",&show_tissue);
            if(const auto result=tissue.result()) {
                if(result->scientific_grid()) ui::Text("Grid %.5g x %.5g px / %lld x %lld",result->downsample_x(),result->downsample_y(),static_cast<long long>(result->width()),static_cast<long long>(result->height()));
                else ui::Text("Level %d / %.5gx / %lld x %lld",result->level(),result->downsample(),static_cast<long long>(result->width()),static_cast<long long>(result->height()));
                ui::TextDisabled("Size filters count sampled pixels");
                ui::Text("Components: %zu",result->components().size());
                ui::Text("Kept pixels: %llu",static_cast<unsigned long long>(result->foreground_pixels()));
                ui::Text("Cleaned: %llu / filled: %llu",static_cast<unsigned long long>(result->cleaned_pixels()),static_cast<unsigned long long>(result->filled_pixels()));
                ui::Text("Samples: %llu",static_cast<unsigned long long>(result->samples()));
                if(ui::CollapsingHeader("Details###Tissue preview timing"))ui::Text("Run time: %.3f ms",result->elapsed_ms());
                if(show_tissue && !tissue_preview.ready()) ui::TextDisabled("Preparing mask preview...");
            }
            const auto trace_available=tissue_available("Trace contours (Ctrl+J)");
            const bool trace_clicked=ui::WorkflowAction("Trace contours (Ctrl+J)",ui::design::Action::Secondary,trace_available) || (trace_available.enabled && ui::chord_pressed(ui::CommandId::Action_Trace_contours_Ctrl_J,ui::frame_commands()));
            if(trace_clicked) {try {tissue.request_contours();tissue_annotations_added=0;notifications.post({ui::Severity::Info,ui::workflow_task_labels[2],"The operation started.",{},true});} catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[2],e,{}));}}
            const auto contours=tissue.contours();
            const auto add_available=tissue_available("Add annotations (Ctrl+Shift+J)");
            const bool keep_contours_clicked=ui::WorkflowAction("Add annotations (Ctrl+Shift+J)",ui::design::Action::Secondary,add_available) || (add_available.enabled && ui::chord_pressed(ui::CommandId::Action_Add_annotations_Ctrl_Shift_J,ui::frame_commands()));
            if(keep_contours_clicked) {try {tissue_annotations_added=tissue.keep_contours(document,selected).size();notifications.resolve(ui::workflow_task_labels[2]);} catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[2],e,{}));}}
            if(contours) ui::Text("Contours: %zu / %zu vertices",contours->components().size(),contours->vertex_count());
            if(tissue.contours_published()) ui::Text("Added %zu annotations (Undo available)",tissue_annotations_added);
            if(!has_geos()) ui::TextDisabled("Contour creation requires GEOS");
            const bool tissue_is_recheck=recheck_tissue_generation==tissue_status.generation && recheck_tissue_id;
            if(tissue_is_recheck) ui::TextWrapped("This run is a recheck of kept result %llu.",static_cast<unsigned long long>(recheck_tissue_id));
            tissue_model.can_keep=tissue_available("Keep tissue mask (Ctrl+U)").enabled;
            detail::retained_result_hint(bool(options.project));
            if(!document.tissue_record_count())ui::TextDisabled("Keep this result to include it in tables.");
            }
            const ui::ResultIds tissue_ids{"Keep tissue mask (Ctrl+U)","Go to tables###Tissue tables","Recheck saved mask (Ctrl+Shift+U)",nullptr,"Remove kept result###Remove saved mask"};
            tissue_model.busy=tissue.busy();
            tissue_model.show_keep=bool(tissue.result());tissue_model.kept_count=document.tissue_record_count();
            tissue_model.keep_requested=tissue_available("Keep tissue mask (Ctrl+U)").enabled && ui::chord_pressed(ui::CommandId::Action_Keep_tissue_mask_Ctrl_U,ui::frame_commands());
            tissue_model_actions.keep=[&]{try {tissue.keep_result(document,selected);kept_tissue_generation=tissue_status.generation;notifications.resolve(ui::workflow_task_labels[2]);} catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[2],e,{}));}tissue_model.kept_count=document.tissue_record_count();};
            tissue_model_actions.view=[&]{focus_tables(2);};
            ui::draw_keep_row(tissue_model,tissue_ids,tissue_model_actions);
            if(const auto saved=document.latest_tissue(selected)) {
                const auto state=document.tissue_status(saved->id);
                tissue_model.kept=ui::KeptView{saved->id,"",ui::parse_record_state(state),false,state,"Kept mask: %s"};
                tissue_model.provenance=[&]{provenance_info(saved->result->provenance());};
                tissue_model.trailing_content=[&]{ui::Text("Kept components: %zu",saved->result->components().size());};
                tissue_model.can_recheck=tissue_available("Recheck saved mask (Ctrl+Shift+U)").enabled;
                tissue_model.recheck_requested=tissue_available("Recheck saved mask (Ctrl+Shift+U)").enabled && ui::chord_pressed(ui::CommandId::Action_Recheck_saved_mask_Ctrl_Shift_U,ui::frame_commands());
                tissue_model_actions.recheck=[&]{try {
                    tissue.request(TissueRequest::replay(document,saved->id));verifying_tissue=saved->id;
                    notifications.post({ui::Severity::Info,ui::workflow_task_labels[2],"The operation started.",{},true});
                } catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[2],e,{}));}};
                tissue_model_actions.remove=[&]{record_removal.request_record(document,workspace_epoch,"Tissue",saved->id,&Document::tissue_record,&Document::remove_tissue);};
                ui::draw_kept_block(tissue_model,tissue_ids,tissue_model_actions,{});
            }

            if(!tissue_preview.error().empty()) ui::TextWrapped("Mask: %s",tissue_preview.error().c_str());
            }
            if(!workspace.enabled() || workflow_task==Task::Cells) {
    ui::i18n::SurfaceTrace translation_surface("task/cells");
            if(!workspace.enabled()) ui::SeparatorText("Watershed cells");
            ImGui::PushItemWidth(-1);
            bool cell_changed=false;
            if(!cell_nominal && !cell_grid) {
            ui::SetNextItemWidth(-1);cell_changed=ui::Combo("Cell feature",&cell_feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0") || cell_changed;
            cell_changed=detail::cell_detector_controls(cell_parameters) || cell_changed;
            cell_changed=ui::InputDouble("Cell threshold",&cell_parameters.threshold,0,0,"%.5g") || cell_changed;
            cell_changed=ui::InputDouble("Blur sigma (sampled px)###Blur sigma",&cell_parameters.sigma,0,0,"%.3g") || cell_changed;
            cell_changed=ui::InputDouble("Cell expansion (sampled px)###Cell expansion",&cell_parameters.cell_expansion,0,0,"%.3g") || cell_changed;
            } else ui::TextDisabled(cell_nominal?"Nominal RGB cell grid":"Explicit cell grid");
            const bool sampled_shape=cell_parameters.algorithm==CellAlgorithm::SampledLogWatershedBodies;
            if(sampled_shape) cell_connectivity=1;

            {
                auto key=sampling_key();key.parameters(cell_parameters);key.parameters(grid_cells);
                key.append(cell_feature,cell_connectivity,cell_min_area,cell_max_area,bool(cell_grid),bool(cell_nominal));
                if(cell_grid)key.append(cell_grid->samples_x,cell_grid->samples_y,cell_grid->context_x,cell_grid->context_y);
                if(cell_nominal)key.append(cell_nominal->downsample,cell_nominal->context_x,cell_nominal->context_y,cell_nominal->sampling);
                cell_sampling_cache.update(std::move(key),[&]{return detail::sampling_summary(build_cell_request(),document.pixel_width_um(),document.pixel_height_um());});
                detail::draw_sampling_summary(cell_sampling_cache);
                ui::TextDisabled("Threshold uses the selected feature's units.");
            }
            if(!workspace.enabled() || ui::CollapsingHeader("Advanced settings###Cell settings")) {
            cell_changed=detail::cell_sampling_controls(cell_grid,cell_nominal,grid_cells) || cell_changed;
            if(cell_nominal) cell_changed=detail::nominal_cell_controls(*cell_nominal,grid_cells) || cell_changed;
            else if(cell_grid) cell_changed=detail::grid_cell_controls(*cell_grid,grid_cells) || cell_changed;
            else {
            ImGui::BeginDisabled(sampled_shape);
            cell_changed=ui::InputDouble("Peak radius >= (sampled px)###Peak radius >=",&cell_parameters.min_peak_radius,0,0,"%.3g") || cell_changed;
            cell_changed=ui::InputDouble("Seed spacing (sampled px)###Seed spacing",&cell_parameters.min_seed_distance,0,0,"%.3g") || cell_changed;
            ImGui::EndDisabled();
            cell_changed=ui::InputScalar("Nucleus minimum (sampled pixel count)###Nucleus min px",ImGuiDataType_U64,&cell_parameters.min_nucleus_pixels) || cell_changed;
            cell_changed=ui::InputScalar("Nucleus maximum (sampled pixel count; 0=all)###Nucleus max (0=all)",ImGuiDataType_U64,&cell_parameters.max_nucleus_pixels) || cell_changed;
            cell_changed=detail::cell_area_controls(cell_min_area,cell_max_area) || cell_changed;
            ImGui::BeginDisabled(sampled_shape);
            cell_changed=ui::Combo("Cell neighbors",&cell_connectivity,"4 neighbors\0 8 neighbors\0") || cell_changed;
            ImGui::EndDisabled();
            const auto cell_level_label=cell_parameters.level<0?std::string("Auto (budget)"):"Level "+std::to_string(cell_parameters.level);
            if(ui::ComboScope combo{"Cell level",cell_level_label.c_str()};combo) {
                if(ui::Selectable("Auto (budget)",cell_parameters.level<0)) {cell_parameters.level=-1;cell_changed=true;}
                for(std::size_t i=0;i<image.levels.size();++i) {const auto label="Level "+std::to_string(i);if(ui::Selectable(ui::data_label(label,std::to_string(i)).c_str(),cell_parameters.level==static_cast<std::int32_t>(i))) {cell_parameters.level=static_cast<std::int32_t>(i);cell_changed=true;}}
            }
            ui::TextWrapped("Radii and nucleus count limits use sampled pixels.");
            }
            }
            if(cell_changed) cell_job.cancel();
            const auto cell_status=cell_job.poll(document,selected);
            ui::ResultModel cells_model;
            cells_model.availability=[&](const char* action){return cells_available(action);};
            cells_model.raw_run=cell_status.state;cells_model.run=ui::parse_run_state(cell_status.state);cells_model.busy=cell_job.busy();
            cells_model.cancellation=input_feedback.message(InputConsumer::Cells,cell_status.generation);
            cells_model.has_preview=bool(cell_job.result());cells_model.project=bool(options.project);
            cells_model.preview_is_recheck=recheck_cell_generation==cell_status.generation && recheck_cell_id;
            cells_model.preview_is_kept=kept_cell_generation==cell_status.generation;
            cells_model.can_keep=cells_available("Keep cell run (Ctrl+Shift+C)").enabled;
            cells_model.done=cell_status.completed_tiles;cells_model.total=cell_status.total_tiles;
            cells_model.cancel_original="Cancel cells (Ctrl+Alt+G)";cells_model.error_format="Cells: %s";
            cells_model.is_busy=[&]{return cell_job.busy();};
            ui::ResultActions cells_model_actions;cells_model_actions.cancel=[&]{cell_job.cancel();};
            const auto run_available=cells_available("Detect cells (Ctrl+G)");
            const bool cell_key=run_available.enabled && ui::chord_pressed(ui::CommandId::Action_Detect_cells_Ctrl_G,ui::frame_commands());
            const bool detect_cells_clicked=ui::WorkflowAction("Detect cells (Ctrl+G)",ui::result_role(cells_model,ui::ResultPrimary::Run),run_available) || cell_key;
            ui::WorkflowReason("Detect cells (Ctrl+G)",run_available);
            if(detect_cells_clicked) {try {
                cell_parameters.feature=static_cast<TissueFeature>(cell_feature);cell_parameters.connectivity=cell_connectivity?8:4;
                cell_job.request(build_cell_request());cell_annotations_added=0;notifications.post({ui::Severity::Info,ui::workflow_task_labels[3],"The operation started.",{},true});
            } catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[3],e,{}));}}
            cells_model.busy=cell_job.busy();
            cells_model.phase=ui::cell_step_sentence(cell_status.phase);cells_model.error=cell_status.error;
            ui::draw_run_status(cells_model,cells_model_actions);
            if(cells_available("Cancel cells (Ctrl+Alt+G)").enabled && ui::chord_pressed(ui::CommandId::Action_Cancel_cells_Ctrl_Alt_G,ui::frame_commands())) cell_job.cancel();
            ImGui::PopItemWidth();
            if(const auto result=cell_job.result()) {
                ui::Checkbox("Show cell preview",&show_cells);ui::Combo("Cell preview",&cell_preview_body,"Nuclei\0Cell bodies\0");
                ui::Text("Detected cells: %zu",result->features().size());ui::Text("Markers: %zu",result->markers().size());
                if(ui::CollapsingHeader("Details###Cells preview timing"))ui::Text("Run time: %.3f ms",result->elapsed_ms());
                if(const auto* nominal=result->nuclei()->nominal_sampling()) {
                    ui::Text("Nominal RGB grid: %lld x %lld",static_cast<long long>(nominal->core_width()),static_cast<long long>(nominal->core_height()));
                    ui::Text("Nominal sample step: %.5g px",nominal->source_request().downsample);
                } else if(const auto* grid=result->nuclei()->scientific_grid()) {
                    ui::Text("Cell grid: %lld x %lld",static_cast<long long>(grid->x().samples()),static_cast<long long>(grid->y().samples()));
                    ui::Text("Sample steps: %.5g x %.5g px",result->nuclei()->downsample_x(),result->nuclei()->downsample_y());
                } else ui::Text("Cell level: %d / %.5gx",result->nuclei()->level(),result->nuclei()->downsample());
            }
            if(cell_job.result()) {
            const auto add_available=cells_available("Add cells (Ctrl+Shift+G)");
            const bool add_cells=ui::WorkflowAction("Add cells (Ctrl+Shift+G)",ui::design::Action::Secondary,add_available) || (add_available.enabled && ui::chord_pressed(ui::CommandId::Action_Add_cells_Ctrl_Shift_G,ui::frame_commands()));
            if(add_cells) {try {cell_annotations_added=cell_job.keep_result(document,selected).size();notifications.resolve(ui::workflow_task_labels[3]);} catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[3],e,{}));}}
            if(cell_job.published()) ui::Text("Added %zu cells (Undo available)",cell_annotations_added);
            detail::retained_result_hint(bool(options.project));
            if(!document.cell_record_count())ui::TextDisabled("Keep this result to include it in tables.");
            const bool cells_is_recheck=recheck_cell_generation==cell_status.generation && recheck_cell_id;
            if(cells_is_recheck) ui::TextWrapped("This run is a recheck of kept result %llu.",static_cast<unsigned long long>(recheck_cell_id));
            cells_model.can_keep=cells_available("Keep cell run (Ctrl+Shift+C)").enabled;
            }
            const ui::ResultIds cells_ids{"Keep cell run (Ctrl+Shift+C)","Go to tables###Cell tables","Recheck cells (Ctrl+Alt+C)","Add kept cells###Add saved cells","Remove kept result###Remove cell result"};
            cells_model.busy=cell_job.busy();
            cells_model.show_keep=bool(cell_job.result());cells_model.kept_count=document.cell_record_count();
            cells_model.keep_requested=cells_available("Keep cell run (Ctrl+Shift+C)").enabled && ui::chord_pressed(ui::CommandId::Action_Keep_cell_run_Ctrl_Shift_C,ui::frame_commands());
            cells_model_actions.keep=[&]{try {cell_job.keep_record(document,selected);kept_cell_generation=cell_status.generation;notifications.resolve(ui::workflow_task_labels[3]);} catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[3],e,{}));}cells_model.kept_count=document.cell_record_count();};
            cells_model_actions.view=[&]{focus_tables(0);};
            ui::draw_keep_row(cells_model,cells_ids,cells_model_actions);
            if(const auto saved=document.latest_cells(selected)) {
                const auto state=document.cell_status(saved->id);
                cells_model.kept=ui::KeptView{saved->id,"",ui::parse_record_state(state),state=="current" && has_geos(),state,"Cell record: %s"};
                cells_model.provenance=[&]{provenance_info(saved->result->nuclei()->provenance());};
                cells_model.leading_content=[&]{ui::Text("Kept cell run: %llu",static_cast<unsigned long long>(saved->id));ui::Text("Kept cells: %zu",saved->result->features().size());};
                cells_model.trailing_content=[&]{
                if(saved->result->grid_parameters()) {
                    const auto& frozen=*saved->result->resolved_grid_parameters();
                    ui::TextWrapped("Kept grid nucleus samples: %llu to %llu",static_cast<unsigned long long>(frozen.min_nucleus_samples),static_cast<unsigned long long>(frozen.max_nucleus_samples));
                } else {
                    const auto& frozen=saved->result->parameters();
                    if(frozen.max_nucleus_pixels) ui::TextWrapped("Kept nucleus samples: %llu to %llu",
                        static_cast<unsigned long long>(frozen.min_nucleus_pixels),static_cast<unsigned long long>(frozen.max_nucleus_pixels));
                    else ui::TextWrapped("Kept nucleus samples: at least %llu",static_cast<unsigned long long>(frozen.min_nucleus_pixels));
                }
                };
                cells_model.can_recheck=cells_available("Recheck cells (Ctrl+Alt+C)").enabled;
                cells_model.recheck_requested=cells_available("Recheck cells (Ctrl+Alt+C)").enabled && ui::chord_pressed(ui::CommandId::Action_Recheck_cells_Ctrl_Alt_C,ui::frame_commands());
                cells_model_actions.recheck=[&]{try {
                    cell_job.request(CellRequest::replay(document,saved->id));verifying_cells=saved->id;notifications.post({ui::Severity::Info,ui::workflow_task_labels[3],"The operation started.",{},true});
                } catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[3],e,{}));}};
                cells_model_actions.remove=[&]{record_removal.request_record(document,workspace_epoch,"Cells",saved->id,&Document::cell_record,&Document::remove_cells);};
                cells_model_actions.add=[&]{try {document.publish_cell_record(saved->id);notifications.resolve(ui::workflow_task_labels[3]);} catch(const std::exception& e) {notifications.post(ui::exception_notice(ui::workflow_task_labels[3],e,{}));}};
                cells_model.after_actions=[&]{
                const auto export_available=cells_available("Export cell table (Ctrl+Alt+X)");
                const bool export_table=ui::WorkflowAction("Export cell table (Ctrl+Alt+X)",ui::design::Action::Secondary,export_available);
                if(export_table) {
                    begin_export(ExportOp::SavedCells,{saved->id});
                }
                };
                ui::draw_kept_block(cells_model,cells_ids,cells_model_actions,{});
            }
            if(!has_geos()) ui::TextDisabled("Cell objects require GEOS");

            if(!cell_preview.error().empty()) ui::TextWrapped("Cell preview: %s",cell_preview.error().c_str());

            }
}
}
