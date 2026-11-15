#include "gui_cell_record_rows.hpp"
#include "gui_result_state.hpp"
#include "gui_analysis_tasks.hpp"
#include <iterator>
#include <initializer_list>
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/qupath_project.hpp"
#include "qupathxx/project_source.hpp"
#include "ascii_case.hpp"
#include "reader_registry.hpp"
#include "annotation_format_registry.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_recovery.hpp"
#include "qupathxx/project_save.hpp"
#include "qupathxx/interchange.hpp"
#include "qupathxx/analysis.hpp"
#include "gui_profile.hpp"
#include "gui_text_buffers.hpp"
#include "gui_draft.hpp"
#include "gui_project_forms.hpp"
#include "gui_core_paths.hpp"
#include "gui_stain_model.hpp"
#include "gui_entry_editor.hpp"
#include "gui_calibration_editor.hpp"
#include "gui_class_editor.hpp"
#include "gui_tiles.hpp"
#include "gui_thumbnail.hpp"
#include "gui_project_thumbnails.hpp"
#include "gui_workspace.hpp"
#include "gui_tool_catalog.hpp"
#include "gui_scale.hpp"
#include "gui_status_summary.hpp"
#include "gui_fonts.hpp"
#include "gui_tissue.hpp"
#include "qupathxx/cells.hpp"
#include "calibrated_cell_numeric.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "gui_replay.hpp"
#include "gui_output_gate.hpp"
#include "gui_sampling.hpp"
#include "gui_measurements.hpp"
#include "selection_ranges.hpp"
#include "viewer_bridge.hpp"
#include <mutex>
#include "gui_overlay.hpp"
#include "gui_batch.hpp"
#include "gui_tma.hpp"
#include "gui_spatial.hpp"
#include "gui_genes.hpp"
#include "gui_spatial_import.hpp"
#include "gui_registration.hpp"
#include "gui_fluorescence.hpp"
#include "gui_dark_flat.hpp"
#include "gui_linear_unmixing.hpp"
#include "gui_project_density.hpp"
#include "gui_project_browser.hpp"
#include "gui_project_inspector.hpp"
#include "gui_raw.hpp"
#include "gui_raw_pixel.hpp"
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
#include "gui_kept_counts.hpp"
#include "gui_dialogs.hpp"
#include "gui_header.hpp"
#include "gui_theme.hpp"
#include "gui_object_classifier.hpp"
#include "gui_pixel_classifier.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <future>
#include <fstream>
#include <filesystem>
#include <tuple>
#include <cstdio>
#include <variant>
#include <cstring>
#include <limits>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace qpx::detail {
std::weak_ptr<const char> tool_document_identity(const Document& doc){return GuiTma::document_identity(doc);}
}
namespace qpx {
namespace {
std::mutex viewer_mutex;
void image_data_value(const std::string& key,const ImageDataValue& v) {
    ImGui::PushID(key.c_str());const char* label=key.empty()?"(empty key)":key.c_str();
    const auto* list=std::get_if<ImageDataValue::List>(&v.value);const auto* map=std::get_if<ImageDataValue::Map>(&v.value);
    if(list || map) {
        const auto size=list?list->size():map->size();
        if(ImGui::TreeNodeEx("value",size<=8?ImGuiTreeNodeFlags_DefaultOpen:0,"%s (%s, %zu)",label,list?"list":"map",size)) {
            if(list)for(std::size_t i=0;i<list->size();++i)image_data_value(std::to_string(i),(*list)[i]);
            else for(const auto& [k,item]:*map)image_data_value(k,item);
            ImGui::TreePop();
        }
    } else {
        ui::DataText(label);ImGui::SameLine();
        switch(v.value.index()) {
            case 0:ui::TextUnformatted("null");break;
            case 1:ui::Text("bool: %s",std::get<bool>(v.value)?"true":"false");break;
            case 2:ui::Text("integer: %lld",static_cast<long long>(std::get<std::int64_t>(v.value)));break;
            case 3:ui::Text("real: %.17g",std::get<double>(v.value));break;
            case 4:ui::TextUnformatted("text:");ImGui::SameLine();ui::DataText(std::get<std::string>(v.value).c_str());break;
        }
    }
    ImGui::PopID();
}
std::string recovery_label(const RecoveryInfo& info) {
    if(!info.error.empty()) return "Unreadable recovery";
    auto seconds=static_cast<std::time_t>(info.created_unix_ms/1000); std::tm utc{};
#ifdef _WIN32
    if(gmtime_s(&utc,&seconds)!=0) return "Recovery (unknown date)";
#else
    if(!gmtime_r(&seconds,&utc)) return "Recovery (unknown date)";
#endif
    std::ostringstream text; text.imbue(std::locale::classic());
    text<<std::put_time(&utc,"%Y-%m-%d %H:%M UTC")<<" / "<<info.objects<<" objects"; return text.str();
}
bool same_recovery_file(const std::string& a,const std::string& b) {
    if(a.empty() || b.empty()) return false;
    std::error_code error;
    if(std::filesystem::equivalent(std::filesystem::u8path(a),std::filesystem::u8path(b),error) && !error) return true;
    return std::filesystem::u8path(a).lexically_normal()==std::filesystem::u8path(b).lexically_normal();
}
ImVec2 screen(const Viewport& v, Point p, ImVec2 offset) {
    auto s = v.image_to_screen(p); return {offset.x + static_cast<float>(s.x), offset.y + static_cast<float>(s.y)};
}
void draw_shape(ImDrawList* draw,const Roi& geometry,const Viewport& view,ImVec2 offset,ImU32 color) {
    auto r=geometry.bounds();
    auto p0=screen(view,{r.x,r.y},offset),p1=screen(view,{r.x+r.width,r.y+r.height},offset);
    if(geometry.kind()==RoiKind::Rectangle) { draw->AddRect(p0,p1,color,0.f,2.f,0); return; }
    if(geometry.kind()==RoiKind::Ellipse) {
        draw->AddEllipse({(p0.x+p1.x)/2,(p0.y+p1.y)/2},{(p1.x-p0.x)/2,(p1.y-p0.y)/2},color,0,0,2); return;
    }
    for(const auto& path:geometry.paths()) {
        if(geometry.kind()==RoiKind::Points) { for(auto p:path) draw->AddCircleFilled(screen(view,p,offset),4,color); continue; }
        std::vector<ImVec2> vertices; vertices.reserve(path.size());
        for(auto p:path) vertices.push_back(screen(view,p,offset));
        draw->AddPolyline(vertices.data(),static_cast<int>(vertices.size()),color,2.f,geometry.is_area()?ImDrawFlags_Closed:0);
    }
}
struct Session {
    GLFWwindow* window = nullptr;
    bool context = false, platform = false, renderer = false;
    ~Session() {
        if (renderer) ImGui_ImplOpenGL3_Shutdown();
        if (platform) ImGui_ImplGlfw_Shutdown();
        if (context) ImGui::DestroyContext();
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }
};
class MeanJob {
    std::future<double> future_;
    std::shared_ptr<std::atomic<bool>> cancelled_;
    std::uint64_t ticket_=0;
    std::uint64_t revision_=0;
public:
    std::uint64_t measured=0;
    std::optional<double> value;
    std::string error;
    ~MeanJob() { if (cancelled_) cancelled_->store(true); }
    bool active() const { return future_.valid(); }
    void invalidate() {
        if(cancelled_) cancelled_->store(true);
        ticket_=0; revision_=std::numeric_limits<std::uint64_t>::max();
        measured=0; value.reset(); error.clear();
    }
    void poll(std::uint64_t selection,std::uint64_t revision) {
        if ((ticket_!=selection || revision_!=revision) && cancelled_) { cancelled_->store(true); value.reset(); error.clear(); }
        if (active() && future_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try { auto result=future_.get(); if (ticket_==selection && revision_==revision) { measured=ticket_; value=result; error.clear(); } }
            catch(const std::exception& e) { if (ticket_==selection && revision_==revision) error=e.what(); }
        }
    }
    void start(std::shared_ptr<ImageServer> server, const Annotation& a,std::uint64_t revision) {
        ticket_=a.id; revision_=revision; value.reset(); error.clear(); cancelled_=std::make_shared<std::atomic<bool>>(false);
        future_=std::async(std::launch::async,[server=std::move(server),roi=a.geometry,cancelled=cancelled_] { return mean_rgb(*server,roi,cancelled.get()); });
    }
};
void capture(const std::string& path, int width, int height) {
    if (width<=0 || height<=0) throw std::runtime_error("cannot capture a minimized framebuffer");
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width)*height*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("framebuffer capture failed");
    std::ofstream file(std::filesystem::u8path(path),std::ios::binary);
    file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for (int y=height-1;y>=0;--y) file.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*width*3),static_cast<std::streamsize>(width)*3);
    file.close(); if (!file) throw std::runtime_error("framebuffer capture write failed");
}

// Keep mutually exclusive request temporaries outside the viewer's large frame.
#if defined(_MSC_VER)
#define QPX_GUI_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define QPX_GUI_NOINLINE __attribute__((noinline))
#else
#define QPX_GUI_NOINLINE
#endif
template<class Transform>
QPX_GUI_NOINLINE TissueRequest make_tissue_request(const Document& document,std::uint64_t selected,
    const TissueParameters& parameters,int feature,int connectivity,const TissueGridSampling* grid,
    bool nucleus,std::optional<double> minimum,std::optional<double> maximum,const Transform& make_transform) {
    auto p=parameters;p.feature=static_cast<TissueFeature>(feature);p.connectivity=connectivity?8:4;
    if(grid) {
        const auto transform=make_transform(feature!=0);
        if(detail::analysis_inputs_trace)detail::analysis_inputs_trace->tissue_request_stains=transform.stains();
        return TissueRequest::for_grid_roi(document,selected,transform,*grid,p,nucleus,minimum,maximum);
    }
    const auto transform=make_transform(feature!=0);
    if(detail::analysis_inputs_trace)detail::analysis_inputs_trace->tissue_request_stains=transform.stains();
    return TissueRequest::with_component_area_limits(document,selected,transform,minimum,maximum,p,nucleus);
}
template<class Transform>
QPX_GUI_NOINLINE CellRequest make_cell_request(const Document& document,std::uint64_t selected,
    const CellParameters& parameters,int feature,int connectivity,const std::optional<NominalCellRoiSampling>& nominal,
    const std::optional<GridCellSampling>& grid,const GridCellParameters& grid_cells,
    std::optional<double> minimum,std::optional<double> maximum,const Transform& make_transform) {
    auto p=parameters;p.feature=static_cast<TissueFeature>(feature);p.connectivity=connectivity?8:4;
    if(nominal) {
        const auto transform=make_transform(grid_cells.feature!=TissueFeature::OpticalDensitySum);
        return CellRequest::for_nominal_roi(document,selected,transform,nominal->downsample,nominal->context_x,nominal->context_y,grid_cells,nominal->sampling);
    }
    if(grid) {
        const auto transform=make_transform(grid_cells.feature!=TissueFeature::OpticalDensitySum);
        return CellRequest::for_grid_roi(document,selected,transform,*grid,grid_cells);
    }
    const auto transform=make_transform(feature!=0);
    return CellRequest::with_nucleus_area_limits(document,selected,transform,minimum,maximum,p);
}
struct NoticeSources {
    ui::Notifications& notifications;
    detail::GuiPixelClassifier& pixel_classifier;
    detail::GuiObjectClassifier& classifier;
    detail::GuiBatch& batch;
    detail::GuiFluorescence& fluorescence;
    AnalysisJob& analysis;StainEstimateJob& stain_estimate;TissueJob& tissue;CellJob& cell_job;
    FileFingerprintJob& fingerprint_job;detail::GuiCorePaths& paths;MeanJob& mean;
    TissuePreview& tissue_preview;TissuePreview& cell_preview;detail::GuiStainModel& stain_model_io;
    detail::OutputGate& output_gate;AutosaveStats& autosave_stats;GuiWorkspace& workspace;
    GuiThumbnail& overview;GuiTiles* tiles;ui::RmlShell* shell;Document& document;std::uint64_t selected;
};
struct GuiNoticeObservers {
    std::array<ui::NoticeJob,13> notice_jobs;
    std::array<std::string,9> noticed_errors;
    char status_summary[512]{};
    QPX_GUI_NOINLINE void observe(NoticeSources sources);
};
}
GuiReport run_gui(Document& document, int frame_limit, const std::string& capture_path, const GuiOptions& options) {
    ui::i18n::SurfaceTrace translation_surface("shell");
    std::unique_lock<std::mutex> viewer_lock(viewer_mutex,std::try_to_lock);
    if(!viewer_lock.owns_lock()) throw std::runtime_error("a viewer is already running");
    if (frame_limit < 0) throw std::invalid_argument("frame_limit must be nonnegative");
    if (!capture_path.empty() && !frame_limit) throw std::invalid_argument("capture_path requires a positive frame_limit");
    if(options.settle_tiles && !frame_limit) throw std::invalid_argument("tile settlement requires a positive frame_limit");
    const auto annotation_formats=detail::annotation_format_catalog();
    const bool annotation_geojson_available=std::any_of(annotation_formats.begin(),annotation_formats.end(),
        [](const auto& format){return format.provider_id=="geojson" && format.available;});
    {ThumbnailOptions overview_options;overview_options.source_pixel_limit=options.overview_source_pixel_limit;overview_options.validate();}
    GuiReplay replay(options.inputs,options.replay_timeout_seconds);
    detail::GuiProfiler profiler;
    auto batch_owner=std::make_unique<detail::GuiBatch>(options);auto& batch=*batch_owner;
    auto spatial_owner=std::make_unique<detail::GuiSpatial>(options);auto& spatial=*spatial_owner;
    auto density_owner=std::make_unique<detail::GuiProjectDensity>();auto& project_density=*density_owner;
    auto tma_owner=std::make_unique<detail::GuiTma>(options);auto& tma=*tma_owner;
    auto raw_owner=std::make_unique<detail::GuiRaw>(options);auto& raw=*raw_owner;
    auto classifier_owner=std::make_unique<detail::GuiObjectClassifier>(options);auto& classifier=*classifier_owner;
    GuiReplayDeadline settlement(options.replay_timeout_seconds);
    GuiReplayDeadline minimized_startup(options.replay_timeout_seconds);
    options.display.validate();
    if(options.analysis_vectors.size()!=6 || options.analysis_white.size()!=3 || options.analysis_kind<0 || options.analysis_kind>1)
        throw std::invalid_argument("invalid initial analysis parameters");
    detail::AnalysisInputs inputs(options);
    auto make_transform=[&](bool separate) {
        bool same=separate && bool(inputs.estimated_basis);
        if(same) for(std::size_t c=0;c<3;++c) same=same && inputs.white[c]==inputs.estimated_white[c] && inputs.vectors[c]==(*inputs.estimated_basis)[0][c] && inputs.vectors[c+3]==(*inputs.estimated_basis)[1][c];
        if(same) return StainTransform::from_normalized(*inputs.estimated_basis,inputs.estimated_white);
        return StainTransform(separate?std::vector<ColorVector>{{inputs.vectors[0],inputs.vectors[1],inputs.vectors[2]},
        {inputs.vectors[3],inputs.vectors[4],inputs.vectors[5]}}:std::vector<ColorVector>{{1,0,0},{0,1,0}},
        {inputs.white[0],inputs.white[1],inputs.white[2]});};
    make_transform(options.analysis_kind==1);
    validate_sampling_policy(options.roi_sampling_policy);
    validate_sampling_policy(options.tissue_sampling_policy);
    TissueParameters tissue_parameters;
    tissue_parameters.sampling_policy=options.tissue_sampling_policy;
    tissue_parameters.feature=static_cast<TissueFeature>(options.tissue_feature);tissue_parameters.threshold=options.tissue_threshold;tissue_parameters.smoothing_sigma=options.tissue_smoothing_sigma;
    tissue_parameters.connectivity=options.tissue_connectivity;tissue_parameters.min_pixels=options.tissue_min_pixels;tissue_parameters.max_pixels=options.tissue_max_pixels;
    tissue_parameters.level=options.tissue_level;tissue_parameters.open_radius=options.tissue_open_radius;tissue_parameters.close_radius=options.tissue_close_radius;
    detail::validate_tissue_area_limits(options.tissue_min_component_area_um2,options.tissue_max_component_area_um2);
    auto tissue_min_area=options.tissue_min_component_area_um2,tissue_max_area=options.tissue_max_component_area_um2;
    tissue_parameters.canonical_pyramid=options.tissue_canonical_pyramid;tissue_parameters.work_bytes=options.tissue_work_bytes;
    tissue_parameters.median_cleanup=options.tissue_median_cleanup;tissue_parameters.fill_holes=options.tissue_fill_holes;tissue_parameters.max_hole_pixels=options.tissue_max_hole_pixels;tissue_parameters.validate();
    auto tissue_grid=options.tissue_grid?std::make_shared<const TissueGridSampling>(*options.tissue_grid):nullptr;
    if(tissue_grid) {
        tissue_grid->validate();
        if(tissue_parameters.level>0) throw std::invalid_argument("tissue grid requires level zero/auto");
        tissue_parameters.level=0;
    }
    if(!std::isfinite(options.autosave_interval_seconds) || options.autosave_interval_seconds<0 || options.autosave_interval_seconds>86400)
        throw std::invalid_argument("autosave interval must be between 0 and 86400 seconds");
    if(options.project && options.project->active_image() && options.project->document(options.project->active_image()).get()!=&document)
        throw std::invalid_argument("GUI document must be the project's active image");
    if(options.start_view && (!std::isfinite(options.start_view->zoom) || options.start_view->zoom<=0 ||
        !std::isfinite(options.start_view->origin.x) || !std::isfinite(options.start_view->origin.y))) throw std::invalid_argument("invalid initial viewport");
    if (options.width<640 || options.width>8192 || options.height<480 || options.height>8192) throw std::invalid_argument("invalid window dimensions");
    GuiScale::validate(options.ui_scale);
    for (const auto& event:options.inputs) {
        const bool window_control=event.kind==GuiInputKind::ResizeWindow || event.kind==GuiInputKind::MinimizeWindow || event.kind==GuiInputKind::RestoreWindow || event.kind==GuiInputKind::SetUiScale;
        if(event.frame<(window_control?0:2) || event.frame>=frame_limit ||
           (!window_control && (!std::isfinite(event.x) || !std::isfinite(event.y) ||
            (event.target!=GuiInputTarget::Image && (event.x<0 || event.x>1 || event.y<0 || event.y>1)) ||
            !std::isfinite(event.wheel) || std::abs(event.wheel)>100)))
            throw std::invalid_argument("invalid replay event; frame must be inside finite run, starting at 2 for UI input");
    }
    const bool startup_requested=!options.startup_kind.empty();
    if(startup_requested) {
        if((options.startup_kind!="image" && options.startup_kind!="project" && options.startup_kind!="recovery") ||
           options.startup_path.empty() || options.startup_path.size()>32768 || options.startup_path.find('\0')!=std::string::npos ||
           !options.project || options.project->revision_!=0 || !options.project->images().empty() || !options.project->path().empty())
            throw std::invalid_argument("startup requires an empty project, a valid path and image/project/recovery kind");
        if(options.startup_kind=="image" && options.startup_image_id) throw std::invalid_argument("startup image ID requires a stored project");
    } else if(!options.startup_path.empty() || options.startup_image_id) throw std::invalid_argument("startup path/image ID requires a startup kind");
    if(options.overlay_cache_budget>256u*1024u*1024u) throw std::invalid_argument("overlay cache budget exceeds 256 MiB");
    if(options.cell_table_path.size()>32768 || options.cell_table_path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid cell table path");
    CellTableOptions cell_table_options;cell_table_options.require_current=options.cell_table_require_current;
    cell_table_options.published_objects=options.cell_table_published_objects;
    cell_table_options.include_capture_calibration=options.cell_table_capture_calibration;
    cell_table_options.include_intensity_statistics=options.cell_table_intensity_statistics;
    cell_table_options.byte_limit=options.cell_table_byte_limit;cell_table_options.snapshot_budget=options.cell_table_snapshot_budget;cell_table_options.validate();
    for(const auto* path:{&options.file_fingerprint_path,&options.file_fingerprint_output}) if(path->size()>32768 || path->find('\0')!=std::string::npos) throw std::invalid_argument("invalid file fingerprint path");
    FileFingerprintOptions fingerprint_options;fingerprint_options.byte_limit=options.file_fingerprint_byte_limit;fingerprint_options.buffer_bytes=options.file_fingerprint_buffer_bytes;fingerprint_options.validate();
    bool fingerprint_associate=options.file_fingerprint_associate_source;
    std::shared_ptr<ProjectSourceReview> source_review;
    bool source_accept_changed=false,source_popup_requested=false;
    std::weak_ptr<Document> fingerprint_document;
    std::weak_ptr<Project> fingerprint_project;
    std::string fingerprint_generation;
    std::uint64_t fingerprint_image=0;
    std::array<Rect,2> source_review_bounds;
    FileFingerprintJob fingerprint_job;std::string fingerprint_error;bool fingerprint_refused=false;
    auto text_buffers=std::make_unique<detail::GuiTextBuffers>();
    auto forms_owner=std::make_unique<detail::GuiProjectForms>(options);auto& forms=*forms_owner;
    auto paths_owner=std::make_unique<detail::GuiCorePaths>(options);auto& paths=*paths_owner;
    std::string path_project_source;
    auto all_paths=forms.paths();for(auto* field:paths.paths())all_paths.push_back(field);
    std::vector<detail::GuiPathDrop> path_drops;
    detail::validate_cell_area_limits(options.cell_min_nucleus_area_um2,options.cell_max_nucleus_area_um2);
    detail::gui_font_assets(); // Authenticate packaged immutable bytes before creating a window.
    if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
    Session session;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    session.window = glfwCreateWindow(options.width, options.height, "qupathxx | Bioimage analysis", nullptr, nullptr);
    if (!session.window) throw std::runtime_error("OpenGL window creation failed");
    glfwSetWindowUserPointer(session.window,&path_drops);
    glfwSetDropCallback(session.window,[](GLFWwindow* window,int count,const char** paths) {
        detail::GuiPathDrop drop;glfwGetCursorPos(window,&drop.x,&drop.y);
        for(int i=0;i<count;++i)drop.paths.emplace_back(paths[i]);
        static_cast<std::vector<detail::GuiPathDrop>*>(glfwGetWindowUserPointer(window))->push_back(std::move(drop));
    });
    glfwMakeContextCurrent(session.window); glfwSwapInterval(1);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); session.context = true;ui::begin_frame();
    ImGui::GetIO().IniFilename=nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if(options.docked_workspace) ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ui::i18n::Session language_session;
    ui::design::Session design_session;
    GuiWorkspace workspace(options);
    const std::array<ImGuiID,5> command_workspace_windows={ImHashStr("Workspace"),ImHashStr(GuiWorkspace::project_pane),ImHashStr(GuiWorkspace::image_pane),ImHashStr(GuiWorkspace::objects_pane),ImHashStr(GuiWorkspace::workflow_pane)};
    ui::PathContext path_context{{},&workspace.projects.last_directory};
    auto genes_owner=std::make_unique<detail::GuiGeneDisplay>();auto& genes=*genes_owner;genes.load_preference(workspace.gene_preference);
    auto spatial_import=std::make_unique<detail::GuiSpatialImporter>();
    auto landmark_registration_owner=std::make_unique<detail::GuiLandmarkRegistration>();auto& landmark_registration=*landmark_registration_owner;
    auto fluorescence_owner=std::make_unique<detail::GuiFluorescence>();auto& fluorescence=*fluorescence_owner;
    using Task=ui::WorkflowTask;
    using ProjectForm=detail::ProjectForm;
    bool rename_project_image=false,focus_project_rename=false;
    std::uint64_t inspected_image=0;
    bool project_settings_visible=false;int project_settings_section=0;
    std::uint64_t relink_image_id=0,remove_image_id=0;
    std::weak_ptr<Project> remove_project;
    bool overview_failure_revealed=false;
    char tool_search[128]={};
    auto& project_form=forms.shown;
    auto& focus_add_image=forms.focus_image;auto& focus_open_project=forms.focus_open;auto& focus_save_project=forms.focus_save;
    Task last_workflow_task=workspace.workflow_task();
    int pending_language=-2;bool language_requested=false,language_dialog_active=false;char language_search[128]{};
    detail::GuiHelpWindow help_window;
    auto pixel_classifier_owner=std::make_unique<detail::GuiPixelClassifier>(options);auto& pixel_classifier=*pixel_classifier_owner;
    auto raw_pixel_owner=std::make_unique<detail::GuiRawPixel>(options);auto& raw_pixel=*raw_pixel_owner;
    for(auto* field:tma.paths())all_paths.push_back(field);
    for(auto* field:raw.paths())all_paths.push_back(field);
    for(auto* field:spatial.paths())all_paths.push_back(field);
    for(auto* field:batch.paths())all_paths.push_back(field);
    for(auto* field:project_density.paths())all_paths.push_back(field);
    for(auto* field:classifier.paths())all_paths.push_back(field);
    for(auto* field:pixel_classifier.paths())all_paths.push_back(field);
    for(auto* field:raw_pixel.paths())all_paths.push_back(field);
    for(auto* field:fluorescence.paths())all_paths.push_back(field);
    using Tool=ui::ToolId;
    const ui::ToolLauncher tools({
        {Tool::ObjectClassifier,[&]{classifier.open();}},
        {Tool::PixelClassifier,[&]{pixel_classifier.open();}},
        {Tool::RawPixelClassifier,[&]{raw_pixel.open();}},
        {Tool::Fluorescence,[&]{fluorescence.open();}},
        {Tool::Spatial,[&]{spatial.open();}},
        {Tool::Genes,[&]{genes.open();}},
        {Tool::Registration,[&]{landmark_registration.open();}},
        {Tool::SpatialImport,[&]{spatial_import->open(options.project.get());}},
        {Tool::Batch,[&]{batch.open();}},
        {Tool::Tma,[&]{tma.open();}},
        {Tool::Density,[&]{project_density.open();}},
        {Tool::Channels,[&]{raw.open();}}
    },{
        {ui::ToolChild::MarkerRules,[&]{fluorescence.open_marker_rules();}},
        {ui::ToolChild::DarkFlat,[&]{fluorescence.correction_tool().open();}},
        {ui::ToolChild::Unmixing,[&]{fluorescence.unmixing_tool().open();}},
        {ui::ToolChild::SavedStreams,[&]{pixel_classifier.open_saved_streams();}}
    });
    ui::apply_desktop_theme();
    auto& style = ImGui::GetStyle();
    ImGuiStyle base_style=style;auto applied_theme=ui::design::current();
    double ui_multiplier=options.ui_scale;
    GuiScale ui_scale,font_scale;bool fonts_initialized=false;std::uint64_t font_rebuilds=0;
    session.platform = ImGui_ImplGlfw_InitForOpenGL(session.window, true);
    if (!session.platform) throw std::runtime_error("ImGui GLFW initialization failed");
    session.renderer = ImGui_ImplOpenGL3_Init("#version 150");
    if (!session.renderer) throw std::runtime_error("ImGui OpenGL initialization failed");
    GLint max_texture = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    if (max_texture<256) throw std::runtime_error("GPU does not support 256-pixel tile textures");
    detail::configure_gui_fonts(*ImGui::GetIO().Fonts,max_texture);
    std::unique_ptr<ui::RmlShell> shell;
    if(workspace.enabled()) shell=std::make_unique<ui::RmlShell>(session.window);
    // Private UI backing only: never insert this into a project or request its
    // pixels. It keeps empty-state bindings separate from any removed document.
    auto empty_document=std::make_shared<Document>(Image(1,1,std::vector<std::uint8_t>{0,0,0}));
    Document* active_document=options.project && !options.project->active_image()?empty_document.get():&document;
    auto tiles=std::make_unique<GuiTiles>(active_document->server());
    std::shared_ptr<Document> active_owner=options.project && options.project->active_image()?options.project->document(options.project->active_image()):nullptr;
    auto active_image=options.project?options.project->active_image():0;
    MeanJob mean;
    detail::GuiStainModel stain_model_io;
    Document* stain_model_document=nullptr;std::uint64_t stain_model_revision=0,stain_model_project=0,stain_model_image=0;int stain_model_kind=0;
    std::vector<double> stain_model_vectors,stain_model_white;
    std::optional<std::array<ColorVector,3>> stain_model_basis;ColorVector stain_model_basis_white{};
    std::array<Rect,2> stain_model_bounds{};
    AnalysisJob analysis;StainEstimateJob stain_estimate;bool stain_estimate_applied=false;std::string stain_estimate_error;
    detail::analysis_tasks::State analysis_task_state(options);auto& stain_estimate_parameters=analysis_task_state.stain_estimate_parameters;
    TissueJob tissue;
    CellJob cell_job;TissuePreview cell_preview;

    if(options.measurement_table_path.size()>32768 || options.measurement_table_path.find('\0')!=std::string::npos || options.measurement_table_kind<0 || options.measurement_table_kind>1) throw std::invalid_argument("invalid measurement table options");
    MeasurementTableOptions measurement_options;measurement_options.require_current=options.measurement_table_require_current;
    measurement_options.byte_limit=options.measurement_table_byte_limit;measurement_options.snapshot_budget=options.measurement_table_snapshot_budget;measurement_options.validate();
    // Report history survives the end of a visible refusal line.
    enum class ExportFeedback {Error,Refused,CellsRefused};
    MeasurementTableExportJob measurement_job;std::string measurement_error;ExportFeedback measurement_feedback=ExportFeedback::Error;
    int measurement_kind=options.measurement_table_kind;
    ui::RecordRows table_record_rows;
    int table_kind=options.measurement_table_path.empty()?0:1+measurement_kind,table_scope=options.project?1:0;
    bool focus_cell_table_path=false;
    std::string cell_export_destination,measurement_export_destination;
    detail::GuiKeptCounts project_kept_cache;

    CellTableExportJob cell_table_job;std::string cell_table_error;ExportFeedback cell_table_feedback=ExportFeedback::Error;
    OverlayCache overlay_cache;std::optional<DisplaySettings> overlay_display;
    CellParameters cell_parameters=options.cell_parameters?*options.cell_parameters:CellParameters{};cell_parameters.validate();
    std::optional<GridCellSampling> cell_grid=options.cell_grid?std::optional<GridCellSampling>(*options.cell_grid):std::nullopt;
    std::optional<NominalCellRoiSampling> cell_nominal=options.cell_nominal?std::optional<NominalCellRoiSampling>(*options.cell_nominal):std::nullopt;
    if(cell_grid && cell_nominal) throw std::invalid_argument("cell area-grid and nominal sampling are mutually exclusive");
    GridCellParameters grid_cells=options.grid_cell_parameters?*options.grid_cell_parameters:GridCellParameters{};
    if(cell_nominal) cell_nominal->validate();
    if(cell_grid || cell_nominal) {if(cell_grid) cell_grid->validate();grid_cells.validate();if(options.cell_min_nucleus_area_um2 || options.cell_max_nucleus_area_um2) throw std::invalid_argument("grid cells use grid-unit area limits");}
    if(grid_cells.processing!=CellProcessingPolicy::Quantitative && !cell_nominal) throw std::invalid_argument("ImageJ processing requires nominal RGB cell sampling");
    auto cell_min_area=options.cell_min_nucleus_area_um2,cell_max_area=options.cell_max_nucleus_area_um2;
    int cell_feature=static_cast<int>(cell_parameters.feature),cell_connectivity=cell_parameters.connectivity==8?1:0,cell_preview_body=0;
    bool show_cells=true;std::size_t cell_annotations_added=0;
    std::size_t tissue_annotations_added=0;
    std::uint64_t kept_tissue_generation=0,verifying_tissue=0;
    std::uint64_t kept_cell_generation=0,verifying_cells=0;
    TissuePreview tissue_preview;
    bool show_tissue=true;
    int tissue_feature=options.tissue_feature,tissue_connectivity=options.tissue_connectivity==8?1:0;
    std::uint64_t recheck_analysis_generation=0,recheck_analysis_id=0,recheck_tissue_generation=0,recheck_tissue_id=0,recheck_cell_generation=0,recheck_cell_id=0;
    std::uint64_t kept_analysis_generation=0,kept_analysis_id=0,pending_analysis_recheck=0;
    Viewport viewport;
    if(options.start_view) viewport=*options.start_view;
    bool fit = true, drawing = false;
    if(options.start_view) fit=false;
    int tool=0,previous_tool=0;
    Ring draft_points;
    detail::ToolScope canvas_scope;
    struct PlaneSketch {ObjectPlane plane;Ring points;int tool;std::uint64_t hole_id,hole_revision;};
    std::vector<PlaneSketch> plane_sketches;
    std::vector<Ring> vertex_paths;
    std::uint64_t vertex_id=0,vertex_revision=0,hole_id=0,hole_revision=0;
    std::optional<std::pair<std::size_t,std::size_t>> active_vertex;
    bool vertex_dragging=false;
    Point vertex_cursor;
    int boolean_operation=0;bool discard_merge_points=false;
    std::array<std::uint64_t,4> merge_stamp{{~0ULL,~0ULL,~0ULL,~0ULL}};
    bool merge_areas=false,merge_points=false;
    Point anchor;
    auto selection=std::make_unique<ObjectSelection>(*active_document);
    std::uint64_t selected = 0,list_anchor=0;
    using InputConsumer=detail::InputConsumer;using InputEditor=detail::AnalysisInputFeedback::Editor;
    ui::Notifications notifications;
    GuiNoticeObservers notice_observers;
    detail::AnalysisInputFeedback input_feedback;
    inputs.apply_edit=[&](unsigned fields) {
        input_feedback.begin(input_feedback.next_editor);
        for(std::size_t i=0;i<detail::analysis_input_consumers.size();++i) {
            if(!(fields&detail::analysis_input_consumers[i].fields))continue;
            const auto consumer=static_cast<InputConsumer>(i);
            const auto cancelled=[&](std::uint64_t generation,const std::string& state,bool has_work,bool kept=false) {
                if(state!="cancelling" && has_work && !kept) {
                    input_feedback.record(consumer,generation);
                    const char* source=input_feedback.origin==InputEditor::Region?ui::workflow_task_labels[1]:input_feedback.origin==InputEditor::Tissue?ui::workflow_task_labels[2]:input_feedback.origin==InputEditor::Cells?ui::workflow_task_labels[3]:input_feedback.origin==InputEditor::Profile?"Image":"Stain model";
                    notifications.post({ui::Severity::Info,source,detail::analysis_input_consumers[i].cancelled,{},true});
                }
            };
            auto& doc=*active_document;
            switch(consumer) {
                case InputConsumer::Region: {const auto s=analysis.poll(doc,selected);cancelled(s.generation,s.state,analysis.busy() || bool(analysis.result()),!analysis.busy() && (kept_analysis_generation==s.generation || (recheck_analysis_generation==s.generation && recheck_analysis_id)));analysis.cancel();break;}
                case InputConsumer::Tissue: {const auto s=tissue.poll(doc,selected);cancelled(s.generation,s.state,tissue.busy() || bool(tissue.result()),!tissue.busy() && (kept_tissue_generation==s.generation || (recheck_tissue_generation==s.generation && recheck_tissue_id)));tissue.cancel();break;}
                case InputConsumer::Cells: {const auto s=cell_job.poll(doc,selected);cancelled(s.generation,s.state,cell_job.busy() || bool(cell_job.result()),!cell_job.busy() && (kept_cell_generation==s.generation || (recheck_cell_generation==s.generation && recheck_cell_id)));cell_job.cancel();break;}
                case InputConsumer::Training: {const auto s=pixel_classifier.input_training_status(doc);cancelled(s.generation,s.state,s.state=="running");pixel_classifier.invalidate_training();break;}
                case InputConsumer::StainEstimation: {const auto s=stain_estimate.poll(doc,selected);cancelled(s.generation,s.state,stain_estimate.busy() || bool(stain_estimate.result()));stain_estimate.cancel();break;}
                default:break;
            }
        }
        if(fields&(detail::AnalysisInputs::Kind|detail::AnalysisInputs::Vectors|detail::AnalysisInputs::White))stain_estimate_applied=false;
    };
    bool marquee=false,marquee_additive=false;
    Point marquee_anchor;
    std::string batch_class;
    const char* project_path=forms.destination.text();
    std::string project_error;
    detail::EntryEditor entry_editor;
    auto image_class_editor=std::make_unique<detail::ClassEditor>();
    auto project_class_editor=std::make_unique<detail::ClassEditor>(true);
    ProjectAutosave autosave(options.autosave_snapshot_budget);
    RecoveryRetention retention;
    retention.enabled=options.recovery_retention_enabled; retention.keep_latest=options.recovery_keep_latest;
    retention.max_age_days=options.recovery_max_age_days; autosave.set_retention(retention);
    ProjectSaveJob save_job(options.save_snapshot_budget);
    bool save_result_pending=false,close_wait_only=false;
    AutosaveStats autosave_stats;
    double autosave_interval=options.autosave_interval_seconds;
    auto next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
    const char* recovery_path=paths.recovery.text();const char* recovery_base_path=paths.recovery_primary.text();
    auto copy_recovery_path=[](char* buffer,const std::string& path) {
        if(path.size()>32768 || path.find('\0')!=std::string::npos) throw ui::NoticeException<std::invalid_argument>("invalid recovery path");
        std::memcpy(buffer,path.c_str(),path.size()+1);
    };

    std::string autosave_ui_error;
    std::future<std::vector<RecoveryInfo>> recovery_scan;
    std::string recovery_scan_base;
    std::uint64_t recovery_scan_generation=0;
    detail::RecordRemovalDialog record_removal;
    detail::OutputGate output_gate;
    output_gate.notifications(&notifications);raw_pixel.notifications(&output_gate);
    std::uint64_t workspace_epoch=0,recovery_scan_epoch=0;
    std::uint64_t recovery_catalog_revision=0,recovery_scan_revision=0;
    std::vector<RecoveryInfo> recovery_candidates;
    bool reveal_recoveries=false;
    if(options.project && !frame_limit && !startup_requested) {
        auto base=options.project->path(); recovery_scan_base=base; recovery_scan_generation=options.project->generation();
        recovery_scan=std::async(std::launch::async,[base]{return Project::find_recoveries(base);});
    }
    std::shared_ptr<Project> staged_project;
    bool staged_is_recovery=true;
    struct LoadedProject { std::shared_ptr<Project> project; double ms=0; bool recovery=false,qupath_import=false; };
    auto load_project=[](const std::string& path,bool recovery,const std::string& base_path) {
        const auto start=std::chrono::steady_clock::now();
        auto candidate=recovery?Project::recover(path,base_path):Project::open(path);
        if(candidate->active_image()) candidate->document(candidate->active_image())->query_region({0,0,0,0});
        return LoadedProject{std::move(candidate),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),recovery};
    };
    std::future<LoadedProject> project_load;
    std::future<std::shared_ptr<const QuPathProjectPreview>> qupath_review;
    std::shared_ptr<const QuPathProjectPreview> qupath_preview;
    auto qupath_cancel=std::make_shared<std::atomic<bool>>(false);
    struct ImportCancellation {std::shared_ptr<std::atomic<bool>> flag;~ImportCancellation(){flag->store(true);}} qupath_scope{qupath_cancel};
    std::string qupath_review_input;bool qupath_losses=false;
    std::string queued_raw_path;
    auto review_qupath=[&](const std::string& path) {
        qupath_cancel->store(false);qupath_review_input=path;
        qupath_review=std::async(std::launch::async,[path,cancel=qupath_cancel]{return preview_qupath_project(path,{},"",cancel.get());});
    };
    auto startup_progress=std::make_shared<Project::LoadProgress>();
    bool startup_active=startup_requested,startup_pending=startup_requested,startup_succeeded=false,startup_cancelled=false;
    std::size_t startup_frames=0,startup_finish_frame=0;
    double startup_ms=0;
    bool project_load_cancelled=false;
    bool loading_recovery=false;
    std::uint64_t project_replacements=0;
    double project_load_ms=0;
    double recovery_load_ms=0;
    const char* project_open_path=forms.open_path.text();
    ImVec2 cancel_load_offset{},cancel_load_size{};
    struct MaintenanceResult {std::optional<RecoveryInfo> reviewed; std::string deleted,error;};
    std::future<MaintenanceResult> maintenance_job;
    std::uint64_t maintenance_epoch=0,checkpoints_deleted=0;
    std::string maintenance_error;
    std::optional<RecoveryInfo> reviewed_checkpoint;
    bool maintenance_popup_requested=false,maintenance_dialog_active=false;
    ImVec2 confirm_checkpoint_offset{},confirm_checkpoint_size{},cancel_checkpoint_offset{},cancel_checkpoint_size{};
    bool recovery_popup_requested=false,recovery_dialog_active=false,restore_pending=false,restore_after_save=false,close_recovery_popup=false;
    bool close_popup_requested=false,close_dialog_active=false,exit_requested=false,close_keep_started=false;
    bool close_edit_destination=false,close_edit_recovery=false;
    bool remove_popup_requested=false,remove_dialog_active=false;
    std::uint64_t pending_remove=0,images_removed=0;
    ImVec2 confirm_remove_offset{},confirm_remove_size{},cancel_remove_offset{},cancel_remove_size{};
    int close_action=0; // 1 saved, 2 keep recovery, 3 discard.
    ImVec2 discard_recovery_offset{},discard_recovery_size{},keep_close_offset{},keep_close_size{},cancel_close_offset{},cancel_close_size{},discard_close_offset{},discard_close_size{};
    ImVec2 save_close_offset{},save_close_size{},save_recovery_offset{},save_recovery_size{},cancel_recovery_offset{},cancel_recovery_size{};
    const char* project_image_path=forms.image.text();
    auto& project_image_name=text_buffers->project_image_name;
    auto& project_filter=text_buffers->project_filter;
    std::vector<std::string> reader_names{"auto","ppm","openslide","tiff","ome-zarr"};
    constexpr char native_reader_labels[]="Auto\0PPM\0OpenSlide\0TIFF quantitative\0OME-Zarr\0";
    std::string reader_labels(native_reader_labels,sizeof(native_reader_labels)-1);
    // Preserve the five legacy positions; configured native providers append.
    for(const auto& reader:detail::reader_catalog())if(!detail::known_reader_id(reader.provider_id)) {
        reader_names.push_back(reader.provider_id);reader_labels+=reader.display_name;reader_labels+='\0';
    }
    reader_labels+='\0';int image_reader=0;
    for(std::size_t i=0;i<reader_names.size();++i) if(options.image_reader==reader_names[i]) image_reader=int(i);
    std::vector<ProjectImage> project_images;
    ui::ProjectBrowser project_browser;
    project_browser.group_key=workspace.projects.group;
    bool project_catalog_dirty=true,project_filter_dirty=true;
    std::uint64_t pending_image=0;
    bool refresh_active_image=false;
    ImVec2 confirm_export_offset{},confirm_export_size{},cancel_export_offset{},cancel_export_size{};
    std::optional<detail::PendingGuiExport> pending_path_export;ui::PathField* pending_path_field=nullptr;
    bool interchange_pixels=options.interchange_assume_pixels,interchange_losses=options.interchange_allow_losses,interchange_selected=false,csv_intensity=false;
    bool interchange_nested=options.interchange_nested_hierarchy;
    bool qupath_ellipse_precision=options.qupath_ellipse_precision;
    std::string interchange_status,clipboard_error;
    const auto feedback=[](const char* key,auto... values) {
        const auto* format=ui::i18n::text(key);const auto size=std::snprintf(nullptr,0,format,values...);
        if(size<0)throw ui::NoticeException<std::runtime_error>("Cannot format workflow feedback");
        std::vector<char> buffer(std::size_t(size)+1);std::snprintf(buffer.data(),buffer.size(),format,values...);return std::string(buffer.data());
    };
    bool clipboard_descendants=false;
    std::size_t clipboard_copies=0,clipboard_pastes=0,clipboard_copied_objects=0,clipboard_pasted_objects=0;
    std::array<Rect,3> clipboard_bounds{};
    Rect image_stains_bounds{};
    detail::CalibrationEditor calibration_editor;
    bool image_info_requested=false,image_info_active=false,image_info_analysis=false,image_info_stains=false;
    const Document* image_info_document=nullptr;std::uint64_t image_info_epoch=0;char image_info_search[257]{};
    bool object_info_requested=false,object_info_active=false;
    const Document* object_info_document=nullptr;std::uint64_t object_info_epoch=0;
    std::optional<Annotation> object_info_snapshot;char object_info_search[257]{};
    std::optional<StainTransform> image_info_transform,active_inputs_transform;
    std::optional<detail::SamplingInputKey> active_inputs_key;std::string active_inputs_error;
    detail::Draft<detail::GeometryDraft> geometry_draft;
    detail::Draft<detail::AppearanceDraft> appearance_draft;
    detail::ParentChoices parent_choices;
    detail::DraftTextOwner editor_text;
    bool geometry_discarded=false,appearance_discarded=false;
    std::string object_baseline_name;
    std::uint64_t editor_id=0,editor_revision=0;
    auto& draft_name=text_buffers->draft_name;
    auto& draft_class=text_buffers->draft_class;
    auto& draft_use_object_color=appearance_draft.value.color;auto& draft_use_class_probability=appearance_draft.value.probability;
    auto& draft_object_color=appearance_draft.value.rgb;auto& draft_class_probability=appearance_draft.value.reported;
    std::array<Rect,3> object_state_bounds{};
    auto& draft_roi=geometry_draft.value.bounds;auto& draft_parent=geometry_draft.value.parent;
    std::array<std::optional<double>,4> geometry_inputs;std::optional<double> probability_input;
    const auto drop_editor=[&] {
        geometry_discarded=editor_id && geometry_draft.dirty();appearance_discarded=editor_id && appearance_draft.dirty();
        detail::retire_draft_text(editor_text);
        geometry_draft.reset({});appearance_draft.reset({});editor_id=0;
    };
    std::uint64_t calibration_revision=std::numeric_limits<std::uint64_t>::max();
    std::vector<std::uint64_t> list_ids;
    std::uint64_t list_revision=std::numeric_limits<std::uint64_t>::max();
    std::uint64_t filtered_selection_revision=~std::uint64_t(0),filtered_document_revision=~std::uint64_t(0);
    std::size_t selection_not_shown=0;
    bool list_annotations_only=false;auto& objects_visible=workspace.objects_visible;auto& annotations_only=workspace.annotations_only;
    int frames = 0;
    std::uint64_t settle_frames=0;
    std::array<int,detail::gui_input_target_count> target_seen;
    std::optional<ImVec2> replay_mouse; // Hold synthetic pointer state between replay events.
    ui::ReplayControlPresses replay_controls;
    std::set<unsigned int> replay_wheels;std::uint64_t ignored_physical_wheels=0;
    target_seen.fill(-2);
    auto mark_target=[&](GuiInputTarget target) {
        const auto* modal=ImGui::GetTopMostPopupModal();
        target_seen[static_cast<std::size_t>(target)]=!ImGui::GetCurrentWindow()->SkipItems && (!modal || modal==ImGui::GetCurrentWindow()->RootWindow) && ImGui::IsItemVisible() &&
            !ImGui::IsWindowAppearing() && !(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)?frames:-2;
    };
    std::size_t visible_count=0;
    std::vector<double> frame_times,frame_wall_times;
    TilePlan plan;
    GuiThumbnail overview(options.overview_source_pixel_limit);
    GuiProjectThumbnails project_thumbnails;
    ImVec2 overview_more_offset{},overview_more_size{},overview_less_offset{},overview_less_size{};
    bool canvas_visible=true;
    ImVec2 layout_reset_offset{},layout_reset_size{},save_project_offset{},save_project_size{};
    Viewport overview_view;
    DisplaySettings display=options.display;
    std::optional<ProjectView> restore_view;
    if(options.project && active_image && !options.start_view) {
        restore_view=options.project->view(active_image);
        if(restore_view) { display=restore_view->display; fit=false; }
    }
    ImGuiID previous_canvas_window=0;
    ImVec2 previous_canvas{}, previous_canvas_size{}, overview_offset{}, overview_size{250,150},list_offset{},list_size{};
    std::optional<Point> overview_center;
    int last_width=0,last_height=0;
    int rendered_window_width=0,rendered_window_height=0;
    std::uint64_t suspended_iterations=0;
    double suspended_work_ms=0,suspended_wait_ms=0;
    bool was_suspended=false;
    std::optional<std::tuple<double,double,double,float,float,float>> plan_inputs;
    std::optional<std::tuple<double,double,double,float,float,int,double,double,double>> published_view_inputs;
    auto view_inputs=[&](float width,float height) { return std::make_tuple(viewport.origin.x,viewport.origin.y,viewport.zoom,width,height,static_cast<int>(display.mode),display.black,display.white,display.gamma); };
    struct ImageView {
        Viewport viewport; DisplaySettings display;
        std::unique_ptr<ObjectSelection> selection;
        std::uint64_t list_anchor=0;
    };
    std::map<std::uint64_t,ImageView> image_views;
    std::uint64_t image_switches=0,previous_uploads=0,previous_generations=0;
    std::size_t previous_peak_bytes=0;
    double max_image_switch_ms=0;
    std::future<double> retired_workspace;
    std::uint64_t retired_workspaces=0;
    double max_retirement_ms=0;
    ImVec2 project_list_offset{},project_list_size{};
    ImVec2 display_reset_offset{},display_reset_size{};
    auto poll_maintenance=[&](bool finishing=false) {
        if(!maintenance_job.valid() || maintenance_job.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        if(!finishing && (close_dialog_active || remove_dialog_active || recovery_dialog_active || restore_pending)) return;
        try {
            auto result=maintenance_job.get();
            if(!result.deleted.empty()) ++checkpoints_deleted;
            if(maintenance_epoch!=workspace_epoch) return;
            {maintenance_error=std::move(result.error);if(!maintenance_error.empty())notifications.post({ui::Severity::Error,"Project",maintenance_error,{},false});}
            if(result.reviewed && !finishing) {
                reviewed_checkpoint=std::move(result.reviewed); paths.recovery.set(reviewed_checkpoint->path);
                maintenance_popup_requested=maintenance_dialog_active=true;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
            }
            if(!result.deleted.empty()) {
                ++recovery_catalog_revision;
                recovery_candidates.erase(std::remove_if(recovery_candidates.begin(),recovery_candidates.end(),[&](const auto& item){return item.path==result.deleted;}),recovery_candidates.end());
                if(paths.recovery.state().resolved==result.deleted) {paths.recovery.set("");paths.recovery_primary.set("");}
                next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
            }
        } catch(const std::exception& e) {if(maintenance_epoch==workspace_epoch) {maintenance_error=e.what();if(!maintenance_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
    };
    auto reconcile_save=[&] {
        const auto stats=save_job.poll();
        if(save_result_pending && !save_job.busy()) {
            save_result_pending=false;
            if(stats.state=="saved" && stats.applied) {
                notifications.post({ui::Severity::Success,"Project","The operation completed.",options.project->path(),true});
                project_error.clear();
                forms.destination.set(options.project->path());
                if(project_form==ProjectForm::Save) project_form=ProjectForm::None;
                if(!options.project->dirty()) {
                    // Clear only this controller's checkpoint and only when
                    // the live canonical state actually reached the primary.
                    try {
                        if(restore_after_save && staged_project && same_recovery_file(autosave.stats().path,staged_project->recovery_source())) autosave.cancel();
                        else autosave.clear();
                    } catch(const std::exception& e) {{autosave_ui_error=e.what();if(!autosave_ui_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
                }
            } else {
                {project_error=stats.error;if(!project_error.empty())notifications.post({ui::Severity::Error,"Project",project_error,{},false});} restore_after_save=false;
                if(close_action==1) {close_action=0; close_wait_only=false;}
            }
        }
    };
    auto select_project_form=[&](ProjectForm next) {forms.saved_destination=options.project?options.project->path():std::string{};forms.select(next);};
    auto* live=options.viewer_owner;
    auto live_context=[&] {
        if(startup_requested && !startup_active && !startup_succeeded && !startup_cancelled && !restore_pending && !staged_project && !project_load.valid() && !project_error.empty()) throw std::runtime_error(project_error);
        if(startup_requested && !startup_active && !startup_succeeded && startup_cancelled) exit_requested=true;
        detail::ViewerContext c;c.opening=startup_requested && !startup_succeeded;c.project=options.project;c.catalog=&project_images;c.epoch=startup_requested && workspace_epoch?workspace_epoch-1:workspace_epoch;c.frame=frames;c.active_image=active_image;
        c.blocked=c.opening || startup_active || close_dialog_active || recovery_dialog_active || restore_pending || remove_dialog_active || maintenance_dialog_active;
        c.closing=exit_requested || glfwWindowShouldClose(session.window);
        c.activate=[&](std::uint64_t id){pending_image=id;};
        c.refresh_source=[&](std::uint64_t id){if(id==active_image) {mean.invalidate();pending_image=id;refresh_active_image=true;}};
        c.select=[&](const std::vector<std::uint64_t>& ids){selection->replace(ids);selected=selection->primary();};
        c.save=[&](const std::string& path){if(save_job.busy()) return false;const auto started=save_job.request(options.project,path);if(started) {save_result_pending=true;forms.destination.set(path);}return started;};
        c.save_stats=[&]{reconcile_save();return save_job.stats();};c.cancel_save=[&]{save_job.cancel();};
        c.close=[&](bool discard){if(discard) {if(startup_active) startup_cancelled=true;startup_progress->cancelled.store(true);project_load_cancelled=true;autosave.cancel();exit_requested=true;}else glfwSetWindowShouldClose(session.window,true);};return c;
    };
    auto within_frames=[&] {return !frame_limit || replay.frame()<static_cast<std::uint64_t>(frame_limit) ||
        (startup_requested && (startup_active || replay.frame()<startup_finish_frame+frame_limit));};
    auto editing_is_blocked=[&] {return startup_active || close_dialog_active || recovery_dialog_active || restore_pending || remove_dialog_active || maintenance_dialog_active;};
    auto service_project=[&](bool save_project_requested,bool autosave_requested,bool editing_blocked,bool publish_view) {
        if(options.project) {
            bool recovery_operation=false;
            try {
                auto inputs=view_inputs(previous_canvas_size.x,previous_canvas_size.y);
                if(publish_view && canvas_visible && !restore_view && active_image && (!published_view_inputs || *published_view_inputs!=inputs)) {
                    options.project->set_view(active_image,{viewport.screen_to_image({previous_canvas_size.x/2,previous_canvas_size.y/2}),viewport.zoom,display});
                    published_view_inputs=inputs;
                }
                if(save_project_requested) {
                    if(save_job.request(options.project,forms.destination.state().resolved)) {save_result_pending=true; project_error.clear();}
                }
                reconcile_save();
                if(restore_after_save && !save_job.busy()) {
                    if(options.project->dirty()) {
                        save_result_pending=save_job.request(options.project,forms.destination.state().resolved);
                    } else if(staged_project && !staged_is_recovery && same_recovery_file(options.project->path(),staged_project->path())) {
                        // Saving over the selected source invalidates the staged
                        // generation. Reopen the successful save on the worker.
                        project_load=std::async(std::launch::async,load_project,staged_project->path(),false,std::string{}); project_load_cancelled=false; loading_recovery=false;
                        staged_project.reset();
                        close_recovery_popup=true; restore_after_save=false;
                    } else { restore_pending=true; restore_after_save=false; }
                }
                if(close_action==1 && !save_job.busy() && options.project->dirty()) {
                    if(close_wait_only) {close_action=0; close_wait_only=false;}
                    else save_result_pending=save_job.request(options.project,forms.destination.state().resolved);
                }
                if((close_action==1 && !save_job.busy()) || close_action==3) {
                    recovery_operation=true;
                    autosave_stats=autosave.poll();
                    if(!autosave.busy()) {
                        if(close_action==3 && !autosave_stats.error.empty()) { autosave_ui_error=autosave_stats.error; close_action=0; }
                        else {
                            exit_requested=true;
                        }
                    }
                } else if(close_action==2 && !autosave.busy()) {
                    recovery_operation=true;
                    if(close_keep_started && autosave_stats.state=="error") { autosave_ui_error=autosave_stats.error; close_action=0; }
                    else {
                        const bool started=autosave.request(*options.project,paths.autosave.state().resolved); close_keep_started=true;
                        if(started || !options.project->dirty() || autosave.stats().state=="ready") autosave_ui_error.clear();
                        if(!started && (!options.project->dirty() || autosave.stats().state=="ready")) exit_requested=true;
                    }
                } else if(!editing_blocked && !maintenance_job.valid() && (autosave_requested || (autosave_interval>0 && std::chrono::steady_clock::now()>=next_autosave))) {
                    recovery_operation=true;
                    autosave.request(*options.project,paths.autosave.state().resolved); autosave_ui_error.clear();
                    next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
                }
            } catch(const std::exception& e) {
                if(recovery_operation) {autosave_ui_error=e.what();if(!autosave_ui_error.empty())notifications.post(ui::exception_notice("Project",e,{}));} else {project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}
                if(save_project_requested) { restore_after_save=false; close_action=0; }
                if(close_action==2 || close_action==3) close_action=0;
                next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(std::max(1.,autosave_interval));
            }
        }
    };
        using ExportOp=detail::GuiExportOperation;
        auto export_busy=[&]{return cell_table_job.busy() || measurement_job.busy() || fingerprint_job.busy();};
        auto export_protection=[&] {
            std::vector<std::string> recovery{paths.autosave.state().resolved,autosave.stats().path};
            if(options.project) recovery.push_back(autosave.default_path(*options.project));
            return detail::export_protected_paths(options.project.get(),*active_document,workspace.path(),recovery);
        };
        auto capture_export_owner=[&](bool selected_only,bool project_scope,bool capture_members=true) {
            const auto& document=*active_document;
            detail::ExportOwnerStamp stamp;stamp.project=options.project.get();stamp.document=&document;stamp.server=document.server().get();
            stamp.epoch=workspace_epoch;stamp.image=active_image;stamp.revision=document.revision();stamp.calibration=document.calibration_revision();
            stamp.selection=selected_only?selection->revision():0;stamp.selected_only=selected_only;stamp.project_scope=project_scope;
            if(const auto source=document.server()->reader_source())stamp.source_generation=source->generation;
            if(options.project) {stamp.project_generation=options.project->generation();stamp.project_revision=options.project->revision_;}
            if(capture_members && project_scope && options.project) for(const auto& image:options.project->images()) {
                const auto member=options.project->document(image.id);
                stamp.members.emplace_back(image.id,member.get(),member->revision(),member->calibration_revision(),member->server().get());
            }
            return stamp;
        };
        output_gate.bind(capture_export_owner,export_protection);
        auto export_owner=[&](const detail::PendingGuiExport& intent){return capture_export_owner(intent.selected_only,intent.project_scope());};
        auto export_failure=[&](ExportOp operation,const std::string& message,bool announce=true,const std::exception* exception=nullptr,const char* source=nullptr) {
            if(!source)source=operation==ExportOp::GeoJson || operation==ExportOp::ObjectCsv?"Annotations":ui::workflow_task_labels[4];
            if(announce)notifications.post(exception?ui::exception_notice(source,*exception):ui::Notice{ui::Severity::Error,source,message,{}});
            if(operation==ExportOp::GeoJson || operation==ExportOp::ObjectCsv)interchange_status=message;
            else if(operation==ExportOp::ImageMeasurements || operation==ExportOp::ProjectMeasurements){measurement_error=message;measurement_feedback=ExportFeedback::Error;}
            else {cell_table_error=message;cell_table_feedback=ExportFeedback::Error;}
        };
        using PreparedExportRequest=std::variant<std::monostate,CellTableRequest,MeasurementTableRequest>;
        auto prepare_export_request=[&](const detail::PendingGuiExport& intent)->PreparedExportRequest {
            const auto& document=*active_document;
            if(intent.operation==ExportOp::SavedCells || intent.operation==ExportOp::ImageCells) {
                return CellTableRequest(document,intent.ids,intent.cell_options);
            }
            if(intent.operation==ExportOp::ProjectCells) {return CellTableRequest(*options.project,{},intent.cell_options);}
            if(intent.operation==ExportOp::ImageMeasurements) {return MeasurementTableRequest(document,intent.measurement_kind,{},intent.measurement_options);}
            if(intent.operation==ExportOp::ProjectMeasurements) {return MeasurementTableRequest(*options.project,intent.measurement_kind,{},intent.measurement_options);}
            return std::monostate{};
        };
        auto dispatch_export=[&](detail::PendingGuiExport intent,ExportWritePolicy policy,PreparedExportRequest prepared={}) {
            const auto& document=*active_document;
            if(export_busy())throw ui::NoticeException<std::runtime_error>("Wait for the current export to finish.");
            if(!capture_export_owner(intent.selected_only,intent.project_scope(),false).same_context(intent.owner))throw ui::NoticeException<std::runtime_error>("Export context changed. Cancel and export again.");
            const auto current_protection=export_protection();intent.protected_paths.insert(intent.protected_paths.end(),current_protection.begin(),current_protection.end());
            std::sort(intent.protected_paths.begin(),intent.protected_paths.end());intent.protected_paths.erase(std::unique(intent.protected_paths.begin(),intent.protected_paths.end()),intent.protected_paths.end());
            detail::guard_export_output(intent.path,intent.protected_paths);
            if(std::holds_alternative<std::monostate>(prepared))prepared=prepare_export_request(intent);
            switch(intent.operation) {
            case ExportOp::GeoJson: {
                const detail::AnnotationFileExportRequest request{"geojson",intent.path,
                    detail::AnnotationGeoJsonExportOptions{std::cref(intent.ids),intent.allow_losses,intent.nested},
                    policy,std::cref(intent.protected_paths)};
                const auto report=detail::export_annotations(document,request);
                interchange_status=feedback("Exported %zu objects to %s",std::size_t(report.features),intent.path.c_str());
                for(const auto& issue:report.issues)interchange_status+="\n"+issue.message+" ("+std::to_string(issue.count)+")";break;
            }
            case ExportOp::ObjectCsv: {
                const detail::AnnotationFileExportRequest request{"measurements-csv",intent.path,
                    detail::AnnotationMeasurementsCsvOptions{std::cref(intent.ids),intent.intensity},
                    policy,std::cref(intent.protected_paths)};
                detail::export_annotations(document,request);
                interchange_status=feedback("Exported measurements to %s",intent.path.c_str());break;
            }
            case ExportOp::SavedCells:case ExportOp::ImageCells:
                if(!cell_table_job.request_with_policy(std::get<CellTableRequest>(prepared),intent.path,policy,intent.protected_paths))throw ui::NoticeException<std::runtime_error>("Cell export is busy.");
                cell_export_destination=intent.path;cell_table_error.clear();cell_table_feedback=ExportFeedback::Error;break;
            case ExportOp::ProjectCells:
                if(!cell_table_job.request_with_policy(std::get<CellTableRequest>(prepared),intent.path,policy,intent.protected_paths))throw ui::NoticeException<std::runtime_error>("Cell export is busy.");
                cell_export_destination=intent.path;cell_table_error.clear();cell_table_feedback=ExportFeedback::Error;break;
            case ExportOp::ImageMeasurements:
                if(!measurement_job.request_with_policy(std::get<MeasurementTableRequest>(prepared),intent.path,policy,intent.protected_paths))throw ui::NoticeException<std::runtime_error>("Measurement export is busy.");
                measurement_export_destination=intent.path;measurement_error.clear();measurement_feedback=ExportFeedback::Error;break;
            case ExportOp::ProjectMeasurements:
                if(!measurement_job.request_with_policy(std::get<MeasurementTableRequest>(prepared),intent.path,policy,intent.protected_paths))throw ui::NoticeException<std::runtime_error>("Measurement export is busy.");
                measurement_export_destination=intent.path;measurement_error.clear();measurement_feedback=ExportFeedback::Error;break;
            }
        };
        auto submit_export=[&](detail::PendingGuiExport intent) {
            const auto& document=*active_document;
            if(!(export_owner(intent)==intent.owner))throw ui::NoticeException<std::runtime_error>("Export context changed. Cancel and export again.");
            const auto operation=intent.operation;
            intent.path=detail::export_path(intent.path);intent.protected_paths=export_protection();
            detail::guard_output_intent(intent.path,intent.protected_paths);
            auto prepared=prepare_export_request(intent);
            const auto rows=std::visit([&](const auto& request)->std::uint64_t {if constexpr(std::is_same_v<std::decay_t<decltype(request)>,std::monostate>)return intent.selected_only?intent.ids.size():document.object_count();else return request.row_count();},prepared);
            const bool objects=operation==ExportOp::GeoJson || operation==ExportOp::ObjectCsv;
            const char* kind=operation==ExportOp::GeoJson?"GeoJSON":operation==ExportOp::ObjectCsv?"Object measurements CSV":operation==ExportOp::ImageMeasurements || operation==ExportOp::ProjectMeasurements?(intent.measurement_kind==MeasurementTableKind::Roi?"ROI measurements CSV":"Tissue measurements CSV"):"Cell table CSV";
            intent.scope=feedback(objects?"Scope: %s / %llu objects":"Scope: %s / %llu rows",ui::i18n::text(intent.project_scope()?"Project":intent.selected_only?"Selected objects":"Current image"),static_cast<unsigned long long>(rows));
            detail::OutputIntent output{intent.path,intent.scope,intent.protected_paths,intent.owner,false,
                [&,intent,prepared=std::move(prepared)](ExportWritePolicy policy) mutable {
                    dispatch_export(intent,policy,prepared);
                    if(intent.operation==ExportOp::GeoJson || intent.operation==ExportOp::ObjectCsv)output_gate.succeeded(intent.path);
                    else if(intent.operation==ExportOp::ImageMeasurements || intent.operation==ExportOp::ProjectMeasurements)
                        output_gate.watch(intent.path,[&]()->std::optional<bool>{if(measurement_job.busy())return {};return measurement_job.poll().state=="succeeded";},[&]{const auto status=measurement_job.poll();return status.state=="failed"?status.error:std::string{};});
                    else output_gate.watch(intent.path,[&]()->std::optional<bool>{if(cell_table_job.busy())return {};return cell_table_job.poll().state=="succeeded";},[&]{const auto status=cell_table_job.poll();return status.state=="failed"?status.error:std::string{};});
                },[export_failure,operation](const std::string& message){export_failure(operation,message,false);}};
            output.kind=kind;output.source=intent.source;output_gate.submit(std::move(output));
        };

    bool finite_run_finished=false;
    while (!exit_requested) {
        if(ui::command_trace && ui::command_trace->end_session)break;
        ui::CommandFrame command_inputs;
        const bool settling=!within_frames();
        if(settling) {
            // An accepted save-and-replace/discard transition must reconcile
            // its new authority before a finite embedding returns.
            if(restore_after_save || restore_pending) settlement.check(std::chrono::steady_clock::now(),"workspace transition");
            else {if(!options.settle_tiles) {finite_run_finished=true;break;}settlement.check(std::chrono::steady_clock::now(),"tile uploads");}
        }
        auto frame_start=std::chrono::steady_clock::now();
        detail::GuiFrameSample profile;profile.frame=frames;profile.startup=startup_active;auto profile_mark=frame_start;
        auto profile_lap=[&] {if(!profiler.enabled()) return 0.;const auto now=std::chrono::steady_clock::now();const auto ms=std::chrono::duration<double,std::milli>(now-profile_mark).count();profile_mark=now;return ms;};
        if(retired_workspace.valid() && retired_workspace.wait_for(std::chrono::seconds(0))==std::future_status::ready)
            max_retirement_ms=std::max(max_retirement_ms,retired_workspace.get());
        poll_maintenance();
        batch.poll();
        auto previous_autosave_state=autosave_stats.state;
        autosave_stats=autosave.poll();
        if(autosave_stats.state=="error" && previous_autosave_state!="error")
            next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(std::max(5.,autosave_interval));
        if(qupath_review.valid() && qupath_review.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {auto reviewed=qupath_review.get();if(qupath_review_input==forms.open_path.state().resolved) {qupath_preview=std::move(reviewed);qupath_losses=false;forms.qupath_destination.set(std::filesystem::u8path(qupath_review_input).replace_extension(".qpxproj").u8string());project_error.clear();}}
            catch(const std::exception& e) {{project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
        }
        if(project_load.valid() && !close_dialog_active && !remove_dialog_active && project_load.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {
                auto loaded=project_load.get();
                if(startup_active) startup_ms=loaded.ms;
                if(loaded.recovery) recovery_load_ms=loaded.ms; else project_load_ms=loaded.ms;
                if(!project_load_cancelled) {
                    if(loaded.qupath_import) {forms.open_path.set(loaded.project->path());qupath_preview.reset();qupath_losses=false;}
                    staged_project=std::move(loaded.project); staged_is_recovery=loaded.recovery;
                    drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
                    if(options.project->dirty() && !startup_active) recovery_popup_requested=recovery_dialog_active=true;
                    else { autosave.cancel(); restore_pending=true; }
                    project_error.clear();notifications.post({ui::Severity::Success,"Project","The operation completed.",staged_project->path(),true});
                }
            } catch(const std::exception& e) { if(!project_load_cancelled) {project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));} }
            if(startup_active && !restore_pending && !staged_project) {
                startup_active=false; startup_cancelled=project_load_cancelled; startup_finish_frame=replay.frame();
            }
        }
        if(recovery_scan.valid() && recovery_scan.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            const bool current=options.project && recovery_scan_epoch==workspace_epoch && recovery_scan_revision==recovery_catalog_revision && recovery_scan_base==options.project->path() && recovery_scan_generation==options.project->generation();
            try {
                auto candidates=recovery_scan.get();
                if(current) {
                    recovery_candidates=std::move(candidates);
                    reveal_recoveries=!recovery_candidates.empty();
                    if(!recovery_candidates.empty() && !*recovery_path) {
                        paths.recovery.set(recovery_candidates.front().path);
                        if(!*recovery_base_path) paths.recovery_primary.set(recovery_candidates.front().associated_base_path);
                    }
                }
            } catch(const std::exception& e) { if(current) {autosave_ui_error=e.what();if(!autosave_ui_error.empty())notifications.post(ui::exception_notice("Project",e,{}));} }
        }
        if(live) live->poll(live_context());
        if(glfwWindowShouldClose(session.window) && !restore_pending && !pending_remove) {
            if(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED)) glfwRestoreWindow(session.window);
            project_load_cancelled=true; startup_progress->cancelled.store(true);
            if(startup_active) startup_cancelled=true;
            glfwSetWindowShouldClose(session.window,false);
            if(save_job.busy()) { close_popup_requested=close_dialog_active=true; close_action=1; close_wait_only=true; }
            else if(options.project && options.project->dirty() && !startup_active) { close_popup_requested=close_dialog_active=true; }
            else { autosave.clear(); if(!autosave.busy()) break; close_action=1; close_popup_requested=close_dialog_active=true; }
            drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
        }
        if(restore_pending && !autosave.busy() && !save_job.busy() && !retired_workspace.valid()) {
            try {
                auto owner=staged_project->active_image()?staged_project->document(staged_project->active_image()):empty_document;
                auto next_selection=std::make_unique<ObjectSelection>(*owner);
                auto next_tiles=std::make_unique<GuiTiles>(owner->server());
                const bool native_retirement=options.project->can_retire_on_worker();
                mean.invalidate();
                decltype(image_views) previous_views; previous_views.swap(image_views);
                auto previous_selection=std::move(selection);
                options.project->adopt_workspace(*staged_project);
                ++workspace_epoch; reviewed_checkpoint.reset(); maintenance_error.clear();
                previous_uploads+=tiles->uploads(); previous_generations+=tiles->generations(); previous_peak_bytes=std::max(previous_peak_bytes,tiles->peak_bytes());
                tiles=std::move(next_tiles); selection=std::move(next_selection); active_owner=std::move(owner); active_document=active_owner.get();
                overlay_cache.clear();overlay_display.reset();
                active_image=options.project->active_image(); pending_image=pending_remove=0; refresh_active_image=false;
                restore_view=active_image?options.project->view(active_image):std::nullopt; viewport={}; display=restore_view?restore_view->display:options.display; fit=!restore_view;
                drop_editor();drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); vertex_paths.clear(); vertex_id=hole_id=selected=list_anchor=0;
                tool=previous_tool=0; batch_class.clear();  interchange_status.clear(); input_feedback={};
                list_ids.clear(); list_revision=calibration_revision=std::numeric_limits<std::uint64_t>::max();
                plan={}; overview.reset(); plan_inputs.reset(); published_view_inputs.reset(); overview_center.reset();
                project_catalog_dirty=true; forms.destination.set(options.project->path()); project_error.clear();select_project_form(ProjectForm::None);
                recovery_candidates.clear();
                if(!same_recovery_file(autosave.stats().path,options.project->recovery_source())) autosave.clear();
                if(native_retirement) {
                    try {
                        // One retirement, no backlog. The candidate Project is
                        // native-created and all old document control blocks
                        // came from native factories. GPU objects stay here.
                        retired_workspace=std::async(std::launch::async,[old=std::move(staged_project),views=std::move(previous_views),selected=std::move(previous_selection)]() mutable {
                            const auto start=std::chrono::steady_clock::now();
                            views.clear(); selected.reset(); old.reset();
                            return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                        });
                        ++retired_workspaces;
                    } catch(const std::exception&) {
                        // Thread-launch failure destroys the native captures on
                        // this owner thread. Replacement itself already succeeded.
                    }
                } else {
                    // Externally supplied documents may have Python/GIL deleters.
                    previous_views.clear(); previous_selection.reset(); staged_project.reset();
                }
                if(startup_active) {
                    startup_active=false; startup_succeeded=true; startup_finish_frame=replay.frame();
                    if(!frame_limit && !recovery_scan.valid()) {
                        auto base=options.project->path(); recovery_scan_base=base; recovery_scan_generation=options.project->generation();
                        recovery_scan_epoch=workspace_epoch; recovery_scan_revision=recovery_catalog_revision;
                        recovery_scan=std::async(std::launch::async,[base]{return Project::find_recoveries(base);});
                    }
                }
                ++project_replacements; close_recovery_popup=recovery_dialog_active;
                project_browser=ui::ProjectBrowser{};project_browser.group_key=workspace.projects.group;
                project_browser.selected=active_image;project_settings_visible=rename_project_image=focus_project_rename=false;inspected_image=0;relink_image_id=remove_image_id=0;
                project_filter[0]=0;forms.image.set("");paths.geojson.set("");paths.object_csv.set("");
                next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
            } catch(const std::exception& e) {
                {project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}
                if(startup_active) {startup_active=false; startup_finish_frame=replay.frame();}
            }
            restore_pending=false;
        }
        // Switch only at frame boundaries: no draw commands can retain old GPU
        // texture IDs, document references or partially edited shape state.
        if(options.project && pending_remove && pending_remove!=active_image) {
                try {options.project->remove_image(pending_remove);image_views.erase(pending_remove);++images_removed;project_catalog_dirty=true;project_error.clear();}
                catch(const std::exception& e) {{project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
                pending_remove=0;
        }
        if(options.project && (pending_image || pending_remove)) {
            const bool removing=pending_remove!=0;
            auto next=pending_image; pending_image=0;
            if(removing) {
                next=0;
                for(const auto& item:project_images) if(item.id!=pending_remove) { next=item.id; break; }
            }
            pending_remove=0;
            try {
                if(next!=active_image || refresh_active_image || removing) {
                    auto owner=next?options.project->document(next):empty_document;
                    const bool switching=next!=active_image || detail::tool_document_identity(*owner).lock()!=detail::tool_document_identity(*active_document).lock();
                    auto saved=image_views.find(next);
                    std::unique_ptr<ObjectSelection> next_selection;
                    if(next!=active_image && (saved==image_views.end() || !saved->second.selection)) next_selection=std::make_unique<ObjectSelection>(*owner);
                    auto next_tiles=std::make_unique<GuiTiles>(owner->server());
                    auto* current=active_image && !removing?&image_views.try_emplace(active_image).first->second:nullptr;
                    if(removing) { options.project->remove_image(active_image); image_views.erase(active_image); ++images_removed; project_catalog_dirty=true; }
                    else options.project->set_active_image(next);
                    mean.invalidate();
                    previous_uploads+=tiles->uploads(); previous_generations+=tiles->generations();
                    previous_peak_bytes=std::max(previous_peak_bytes,tiles->peak_bytes());
                    tiles=std::move(next_tiles);
                    active_owner=std::move(owner); active_document=active_owner.get();
                    overlay_cache.clear();overlay_display.reset();
                    if(next!=active_image) {
                        if(current) { current->viewport=viewport; current->display=display; current->list_anchor=list_anchor; current->selection=std::move(selection); }
                        ++image_switches;
                        saved=image_views.find(next);
                        if(saved!=image_views.end() && saved->second.selection) {
                            viewport=saved->second.viewport; display=saved->second.display; list_anchor=saved->second.list_anchor;
                            selection=std::move(saved->second.selection); fit=false;
                        } else { viewport={}; display=options.display; list_anchor=0; selection=std::move(next_selection); fit=true; }
                    }
                    active_image=next; selected=selection->primary();
                    project_browser.selected=active_image;
                    restore_view=next?options.project->view(next):std::nullopt;
                    if(restore_view) { display=restore_view->display; fit=false; }
                    published_view_inputs.reset();
                    auto info=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==active_image;});
                    if(info!=project_images.end()) std::memcpy(project_image_name,info->name.c_str(),info->name.size()+1);
                    drawing=false; marquee=false; vertex_dragging=false; active_vertex.reset();
                    if(switching) {drop_editor();draft_points.clear();vertex_paths.clear();vertex_id=hole_id=0;tool=previous_tool=0;}
                    batch_class.clear();  interchange_status.clear(); input_feedback={};
                    list_ids.clear(); list_revision=calibration_revision=std::numeric_limits<std::uint64_t>::max();
                    plan={}; overview.reset(); plan_inputs.reset(); overview_center.reset();
                    max_image_switch_ms=std::max(max_image_switch_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count());
                }
                refresh_active_image=false; project_error.clear();
            } catch(const std::exception& e) { {project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));} refresh_active_image=false; }
        }
        auto& document=*active_document;
        if(detail::analysis_inputs_trace && detail::analysis_inputs_trace->before_frame)detail::analysis_inputs_trace->before_frame(inputs);
        if(auto loaded=stain_model_io.poll()) {
            if(detail::analysis_inputs_trace)++detail::analysis_inputs_trace->model_loads;
            if(stain_model_document!=&document || stain_model_project!=project_replacements || stain_model_image!=active_image || stain_model_revision!=document.revision() || stain_model_kind!=inputs.kind ||
                stain_model_vectors!=inputs.vectors || stain_model_white!=inputs.white || stain_model_basis!=inputs.estimated_basis || stain_model_basis_white!=inputs.estimated_white)
                stain_model_io.fail("Image or stain settings changed while loading; load the model again.","stale");
            else {
                input_feedback.next_editor=InputEditor::Model;inputs.use_stains(loaded->stains(),loaded->white());notifications.resolve("Stain model");
            }
        }
        input_feedback.observe(InputConsumer::Region,analysis.poll(document,selected).generation);
        input_feedback.observe(InputConsumer::StainEstimation,stain_estimate.poll(document,selected).generation);
        classifier.update(document,*selection);pixel_classifier.update(document,*selection);raw_pixel.update(document);
        input_feedback.observe(InputConsumer::Training,pixel_classifier.training_stats().generation);
        input_feedback.observe(InputConsumer::Tissue,tissue.poll(document,selected).generation);
        input_feedback.observe(InputConsumer::Cells,cell_job.poll(document,selected).generation);
        cell_table_job.poll();measurement_job.poll();
        if(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED))fluorescence.poll(document,true);
        cell_job.poll(document,selected);if(!cell_job.result()) cell_preview.clear();
        if(verifying_cells && !cell_job.busy()) {
            try {if(cell_job.poll(document,selected).state=="ready") {recheck_cell_generation=cell_job.poll(document,selected).generation;recheck_cell_id=verifying_cells;cell_job.verify_record(document,verifying_cells);}} catch(const std::exception& e) {const auto failed=cell_job.poll(document,selected);notice_observers.notice_jobs[7].observe(notifications,ui::workflow_task_labels[3],failed.state,false,false,{}, {},failed.generation);notifications.post(ui::exception_notice(ui::workflow_task_labels[3],e,{}));}
            verifying_cells=0;
        }
        tissue.poll(document,selected);
        if(verifying_tissue && !tissue.busy()) {
            try {if(tissue.poll(document,selected).state=="ready") {recheck_tissue_generation=tissue.poll(document,selected).generation;recheck_tissue_id=verifying_tissue;tissue.verify_result(document,verifying_tissue);}} catch(const std::exception& e) {const auto failed=tissue.poll(document,selected);notice_observers.notice_jobs[6].observe(notifications,ui::workflow_task_labels[2],failed.state,false,false,{}, {},failed.generation);notifications.post(ui::exception_notice(ui::workflow_task_labels[2],e,{}));}
            verifying_tissue=0;
        }
        if(!tissue.result()) tissue_preview.clear();
        const bool has_image=!options.project || active_image!=0;
        if(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED))genes.update(document,has_image,options.project.get(),active_image);
        const auto& image=document.metadata();
        if(options.project && project_catalog_dirty) {
            project_images=options.project->images(); project_catalog_dirty=false; project_filter_dirty=true;
            if(!project_browser.selected)project_browser.selected=active_image;
            auto current=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==project_browser.selected;});
            if(current!=project_images.end()) std::memcpy(project_image_name,current->name.c_str(),current->name.size()+1);
            else project_image_name[0]=0;
        }
        if(project_filter_dirty) {
            const auto previous_entry=project_browser.selected;
            project_browser.rebuild(project_images,project_filter);
            if(previous_entry!=project_browser.selected) {
                const auto item=std::find_if(project_images.begin(),project_images.end(),[&](const auto& image){return image.id==project_browser.selected;});
                if(item!=project_images.end())std::memcpy(project_image_name,item->name.c_str(),item->name.size()+1);
            }
            project_filter_dirty=false;
        }
        project_thumbnails.begin_frame(workspace_epoch);
        if(options.project)workspace.projects.remember(options.project->path());
        if(live) live->pump(live_context());
        if(exit_requested) break;
        profile.model_ms=profile_lap();
        glfwPollEvents();
        auto& input=ImGui::GetIO();
        bool replay_geometry_changed=false;
        auto replay_ready=[&](const GuiInput& event) {
            if(event.wait_for==GuiInputWait::WindowReady) {
                int width=0,height=0;glfwGetWindowSize(session.window,&width,&height);
                return !glfwGetWindowAttrib(session.window,GLFW_ICONIFIED) && width>0 && height>0 &&
                    rendered_window_width==width && rendered_window_height==height;
            }
            if(event.wait_for==GuiInputWait::StainModelFinished) return stain_model_io.finished();
            if(event.wait_for==GuiInputWait::TmaGridReady) return document.tma_core_count()>0;
            if(event.wait_for==GuiInputWait::TiffReady) return raw.ready(document);
            if(event.wait_for==GuiInputWait::TiffViewApplied) return raw.applied(document);
            if(event.wait_for==GuiInputWait::RawTaskFinished) return raw.finished();
            if(event.wait_for==GuiInputWait::RawAnalysisKept) return raw.record_kept();
            if(event.wait_for==GuiInputWait::RawAnalysisVerified) return raw.record_verified();
            if(event.wait_for==GuiInputWait::RawMeasurementsSaved) return raw.csv_state()=="saved";
            if(event.wait_for==GuiInputWait::RawNumpySaved) return raw.task_state()=="saved";
            if(event.wait_for==GuiInputWait::TmaMetadataSaved) return tma.export_state()=="saved";
            if(event.wait_for==GuiInputWait::BatchFinished) return batch.finished();
            if(event.wait_for==GuiInputWait::ObjectClassifierTrained) return classifier.trained();
            if(event.wait_for==GuiInputWait::ObjectClassifierPredicted) return classifier.predicted();
            if(event.wait_for==GuiInputWait::ObjectClassifierApplied) return classifier.applied();
            if(event.wait_for==GuiInputWait::ClassifierModelSaved) return classifier.io_state()=="saved";
            if(event.wait_for==GuiInputWait::ClassifierModelLoaded) return classifier.io_state()=="loaded";
            if(event.wait_for==GuiInputWait::ClassifierTrainingRetained) return classifier.retained();
            if(event.wait_for==GuiInputWait::ClassifierRecordLoaded) return classifier.record_loaded();
            if(event.wait_for==GuiInputWait::RawPixelFinished) {const auto r=raw_pixel.report();return r.state!="running";}
            if(event.wait_for==GuiInputWait::RawPixelTrained) return raw_pixel.report().state=="trained";
            if(event.wait_for==GuiInputWait::RawPixelPredicted) return bool(raw_pixel.report().prediction);
            if(event.wait_for==GuiInputWait::RawPixelPreviewReady) return raw_pixel.preview().ready();
            if(event.wait_for==GuiInputWait::RawPixelKept) {const auto r=raw_pixel.report();return r.training_record || r.stream_record;}
            if(event.wait_for==GuiInputWait::RawPixelFileSaved) return bool(raw_pixel.report().link);
            if(event.wait_for==GuiInputWait::RawPixelFileRead) return bool(raw_pixel.report().historical) || raw_pixel.report().state=="verified";
            if(event.wait_for==GuiInputWait::RawPixelModelSaved) return raw_pixel.report().state=="saved";
            if(event.wait_for==GuiInputWait::RawPixelContoursReady) {const auto r=raw_pixel.report();return bool(r.contours || r.stream_contours || r.historical_contours) && r.contour_preview_ready;}
            if(event.wait_for==GuiInputWait::RawPixelContoursPublished) return raw_pixel.report().contours_published;
            if(event.wait_for==GuiInputWait::RawPixelModelLoaded) return raw_pixel.report().state=="loaded";
            if(event.wait_for==GuiInputWait::PixelClassifierTrained) return pixel_classifier.trained();
            if(event.wait_for==GuiInputWait::PixelClassifierPredicted) return pixel_classifier.predicted();
            if(event.wait_for==GuiInputWait::PixelPreviewReady) return pixel_classifier.preview_ready();
            if(event.wait_for==GuiInputWait::PixelContoursReady) return pixel_classifier.contours_ready();
            if(event.wait_for==GuiInputWait::PixelContoursPublished) return pixel_classifier.contours_published();
            if(event.wait_for==GuiInputWait::PixelStreamLinked) return pixel_classifier.browser().kept()!=0;
            if(event.wait_for==GuiInputWait::PixelStreamReadReady) return pixel_classifier.browser().stats().state=="ready" && pixel_classifier.browser().stats().block && pixel_classifier.browser().preview().ready();
            if(event.wait_for==GuiInputWait::PixelStreamVerified) return pixel_classifier.browser().stats().state=="ready" && pixel_classifier.browser().stats().verified;
            if(event.wait_for==GuiInputWait::PixelStreamReadFinished) return pixel_classifier.browser().stats().state=="ready" || pixel_classifier.browser().stats().state=="failed" || pixel_classifier.browser().stats().state=="cancelled";
            if(event.wait_for==GuiInputWait::PixelStreamFinished) return pixel_classifier.stream_stats().state=="saved" || pixel_classifier.stream_stats().state=="failed" || pixel_classifier.stream_stats().state=="cancelled";
            if(event.wait_for==GuiInputWait::PixelRecordKept) return pixel_classifier.archives().state()=="kept";
            if(event.wait_for==GuiInputWait::PixelRecordModelLoaded) return pixel_classifier.record_loaded();
            if(event.wait_for==GuiInputWait::PixelHistoricalPreviewReady) return bool(pixel_classifier.historical_result()) && pixel_classifier.preview_ready();
            if(event.wait_for==GuiInputWait::PixelModelSaved) return pixel_classifier.io_state()=="saved";
            if(event.wait_for==GuiInputWait::PixelModelLoaded) return pixel_classifier.io_state()=="loaded";
            if(event.wait_for==GuiInputWait::StainsEstimated || event.wait_for==GuiInputWait::StainsApplied) return !stain_estimate.busy() && bool(stain_estimate.result()) && (event.wait_for==GuiInputWait::StainsEstimated || stain_estimate_applied);
            if(event.wait_for==GuiInputWait::FileFingerprintReady) return fingerprint_job.poll().state=="succeeded";
            if(event.wait_for==GuiInputWait::CellsReady) return !cell_job.busy() && bool(cell_job.result());
            if(event.wait_for==GuiInputWait::MeasurementTableSaved) return !measurement_job.busy() && measurement_job.poll().state=="succeeded";
            if(event.wait_for==GuiInputWait::CellTableSaved) return !cell_table_job.busy() && cell_table_job.poll().state=="succeeded";
            if(event.wait_for==GuiInputWait::CellsPublished) return !cell_job.busy() && cell_job.published();
            if(event.wait_for==GuiInputWait::CellsArchived || event.wait_for==GuiInputWait::CellsVerified) {
                const auto record=document.latest_cells(selected);
                return record && (event.wait_for==GuiInputWait::CellsArchived || (!cell_job.busy() && !verifying_cells && document.cell_status(record->id)=="current"));
            }
            if(event.wait_for==GuiInputWait::SpatialReady) {const auto r=spatial.report();return !r.busy && r.state=="ready" && (bool(r.result) || bool(r.neighborhood_result) || bool(r.density_result));}
            if(event.wait_for==GuiInputWait::SpatialKept) return spatial.report().record_id!=0;
            if(event.wait_for==GuiInputWait::SpatialVerified) {const auto r=spatial.report();return !r.busy && r.state=="verified" && r.record_status=="current";}
            if(event.wait_for==GuiInputWait::SpatialExported) return spatial.report().export_state=="saved";
            if(event.wait_for==GuiInputWait::StartupFinished) return !startup_active;
            if(event.wait_for==GuiInputWait::TissueArchived || event.wait_for==GuiInputWait::TissueVerified) {
                const auto record=document.latest_tissue(selected);
                return record && (event.wait_for==GuiInputWait::TissueArchived || (!tissue.busy() && !verifying_tissue && document.tissue_status(record->id)=="current"));
            }
            if(event.wait_for==GuiInputWait::TissueReady) return !tissue.busy() && bool(tissue.result());
            if(event.wait_for==GuiInputWait::TissueContoursReady) return !tissue.busy() && bool(tissue.contours());
            if(event.wait_for==GuiInputWait::TissueContoursPublished) return !tissue.busy() && tissue.contours_published();
            if(!ui::DialogOwnsInput() && detail::GuiPixelClassifier::handles(event.target))pixel_classifier.reveal_target(event.target);
            if(!ui::DialogOwnsInput() && detail::GuiTma::handles(event.target))tma.reveal_target(event.target);
            if(target_seen[static_cast<std::size_t>(event.target)]!=frames-1) return false;
            if(detail::GuiSpatial::handles(event.target)) return spatial.enabled(event.target);
            if(detail::GuiTma::handles(event.target)) return tma.enabled(event.target);
            if(event.target>=GuiInputTarget::RawPixelTrain && event.target<=GuiInputTarget::RawPixelApplyFile) return raw_pixel.enabled(event.target);
            if(event.target>=GuiInputTarget::ApplyTiffView && event.target<=GuiInputTarget::RawRecordsTab) return raw.enabled(event.target);
            switch(event.target) {
                case GuiInputTarget::DisplayReset:
                    return has_image && !startup_active && !close_dialog_active && !recovery_dialog_active && !restore_pending && !remove_dialog_active && !maintenance_dialog_active;
                case GuiInputTarget::CancelProjectLoad:
                    return project_load.valid() && !project_load_cancelled;
                case GuiInputTarget::ConfirmRemoveImage: case GuiInputTarget::CancelRemoveImage:
                    return remove_dialog_active;
                case GuiInputTarget::ConfirmReplaceExport: case GuiInputTarget::CancelReplaceExport:
                    return output_gate.pending();
                case GuiInputTarget::ConfirmDiscardCheckpoint: case GuiInputTarget::CancelDiscardCheckpoint:
                    return maintenance_dialog_active && reviewed_checkpoint.has_value();
                case GuiInputTarget::DiscardClose: case GuiInputTarget::SaveClose: case GuiInputTarget::KeepRecoveryClose: case GuiInputTarget::CancelClose:
                    return close_dialog_active && close_action==0;
                case GuiInputTarget::DiscardRecoveryReplacement: case GuiInputTarget::SaveRecoveryReplacement: case GuiInputTarget::CancelRecoveryReplacement:
                    return recovery_dialog_active && !restore_pending && !restore_after_save && !save_job.busy();
                default: return true;
            }
        };
        auto replay_emit=[&](const GuiInput& event) {
            // Every key names a command; Escape/Enter also reach controls as keys.
            // Key inputs also position the pointer for commands that use hovered coordinates.
            auto requested_command=ui::CommandId::Count;
            switch(event.kind) {
            case GuiInputKind::RectangleKey:requested_command=ui::CommandId::Shell_Rectangle;break;
            case GuiInputKind::UndoKey:requested_command=ui::CommandId::Header_Undo;break;
            case GuiInputKind::RedoKey:requested_command=ui::CommandId::Header_Redo;break;
            case GuiInputKind::DeleteKey:requested_command=ui::CommandId::Key_Delete;break;
            case GuiInputKind::EllipseKey:requested_command=ui::CommandId::Shell_Ellipse;break;
            case GuiInputKind::PolygonKey:requested_command=ui::CommandId::Shell_Polygon;break;
            case GuiInputKind::PolylineKey:requested_command=ui::CommandId::Shell_Polyline;break;
            case GuiInputKind::PointsKey:requested_command=ui::CommandId::Shell_Points;break;
            case GuiInputKind::FinishKey:requested_command=ui::CommandId::Key_FinishDraft;break;
            case GuiInputKind::SelectAllKey:requested_command=ui::CommandId::Key_SelectAll;break;
            case GuiInputKind::EscapeKey:requested_command=ui::CommandId::Key_EscapeCanvas;break;
            case GuiInputKind::VertexKey:requested_command=ui::CommandId::Shell_Vertices;break;
            case GuiInputKind::HoleKey:requested_command=ui::CommandId::Shell_Hole;break;
            case GuiInputKind::InsertKey:requested_command=ui::CommandId::Key_InsertVertex;break;
            case GuiInputKind::RunBatchKey:requested_command=ui::CommandId::Action_Run_batch_Ctrl_Alt_B;break;
            case GuiInputKind::CancelBatchKey:requested_command=ui::CommandId::Key_CancelBatch;break;
            case GuiInputKind::BooleanKey:requested_command=ui::CommandId::Action_Apply_Boolean_B;break;
            case GuiInputKind::CopyObjectsKey:requested_command=ui::CommandId::Header_CopyObjects;break;
            case GuiInputKind::PasteObjectsKey:requested_command=ui::CommandId::Header_PasteObjects;break;
            case GuiInputKind::PasteCurrentPlaneKey:requested_command=ui::CommandId::Key_PasteCurrentPlane;break;
            case GuiInputKind::MergeKey:requested_command=ui::CommandId::Action_Merge_annotations_M;break;
            case GuiInputKind::SaveKey:requested_command=ui::CommandId::Header_SaveProject;break;
            case GuiInputKind::ExportKey:requested_command=ui::CommandId::Action_Export_GeoJSON_F6;break;
            case GuiInputKind::ImportKey:requested_command=ui::CommandId::Action_Import_GeoJSON_F7;break;
            case GuiInputKind::NextImageKey:requested_command=ui::CommandId::Key_NextImage;break;
            case GuiInputKind::PreviousImageKey:requested_command=ui::CommandId::Key_PreviousImage;break;
            case GuiInputKind::AddImageKey:requested_command=ui::CommandId::Header_AddImages;break;
            case GuiInputKind::RelinkImageKey:requested_command=ui::CommandId::Key_RelinkImage;break;
            case GuiInputKind::AutosaveKey:requested_command=ui::CommandId::Key_Autosave;break;
            case GuiInputKind::RecoverKey:requested_command=ui::CommandId::Key_Recover;break;
            case GuiInputKind::OpenProjectKey:requested_command=ui::CommandId::Header_OpenProject;break;
            case GuiInputKind::NewProjectKey:requested_command=ui::CommandId::Key_NewProject;break;
            case GuiInputKind::RemoveImageKey:requested_command=ui::CommandId::Key_RemoveImage;break;
            case GuiInputKind::ReviewRecoveryKey:requested_command=ui::CommandId::Key_ReviewCheckpoint;break;
            case GuiInputKind::FindRecoveriesKey:requested_command=ui::CommandId::Key_FindRecoveries;break;
            case GuiInputKind::StopRecoveryMaintenanceKey:requested_command=ui::CommandId::Key_StopRecoveryMaintenance;break;
            case GuiInputKind::AnalyzeKey:requested_command=ui::CommandId::Action_Analyze_region_Ctrl_M;break;
            case GuiInputKind::CancelAnalysisKey:requested_command=ui::CommandId::Action_Cancel_analysis;break;
            case GuiInputKind::KeepAnalysisKey:requested_command=ui::CommandId::Action_Keep_result_Ctrl_K;break;
            case GuiInputKind::RecheckAnalysisKey:requested_command=ui::CommandId::Action_Recheck_saved_Ctrl_Shift_K;break;
            case GuiInputKind::DetectTissueKey:requested_command=ui::CommandId::Action_Detect_tissue_Ctrl_D;break;
            case GuiInputKind::CancelTissueKey:requested_command=ui::CommandId::Action_Cancel_tissue;break;
            case GuiInputKind::DetectCellsKey:requested_command=ui::CommandId::Action_Detect_cells_Ctrl_G;break;
            case GuiInputKind::KeepCellsKey:requested_command=ui::CommandId::Action_Add_cells_Ctrl_Shift_G;break;
            case GuiInputKind::CancelCellsKey:requested_command=ui::CommandId::Action_Cancel_cells_Ctrl_Alt_G;break;
            case GuiInputKind::KeepCellRunKey:requested_command=ui::CommandId::Action_Keep_cell_run_Ctrl_Shift_C;break;
            case GuiInputKind::RecheckCellRunKey:requested_command=ui::CommandId::Action_Recheck_cells_Ctrl_Alt_C;break;
            case GuiInputKind::ReviewSourceVerificationKey:requested_command=ui::CommandId::Action_Review_for_project_Ctrl_Alt_F;break;
            case GuiInputKind::FingerprintFileKey:requested_command=ui::CommandId::Action_Hash_file_Ctrl_Shift_F;break;
            case GuiInputKind::CancelFingerprintFileKey:requested_command=ui::CommandId::Key_CancelFingerprint;break;
            case GuiInputKind::EstimateStainsKey:requested_command=ui::CommandId::Action_Estimate_stains_F3;break;
            case GuiInputKind::ApplyStainEstimateKey:requested_command=ui::CommandId::Action_Use_estimated_stains_Shift_F3;break;
            case GuiInputKind::CancelStainEstimateKey:requested_command=ui::CommandId::Action_Cancel_estimate_Ctrl_F3;break;
            case GuiInputKind::ObjectClassifierKey:requested_command=ui::CommandId::Tool_ObjectClassifier;break;
            case GuiInputKind::TmaKey:requested_command=ui::CommandId::Tool_Tma;break;
            case GuiInputKind::RawChannelsKey:requested_command=ui::CommandId::Tool_Channels;break;
            case GuiInputKind::PixelClassifierKey:requested_command=ui::CommandId::Tool_PixelClassifier;break;
            case GuiInputKind::ExportMeasurementTableKey:requested_command=event.shift?ui::CommandId::Action_Export_project_Ctrl_Shift_Q:ui::CommandId::Action_Export_image_Ctrl_Alt_Q;break;
            case GuiInputKind::CancelMeasurementTableKey:requested_command=ui::CommandId::Action_Cancel_export_Ctrl_Alt_Shift_Q;break;
            case GuiInputKind::ExportCellTableKey:requested_command=event.shift?ui::CommandId::Action_Export_project_cells_Ctrl_Shift_X:ui::CommandId::Action_Export_image_cells_Ctrl_Alt_X;break;
            case GuiInputKind::CancelCellTableKey:requested_command=ui::CommandId::Action_Cancel_table_Ctrl_Alt_Shift_X;break;
            case GuiInputKind::KeepTissueKey:requested_command=ui::CommandId::Action_Keep_tissue_mask_Ctrl_U;break;
            case GuiInputKind::RecheckTissueKey:requested_command=ui::CommandId::Action_Recheck_saved_mask_Ctrl_Shift_U;break;
            case GuiInputKind::TraceTissueKey:requested_command=ui::CommandId::Action_Trace_contours_Ctrl_J;break;
            case GuiInputKind::KeepTissueContoursKey:requested_command=ui::CommandId::Action_Add_annotations_Ctrl_Shift_J;break;
            default:break;
            }

            if(event.kind==GuiInputKind::SetUiScale) {ui_multiplier=event.x;return;}
            if(event.kind==GuiInputKind::ResizeWindow) {glfwSetWindowSize(session.window,static_cast<int>(event.x),static_cast<int>(event.y));replay_geometry_changed=true;return;}
            if(event.kind==GuiInputKind::MinimizeWindow) {glfwIconifyWindow(session.window);return;}
            if(event.kind==GuiInputKind::RestoreWindow) {glfwRestoreWindow(session.window);replay_geometry_changed=true;return;}
            if(event.kind==GuiInputKind::CloseWindow) {
                glfwSetWindowShouldClose(session.window,true);
                if(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED)) glfwRestoreWindow(session.window);
                return;
            }
            if(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED)) throw std::runtime_error("restore the window before replaying UI input");
            auto origin=event.target==GuiInputTarget::Canvas?previous_canvas:(event.target==GuiInputTarget::Overview?overview_offset:list_offset);
            auto extent=event.target==GuiInputTarget::Canvas?previous_canvas_size:(event.target==GuiInputTarget::Overview?overview_size:list_size);
            if(event.target==GuiInputTarget::Window) {origin={0,0};extent=input.DisplaySize;}
            if(event.target==GuiInputTarget::SaveProject) {origin=save_project_offset;extent=save_project_size;}
            if(detail::GuiObjectClassifier::handles(event.target)) {
                const auto bounds=classifier.bounds(event.target);origin={static_cast<float>(bounds.x),static_cast<float>(bounds.y)};extent={static_cast<float>(bounds.width),static_cast<float>(bounds.height)};
            }
            if(detail::GuiPixelClassifier::handles(event.target)) {
                const auto bounds=pixel_classifier.bounds(event.target);origin={static_cast<float>(bounds.x),static_cast<float>(bounds.y)};extent={static_cast<float>(bounds.width),static_cast<float>(bounds.height)};
            }
            if(detail::GuiSpatial::handles(event.target)) {const auto bounds=spatial.bounds(event.target);origin={float(bounds.x),float(bounds.y)};extent={float(bounds.width),float(bounds.height)};}
            if(detail::GuiTma::handles(event.target)) {const auto bounds=tma.bounds(event.target);origin={static_cast<float>(bounds.x),static_cast<float>(bounds.y)};extent={static_cast<float>(bounds.width),static_cast<float>(bounds.height)};}
            if(event.target>=GuiInputTarget::RawPixelTrain && event.target<=GuiInputTarget::RawPixelApplyFile) {const auto bounds=raw_pixel.bounds(event.target);origin={float(bounds.x),float(bounds.y)};extent={float(bounds.width),float(bounds.height)};}
            if(event.target>=GuiInputTarget::ApplyTiffView && event.target<=GuiInputTarget::RawRecordsTab) {const auto bounds=raw.bounds(event.target);origin={static_cast<float>(bounds.x),static_cast<float>(bounds.y)};extent={static_cast<float>(bounds.width),static_cast<float>(bounds.height)};}
            if(event.target==GuiInputTarget::LoadStainModel || event.target==GuiInputTarget::SaveStainModel) {
                const auto& bounds=stain_model_bounds[event.target==GuiInputTarget::LoadStainModel?0:1];
                origin={float(bounds.x),float(bounds.y)};extent={float(bounds.width),float(bounds.height)};
            }
            if(event.target==GuiInputTarget::UseImageStains) {origin={float(image_stains_bounds.x),float(image_stains_bounds.y)};extent={float(image_stains_bounds.width),float(image_stains_bounds.height)};}
            if(event.target>=GuiInputTarget::CopyObjects && event.target<=GuiInputTarget::PasteCurrentPlane) {
                const auto& bounds=clipboard_bounds[std::size_t(event.target)-std::size_t(GuiInputTarget::CopyObjects)];origin={float(bounds.x),float(bounds.y)};extent={float(bounds.width),float(bounds.height)};
            }
            if(event.target>=GuiInputTarget::ToggleObjectLock && event.target<=GuiInputTarget::ClearObjectProperties) {
                const auto& bounds=object_state_bounds[std::size_t(event.target)-std::size_t(GuiInputTarget::ToggleObjectLock)];origin={float(bounds.x),float(bounds.y)};extent={float(bounds.width),float(bounds.height)};
            }
            if(event.target==GuiInputTarget::ApplySourceVerification || event.target==GuiInputTarget::AcceptSourceReplacement) {
                const auto& bounds=source_review_bounds[event.target==GuiInputTarget::ApplySourceVerification?0:1];origin={float(bounds.x),float(bounds.y)};extent={float(bounds.width),float(bounds.height)};
            }
            if(event.target==GuiInputTarget::ResetLayout) {origin=layout_reset_offset;extent=layout_reset_size;}
            if(event.target==GuiInputTarget::ProjectList) { origin=project_list_offset; extent=project_list_size; }
            if(event.target==GuiInputTarget::DisplayReset) { origin=display_reset_offset; extent=display_reset_size; }
            if(event.target==GuiInputTarget::OverviewBudgetIncrease) {origin=overview_more_offset;extent=overview_more_size;}
            if(event.target==GuiInputTarget::OverviewBudgetDecrease) {origin=overview_less_offset;extent=overview_less_size;}
            if(event.target==GuiInputTarget::DiscardRecoveryReplacement) { origin=discard_recovery_offset; extent=discard_recovery_size; }
            if(event.target==GuiInputTarget::KeepRecoveryClose) { origin=keep_close_offset; extent=keep_close_size; }
            if(event.target==GuiInputTarget::CancelClose) { origin=cancel_close_offset; extent=cancel_close_size; }
            if(event.target==GuiInputTarget::DiscardClose) { origin=discard_close_offset; extent=discard_close_size; }
            if(event.target==GuiInputTarget::SaveClose) { origin=save_close_offset; extent=save_close_size; }
            if(event.target==GuiInputTarget::SaveRecoveryReplacement) { origin=save_recovery_offset; extent=save_recovery_size; }
            if(event.target==GuiInputTarget::CancelRecoveryReplacement) { origin=cancel_recovery_offset; extent=cancel_recovery_size; }
            if(event.target==GuiInputTarget::CancelProjectLoad) { origin=cancel_load_offset; extent=cancel_load_size; }
            if(event.target==GuiInputTarget::ConfirmReplaceExport) { origin=confirm_export_offset; extent=confirm_export_size; }
            if(event.target==GuiInputTarget::CancelReplaceExport) { origin=cancel_export_offset; extent=cancel_export_size; }
            if(event.target==GuiInputTarget::ConfirmRemoveImage) { origin=confirm_remove_offset; extent=confirm_remove_size; }
            if(event.target==GuiInputTarget::CancelRemoveImage) { origin=cancel_remove_offset; extent=cancel_remove_size; }
            if(event.target==GuiInputTarget::ConfirmDiscardCheckpoint) { origin=confirm_checkpoint_offset; extent=confirm_checkpoint_size; }
            if(event.target==GuiInputTarget::CancelDiscardCheckpoint) { origin=cancel_checkpoint_offset; extent=cancel_checkpoint_size; }
            if(event.target==GuiInputTarget::Image) {
                auto p=viewport.image_to_screen({event.x,event.y});
                const auto limit=static_cast<double>(std::numeric_limits<float>::max())/4;
                if(!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x)>limit || std::abs(p.y)>limit)
                    throw std::invalid_argument("image replay position exceeds screen coordinate range");
                replay_mouse=ImVec2{previous_canvas.x+static_cast<float>(p.x),previous_canvas.y+static_cast<float>(p.y)};
                input.AddMousePosEvent(replay_mouse->x,replay_mouse->y);
            } else {replay_mouse=ImVec2{origin.x+static_cast<float>(event.x)*extent.x,origin.y+static_cast<float>(event.y)*extent.y};input.AddMousePosEvent(replay_mouse->x,replay_mouse->y);}
            if(requested_command!=ui::CommandId::Count) {
                if(event.kind==GuiInputKind::EscapeKey)replay_controls.enqueue(requested_command,ImGuiKey_Escape);
                else if(event.kind==GuiInputKind::FinishKey)replay_controls.enqueue(requested_command,ImGuiKey_Enter);
                else command_inputs.request(requested_command);
                return;
            }
            if(event.ctrl) input.AddKeyEvent(ImGuiMod_Ctrl,true);
            if(event.shift) input.AddKeyEvent(ImGuiMod_Shift,true);
            switch(event.kind) {
                case GuiInputKind::LeftDown: input.AddMouseButtonEvent(0,true); break;
                case GuiInputKind::LeftUp: input.AddMouseButtonEvent(0,false); break;
                case GuiInputKind::RightDown: input.AddMouseButtonEvent(1,true); break;
                case GuiInputKind::RightUp: input.AddMouseButtonEvent(1,false); break;
                case GuiInputKind::Wheel: {
                    auto& events=ImGui::GetCurrentContext()->InputEventsQueue;const auto before=events.Size;
                    input.AddMouseWheelEvent(0,static_cast<float>(event.wheel));
                    if(events.Size>before) replay_wheels.insert(events.back().EventId);break;
                }
                case GuiInputKind::CloseWindow: glfwSetWindowShouldClose(session.window,true); break;
                default: break;
            }
        };

        if(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED)) {
            replay.dispatch(replay_ready,replay_emit,std::chrono::steady_clock::now());
            glfwPollEvents();
        }
        auto suspend_window=[&] {
            if(!glfwGetWindowAttrib(session.window,GLFW_ICONIFIED)) return false;
            if(!was_suspended) {
                drawing=marquee=vertex_dragging=false;active_vertex.reset();
                input.AddMouseButtonEvent(0,false);input.AddMouseButtonEvent(1,false);ImGui::ClearActiveID();
            }
            was_suspended=true;target_seen.fill(-2);rendered_window_width=rendered_window_height=0;
            service_project(false,false,editing_is_blocked(),false);
            ++suspended_iterations;
            suspended_work_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count();
            const auto began_wait=std::chrono::steady_clock::now();glfwWaitEventsTimeout(.02);
            suspended_wait_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began_wait).count();
            replay.finish_frame(!settling);
            if(!within_frames() && (options.settle_tiles || !capture_path.empty()))
                throw std::runtime_error("restore the window before capture or tile settlement");
            if(frame_limit && replay.frame()>=static_cast<std::uint64_t>(frame_limit) && startup_active)
                minimized_startup.check(std::chrono::steady_clock::now(),"minimized startup");
            return true;
        };
        if(suspend_window()) continue;
        was_suspended=false;minimized_startup.reset();
        ImGui_ImplGlfw_NewFrame();
        // Release replayed control keys as well as mouse-gesture modifiers.
        // ImGui may defer a queued release behind a trickled press.
        if(!options.inputs.empty()) {
            input.AddKeyEvent(ImGuiMod_Ctrl,false);input.AddKeyEvent(ImGuiMod_Shift,false);
            input.AddKeyEvent(ImGuiKey_Escape,false);input.AddKeyEvent(ImGuiKey_Enter,false);
        }
        if(replay_mouse) input.AddMousePosEvent(replay_mouse->x,replay_mouse->y);
        replay_geometry_changed=false;
        if(ui::command_trace && ui::command_trace->replay_input)ui::command_trace->replay_input(replay_emit);
        replay.dispatch(replay_ready,replay_emit,std::chrono::steady_clock::now());
        if(suspend_window()) continue;
        if(replay_geometry_changed) ImGui_ImplGlfw_NewFrame();

        float sx=1,sy=1;glfwGetWindowContentScale(session.window,&sx,&sy);
        const auto next_scale=GuiScale::from(std::max(sx,sy),std::max(input.DisplayFramebufferScale.x,input.DisplayFramebufferScale.y),ui_multiplier);
        if(const auto next_theme=ui::design::current();next_theme!=applied_theme) {
            ui::design::apply(base_style,next_theme);style=base_style;style.ScaleAllSizes(next_scale.logical);
            style.FontSizeBase=ui::design::metrics.font_size*next_scale.logical;applied_theme=next_theme;
        }
        if(!fonts_initialized || !font_scale.same_fonts(next_scale)) {
            // Rebuild only between frames, on the GL owner thread. Never scale
            // the previous style, which would accumulate rounding and drift.
            overlay_cache.clear();
            style=base_style;style.ScaleAllSizes(next_scale.logical);
            style.FontSizeBase=ui::design::metrics.font_size*next_scale.logical;
            // Dynamic bakes reuse the same sources; framebuffer density comes
            // from GLFW. The backend owns texture creation/update/destruction.
            font_scale=next_scale;fonts_initialized=true;++font_rebuilds;
            drawing=marquee=vertex_dragging=false;active_vertex.reset();
            input.AddMouseButtonEvent(0,false);input.AddMouseButtonEvent(1,false);ImGui::ClearActiveID();
        }
        ui_scale=next_scale;
        ImGui_ImplOpenGL3_NewFrame();
        // Keep explicitly replayed wheel events, including events deferred by
        // ImGui's trickle queue. Physical wheel messages can otherwise zoom the
        // canvas or scroll panels while a deterministic test is running.
        if(!options.inputs.empty()) {
            auto& events=ImGui::GetCurrentContext()->InputEventsQueue;std::set<unsigned int> pending;
            for(int i=0;i<events.Size;) {
                const auto& event=events[i];
                if(event.Type!=ImGuiInputEventType_MouseWheel) {++i;continue;}
                if(!replay_wheels.count(event.EventId)) {events.erase(events.Data+i);++ignored_physical_wheels;}
                else {pending.insert(event.EventId);++i;}
            }
            replay_wheels.swap(pending);
        }
        if(pending_language!=-2) {ui::i18n::choose(pending_language);pending_language=-2;}
        ui::CommandFrameScope command_scope(command_inputs);
        if(ui::workflow_trace && ui::workflow_trace->before_frame)ui::workflow_trace->before_frame(frames);
        // A key that dismisses a popup or ends text editing still belongs to
        // that owner for this entire frame.
        const bool popup_owned_keys=ui::drawn_popup_owns_keys(true);
        const bool text_owned_keys=ImGui::GetIO().WantTextInput;
        ImGui::NewFrame();ui::begin_frame();ui::reset_workflow_trace();
        if(ui::notifications_trace && ui::notifications_trace->before_frame)ui::notifications_trace->before_frame(notifications);
        const auto previous_plane=canvas_scope.plane;const auto sketch_change=detail::update(canvas_scope,*active_document,active_image);
        if(sketch_change.image || sketch_change.source) {plane_sketches.clear();draft_points.clear();hole_id=0;}
        else if(sketch_change.plane) {
            auto previous=std::find_if(plane_sketches.begin(),plane_sketches.end(),[&](const auto& p){return p.plane==previous_plane;});
            if(!draft_points.empty() || hole_id) {
                PlaneSketch saved{previous_plane,std::move(draft_points),tool,hole_id,hole_revision};
                if(previous==plane_sketches.end())plane_sketches.push_back(std::move(saved));else *previous=std::move(saved);
            }else if(previous!=plane_sketches.end())plane_sketches.erase(previous);
            const auto next=std::find_if(plane_sketches.begin(),plane_sketches.end(),[&](const auto& p){return p.plane==canvas_scope.plane;});
            if(next==plane_sketches.end() || (next->points.empty() && !next->hole_id)) {draft_points.clear();hole_id=0;}else {draft_points=next->points;tool=previous_tool=next->tool;hole_id=next->hole_id;hole_revision=next->hole_revision;}
            drawing=marquee=vertex_dragging=false;active_vertex.reset();
        }
        // Refresh retained overlays before the canvas can draw them.
        fluorescence.poll(*active_document,true);
        genes.update(*active_document,!options.project || active_image!=0,options.project.get(),active_image);
        replay_controls.dispatch(command_inputs);
        if(options.project)ui::claim_command_navigation();
        const std::string_view project_path=options.project?std::string_view(options.project->path()):std::string_view{};
        if(project_path!=path_project_source){path_project_source=project_path;path_context.project_directory=project_path.empty()?std::string{}:std::filesystem::absolute(std::filesystem::u8path(project_path)).parent_path().u8string();}
        forms.saved_destination=options.project?options.project->path():std::string{};
        for(auto* field:all_paths)field->sync(path_context);
        if(detail::project_forms_trace && detail::project_forms_trace->before_frame)detail::project_forms_trace->before_frame(forms,all_paths,path_drops);
        const auto* drop_modal=ImGui::GetTopMostPopupModal();
        for(const auto& drop:path_drops) {
            const auto previous_form=forms.shown;const auto previous_total=forms.total();
            std::string drop_refusal;const char* drop_reason=nullptr;
            if(detail::route_path_drop(drop,forms,all_paths,drop_modal || project_load.valid() || save_job.busy(),drop_modal?drop_modal->ID:0,bool(options.project),&drop_refusal,&drop_reason) && workspace.enabled() && (previous_form!=forms.shown || previous_total!=forms.total()))workspace.select(GuiWorkspace::project_pane);
            if(drop_reason)notifications.post({ui::Severity::Warning,ui::command(ui::CommandId::Header_ProjectPanel).caption,drop_reason,{},true});
        }
        path_drops.clear();
        // Preserve each scientific/cached texture's filtering. The newer
        // backend otherwise overrides GL_NEAREST with its default sampler.
        ImGui::GetBackgroundDrawList()->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerFromTex,nullptr);
        bool save_project_requested=false;
        bool autosave_requested=false;
        const bool editing_blocked=calibration_editor.active() || object_info_active || image_info_active || language_dialog_active || output_gate.pending() || bool(source_review) || editing_is_blocked() || ui::DialogOwnsInput();
        const auto& io = ImGui::GetIO();
        if(ui::command_trace && ui::command_trace->before_context)ui::command_trace->before_context(frames);
        command_inputs.context=ui::chord_context(popup_owned_keys,previous_canvas_window,command_workspace_windows,shell && shell->keyboard_active());
        command_inputs.context.text_input=command_inputs.context.text_input || text_owned_keys;
        if(ui::command_trace && ui::command_trace->probe_all)
            for(const auto& row:ui::command_table)if(row.chord)ui::chord_pressed(row.id,command_inputs);
        const bool overlay_gesture=drawing || marquee || vertex_dragging || !draft_points.empty();
        auto toggle_objects=[&] {if(has_image && !editing_blocked && !overlay_gesture) {objects_visible=!objects_visible;if(!objects_visible)tool=0;}};
        if(ui::chord_pressed(ui::CommandId::Header_ShowObjects,ui::frame_commands()))toggle_objects();
        auto history_action=[&](bool redo) { try { if(redo) document.redo(); else document.undo(); notifications.resolve("Annotations"); } catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); } };
        auto focus_task=[&](Task task) {workspace.select_task(task);if(workspace.enabled())workspace.select(GuiWorkspace::workflow_pane);};
        const ui::ToolContext tool_context{has_image,bool(options.project),has_image && detail::GuiRaw::supports(document.server()->reader_id())};
        auto focus_tables=[&](int kind) {
            table_kind=kind;table_scope=0;
            if(kind>0)measurement_kind=kind-1;
            focus_task(Task::Tables);
        };
        auto open_image_information=[&](bool analysis,std::optional<StainTransform> transform=std::nullopt,bool separate_stains=false) {
            image_info_document=active_document;image_info_epoch=workspace_epoch;image_info_requested=image_info_active=true;
            image_info_search[0]=0;image_info_analysis=analysis;image_info_transform=std::move(transform);image_info_stains=separate_stains;
        };


        auto build_tissue_request=[&] {
            return make_tissue_request(document,selected,tissue_parameters,tissue_feature,tissue_connectivity,
                tissue_grid.get(),inputs.nucleus,tissue_min_area,tissue_max_area,make_transform);
        };
        auto build_cell_request=[&] {
            return make_cell_request(document,selected,cell_parameters,cell_feature,cell_connectivity,
                cell_nominal,cell_grid,grid_cells,cell_min_area,cell_max_area,make_transform);
        };
        auto sampling_key=[&] {
            detail::SamplingInputKey key;key.values.reserve(110);
            key.append(reinterpret_cast<std::uintptr_t>(&document),reinterpret_cast<std::uintptr_t>(document.server().get()),
                workspace_epoch,active_image,selected,document.revision(),document.calibration_revision(),document.object_visibility_revision(),inputs.nucleus,inputs.intensity_statistics);
            for(const auto v:inputs.vectors)key.add(v);for(const auto v:inputs.white)key.add(v);
            key.add(bool(inputs.estimated_basis));if(inputs.estimated_basis)for(const auto& vector:*inputs.estimated_basis)for(const auto v:vector)key.add(v);
            for(const auto v:inputs.estimated_white)key.add(v);return key;
        };
        cell_table_job.poll();const auto measurement_export_status=measurement_job.poll();fingerprint_job.poll();
        bool export_submitted=false;
        if(pending_path_export) {
            const auto operation=pending_path_export->operation;
            const auto source=pending_path_export->source;
            if(pending_path_field->take_picked()) {
                auto intent=std::move(*pending_path_export);pending_path_export.reset();intent.path=pending_path_field->state().resolved;export_submitted=true;
                try{submit_export(std::move(intent));}catch(const std::exception& e){export_failure(operation,e.what(),true,&e,source);}
            }else if(!pending_path_field->picker_active())pending_path_export.reset();
        }
        auto begin_export=[&](ExportOp operation,std::vector<std::uint64_t> ids=std::vector<std::uint64_t>{},bool all_kept_runs=false,const char* source=nullptr) {
            if(!source)source=operation==ExportOp::GeoJson || operation==ExportOp::ObjectCsv?"Annotations":ui::workflow_task_labels[4];
            try {
                if(export_submitted)return;export_submitted=true;
                if(output_gate.pending() || pending_path_export || export_busy())throw ui::NoticeException<std::runtime_error>("Finish or cancel the current export first.");
                detail::PendingGuiExport intent;intent.operation=operation;intent.ids=std::move(ids);intent.source=source;
                intent.cell_options=cell_table_options;if(all_kept_runs)intent.cell_options.published_objects=false;intent.measurement_options=measurement_options;
                intent.measurement_kind=measurement_kind==0?MeasurementTableKind::Roi:MeasurementTableKind::Tissue;
                intent.allow_losses=interchange_losses;intent.nested=interchange_nested;intent.intensity=csv_intensity;
                if(operation==ExportOp::GeoJson || operation==ExportOp::ObjectCsv) {
                    intent.selected_only=interchange_selected;
                    if(interchange_selected){intent.ids=selection->ids();if(intent.ids.empty())throw ui::NoticeException<std::invalid_argument>("Select objects before exporting a subset");}
                }
                if(intent.project_scope() && !options.project)throw ui::NoticeException<std::invalid_argument>("Open a project before exporting project tables.");
                if(!intent.project_scope() && !has_image)throw ui::NoticeException<std::invalid_argument>("Select an image before exporting.");
                auto& field=paths.output(operation,measurement_kind);intent.owner=export_owner(intent);
                if(!*field.text()) {
                    field.open_picker(path_context,detail::suggested_export_name(options.project?options.project->path():std::string{},document.server()->source_locator(),operation,measurement_kind));
                    pending_path_export=std::move(intent);pending_path_field=&field;return;
                }
                field.sync(path_context);intent.path=field.state().resolved;submit_export(std::move(intent));
            }catch(const std::exception& e){export_failure(operation,e.what(),true,&e,source);}
        };

        const auto tables_availability=[&](int kind,int scope,bool for_cells=false)->ui::Availability {
            if(editing_blocked)return ui::WorkflowBlocked();
            if(kind<0)return {};
            if(cell_table_job.busy() || measurement_job.busy())return {false,"Wait for the current work to finish."};
            if(scope==1 && !options.project)return {false,"Open a project first."};
            if(scope!=1 && !has_image)return {false,"Open an image first."};
            if(kind==0 && scope==0)return detail::analysis_tasks::kept_cells_export_availability(document,selected,cell_table_options,cell_table_job.busy(),editing_blocked,for_cells);
            if(kind==0 && scope==2 && !document.cell_record_count())return {false,"Keep a cell run before exporting."};
            return {};
        };
        auto export_image_cells=[&](const char* source=nullptr) {
            if(cell_table_options.published_objects)begin_export(ExportOp::ImageCells,{},false,source);
            else if(const auto saved=document.latest_cells(selected))begin_export(ExportOp::SavedCells,{saved->id},false,source);
        };
        auto import_objects=[&] {
            // Immediate import names the file and advertises Undo so the user
            // can identify and reverse the change.
            if(output_gate.pending())return;
            try {
                const detail::AnnotationFileImportRequest request{"geojson",paths.import_source.state().resolved,
                    {interchange_pixels,interchange_losses,qupath_ellipse_precision}};
                auto report=detail::import_annotations(document,request);
                auto visible=report.imported_ids;visible.erase(std::remove_if(visible.begin(),visible.end(),[&](auto id){return !document.object_visible(id);}),visible.end());
                selection->replace(visible);selected=selection->primary();interchange_status=feedback("Imported %zu objects from %s (Undo available)",std::size_t(report.features),detail::export_path(paths.import_source.state().resolved).c_str());
                for(const auto& issue:report.issues)interchange_status+="\n"+issue.message+" ("+std::to_string(issue.count)+")";
                if(visible.size()!=report.imported_ids.size())interchange_status+="\nSwitch Z/time or display channels to view objects on other planes.";
                notifications.resolve("Annotations");
            } catch(const std::exception& e) {interchange_status=e.what();notifications.post(ui::exception_notice("Annotations",e,paths.import_source.state().resolved));}
        };
        if(has_image && annotation_geojson_available && !editing_blocked && ui::chord_pressed(ui::CommandId::Action_Export_GeoJSON_F6,ui::frame_commands()))begin_export(ExportOp::GeoJson);
        if(has_image && annotation_geojson_available && !editing_blocked && ui::chord_pressed(ui::CommandId::Action_Import_GeoJSON_F7,ui::frame_commands()))import_objects();
        // Cancellation belongs beside progress, where its job is visible.
        // These rows still accept replay requests without a physical chord.
        if(!editing_blocked && ui::chord_pressed(ui::CommandId::Action_Cancel_table_Ctrl_Alt_Shift_X,ui::frame_commands()))cell_table_job.cancel();
        const bool project_cells_key=ui::chord_pressed(ui::CommandId::Action_Export_project_cells_Ctrl_Shift_X,ui::frame_commands());
        const bool image_cells_key=ui::chord_pressed(ui::CommandId::Action_Export_image_cells_Ctrl_Alt_X,ui::frame_commands());
        const auto refused_export=[&](ui::Availability available,bool cells,bool cells_rule=false) {
            if(available.enabled || editing_blocked)return false;
            auto& message=cells?cell_table_error:measurement_error;
            message=available.reason?ui::i18n::text(available.reason):"";
            (cells?cell_table_feedback:measurement_feedback)=cells_rule?ExportFeedback::CellsRefused:ExportFeedback::Refused;
            if(cells_rule)notifications.post({ui::Severity::Warning,ui::workflow_task_labels[3],available.reason,{},true});
            return true;
        };
        const bool image_key_for_cells=workspace.enabled() && workspace.workflow_task()==Task::Cells;
        const bool project_cells_refused=project_cells_key && refused_export(tables_availability(0,1),true);
        const bool image_cells_refused=image_cells_key && refused_export(tables_availability(0,0,image_key_for_cells),true,image_key_for_cells);
        if(project_cells_key && !project_cells_refused && !editing_blocked)begin_export(ExportOp::ProjectCells);
        if(image_cells_key && !image_cells_refused && !editing_blocked)export_image_cells(image_key_for_cells?ui::workflow_task_labels[3]:ui::workflow_task_labels[4]);
        if(!editing_blocked && ui::chord_pressed(ui::CommandId::Action_Cancel_export_Ctrl_Alt_Shift_Q,ui::frame_commands()))measurement_job.cancel();
        const bool project_measurements_key=ui::chord_pressed(ui::CommandId::Action_Export_project_Ctrl_Shift_Q,ui::frame_commands());
        const bool image_measurements_key=ui::chord_pressed(ui::CommandId::Action_Export_image_Ctrl_Alt_Q,ui::frame_commands());
        if(project_measurements_key && !refused_export(tables_availability(1+measurement_kind,1),false) && !editing_blocked)begin_export(ExportOp::ProjectMeasurements);
        if(image_measurements_key && !refused_export(tables_availability(1+measurement_kind,0),false) && !editing_blocked)begin_export(ExportOp::ImageMeasurements);
        const auto files_availability=[&](ui::CommandId action)->ui::Availability {
            if(editing_blocked)return ui::WorkflowBlocked();
            if(fingerprint_job.busy())return {false,"Wait for the current work to finish."};
            if(action==ui::CommandId::Action_Review_for_project_Ctrl_Alt_F) {
                if(!options.project)return {false,"Open a project first."};
                if(!active_image)return {false,"Open an image first."};
                if(!fingerprint_job.result())return {false,"Hash the file first."};
            }
            return {};
        };
        const auto hash_available=files_availability(ui::CommandId::Action_Hash_file_Ctrl_Shift_F);
        const auto review_available=files_availability(ui::CommandId::Action_Review_for_project_Ctrl_Alt_F);
        const bool hash_key=ui::chord_pressed(ui::CommandId::Action_Hash_file_Ctrl_Shift_F,ui::frame_commands());
        const bool review_key=ui::chord_pressed(ui::CommandId::Action_Review_for_project_Ctrl_Alt_F,ui::frame_commands());
        if(review_key && !review_available.enabled && !editing_blocked){fingerprint_error=ui::i18n::text(review_available.reason);fingerprint_refused=true;notifications.post({ui::Severity::Warning,ui::workflow_task_labels[6],review_available.reason,{},true});}
        const bool open_project_key=options.project && !editing_blocked && ui::chord_pressed(ui::CommandId::Header_OpenProject,ui::frame_commands());
        if(open_project_key) {select_project_form(ProjectForm::Open);focus_open_project=!project_open_path[0];if(workspace.enabled())workspace.select(GuiWorkspace::project_pane);}
        if(!editing_blocked) {
            if(has_image && annotation_geojson_available && (ui::chord_pressed(ui::CommandId::Action_Export_GeoJSON_F6,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Import_GeoJSON_F7,ui::frame_commands())) && workspace.enabled())workspace.select(GuiWorkspace::objects_pane);
            if(ui::chord_pressed(ui::CommandId::Action_Estimate_stains_F3,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Use_estimated_stains_Shift_F3,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Cancel_estimate_Ctrl_F3,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Analyze_region_Ctrl_M,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Cancel_analysis,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Keep_result_Ctrl_K,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Recheck_saved_Ctrl_Shift_K,ui::frame_commands()))focus_task(Task::Region);
            if(ui::chord_pressed(ui::CommandId::Action_Detect_tissue_Ctrl_D,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Cancel_tissue,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Trace_contours_Ctrl_J,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Add_annotations_Ctrl_Shift_J,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Keep_tissue_mask_Ctrl_U,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Recheck_saved_mask_Ctrl_Shift_U,ui::frame_commands()))focus_task(Task::Tissue);
            if(ui::chord_pressed(ui::CommandId::Action_Detect_cells_Ctrl_G,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Add_cells_Ctrl_Shift_G,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Cancel_cells_Ctrl_Alt_G,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Keep_cell_run_Ctrl_Shift_C,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Recheck_cells_Ctrl_Alt_C,ui::frame_commands()))focus_task(Task::Cells);
            if(hash_key || review_key || ui::chord_pressed(ui::CommandId::Key_CancelFingerprint,ui::frame_commands()))focus_task(Task::Files);
            const bool project_cells=project_cells_key;
            const bool cancel_cells_table=ui::chord_pressed(ui::CommandId::Action_Cancel_table_Ctrl_Alt_Shift_X,ui::frame_commands());
            if(project_cells || cancel_cells_table || image_cells_key) {
                table_kind=0;table_scope=project_cells || cancel_cells_table?1:0;
                focus_task(image_key_for_cells && !project_cells && !cancel_cells_table?Task::Cells:Task::Tables);
            }
            const bool project_measurements=project_measurements_key;
            const bool cancel_measurements=ui::chord_pressed(ui::CommandId::Action_Cancel_export_Ctrl_Alt_Shift_Q,ui::frame_commands());
            if(project_measurements || cancel_measurements || image_measurements_key) {table_kind=1+measurement_kind;table_scope=project_measurements || cancel_measurements?1:0;focus_task(Task::Tables);}
        }
        ImDrawList* shell_draw_list=nullptr;
        bool shell_toolbar_placed=false;
        bool settings_requested=workspace.take_settings_request();
        if(shell) {
            const auto& labels=ui::shell_labels();
            ui::ShellSnapshot snapshot{workspace.active_pane(),tool,has_image,document.can_undo(),document.can_redo(),detail::GuiRaw::supports(document.server()->reader_id()),editing_blocked || ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)};
            snapshot.objects_visible=objects_visible;snapshot.gesture_active=overlay_gesture;
            shell->update(static_cast<int>(io.DisplaySize.x),static_cast<int>(io.DisplaySize.y),ui_scale.logical,applied_theme==ui::design::Theme::Light,snapshot,labels,std::max(io.DisplayFramebufferScale.x,io.DisplayFramebufferScale.y));
            const auto* hovered=ImGui::GetCurrentContext()->HoveredWindow;
            const bool native_overlay=io.WantTextInput || (hovered && hovered->RootWindow->ID!=ImHashStr("Workspace") && hovered->RootWindow->ID!=ImHashStr("Slide"));
            shell->input({native_overlay?-10000.f:io.MousePos.x,native_overlay?-10000.f:io.MousePos.y,io.MouseWheel,ImGui::IsMouseClicked(0),ImGui::IsMouseReleased(0),
                !native_overlay && ImGui::IsKeyPressed(ImGuiKey_Tab),!native_overlay && ImGui::IsKeyPressed(ImGuiKey_Enter),!native_overlay && ImGui::IsKeyPressed(ImGuiKey_Space),io.KeyShift,io.KeyCtrl});
            // One keyboard focus owner: native widgets must not also activate
            // when Tab/Enter/Space operate a retained shell control.
            if(shell->keyboard_active()) {
                input.ConfigFlags&=~ImGuiConfigFlags_NavEnableKeyboard;
                for(auto key:{ImGuiKey_Tab,ImGuiKey_Enter,ImGuiKey_Space}) ImGui::SetKeyOwner(key,ImHashStr("RmlUi shell input"),ImGuiInputFlags_LockThisFrame);
                ImGui::NavMoveRequestCancel();ImGui::GetCurrentContext()->NavActivateId=0;
            } else input.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
            if(ui::chord_pressed(ui::CommandId::Header_ReloadUi,ui::frame_commands())) shell->reload();
            for(auto command:shell->take_commands()) {
                const int value=static_cast<int>(command);
                if(!snapshot.enabled(command)) continue;
                if(value<4) workspace.activate(GuiWorkspace::panes[value]);
                else if(value<12) tool=value-4;
                else if(command==ui::ShellCommand::Fit) fit=true;
                else if(command==ui::ShellCommand::Undo) history_action(false);
                else if(command==ui::ShellCommand::Redo) history_action(true);
                else if(command==ui::ShellCommand::Spatial) tools.open(Tool::Spatial,tool_context);
                else if(command==ui::ShellCommand::Channels) tools.open(Tool::Channels,tool_context);
                else if(command==ui::ShellCommand::Objects) toggle_objects();
                else if(command==ui::ShellCommand::ResetLayout) workspace.reset();
                else if(command==ui::ShellCommand::Settings) settings_requested=true;
            }
        }
        auto clipboard_status=[&](const InterchangeReport& report,const char* action) {
            interchange_status=std::string(action)+" "+std::to_string(report.features)+" objects";
            for(const auto& issue:report.issues)interchange_status+="\n"+issue.message+" ("+std::to_string(issue.count)+")";
            clipboard_error.clear();notifications.resolve("Annotations");
        };
        auto copy_selection=[&]() {
            try {
                const auto ids=selection->ids();
                const detail::AnnotationClipboardExportRequest request{"geojson",std::cref(ids),clipboard_descendants,interchange_losses};
                auto report=detail::export_annotations(document,request);
                glfwSetClipboardString(session.window,report.text.c_str());
                ++clipboard_copies;clipboard_copied_objects+=report.features;clipboard_status(report,"Copied");
            } catch(const std::exception& e) {clipboard_error=e.what();interchange_status=clipboard_error;notifications.post(ui::exception_notice("Annotations",e,{}));}
        };
        auto paste_clipboard=[&](bool current_plane) {
            try {
                const auto* text=glfwGetClipboardString(session.window);
                if(!text)throw ui::NoticeException<std::invalid_argument>("Clipboard does not contain GeoJSON text");
                constexpr std::size_t limit=16*1024*1024;std::size_t length=0;
                while(length<=limit && text[length])++length;
                if(length>limit)throw ui::NoticeException<std::length_error>("Clipboard exceeds 16 MiB limit");
                const detail::AnnotationClipboardImportRequest request{"geojson",std::string(text,length),
                    current_plane?std::optional<ObjectPlane>(document.active_plane()):std::nullopt,interchange_losses};
                auto report=detail::import_annotations(document,request);
                auto visible=report.imported_ids;
                visible.erase(std::remove_if(visible.begin(),visible.end(),[&](auto id){return !document.object_visible(id);}),visible.end());
                selection->replace(visible);selected=selection->primary();
                vertex_dragging=false;active_vertex.reset();marquee=false;drawing=false;draft_points.clear();
                ++clipboard_pastes;clipboard_pasted_objects+=report.features;clipboard_status(report,"Pasted");
                if(visible.size()!=report.imported_ids.size())interchange_status+="\nSwitch Z/time or channels to view objects on other planes.";
            } catch(const std::exception& e) {clipboard_error=e.what();interchange_status=clipboard_error;notifications.post(ui::exception_notice("Annotations",e,{}));}
        };
        if(has_image && annotation_geojson_available && !editing_blocked) {
            if(ui::chord_pressed(ui::CommandId::Header_CopyObjects,ui::frame_commands()))copy_selection();
            if(ui::chord_pressed(ui::CommandId::Header_PasteObjects,ui::frame_commands()))paste_clipboard(false);
            if(ui::chord_pressed(ui::CommandId::Key_PasteCurrentPlane,ui::frame_commands()))paste_clipboard(true);
        }

        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);ImGui::PopStyleVar();
        if(shell)shell_draw_list=ImGui::GetWindowDrawList();
        auto focus_pane=[&](const char* name) {if(workspace.enabled()) workspace.select(name);};
        ui::HeaderSnapshot header;
        header.editing_blocked=editing_blocked;header.image=has_image;header.project=bool(options.project);
        header.project_dirty=options.project && options.project->dirty();header.save_busy=save_job.busy();
        header.can_undo=document.can_undo();header.can_redo=document.can_redo();
        header.can_copy=has_image && annotation_geojson_available && !selection->ids().empty();
        header.can_paste=has_image && annotation_geojson_available;header.objects_visible=objects_visible;
        header.overlay_gesture=overlay_gesture;header.workspace=workspace.enabled();header.reload_ui=bool(shell);header.desktop=ui::desktop_application;
        header.ui_size=ui_multiplier;header.theme=ui::design::state.choice;
        const ui::HeaderDispatch header_dispatch=[&](ui::HeaderCommand command) {
            using Command=ui::HeaderCommand;
            switch(command) {
            case Command::OpenProject:focus_pane(GuiWorkspace::project_pane);focus_open_project=true;break;
            case Command::AddImages:focus_pane(GuiWorkspace::project_pane);focus_add_image=true;break;
            case Command::SaveProject:save_project_requested=true;break;
            case Command::SaveProjectAs:focus_pane(GuiWorkspace::project_pane);select_project_form(ProjectForm::Save);focus_save_project=true;break;
            case Command::Close:glfwSetWindowShouldClose(session.window,true);break;
            case Command::Undo:history_action(false);break;
            case Command::Redo:history_action(true);break;
            case Command::CopyObjects:copy_selection();break;
            case Command::PasteObjects:paste_clipboard(false);break;
            case Command::FitImage:fit=true;break;
            case Command::ShowObjects:toggle_objects();break;
            case Command::ProjectPanel:focus_pane(GuiWorkspace::project_pane);break;
            case Command::ImagePanel:focus_pane(GuiWorkspace::image_pane);break;
            case Command::AnnotationsPanel:focus_pane(GuiWorkspace::objects_pane);break;
            case Command::WorkflowPanel:focus_pane(GuiWorkspace::workflow_pane);break;
            case Command::ResetLayout:workspace.reset();break;
            case Command::ReloadUi:if(shell)shell->reload();break;
            case Command::IncreaseUiSize:ui_multiplier=std::min(3.,ui_multiplier+.25);break;
            case Command::DecreaseUiSize:ui_multiplier=std::max(.5,ui_multiplier-.25);break;
            case Command::AutomaticUiSize:ui_multiplier=1;break;
            case Command::ThemeDark:ui::design::state.choice=ui::design::Theme::Dark;break;
            case Command::ThemeLight:ui::design::state.choice=ui::design::Theme::Light;break;
            case Command::ThemeSystem:ui::design::state.choice=ui::design::Theme::System;break;
            case Command::ChooseLanguage:language_requested=true;break;
            case Command::QuickReference:help_window.request(detail::HelpPage::QuickReference);break;
            case Command::Documentation:help_window.request(detail::HelpPage::Documentation);break;
            case Command::About:help_window.request(detail::HelpPage::About);break;
            }
        };
        ui::draw_header(header,header_dispatch,[&] {
            for(std::size_t i=0;i<ui::workflow_task_labels.size();++i)
                if(ui::MenuItem(ui::workflow_task_labels[i]))focus_task(static_cast<Task>(i));
            ui::Separator();ui::ToolMenus(tools,tool_context);
        });
        if(settings_requested && !editing_blocked) {
            const float activity=ui::design::metrics.activity_row*ui_scale.logical;
            const auto anchor=shell?shell->bounds(ui::ShellCommand::Settings):ui::ShellRect{0,io.DisplaySize.y-activity,ui::design::metrics.activity_width*ui_scale.logical,activity};
            ImGui::SetNextWindowPos({anchor.x+anchor.width,anchor.y+anchor.height},ImGuiCond_Appearing,{0,1});
            ImGui::OpenPopup("Workspace settings");
        }
        if(ui::BeginPopupMenu("Workspace settings")) {ui::draw_header_settings(header,header_dispatch);ImGui::EndPopup();}
        const auto header_keys=ui::header_shortcuts(editing_blocked);
        if(header_keys.language)language_requested=true;
        if(header_keys.help)help_window.request(detail::HelpPage::QuickReference);
        if(startup_active) {
            ++startup_frames;
            const auto phase=startup_progress->phase.load();
            ui::DataText(project_load_cancelled?"Finishing cancelled opening...":
                (options.startup_kind=="image"?"Opening image...":options.startup_kind=="recovery"?"Opening recovery...":"Opening project..."));
            ui::TextWrapped("%s",options.startup_path.c_str());
            const char* stage=phase==1?"Reading file metadata":phase==2?"Reading project data":phase==3?"Validating image objects":phase==4?"Checking project references":"Preparing workspace";
            ui::TextDisabled("%s",stage);
            if(options.startup_kind!="image" && phase>1) {
                ui::Text("Images: %zu / %zu   Objects read: %zu / %zu",startup_progress->images.load(),startup_progress->total_images.load(),
                    startup_progress->objects.load(),startup_progress->total_objects.load());
            }
            ImGui::BeginDisabled(project_load_cancelled);
            if(ui::Button("Cancel opening")) {project_load_cancelled=true; startup_progress->cancelled.store(true);}
            cancel_load_offset=ImGui::GetItemRectMin(); auto cl=ImGui::GetItemRectMax(); cancel_load_size={cl.x-cancel_load_offset.x,cl.y-cancel_load_offset.y}; mark_target(GuiInputTarget::CancelProjectLoad);
            ImGui::EndDisabled(); ui::Separator();
        } else if(startup_requested && !startup_succeeded && project_replacements==0 &&
                  options.project->revision_==0 && options.project->path().empty()) {
            ui::TextWrapped("%s",startup_cancelled?"Opening cancelled. The workspace is empty.":project_error.c_str());
        }
        ui::IconReason inherited_icon_reason(editing_blocked?(ImGui::GetTopMostPopupModal()?"Finish the current dialog first.":"Wait for the current work to finish."):!has_image?"Open an image first.":nullptr);
        ImGui::BeginDisabled(editing_blocked);
        if(live) {
            const auto scripting=live->stats();ui::TextDisabled("Python session connected / %zu pending",scripting.queued);
            if(scripting.active_ticket) {ui::TextDisabled("Processing script job");ImGui::SameLine();if(ui::SmallButton("Cancel script job")) live->cancel_active();}
            else ui::TextDisabled("Ready");
        }
        ImGui::BeginDisabled(!has_image);
        if(!editing_blocked && has_image) {
            if(ui::chord_pressed(ui::CommandId::Shell_Rectangle,ui::frame_commands()))tool=tool==1?0:1;
            if(ui::chord_pressed(ui::CommandId::Shell_Ellipse,ui::frame_commands()))tool=2;
            if(ui::chord_pressed(ui::CommandId::Shell_Polygon,ui::frame_commands()))tool=3;
            if(ui::chord_pressed(ui::CommandId::Shell_Polyline,ui::frame_commands()))tool=4;
            if(ui::chord_pressed(ui::CommandId::Shell_Points,ui::frame_commands()))tool=5;
            if(ui::chord_pressed(ui::CommandId::Shell_Vertices,ui::frame_commands()))tool=6;
            if(ui::chord_pressed(ui::CommandId::Shell_Hole,ui::frame_commands()))tool=7;
            if(ui::chord_pressed(ui::CommandId::Header_Undo,ui::frame_commands()))history_action(false);
            if(ui::chord_pressed(ui::CommandId::Header_Redo,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Key_RedoAlternate,ui::frame_commands()))history_action(true);
        }
        auto draw_toolbar=[&] {
            ui::ToolPalette palette;
            ui::IconReason toolbar_reason(has_image?"Finish the current gesture first.":"Open an image first.");
            if(shell) {
                const auto p=ImGui::GetCursorScreenPos();
                const float height=shell->place_toolbar(p.x,p.y,ImGui::GetContentRegionAvail().x);
                shell_toolbar_placed=true;
                if(ui::workflow_trace) {
                    const auto eye=shell->bounds(ui::ShellCommand::Objects);
                    ui::workflow_trace->actions["Show objects (O)"]={{eye.x,eye.y,eye.width,eye.height},ui::command(ui::CommandId::Shell_Objects).caption,has_image && !editing_blocked && !overlay_gesture && eye.width>0 && eye.height>0};
                }
                ImGui::Dummy({ImGui::GetContentRegionAvail().x,height});shell_draw_list=ImGui::GetWindowDrawList();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY()-ImGui::GetStyle().ItemSpacing.y);
                const auto reset=shell->bounds(ui::ShellCommand::ResetLayout);
                layout_reset_offset={reset.x,reset.y};layout_reset_size={reset.width,reset.height};
                const bool ready=!editing_blocked && !ImGui::GetTopMostPopupModal();
                target_seen[static_cast<std::size_t>(GuiInputTarget::ResetLayout)]=ready?frames:-2;
                const auto spatial_button=shell->bounds(ui::ShellCommand::Spatial);
                spatial.shell_button({spatial_button.x,spatial_button.y,spatial_button.width,spatial_button.height},ready && has_image);
                target_seen[static_cast<std::size_t>(GuiInputTarget::SpatialOpen)]=ready && has_image?frames:-2;
                return;
            }
            const float scale=ui::design::scale(),side=ui::design::metrics.icon_size*scale,gap=ui::design::spacing.xs*scale;
            const bool docked=workspace.enabled();
            const int utility_controls=3+(detail::GuiRaw::supports(document.server()->reader_id())?1:0)+(docked?1:0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(gap,gap));
            if(docked) {
                const float width=std::max(side,ImGui::GetContentRegionAvail().x-2*ui::design::spacing.md*scale);
                const int rows=ui::toolbar_rows(width,utility_controls);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(ui::design::spacing.md*scale,ui::design::spacing.sm*scale));
                ImGui::BeginChild("Slide toolbar",{0,rows*(side+gap)-gap+2*ui::design::spacing.sm*scale},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::PopStyleVar();
            }
            ImGui::BeginDisabled(!has_image);
        for(int i=0;i<8;++i) {
            if(i && ui::available()<ui::design::metrics.icon_size*ui::design::scale()) ImGui::NewLine();
            if(ui::tool_button(i,tool,ui::shell_commands[4+i])) tool=i;
            ImGui::SameLine();
        }
        if(tool!=previous_tool) { if(tool!=0)objects_visible=true;vertex_dragging=false; active_vertex.reset(); marquee=false; drawing=false; draft_points.clear(); previous_tool=tool; }
        ui::toolbar_separator(2);
        ImGui::BeginDisabled(!document.can_undo()); if (ui::icon_action("##Undo",ui::ActionIcon::Undo,ui::CommandId::Shell_Undo,{has_image && document.can_undo(),has_image?"There are no edits to undo.":"Open an image first."})) history_action(false); ImGui::EndDisabled();
        ImGui::SameLine(); ImGui::BeginDisabled(!document.can_redo()); if (ui::icon_action("##Redo",ui::ActionIcon::Redo,ui::CommandId::Shell_Redo,{has_image && document.can_redo(),has_image?"There are no edits to redo.":"Open an image first."})) history_action(true); ImGui::EndDisabled();
        ImGui::SameLine();ui::toolbar_separator(utility_controls);
        if (ui::icon_action("##Fit image",ui::ActionIcon::Fit,ui::CommandId::Shell_Fit)) fit = true;
        ImGui::SameLine();spatial.open_icon(mark_target);
        if(detail::GuiRaw::supports(document.server()->reader_id())) {ImGui::SameLine();if(ui::icon_action("Raw channels (F2)",ui::Icon::Channels,ui::CommandId::Shell_Channels)) tools.open(Tool::Channels,tool_context);}
            ImGui::SameLine();ImGui::BeginDisabled(overlay_gesture);{ui::IconReason gesture_reason(overlay_gesture?"Finish the current gesture first.":ui::icon_availability.reason);if(ui::IconButton(ui::presentation_label(ui::CommandId::Shell_Objects,"Show objects (O)"),ui::Icon::Eye,ui::design::Action::Ghost,ui::command_chord(ui::CommandId::Shell_Objects),objects_visible))toggle_objects();}ImGui::EndDisabled();
            ui::trace_workflow_action("Show objects (O)",ui::command(ui::CommandId::Shell_Objects).caption);
            ui::trace_layout_command(ui::command(ui::CommandId::Shell_Objects).legacy_id);
            ImGui::EndDisabled();
            if(docked) {
            ImGui::SameLine();
            if(ui::icon_action("##Reset layout",ui::ActionIcon::Reset,ui::CommandId::Shell_ResetLayout)) workspace.reset();
            layout_reset_offset=ImGui::GetItemRectMin();auto lr=ImGui::GetItemRectMax();layout_reset_size={lr.x-layout_reset_offset.x,lr.y-layout_reset_offset.y};mark_target(GuiInputTarget::ResetLayout);
                ImGui::EndChild();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY()-gap);
            }
            ImGui::PopStyleVar();
        };
        if(!workspace.enabled()) draw_toolbar();
        if(tool!=previous_tool) { if(tool!=0)objects_visible=true;vertex_dragging=false; active_vertex.reset(); marquee=false; drawing=false; draft_points.clear(); previous_tool=tool; }
        selected=selection->primary();
        if(!editing_blocked && has_image && ui::chord_pressed(ui::CommandId::Key_SelectAll,ui::frame_commands())) { selection->select_all(); selected=selection->primary(); }
        auto delete_selection=[&]() {
            if(!objects_visible) {objects_visible=true;return;}
            try {
                auto ids=selection->ids();
                if(ids.size()==1) document.remove_annotation(ids.front()); else document.remove_objects(ids);
                selection->clear(); selected=0; notifications.resolve("Annotations");
            } catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
        };
        if (selected && tool!=6 && !editing_blocked && has_image && ui::chord_pressed(ui::CommandId::Key_Delete,ui::frame_commands())) delete_selection();
        ImGui::EndDisabled();
        auto workflow_task=workspace.workflow_task();
        bool workflow_body=false,project_browser_enter=false;
        auto workflow_navigation=[&] {
            if(!workspace.enabled()) return; // Compact API layout retains its continuous inspector.
            workspace.title("Workflow");
            ui::SetNextItemWidth(-1);
            if(ui::ComboScope combo{"##Task",ui::i18n::text(ui::workflow_task_labels[static_cast<std::size_t>(workflow_task)])};combo) {
                for(std::size_t i=0;i<ui::workflow_task_labels.size();++i)
                    if(ui::Selectable(ui::i18n::label(ui::workflow_task_labels[i]).c_str(),workflow_task==static_cast<Task>(i))) {
                        workflow_task=static_cast<Task>(i);workspace.select_task(workflow_task);
                    }
            }
            if(has_image && selected && workflow_task<=Task::Cells)
                ui::TextDisabled("%s",document.annotation(selected).name.c_str());
            const bool changed=last_workflow_task!=workflow_task;last_workflow_task=workflow_task;
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
            ImGui::BeginChild("Workflow task body",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
            ImGui::PopStyleVar(2);
            if(changed) ImGui::SetScrollY(0);workflow_body=true;
        };
        if(workspace.enabled()) {
            if(!workspace.error().empty()) ui::TextWrapped("Layout preferences: %s",workspace.error().c_str());
            if(shell && !shell->error().empty()) ui::TextWrapped("UI resources: %s",shell->error().c_str());
            workspace.dockspace(editing_blocked,shell.get());
            workspace.begin(GuiWorkspace::project_pane,workspace.browser);
        } else ImGui::BeginChild("Inspector", {std::min(285*ui_scale.logical,io.DisplaySize.x*.45f),-ImGui::GetFrameHeightWithSpacing()-2*ui::design::spacing.sm*ui_scale.logical}, ImGuiChildFlags_Borders);
        ui::i18n::SurfaceTrace pane_translation("pane/Project");
        if(options.project) {
            // The table registers these chords' routing claims once per frame,
            // keeping ImGui's PageUp/PageDown navigation out of the inspector.
            const bool next_image=ui::chord_pressed(ui::CommandId::Key_NextImage,ui::frame_commands());
            const bool previous_image=ui::chord_pressed(ui::CommandId::Key_PreviousImage,ui::frame_commands());
            if(focus_open_project) select_project_form(ProjectForm::Open);
            if(focus_add_image && project_form!=ProjectForm::Relink) select_project_form(ProjectForm::Add);
            auto mark_save_button=[&] {
                save_project_offset=ImGui::GetItemRectMin();const auto hi=ImGui::GetItemRectMax();
                save_project_size={hi.x-save_project_offset.x,hi.y-save_project_offset.y};mark_target(GuiInputTarget::SaveProject);
            };
            auto new_project=[&] {
                staged_project=std::make_shared<Project>(); staged_is_recovery=false;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
                if(options.project->dirty()) recovery_popup_requested=recovery_dialog_active=true;
                else { autosave.cancel(); restore_pending=true; }
                project_error.clear();
            };
            if(!editing_blocked && !project_load.valid() && !save_job.busy() && !maintenance_job.valid() && ui::chord_pressed(ui::CommandId::Key_NewProject,ui::frame_commands()))new_project();
            bool switch_project_requested=false;
            if(workspace.enabled())workspace.title("Project");else ui::PaneTitle("Project");
            const auto identity_path=options.project->path();
            std::string identity=identity_path.empty()?ui::i18n::text("Project - unsaved"):std::filesystem::u8path(identity_path).stem().u8string();
            if(!identity_path.empty() && options.project->dirty()) identity+=" *";
            {ui::ActionRow project_path(1);
            if(ui::ComboScope combo{"##Project switcher",identity.c_str()};combo) {
                const bool busy=project_load.valid() || save_job.busy() || maintenance_job.valid();
                for(std::size_t i=0;i<workspace.projects.recent.size();++i) {
                    const auto& path=workspace.projects.recent[i];const auto file=std::filesystem::u8path(path);
                    std::string label=file.stem().u8string();
                    for(const auto& other:workspace.projects.recent)if(other!=path && std::filesystem::u8path(other).stem()==file.stem()) {label+=" ("+file.parent_path().filename().u8string()+")";break;}
                    ImGui::PushID(static_cast<int>(i));std::error_code error;const bool available=std::filesystem::is_regular_file(file,error);
                    if(ui::MenuItem(ui::data_label(label,"recent-project").c_str(),nullptr,path==identity_path,!busy && available && !qupath_review.valid(),true) && path!=identity_path) {
                        forms.open_path.set(path);select_project_form(ProjectForm::Open);switch_project_requested=true;
                    }
                    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",path.c_str());ImGui::PopID();
                }
                if(!workspace.projects.recent.empty())ui::Separator();
                if(ui::MenuItem(ui::presentation_label(ui::CommandId::Key_NewProject,"New project (Ctrl+N)"),ui::command_chord(ui::CommandId::Key_NewProject),false,!busy))new_project();
                if(ui::MenuItem("Open project...",nullptr,false,!busy && !qupath_review.valid())) {select_project_form(ProjectForm::Open);focus_open_project=true;}
                if(ui::MenuItem("Save project as...",nullptr,false,!save_job.busy())) {select_project_form(ProjectForm::Save);focus_save_project=true;}
                ui::Separator();
                if(ui::project_browser_menu(project_browser))project_filter_dirty=true;
                if(ui::MenuItem("Refresh"))project_catalog_dirty=true;
                if(ui::MenuItem("Project settings"))project_settings_visible=true;
            }
            if(ImGui::IsItemHovered() && !identity_path.empty()) ImGui::SetTooltip("%s",identity_path.c_str());
            project_path.next();ImGui::BeginDisabled(save_job.busy() || !options.project->dirty());
            if(ui::icon_action("Save project (Ctrl+S)",ui::Icon::Save,ui::CommandId::Header_SaveProject,{!save_job.busy() && options.project->dirty(),save_job.busy()?"Wait for the current work to finish.":"There are no project changes to save."})) save_project_requested=true;
            mark_save_button();ImGui::EndDisabled();}
            if(!editing_blocked && !save_job.busy() && ui::chord_pressed(ui::CommandId::Header_SaveProject,ui::frame_commands())) save_project_requested=true;
            if(save_job.busy()) ui::TextDisabled("Saving project...");
            if(forms.draw_save(path_context,save_job.busy(),mark_save_button).save)save_project_requested=true;
            if(qupath_preview && qupath_review_input!=forms.open_path.state().resolved){qupath_preview.reset();qupath_losses=false;}
            const auto open_request=forms.draw_open(path_context,project_load.valid() || qupath_review.valid() || save_job.busy() || maintenance_job.valid(),!editing_blocked && (open_project_key || switch_project_requested),qupath_review.valid(),qupath_preview,qupath_losses);
            const auto import_path=forms.open_path.state().resolved;
            auto import_extension=std::filesystem::u8path(import_path).extension().u8string();detail::ascii_lower_in_place(import_extension);
            if(open_request.refresh_review){qupath_preview.reset();qupath_losses=false;review_qupath(import_path);}
            if(open_request.open) {
                project_load_cancelled=false;loading_recovery=false;project_error.clear();
                if(import_extension==".qpproj" && !qupath_preview)review_qupath(import_path);
                else if(import_extension==".qpproj") {
                    const auto preview=qupath_preview;const bool losses=qupath_losses;const auto destination=forms.qupath_destination.state().resolved;qupath_cancel->store(false);
                    project_load=std::async(std::launch::async,[preview,destination,losses,cancel=qupath_cancel]{const auto started=std::chrono::steady_clock::now();auto imported=preview->import_new(destination,losses,cancel.get());return LoadedProject{std::move(imported),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),false,true};});
                }else project_load=std::async(std::launch::async,load_project,import_path,false,std::string{});
            }
            if(project_load.valid() && !startup_active) {
                ui::TextDisabled(project_load_cancelled?"Finishing cancelled load...":(loading_recovery?"Opening recovery...":"Opening project..."));
                ImGui::BeginDisabled(project_load_cancelled);
                if(ui::Button("Cancel opening")) project_load_cancelled=true;
                cancel_load_offset=ImGui::GetItemRectMin(); auto cl=ImGui::GetItemRectMax(); cancel_load_size={cl.x-cancel_load_offset.x,cl.y-cancel_load_offset.y}; mark_target(GuiInputTarget::CancelProjectLoad);
                ImGui::EndDisabled();
            }
            bool add_image=!editing_blocked && ui::chord_pressed(ui::CommandId::Header_AddImages,ui::frame_commands());
            bool relink_image=has_image && !editing_blocked && ui::chord_pressed(ui::CommandId::Key_RelinkImage,ui::frame_commands());
            bool remove_image=has_image && !project_load.valid() && !editing_blocked && ui::chord_pressed(ui::CommandId::Key_RemoveImage,ui::frame_commands());
            if(relink_image)relink_image_id=workspace.enabled() && workspace.active_pane()==0?project_browser.selected:active_image;
            if(remove_image)remove_image_id=workspace.enabled() && workspace.active_pane()==0?project_browser.selected:active_image;
            if(add_image && !project_image_path[0]) {select_project_form(ProjectForm::Add);focus_add_image=true;add_image=false;}
            if(relink_image && !project_image_path[0]) {select_project_form(ProjectForm::Relink);focus_add_image=true;relink_image=false;}
            if(!io.WantTextInput && !editing_blocked && (next_image || previous_image) && project_images.size()>1) {
                // Keep ownership through key-up even if Ctrl is released first;
                // otherwise the still-held Page key resumes ImGui navigation.
                ImGui::SetKeyOwner(next_image?ImGuiKey_PageDown:ImGuiKey_PageUp,ImGui::GetID("Project image switch"),ImGuiInputFlags_LockUntilRelease);
                auto it=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==active_image;});
                auto index=static_cast<std::size_t>(it-project_images.begin());
                pending_image=it==project_images.end()?project_images[next_image?0:project_images.size()-1].id:project_images[(index+project_images.size()+(next_image?1:-1))%project_images.size()].id;
            }
            auto choose_entry=[&](std::uint64_t id) {
                if(project_browser.selected!=id)rename_project_image=focus_project_rename=false;
                project_browser.selected=id;
                const auto item=std::find_if(project_images.begin(),project_images.end(),[&](const auto& image){return image.id==id;});
                if(item!=project_images.end())std::memcpy(project_image_name,item->name.c_str(),item->name.size()+1);
                if(project_form==ProjectForm::Relink && id!=relink_image_id)select_project_form(ProjectForm::None);
            };
            auto entry_menu=[&](const ProjectImage& item) {
                if(ui::MenuItem("Rename image",nullptr,false,!project_load.valid())) {choose_entry(item.id);rename_project_image=focus_project_rename=true;}
                if(!item.embedded && ui::MenuItem(item.available?ui::presentation_label(ui::CommandId::Key_RelinkImage,"Relink image (F9)"):"Locate file...",item.available?ui::command_chord(ui::CommandId::Key_RelinkImage):nullptr,false,!project_load.valid())) {choose_entry(item.id);relink_image_id=item.id;select_project_form(ProjectForm::Relink);focus_add_image=true;}
                ui::Separator();
                if(ui::MenuItem(ui::presentation_label(ui::CommandId::Key_RemoveImage,"Remove image (F12)"),ui::command_chord(ui::CommandId::Key_RemoveImage),false,!project_load.valid())) {remove_image_id=item.id;remove_image=true;}
            };
            const auto image_request=forms.draw_image(path_context,project_load.valid() || save_job.busy(),has_image,project_images,image_reader,reader_labels);
            add_image=add_image || image_request.add;relink_image=relink_image || image_request.relink;

            ui::SetNextItemWidth(-1);
            if(ui::InputTextWithHint("##Image filter","Find images, metadata or tags",project_filter,sizeof(project_filter)))project_filter_dirty=true;
            if(ui::project_browser_filters(project_browser))project_filter_dirty=true;
            workspace.projects.group=project_browser.group_key;
            if(!project_error.empty())ui::TextWrapped("%s",project_error.c_str());
            if(!project_settings_visible && !autosave_stats.error.empty())ui::TextWrapped("%s",autosave_stats.error.c_str());
            const float row_height=ui::design::metrics.project_row_height*ui_scale.logical;
            const float split_room=std::max(1.f,ImGui::GetContentRegionAvail().y-ui::design::spacing.sm*ui_scale.logical-2*ImGui::GetStyle().ItemSpacing.y);
            const float minimum_list=ui::ProjectPreferences::minimum_list_height(split_room,row_height+ImGui::GetStyle().ItemSpacing.y,project_browser.rows.size()),minimum_inspector=std::min(112*ui_scale.logical,split_room*.45f);
            const float list_height=workspace.projects.list_height(split_room,row_height+ImGui::GetStyle().ItemSpacing.y,project_browser.rows.size(),minimum_inspector);
            // Reserve the list's scrollbar gutter on its caption too. Explicit
            // fit/overflow flags keep that edge stable during catalogue changes.
            const bool list_scrolls=project_browser.rows.size()*(row_height+ImGui::GetStyle().ItemSpacing.y)>list_height;
            const auto list_scroll_flags=list_scrolls?ImGuiWindowFlags_AlwaysVerticalScrollbar:project_browser.rows.empty()?ImGuiWindowFlags_None:ImGuiWindowFlags_NoScrollbar;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
            {ui::LayoutRegion image_list_region("Project images");
            {ui::CaptionedStrip images("Images",1,list_scrolls?ImGui::GetStyle().ScrollbarSize:0);images.next();
            if(ui::IconButton(ui::command(ui::CommandId::Header_AddImages).caption,ui::Icon::Plus,ui::design::Action::Ghost,ui::command_chord(ui::CommandId::Header_AddImages))) {select_project_form(ProjectForm::Add);focus_add_image=true;}
            ui::trace_layout_command(ui::command(ui::CommandId::Header_AddImages).legacy_id);}
            ImGui::BeginChild("Project image list",{0,list_height},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground|list_scroll_flags);
            ImGui::PopStyleVar(2);
            const float pitch=row_height+ImGui::GetStyle().ItemSpacing.y;
            project_list_offset=ImGui::GetCursorScreenPos();project_list_size={ui::available(),std::min(ImGui::GetContentRegionAvail().y,pitch*project_browser.rows.size())};
            if(project_browser.rows.empty()) {
                if(project_images.empty())ui::TextDisabled("No images. Use Add images to fill this list.");
                if(project_images.empty()) {if(ui::Button(ui::command(ui::CommandId::Header_AddImages).caption,{ui::available(),0})) {select_project_form(ProjectForm::Add);focus_add_image=true;}}
                else ui::TextDisabled("No images match this filter.");
            }
            ImGuiListClipper image_clipper;image_clipper.Begin(static_cast<int>(project_browser.rows.size()),pitch);
            while(image_clipper.Step())for(int row=image_clipper.DisplayStart;row<image_clipper.DisplayEnd;++row) {
                const auto& entry=project_browser.rows[row];
                if(entry.image==ui::ProjectBrowser::group_row) {
                    const auto at=ImGui::GetCursorScreenPos();const auto count=std::to_string(entry.count);
                    ui::design::Heading heading;ImGui::PushStyleColor(ImGuiCol_Text,ui::design::color(ui::design::colors().text_secondary));
                    ImGui::PushID(row);if(ui::CatalogEntry(entry.group.empty()?ui::i18n::text("Unassigned"):entry.group.c_str(),false,count.c_str(),0,0,32*ui_scale.logical,row_height)) {project_browser.toggle_group(entry.group);project_filter_dirty=true;}
                    const bool folded=project_browser.collapsed.count(entry.group) && !project_filter[0];
                    ui::draw_icon(folded?ui::Icon::ChevronRight:ui::Icon::ChevronDown,at,{at.x+28*ui_scale.logical,at.y+row_height},ImGui::GetColorU32(ImGuiCol_Text));
                    ImGui::PopID();ImGui::PopStyleColor();continue;
                }
                const auto& item=project_images[entry.image];
                ImGui::PushID(static_cast<int>(item.id>>32));ImGui::PushID(static_cast<int>(item.id));
                const char* status=!item.available?ui::i18n::text("Missing file"):nullptr;
                const float gap=ui::design::spacing.sm*ui_scale.logical,side=ui::design::metrics.icon_size*ui_scale.logical;
                const float actions=(item.available?1:2)*(side+gap);
                const auto at=ImGui::GetCursorScreenPos();const float width=ui::available();
                const bool row_hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(at,{at.x+width,at.y+row_height});
                const float leading=(ui::design::metrics.project_thumbnail_width+ui::design::spacing.sm+ui::design::spacing.md)*ui_scale.logical;
                const bool picked=ui::CatalogEntry(item.name.c_str(),item.id==active_image,status,ImGuiSelectableFlags_SelectOnNav,actions,leading,row_height);
                const bool row_focused=ImGui::IsItemFocused();
                if(picked)choose_entry(item.id);
                const bool open_key=ImGui::IsItemFocused() && !io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_Enter);
                if(item.available && ((item.id!=active_image && picked && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) || open_key)) {
                    pending_image=item.id;
                    project_browser_enter=open_key;
                    if(open_key)ImGui::SetKeyOwner(ImGuiKey_Enter,ImGui::GetID("##Catalog entry"),ImGuiInputFlags_LockThisFrame);
                }
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%lld x %lld\n%s",item.name.c_str(),static_cast<long long>(item.width),static_cast<long long>(item.height),item.embedded?"Embedded RGB":item.source_path.c_str());
                if(ui::BeginContextMenu("Image actions")) {choose_entry(item.id);entry_menu(item);ImGui::EndPopup();}
                DisplaySettings preview_display;
                if(item.id==active_image)preview_display=display;else if(const auto view=options.project->view(item.id))preview_display=view->display;
                const float inset=(row_height-ui::design::metrics.project_thumbnail_height*ui_scale.logical)*.5f;
                project_thumbnails.draw(item.id,item.available?options.project->document(item.id)->server():nullptr,preview_display,{at.x+gap,at.y+inset},{at.x+gap+ui::design::metrics.project_thumbnail_width*ui_scale.logical,at.y+row_height-inset});
                if(!item.available) {
                    ImGui::SameLine(0,gap);ImGui::BeginDisabled(project_load.valid());ui::IconReason locate_reason(project_load.valid()?"Wait for the current work to finish.":ui::icon_availability.reason);
                    if(ui::RowIconButton("Locate file...",ui::Icon::Link,true,row_height)) {choose_entry(item.id);relink_image_id=item.id;select_project_form(ProjectForm::Relink);focus_add_image=true;}
                    ImGui::EndDisabled();
                }
                ImGui::SameLine(0,gap);
                if(ui::RowIconButton("Image actions",ui::Icon::More,row_hovered || row_focused || ImGui::IsPopupOpen("Entry actions"),row_height)) {choose_entry(item.id);ImGui::OpenPopup("Entry actions");}
                if(ui::BeginPopupMenu("Entry actions")) {entry_menu(item);ImGui::EndPopup();}
                ImGui::PopID();ImGui::PopID();
            }
            ImGui::EndChild();}
            const auto browsed=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==project_browser.selected;});
            // Start a manual resize at the visible divider, not the unused
            // preferred fraction, so its first drag never jumps.
            float visible_fraction=1-list_height/split_room;
            if(ui::ResizeDivider("##Project inspector divider",visible_fraction,split_room,minimum_inspector,minimum_list)) {
                workspace.projects.inspector_fraction=visible_fraction;workspace.projects.inspector_resized=true;
            }
            {
                ui::ActionRow row(1);ui::PaneTitle("Image metadata");row.next();
                ImGui::BeginDisabled(browsed==project_images.end() || project_load.valid());
                if(ui::IconButton("Edit image metadata",ui::Icon::Edit,ui::Availability{browsed!=project_images.end() && !project_load.valid(),project_load.valid()?"Wait for the current work to finish.":"Select an image to see its metadata."})) {
                    try {entry_editor.open(options.project,project_browser.selected);}
                    catch(const std::exception& e){{project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
                }
                ImGui::EndDisabled();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(ui::design::spacing.sm*ui_scale.logical,ui::design::spacing.sm*ui_scale.logical));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,0);
            if(ImGui::BeginChild("##Project inspector",{0,std::max(1.f,ImGui::GetContentRegionAvail().y)},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoBackground)) {
                if(inspected_image!=project_browser.selected) {ImGui::SetScrollY(0);inspected_image=project_browser.selected;}
                if(browsed==project_images.end())ui::TextDisabled("Select an image to see its metadata.");
                else {
                    ImGui::PushID(static_cast<int>(browsed->id>>32));ImGui::PushID(static_cast<int>(browsed->id));
                    if(rename_project_image) {
                        ui::ActionRow row(2);
                        if(focus_project_rename) {ImGui::SetKeyboardFocusHere();focus_project_rename=false;}
                        const bool submitted=ui::InputText("###Image name",project_image_name,sizeof(project_image_name),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll);
                        const bool cancelled=ImGui::IsKeyPressed(ImGuiKey_Escape);
                        row.next();const bool apply=ui::IconButton("Rename image",ui::Icon::Check);row.next();
                        const bool cancel=ui::IconButton("Cancel",ui::Icon::Close);
                        if(submitted || apply) {
                            project_browser_enter=true;
                            try {options.project->rename_image(browsed->id,project_image_name);project_catalog_dirty=true;project_error.clear();rename_project_image=false;}
                            catch(const std::exception& e){{project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
                        }
                        if(cancelled || cancel) {rename_project_image=false;project_browser_enter=true;}
                    } else {
                        ui::design::Heading name;ImGui::PushTextWrapPos(0);ui::DataText(browsed->name.c_str());ImGui::PopTextWrapPos();
                    }
                    ui::TextDisabled("%lld x %lld pixels",static_cast<long long>(browsed->width),static_cast<long long>(browsed->height));
                    const auto selected_document=options.project->document(browsed->id);
                    if(selected_document->pixel_width_um() && selected_document->pixel_height_um())
                        ui::TextDisabled("%.6g x %.6g %s",*selected_document->pixel_width_um(),*selected_document->pixel_height_um(),ui::unit::um_per_px);
                    else ui::TextDisabled("Physical calibration unknown");
                    ImGui::Spacing();ui::ImageMetadata(*browsed);
                    ImGui::PopID();ImGui::PopID();
                }
            }
            ImGui::EndChild();ImGui::PopStyleVar(2);
            if(remove_image) {
                remove_project=options.project;
                remove_popup_requested=remove_dialog_active=true; pending_image=0;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
            }
            auto add_source=[&](const std::string& path) {
                const auto route=detail::desktop_add_selection(path,reader_names[image_reader]);
                if(route.raw_route)raw.open_image(*options.project,path,options.tiff_options?*options.tiff_options:TiffOpenOptions{},route.selection);
                else {pending_image=options.project->add_image(path,route.selection.provider_id);forms.image.set("");}
                project_catalog_dirty=true;project_error.clear();
            };
            if(forms.adding() && !editing_blocked && !project_load.valid() && !save_job.busy() && queued_raw_path.empty())forms.process_one([&](const std::string& path) {
                const auto route=detail::desktop_add_selection(path,reader_names[image_reader]);
                if(route.raw_route){raw.open_image(*options.project,path,options.tiff_options?*options.tiff_options:TiffOpenOptions{},route.selection);queued_raw_path=path;return false;}
                add_source(path);return true;
            });
            else if(add_image) {
                try{add_source(forms.image.state().resolved);select_project_form(ProjectForm::None);}
                catch(const std::exception& e){{project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
            }
            if(relink_image) {
                try {const auto target=relink_image_id?relink_image_id:active_image;options.project->relink(target,forms.image.state().resolved);if(target==active_image){mean.invalidate();pending_image=active_image;refresh_active_image=true;}project_catalog_dirty=true;project_error.clear();select_project_form(ProjectForm::None);relink_image_id=0;}
                catch(const std::exception& e) { {project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));} }
            }
            bool recover_requested=!editing_blocked && ui::chord_pressed(ui::CommandId::Key_Recover,ui::frame_commands());
            bool review_checkpoint=!editing_blocked && ui::chord_pressed(ui::CommandId::Key_ReviewCheckpoint,ui::frame_commands());
            bool find_recoveries=!editing_blocked && ui::chord_pressed(ui::CommandId::Key_FindRecoveries,ui::frame_commands());
            autosave_requested=!editing_blocked && ui::chord_pressed(ui::CommandId::Key_Autosave,ui::frame_commands());
            if(!editing_blocked && ui::chord_pressed(ui::CommandId::Key_StopRecoveryMaintenance,ui::frame_commands()) && autosave.busy() && autosave_stats.checkpoint_published) autosave.cancel();
            if(reveal_recoveries) {project_settings_visible=true;project_settings_section=2;reveal_recoveries=false;}
            if(ui::layout_trace && ui::layout_trace->project_settings)ui::layout_trace->project_settings(project_settings_visible,project_settings_section);
            if(project_settings_visible) {
                ui::LayoutSurface settings_surface(project_settings_section==0?"settings/Metadata":project_settings_section==1?"settings/Classes":"settings/Recovery");
                ui::SetNextToolWindow("Project settings",{480,640});
                if(ImGui::Begin(ui::i18n::label("Project settings").c_str(),&project_settings_visible,ImGuiWindowFlags_NoCollapse)) {
            ui::SetNextItemWidth(-1);ui::Combo("Section",&project_settings_section,"Metadata\0Classes and defaults\0Recovery\0");
            if(project_settings_section==0) {
            ui::TextWrapped("Project metadata describes the study. Image metadata belongs to each image entry.");
            ImGui::BeginDisabled(project_load.valid());
            if(ui::Button("Edit project metadata")) {
                try {entry_editor.open_project(options.project);}
                catch(const std::exception& e){{project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));}}
            }
            ImGui::EndDisabled();
            }
            if(project_settings_section==1)project_class_editor->draw(options.project,nullptr,active_image);
            if(project_settings_section==2) {
                if(!options.project->recovery_source().empty() && options.project->dirty()) ui::TextWrapped("Recovered workspace. Save this project to keep its changes.");
                ui::TextWrapped("Recovery preserves unsaved work separately from Save project.");
                ui::SetNextItemWidth(-1);
                if(ui::InputDouble("Interval (seconds)",&autosave_interval,10,60,"%.1f")) {
                    if(!std::isfinite(autosave_interval) || autosave_interval<0 || autosave_interval>86400) autosave_interval=60;
                    next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
                }
                ui::FieldHelp("0 disables periodic recovery.");
                paths.autosave.draw(path_context,false,{},[&](bool){
                ImGui::BeginDisabled(autosave.busy());
                if(ui::icon_action("Save recovery now (F10)",ui::Icon::Save,ui::CommandId::Key_Autosave,{!autosave.busy(),"Wait for the current recovery save to finish."})) autosave_requested=true;
                ImGui::EndDisabled();},"Recovery file");
                bool retention_changed=ui::Checkbox("Remove old inactive checkpoints",&retention.enabled);
                if(retention.enabled) {
                    int keep=static_cast<int>(retention.keep_latest);
                    ui::SetNextItemWidth(-1);
                    if(ui::InputInt("Keep newest",&keep)) {retention.keep_latest=static_cast<std::size_t>(std::clamp(keep,1,1024)); retention_changed=true;}
                    ui::SetNextItemWidth(-1);
                    if(ui::InputDouble("Minimum age (days)",&retention.max_age_days,1,7,"%.1f")) {
                        if(!std::isfinite(retention.max_age_days)) retention.max_age_days=30;
                        retention.max_age_days=std::clamp(retention.max_age_days,1.,36500.); retention_changed=true;
                    }
                    ui::TextWrapped("After recovery saves, keep recent checkpoints and active sessions; collect at most 32 old inactive files.");
                }
                if(retention_changed) autosave.set_retention(retention);
                const ui::Availability checkpoint_available=(autosave.busy() || project_load.valid() || save_job.busy() || maintenance_job.valid())?
                    ui::Availability{false,"Wait for the current work to finish."}:!*recovery_path?
                    ui::Availability{false,"Select a checkpoint or choose a checkpoint file."}:ui::Availability{};
                {ui::CaptionedStrip checkpoints("Recovery checkpoints",3,0,true);
                checkpoints.next();if(ui::icon_action("Find recovery files (Ctrl+Shift+F5)",ui::Icon::Refresh,ui::CommandId::Key_FindRecoveries)) find_recoveries=true;
                checkpoints.next();if(ui::icon_action("Open recovery (F11)",ui::Icon::Open,ui::CommandId::Key_Recover,checkpoint_available)) recover_requested=true;
                checkpoints.destructive();if(ui::icon_action("Delete selected checkpoint...",ui::Icon::Trash,ui::CommandId::Key_ReviewCheckpoint,checkpoint_available)) review_checkpoint=true;}
                if(recovery_candidates.empty())ui::TextDisabled("No recovery checkpoints. Use %s.",ui::i18n::text(ui::command(ui::CommandId::Key_FindRecoveries).caption));
                for(const auto& item:recovery_candidates) {
                    ImGui::PushID(item.path.c_str());
                    auto label=recovery_label(item);
                    if(ui::Selectable(ui::data_label(label,"candidate").c_str(),item.path==recovery_path)) {
                        paths.recovery.set(item.path); paths.recovery_primary.set(item.associated_base_path);
                    }
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",item.path.c_str());
                    if(item.base_status=="changed") ui::TextWrapped("Saved project changed; recovered work needs a new destination.");
                    if(item.base_status=="relocated") ui::TextWrapped("Checkpoint found by project identity at this location.");
                    if(!item.error.empty()) ui::TextWrapped("%s",item.error.c_str());
                    ImGui::PopID();
                }
                paths.recovery.draw(path_context,false,{},{},"Checkpoint file");
                paths.recovery_primary.draw(path_context,false,{},{},"Primary location (optional)");
                if(autosave.busy() && autosave_stats.checkpoint_published) {
                    if(autosave_stats.cancel_requested) ui::TextDisabled("Stopping recovery maintenance...");
                    else if(autosave_stats.maintenance_phase=="scanning") {ui::TextDisabled("Recovery saved."); ui::TextDisabled("Checked %zu checkpoint headers...",autosave_stats.retention_inspected);}
                    else if(autosave_stats.maintenance_phase=="collecting") {ui::TextDisabled("Recovery saved."); ui::TextDisabled("Removed %zu old checkpoints...",autosave_stats.retention_removed);}
                    else ui::TextDisabled("Recovery saved. Finishing maintenance...");
                    ImGui::BeginDisabled(autosave_stats.cancel_requested);
                    if(ui::Button(ui::presentation_label(ui::CommandId::Key_StopRecoveryMaintenance,"Stop maintenance (Ctrl+F10)"))) autosave.cancel();
                    ui::trace_workflow_action("Stop maintenance (Ctrl+F10)",ui::command(ui::CommandId::Key_StopRecoveryMaintenance).caption);
                    ImGui::EndDisabled();
                }
                else if(autosave.busy()) ui::TextDisabled("Writing recovery...");
                else if(autosave_stats.state=="ready") ui::TextDisabled("Recovery saved; project still unsaved.");
                if(!autosave.busy() && autosave_stats.retention_cancelled) ui::TextWrapped("Maintenance stopped. Saved recovery kept.");
                if(!autosave_stats.error.empty()) ui::TextWrapped("%s",autosave_stats.error.c_str());
                if(!autosave_stats.work_cleanup_error.empty()) ui::TextWrapped("Recovery work cleanup: %s",autosave_stats.work_cleanup_error.c_str());
                if(!autosave_stats.retention_error.empty()) ui::TextWrapped("Recovery retention: %s",autosave_stats.retention_error.c_str());
                if(!autosave_ui_error.empty()) ui::TextWrapped("%s",autosave_ui_error.c_str());
                if(maintenance_job.valid()) ui::TextDisabled("Working on recovery file...");
                if(!maintenance_error.empty()) ui::TextWrapped("%s",maintenance_error.c_str());
            }
                if(ui::layout_trace && ui::layout_trace->after_settings)ui::layout_trace->after_settings();
                }
                ImGui::End();
            }
            if(entry_editor.draw(options.project,project_browser.selected,editing_is_blocked() || project_load.valid()))project_catalog_dirty=true;
            if(find_recoveries && !recovery_scan.valid()) {
                auto base=options.project->path(); recovery_scan_base=base; recovery_scan_generation=options.project->generation(); recovery_scan_epoch=workspace_epoch; recovery_scan_revision=recovery_catalog_revision;
                recovery_scan=std::async(std::launch::async,[base]{return Project::find_recoveries(base);});
            }
            if(recover_requested && !autosave.busy() && !project_load.valid() && !save_job.busy() && !maintenance_job.valid()) {
                try {
                    project_load_cancelled=false; loading_recovery=true;
                    project_load=std::async(std::launch::async,load_project,paths.recovery.state().resolved,true,paths.recovery_primary.state().resolved);
                    project_error.clear();
                } catch(const std::exception& e) { {project_error=e.what();if(!project_error.empty())notifications.post(ui::exception_notice("Project",e,{}));} }
            }
            if(review_checkpoint && !autosave.busy() && !project_load.valid() && !save_job.busy() && !maintenance_job.valid()) {
                const auto path=paths.recovery.state().resolved; maintenance_epoch=workspace_epoch; maintenance_error.clear();
                maintenance_job=std::async(std::launch::async,[path] {
                    MaintenanceResult result;
                    try {result.reviewed=Project::review_recovery(path);} catch(const std::exception& e) {result.error=e.what();}
                    return result;
                });
            }
        }
        if(workspace.enabled()) {ImGui::End();workspace.begin(GuiWorkspace::image_pane,workspace.display);pane_translation.select("pane/Image");workspace.title("Image");}
        ImDrawList* overview_draw=nullptr;
        ImVec2 overview_clip_min{},overview_clip_max{};
        auto overview_navigation=[&](float width) {
            if(!overview.ready()) {overview_size={0,0};overview_draw=nullptr;return;}
            overview_size.x=width;overview_size.y=width*.6f;
            overview_offset=ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("overview navigation",overview_size);
            if(overview.state()=="failed" && ImGui::IsItemHovered()) ImGui::SetTooltip("%s",overview.error().c_str());
            overview_draw=ImGui::GetWindowDrawList();
            overview_clip_min=overview_draw->GetClipRectMin();overview_clip_max=overview_draw->GetClipRectMax();
            overview_view.fit(image.width,image.height,overview_size.x,overview_size.y);
            if(overview.ready() && ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
                auto center=overview_view.screen_to_image({io.MousePos.x-overview_offset.x,io.MousePos.y-overview_offset.y});
                overview_center=Point{std::clamp(center.x,0.,static_cast<double>(image.width)),std::clamp(center.y,0.,static_cast<double>(image.height))};
            }
        };
        if(has_image) {
        std::string image_name;
        for(const auto& entry:project_images)if(entry.id==active_image){image_name=entry.name;break;}
        const auto image_locator=document.server()->source_locator();
        if(image_name.empty())image_name=image_locator.empty()?ui::i18n::text("Current image"):std::filesystem::u8path(image_locator).filename().u8string();
        {ui::ActionRow row(1);
        {ui::design::Heading name;ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row.field_width());ui::DataText(image_name.c_str());ImGui::PopTextWrapPos();}
        row.next();if(ui::IconButton("Image information",ui::Icon::Info))open_image_information(false);}
        ui::TextDisabled("%lld x %lld pixels",static_cast<long long>(image.width),static_cast<long long>(image.height));
        ImGui::Spacing();
        {ui::ActionRow row(1);ui::PaneTitle("Display");row.next();
        if(ui::IconButton("Reset display",ui::Icon::Reset))display=DisplaySettings{};
        display_reset_offset=ImGui::GetItemRectMin();const auto hi=ImGui::GetItemRectMax();display_reset_size={hi.x-display_reset_offset.x,hi.y-display_reset_offset.y};mark_target(GuiInputTarget::DisplayReset);}
        int mode=static_cast<int>(display.mode);
        if (ui::Combo("Display mode###Channels",&mode,"RGB\0Grayscale\0Red\0Green\0Blue\0")) display.mode=static_cast<DisplayMode>(mode);
        float black=static_cast<float>(display.black),white=static_cast<float>(display.white),gamma=static_cast<float>(display.gamma);
        if (ui::SliderFloat("Black",&black,0,254,"%.0f")) display.black=std::min<double>(black,std::max(0.,display.white-.001));
        if (ui::SliderFloat("White",&white,1,255,"%.0f")) display.white=std::max<double>(white,std::min(255.,display.black+.001));
        if (ui::SliderFloat("Gamma",&gamma,.1f,5,"%.2f")) display.gamma=gamma;
        tiles->set_display(display);overview.update(active_document->server(),display);
        ImGui::Spacing();
        {ui::ActionRow row(1);ui::PaneTitle("Calibration");row.next();
        if(ui::IconButton("Edit calibration",ui::Icon::Edit))calibration_editor.open(document,active_image,workspace_epoch);}
        if(document.pixel_width_um() && document.pixel_height_um())ui::TextWrapped("%.6g x %.6g %s",*document.pixel_width_um(),*document.pixel_height_um(),ui::unit::um_per_px);
        else ui::TextDisabled("Physical calibration unknown");
        if(const auto& settings=document.image_analysis_settings();settings) {
            ui::SeparatorText("Kept analysis profile");
            ui::DataTextWrapped("%s",settings->has_stains()?settings->profile_name().c_str():settings->image_type().c_str());
            ui::TextDisabled("Applied only on request");
            if(ui::Button("Analysis settings"))focus_task(Task::Region);
        }
        if(overview.state()=="failed") {
            ui::SeparatorText("Overview unavailable");
            ui::TextWrapped("%s",overview.error().c_str());
            {ui::ActionRow limits(2);ui::TextDisabled("Source buffer limit: %.3g MiB",overview.source_pixel_limit()*4./(1024*1024));
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Decoder buffers and output use additional memory.");
            limits.next();ImGui::BeginDisabled(overview.source_pixel_limit()>=64ULL*1024*1024);
            if(ui::IconButton("Increase limit",ui::Icon::Plus,ui::Availability{overview.source_pixel_limit()<64ULL*1024*1024,"The maximum source buffer limit is selected."}))overview.set_source_pixel_limit(std::min<std::uint64_t>(64ULL*1024*1024,std::max<std::uint64_t>(4ULL*1024*1024,overview.source_pixel_limit()*2)));
            overview_more_offset=ImGui::GetItemRectMin();auto hi=ImGui::GetItemRectMax();overview_more_size={hi.x-overview_more_offset.x,hi.y-overview_more_offset.y};mark_target(GuiInputTarget::OverviewBudgetIncrease);ImGui::EndDisabled();
            limits.next();ImGui::BeginDisabled(overview.source_pixel_limit()<=4ULL*1024*1024);
            if(ui::IconButton("Decrease limit",ui::Icon::Minus,ui::Availability{overview.source_pixel_limit()>4ULL*1024*1024,"The minimum source buffer limit is selected."}))overview.set_source_pixel_limit(std::max<std::uint64_t>(4ULL*1024*1024,overview.source_pixel_limit()/2));
            overview_less_offset=ImGui::GetItemRectMin();hi=ImGui::GetItemRectMax();overview_less_size={hi.x-overview_less_offset.x,hi.y-overview_less_offset.y};mark_target(GuiInputTarget::OverviewBudgetDecrease);ImGui::EndDisabled();}
        }
        if(!workspace.enabled()) overview_navigation(std::min(250.f*ui_scale.logical,ui::available()));
        if(workspace.enabled()) {ImGui::End();workspace.begin(GuiWorkspace::objects_pane,workspace.objects);pane_translation.select("pane/Annotations");}
        if(workspace.enabled()) workspace.title("Annotations");else ui::SeparatorText("Objects");
        int list_filter=annotations_only?0:1;if(ui::Combo("Show",&list_filter,"Annotations\0All objects\0"))annotations_only=list_filter==0;
        if (list_revision!=document.object_visibility_revision() || list_annotations_only!=annotations_only) {
            list_ids.clear(); list_ids.reserve(document.object_count());
            for (const auto& [id,a]:document.objects()) if(document.object_visible(id) && (!annotations_only || a.kind==ObjectKind::Annotation))list_ids.push_back(id);
            list_revision=document.object_visibility_revision();list_annotations_only=annotations_only;filtered_selection_revision=~std::uint64_t(0);
        }
        if(filtered_selection_revision!=selection->revision() || filtered_document_revision!=document.revision()) {
            selection_not_shown=0;for(auto id:selection->ids())if(!std::binary_search(list_ids.begin(),list_ids.end(),id))++selection_not_shown;
            filtered_selection_revision=selection->revision();filtered_document_revision=document.revision();
        }
        if(selection_not_shown) {ui::TextWrapped("%zu selected / %zu not shown by filter",selection->size(),selection_not_shown);if(ui::Button("Show all objects"))annotations_only=false;}
        {ui::LayoutRegion object_list_region("Objects");ui::PaneTitle("Objects");
        ui::BeginCollection("Object list",list_ids.size());
        if(list_ids.empty())ui::TextDisabled("No objects. Draw an annotation to fill this list.");
        list_offset=ImGui::GetCursorScreenPos(); list_size=ImGui::GetContentRegionAvail();
        ImGuiListClipper clipper; clipper.Begin(static_cast<int>(list_ids.size()));
        while(clipper.Step()) for (int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
            const auto& a=document.objects().at(list_ids[row]);
            auto label = std::string("###Object/")+std::to_string(a.id);
            if (ui::Selectable(label.c_str(), selection->contains(a.id),0,{0,ImGui::GetTextLineHeight()})) {
                if(io.KeyShift) {const bool anchor_shown=std::find(list_ids.begin(),list_ids.end(),list_anchor)!=list_ids.end();detail::select_ordered_range(*selection,list_ids,list_anchor,a.id,io.KeyCtrl);if(!anchor_shown)list_anchor=a.id;}
                else {
                    if(io.KeyCtrl) selection->toggle(a.id); else selection->replace({a.id});
                    list_anchor=a.id;
                }
                selected=selection->primary();
            }
            const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();const float side=ImGui::GetFontSize();
            ImGui::GetWindowDrawList()->PushClipRect({lo.x+side*2,lo.y},hi,true);ImGui::GetWindowDrawList()->AddText({lo.x+side*2,lo.y},ImGui::GetColorU32(ImGuiCol_Text),a.name.c_str());ImGui::GetWindowDrawList()->PopClipRect();
            const char* kind=a.kind==ObjectKind::Cell?"C":a.kind==ObjectKind::Detection?"D":"A";
            ImGui::GetWindowDrawList()->AddText({lo.x+2,lo.y},ImGui::GetColorU32(ImGuiCol_TextDisabled),kind);
            std::uint32_t rgb=0x41e1be;const auto color=document.classes().find(a.class_name);if(color!=document.classes().end())rgb=color->second.color;if(const auto override_rgb=a.color_rgb())rgb=*override_rgb;
            ImGui::GetWindowDrawList()->AddRectFilled({lo.x+side,lo.y+side*.3f},{lo.x+side*1.4f,lo.y+side*.7f},IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255),1);
            if(ImGui::IsItemHovered()) {ImGui::BeginTooltip();ui::DataText(a.name.c_str());ui::TextUnformatted(a.kind==ObjectKind::Cell?"Cell":a.kind==ObjectKind::Detection?"Detection":"Annotation");ui::DataTextWrapped("Class: %s",a.class_name.empty()?ui::i18n::text("Unclassified"):a.class_name.c_str());ui::TextUnformatted(a.color_rgb()?"Appearance: object override":"Appearance: class or default");ImGui::EndTooltip();}
        }
        ImGui::EndChild();}
        {
            ui::ActionRow row(selected?2:0,0,bool(selected));ImGui::AlignTextToFramePadding();
            ui::Text("Selected: %zu",selection->size());
            if(selected) {
                row.next();const bool clear_selection_requested=ui::IconButton("Clear selection",ui::Icon::Deselect);
                row.destructive();if(ui::icon_action("Delete selected",ui::Icon::Trash,ui::CommandId::Key_Delete)) delete_selection();
                if(clear_selection_requested) { selection->clear(); selected=0; }
            }
        }
        if(!selected) ui::TextWrapped("Select an annotation, or draw a region on the image.");
        if(selection->size()>1) {
            if(ui::ComboScope combo{"Batch class",batch_class.empty()?"Unclassified":batch_class.c_str()};combo) {
                if(ui::Selectable("Unclassified",batch_class.empty())) batch_class.clear();
                for(const auto& [name,c]:document.classes()) if(ui::Selectable(ui::data_label(name,std::to_string(reinterpret_cast<std::uintptr_t>(&c))).c_str(),name==batch_class)) batch_class=name;
            }
            if(ui::Button("Classify selected")) {
                try { document.classify_objects(selection->ids(),batch_class); notifications.resolve("Annotations"); }
                catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
            }
            const bool geometry_key=!editing_blocked && (ui::chord_pressed(ui::CommandId::Action_Apply_Boolean_B,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Merge_annotations_M,ui::frame_commands()));
            if(geometry_key) ImGui::SetNextItemOpen(true);
            if(!workspace.enabled() || ui::CollapsingHeader("Combine annotations")) {
            if(has_geos()) {
                ui::SetNextItemWidth(140);
                ui::Combo("Boolean",&boolean_operation,"Union\0Intersection\0Difference\0XOR\0");
                ui::TextDisabled("Target: primary; operands kept");
                ui::TextWrapped("Ellipses use 512-edge approximations.");
                if(ui::WorkflowAction("Apply Boolean (B)") || (!editing_blocked && ui::chord_pressed(ui::CommandId::Action_Apply_Boolean_B,ui::frame_commands()))) {
                    try {
                        auto ids=selection->ids(); ids.erase(std::remove(ids.begin(),ids.end(),selected),ids.end());
                        if(document.annotation(selected).locked())throw ui::NoticeException<std::invalid_argument>("Unlock the ROI before editing its geometry");
                        objects_visible=true;document.apply_boolean(selected,ids,static_cast<BooleanOp>(boolean_operation)); notifications.resolve("Annotations");
                    } catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
                }
            }
            const std::array<std::uint64_t,4> stamp{{workspace_epoch,active_image,document.render_revision(),selection->revision()}};
            if(stamp!=merge_stamp) {
                merge_areas=false;merge_points=false;discard_merge_points=false;
                for(auto id:selection->ids()) {const auto shape=document.annotation(id).geometry;merge_areas|=shape.is_area();merge_points|=shape.kind()==RoiKind::Points;}
                merge_stamp=stamp;
            }
            if(merge_areas && merge_points) ui::Checkbox("Discard point ROIs when merging into an area",&discard_merge_points);
            else discard_merge_points=false;
            ui::TextDisabled("Merge replaces inputs; children remain at root");
            if(ui::WorkflowAction("Merge annotations (M)") || (!editing_blocked && ui::chord_pressed(ui::CommandId::Action_Merge_annotations_M,ui::frame_commands()))) {
                try {
                    for(auto id:selection->ids())if(document.annotation(id).locked())throw ui::NoticeException<std::invalid_argument>("Unlock ROIs before merging");
                    objects_visible=true;const auto id=document.merge_annotations(selection->ids(),discard_merge_points);
                    selection->replace({id},id);selected=id;discard_merge_points=false;notifications.resolve("Annotations");
                } catch(const std::exception& e) {notifications.post(ui::exception_notice("Annotations",e,{}));}
            }
            ui::TextDisabled("Inspector edits the primary object");
            }
        }
        auto commit_name=[&] {
            if(!editor_id || object_baseline_name==draft_name)return;
            try {if(document.annotation(editor_id).name!=draft_name)document.rename_annotation(editor_id,draft_name);notifications.resolve("Annotations");}
            catch(const std::exception& e) {notifications.post(ui::exception_notice("Annotations",e,{}));}
            try {object_baseline_name=document.annotation(editor_id).name;std::memcpy(draft_name,object_baseline_name.c_str(),object_baseline_name.size()+1);}catch(...) {}
        };
        if(editor_id!=selected) {
            // Selection can change before this pane draws. The text still belongs
            // to editor_id, never to the newly selected object.
            commit_name();
            detail::retire_draft_text(editor_text);
            geometry_discarded|=editor_id && geometry_draft.dirty();appearance_discarded|=editor_id && appearance_draft.dirty();
            geometry_draft.reset({});appearance_draft.reset({});
        }
        if(!selected)editor_id=0;
        if (selected) {
            if (editor_id!=selected || editor_revision!=document.revision()) {
                const auto a=document.annotation(selected);
                const detail::GeometryDraft geometry{a.geometry.bounds(),a.parent_id};const auto appearance=detail::AppearanceDraft::from(a);
                if(editor_id!=selected) {geometry_draft.reset(geometry);appearance_draft.reset(appearance);}
                else {detail::rebase_geometry(geometry_draft,geometry);detail::rebase_appearance(appearance_draft,appearance);}
                if(editor_id!=selected || object_baseline_name==draft_name)std::memcpy(draft_name,a.name.c_str(),a.name.size()+1);
                object_baseline_name=a.name;std::memcpy(draft_class,a.class_name.c_str(),a.class_name.size()+1);
                editor_id=selected; editor_revision=document.revision();
            }
            const auto object=document.annotation(selected);
            ui::SeparatorText("Selected object");
            if(workspace.enabled()) {detail::draw_shape_measurements(document,selected,selection->size());ui::Separator();}
            const auto name_input_id=ui::field_input_id("Name");
            if(ImGui::GetActiveID()==name_input_id)if(auto* state=ImGui::GetInputTextState(name_input_id)) {
                // Escape restores the current native name, including a rename
                // made elsewhere while this field still owns focus.
                state->TextToRevertTo.resize(static_cast<int>(object.name.size()+1));
                std::memcpy(state->TextToRevertTo.Data,object.name.c_str(),object.name.size()+1);
            }
            const std::string before_name=draft_name;
            const bool name_enter=ui::InputText("Name",draft_name,sizeof(draft_name),ImGuiInputTextFlags_EnterReturnsTrue);
            editor_text.observe();
            if(before_name!=draft_name)geometry_discarded=appearance_discarded=false;
            if(name_enter || ImGui::IsItemDeactivatedAfterEdit())commit_name();
            ui::trace_workflow_action("Object name","Object name");
            if(ui::ComboScope combo{"Class",draft_class[0]?draft_class:"Unclassified"};combo) {
                auto classify=[&](const std::string& name) {geometry_discarded=appearance_discarded=false;try {if(document.annotation(selected).class_name!=name)document.classify_annotation(selected,name);notifications.resolve("Annotations");}catch(const std::exception& e){notifications.post(ui::exception_notice("Annotations",e,{}));}};
                if(ui::Selectable("Unclassified",!draft_class[0]))classify("");
                for(const auto& [name,c]:document.classes()) if(ui::Selectable(ui::data_label(name,std::to_string(reinterpret_cast<std::uintptr_t>(&c))).c_str(),name==draft_class))classify(name);
            }
            const auto before_geometry=geometry_draft.value;
            if(ui::CollapsingHeader("Geometry")) {
            ImGui::BeginDisabled(object.is_detection() || document.annotation(selected).locked());
            ui::DraftInputDouble("X",&draft_roi.x,geometry_inputs[0]);editor_text.observe();ui::DraftInputDouble("Y",&draft_roi.y,geometry_inputs[1]);editor_text.observe();
            ImGui::BeginDisabled(object.geometry.kind()!=RoiKind::Rectangle && object.geometry.kind()!=RoiKind::Ellipse);
            ui::DraftInputDouble("Width",&draft_roi.width,geometry_inputs[2]);editor_text.observe();ui::DraftInputDouble("Height",&draft_roi.height,geometry_inputs[3]);editor_text.observe();ImGui::EndDisabled();ImGui::EndDisabled();
            std::string parent_caption=ui::i18n::text("No parent");
            if(draft_parent) {const auto parent=document.objects().find(draft_parent);if(parent!=document.objects().end())parent_caption=parent->second.name+" (#"+std::to_string(parent->first)+")";else parent_caption="#"+std::to_string(draft_parent);}
            if(ui::ComboScope combo{"Parent ID",""};combo) {
                if(ImGui::IsWindowAppearing() || parent_choices.revision!=document.revision() || parent_choices.object!=selected)parent_choices.rebuild(document,selected);
                if(ui::Selectable(ui::i18n::text("No parent"),!draft_parent))draft_parent=0;
                parent_choices.draw(draft_parent);
            }
            if(ImGui::BeginComboPreview()) {ui::DataText(parent_caption.c_str());ImGui::EndComboPreview();}
            if(detail::draft_editor_trace && detail::draft_editor_trace->parent_choices)detail::draft_editor_trace->parent_choices(parent_choices);
            }
            if(!(before_geometry==geometry_draft.value))geometry_discarded=appearance_discarded=false;
            if(geometry_discarded)ui::TextWrapped("Unapplied geometry changes were discarded.");
            const auto current_bounds=object.geometry.bounds();
            const bool resizable=object.geometry.kind()==RoiKind::Rectangle || object.geometry.kind()==RoiKind::Ellipse;
            const bool object_edited=draft_parent!=object.parent_id || draft_roi.x!=current_bounds.x || draft_roi.y!=current_bounds.y ||
                (resizable && (draft_roi.width!=current_bounds.width || draft_roi.height!=current_bounds.height));
            const auto geometry_available=ui::DraftAvailability(object_edited);
            if (ui::WorkflowAction("Apply object edit",ui::design::Action::Primary,geometry_available)) {
                try {
                    const auto current=document.annotation(selected);const auto geometry=geometry_draft.value.apply(current.geometry);
                    if(current.locked() && !geometry.equals(current.geometry))throw ui::NoticeException<std::invalid_argument>("Unlock the ROI before editing its geometry");
                    objects_visible=true;document.execute({ObjectEdit::replace_geometry(selected,geometry,current.name,draft_parent,current.class_name,current.kind,current.nucleus)},"Edit annotation");
                    geometry_draft.reset({geometry.bounds(),draft_parent});geometry_discarded=false;notifications.resolve("Annotations");
                }
                catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
            }
            ui::WorkflowReason("Apply object edit",geometry_available);
            auto mark_state=[&](GuiInputTarget target,std::size_t index) {const auto at=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();object_state_bounds[index]={at.x,at.y,size.x,size.y};mark_target(target);};
            bool locked=object.locked();
            if(ui::Checkbox("Lock ROI geometry",&locked)) {geometry_discarded=appearance_discarded=false;try {document.set_object_edit_state(selected,locked,object.color_rgb(),object.class_probability());notifications.resolve("Annotations");}catch(const std::exception& e){notifications.post(ui::exception_notice("Annotations",e,{}));}}
            mark_state(GuiInputTarget::ToggleObjectLock,0);
            if(ui::WorkflowAction("Object information")) {
                object_info_snapshot=object;object_info_document=active_document;object_info_epoch=workspace_epoch;
                object_info_search[0]=0;object_info_requested=object_info_active=true;
            }
            ui::SeparatorText("Color and reported probability");
            const auto before_appearance=appearance_draft.value;
            ui::Checkbox("Use object color override",&draft_use_object_color);ui::trace_workflow_action("Use object color override","Use object color override");
            if(draft_use_object_color) {ui::ColorEdit3("Object RGB",draft_object_color.data());editor_text.observe();}
            ui::Checkbox("Keep reported class probability###Retain reported class probability",&draft_use_class_probability);ui::trace_workflow_action("Retain reported class probability","Keep reported class probability");
            if(draft_use_class_probability) {ui::DraftInputDouble("Reported probability",&draft_class_probability,probability_input);editor_text.observe();}
            const auto appearance_byte=[](float v){return std::uint32_t(std::clamp(v,0.f,1.f)*255+.5f);};
            const auto appearance_rgb=(appearance_byte(draft_object_color[0])<<16)|(appearance_byte(draft_object_color[1])<<8)|appearance_byte(draft_object_color[2]);
            const bool appearance_edited=(draft_use_object_color?std::optional<std::uint32_t>(appearance_rgb):std::nullopt)!=object.color_rgb() ||
                (draft_use_class_probability?std::optional<double>(draft_class_probability):std::nullopt)!=object.class_probability();
            if(!(before_appearance==appearance_draft.value))geometry_discarded=appearance_discarded=false;
            if(appearance_discarded)ui::TextWrapped("Unapplied changes were discarded.");
            if(ui::WorkflowAction("Apply###Apply object display state",ui::design::Action::Secondary,ui::DraftAvailability(appearance_edited))) {try {
                const auto byte=[](float v){return std::uint32_t(std::clamp(v,0.f,1.f)*255+.5f);};
                const auto rgb=(byte(draft_object_color[0])<<16)|(byte(draft_object_color[1])<<8)|byte(draft_object_color[2]);
                document.set_object_edit_state(selected,document.annotation(selected).locked(),draft_use_object_color?std::optional<std::uint32_t>(rgb):std::nullopt,draft_use_class_probability?std::optional<double>(draft_class_probability):std::nullopt);appearance_draft.reset(detail::AppearanceDraft::from(document.annotation(selected)));appearance_discarded=false;notifications.resolve("Annotations");
            }catch(const std::exception& e){notifications.post(ui::exception_notice("Annotations",e,{}));}}
            mark_state(GuiInputTarget::ApplyObjectEditState,1);
            if(appearance_edited)ui::TextDisabled("Color or probability changes have not been applied.");
            ui::SeparatorText("Object operations");
            if(ui::WorkflowAction("Clear properties and unlock (Undo)",ui::design::Action::Secondary,ui::ObjectPropertiesAvailability(bool(object.properties)))) {try {document.set_object_properties(selected,nullptr);notifications.resolve("Annotations");}catch(const std::exception& e){notifications.post(ui::exception_notice("Annotations",e,{}));}}
            mark_state(GuiInputTarget::ClearObjectProperties,2);
            if(tool==6) ui::TextWrapped("Drag a vertex; I near an edge inserts; Delete removes the active vertex. Escape cancels a drag.");
            if(tool==7) ui::TextWrapped("Click an interior ring, then Enter to add a hole. Escape cancels.");
            if(object.kind==ObjectKind::Annotation && has_geos() &&
                (object.geometry.kind()==RoiKind::Rectangle || object.geometry.kind()==RoiKind::Ellipse)) {
                if(object.geometry.kind()==RoiKind::Ellipse) ui::TextWrapped("Polygon conversion uses 512 edges.");
                ImGui::BeginDisabled(document.annotation(selected).locked());
                if(ui::Button("Convert to polygon")) {
                    try { document.update_roi(selected,object.geometry.as_polygon()); notifications.resolve("Annotations"); }
                    catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
                }
                ImGui::EndDisabled();
            }
        }
        if(!selected) {
            if(geometry_discarded)ui::TextWrapped("Unapplied geometry changes were discarded.");
            if(appearance_discarded)ui::TextWrapped("Unapplied changes were discarded.");
        }
        if(detail::draft_editor_trace && detail::draft_editor_trace->after_editor)
            detail::draft_editor_trace->after_editor(geometry_draft,appearance_draft,*selection,draft_name,geometry_discarded,appearance_discarded);
        if(ui::CollapsingHeader("Classes"))image_class_editor->draw(options.project,&document,active_image);
        const bool exchange_shortcut=!editing_blocked && (ui::chord_pressed(ui::CommandId::Action_Export_GeoJSON_F6,ui::frame_commands()) || ui::chord_pressed(ui::CommandId::Action_Import_GeoJSON_F7,ui::frame_commands()));
        if(annotation_geojson_available && exchange_shortcut) ImGui::SetNextItemOpen(true);
        if(annotation_geojson_available && ui::CollapsingHeader("Import / export",options.interchange_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)) {
            if(exchange_shortcut) ImGui::SetScrollHereY(0);
            ui::Checkbox("Copy descendants",&clipboard_descendants);
            {ui::CaptionedStrip clipboard("Clipboard",3);
            auto mark_clipboard=[&](GuiInputTarget target,std::size_t i) {
                const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
                clipboard_bounds[i]={a.x,a.y,b.x-a.x,b.y-a.y};mark_target(target);
            };
            clipboard.next();ImGui::BeginDisabled(selection->ids().empty());
            if(ui::icon_action("Copy selected objects (Ctrl+C)",ui::Icon::Copy,ui::CommandId::Header_CopyObjects,{!selection->ids().empty(),"Select one or more objects first."}))copy_selection();
            mark_clipboard(GuiInputTarget::CopyObjects,0);ImGui::EndDisabled();
            clipboard.next();if(ui::icon_action("Paste objects (Ctrl+V)",ui::Icon::Paste,ui::CommandId::Header_PasteObjects))paste_clipboard(false);
            mark_clipboard(GuiInputTarget::PasteObjects,1);
            clipboard.next();if(ui::icon_action("Paste to displayed plane (Ctrl+Shift+V)",ui::Icon::Paste,ui::CommandId::Key_PasteCurrentPlane))paste_clipboard(true);
            mark_clipboard(GuiInputTarget::PasteCurrentPlane,2);}
            ui::TextWrapped("Image pixels; no physical rescaling. Paste keeps planes unless displayed plane is chosen.");
            ui::SeparatorText("Import GeoJSON");
            paths.import_source.draw(path_context,false,{},{},"Import source");
            ui::TextWrapped("Full-resolution image pixels, top-left origin.");
            ui::Checkbox("Assume image pixels",&interchange_pixels);
            ui::Checkbox("Allow lossy conversion",&interchange_losses);
            if(ui::WorkflowAction("Import GeoJSON (F7)"))import_objects();
            ui::SeparatorText("Export GeoJSON");
            paths.geojson.draw(path_context,false,{},{},"Export destination");
            ImGui::BeginDisabled(!has_geos());
            ui::Checkbox("Match QuPath ellipse bounds",&qupath_ellipse_precision);
            ImGui::EndDisabled();
            if(qupath_ellipse_precision) ui::TextWrapped("Coordinate changes require Allow lossy conversion. Native analytic hints remain exact.");
            ui::Checkbox("QuPath hierarchy (nested export)",&interchange_nested);
            ui::DataTextWrapped(interchange_nested?"Parent links use nested child objects. Maximum 32 object levels.":"Flat parent links are native extensions. QuPath imports these objects without parents.");
            ui::Checkbox("Selected objects only",&interchange_selected);
            ui::TextDisabled(interchange_selected?"Scope: selected objects":"Scope: current image");
            if(ui::WorkflowAction("Export GeoJSON (F6)"))begin_export(ExportOp::GeoJson);

            ui::SeparatorText("Export measurements CSV");
            paths.object_csv.draw(path_context,false,{},{},"Export destination");
            ui::Checkbox("Include mean RGB",&csv_intensity);
            if(ui::Button("Export measurements CSV"))begin_export(ExportOp::ObjectCsv);
            if(!interchange_status.empty()) ui::TextWrapped("%s",interchange_status.c_str());
        }
        if(workspace.enabled()) {ImGui::End();workspace.begin(GuiWorkspace::workflow_pane,workspace.measurements);pane_translation.select("pane/Workflow");}
        workflow_navigation();
        pane_translation.select((std::string("task/")+ui::workflow_task_keys[static_cast<std::size_t>(workflow_task)]).c_str());
        if(has_image && workspace.enabled() && workflow_task>=Task::Region && workflow_task<=Task::Cells) {
            const bool separate=workflow_task==Task::Region?inputs.kind==1:workflow_task==Task::Tissue?tissue_feature!=0:
                (cell_grid || cell_nominal)?grid_cells.feature!=TissueFeature::OpticalDensitySum:cell_feature!=0;
            auto key=sampling_key();key.append(workflow_task,separate);
            if(!active_inputs_key || active_inputs_key->values!=key.values) {
                const auto previous_error=active_inputs_error;
                active_inputs_key=std::move(key);active_inputs_transform.reset();active_inputs_error.clear();
                try {active_inputs_transform=make_transform(separate);}catch(const std::exception& e){active_inputs_error=e.what();if(active_inputs_error!=previous_error)notifications.post(ui::exception_notice(ui::workflow_task_labels[static_cast<std::size_t>(workflow_task)],e,{}));}
            }
            {ui::ActionRow row(1);ui::PaneTitle("Analysis inputs");row.next();
            if(ui::IconButton("Active input details",ui::Icon::Info))open_image_information(true,active_inputs_transform,separate);}
            if(active_inputs_transform) {
                const auto& transform=*active_inputs_transform;
                bool saved=false;if(const auto& settings=document.image_analysis_settings();settings && settings->has_stains())
                    saved=separate && transform.stains()==settings->basis() && transform.white()==settings->white();
                ui::TextDisabled(separate?(saved?"Stains match saved image profile":"Stains use session values"):"Optical density");
                const auto& w=transform.white();ui::TextDisabled("White reference");ui::TextDisabled("%.5g / %.5g / %.5g",w[0],w[1],w[2]);
            }
            if(workflow_task!=Task::Cells)ui::TextDisabled(inputs.nucleus?"Target: nucleus":"Target: object body");
            if(!active_inputs_error.empty())ui::TextWrapped("%s",active_inputs_error.c_str());
            if(workflow_task!=Task::Region) {
                auto& open=workflow_task==Task::Tissue?analysis_task_state.tissue_inputs_open:analysis_task_state.cell_inputs_open;
                if(ui::WorkflowAction("Edit analysis inputs",ui::design::Action::Ghost,detail::analysis_inputs_availability(editing_blocked)))open=!open;
                if(open) {
                    const auto editor=workflow_task==Task::Tissue?InputEditor::Tissue:InputEditor::Cells;
                    input_feedback.next_editor=editor;
                    detail::draw_analysis_inputs_editor(inputs,detail::AnalysisInputs::White|(separate?detail::AnalysisInputs::Vectors:0)|(workflow_task==Task::Tissue?detail::AnalysisInputs::Nucleus:0));
                    input_feedback.draw(editor);
                }
            }
            ui::Separator();
        }
        if(has_image && (!workspace.enabled() || workflow_task==Task::Region)) {
        if(const auto& settings=document.image_analysis_settings();settings) {
            {ui::ActionRow profile(1);ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+profile.field_width());
            ui::TextWrapped("Kept profile: %s",settings->has_stains()?settings->profile_name().c_str():settings->image_type().c_str());ImGui::PopTextWrapPos();
            profile.next();if(ui::WorkflowIcon("Details###Saved analysis details",ui::Icon::Info))open_image_information(true);}
            if(settings->has_stains()) {
                const auto profile_available=editing_blocked?ui::WorkflowBlocked():settings->rgb8_supported()?ui::Availability{}:ui::Availability{false,"White reference kept. Native RGB8 stain application requires each white channel in 1..255."};
                if(ui::WorkflowAction("Use saved stains for analysis",ui::design::Action::Secondary,profile_available)) {
                    input_feedback.next_editor=InputEditor::Profile;inputs.use_stains(settings->basis(),settings->white());notifications.resolve("Image");
                }
                const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();image_stains_bounds={a.x,a.y,b.x-a.x,b.y-a.y};mark_target(GuiInputTarget::UseImageStains);
                ui::WorkflowReason("Use saved stains for analysis",profile_available);
                input_feedback.draw(InputEditor::Profile);
                bool profile_matches=false;
                if(inputs.kind==1) {try {
                    const auto effective=make_transform(true);profile_matches=effective.stains()==settings->basis() && effective.white()==settings->white();
                }catch(const std::exception&) { /* Invalid current inputs are reported by their existing owner. */ }}
                ui::TextDisabled(profile_matches?"Current inputs match this profile":"Profile is not applied to current inputs");
            }
        }
        }
        mean.poll(selected,document.revision());
        auto analysis_status=analysis.poll(document,selected);
        if(pending_analysis_recheck && !analysis.busy()) {
            if(analysis_status.state=="ready") {
                try {recheck_analysis_generation=analysis.poll(document,selected).generation;recheck_analysis_id=pending_analysis_recheck;analysis.verify_result(document,pending_analysis_recheck);}
                catch(const std::exception& e) {const auto failed=analysis.poll(document,selected);notice_observers.notice_jobs[5].observe(notifications,ui::workflow_task_labels[1],failed.state,false,false,{}, {},failed.generation);notifications.post(ui::exception_notice(ui::workflow_task_labels[1],e,{}));}
            }
            pending_analysis_recheck=0;
        }
        if (selected) {
            if(tissue_grid) tissue_parameters.level=0;
            if(!workspace.enabled() || workflow_task==Task::Measurements) {
            if(!workspace.enabled()) ui::SeparatorText(selection->size()>1?"Measurements (primary object)":"Measurements");
            detail::draw_shape_measurements(document,selected,selection->size());
            const auto measured_cell=document.annotation(selected);
            if(measured_cell.cell_feature_link) {
                const auto link=*measured_cell.cell_feature_link;
                ui::SeparatorText("Captured cell features");
                ui::Text("Run: %llu / label: %u",static_cast<unsigned long long>(link.record_id),link.label);
                const auto status=document.cell_object_status(selected);ui::Text("Feature status: %s",ui::caption(ui::parse_record_state(status),status).c_str());
                if(status!="archive_missing" && status!="invalid_label") {
                    const auto f=document.cell_object_feature(selected);
                    ui::Text("Nucleus samples: %llu",static_cast<unsigned long long>(f.nucleus_pixels));
                    ui::Text("Cell samples: %llu",static_cast<unsigned long long>(f.cell_pixels));
                    ui::Text("Nucleus RGB: %.5g / %.5g / %.5g",f.nucleus_rgb[0],f.nucleus_rgb[1],f.nucleus_rgb[2]);
                    ui::Text("Cell RGB: %.5g / %.5g / %.5g",f.cell_rgb[0],f.cell_rgb[1],f.cell_rgb[2]);
                    ui::TextWrapped(ui::kept_samples_hint());
                }
            }
            if (mean.measured==selected && mean.value) ui::Text("Mean RGB: %.3f",*mean.value);
            ImGui::BeginDisabled(mean.active());
            if (ui::Button("Measure mean RGB")) mean.start(document.server(),document.annotation(selected),document.revision());
            ImGui::EndDisabled();
            if (mean.active()) ui::TextDisabled("Measuring...");
            if (!mean.error.empty()) ui::TextWrapped("%s",mean.error.c_str());
            }
            const std::function<StainTransform(bool)> task_make_transform=make_transform;
            const std::function<void(GuiInputTarget)> task_mark_target=mark_target;
            const std::function<void(int)> task_focus_tables=focus_tables;
            const std::function<TissueRequest()> task_build_tissue_request=build_tissue_request;
            const std::function<CellRequest()> task_build_cell_request=build_cell_request;
            const std::function<detail::SamplingInputKey()> task_sampling_key=sampling_key;
            const std::function<void(detail::GuiExportOperation,std::vector<std::uint64_t>)> task_begin_export=[&](auto operation,auto ids){begin_export(operation,std::move(ids),false,ui::workflow_task_labels[3]);};
            detail::analysis_tasks::TaskContext task_context{analysis_task_state,
                workspace,
                workflow_task,
                options,
                document,
                selected,
                io,
                editing_blocked,
                image,
                analysis,
                analysis_status,
                tissue,
                cell_job,
                stain_estimate,
                stain_estimate_applied,
                stain_estimate_error,
                stain_estimate_parameters,
                inputs,
                input_feedback,
                stain_model_io,
                stain_model_document,
                stain_model_revision,
                stain_model_project,
                stain_model_image,
                stain_model_kind,
                stain_model_vectors,
                stain_model_white,
                stain_model_basis,
                stain_model_basis_white,
                stain_model_bounds,
                project_replacements,
                active_image,
                paths,
                path_context,
                output_gate,
                pixel_classifier,
                notifications,
                tissue_parameters,
                tissue_grid,
                tissue_min_area,
                tissue_max_area,
                tissue_feature,
                tissue_connectivity,
                show_tissue,
                tissue_preview,
                tissue_annotations_added,
                cell_parameters,
                cell_grid,
                cell_nominal,
                grid_cells,
                cell_min_area,
                cell_max_area,
                cell_feature,
                cell_connectivity,
                cell_preview_body,
                show_cells,
                cell_preview,
                cell_annotations_added,
                kept_analysis_generation,
                kept_analysis_id,
                pending_analysis_recheck,
                kept_tissue_generation,
                verifying_tissue,
                kept_cell_generation,
                verifying_cells,
                recheck_analysis_generation,
                recheck_analysis_id,
                recheck_tissue_generation,
                recheck_tissue_id,
                recheck_cell_generation,
                recheck_cell_id,
                record_removal,
                workspace_epoch,
                cell_table_options,
                cell_table_job,
                task_make_transform,
                task_mark_target,
                task_focus_tables,
                task_build_tissue_request,
                task_build_cell_request,
                task_sampling_key,
                task_begin_export,measurement_job.busy()};
            detail::analysis_tasks::draw(task_context);
        }
        } else ui::TextDisabled("Select or add an image to use image tools.");
        if(!has_image && workspace.enabled()) {
            ImGui::End();workspace.begin(GuiWorkspace::objects_pane,workspace.objects);pane_translation.select("pane/Annotations");ui::TextDisabled("No active image");
            ImGui::End();workspace.begin(GuiWorkspace::workflow_pane,workspace.measurements);pane_translation.select("pane/Workflow");
            workflow_navigation();
        }
        if(!has_image && !workspace.enabled()) workflow_navigation();
        pane_translation.select((std::string("task/")+ui::workflow_task_keys[static_cast<std::size_t>(workflow_task)]).c_str());
        if((!selected || !has_image) && workflow_task<=Task::Cells) {
            ui::TextWrapped(has_image?"Select an annotation to use this task.":"Add or select an image to use this task.");
            if(ui::GhostButton(has_image?"Open annotations":ui::command(ui::CommandId::Header_AddImages).caption)) {
                if(workspace.enabled()) workspace.select(has_image?GuiWorkspace::objects_pane:GuiWorkspace::project_pane);
                if(!has_image) focus_add_image=true;
            }
        }
        if((workspace.enabled() && workflow_task==Task::Tools) || (!workspace.enabled() && ui::CollapsingHeader("Tools")))
            ui::ToolCatalog(tools,tool_context,tool_search,sizeof(tool_search));
        if((!workspace.enabled() || workflow_task==Task::Tables) && (has_image || options.project)) {
            ui::SetNextItemWidth(-1);if(ui::Combo("Table",&table_kind,"Cells\0ROI channel means\0Tissue components\0") && table_kind>0)measurement_kind=table_kind-1;
            if(table_kind!=0 && table_scope==2)table_scope=0;
            ui::SetNextItemWidth(-1);ui::Combo("Scope",&table_scope,table_kind==0?"Current image\0Project\0Current image (all kept runs)\0":"Current image\0Project\0");

            if(has_image)ui::TextWrapped("Current image: %zu ROI / %zu tissue / %zu cell runs kept",document.analysis_record_count(),document.tissue_record_count(),document.cell_record_count());
            if(table_scope==1 && options.project) {
                const auto& project=*options.project;
                const auto project_kept_counts=project_kept_cache.update(project.entries_,project.instance_identity_,project.generation(),project.revision_,active_image);
                ui::TextWrapped("Project: %zu ROI / %zu tissue / %zu cell runs kept",project_kept_counts[0],project_kept_counts[1],project_kept_counts[2]);
            }
            if(table_kind==0 && table_scope==2) {
                ui::cell_record_rows(document,table_record_rows);
                ui::RecordListActions list_actions;list_actions.status=[&](std::uint64_t id){return document.cell_status(id);};
                ui::draw_record_list("Current image (all kept runs)",table_record_rows,0,list_actions,true);
            }
            if(table_kind==0) {
                if(table_scope!=2) {
                int source=cell_table_options.published_objects?1:0;
                if(ui::Combo("Cell source",&source,table_scope==0?"Selected annotation's kept run\0Live cell objects\0":"Kept project runs\0Live cell objects\0"))cell_table_options.published_objects=source==1;
                if(cell_table_options.published_objects)ui::TextWrapped("Current names, classes and geometry; archived feature values. Missing measurements remain blank.");
                else if(table_scope==0)ui::TextWrapped("Exports the selected annotation's latest kept cell run.");
                }
                ui::Checkbox("Require current measurements",&cell_table_options.require_current);
                if(!workspace.enabled() || ui::CollapsingHeader("Export options")) {
                    ui::Checkbox("Include captured calibration",&cell_table_options.include_capture_calibration);
                    ui::Checkbox("Include intensity statistics",&cell_table_options.include_intensity_statistics);
                }
            } else if(table_kind!=0) {
                measurement_kind=table_kind-1;
                ui::Checkbox("Require current ROI / tissue runs",&measurement_options.require_current);
                ui::TextWrapped("Archived results and capture-time calibration; empty tissue runs have a summary row.");
            }
            const auto available=tables_availability(table_kind,table_scope);
            if(focus_cell_table_path){paths.cells.request_focus();focus_cell_table_path=false;}
            paths.output(table_kind==0?ExportOp::ImageCells:ExportOp::ImageMeasurements,measurement_kind).draw(path_context,false,{},{},"CSV file");

            const char* caption=table_kind==0?(table_scope==2?"Export all kept image cells":table_scope==0?"Export image cells (Ctrl+Alt+X)":"Export project cells (Ctrl+Shift+X)"):
                (table_scope==0?"Export image (Ctrl+Alt+Q)":"Export project (Ctrl+Shift+Q)");

            if(ui::WorkflowAction(caption,ui::design::Action::Primary,available)) {
                if(table_kind==0) {if(table_scope==0)export_image_cells();else if(table_scope==2)begin_export(ExportOp::ImageCells,{},true);else begin_export(ExportOp::ProjectCells);}
                else begin_export(table_scope==0?ExportOp::ImageMeasurements:ExportOp::ProjectMeasurements);
            }
            ui::WorkflowReason(caption,available);
        }
        // Independent jobs remain visible and cancellable across kind/task switches.
        const auto table_status=cell_table_job.poll();
        const bool show_export_success=!workspace.enabled() || workflow_task==Task::Tables;
        if(table_status.state=="succeeded" && show_export_success)ui::TextWrapped("Exported %llu rows to %s",static_cast<unsigned long long>(table_status.rows),cell_export_destination.c_str());
        if(table_status.state!="idle" && table_status.state!="succeeded") {
            ui::Text("Cell table: %s",ui::caption(ui::parse_export_state(table_status.state),table_status.state).c_str());
            ui::Text("Cell rows: %llu / %llu",static_cast<unsigned long long>(table_status.rows),static_cast<unsigned long long>(table_status.total_rows));
            if(cell_table_job.busy() && ui::WorkflowIcon("Cancel table (Ctrl+Alt+Shift+X)",ui::Icon::Stop,tables_availability(-1,0)))cell_table_job.cancel();
            if(!table_status.error.empty())ui::TextWrapped("%s",table_status.error.c_str());
        }
        if(cell_table_feedback==ExportFeedback::CellsRefused && !editing_blocked && !ui::DialogOwnsInput()) {
            const auto available=tables_availability(0,0,true);
            if(available.enabled)cell_table_feedback=ExportFeedback::Refused;
        }
        if(cell_table_feedback==ExportFeedback::Error && !cell_table_error.empty())ui::TextWrapped("%s",cell_table_error.c_str());
        if(measurement_export_status.state=="succeeded" && show_export_success)ui::TextWrapped("Exported %llu rows to %s",static_cast<unsigned long long>(measurement_export_status.rows),measurement_export_destination.c_str());
        if(measurement_export_status.state!="idle" && measurement_export_status.state!="succeeded") {
            ui::Text("Measurements: %s",ui::caption(ui::parse_export_state(measurement_export_status.state),measurement_export_status.state).c_str());
            ui::Text("Rows: %llu / %llu",static_cast<unsigned long long>(measurement_export_status.rows),static_cast<unsigned long long>(measurement_export_status.total_rows));
            if(measurement_job.busy() && ui::WorkflowIcon("Cancel export (Ctrl+Alt+Shift+Q)",ui::Icon::Stop,tables_availability(-1,0)))measurement_job.cancel();
        }
        if(!measurement_export_status.error.empty())ui::TextWrapped("%s",measurement_export_status.error.c_str());
        if(!measurement_error.empty() && measurement_feedback==ExportFeedback::Error)ui::TextWrapped("%s",measurement_error.c_str());
        const bool fingerprint_key=hash_key && hash_available.enabled;
        const bool fingerprint_cancel=!editing_blocked && ui::chord_pressed(ui::CommandId::Key_CancelFingerprint,ui::frame_commands());
        bool fingerprint_start=fingerprint_key;
        bool review_source_requested=review_key && review_available.enabled;
        if(!workspace.enabled() && (hash_key || review_key)) ImGui::SetNextItemOpen(true);
        if((workspace.enabled() && workflow_task==Task::Files) || (!workspace.enabled() && ui::CollapsingHeader("File checksum",options.file_fingerprint_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen))) {
            ui::Checkbox("Associate active image",&fingerprint_associate);
            {ui::IconReason association_reason(fingerprint_associate?"Hashes the active image path; JSON includes its reader generation.":"Wait for the current work to finish.");
            ImGui::BeginDisabled(fingerprint_associate);
            paths.fingerprint_input.draw(path_context,false,{},{},"File");
            if(has_image && ui::Button("Use image file")) {
                try {const auto path=document.server()->source_locator();if(path.empty()) throw ui::NoticeException<std::invalid_argument>("This image has no file locator; enter a path.");if(path.size()>32768) throw ui::NoticeException<std::length_error>("image locator is too long");paths.fingerprint_input.set(path);} catch(const std::exception& e) {{fingerprint_error=e.what();fingerprint_refused=false;if(!fingerprint_error.empty())notifications.post(ui::exception_notice(ui::workflow_task_labels[6],e,{}));}}
            }
            ImGui::EndDisabled();
            }
            if(fingerprint_associate) ui::TextWrapped("Hashes the active image path; JSON includes its reader generation.");
            paths.fingerprint_output.draw(path_context,false,{},{},"Optional JSON destination");
            ui::TextWrapped("Hashes one file. Multi-file slides need separate entries.");
            ui::TextWrapped("Existing JSON destinations ask before replacement. If writing fails, choose a path and hash again.");
            if(ui::WorkflowAction("Hash file (Ctrl+Shift+F)",ui::design::Action::Primary,hash_available))fingerprint_start=true;
            ui::WorkflowReason("Hash file (Ctrl+Shift+F)",hash_available);
            if(fingerprint_job.busy()) {
                if(ui::Button(ui::presentation_label(ui::CommandId::Key_CancelFingerprint,"Cancel file hash"))) fingerprint_job.cancel();
                ui::trace_workflow_action("Cancel file hash",ui::command(ui::CommandId::Key_CancelFingerprint).caption);
            }
            const auto status=fingerprint_job.poll();
            if(status.state!="idle") {ui::Text("File checksum: %s",ui::caption(ui::parse_run_state(status.state),status.state).c_str());ui::TextWrapped("Bytes: %llu / %llu",static_cast<unsigned long long>(status.bytes),static_cast<unsigned long long>(status.total_bytes));ui::TextWrapped("%s",status.path.c_str());}
            if(const auto value=fingerprint_job.result()) {ui::TextUnformatted("SHA-256:");ui::DataText(value->sha256.substr(0,32).c_str());ui::DataText(value->sha256.substr(32).c_str());ui::TextWrapped("Observed file metadata was stable while reading.");}
            if(const auto association=fingerprint_job.association()) {ui::TextWrapped("Reader match: %s",ui::caption(ui::parse_source_metadata_state(association->status),association->status).c_str());ui::TextWrapped("Scope: %s",association->source.scope.c_str());ui::TextWrapped("Metadata comparison; not proof of historical decoder bytes.");}
            if(options.project && active_image) {
                if(const auto saved=options.project->source_verification(active_image)) {ui::TextUnformatted("Saved project checksum:");ui::TextWrapped("%s",saved->fingerprint.sha256.c_str());}
                if(ui::WorkflowAction("Review for project (Ctrl+Alt+F)",ui::design::Action::Secondary,review_available))review_source_requested=true;
            }
            if(!status.error.empty()) ui::TextWrapped("%s",status.error.c_str());
            if(!fingerprint_refused && !fingerprint_error.empty()) ui::TextWrapped("%s",fingerprint_error.c_str());
        } else fingerprint_job.poll();
        if(fingerprint_cancel) fingerprint_job.cancel();
        if(fingerprint_start && !fingerprint_job.busy()) {try {
            const auto associate=fingerprint_associate;const auto input=paths.fingerprint_input.state().resolved;
            if(associate && !has_image)throw ui::NoticeException<std::invalid_argument>("select an image to associate its source");
            const auto source=associate?document.server()->reader_source():nullptr;
            if(associate && !source)throw ui::NoticeException<std::invalid_argument>("reader source identity is unavailable");
            auto start=[&,associate,input,source,settings=fingerprint_options](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& protected_paths) {
                source_review.reset();source_accept_changed=false;
                if(associate)fingerprint_job.request_source_with_policy(*source,path,policy,settings,protected_paths);
                else fingerprint_job.request_with_policy(input,path,policy,settings,protected_paths);
                fingerprint_project=options.project;fingerprint_image=active_image;fingerprint_document=options.project && active_image?options.project->document(active_image):nullptr;
                const auto live_source=active_document->server()->reader_source();fingerprint_generation=live_source?live_source->generation:std::string{};fingerprint_error.clear();fingerprint_refused=false;
                if(!path.empty())output_gate.watch(path,[&]()->std::optional<bool>{if(fingerprint_job.busy())return {};return bool(fingerprint_job.result()) && fingerprint_job.poll().error.empty();},[&]{const auto status=fingerprint_job.poll();return status.state=="failed"?status.error:std::string{};});
            };
            const auto output=paths.fingerprint_output.state().resolved;
            if(output.empty())start({},ExportWritePolicy::CreateNew,export_protection());
            else output_gate.submit(output,detail::OutputScope{std::string(ui::i18n::text("File fingerprint"))+" / "+(associate?source->locator:input),ui::workflow_task_labels[6]},std::move(start),[&](const std::string& message){fingerprint_error=message;fingerprint_refused=false;},false,{},associate?std::vector<std::string>{}:std::vector<std::string>{input});
        }catch(const std::exception& e){{fingerprint_error=e.what();fingerprint_refused=false;if(!fingerprint_error.empty())notifications.post(ui::exception_notice(ui::workflow_task_labels[6],e,{}));}}}
        if(review_source_requested) {
            try {
                if(!options.project || !active_image || fingerprint_project.lock()!=options.project || fingerprint_image!=active_image || fingerprint_document.lock()!=options.project->document(active_image))throw ui::NoticeException<std::runtime_error>("Hash the selected project image before reviewing its source.");
                const auto current_source=document.server()->reader_source();
                if(!current_source || current_source->generation!=fingerprint_generation)throw ui::NoticeException<std::runtime_error>("Image source changed; hash it again before review.");
                const auto value=fingerprint_job.result();if(fingerprint_job.busy() || !value)throw ui::NoticeException<std::runtime_error>("Wait for a successful file checksum first.");
                source_review=options.project->review_source_verification(active_image,*value);source_accept_changed=false;fingerprint_error.clear();fingerprint_refused=false;
                source_popup_requested=true;
            } catch(const std::exception& e) {{fingerprint_error=e.what();fingerprint_refused=false;if(!fingerprint_error.empty())notifications.post(ui::exception_notice(ui::workflow_task_labels[6],e,{}));}}
        }
        if(workflow_body) ImGui::EndChild();
        pane_translation.select("shell");
        if(workspace.enabled()) {
            ImGui::End();canvas_visible=workspace.begin("Slide",workspace.canvas,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
            if(canvas_visible) draw_toolbar();
        } else {
            ImGui::EndChild(); ImGui::SameLine();
            ImGui::BeginChild("Canvas", {0,-ImGui::GetFrameHeightWithSpacing()-2*ui::design::spacing.sm*ui_scale.logical}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        }
        auto offset = ImGui::GetCursorScreenPos(); auto size = ImGui::GetContentRegionAvail();
        size.x = std::max(size.x, 1.f); size.y = std::max(size.y, 1.f);
        ImGui::InvisibleButton("image canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if(has_image && canvas_visible && !fit && !restore_view && previous_canvas_size.x>0 && previous_canvas_size.y>0 &&
           (previous_canvas_size.x!=size.x || previous_canvas_size.y!=size.y)) {
            const auto center=viewport.screen_to_image({previous_canvas_size.x/2,previous_canvas_size.y/2});
            const bool synced=published_view_inputs && *published_view_inputs==view_inputs(previous_canvas_size.x,previous_canvas_size.y);
            viewport.center_on(center,size.x,size.y);
            if(synced) published_view_inputs=view_inputs(size.x,size.y);
            drawing=marquee=vertex_dragging=false;active_vertex.reset();
        }
        if(canvas_visible) {previous_canvas=offset; previous_canvas_size=size;previous_canvas_window=ImGui::GetCurrentWindow()->ID;}
        if(has_image && canvas_visible) {
        if (fit) { viewport.fit(image.width, image.height, size.x, size.y); fit = false; }
        if(restore_view) {
            viewport.zoom=restore_view->zoom; viewport.center_on(restore_view->center,size.x,size.y);
            restore_view.reset(); published_view_inputs=view_inputs(size.x,size.y);
        }
        if (overview_center) { viewport.center_on(*overview_center,size.x,size.y); overview_center.reset(); }
        Point mouse{io.MousePos.x - offset.x, io.MousePos.y - offset.y};
        const bool over_overview=workspace.enabled() && overview.ready() && io.MousePos.x>=overview_offset.x && io.MousePos.y>=overview_offset.y &&
            io.MousePos.x<overview_offset.x+overview_size.x && io.MousePos.y<overview_offset.y+overview_size.y;
        bool hovered = ImGui::IsItemHovered() && !over_overview;
        if (hovered && io.MouseWheel != 0) viewport.zoom_at(mouse, std::pow(1.2, io.MouseWheel));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            viewport.origin.x += io.MouseDelta.x; viewport.origin.y += io.MouseDelta.y;
        }
        if(tool==6 && selected && !document.annotation(selected).locked() && document.annotation(selected).kind==ObjectKind::Annotation) {
            const auto& object=document.objects().at(selected);
            if(vertex_id!=selected || vertex_revision!=object.revision) {
                if(vertex_dragging) { vertex_dragging=false; notifications.post({ui::Severity::Error,"Annotations","Vertex edit cancelled: object changed",{},true}); }
                if(vertex_id!=selected) active_vertex.reset();
                vertex_id=selected; vertex_revision=object.revision;
                vertex_paths=(object.geometry.kind()==RoiKind::Rectangle || object.geometry.kind()==RoiKind::Ellipse)?std::vector<Ring>{}:object.geometry.paths();
                if(active_vertex && (active_vertex->first>=vertex_paths.size() || active_vertex->second>=vertex_paths[active_vertex->first].size())) active_vertex.reset();
            }
        } else { vertex_paths.clear(); vertex_id=0; vertex_dragging=false; active_vertex.reset(); }
        auto clamp_image = [&](Point p) { return Point{std::clamp(p.x, 0., static_cast<double>(image.width)), std::clamp(p.y, 0., static_cast<double>(image.height))}; };
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            auto p = viewport.screen_to_image(mouse);
            if ((tool==1 || tool==2) && p.x >= 0 && p.y >= 0 && p.x < image.width && p.y < image.height) { anchor = p; drawing = true; }
            else if(((tool>=3 && tool<=5) || tool==7) && p.x>=0 && p.y>=0 && p.x<image.width && p.y<image.height) {
                if(tool==7 && draft_points.empty()) {
                    if(!selected || document.annotation(selected).locked() || document.annotation(selected).kind!=ObjectKind::Annotation || !document.annotation(selected).geometry.is_area())
                        notifications.post({ui::Severity::Error,"Annotations","Select an unlocked area annotation before drawing a hole",{},true});
                    else { hole_id=selected; hole_revision=document.annotation(selected).revision; draft_points.push_back(p); }
                } else if(draft_points.empty() || draft_points.back().x!=p.x || draft_points.back().y!=p.y) draft_points.push_back(p);
            }
            else if(tool==6) {
                double best=8/viewport.zoom; active_vertex.reset();
                for(std::size_t path=0;path<vertex_paths.size();++path) for(std::size_t v=0;v<vertex_paths[path].size();++v) {
                    auto point=vertex_paths[path][v]; auto distance=std::hypot(point.x-p.x,point.y-p.y);
                    if(distance<=best) { best=distance; active_vertex={{path,v}}; }
                }
                vertex_dragging=active_vertex.has_value(); vertex_cursor=p;
                if(!active_vertex) {
                    auto hits=document.pick(p,4/viewport.zoom);
                    if(!hits.empty()) { selection->replace({hits.back()}); selected=selection->primary(); }
                }
            }
            else if (!tool && objects_visible) {
                if(io.KeyShift) { marquee=true; marquee_anchor=p; marquee_additive=io.KeyCtrl; }
                else {
                    auto hits=document.pick(p,4/viewport.zoom);
                    if(hits.empty()) { if(!io.KeyCtrl) selection->clear(); }
                    else if(io.KeyCtrl) selection->toggle(hits.back()); else selection->replace({hits.back()});
                    selected=selection->primary();
                }
            }
        }
        auto end = clamp_image(viewport.screen_to_image(mouse));
        if(vertex_dragging) {
            vertex_cursor=end;
            if(ui::chord_pressed(ui::CommandId::Key_EscapeCanvas,ui::frame_commands())) vertex_dragging=false;
            else if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                try { document.edit_vertex(vertex_id,active_vertex->first,active_vertex->second,vertex_cursor); notifications.resolve("Annotations"); }
                catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
                vertex_dragging=false;
            }
        }
        if(tool==6 && !io.WantTextInput && !editing_blocked && !vertex_dragging) {
            if(active_vertex && ui::chord_pressed(ui::CommandId::Key_Delete,ui::frame_commands())) {
                try { document.remove_vertex(vertex_id,active_vertex->first,active_vertex->second); active_vertex.reset(); notifications.resolve("Annotations"); }
                catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
            }
            if(hovered && vertex_id && ui::chord_pressed(ui::CommandId::Key_InsertVertex,ui::frame_commands())) {
                try {
                    const auto& geometry=document.objects().at(vertex_id).geometry;
                    if(geometry.kind()==RoiKind::Points) document.insert_vertex(vertex_id,0,vertex_paths.at(0).size(),end);
                    else {
                        double best=8/viewport.zoom; std::optional<std::pair<std::size_t,std::size_t>> edge;
                        for(std::size_t path=0;path<vertex_paths.size();++path) {
                            const auto& ring=vertex_paths[path]; auto count=ring.size()-(geometry.is_area()?0:1);
                            for(std::size_t v=0;v<count;++v) {
                                auto a=ring[v],b=ring[(v+1)%ring.size()]; auto dx=b.x-a.x,dy=b.y-a.y;
                                auto length=dx*dx+dy*dy;
                                auto t=length?std::clamp(((end.x-a.x)*dx+(end.y-a.y)*dy)/length,0.,1.):0.;
                                auto distance=std::hypot(end.x-a.x-t*dx,end.y-a.y-t*dy);
                                if(distance<=best) { best=distance; edge={{path,v+1}}; }
                            }
                        }
                        if(!edge) throw ui::NoticeException<std::invalid_argument>("Point near an edge to insert a vertex");
                        document.insert_vertex(vertex_id,edge->first,edge->second,end);
                    }
                    active_vertex.reset(); notifications.resolve("Annotations");
                } catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
            }
        }
        if(marquee && objects_visible && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            try {
                auto p=viewport.screen_to_image(mouse);
                if(std::abs(p.x-marquee_anchor.x)*viewport.zoom<3 && std::abs(p.y-marquee_anchor.y)*viewport.zoom<3) {
                    auto hits=document.pick(p,4/viewport.zoom);
                    if(!hits.empty()) selection->toggle(hits.back());
                } else selection->select_region({std::min(p.x,marquee_anchor.x),std::min(p.y,marquee_anchor.y),
                    std::abs(p.x-marquee_anchor.x),std::abs(p.y-marquee_anchor.y)},marquee_additive);
                selected=selection->primary(); notifications.resolve("Annotations");
            } catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
            marquee=false;
        }
        if (drawing && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            Rect r{std::min(anchor.x,end.x), std::min(anchor.y,end.y), std::abs(anchor.x-end.x), std::abs(anchor.y-end.y)};
            if (r.width * viewport.zoom >= 2 && r.height * viewport.zoom >= 2) {
                try {
                    auto name="Region "+std::to_string(document.object_count()+1);
                    selected=tool==2?document.add_roi(Roi::ellipse(r),name):document.add_rectangle(r,name); selection->replace({selected}); notifications.resolve("Annotations");
                }
                catch (const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
            }
            drawing = false;
        }
        if(!editing_blocked && !project_browser_enter && ui::chord_pressed(ui::CommandId::Key_EscapeCanvas,ui::frame_commands())) {
            if(!drawing && draft_points.empty() && !marquee && tool!=6) { selection->clear(); selected=0; }
            drawing=false; marquee=false; draft_points.clear();
        }
        if(((tool>=3 && tool<=5) || tool==7) && !draft_points.empty() && !io.WantTextInput && !editing_blocked && !project_browser_enter && (ui::chord_pressed(ui::CommandId::Key_FinishDraft,ui::frame_commands()) || (hovered && ImGui::IsMouseDoubleClicked(0)))) {
            try {
                if(tool==7) {
                    if(selected!=hole_id || !document.objects().count(hole_id) || document.annotation(hole_id).revision!=hole_revision)
                        throw ui::NoticeException<std::invalid_argument>("Hole edit cancelled: target changed");
                    auto geometry=document.annotation(hole_id).geometry.as_polygon();
                    auto hole=Roi::polygon(draft_points); auto parts=geometry.polygon_parts(); bool added=false;
                    for(std::size_t i=0;i<parts.size();++i) if(Roi::polygon(parts[i].shell,parts[i].holes).covers(hole)) {
                        document.add_hole(hole_id,i,draft_points); added=true; break;
                    }
                    if(!added) throw ui::NoticeException<std::invalid_argument>("Hole must lie inside one polygon part without overlapping existing holes");
                } else {
                    auto geometry=tool==3?Roi::polygon(draft_points):(tool==4?Roi::polyline(draft_points):Roi::points(draft_points));
                    selected=document.add_roi(geometry,"Region "+std::to_string(document.object_count()+1)); selection->replace({selected});
                }
                draft_points.clear(); notifications.resolve("Annotations");
            } catch(const std::exception& e) { notifications.post(ui::exception_notice("Annotations",e,{})); }
        }
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(offset, {offset.x+size.x, offset.y+size.y}, true);
        profile.widgets_ms=profile_lap();
        draw->AddRectFilled(offset, {offset.x+size.x,offset.y+size.y}, IM_COL32(18,21,27,255));
        auto dpi=std::max(io.DisplayFramebufferScale.x,io.DisplayFramebufferScale.y);
        auto inputs=std::make_tuple(viewport.origin.x,viewport.origin.y,viewport.zoom,size.x,size.y,dpi);
        if (!plan_inputs || *plan_inputs!=inputs) {
            plan=plan_view_tiles(image,viewport,size.x,size.y,dpi,256,128); plan_inputs=inputs;
        }
        tiles->update(plan);
        tissue_preview.update(tissue.result(),show_tissue);pixel_classifier.update_preview();
        const auto detected=cell_job.result();cell_preview.update(detected?(cell_preview_body?detected->cells():detected->nuclei()):nullptr,show_cells);
        for(const auto& tile:plan.tiles) tiles->texture(tile); // Preserve tile LRU use even on a canvas-cache hit.
        if(!overlay_display || !(*overlay_display==display)) {overlay_cache.clear();overlay_display=display;}
        profile.tiles_ms=profile_lap();
        const auto visible_min=viewport.screen_to_image({0,0}),visible_max=viewport.screen_to_image({size.x,size.y});
        const auto clip=draw->_CmdHeader.ClipRect;const auto* shared=ImGui::GetDrawListSharedData();
        OverlayKey next_overlay_key{&document,selection.get(),document.render_revision(),selection->revision(),selected,viewport.zoom,viewport.origin.x,viewport.origin.y,
            offset.x,offset.y,size.x,size.y,clip.x,clip.y,clip.z,clip.w,ImGui::GetFont(),ImGui::GetFontSize(),static_cast<ImTextureID>(ImGui::GetIO().Fonts->TexData->UniqueID),
            draw->Flags,shared->TexUvWhitePixel.x,shared->TexUvWhitePixel.y,shared->CurveTessellationMaxError,shared->CircleTessellationMaxError,
            tiles->uploads(),tiles->generations(),tissue.poll(document,selected).generation,cell_job.poll(document,selected).generation,tiles->bytes(),tissue.result().get(),detected.get(),
            show_tissue,show_cells,tissue_preview.ready(),cell_preview.ready(),cell_preview_body,pixel_classifier.preview().generation(),pixel_classifier.show_overlay(),pixel_classifier.preview_ready(),raw_pixel.preview().generation(),raw_pixel.shown(),raw_pixel.preview().ready()};
        next_overlay_key.objects_visible=objects_visible;
        next_overlay_key.font_baked_id=ImGui::GetFontBaked()->BakedId;
        next_overlay_key.font_glyphs=ImGui::GetFontBaked()->Glyphs.Size;
        const bool overlay_hit=overlay_cache.prepare(next_overlay_key,clip,max_texture,options.overlay_cache_budget);profile.cache_hit=overlay_hit;profile.cache_ms=profile_lap();
        if(overlay_hit) overlay_cache.draw(draw);
        else {
        for (const auto& tile : plan.tiles) if (auto texture=tiles->texture(tile)) {
            draw->AddImage(static_cast<ImTextureID>(texture),screen(viewport,{tile.bounds.x,tile.bounds.y},offset),
                screen(viewport,{tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height},offset));
        }
        if(show_tissue) tissue_preview.draw(draw,viewport,offset,size);
        pixel_classifier.draw_preview(draw,viewport,offset,size);raw_pixel.draw_preview(draw,viewport,offset,size);
        if(show_cells) cell_preview.draw(draw,viewport,offset,size);
        auto visible_ids=document.query_region({visible_min.x,visible_min.y,visible_max.x-visible_min.x,visible_max.y-visible_min.y});
        visible_count=visible_ids.size();profile.query_ms=profile_lap();
        // Dense selections use a single sorted walk. Sparse views retain tree
        // lookups so a tiny viewport never copies a large off-screen selection.
        const auto selection_count=selection->size();
        const bool all_selected=selection_count==document.objects().size();
        const bool merge_selection=objects_visible && !all_selected && selection_count>=64 && visible_ids.size()>=64 && selection_count/4<=visible_ids.size();
        const auto selected_ids=merge_selection?selection->ids():std::vector<std::uint64_t>{};
        auto selected_cursor=selected_ids.begin();
        if(objects_visible)for (auto id:visible_ids) {
            bool is_selected=all_selected;
            if(merge_selection) {
                while(selected_cursor!=selected_ids.end() && *selected_cursor<id) ++selected_cursor;
                is_selected=selected_cursor!=selected_ids.end() && *selected_cursor==id;
            } else if(!all_selected && selection_count) is_selected=selection->contains(id);
            const auto& a=document.objects().at(id); const auto r=a.geometry.bounds();
            auto p0 = screen(viewport,{r.x,r.y},offset);
            auto color = is_selected ? IM_COL32(255,205,85,255) : IM_COL32(65,225,190,255);
            if(!is_selected && !a.class_name.empty()) {
                auto found=document.classes().find(a.class_name);
                if(found!=document.classes().end()) {
                    auto rgb=found->second.color; color=IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);
                }
            }
            if(!is_selected)if(const auto rgb=a.color_rgb())color=IM_COL32((*rgb>>16)&255,(*rgb>>8)&255,*rgb&255,255);
            if(!is_selected && a.tma_core && a.tma_core->missing()) color=IM_COL32(145,145,145,230);
            draw_shape(draw,a.geometry,viewport,offset,color);
            if(a.nucleus) draw_shape(draw,*a.nucleus,viewport,offset,IM_COL32(195,145,255,255));
            // Detection labels at low zoom overwhelm the canvas and scale with
            // every visible detection. Keep annotation names and selected labels.
            if(!a.is_detection() || a.id==selected) draw->AddText(p0,color,a.name.c_str());
        }
        profile.geometry_ms=profile_lap();
        overlay_cache.capture(draw);
        }
        tma.draw_qptma_preview(document,draw,viewport,offset,size);
        genes.draw_overlay(draw,viewport,offset,size);
        fluorescence.draw_preview(draw,viewport,offset,size);
        if(tool==6 && vertex_id) {
            for(std::size_t path=0;path<vertex_paths.size();++path) {
                const auto& ring=vertex_paths[path];
                std::vector<ImVec2> preview; if(vertex_dragging) preview.reserve(ring.size());
                for(std::size_t v=0;v<ring.size();++v) {
                    auto point=(vertex_dragging && active_vertex==std::make_pair(path,v))?vertex_cursor:ring[v];
                    auto p=screen(viewport,point,offset);
                    if(vertex_dragging) preview.push_back(p);
                    if(p.x>=offset.x && p.y>=offset.y && p.x<=offset.x+size.x && p.y<=offset.y+size.y)
                        draw->AddCircleFilled(p,4,active_vertex==std::make_pair(path,v)?IM_COL32(255,120,100,255):IM_COL32(255,205,85,255));
                }
                if(vertex_dragging) draw->AddPolyline(preview.data(),static_cast<int>(preview.size()),IM_COL32(255,120,100,255),2.f,document.annotation(vertex_id).geometry.is_area()?ImDrawFlags_Closed:0);
            }
        }
        if(marquee) {
            auto p=viewport.screen_to_image(mouse);
            auto a=screen(viewport,marquee_anchor,offset),b=screen(viewport,p,offset);
            ImVec2 lo{std::min(a.x,b.x),std::min(a.y,b.y)},hi{std::max(a.x,b.x),std::max(a.y,b.y)};
            draw->AddRectFilled(lo,hi,IM_COL32(255,205,85,30)); draw->AddRect(lo,hi,IM_COL32(255,205,85,255));
        }
        if (drawing) {
            Rect draft{std::min(anchor.x,end.x),std::min(anchor.y,end.y),std::abs(anchor.x-end.x),std::abs(anchor.y-end.y)};
            if(draft.width>0 && draft.height>0) draw_shape(draw,tool==2?Roi::ellipse(draft):Roi::rectangle(draft),viewport,offset,IM_COL32(255,205,85,255));
        }
        for(std::size_t i=0;i<draft_points.size();++i) {
            auto p=screen(viewport,draft_points[i],offset); draw->AddCircleFilled(p,3,IM_COL32(255,205,85,255));
            if(i) draw->AddLine(screen(viewport,draft_points[i-1],offset),p,IM_COL32(255,205,85,255),2);
        }
        if(!draft_points.empty()) draw->AddLine(screen(viewport,draft_points.back(),offset),screen(viewport,end,offset),IM_COL32(255,205,85,255),1);
        draw->PopClipRect();
        std::string canvas_notice=tiles->error().empty() ? (tiles->pending() ? "Loading image..." : "") : tiles->error();
        if (plan.truncated) canvas_notice="View exceeds tile limit; zoom in for complete coverage";
        if(!draft_points.empty()) canvas_notice="Enter or double-click: finish shape | Escape: cancel";
        if (!canvas_notice.empty()) draw->AddText({offset.x+12,offset.y+12},IM_COL32(255,220,130,255),canvas_notice.c_str());
        if (document.pixel_width_um()) {
            auto target=120/viewport.zoom * *document.pixel_width_um();
            if (target>0 && std::isfinite(target)) {
            auto magnitude=std::pow(10.,std::floor(std::log10(target)));
            auto nice=std::floor(target/magnitude)*magnitude;
            auto length=static_cast<float>(nice/ *document.pixel_width_um()*viewport.zoom);
            ImVec2 p{offset.x+20,offset.y+size.y-30};
            const auto bar=scale_bar_layout(ImGui::GetTextLineHeight(),ui_scale.logical);
            draw->AddLine(p,{p.x+length,p.y},IM_COL32_BLACK,bar.thickness+2*bar.outline);
            draw->AddLine(p,{p.x+length,p.y},IM_COL32_WHITE,bar.thickness);
            auto label=std::to_string(nice); label.erase(label.find_last_not_of('0')+1); if (!label.empty() && label.back()=='.') label.pop_back();
            label+=" ";label+=ui::unit::um; const ImVec2 label_at{p.x,p.y-bar.label_offset_y};
            for(const auto delta:std::array<ImVec2,4>{{{-bar.outline,0},{bar.outline,0},{0,-bar.outline},{0,bar.outline}}}) draw->AddText({label_at.x+delta.x,label_at.y+delta.y},IM_COL32_BLACK,label.c_str());
            draw->AddText(label_at,IM_COL32_WHITE,label.c_str());
            }
        }
        } else if(!has_image) {
            overview.update(nullptr,display); // Reap a cancelled old-source job in an empty workspace.
            overlay_cache.clear();overlay_display.reset();
            visible_count=0;
            ImGui::GetWindowDrawList()->AddText({offset.x+32,offset.y+32},IM_COL32(190,205,220,255),project_images.empty()?"No images in this project.\nAdd an image in Project images, or open another project.":"No image selected.\nSelect an image in Project images.");
        }
        if(workspace.enabled() && has_image && canvas_visible && overview.ready()) {
            const float width=std::min(ui::design::metrics.overview_width*ui_scale.logical,size.x*.28f);
            const auto padding=ImGui::GetStyle().WindowPadding;
            ImGui::SetCursorScreenPos({offset.x+size.x-width-2*padding.x-ui::design::metrics.overview_margin*ui_scale.logical,offset.y+ui::design::metrics.overview_margin*ui_scale.logical});
            ImGui::BeginChild("Slide overview",{width+2*padding.x,width*.6f+2*padding.y},ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
            overview_navigation(width);
            ImGui::EndChild();
        }
        if(!canvas_visible) visible_count=0;
        if(has_image && overview_draw) {
        // Keep the image and viewport outline clipped to their own overview
        // surface, whether it is in the viewer or the compact inspector.
        overview_draw->PushClipRect({std::max(overview_offset.x,overview_clip_min.x),std::max(overview_offset.y,overview_clip_min.y)},
            {std::min(overview_offset.x+overview_size.x,overview_clip_max.x),std::min(overview_offset.y+overview_size.y,overview_clip_max.y)},false);
        overview_draw->AddRectFilled(overview_offset,{overview_offset.x+overview_size.x,overview_offset.y+overview_size.y},IM_COL32(18,21,27,255));
        if(overview.ready()) overview_draw->AddImage(static_cast<ImTextureID>(overview.texture()),
            screen(overview_view,{0,0},overview_offset),screen(overview_view,{double(image.width),double(image.height)},overview_offset));
        else overview_draw->AddText({overview_offset.x+4,overview_offset.y+4},IM_COL32(210,215,225,255),
            overview.state()=="failed"?"Overview unavailable":"Loading overview...");
        auto top_left=viewport.screen_to_image({0,0}),bottom_right=viewport.screen_to_image({previous_canvas_size.x,previous_canvas_size.y});
        overview_draw->AddRect(screen(overview_view,top_left,overview_offset),screen(overview_view,bottom_right,overview_offset),IM_COL32(255,205,85,255),0.f,2.f,0);
        overview_draw->PopClipRect();
        }
        if(workspace.enabled()) ImGui::End(); else ImGui::EndChild();
        ImGui::EndDisabled();
        ui::Separator();
        auto& status_summary=notice_observers.status_summary;
        ui::status_summary(status_summary,has_image,bool(options.project),objects_visible,options.project && options.project->dirty(),
            project_browser.matched,project_images.size(),static_cast<long long>(image.width),static_cast<long long>(image.height),
            viewport.zoom*100,document.object_count(),selection->size());
        notifications.draw_status_bar(status_summary);
        using DialogAction=ui::DialogAction;using DialogOutcome=ui::DialogOutcome;
        auto mark_dialog=[&](GuiInputTarget target,ImVec2& offset,ImVec2& size) {offset=ImGui::GetItemRectMin();const auto end=ImGui::GetItemRectMax();size={end.x-offset.x,end.y-offset.y};mark_target(target);};

        output_gate.draw([&](const detail::ExportOwnerStamp& stamp) {
            return capture_export_owner(stamp.selected_only,stamp.project_scope,false).same_context(stamp);
        },[&](DialogAction action) {
            if(action==DialogAction::Confirm)mark_dialog(GuiInputTarget::ConfirmReplaceExport,confirm_export_offset,confirm_export_size);
            if(action==DialogAction::Cancel)mark_dialog(GuiInputTarget::CancelReplaceExport,cancel_export_offset,cancel_export_size);
        });

        if(object_info_requested) {ui::OpenDialog(ui::DialogKind::ObjectInformation);object_info_requested=false;}
        bool object_info_owner=has_image && object_info_document==active_document && object_info_epoch==workspace_epoch && object_info_snapshot && selected==object_info_snapshot->id;
        if(object_info_owner) {const auto found=document.objects().find(object_info_snapshot->id);object_info_owner=found!=document.objects().end() && found->second.revision==object_info_snapshot->revision;}
        ui::DialogState object_info_state(object_info_owner,false);object_info_state.dismiss_requested=!object_info_active;
        ui::RenderDialog(ui::DialogKind::ObjectInformation,object_info_state,{}, {},[&](ui::DialogControls&) {
            if(!object_info_snapshot)return;const auto& object=*object_info_snapshot;
            ui::DataTextWrapped("%s",object.name.c_str());ui::Separator();
            ui::Text("Kind: %s",object.kind==ObjectKind::Cell?"Cell":object.kind==ObjectKind::Detection?"Detection":"Annotation");
            if(object.plane.c<0) {ui::Text("%s %s",ui::i18n::text("ROI plane:"),ui::i18n::text("all channels"));ui::Text("Z %u / time %u",object.plane.z,object.plane.t);}
            else ui::Text("ROI plane: channel %d / Z %u / time %u",object.plane.c,object.plane.z,object.plane.t);
            if(const auto probability=object.class_probability())ui::Text("Reported probability: %.17g",*probability);
            if(object.kind==ObjectKind::Cell)ui::TextDisabled(object.nucleus?"Nucleus ROI present":"No nucleus ROI");
            ui::TextWrapped("Kept values are historical; geometry and calibration edits do not recompute them.");
            ui::SetNextItemWidth(-1);ui::InputTextWithHint("##Object information search","Search keys or values",object_info_search,sizeof(object_info_search));
            const auto matches=[&](std::string key,const std::string& value) {key+=" "+value;std::string needle=object_info_search;detail::ascii_lower_in_place(key);detail::ascii_lower_in_place(needle);return key.find(needle)!=std::string::npos;};
            bool found=false;
            if(object.properties) {
                for(const auto& [name,value]:object.properties->metadata())if(matches(name,value)) {ui::DataTextWrapped("%s",name.c_str());ui::DataTextWrapped("%s",value.c_str());ui::Separator();found=true;}
                for(const auto& [name,value]:object.properties->measurements()) {char number[64];std::snprintf(number,sizeof(number),"%.17g",value);if(matches(name,number)){ui::DataTextWrapped("%s",name.c_str());ui::DataTextWrapped("%s",number);ui::Separator();found=true;}}
            }
            if(!found)ui::TextDisabled("No matching imported properties.");
        },[&](DialogAction) {object_info_active=false;return DialogOutcome::Success;});
        calibration_editor.draw(has_image?active_document:nullptr,active_image,workspace_epoch);
        if(image_info_requested) {ui::OpenDialog(ui::DialogKind::ImageInformation);image_info_requested=false;}
        const bool image_info_owner=has_image && image_info_document==active_document && image_info_epoch==workspace_epoch;
        ui::DialogState image_info_state(image_info_owner,false);image_info_state.dismiss_requested=!image_info_active;
        ui::RenderDialog(ui::DialogKind::ImageInformation,image_info_state,{}, {},[&](ui::DialogControls&) {
            if(!image_info_owner) {ui::TextWrapped("The image changed. Close this information view and open it again.");return;}
            auto search_imported=[&] {ui::SetNextItemWidth(-1);ui::InputTextWithHint("##Image information search","Search keys or values",image_info_search,sizeof(image_info_search));};
            auto matches=[&](std::string key,std::string value=std::string{}) {key+=" "+value;std::string needle=image_info_search;detail::ascii_lower_in_place(key);detail::ascii_lower_in_place(needle);return key.find(needle)!=std::string::npos;};
            if(ImGui::BeginTabBar("Image information tabs")) {
                if(ImGui::BeginTabItem(ui::i18n::label("Metadata").c_str())) {
            ui::TextUnformatted("Source");ui::DataTextWrapped("%s",document.server()->source_locator().empty()?ui::i18n::text("Embedded image"):document.server()->source_locator().c_str());
            ui::Text("Reader: %s",document.server()->reader_id().c_str());
            ui::TextWrapped("%s: %zu",ui::i18n::text("Pyramid levels"),image.levels.size());
            ui::TextWrapped("Imported values are historical metadata, not live ROI measurements.");
                    ui::Separator();search_imported();bool found=false;
                    if(const auto props=document.image_data_properties())for(const auto& [key,value]:props->values()) {
                        const auto* text=std::get_if<std::string>(&value.value);
                        if(matches(key,text?*text:std::string{})) {image_data_value(key,value);found=true;}
                    }
                    if(const auto facts=document.image_properties())for(const auto& [key,value]:facts->metadata())if(matches(key,value)) {
                        ui::DataTextWrapped("%s",key.c_str());ui::DataTextWrapped("%s",value.c_str());ui::Separator();found=true;
                    }
                    if(!found)ui::TextDisabled("No matching imported metadata.");
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem(ui::i18n::label("Imported measurements").c_str())) {
                    ui::TextWrapped("Imported values are historical metadata, not live ROI measurements.");
                    search_imported();bool found=false;
                    if(const auto facts=document.image_properties())for(const auto& [key,value]:facts->measurements()) {
                        char number[80];std::snprintf(number,sizeof(number),"%.17g",value);
                        if(matches(key,number)) {ui::DataTextWrapped("%s",key.c_str());ui::DataText(number);ui::Separator();found=true;}
                    }
                    if(!found)ui::TextDisabled("No matching imported measurements.");
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem(ui::i18n::label("Analysis").c_str(),nullptr,image_info_analysis?ImGuiTabItemFlags_SetSelected:0)) {
                    if(image_info_transform) {
                        ui::SeparatorText("Active inputs when opened");const auto& tr=*image_info_transform;const auto& w=tr.white();
                        ui::TextDisabled(image_info_stains?"Supplied stains":"RGB optical density");
                        ui::TextWrapped("White: %.17g / %.17g / %.17g",w[0],w[1],w[2]);
                        const char* channels[]={"Red","Green","Blue"};
                        for(std::size_t i=0;i<3;++i) {
                            if(image_info_stains)ui::TextWrapped("Stain %zu: %.17g / %.17g / %.17g",i+1,tr.stains()[i][0],tr.stains()[i][1],tr.stains()[i][2]);
                            else ui::TextWrapped("%s: %.17g / %.17g / %.17g",ui::i18n::text(channels[i]),tr.stains()[i][0],tr.stains()[i][1],tr.stains()[i][2]);
                        }
                    }
                    if(const auto& settings=document.image_analysis_settings();settings) {
                        ui::SeparatorText("Kept analysis profile");ui::DataTextWrapped("%s",settings->image_type().c_str());
                        ui::DataTextWrapped("%s",settings->profile_name().c_str());ui::TextDisabled("Applied only on request");
                        if(settings->has_stains())for(std::size_t i=0;i<3;++i)ui::TextWrapped("%s: %.17g / %.17g / %.17g",settings->stain_names()[i].c_str(),settings->basis()[i][0],settings->basis()[i][1],settings->basis()[i][2]);
                        ui::TextWrapped("White: %.17g / %.17g / %.17g",settings->white()[0],settings->white()[1],settings->white()[2]);
                    } else ui::TextDisabled("No saved image analysis settings.");
                    ImGui::EndTabItem();
                }
                image_info_analysis=false;
                ImGui::EndTabBar();
            }
        },[&](DialogAction) {image_info_active=false;return DialogOutcome::Success;});
        help_window.draw(*build_environment());
        if(language_requested) {ui::OpenDialog(ui::DialogKind::Language);language_requested=false;language_dialog_active=true;language_search[0]=0;}
        ui::DialogState language_state(true,false);
        ui::RenderDialog(ui::DialogKind::Language,language_state,{}, {},[&](ui::DialogControls&) {
            ui::SetNextItemWidth(-1);ui::InputTextWithHint("##Language search","Search languages",language_search,sizeof(language_search));
            ui::LayoutRegion languages_region("Languages");ui::PaneTitle("Languages");
            if(ImGui::BeginChild("Language list",{0,std::min(340.f*ui_scale.logical,io.DisplaySize.y*.45f)},ImGuiChildFlags_Borders)) {
                bool found=false;
                if(!language_search[0] || ui::i18n::matches_search({"auto",ui::i18n::text("Automatic (system)"),"Automatic system"},language_search)) {
                    const std::string caption=std::string(ui::i18n::text("Automatic (system)"))+" / "+ui::i18n::languages[ui::i18n::system_language()].native_name+"###language-auto";
                    if(ui::Selectable(caption.c_str(),ui::i18n::state.choice<0))pending_language=-1;found=true;
                }
                for(int i=0;i<ui::i18n::language_count;++i) {
                    const auto& language=ui::i18n::languages[i];if(!ui::i18n::matches_search(language,language_search))continue;
                    const auto caption=std::string(language.native_name)+" / "+language.english_name+"###language-"+language.code;
                    if(ui::Selectable(caption.c_str(),ui::i18n::state.choice==i))pending_language=i;found=true;
                }
                if(!found)ui::TextUnformatted("No matching languages");
            }ImGui::EndChild();ui::TextWrapped("Untranslated messages use English.");
        },[&](DialogAction) {language_dialog_active=false;return DialogOutcome::Success;});
        if(source_popup_requested) {ui::OpenDialog(ui::DialogKind::SourceVerification);source_popup_requested=false;}
        const bool source_owner=source_review && options.project && fingerprint_project.lock()==options.project && active_image && active_image==fingerprint_image && fingerprint_document.lock().get()==active_document;
        ui::DialogState source_state(source_owner,false);source_state.dismiss_requested=!source_review;
        ui::RenderDialog(ui::DialogKind::SourceVerification,source_state,source_review?source_review->path():std::string{},{&fingerprint_error},[&](ui::DialogControls&) {
            if(!source_review)return;
            const bool changed=source_review->requires_acceptance();
            ui::TextWrapped(source_review->comparison()=="changed_dependencies"?"The primary checksum matches, but source dependency metadata changed.":source_review->comparison()=="changed_observed_source"?"The file differs from the opened image source. Its previous checksum is unknown.":changed?"File bytes differ from the saved checksum.":source_review->comparison()=="matching_bytes"?"File bytes match the saved checksum.":"No saved file verification. Save this checksum with the project.");
            const auto association=source_review->association();ui::TextUnformatted("Reviewed SHA-256:");
            ui::DataText(association->fingerprint.sha256.substr(0,32).c_str());ui::DataText(association->fingerprint.sha256.substr(32).c_str());
            ui::TextWrapped(source_review->coverage()=="compound_primary_only"?"Only the primary file was hashed. Companion files were not verified by checksum.":source_review->coverage()=="primary_file"?"Primary file checksum; other dependencies are outside this record.":"Single source file checksum.");
            if(changed) {ui::Checkbox("Accept this source change and save its verification",&source_accept_changed);const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();source_review_bounds[1]={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};mark_target(GuiInputTarget::AcceptSourceReplacement);}
            source_state.submit_enabled=!changed || source_accept_changed;
        },[&](DialogAction action) {
            if(action==DialogAction::Cancel) {source_review.reset();return DialogOutcome::Success;}
            try {const auto previous_server=document.server();options.project->apply_source_review(source_review,source_accept_changed);if(document.server()!=previous_server) {mean.invalidate();pending_image=active_image;refresh_active_image=true;project_catalog_dirty=true;}source_review.reset();fingerprint_error.clear();fingerprint_refused=false;return DialogOutcome::Success;}
            catch(const std::exception& e){{fingerprint_error=e.what();fingerprint_refused=false;if(!fingerprint_error.empty())notifications.post(ui::exception_notice(ui::workflow_task_labels[6],e,{}));}return DialogOutcome::Refused;}
        },[&](DialogAction action) {if(action==DialogAction::Confirm) {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();source_review_bounds[0]={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};mark_target(GuiInputTarget::ApplySourceVerification);}});
        if(maintenance_popup_requested) {ui::OpenDialog(ui::DialogKind::DeleteCheckpoint);maintenance_popup_requested=false;}
        ui::DialogState checkpoint_state(reviewed_checkpoint.has_value(),maintenance_job.valid());checkpoint_state.dismiss_requested=!reviewed_checkpoint;
        ui::RenderDialog(ui::DialogKind::DeleteCheckpoint,checkpoint_state,reviewed_checkpoint?reviewed_checkpoint->path:std::string{},{&maintenance_error},[&](ui::DialogControls&) {
            if(!reviewed_checkpoint)return;
            ui::TextWrapped("Permanently delete this recovery checkpoint? This does not discard the live workspace or change the primary project.");
            ui::TextWrapped("%s / %zu images",recovery_label(*reviewed_checkpoint).c_str(),reviewed_checkpoint->images);
            ui::TextWrapped("Automatic recovery saving or another open window may create another checkpoint later.");
        },[&](DialogAction action) {
            if(action==DialogAction::Cancel) {reviewed_checkpoint.reset();maintenance_dialog_active=false;return DialogOutcome::Success;}
            auto reviewed=*reviewed_checkpoint;maintenance_epoch=workspace_epoch;maintenance_error.clear();
            maintenance_job=std::async(std::launch::async,[reviewed] {MaintenanceResult result;try {Project::discard_recovery_checked(reviewed);result.deleted=reviewed.path;}catch(const std::exception& e){result.error=e.what();}return result;});
            reviewed_checkpoint.reset();maintenance_dialog_active=false;return DialogOutcome::Success;
        },[&](DialogAction action) {if(action==DialogAction::Confirm)mark_dialog(GuiInputTarget::ConfirmDiscardCheckpoint,confirm_checkpoint_offset,confirm_checkpoint_size);if(action==DialogAction::Cancel)mark_dialog(GuiInputTarget::CancelDiscardCheckpoint,cancel_checkpoint_offset,cancel_checkpoint_size);});
        if(remove_popup_requested) {ui::OpenDialog(ui::DialogKind::RemoveImage);remove_popup_requested=false;}
        const auto removal=ui::IsDialogOpen(ui::DialogKind::RemoveImage)?std::find_if(project_images.begin(),project_images.end(),[&](const auto& image){return image.id==remove_image_id;}):project_images.end();
        ui::DialogState remove_state(remove_project.lock()==options.project && removal!=project_images.end(),false);
        ui::RenderDialog(ui::DialogKind::RemoveImage,remove_state,removal==project_images.end()?std::string{}:removal->name,{},[&](ui::DialogControls&) {
            ui::TextWrapped("This removes the entry, its objects, view and editing history from the project. Undo cannot restore the entry. Save project to persist the removal. The source image file stays on disk.");
        },[&](DialogAction action) {if(action==DialogAction::Confirm)pending_remove=remove_image_id;remove_dialog_active=false;return DialogOutcome::Success;},[&](DialogAction action) {if(action==DialogAction::Confirm)mark_dialog(GuiInputTarget::ConfirmRemoveImage,confirm_remove_offset,confirm_remove_size);if(action==DialogAction::Cancel)mark_dialog(GuiInputTarget::CancelRemoveImage,cancel_remove_offset,cancel_remove_size);});
        auto edit_dialog_path=[&](const char* label,ui::PathField& field) {
            copy_recovery_path(text_buffers->project_path,field.text());
            if(ui::DialogInput(label,text_buffers->project_path,sizeof(text_buffers->project_path))){field.set(text_buffers->project_path);field.refresh(path_context);}
            if(*field.text()) {
                if(!field.state().resolved.empty())ui::TextWrapped("Resolved path: %s",field.state().resolved.c_str());
                if(!field.state().problem.empty())ui::TextWrapped("%s",ui::i18n::text(field.state().problem.c_str()));
            }
        };
        for(auto* field:all_paths)field->draw_picker(path_context);
        if(recovery_popup_requested) {ui::OpenDialog(ui::DialogKind::ReplaceWorkspace);recovery_popup_requested=false;}
        ui::DialogState replacement_state(bool(staged_project),restore_pending || restore_after_save || save_job.busy(),staged_is_recovery?ui::DialogVariant::Recovery:ui::DialogVariant::Default);replacement_state.dismiss_requested=close_recovery_popup;
        const auto replacement=ui::RenderDialog(ui::DialogKind::ReplaceWorkspace,replacement_state,staged_project?(staged_is_recovery?staged_project->recovery_source():staged_project->path()):std::string{},{&project_error},[&](ui::DialogControls&) {
            ui::TextWrapped(staged_is_recovery?"Replace this workspace with the selected recovery? Save or discard its current unsaved changes first.":"Replace this workspace with the opened project? Save or discard its current unsaved changes first.");
            if(staged_project && staged_is_recovery && !staged_project->path().empty())ui::TextWrapped("Saved project location: %s",staged_project->path().c_str());
            edit_dialog_path("Current project destination",forms.destination);if(restore_pending)ui::TextDisabled("Preparing workspace...");
        },[&](DialogAction action) {
            if(action==DialogAction::Cancel) {staged_project.reset();recovery_dialog_active=false;return DialogOutcome::Success;}
            if(action==DialogAction::Discard) {autosave.cancel();restore_pending=true;}else {save_project_requested=true;restore_after_save=true;}
            return DialogOutcome::Pending;
        },[&](DialogAction action) {if(action==DialogAction::Cancel)mark_dialog(GuiInputTarget::CancelRecoveryReplacement,cancel_recovery_offset,cancel_recovery_size);if(action==DialogAction::Discard)mark_dialog(GuiInputTarget::DiscardRecoveryReplacement,discard_recovery_offset,discard_recovery_size);if(action==DialogAction::Save)mark_dialog(GuiInputTarget::SaveRecoveryReplacement,save_recovery_offset,save_recovery_size);});
        if(close_recovery_popup && (replacement.dismissed || !replacement.visible)) {close_recovery_popup=false;recovery_dialog_active=false;}
        if(close_popup_requested) {ui::OpenDialog(ui::DialogKind::CloseProject);close_popup_requested=false;close_edit_destination=close_edit_recovery=false;}
        ui::DialogState close_state(bool(options.project),close_action!=0);
        ui::RenderDialog(ui::DialogKind::CloseProject,close_state,options.project && !options.project->path().empty()?std::filesystem::u8path(options.project->path()).filename().u8string():ui::i18n::text("Untitled project"),{&project_error,&autosave_ui_error},[&](ui::DialogControls& controls) {
            ui::TextWrapped("Save your changes before closing.");ImGui::Spacing();
            if(!options.project || options.project->path().empty() || close_edit_destination || std::string(project_path)!=options.project->path())edit_dialog_path("Project destination",forms.destination);
            else {ui::ActionRow location(1);ui::TextDisabled("Save to the current project.");location.next();if(ui::IconButton("Change destination",ui::Icon::Open))close_edit_destination=true;}
            ImGui::Spacing();
            {ui::ActionRow recovery(1);ui::TextDisabled("Keep unsaved work in a recovery copy instead.");recovery.next();if(ui::IconButton("Change recovery destination###Recovery location",ui::Icon::Open))close_edit_recovery=!close_edit_recovery;}
            if(close_edit_recovery)edit_dialog_path("Recovery destination",paths.autosave);
            controls.auxiliary(DialogAction::KeepRecovery);
            if(close_action)ui::TextDisabled("Finishing save...");
        },[&](DialogAction action) {
            if(action==DialogAction::Cancel) {close_dialog_active=false;return DialogOutcome::Success;}
            if(action==DialogAction::KeepRecovery) {close_action=2;close_keep_started=false;}
            if(action==DialogAction::Discard) {autosave.clear();close_action=3;}
            if(action==DialogAction::Save) {save_project_requested=true;close_action=1;close_wait_only=false;}
            return DialogOutcome::Pending;
        },[&](DialogAction action) {if(action==DialogAction::Cancel)mark_dialog(GuiInputTarget::CancelClose,cancel_close_offset,cancel_close_size);if(action==DialogAction::Save)mark_dialog(GuiInputTarget::SaveClose,save_close_offset,save_close_size);if(action==DialogAction::Discard)mark_dialog(GuiInputTarget::DiscardClose,discard_close_offset,discard_close_size);if(action==DialogAction::KeepRecovery)mark_dialog(GuiInputTarget::KeepRecoveryClose,keep_close_offset,keep_close_size);});
        ImGui::End();
        spatial.draw(document,*selection,options.project.get(),&project_images,editing_blocked,has_image,mark_target,&record_removal,workspace_epoch,&output_gate,path_context);
        genes.draw(display,options.project.get(),active_image,editing_blocked || save_job.busy() || project_load.valid());
        fluorescence.draw(document,selection->primary(),editing_blocked || save_job.busy() || project_load.valid(),path_context,has_image);
        if(has_image)landmark_registration.draw(document,options.project?options.project->identity()+":"+std::to_string(active_image):"current-image",display,editing_blocked || save_job.busy() || project_load.valid());
        if(const auto imported=spatial_import->draw(!options.project || editing_blocked || project_load.valid() || save_job.busy() || maintenance_job.valid(),&notifications);!imported.empty()) {
            project_load_cancelled=false;loading_recovery=false;project_error.clear();forms.open_path.set(imported);
            project_load=std::async(std::launch::async,load_project,imported,false,std::string{});
        }
        if(tma.draw(document,*selection,options.project.get(),editing_blocked,has_image,mark_target,&output_gate,path_context)) batch.open_tma();
        const auto raw_change=raw.draw(document,*selection,options.project.get(),editing_blocked,mark_target,&record_removal,workspace_epoch,&output_gate,path_context,has_image);
        if(raw_change.image) {forms.image.set("");pending_image=raw_change.image;project_catalog_dirty=true;}
        if(!queued_raw_path.empty() && raw.state()!="changing") {forms.finish_one(raw_change.image?std::string{}:raw.error().empty()?"Image opening cancelled.":raw.error());queued_raw_path.clear();}
        if(raw_change.refresh) {
            mean.invalidate();previous_uploads+=tiles->uploads();previous_generations+=tiles->generations();previous_peak_bytes=std::max(previous_peak_bytes,tiles->peak_bytes());
            tiles=std::make_unique<GuiTiles>(document.server());overlay_cache.clear();overlay_display.reset();plan={};overview.reset();plan_inputs.reset();overview_center.reset();published_view_inputs.reset();
        }
        project_density.draw(options.project.get(),project_images,editing_blocked || save_job.busy() || project_load.valid(),path_context,&notifications);
        batch.draw(options.project.get(),active_image,selection.get(),editing_blocked,{tissue_parameters,tissue_grid,tissue_min_area,tissue_max_area,cell_parameters,cell_min_area,cell_max_area,cell_grid,cell_nominal,grid_cells,static_cast<AnalysisKind>(inputs.kind),inputs.roi_sampling,inputs.nucleus,pixel_classifier.model(),inputs.intensity_statistics,inputs.vectors,inputs.white},make_transform,path_context);
        raw_pixel.draw(document,*selection,options.project.get(),editing_blocked,has_image,mark_target,&record_removal,workspace_epoch,&output_gate,path_context);
        classifier.draw(document,*selection,editing_blocked,has_image,mark_target,&output_gate,path_context,&record_removal,workspace_epoch);
        const auto pixel_top_left=viewport.screen_to_image({0,0}),pixel_bottom_right=viewport.screen_to_image({previous_canvas_size.x,previous_canvas_size.y});
        pixel_classifier.draw(document,*selection,editing_blocked,has_image,mark_target,make_transform,{pixel_top_left.x,pixel_top_left.y,pixel_bottom_right.x-pixel_top_left.x,pixel_bottom_right.y-pixel_top_left.y},&record_removal,workspace_epoch,&output_gate,path_context,input_feedback.message(InputConsumer::Training,pixel_classifier.training_stats().generation));
        std::string removal_failure;record_removal.draw(document,workspace_epoch,removal_failure);
        if(!removal_failure.empty())notifications.post({ui::Severity::Error,"Remove kept result",removal_failure,{},false});
        if(save_project_requested && options.project && !project_path[0] && !close_dialog_active && !recovery_dialog_active) {
            save_project_requested=false;select_project_form(ProjectForm::Save);focus_save_project=true;
            if(workspace.enabled()) workspace.select(GuiWorkspace::project_pane);
        }
        notice_observers.observe({notifications,pixel_classifier,classifier,batch,fluorescence,
            analysis,stain_estimate,tissue,cell_job,fingerprint_job,paths,mean,tissue_preview,cell_preview,
            stain_model_io,output_gate,autosave_stats,workspace,overview,tiles.get(),shell.get(),document,selected});
        if(ui::notifications_trace && ui::notifications_trace->after_frame)ui::notifications_trace->after_frame(notifications);
        notifications.draw_activity();
        service_project(save_project_requested,autosave_requested,editing_blocked,true);
        profile.other_ms=profile_lap();
        if(shell)shell->set_toolbar_visible(shell_toolbar_placed);
        if(shell_draw_list) {
            shell_draw_list->AddCallback([](const ImDrawList*,const ImDrawCmd* cmd) {
                const auto& io=ImGui::GetIO();
                static_cast<ui::RmlShell*>(cmd->UserCallbackData)->render(static_cast<int>(io.DisplaySize.x*io.DisplayFramebufferScale.x),static_cast<int>(io.DisplaySize.y*io.DisplayFramebufferScale.y));
            },shell.get());
            shell_draw_list->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        }
        if(auto* trace=detail::analysis_inputs_trace) {
            const auto a=analysis.poll(document,selected);const auto t=tissue.poll(document,selected);const auto c=cell_job.poll(document,selected);const auto s=stain_estimate.poll(document,selected);const auto p=pixel_classifier.training_stats();
            trace->jobs={detail::AnalysisInputsTrace::Job{a.state,a.generation,analysis.busy(),bool(analysis.result())},
                detail::AnalysisInputsTrace::Job{t.state,t.generation,tissue.busy(),bool(tissue.result())},
                detail::AnalysisInputsTrace::Job{c.state,c.generation,cell_job.busy(),bool(cell_job.result())},
                detail::AnalysisInputsTrace::Job{p.state,p.generation,p.state=="running" || p.state=="cancelling",bool(pixel_classifier.table())},
                detail::AnalysisInputsTrace::Job{s.state,s.generation,stain_estimate.busy(),bool(stain_estimate.result())}};
            trace->estimate_applied=stain_estimate_applied;
            trace->training_trained=pixel_classifier.trained();trace->active_image=active_image;
            if(trace->after_frame)trace->after_frame(inputs,input_feedback);
        }
        if(ui::command_trace) {
            ui::command_trace->batch_state=batch.stats().state;ui::command_trace->batch_busy=batch.busy();ui::command_trace->fingerprint_busy=fingerprint_job.busy();
            ui::command_trace->draft_points=draft_points.size();ui::command_trace->project_browser_enter=project_browser_enter;
            ui::command_trace->recovery={autosave.busy(),autosave_stats.checkpoint_published,autosave_stats.cancel_requested,autosave_stats.retention_cancelled,autosave_stats.completed,autosave_stats.maintenance_phase};
            ui::command_trace->tool=tool;ui::command_trace->task=static_cast<int>(workspace.workflow_task());ui::command_trace->objects_visible=objects_visible;
            if(ui::command_trace->observe_task_jobs) {
                const auto region=analysis.poll(document,selected);
                const auto tissue_job=tissue.poll(document,selected);
                const auto cells=cell_job.poll(document,selected);
                ui::command_trace->task_jobs={ui::CommandTrace::TaskJob{region.state,region.generation,analysis.busy()},
                    ui::CommandTrace::TaskJob{tissue_job.state,tissue_job.generation,tissue.busy()},
                    ui::CommandTrace::TaskJob{cells.state,cells.generation,cell_job.busy()}};
            }
            if(ui::command_trace->after_frame)ui::command_trace->after_frame(command_inputs.context,command_inputs.fired);
        }
        ImGui::Render(); int w,h; glfwGetFramebufferSize(session.window,&w,&h); last_width=w; last_height=h; glViewport(0,0,w,h);
        glClearColor(.07f,.08f,.1f,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if(detail::analysis_inputs_trace && detail::analysis_inputs_trace->after_render)detail::analysis_inputs_trace->after_render();
        profile.render_ms=profile_lap();profile.visible=visible_count;
        rendered_window_width=static_cast<int>(io.DisplaySize.x);rendered_window_height=static_cast<int>(io.DisplaySize.y);
        auto cpu_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count();
        profile.total_ms=cpu_ms;profiler.add(profile);
        const bool sampled_frame=frames>=10 || startup_requested;
        if (sampled_frame) {
            if (frame_times.size()==4096) frame_times.erase(frame_times.begin(),frame_times.begin()+2048);
            frame_times.push_back(cpu_ms);
        }
        replay.finish_frame(!settling);
        if(settling) ++settle_frames;
        const bool tiles_settled=genes.settled() && raw_pixel.settled() && pixel_classifier.settled() && project_thumbnails.settled() && !startup_active && !project_load.valid() && !qupath_review.valid() && !restore_pending && (!canvas_visible || (!tiles->pending() && tiles->error().empty())) && (!has_image || overview.settled());
        if(options.settle_tiles && !within_frames() && !tiles->error().empty()) throw std::runtime_error("GUI tile settlement failed: "+tiles->error());
        const bool final_frame=!within_frames() && !restore_after_save && !restore_pending && (!options.settle_tiles || tiles_settled);
        if (!capture_path.empty() && final_frame) capture(capture_path,w,h);
        glfwSwapBuffers(session.window); ++frames;
        if(startup_pending) {
            startup_pending=false;
            if(project_load_cancelled) {
                startup_active=false; startup_cancelled=true; startup_finish_frame=replay.frame();
            } else {
                // Paint the initial workspace before any file I/O starts. The
                // worker owns only native factories, metadata and atomics.
                auto kind=options.startup_kind,path=options.startup_path,base=options.recovery_base_path,reader=options.image_reader;
                const auto tiff_options=options.tiff_options?*options.tiff_options:TiffOpenOptions{};
                const auto image_id=options.startup_image_id;
                project_load=std::async(std::launch::async,[kind,path,base,reader,tiff_options,image_id,progress=startup_progress] {
                    const auto start=std::chrono::steady_clock::now();
                    std::shared_ptr<Project> candidate;
                    if(kind=="image") {
                        if(progress->cancelled.load()) throw std::runtime_error("image opening cancelled");
                        candidate=std::make_shared<Project>();const auto route=detail::desktop_startup_selection(path,reader);if(route.raw_route){detail::ReaderOpenOptions reader_options;reader_options.tiff=tiff_options;candidate->add_tiff_source(detail::open_reader(path,route.selection,reader_options,&progress->cancelled));}else candidate->add_image(path,route.selection.provider_id);
                        progress->total_images.store(1); progress->images.store(1);
                    } else candidate=Project::open_impl(path,kind=="recovery",base,progress.get());
                    if(progress->cancelled.load()) throw std::runtime_error("opening cancelled");
                    if(image_id) candidate->set_active_image(image_id);
                    if(candidate->active_image() && !progress->cancelled.load()) candidate->document(candidate->active_image())->query_region({0,0,0,0});
                    progress->phase.store(5);
                    return LoadedProject{std::move(candidate),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),kind=="recovery"};
                });
            }
        }
        if(sampled_frame) {
            const auto wall_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count();
            if(frame_wall_times.size()==4096) frame_wall_times.erase(frame_wall_times.begin(),frame_wall_times.begin()+2048);
            frame_wall_times.push_back(wall_ms);
        }
        if(final_frame) {finite_run_finished=true;break;}
    }
    qupath_cancel->store(true);
    // Finite embedded runs also return a durable save result and reconcile the
    // caller's project. No live authority or Python-owned objects enter workers.
    // Drain a completed finite embedding; a confirmed early close cancels even
    // when a replay frame cap was supplied. Both paths join before retirement.
    spatial.finish(*active_document,finite_run_finished);
    tma.finish(frame_limit>0);
    raw.finish(active_document,frame_limit>0);
    batch.finish();
    project_density.finish();
    classifier.finish(*active_document,*selection,frame_limit>0);
    pixel_classifier.finish(*active_document,*selection,frame_limit>0);raw_pixel.finish(*active_document,frame_limit>0);
    if(stain_estimate.busy()) {if(!frame_limit) stain_estimate.cancel();stain_estimate.wait(*active_document,selection->primary());}
    if(save_job.busy()) save_job.wait();
    if(cell_table_job.busy()) cell_table_job.wait();
    if(measurement_job.busy()) {if(frame_limit==0) measurement_job.cancel();measurement_job.wait();}
    if(!frame_limit) fingerprint_job.cancel();
    const auto final_fingerprint=fingerprint_job.wait();
    reconcile_save();
    if(live) live->finish(live_context());
    if(autosave.busy()) autosave.wait();
    if(maintenance_job.valid()) {maintenance_job.wait(); poll_maintenance(true);}
    if(retired_workspace.valid()) max_retirement_ms=std::max(max_retirement_ms,retired_workspace.get());
    profiler.save();
    std::sort(frame_times.begin(),frame_times.end());
    std::sort(frame_wall_times.begin(),frame_wall_times.end());
    auto p95=frame_times.empty()?0:frame_times[static_cast<std::size_t>((frame_times.size()-1)*.95)];
    const bool empty=options.project && !active_image;
    const auto analysis_status=analysis.poll(*active_document,selection->primary());
    const auto final_estimate=stain_estimate.poll(*active_document,selection->primary());const auto estimate_value=stain_estimate.result();
    const auto estimate_result=estimate_value?std::make_shared<const StainEstimateResult>(*estimate_value):std::shared_ptr<const StainEstimateResult>{};
    const auto analysis_result=analysis.result();
    const auto saved_analysis=active_document->latest_analysis(selection->primary());
    const auto tissue_status=tissue.poll(*active_document,selection->primary());
    const auto tissue_result=tissue.result();
    const auto tissue_contours=tissue.contours();
    const auto saved_tissue=active_document->latest_tissue(selection->primary());
    const auto final_cells=cell_job.poll(*active_document,selection->primary());
    const auto saved_cells=active_document->latest_cells(selection->primary());
    const auto final_table=cell_table_job.poll();
    const auto final_measurement=measurement_job.poll();
    if(!cell_job.result()) cell_preview.clear();
    if(!tissue_result) tissue_preview.clear();
    glfwGetFramebufferSize(session.window,&last_width,&last_height);
    workspace.protect(active_document->server()->source_locator());
    if(options.project && !options.layout_path.empty()) {
        workspace.protect(options.project->path());workspace.protect(options.project->recovery_source());
        for(const auto& image:options.project->images()) workspace.protect(options.project->document(image.id)->server()->source_locator());
    }
    workspace.gene_preference=genes.preference();
    const bool layout_saved=workspace.save();
    GuiReport report{frames,previous_uploads+tiles->uploads(),std::max(previous_peak_bytes,tiles->peak_bytes()),previous_generations+tiles->generations(),p95,tiles->pending(),empty?0:viewport.zoom,empty?Point{}:viewport.origin,
        empty?Point{}:viewport.screen_to_image({previous_canvas_size.x/2,previous_canvas_size.y/2}),last_width,last_height,
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)),reinterpret_cast<const char*>(glGetString(GL_VERSION)),visible_count,selection->ids(),selection->primary(),active_image,image_switches,max_image_switch_ms,project_error,
        autosave.stats().completed,autosave.stats().path,autosave.stats().error.empty()?autosave_ui_error:autosave.stats().error,autosave.stats().capture_ms,frame_times.empty()?0:frame_times.back(),project_replacements,project_load_ms,images_removed,
        save_job.stats().completed,save_job.stats().capture_ms,save_job.stats().write_ms,recovery_load_ms,checkpoints_deleted,maintenance_error,retired_workspaces,max_retirement_ms,
        autosave.stats().checkpoints_pruned,autosave.stats().retention_ms,autosave.stats().retention_error,
        autosave.stats().retention_cancelled,autosave.stats().retention_inspected,autosave.stats().retention_metadata_ms,autosave.stats().retention_ownership_ms,autosave.stats().retention_removal_ms,autosave.stats().retention_prefetch_peak,
        startup_succeeded,startup_cancelled,startup_frames,startup_progress->images.load(),startup_progress->objects.load(),startup_ms,
        analysis_status.state,analysis_status.error,analysis_result?std::vector<double>(analysis_result->means.begin(),analysis_result->means.end()):std::vector<double>{},
        analysis_result?analysis_result->samples:0,analysis_result?analysis_result->object_id:0,analysis_status.completed_tiles,analysis_status.elapsed_ms,
        active_document->analysis_record_count(),saved_analysis?active_document->analysis_status(saved_analysis->id):std::string{},
        tissue_status.state,tissue_status.error.empty()?tissue_preview.error():tissue_status.error,tissue_result?tissue_result->input_sha256():std::string{},
        tissue_result?tissue_result->components().size():0,tissue_preview.peak_bytes(),tissue_result?tissue_result->samples():0,tissue_result?tissue_result->foreground_pixels():0,
        tissue_status.elapsed_ms,tissue_preview.ready(),tissue_result?tissue_result->level():-1,tissue_result && !tissue_result->scientific_grid()?tissue_result->downsample():0,
        tissue_result?tissue_result->width():0,tissue_result?tissue_result->height():0,tissue_result?tissue_result->cleaned_pixels():0,
        tissue_result?tissue_result->filled_pixels():0,tissue_result?tissue_result->cleanup_ms():0,
        replay.frame(),replay.waits(),settle_frames,replay.complete(),!startup_active && !project_load.valid() && !qupath_review.valid() && !restore_pending && project_thumbnails.settled() && (!canvas_visible || (!tiles->pending() && tiles->error().empty())) && (empty || overview.settled()),
        tissue_contours?tissue_contours->components().size():0,tissue_contours?tissue_contours->edge_count():0,tissue_contours?tissue_contours->vertex_count():0,
        tissue.contours_published()?tissue_annotations_added:0,tissue_contours?tissue_contours->elapsed_ms():0,tissue.contours_published(),
        saved_tissue?saved_tissue->id:0,saved_tissue?active_document->tissue_status(saved_tissue->id):std::string{},
        final_cells.state,final_cells.error.empty()?cell_preview.error():final_cells.error,cell_job.result(),cell_job.published()?cell_annotations_added:0,cell_preview.peak_bytes(),cell_preview.ready(),cell_job.published(),
        overlay_cache.bytes(),overlay_cache.peak_bytes,overlay_cache.builds,overlay_cache.hits,overlay_cache.fallbacks,
        saved_cells?saved_cells->id:0,saved_cells?active_document->cell_status(saved_cells->id):std::string{},
        final_table.state,cell_table_error.empty()?final_table.error:cell_table_error,final_table.path,final_table.rows,final_table.bytes,final_table.capture_ms,final_table.write_ms,
        final_fingerprint.state,final_fingerprint.error.empty()?fingerprint_error:final_fingerprint.error,fingerprint_job.result(),final_fingerprint.bytes,final_fingerprint.elapsed_ms,
        final_measurement.state,measurement_error.empty()?final_measurement.error:measurement_error,final_measurement.rows,final_measurement.capture_ms,final_measurement.write_ms,fingerprint_job.association(),final_estimate.state,stain_estimate_error.empty()?final_estimate.error:stain_estimate_error,estimate_result,stain_estimate_applied,batch.stats().state,batch.error(),batch.result(),suspended_iterations,suspended_work_ms,suspended_wait_ms,bool(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED)),overview.state(),overview.error(),overview.result(),overview.bytes(),overview.peak_bytes(),workspace.enabled(),workspace.loaded(),layout_saved,workspace.error(),workspace.browser,workspace.display,workspace.measurements,workspace.canvas,canvas_visible,ui_scale.monitor,ui_scale.framebuffer,ui_scale.logical,font_rebuilds,ui::layout_stats.stacked_fields,ui::layout_stats.wrapped_buttons,ui::layout_stats.overflow_items,ui::layout_stats.max_overflow,ui::layout_stats.first_overflow,classifier.stats().state,classifier.error(),classifier.io_state(),classifier.io_error(),classifier.model(),classifier.result(),classifier.changed(),classifier.capture_ms(),classifier.record_id(),pixel_classifier.training_stats().state,pixel_classifier.prediction_stats().state,pixel_classifier.error(),pixel_classifier.io_state(),pixel_classifier.io_error(),pixel_classifier.model(),pixel_classifier.table(),pixel_classifier.result(),pixel_classifier.preview_ready(),canvas_visible && pixel_classifier.overlay_visible(),pixel_classifier.preview().bytes(),pixel_classifier.preview().peak_bytes(),pixel_classifier.preview().completed_tiles(),pixel_classifier.preview().generation(),pixel_classifier.capture_ms(),pixel_classifier.catalog_inspected(),pixel_classifier.catalog_max_step(),pixel_classifier.catalog_complete(),Rect{previous_canvas.x,previous_canvas.y,previous_canvas_size.x,previous_canvas_size.y},pixel_classifier.contour_state(),pixel_classifier.traced(),pixel_classifier.published_ids(),pixel_classifier.contours_published(),pixel_classifier.archives().state(),pixel_classifier.archives().kept(),pixel_classifier.archives().selected(),pixel_classifier.historical_result(),pixel_classifier.archives().max_step(),pixel_classifier.archives().complete(),ignored_physical_wheels,std::make_shared<const PixelStreamExportStats>(pixel_classifier.stream_stats()),std::make_shared<const PixelStreamReadStats>(pixel_classifier.browser().stats()),pixel_classifier.browser().preview().bytes(),pixel_classifier.browser().preview().peak_bytes(),pixel_classifier.browser().preview().ready(),pixel_classifier.browser().preview_bounds(),pixel_classifier.browser().kept(),pixel_classifier.browser().selected(),pixel_classifier.browser().catalog_max_step(),pixel_classifier.browser().catalog_complete()};
    report.spatial=std::make_shared<const GuiSpatialReport>(spatial.report());
    report.tma_error=tma.error();report.tma_export_state=tma.export_state();report.tma_created_ids=tma.created();report.tma_cores=active_document->tma_core_count();report.tma_missing_cores=report.tma_cores-active_document->tma_core_ids().size();report.tma_catalog_max_step=tma.catalog_max_step();report.tma_metadata_bytes=tma.metadata_bytes();report.tma_dearray_result=tma.dearray_result();report.qptma_state=tma.qptma_state();report.qptma_preview_cores=tma.qptma_cores();report.qptma_preview_missing=tma.qptma_missing();report.qptma_preview_max_step=tma.qptma_max_step();report.qptma_export_bytes=tma.qptma_bytes();
    report.tiff_state=raw.state();report.raw_task_state=raw.task_state();report.raw_error=raw.error();report.raw_statistics=raw.statistics();report.tiff_view_changes=raw.changes();report.raw_export_bytes=raw.export_bytes();report.raw_catalog_max_step=raw.catalog_max_step();report.raw_analysis_result=raw.analysis_result();report.raw_analysis_state=raw.analysis_state();report.raw_csv_state=raw.csv_state();report.raw_csv_rows=raw.csv_rows();report.raw_csv_bytes=raw.csv_bytes();report.raw_record_id=raw.record_id();report.raw_records=active_document?active_document->raw_analysis_record_count():0;report.raw_record_status=active_document?raw.record_status(*active_document):std::string{};report.overview_source_pixel_limit=overview.source_pixel_limit();
    report.raw_pixel=std::make_shared<const GuiRawPixelReport>(raw_pixel.report());
    report.rendered_frame_wall_samples=frame_wall_times.size();
    report.rendered_frame_wall_p95_ms=frame_wall_times.empty()?0:frame_wall_times[static_cast<std::size_t>((frame_wall_times.size()-1)*.95)];
    report.max_rendered_frame_wall_ms=frame_wall_times.empty()?0:frame_wall_times.back();
    report.clipboard_copies=clipboard_copies;report.clipboard_pastes=clipboard_pastes;
    report.clipboard_copied_objects=clipboard_copied_objects;report.clipboard_pasted_objects=clipboard_pasted_objects;
    report.stain_model_state=stain_model_io.state();report.stain_model_error=stain_model_io.error();report.stain_model_sha256=stain_model_io.digest();
    report.clipboard_error=clipboard_error;
    if(const auto measured=analysis.result()) report.analysis_intensity_statistics=measured->intensity_statistics;
    // Existing replay message fields expose input cancellations without changing
    // the public report ABI shared with the retained native runtime.
    const auto cancellation_report=[&](InputConsumer consumer,std::uint64_t generation,std::string& message) {
        message=input_feedback.report_message(consumer,generation,message);
    };
    cancellation_report(InputConsumer::Region,analysis.poll(*active_document,selected).generation,report.analysis_error);
    cancellation_report(InputConsumer::Tissue,tissue.poll(*active_document,selected).generation,report.tissue_error);
    cancellation_report(InputConsumer::Cells,cell_job.poll(*active_document,selected).generation,report.cell_error);
    cancellation_report(InputConsumer::Training,pixel_classifier.training_stats().generation,report.pixel_error);
    cancellation_report(InputConsumer::StainEstimation,final_estimate.generation,report.stain_estimate_error);
    if(tissue_result) {
        if(tissue_result->scientific_grid()) report.tissue_grid=*tissue_result->scientific_grid();
        report.tissue_downsample_x=tissue_result->downsample_x();report.tissue_downsample_y=tissue_result->downsample_y();
    }
    if(detail::analysis_inputs_trace && detail::analysis_inputs_trace->report)detail::analysis_inputs_trace->report(report);
    return report;
}
namespace {
QPX_GUI_NOINLINE void GuiNoticeObservers::observe(NoticeSources sources) {
    const auto& [notifications,pixel_classifier,classifier,batch,fluorescence,
        analysis,stain_estimate,tissue,cell_job,fingerprint_job,paths,mean,tissue_preview,cell_preview,
        stain_model_io,output_gate,autosave_stats,workspace,overview,tiles,shell,document,selected]=sources;
        const auto observe_notice=[&](std::size_t i,const char* source,const auto& status,std::string_view path={},std::uint64_t generation=0) {
            const auto state=ui::parse_run_state(status.state);
            notice_jobs[i].observe(notifications,source,status.state,state==ui::RunState::Ready || state==ui::RunState::Verified,state==ui::RunState::Failed || state==ui::RunState::Partial,status.error,path,generation);
        };
        const auto pixel_source=ui::command(ui::CommandId::Tool_PixelClassifier).caption;
        const auto& training_notice=pixel_classifier.training_stats();
        notice_jobs[0].observe(notifications,pixel_source,training_notice.state,training_notice.state=="ready",training_notice.state=="failed",training_notice.error,{},training_notice.generation);
        const auto& prediction_notice=pixel_classifier.prediction_stats();
        notice_jobs[1].observe(notifications,pixel_source,prediction_notice.state,prediction_notice.state=="ready",prediction_notice.state=="failed",prediction_notice.error,{},prediction_notice.generation);
        const auto& object_notice=classifier.stats();
        notice_jobs[2].observe(notifications,ui::command(ui::CommandId::Tool_ObjectClassifier).caption,object_notice.state,object_notice.state=="ready",object_notice.state=="failed",object_notice.error,{},object_notice.generation);
        const auto& batch_notice=batch.stats();
        const auto batch_report=batch.result();
        notice_jobs[3].observe(notifications,ui::command(ui::CommandId::Tool_Batch).caption,batch_notice.state,batch_notice.state=="succeeded",batch_notice.state=="failed" || batch_notice.state=="partial",batch_notice.error,batch_report?std::string_view(batch_report->report_path):std::string_view{},batch.notice_generation());
        const auto& fluorescence_state=fluorescence.state();
        const auto fluorescence_error_key=fluorescence.notification_error_key();
        const auto fluorescence_report=fluorescence.tiled_result();
        const std::string_view fluorescence_path=fluorescence_report?std::string_view(fluorescence_report->checkpoint_path):std::string_view{};
        if(fluorescence_state=="unresolved" && notice_jobs[4].state!=fluorescence_state)notifications.post({ui::Severity::Warning,ui::command(ui::CommandId::Tool_Fluorescence).caption,"Some results remain unresolved.",std::string(fluorescence_path),true});
        notice_jobs[4].observe(notifications,ui::command(ui::CommandId::Tool_Fluorescence).caption,fluorescence_state,
            fluorescence_state=="ready" || fluorescence_state=="verified",fluorescence_state=="failed" || fluorescence_error_key!=nullptr,fluorescence_error_key?std::string_view(fluorescence_error_key):std::string_view(fluorescence.error()),fluorescence_path,0,fluorescence_error_key!=nullptr);
        const auto region_notice=analysis.poll(document,selected);const auto stain_notice=stain_estimate.poll(document,selected);
        const auto tissue_notice=tissue.poll(document,selected);const auto cell_notice=cell_job.poll(document,selected);
        observe_notice(5,ui::workflow_task_labels[1],region_notice,{},region_notice.generation);
        observe_notice(6,ui::workflow_task_labels[2],tissue_notice,{},tissue_notice.generation);
        observe_notice(7,ui::workflow_task_labels[3],cell_notice,{},cell_notice.generation);
        observe_notice(8,"Stain estimation",stain_notice,{},stain_notice.generation);
        const auto fingerprint_notice=fingerprint_job.poll();
        notice_jobs[9].observe(notifications,ui::workflow_task_labels[6],fingerprint_notice.state,false,fingerprint_notice.state=="failed" && paths.fingerprint_output.state().resolved.empty(),fingerprint_notice.error);
        const auto observe_error=[&](std::size_t i,const char* source,const std::string& message) {
            if(noticed_errors[i]==message)return;noticed_errors[i]=message;
            if(!message.empty())notifications.post({ui::Severity::Error,source,message,{}});
        };
        observe_error(0,ui::workflow_task_labels[0],mean.error);
        observe_error(1,ui::workflow_task_labels[2],tissue_preview.error());
        observe_error(2,ui::workflow_task_labels[3],cell_preview.error());
        const bool stain_exporting=stain_model_io.operation()==detail::GuiStainModel::Operation::Save;
        notice_jobs[10].observe(notifications,"Stain model",stain_model_io.state(),false,stain_model_io.state()=="failed" && !stain_exporting,stain_model_io.error());
        const auto observe_model_load=[&](std::size_t i,const char* source,const std::string& state,const std::string& message) {
            const bool exporting=notice_jobs[i].state=="saving" || output_gate.watching(source);
            notice_jobs[i].observe(notifications,source,state,state=="loaded",state=="failed" && !exporting,message);
        };
        observe_model_load(11,ui::command(ui::CommandId::Tool_ObjectClassifier).caption,classifier.io_state(),classifier.io_error());
        observe_model_load(12,pixel_source,pixel_classifier.io_state(),pixel_classifier.io_error());
        observe_error(3,"Project",autosave_stats.error);
        observe_error(4,"Project",autosave_stats.retention_error);
        observe_error(5,"Project",workspace.error());
        observe_error(6,"Image",overview.error());
        observe_error(7,"Image",tiles->error());
        observe_error(8,"Project",shell?shell->error():std::string{});
}
}
#undef QPX_GUI_NOINLINE
GuiReport run_project_gui(std::shared_ptr<Project> project,int frame_limit,const std::string& capture_path,const GuiOptions& options) {
    if(!project) throw std::invalid_argument("GUI requires a project");
    if(options.project && options.project!=project) throw std::invalid_argument("GUI options refer to a different project");
    auto configured=options; configured.project=project;
    if(project->active_image()) {
        // run_gui owns the active binding. Avoid retaining a temporary shared
        // pointer to the first image for the entire desktop session after it
        // has been removed or the project has been replaced.
        auto* initial=project->document(project->active_image()).get();
        return run_gui(*initial,frame_limit,capture_path,configured);
    }
    Document empty(Image(1,1,std::vector<std::uint8_t>{0,0,0}));
    return run_gui(empty,frame_limit,capture_path,configured);
}
GuiReport run_viewer_session(std::shared_ptr<ViewerSession> session,int frames,const std::string& capture,const GuiOptions& options) {
    if(!session) throw std::invalid_argument("viewer session is required");
    if(options.project || options.viewer_owner || !options.startup_kind.empty() || !options.startup_path.empty() || options.startup_image_id) throw std::invalid_argument("managed viewer owns its project; project/startup overrides are not allowed");
    detail::ViewerOwner owner(*session);
    try {auto project=std::make_shared<Project>();auto configured=options;configured.viewer_owner=&owner;configured.startup_kind="project";configured.startup_path=session->project_path();configured.save_snapshot_budget=session->limits().snapshot_bytes;return run_project_gui(std::move(project),frames,capture,configured);}
    catch(const std::exception& e) {owner.fail(e.what());throw;}
}

}
