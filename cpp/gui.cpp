#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_recovery.hpp"
#include "qupathxx/project_save.hpp"
#include "qupathxx/interchange.hpp"
#include "qupathxx/analysis.hpp"
#include "gui_profile.hpp"
#include "gui_tiles.hpp"
#include "gui_thumbnail.hpp"
#include "gui_workspace.hpp"
#include "gui_scale.hpp"
#include "gui_tissue.hpp"
#include "qupathxx/cells.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "gui_replay.hpp"
#include "viewer_bridge.hpp"
#include <mutex>
#include "gui_overlay.hpp"
#include "gui_batch.hpp"
#include "gui_layout.hpp"
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
#include <cstring>
#include <limits>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace qpx {
namespace {
std::mutex viewer_mutex;
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
    if(geometry.kind()==RoiKind::Rectangle) { draw->AddRect(p0,p1,color,0,0,2); return; }
    if(geometry.kind()==RoiKind::Ellipse) {
        draw->AddEllipse({(p0.x+p1.x)/2,(p0.y+p1.y)/2},{(p1.x-p0.x)/2,(p1.y-p0.y)/2},color,0,0,2); return;
    }
    for(const auto& path:geometry.paths()) {
        if(geometry.kind()==RoiKind::Points) { for(auto p:path) draw->AddCircleFilled(screen(view,p,offset),4,color); continue; }
        std::vector<ImVec2> vertices; vertices.reserve(path.size());
        for(auto p:path) vertices.push_back(screen(view,p,offset));
        draw->AddPolyline(vertices.data(),static_cast<int>(vertices.size()),color,geometry.is_area()?ImDrawFlags_Closed:0,2);
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
}
GuiReport run_gui(Document& document, int frame_limit, const std::string& capture_path, const GuiOptions& options) {
    std::unique_lock<std::mutex> viewer_lock(viewer_mutex,std::try_to_lock);
    if(!viewer_lock.owns_lock()) throw std::runtime_error("a viewer is already running");
    if (frame_limit < 0) throw std::invalid_argument("frame_limit must be nonnegative");
    if (!capture_path.empty() && !frame_limit) throw std::invalid_argument("capture_path requires a positive frame_limit");
    if(options.settle_tiles && !frame_limit) throw std::invalid_argument("tile settlement requires a positive frame_limit");
    GuiReplay replay(options.inputs,options.replay_timeout_seconds);
    detail::GuiProfiler profiler;
    detail::GuiBatch batch(options);
    detail::GuiObjectClassifier classifier(options);
    GuiReplayDeadline settlement(options.replay_timeout_seconds);
    GuiReplayDeadline minimized_startup(options.replay_timeout_seconds);
    options.display.validate();
    if(options.analysis_vectors.size()!=6 || options.analysis_white.size()!=3 || options.analysis_kind<0 || options.analysis_kind>1)
        throw std::invalid_argument("invalid initial analysis parameters");
    auto analysis_vectors=options.analysis_vectors,analysis_white=options.analysis_white;
    std::optional<std::array<ColorVector,3>> estimated_basis;ColorVector estimated_white{};
    auto make_transform=[&](bool separate) {
        bool same=separate && bool(estimated_basis);
        if(same) for(std::size_t c=0;c<3;++c) same=same && analysis_white[c]==estimated_white[c] && analysis_vectors[c]==(*estimated_basis)[0][c] && analysis_vectors[c+3]==(*estimated_basis)[1][c];
        if(same) return StainTransform::from_normalized(*estimated_basis,estimated_white);
        return StainTransform(separate?std::vector<ColorVector>{{analysis_vectors[0],analysis_vectors[1],analysis_vectors[2]},
        {analysis_vectors[3],analysis_vectors[4],analysis_vectors[5]}}:std::vector<ColorVector>{{1,0,0},{0,1,0}},
        {analysis_white[0],analysis_white[1],analysis_white[2]});};
    make_transform(options.analysis_kind==1);
    TissueParameters tissue_parameters;
    tissue_parameters.feature=static_cast<TissueFeature>(options.tissue_feature);tissue_parameters.threshold=options.tissue_threshold;
    tissue_parameters.connectivity=options.tissue_connectivity;tissue_parameters.min_pixels=options.tissue_min_pixels;tissue_parameters.max_pixels=options.tissue_max_pixels;
    tissue_parameters.level=options.tissue_level;tissue_parameters.open_radius=options.tissue_open_radius;tissue_parameters.close_radius=options.tissue_close_radius;
    tissue_parameters.fill_holes=options.tissue_fill_holes;tissue_parameters.max_hole_pixels=options.tissue_max_hole_pixels;tissue_parameters.validate();
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
    cell_table_options.byte_limit=options.cell_table_byte_limit;cell_table_options.snapshot_budget=options.cell_table_snapshot_budget;cell_table_options.validate();
    for(const auto* path:{&options.file_fingerprint_path,&options.file_fingerprint_output}) if(path->size()>32768 || path->find('\0')!=std::string::npos) throw std::invalid_argument("invalid file fingerprint path");
    FileFingerprintOptions fingerprint_options;fingerprint_options.byte_limit=options.file_fingerprint_byte_limit;fingerprint_options.buffer_bytes=options.file_fingerprint_buffer_bytes;fingerprint_options.validate();
    bool fingerprint_associate=options.file_fingerprint_associate_source;
    FileFingerprintJob fingerprint_job;std::string fingerprint_error;
    char fingerprint_path[32769]={},fingerprint_output[32769]={};
    std::copy(options.file_fingerprint_path.begin(),options.file_fingerprint_path.end(),fingerprint_path);
    std::copy(options.file_fingerprint_output.begin(),options.file_fingerprint_output.end(),fingerprint_output);
    if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
    Session session;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    session.window = glfwCreateWindow(options.width, options.height, "qupathxx | Bioimage analysis", nullptr, nullptr);
    if (!session.window) throw std::runtime_error("OpenGL window creation failed");
    glfwMakeContextCurrent(session.window); glfwSwapInterval(1);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); session.context = true;ui::begin_frame();
    ImGui::GetIO().IniFilename=nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if(options.docked_workspace) ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    GuiWorkspace workspace(options);
    detail::GuiPixelClassifier pixel_classifier(options);
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle(); style.FrameRounding = 4; style.WindowRounding = 0;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.075f, .085f, .105f, 1);
    const ImGuiStyle base_style=style;
    double ui_multiplier=options.ui_scale;
    GuiScale ui_scale,font_scale;bool fonts_initialized=false;std::uint64_t font_rebuilds=0;
    session.platform = ImGui_ImplGlfw_InitForOpenGL(session.window, true);
    if (!session.platform) throw std::runtime_error("ImGui GLFW initialization failed");
    session.renderer = ImGui_ImplOpenGL3_Init("#version 150");
    if (!session.renderer) throw std::runtime_error("ImGui OpenGL initialization failed");
    GLint max_texture = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    if (max_texture<256) throw std::runtime_error("GPU does not support 256-pixel tile textures");
    // Private UI backing only: never insert this into a project or request its
    // pixels. It keeps empty-state bindings separate from any removed document.
    auto empty_document=std::make_shared<Document>(Image(1,1,std::vector<std::uint8_t>{0,0,0}));
    Document* active_document=options.project && !options.project->active_image()?empty_document.get():&document;
    auto tiles=std::make_unique<GuiTiles>(active_document->server());
    std::shared_ptr<Document> active_owner=options.project && options.project->active_image()?options.project->document(options.project->active_image()):nullptr;
    auto active_image=options.project?options.project->active_image():0;
    MeanJob mean;
    AnalysisJob analysis;StainEstimateJob stain_estimate;bool stain_estimate_applied=false;std::string stain_estimate_error;
    StainEstimateParameters stain_estimate_parameters=options.stain_estimate_parameters?*options.stain_estimate_parameters:StainEstimateParameters{};stain_estimate_parameters.validate();
    TissueJob tissue;
    CellJob cell_job;TissuePreview cell_preview;
    auto provenance_info=[](const std::shared_ptr<const AnalysisProvenance>& p) {
        if(!p) {ImGui::TextWrapped("Source/calibration history unavailable");return;}
        ImGui::TextWrapped("Recorded with qupathxx %s",p->environment->app_version.c_str());
        if(p->effective_pixel_width_um && p->effective_pixel_height_um) ImGui::TextWrapped("Captured calibration: %.5g / %.5g um/px",*p->effective_pixel_width_um,*p->effective_pixel_height_um);
        else ImGui::TextWrapped("Captured calibration: unknown");
    };
    if(options.measurement_table_path.size()>32768 || options.measurement_table_path.find('\0')!=std::string::npos || options.measurement_table_kind<0 || options.measurement_table_kind>1) throw std::invalid_argument("invalid measurement table options");
    MeasurementTableOptions measurement_options;measurement_options.require_current=options.measurement_table_require_current;
    measurement_options.byte_limit=options.measurement_table_byte_limit;measurement_options.snapshot_budget=options.measurement_table_snapshot_budget;measurement_options.validate();
    MeasurementTableExportJob measurement_job;std::string measurement_error;
    int measurement_kind=options.measurement_table_kind;
    char measurement_path[32769]={};std::copy(options.measurement_table_path.begin(),options.measurement_table_path.end(),measurement_path);
    CellTableExportJob cell_table_job;std::string cell_table_error;
    char cell_table_path[32769]={};std::copy(options.cell_table_path.begin(),options.cell_table_path.end(),cell_table_path);
    OverlayCache overlay_cache;std::optional<DisplaySettings> overlay_display;
    CellParameters cell_parameters=options.cell_parameters?*options.cell_parameters:CellParameters{};cell_parameters.validate();
    int cell_feature=static_cast<int>(cell_parameters.feature),cell_connectivity=cell_parameters.connectivity==8?1:0,cell_preview_body=0;
    bool show_cells=true;std::size_t cell_annotations_added=0;
    std::size_t tissue_annotations_added=0;
    std::uint64_t kept_tissue_generation=0,verifying_tissue=0;
    std::uint64_t kept_cell_generation=0,verifying_cells=0;
    TissuePreview tissue_preview;
    bool show_tissue=true;
    int tissue_feature=options.tissue_feature,tissue_connectivity=options.tissue_connectivity==8?1:0;
    int analysis_kind=options.analysis_kind;
    bool analysis_nucleus=options.analysis_nucleus;
    std::uint64_t kept_analysis_generation=0,pending_analysis_recheck=0;
    Viewport viewport;
    if(options.start_view) viewport=*options.start_view;
    bool fit = true, drawing = false;
    if(options.start_view) fit=false;
    int tool=0,previous_tool=0;
    Ring draft_points;
    std::vector<Ring> vertex_paths;
    std::uint64_t vertex_id=0,vertex_revision=0,hole_id=0,hole_revision=0;
    std::optional<std::pair<std::size_t,std::size_t>> active_vertex;
    bool vertex_dragging=false;
    Point vertex_cursor;
    int boolean_operation=0;
    Point anchor;
    auto selection=std::make_unique<ObjectSelection>(*active_document);
    std::uint64_t selected = 0,list_anchor=0;
    bool marquee=false,marquee_additive=false;
    Point marquee_anchor;
    std::string batch_class;
    std::string error;
    char project_path[32769]={};
    std::string project_error;
    ProjectAutosave autosave(options.autosave_snapshot_budget);
    RecoveryRetention retention;
    retention.enabled=options.recovery_retention_enabled; retention.keep_latest=options.recovery_keep_latest;
    retention.max_age_days=options.recovery_max_age_days; autosave.set_retention(retention);
    ProjectSaveJob save_job(options.save_snapshot_budget);
    bool save_result_pending=false,close_wait_only=false;
    AutosaveStats autosave_stats;
    double autosave_interval=options.autosave_interval_seconds;
    auto next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
    char autosave_path[32769]={},recovery_path[32769]={},recovery_base_path[32769]={};
    auto copy_recovery_path=[](char* buffer,const std::string& path) {
        if(path.size()>32768 || path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid recovery path");
        std::memcpy(buffer,path.c_str(),path.size()+1);
    };
    copy_recovery_path(autosave_path,options.autosave_path); copy_recovery_path(recovery_path,options.recovery_path);
    copy_recovery_path(recovery_base_path,options.recovery_base_path);
    std::string autosave_ui_error;
    std::future<std::vector<RecoveryInfo>> recovery_scan;
    std::string recovery_scan_base;
    std::uint64_t recovery_scan_generation=0;
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
    struct LoadedProject { std::shared_ptr<Project> project; double ms=0; bool recovery=false; };
    auto load_project=[](const std::string& path,bool recovery,const std::string& base_path) {
        const auto start=std::chrono::steady_clock::now();
        auto candidate=recovery?Project::recover(path,base_path):Project::open(path);
        if(candidate->active_image()) candidate->document(candidate->active_image())->query_region({0,0,0,0});
        return LoadedProject{std::move(candidate),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),recovery};
    };
    std::future<LoadedProject> project_load;
    auto startup_progress=std::make_shared<Project::LoadProgress>();
    bool startup_active=startup_requested,startup_pending=startup_requested,startup_succeeded=false,startup_cancelled=false;
    std::size_t startup_frames=0,startup_finish_frame=0;
    double startup_ms=0;
    bool project_load_cancelled=false;
    bool loading_recovery=false;
    std::uint64_t project_replacements=0;
    double project_load_ms=0;
    double recovery_load_ms=0;
    char project_open_path[32769]={};
    copy_recovery_path(project_open_path,options.project_open_path);
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
    bool remove_popup_requested=false,remove_dialog_active=false;
    std::uint64_t pending_remove=0,images_removed=0;
    ImVec2 confirm_remove_offset{},confirm_remove_size{},cancel_remove_offset{},cancel_remove_size{};
    int close_action=0; // 1 saved, 2 keep recovery, 3 discard.
    ImVec2 discard_recovery_offset{},discard_recovery_size{},keep_close_offset{},keep_close_size{},cancel_close_offset{},cancel_close_size{},discard_close_offset{},discard_close_size{};
    ImVec2 save_close_offset{},save_close_size{},save_recovery_offset{},save_recovery_size{},cancel_recovery_offset{},cancel_recovery_size{};
    char project_image_path[32769]={},project_image_name[4097]={},project_filter[257]={};
    if(options.project_image_path.size()>32768 || options.project_image_path.find('\0')!=std::string::npos)
        throw std::invalid_argument("invalid project image path");
    std::memcpy(project_image_path,options.project_image_path.c_str(),options.project_image_path.size()+1);
    std::vector<ProjectImage> project_images;
    std::vector<std::size_t> project_rows;
    bool project_catalog_dirty=true,project_filter_dirty=true;
    std::uint64_t pending_image=0;
    bool refresh_active_image=false;
    if(options.project) {
        const auto& path=options.project->path();
        if(path.size()>32768) throw std::invalid_argument("project path too long");
        std::memcpy(project_path,path.c_str(),path.size()+1);
    }
    char interchange_path[32769]={};
    if(options.interchange_path.size()>32768 || options.interchange_path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid interchange path");
    std::memcpy(interchange_path,options.interchange_path.c_str(),options.interchange_path.size()+1);
    bool interchange_pixels=options.interchange_assume_pixels,interchange_losses=options.interchange_allow_losses,interchange_selected=false,csv_intensity=false;
    bool qupath_ellipse_precision=options.qupath_ellipse_precision;
    std::string interchange_status;
    std::uint64_t editor_id=0,editor_revision=0;
    char draft_name[4097]={},draft_class[4097]={};
    char class_name[4097]={};
    std::string class_key;
    float class_color[3]={65/255.f,225/255.f,190/255.f};
    Rect draft_roi;
    Roi draft_geometry;
    std::uint64_t draft_parent=0;
    std::uint64_t calibration_revision=std::numeric_limits<std::uint64_t>::max();
    double draft_calibration[2]={0,0};
    std::vector<std::uint64_t> list_ids;
    std::uint64_t list_revision=std::numeric_limits<std::uint64_t>::max();
    int frames = 0;
    std::uint64_t settle_frames=0;
    std::array<int,static_cast<std::size_t>(GuiInputTarget::PublishPixelContours)+1> target_seen;
    target_seen.fill(-2);
    auto mark_target=[&](GuiInputTarget target) {
        const auto* modal=ImGui::GetTopMostPopupModal();
        target_seen[static_cast<std::size_t>(target)]=!ImGui::GetCurrentWindow()->SkipItems && (!modal || modal==ImGui::GetCurrentWindow()) && ImGui::IsItemVisible() &&
            !ImGui::IsWindowAppearing() && !(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)?frames:-2;
    };
    std::size_t visible_count=0;
    std::vector<double> frame_times;
    TilePlan plan;
    GuiThumbnail overview;
    bool canvas_visible=true;
    ImVec2 layout_reset_offset{},layout_reset_size{},save_project_offset{},save_project_size{};
    Viewport overview_view;
    DisplaySettings display=options.display;
    std::optional<ProjectView> restore_view;
    if(options.project && active_image && !options.start_view) {
        restore_view=options.project->view(active_image);
        if(restore_view) { display=restore_view->display; fit=false; }
    }
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
            maintenance_error=std::move(result.error);
            if(result.reviewed && !finishing) {
                reviewed_checkpoint=std::move(result.reviewed); copy_recovery_path(recovery_path,reviewed_checkpoint->path);
                maintenance_popup_requested=maintenance_dialog_active=true;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
            }
            if(!result.deleted.empty()) {
                ++recovery_catalog_revision;
                recovery_candidates.erase(std::remove_if(recovery_candidates.begin(),recovery_candidates.end(),[&](const auto& item){return item.path==result.deleted;}),recovery_candidates.end());
                if(std::string(recovery_path)==result.deleted) recovery_path[0]=recovery_base_path[0]=0;
                next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
            }
        } catch(const std::exception& e) {if(maintenance_epoch==workspace_epoch) maintenance_error=e.what();}
    };
    auto reconcile_save=[&] {
        const auto stats=save_job.poll();
        if(save_result_pending && !save_job.busy()) {
            save_result_pending=false;
            if(stats.state=="saved" && stats.applied) {
                project_error.clear();
                if(!options.project->dirty()) {
                    // Clear only this controller's checkpoint and only when
                    // the live canonical state actually reached the primary.
                    try {
                        if(restore_after_save && staged_project && same_recovery_file(autosave.stats().path,staged_project->recovery_source())) autosave.cancel();
                        else autosave.clear();
                    } catch(const std::exception& e) {autosave_ui_error=e.what();}
                }
            } else {
                project_error=stats.error; restore_after_save=false;
                if(close_action==1) {close_action=0; close_wait_only=false;}
            }
        }
    };
    auto* live=options.viewer_owner;
    auto live_context=[&] {
        if(startup_requested && !startup_active && !startup_succeeded && !startup_cancelled && !restore_pending && !staged_project && !project_load.valid() && !project_error.empty()) throw std::runtime_error(project_error);
        if(startup_requested && !startup_active && !startup_succeeded && startup_cancelled) exit_requested=true;
        detail::ViewerContext c;c.opening=startup_requested && !startup_succeeded;c.project=options.project;c.catalog=&project_images;c.epoch=startup_requested && workspace_epoch?workspace_epoch-1:workspace_epoch;c.frame=frames;c.active_image=active_image;
        c.blocked=c.opening || startup_active || close_dialog_active || recovery_dialog_active || restore_pending || remove_dialog_active || maintenance_dialog_active;
        c.closing=exit_requested || glfwWindowShouldClose(session.window);
        c.activate=[&](std::uint64_t id){pending_image=id;};
        c.select=[&](const std::vector<std::uint64_t>& ids){selection->replace(ids);selected=selection->primary();};
        c.save=[&](const std::string& path){if(save_job.busy()) return false;const auto started=save_job.request(options.project,path);if(started) {save_result_pending=true;std::memcpy(project_path,path.c_str(),path.size()+1);}return started;};
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
                    if(save_job.request(options.project,project_path)) {save_result_pending=true; project_error.clear();}
                }
                reconcile_save();
                if(restore_after_save && !save_job.busy()) {
                    if(options.project->dirty()) {
                        save_result_pending=save_job.request(options.project,project_path);
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
                    else save_result_pending=save_job.request(options.project,project_path);
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
                        const bool started=autosave.request(*options.project,autosave_path); close_keep_started=true;
                        if(started || !options.project->dirty() || autosave.stats().state=="ready") autosave_ui_error.clear();
                        if(!started && (!options.project->dirty() || autosave.stats().state=="ready")) exit_requested=true;
                    }
                } else if(!editing_blocked && !maintenance_job.valid() && (autosave_requested || (autosave_interval>0 && std::chrono::steady_clock::now()>=next_autosave))) {
                    recovery_operation=true;
                    autosave.request(*options.project,autosave_path); autosave_ui_error.clear();
                    next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
                }
            } catch(const std::exception& e) {
                if(recovery_operation) autosave_ui_error=e.what(); else project_error=e.what();
                if(save_project_requested) { restore_after_save=false; close_action=0; }
                if(close_action==2 || close_action==3) close_action=0;
                next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(std::max(1.,autosave_interval));
            }
        }
    };
    while (!exit_requested) {
        const bool settling=!within_frames();
        if(settling) {
            // An accepted save-and-replace/discard transition must reconcile
            // its new authority before a finite embedding returns.
            if(restore_after_save || restore_pending) settlement.check(std::chrono::steady_clock::now(),"workspace transition");
            else {if(!options.settle_tiles) break;settlement.check(std::chrono::steady_clock::now(),"tile uploads");}
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
        if(project_load.valid() && !close_dialog_active && !remove_dialog_active && project_load.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {
                auto loaded=project_load.get();
                if(startup_active) startup_ms=loaded.ms;
                if(loaded.recovery) recovery_load_ms=loaded.ms; else project_load_ms=loaded.ms;
                if(!project_load_cancelled) {
                    staged_project=std::move(loaded.project); staged_is_recovery=loaded.recovery;
                    drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
                    if(options.project->dirty() && !startup_active) recovery_popup_requested=recovery_dialog_active=true;
                    else { autosave.cancel(); restore_pending=true; }
                    project_error.clear();
                }
            } catch(const std::exception& e) { if(!project_load_cancelled) project_error=e.what(); }
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
                        copy_recovery_path(recovery_path,recovery_candidates.front().path);
                        if(!*recovery_base_path) copy_recovery_path(recovery_base_path,recovery_candidates.front().associated_base_path);
                    }
                }
            } catch(const std::exception& e) { if(current) autosave_ui_error=e.what(); }
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
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); vertex_paths.clear(); vertex_id=hole_id=editor_id=selected=list_anchor=0;
                tool=previous_tool=0; class_key.clear(); batch_class.clear(); error.clear(); interchange_status.clear();
                list_ids.clear(); list_revision=calibration_revision=std::numeric_limits<std::uint64_t>::max();
                plan={}; overview.reset(); plan_inputs.reset(); published_view_inputs.reset(); overview_center.reset();
                project_catalog_dirty=true; copy_recovery_path(project_path,options.project->path()); project_error.clear();
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
                project_filter[0]=project_image_path[0]=interchange_path[0]=0;
                next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
            } catch(const std::exception& e) {
                project_error=e.what();
                if(startup_active) {startup_active=false; startup_finish_frame=replay.frame();}
            }
            restore_pending=false;
        }
        // Switch only at frame boundaries: no draw commands can retain old GPU
        // texture IDs, document references or partially edited shape state.
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
                    restore_view=next?options.project->view(next):std::nullopt;
                    if(restore_view) { display=restore_view->display; fit=false; }
                    published_view_inputs.reset();
                    auto info=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==active_image;});
                    if(info!=project_images.end()) std::memcpy(project_image_name,info->name.c_str(),info->name.size()+1);
                    drawing=false; marquee=false; vertex_dragging=false; active_vertex.reset();
                    draft_points.clear(); vertex_paths.clear(); vertex_id=hole_id=editor_id=0;
                    tool=previous_tool=0; class_key.clear(); batch_class.clear(); error.clear(); interchange_status.clear();
                    list_ids.clear(); list_revision=calibration_revision=std::numeric_limits<std::uint64_t>::max();
                    plan={}; overview.reset(); plan_inputs.reset(); overview_center.reset();
                    max_image_switch_ms=std::max(max_image_switch_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count());
                }
                refresh_active_image=false; project_error.clear();
            } catch(const std::exception& e) { project_error=e.what(); refresh_active_image=false; }
        }
        auto& document=*active_document;
        analysis.poll(document,selected);stain_estimate.poll(document,selected);
        classifier.update(document,*selection);pixel_classifier.update(document,*selection);
        cell_table_job.poll();measurement_job.poll();
        cell_job.poll(document,selected);if(!cell_job.result()) cell_preview.clear();
        if(verifying_cells && !cell_job.busy()) {
            try {if(cell_job.poll(document,selected).state=="ready") cell_job.verify_record(document,verifying_cells);} catch(const std::exception& e) {error=e.what();}
            verifying_cells=0;
        }
        tissue.poll(document,selected);
        if(verifying_tissue && !tissue.busy()) {
            try {if(tissue.poll(document,selected).state=="ready") tissue.verify_result(document,verifying_tissue);} catch(const std::exception& e) {error=e.what();}
            verifying_tissue=0;
        }
        if(!tissue.result()) tissue_preview.clear();
        const bool has_image=!options.project || active_image!=0;
        const auto& image=document.metadata();
        if(options.project && project_catalog_dirty) {
            project_images=options.project->images(); project_catalog_dirty=false; project_filter_dirty=true;
            auto current=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==active_image;});
            if(current!=project_images.end()) std::memcpy(project_image_name,current->name.c_str(),current->name.size()+1);
            else project_image_name[0]=0;
        }
        if(project_filter_dirty) {
            project_rows.clear();
            for(std::size_t i=0;i<project_images.size();++i) if(project_images[i].name.find(project_filter)!=std::string::npos) project_rows.push_back(i);
            project_filter_dirty=false;
        }
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
            if(event.wait_for==GuiInputWait::BatchFinished) return batch.finished();
            if(event.wait_for==GuiInputWait::ObjectClassifierTrained) return classifier.trained();
            if(event.wait_for==GuiInputWait::ObjectClassifierPredicted) return classifier.predicted();
            if(event.wait_for==GuiInputWait::ObjectClassifierApplied) return classifier.applied();
            if(event.wait_for==GuiInputWait::ClassifierModelSaved) return classifier.io_state()=="saved";
            if(event.wait_for==GuiInputWait::ClassifierModelLoaded) return classifier.io_state()=="loaded";
            if(event.wait_for==GuiInputWait::ClassifierTrainingRetained) return classifier.retained();
            if(event.wait_for==GuiInputWait::ClassifierRecordLoaded) return classifier.record_loaded();
            if(event.wait_for==GuiInputWait::PixelClassifierTrained) return pixel_classifier.trained();
            if(event.wait_for==GuiInputWait::PixelClassifierPredicted) return pixel_classifier.predicted();
            if(event.wait_for==GuiInputWait::PixelPreviewReady) return pixel_classifier.preview_ready();
            if(event.wait_for==GuiInputWait::PixelContoursReady) return pixel_classifier.contours_ready();
            if(event.wait_for==GuiInputWait::PixelContoursPublished) return pixel_classifier.contours_published();
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
            if(event.wait_for==GuiInputWait::StartupFinished) return !startup_active;
            if(event.wait_for==GuiInputWait::TissueArchived || event.wait_for==GuiInputWait::TissueVerified) {
                const auto record=document.latest_tissue(selected);
                return record && (event.wait_for==GuiInputWait::TissueArchived || (!tissue.busy() && !verifying_tissue && document.tissue_status(record->id)=="current"));
            }
            if(event.wait_for==GuiInputWait::TissueReady) return !tissue.busy() && bool(tissue.result());
            if(event.wait_for==GuiInputWait::TissueContoursReady) return !tissue.busy() && bool(tissue.contours());
            if(event.wait_for==GuiInputWait::TissueContoursPublished) return !tissue.busy() && tissue.contours_published();
            if(target_seen[static_cast<std::size_t>(event.target)]!=frames-1) return false;
            switch(event.target) {
                case GuiInputTarget::DisplayReset:
                    return has_image && !startup_active && !close_dialog_active && !recovery_dialog_active && !restore_pending && !remove_dialog_active && !maintenance_dialog_active;
                case GuiInputTarget::CancelProjectLoad:
                    return project_load.valid() && !project_load_cancelled;
                case GuiInputTarget::ConfirmRemoveImage: case GuiInputTarget::CancelRemoveImage:
                    return remove_dialog_active;
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
            if(event.target>=GuiInputTarget::TrainObjectClassifier && event.target<=GuiInputTarget::UseClassifierRecord) {
                const auto bounds=classifier.bounds(event.target);origin={static_cast<float>(bounds.x),static_cast<float>(bounds.y)};extent={static_cast<float>(bounds.width),static_cast<float>(bounds.height)};
            }
            if(event.target>=GuiInputTarget::TrainPixelClassifier && event.target<=GuiInputTarget::PublishPixelContours) {
                const auto bounds=pixel_classifier.bounds(event.target);origin={static_cast<float>(bounds.x),static_cast<float>(bounds.y)};extent={static_cast<float>(bounds.width),static_cast<float>(bounds.height)};
            }
            if(event.target==GuiInputTarget::ResetLayout) {origin=layout_reset_offset;extent=layout_reset_size;}
            if(event.target==GuiInputTarget::ProjectList) { origin=project_list_offset; extent=project_list_size; }
            if(event.target==GuiInputTarget::DisplayReset) { origin=display_reset_offset; extent=display_reset_size; }
            if(event.target==GuiInputTarget::DiscardRecoveryReplacement) { origin=discard_recovery_offset; extent=discard_recovery_size; }
            if(event.target==GuiInputTarget::KeepRecoveryClose) { origin=keep_close_offset; extent=keep_close_size; }
            if(event.target==GuiInputTarget::CancelClose) { origin=cancel_close_offset; extent=cancel_close_size; }
            if(event.target==GuiInputTarget::DiscardClose) { origin=discard_close_offset; extent=discard_close_size; }
            if(event.target==GuiInputTarget::SaveClose) { origin=save_close_offset; extent=save_close_size; }
            if(event.target==GuiInputTarget::SaveRecoveryReplacement) { origin=save_recovery_offset; extent=save_recovery_size; }
            if(event.target==GuiInputTarget::CancelRecoveryReplacement) { origin=cancel_recovery_offset; extent=cancel_recovery_size; }
            if(event.target==GuiInputTarget::CancelProjectLoad) { origin=cancel_load_offset; extent=cancel_load_size; }
            if(event.target==GuiInputTarget::ConfirmRemoveImage) { origin=confirm_remove_offset; extent=confirm_remove_size; }
            if(event.target==GuiInputTarget::CancelRemoveImage) { origin=cancel_remove_offset; extent=cancel_remove_size; }
            if(event.target==GuiInputTarget::ConfirmDiscardCheckpoint) { origin=confirm_checkpoint_offset; extent=confirm_checkpoint_size; }
            if(event.target==GuiInputTarget::CancelDiscardCheckpoint) { origin=cancel_checkpoint_offset; extent=cancel_checkpoint_size; }
            if(event.target==GuiInputTarget::Image) {
                auto p=viewport.image_to_screen({event.x,event.y});
                const auto limit=static_cast<double>(std::numeric_limits<float>::max())/4;
                if(!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x)>limit || std::abs(p.y)>limit)
                    throw std::invalid_argument("image replay position exceeds screen coordinate range");
                input.AddMousePosEvent(previous_canvas.x+static_cast<float>(p.x),previous_canvas.y+static_cast<float>(p.y));
            } else input.AddMousePosEvent(origin.x+static_cast<float>(event.x)*extent.x,origin.y+static_cast<float>(event.y)*extent.y);
            if(event.ctrl) input.AddKeyEvent(ImGuiMod_Ctrl,true);
            if(event.shift) input.AddKeyEvent(ImGuiMod_Shift,true);
            switch(event.kind) {
                case GuiInputKind::LeftDown: input.AddMouseButtonEvent(0,true); break;
                case GuiInputKind::LeftUp: input.AddMouseButtonEvent(0,false); break;
                case GuiInputKind::RightDown: input.AddMouseButtonEvent(1,true); break;
                case GuiInputKind::RightUp: input.AddMouseButtonEvent(1,false); break;
                case GuiInputKind::Wheel: input.AddMouseWheelEvent(0,static_cast<float>(event.wheel)); break;
                case GuiInputKind::RectangleKey: input.AddKeyEvent(ImGuiKey_R,true); break;
                case GuiInputKind::UndoKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_Z,true); break;
                case GuiInputKind::RedoKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_Y,true); break;
                case GuiInputKind::DeleteKey: input.AddKeyEvent(ImGuiKey_Delete,true); break;
                case GuiInputKind::EllipseKey: input.AddKeyEvent(ImGuiKey_E,true); break;
                case GuiInputKind::PolygonKey: input.AddKeyEvent(ImGuiKey_P,true); break;
                case GuiInputKind::PolylineKey: input.AddKeyEvent(ImGuiKey_L,true); break;
                case GuiInputKind::PointsKey: input.AddKeyEvent(ImGuiKey_T,true); break;
                case GuiInputKind::FinishKey: input.AddKeyEvent(ImGuiKey_Enter,true); break;
                case GuiInputKind::SelectAllKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_A,true); break;
                case GuiInputKind::EscapeKey: input.AddKeyEvent(ImGuiKey_Escape,true); break;
                case GuiInputKind::VertexKey: input.AddKeyEvent(ImGuiKey_V,true); break;
                case GuiInputKind::HoleKey: input.AddKeyEvent(ImGuiKey_H,true); break;
                case GuiInputKind::InsertKey: input.AddKeyEvent(ImGuiKey_I,true); break;
                case GuiInputKind::RunBatchKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,true);input.AddKeyEvent(ImGuiKey_B,true);break;
                case GuiInputKind::CancelBatchKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_B,true);break;
                case GuiInputKind::BooleanKey: input.AddKeyEvent(ImGuiKey_B,true); break;
                case GuiInputKind::SaveKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_S,true); break;
                case GuiInputKind::ExportKey: input.AddKeyEvent(ImGuiKey_F6,true); break;
                case GuiInputKind::ImportKey: input.AddKeyEvent(ImGuiKey_F7,true); break;
                case GuiInputKind::NextImageKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_PageDown,true); break;
                case GuiInputKind::PreviousImageKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_PageUp,true); break;
                case GuiInputKind::AddImageKey: input.AddKeyEvent(ImGuiKey_F8,true); break;
                case GuiInputKind::RelinkImageKey: input.AddKeyEvent(ImGuiKey_F9,true); break;
                case GuiInputKind::AutosaveKey: input.AddKeyEvent(ImGuiKey_F10,true); break;
                case GuiInputKind::RecoverKey: input.AddKeyEvent(ImGuiKey_F11,true); break;
                case GuiInputKind::CloseWindow: glfwSetWindowShouldClose(session.window,true); break;
                case GuiInputKind::OpenProjectKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_O,true); break;
                case GuiInputKind::NewProjectKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_N,true); break;
                case GuiInputKind::RemoveImageKey: input.AddKeyEvent(ImGuiKey_F12,true); break;
                case GuiInputKind::ReviewRecoveryKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiMod_Shift,true); input.AddKeyEvent(ImGuiKey_Backspace,true); break;
                case GuiInputKind::FindRecoveriesKey: input.AddKeyEvent(ImGuiKey_F5,true); break;
                case GuiInputKind::StopRecoveryMaintenanceKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_F10,true); break;
                case GuiInputKind::AnalyzeKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_M,true); break;
                case GuiInputKind::CancelAnalysisKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiMod_Shift,true); input.AddKeyEvent(ImGuiKey_M,true); break;
                case GuiInputKind::KeepAnalysisKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_K,true); break;
                case GuiInputKind::RecheckAnalysisKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiMod_Shift,true); input.AddKeyEvent(ImGuiKey_K,true); break;
                case GuiInputKind::DetectTissueKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_D,true); break;
                case GuiInputKind::CancelTissueKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiMod_Shift,true); input.AddKeyEvent(ImGuiKey_D,true); break;
                case GuiInputKind::DetectCellsKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiKey_G,true);break;
                case GuiInputKind::KeepCellsKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_G,true);break;
                case GuiInputKind::CancelCellsKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,true);input.AddKeyEvent(ImGuiKey_G,true);break;
                case GuiInputKind::KeepCellRunKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_C,true);break;
                case GuiInputKind::RecheckCellRunKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,true);input.AddKeyEvent(ImGuiKey_C,true);break;
                case GuiInputKind::FingerprintFileKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_F,true);break;
                case GuiInputKind::CancelFingerprintFileKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiMod_Alt,true);input.AddKeyEvent(ImGuiKey_F,true);break;
                case GuiInputKind::EstimateStainsKey: input.AddKeyEvent(ImGuiKey_F3,true);break;
                case GuiInputKind::ApplyStainEstimateKey: input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_F3,true);break;
                case GuiInputKind::CancelStainEstimateKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiKey_F3,true);break;
                case GuiInputKind::ObjectClassifierKey: input.AddKeyEvent(ImGuiKey_F4,true);break;
                case GuiInputKind::PixelClassifierKey: input.AddKeyEvent(ImGuiKey_F5,true);break;
                case GuiInputKind::ExportMeasurementTableKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,!event.shift);input.AddKeyEvent(ImGuiKey_Q,true);break;
                case GuiInputKind::CancelMeasurementTableKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_Q,true);break;
                case GuiInputKind::ExportCellTableKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,!event.shift);input.AddKeyEvent(ImGuiKey_X,true);break;
                case GuiInputKind::CancelCellTableKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Alt,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_X,true);break;
                case GuiInputKind::KeepTissueKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiKey_U,true);break;
                case GuiInputKind::RecheckTissueKey: input.AddKeyEvent(ImGuiMod_Ctrl,true);input.AddKeyEvent(ImGuiMod_Shift,true);input.AddKeyEvent(ImGuiKey_U,true);break;
                case GuiInputKind::TraceTissueKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiKey_J,true); break;
                case GuiInputKind::KeepTissueContoursKey: input.AddKeyEvent(ImGuiMod_Ctrl,true); input.AddKeyEvent(ImGuiMod_Shift,true); input.AddKeyEvent(ImGuiKey_J,true); break;
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
                drawing=marquee=vertex_dragging=false;active_vertex.reset();draft_points.clear();
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
        // Test/benchmark replay enters the same ImGui event queue as GLFW input.
        if (!options.inputs.empty()) {
            input.AddKeyEvent(ImGuiKey_R,false); input.AddKeyEvent(ImGuiKey_Z,false); input.AddKeyEvent(ImGuiKey_Y,false);
            input.AddKeyEvent(ImGuiKey_J,false);input.AddKeyEvent(ImGuiKey_U,false);input.AddKeyEvent(ImGuiKey_G,false);input.AddKeyEvent(ImGuiKey_C,false);input.AddKeyEvent(ImGuiKey_X,false);input.AddKeyEvent(ImGuiMod_Alt,false);
            input.AddKeyEvent(ImGuiMod_Ctrl,false); input.AddKeyEvent(ImGuiMod_Shift,false); input.AddKeyEvent(ImGuiKey_Delete,false);
            for(auto key:{ImGuiKey_E,ImGuiKey_P,ImGuiKey_L,ImGuiKey_T,ImGuiKey_Enter,ImGuiKey_A,ImGuiKey_Escape,ImGuiKey_V,ImGuiKey_H,ImGuiKey_I,ImGuiKey_B,ImGuiKey_S,ImGuiKey_O,ImGuiKey_N,ImGuiKey_Backspace,ImGuiKey_F5,ImGuiKey_F6,ImGuiKey_F7,ImGuiKey_PageDown,ImGuiKey_PageUp,ImGuiKey_F8,ImGuiKey_F9,ImGuiKey_F10,ImGuiKey_F11,ImGuiKey_F12,ImGuiKey_M,ImGuiKey_K,ImGuiKey_D,ImGuiKey_F,ImGuiKey_Q,ImGuiKey_F3}) input.AddKeyEvent(key,false);
        }
        replay_geometry_changed=false;
        replay.dispatch(replay_ready,replay_emit,std::chrono::steady_clock::now());
        if(suspend_window()) continue;
        if(replay_geometry_changed) ImGui_ImplGlfw_NewFrame();

        float sx=1,sy=1;glfwGetWindowContentScale(session.window,&sx,&sy);
        const auto next_scale=GuiScale::from(std::max(sx,sy),std::max(input.DisplayFramebufferScale.x,input.DisplayFramebufferScale.y),ui_multiplier);
        if(!fonts_initialized || !font_scale.same_fonts(next_scale)) {
            // Rebuild only between frames, on the GL owner thread. Never scale
            // the previous style, which would accumulate rounding and drift.
            overlay_cache.clear();
            ImGui_ImplOpenGL3_DestroyFontsTexture();input.Fonts->Clear();
            style=base_style;style.ScaleAllSizes(next_scale.logical);
            ImFontConfig font;font.SizePixels=13.f*next_scale.logical;font.RasterizerDensity=next_scale.framebuffer;
            input.Fonts->AddFontDefault(&font);
            // The backend creates the texture in NewFrame, after its device
            // objects exist. Creating it here would duplicate the first upload.
            font_scale=next_scale;fonts_initialized=true;++font_rebuilds;
            drawing=marquee=vertex_dragging=false;active_vertex.reset();draft_points.clear();
            input.AddMouseButtonEvent(0,false);input.AddMouseButtonEvent(1,false);ImGui::ClearActiveID();
        }
        ui_scale=next_scale;
        ImGui_ImplOpenGL3_NewFrame();
        if(!input.Fonts->TexID) throw std::runtime_error("UI font texture creation failed");
        ImGui::NewFrame();ui::begin_frame();
        bool save_project_requested=false;
        bool autosave_requested=false;
        const bool editing_blocked=editing_is_blocked();
        const auto& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);
        if(startup_active) {
            ++startup_frames;
            const auto phase=startup_progress->phase.load();
            ImGui::TextUnformatted(project_load_cancelled?"Finishing cancelled opening...":
                (options.startup_kind=="image"?"Opening image...":options.startup_kind=="recovery"?"Opening recovery...":"Opening project..."));
            ImGui::TextWrapped("%s",options.startup_path.c_str());
            const char* stage=phase==1?"Reading file metadata":phase==2?"Reading project data":phase==3?"Validating image objects":phase==4?"Checking project references":"Preparing workspace";
            ui::TextDisabled("%s",stage);
            if(options.startup_kind!="image" && phase>1) {
                ImGui::Text("Images: %zu / %zu   Objects read: %zu / %zu",startup_progress->images.load(),startup_progress->total_images.load(),
                    startup_progress->objects.load(),startup_progress->total_objects.load());
            }
            ImGui::BeginDisabled(project_load_cancelled);
            if(ui::Button("Cancel opening")) {project_load_cancelled=true; startup_progress->cancelled.store(true);}
            cancel_load_offset=ImGui::GetItemRectMin(); auto cl=ImGui::GetItemRectMax(); cancel_load_size={cl.x-cancel_load_offset.x,cl.y-cancel_load_offset.y}; mark_target(GuiInputTarget::CancelProjectLoad);
            ImGui::EndDisabled(); ImGui::Separator();
        } else if(startup_requested && !startup_succeeded && project_replacements==0 &&
                  options.project->revision_==0 && options.project->path().empty()) {
            ImGui::TextWrapped("%s",startup_cancelled?"Opening cancelled. The workspace is empty.":project_error.c_str());
        }
        ImGui::BeginDisabled(editing_blocked);
        ImGui::TextColored({.3f,.8f,.75f,1}, "qupathxx"); ImGui::SameLine(); ui::TextDisabled("/  Research workspace");
        if(!io.WantTextInput && !editing_blocked && io.KeyCtrl && io.KeyAlt && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_X,false)) cell_table_job.cancel();
        if(live) {
            const auto scripting=live->stats();ui::TextDisabled("Python session connected / %zu pending",scripting.queued);
            if(scripting.active_ticket) {ui::TextDisabled("Processing script job");ImGui::SameLine();if(ui::SmallButton("Cancel script job")) live->cancel_active();}
            else ui::TextDisabled("Ready");
        }
        ImGui::Separator();
        ImGui::BeginDisabled(!has_image);
        if (ImGui::IsKeyPressed(ImGuiKey_R) && !io.WantTextInput && !editing_blocked && has_image) tool=tool==1?0:1;
        if(!io.WantTextInput && !editing_blocked && has_image) {
            if(ImGui::IsKeyPressed(ImGuiKey_E)) tool=2;
            if(ImGui::IsKeyPressed(ImGuiKey_P)) tool=3;
            if(ImGui::IsKeyPressed(ImGuiKey_L)) tool=4;
            if(ImGui::IsKeyPressed(ImGuiKey_T)) tool=5;
            if(ImGui::IsKeyPressed(ImGuiKey_V)) tool=6;
            if(ImGui::IsKeyPressed(ImGuiKey_H)) tool=7;
        }
        ui::SetNextItemWidth(125);
        ui::Combo("Tool",&tool,"Select\0Rectangle\0Ellipse\0Polygon\0Polyline\0Points\0Vertices\0Hole\0"); ImGui::SameLine();
        if(tool!=previous_tool) { vertex_dragging=false; active_vertex.reset(); marquee=false; drawing=false; draft_points.clear(); previous_tool=tool; }
        if (ui::Button("Fit image")) fit = true;
        auto history_action=[&](bool redo) { try { if(redo) document.redo(); else document.undo(); error.clear(); } catch(const std::exception& e) { error=e.what(); } };
        if (!io.WantTextInput && !editing_blocked && has_image && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) history_action(false);
        if (!io.WantTextInput && !editing_blocked && has_image && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) history_action(true);
        ImGui::SameLine(); ImGui::BeginDisabled(!document.can_undo()); if (ui::Button("Undo")) history_action(false); ImGui::EndDisabled();
        ImGui::SameLine(); ImGui::BeginDisabled(!document.can_redo()); if (ui::Button("Redo")) history_action(true); ImGui::EndDisabled();
        ImGui::SameLine();if(ui::Button(batch.busy()?"Batch running...":"Batch...")) batch.open();
        selected=selection->primary();
        if(!io.WantTextInput && !editing_blocked && has_image && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) { selection->select_all(); selected=selection->primary(); }
        auto delete_selection=[&]() {
            try {
                auto ids=selection->ids();
                if(ids.size()==1) document.remove_annotation(ids.front()); else document.remove_objects(ids);
                selection->clear(); selected=0; error.clear();
            } catch(const std::exception& e) { error=e.what(); }
        };
        if (selected && tool!=6 && !io.WantTextInput && !editing_blocked && has_image && ImGui::IsKeyPressed(ImGuiKey_Delete)) delete_selection();
        ImGui::SameLine();
        constexpr auto navigation_help="Ctrl+click: toggle | Shift+drag: select box | Ctrl+A: all | Right drag: pan";
        if(ImGui::CalcTextSize(navigation_help).x<=ui::available()) ui::TextDisabled("%s",navigation_help);
        else {
            if(ui::SmallButton("Help")) ImGui::OpenPopup("Navigation shortcuts");
            if(ImGui::BeginPopup("Navigation shortcuts")) {
                ImGui::TextUnformatted("Ctrl+click: toggle selection\nShift+drag: select box\nCtrl+A: select all\nRight drag: pan\nF4: object classifiers\nF5: pixel classifiers");ImGui::EndPopup();
            }
        }
        ImGui::EndDisabled();
        if(ui::SmallButton("UI -")) ui_multiplier=std::max(.5,ui_multiplier-.25);
        ImGui::SameLine();ImGui::Text("UI %.0f%%",ui_multiplier*100);
        ImGui::SameLine();if(ui::SmallButton("UI +")) ui_multiplier=std::min(3.,ui_multiplier+.25);
        ImGui::SameLine();if(ui::SmallButton("Auto UI")) ui_multiplier=1;
        if(workspace.enabled()) {
            ImGui::SameLine();
            if(ui::Button("Reset layout")) workspace.reset();
            layout_reset_offset=ImGui::GetItemRectMin();auto lr=ImGui::GetItemRectMax();layout_reset_size={lr.x-layout_reset_offset.x,lr.y-layout_reset_offset.y};mark_target(GuiInputTarget::ResetLayout);
            if(!workspace.error().empty()) ImGui::TextWrapped("Layout preferences: %s",workspace.error().c_str());
            workspace.dockspace();
            workspace.begin("Project and objects",workspace.browser);
        } else ImGui::BeginChild("Inspector", {std::min(285*ui_scale.logical,io.DisplaySize.x*.45f),0}, ImGuiChildFlags_Borders);
        if(options.project) {
            // Route these chords so ImGui's own PageUp/PageDown navigation
            // cannot also focus/activate an inspector text field.
            const bool next_image=ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_PageDown,ImGuiInputFlags_RouteGlobal);
            const bool previous_image=ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_PageUp,ImGuiInputFlags_RouteGlobal);
            ui::SeparatorText(options.project->dirty()?"Project - unsaved":"Project");
            ImGui::BeginDisabled(save_job.busy());
            ui::SetNextItemWidth(240); ui::InputTextWithHint("##Project path","Destination .qpxproj file",project_path,sizeof(project_path));
            const bool save_pressed=ui::Button("Save project (Ctrl+S)");
            save_project_offset=ImGui::GetItemRectMin();const auto save_max=ImGui::GetItemRectMax();
            save_project_size={save_max.x-save_project_offset.x,save_max.y-save_project_offset.y};mark_target(GuiInputTarget::SaveProject);
            if(save_pressed || (!editing_blocked && !save_job.busy() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))) {
                save_project_requested=true;
            }
            ImGui::EndDisabled();
            if(save_job.busy()) ui::TextDisabled("Saving project...");
            ui::SetNextItemWidth(240); ui::InputTextWithHint("##Open project path","Project file to open",project_open_path,sizeof(project_open_path));
            ImGui::BeginDisabled(project_load.valid() || save_job.busy() || maintenance_job.valid());
            if(ui::Button("Open project (Ctrl+O)") || (!editing_blocked && !project_load.valid() && !save_job.busy() && !maintenance_job.valid() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))) {
                const std::string path=project_open_path;
                project_load_cancelled=false; loading_recovery=false; project_error.clear();
                project_load=std::async(std::launch::async,load_project,path,false,std::string{});
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(project_load.valid() || save_job.busy() || maintenance_job.valid());
            if(ui::Button("New project (Ctrl+N)") || (!editing_blocked && !project_load.valid() && !save_job.busy() && !maintenance_job.valid() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))) {
                staged_project=std::make_shared<Project>(); staged_is_recovery=false;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
                if(options.project->dirty()) recovery_popup_requested=recovery_dialog_active=true;
                else { autosave.cancel(); restore_pending=true; }
                project_error.clear();
            }
            ImGui::EndDisabled();
            if(project_load.valid() && !startup_active) {
                ui::TextDisabled(project_load_cancelled?"Finishing cancelled load...":(loading_recovery?"Opening recovery...":"Opening project..."));
                ImGui::BeginDisabled(project_load_cancelled);
                if(ui::Button("Cancel opening")) project_load_cancelled=true;
                cancel_load_offset=ImGui::GetItemRectMin(); auto cl=ImGui::GetItemRectMax(); cancel_load_size={cl.x-cancel_load_offset.x,cl.y-cancel_load_offset.y}; mark_target(GuiInputTarget::CancelProjectLoad);
                ImGui::EndDisabled();
            }
            bool add_image=!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F8);
            bool relink_image=has_image && !io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F9);
            bool remove_image=has_image && !project_load.valid() && !io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F12);
            if(!io.WantTextInput && !editing_blocked && (next_image || previous_image) && project_images.size()>1) {
                // Keep ownership through key-up even if Ctrl is released first;
                // otherwise the still-held Page key resumes ImGui navigation.
                ImGui::SetKeyOwner(next_image?ImGuiKey_PageDown:ImGuiKey_PageUp,ImGui::GetID("Project image switch"),ImGuiInputFlags_LockUntilRelease);
                auto it=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==active_image;});
                auto index=static_cast<std::size_t>(it-project_images.begin());
                pending_image=it==project_images.end()?project_images[next_image?0:project_images.size()-1].id:project_images[(index+project_images.size()+(next_image?1:-1))%project_images.size()].id;
            }
            if(ImGui::CollapsingHeader("Project images",ImGuiTreeNodeFlags_DefaultOpen)) {
                ui::SetNextItemWidth(170);
                if(ui::InputTextWithHint("##Image filter","Filter image names",project_filter,sizeof(project_filter))) project_filter_dirty=true;
                ImGui::SameLine(); if(ui::SmallButton("Refresh")) project_catalog_dirty=true;
                ImGui::BeginChild("Project image list",{0,100},ImGuiChildFlags_Borders);
                project_list_offset=ImGui::GetCursorScreenPos(); project_list_size=ImGui::GetContentRegionAvail();
                ImGuiListClipper image_clipper; image_clipper.Begin(static_cast<int>(project_rows.size()));
                while(image_clipper.Step()) for(int row=image_clipper.DisplayStart;row<image_clipper.DisplayEnd;++row) {
                    const auto& item=project_images[project_rows[row]];
                    ImGui::PushID(static_cast<int>(item.id>>32)); ImGui::PushID(static_cast<int>(item.id));
                    auto label=item.name+(item.available?"":" [missing]");
                    if(ImGui::Selectable(label.c_str(),item.id==active_image)) pending_image=item.id;
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%lld x %lld\n%s\nCtrl+PageUp / PageDown: switch images",static_cast<long long>(item.width),static_cast<long long>(item.height),item.embedded?"Embedded RGB":item.source_path.c_str());
                    ImGui::PopID(); ImGui::PopID();
                }
                ImGui::EndChild();
                ui::TextDisabled("%zu of %zu images",project_rows.size(),project_images.size());
                ImGui::BeginDisabled(!has_image || project_load.valid());
                if(ui::Button("Remove image (F12)")) remove_image=true;
                ImGui::EndDisabled();
                if(ImGui::TreeNodeEx("Image source / name",options.project_image_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)) {
                    ui::SetNextItemWidth(240); ui::InputText("Image name",project_image_name,sizeof(project_image_name));
                    ImGui::BeginDisabled(!has_image);
                    if(ui::Button("Rename image")) {
                        try { options.project->rename_image(active_image,project_image_name); project_catalog_dirty=true; project_error.clear(); }
                        catch(const std::exception& e) { project_error=e.what(); }
                    }
                    ImGui::EndDisabled();
                    ui::SetNextItemWidth(240); ui::InputTextWithHint("##Image source","Image file to add or relink",project_image_path,sizeof(project_image_path));
                    if(ui::Button("Add image (F8)")) add_image=true;
                    auto current=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==active_image;});
                    ImGui::BeginDisabled(current==project_images.end() || current->embedded);
                    if(ui::Button("Relink image (F9)")) relink_image=true;
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }
            }
            if(remove_image) {
                remove_popup_requested=remove_dialog_active=true; pending_image=0;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
            }
            if(add_image) {
                try { pending_image=options.project->add_image(project_image_path); project_catalog_dirty=true; project_error.clear(); }
                catch(const std::exception& e) { project_error=e.what(); }
            }
            if(relink_image) {
                try { options.project->relink(active_image,project_image_path); mean.invalidate(); pending_image=active_image; refresh_active_image=true; project_catalog_dirty=true; project_error.clear(); }
                catch(const std::exception& e) { project_error=e.what(); }
            }
            bool recover_requested=!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F11);
            bool review_checkpoint=!io.WantTextInput && !editing_blocked && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Backspace);
            bool find_recoveries=!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F5);
            autosave_requested=!io.WantTextInput && !editing_blocked && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F10);
            if(!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F10) && autosave.busy() && autosave_stats.checkpoint_published) autosave.cancel();
            if(reveal_recoveries) {ImGui::SetNextItemOpen(true); reveal_recoveries=false;}
            if(ImGui::CollapsingHeader("Recovery",(!options.recovery_path.empty() || !recovery_candidates.empty() || !options.project->recovery_source().empty())?ImGuiTreeNodeFlags_DefaultOpen:0)) {
                if(!options.project->recovery_source().empty() && options.project->dirty()) ImGui::TextWrapped("Recovered workspace. Save this project to retain its changes.");
                ImGui::TextWrapped("Recovery preserves unsaved work separately from Save project.");
                ui::SetNextItemWidth(110);
                if(ui::InputDouble("Interval (seconds)",&autosave_interval,10,60,"%.1f")) {
                    if(!std::isfinite(autosave_interval) || autosave_interval<0 || autosave_interval>86400) autosave_interval=60;
                    next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
                }
                ui::TextDisabled("0 disables periodic recovery.");
                ui::SetNextItemWidth(240); ui::InputTextWithHint("##Autosave path","Automatic destination",autosave_path,sizeof(autosave_path));
                ImGui::BeginDisabled(autosave.busy());
                if(ui::Button("Save recovery now (F10)")) autosave_requested=true;
                ImGui::EndDisabled();
                if(autosave.busy() && autosave_stats.checkpoint_published) {
                    if(autosave_stats.cancel_requested) ui::TextDisabled("Stopping recovery maintenance...");
                    else if(autosave_stats.maintenance_phase=="scanning") {ui::TextDisabled("Recovery published."); ui::TextDisabled("Checked %zu checkpoint headers...",autosave_stats.retention_inspected);}
                    else if(autosave_stats.maintenance_phase=="collecting") {ui::TextDisabled("Recovery published."); ui::TextDisabled("Removed %zu old checkpoints...",autosave_stats.retention_removed);}
                    else ui::TextDisabled("Recovery published. Finishing maintenance...");
                    ImGui::BeginDisabled(autosave_stats.cancel_requested);
                    if(ui::Button("Stop maintenance (Ctrl+F10)")) autosave.cancel();
                    ImGui::EndDisabled();
                }
                else if(autosave.busy()) ui::TextDisabled("Writing recovery...");
                else if(autosave_stats.state=="ready") ui::TextDisabled("Recovery saved; project still unsaved.");
                if(!autosave.busy() && autosave_stats.retention_cancelled) ImGui::TextWrapped("Maintenance stopped. Published recovery retained.");
                if(!autosave_stats.error.empty()) ImGui::TextWrapped("%s",autosave_stats.error.c_str());
                if(!autosave_stats.work_cleanup_error.empty()) ImGui::TextWrapped("Recovery work cleanup: %s",autosave_stats.work_cleanup_error.c_str());
                bool retention_changed=ui::Checkbox("Remove old inactive checkpoints",&retention.enabled);
                if(retention.enabled) {
                    int keep=static_cast<int>(retention.keep_latest);
                    ui::SetNextItemWidth(110);
                    if(ui::InputInt("Keep newest",&keep)) {retention.keep_latest=static_cast<std::size_t>(std::clamp(keep,1,1024)); retention_changed=true;}
                    ui::SetNextItemWidth(110);
                    if(ui::InputDouble("Minimum age (days)",&retention.max_age_days,1,7,"%.1f")) {
                        if(!std::isfinite(retention.max_age_days)) retention.max_age_days=30;
                        retention.max_age_days=std::clamp(retention.max_age_days,1.,36500.); retention_changed=true;
                    }
                    ImGui::TextWrapped("After recovery saves, keep recent checkpoints and active sessions; collect at most 32 old inactive files.");
                }
                if(retention_changed) autosave.set_retention(retention);
                if(!autosave_stats.retention_error.empty()) ImGui::TextWrapped("Recovery retention: %s",autosave_stats.retention_error.c_str());
                if(!autosave_ui_error.empty()) ImGui::TextWrapped("%s",autosave_ui_error.c_str());
                if(ui::Button("Find recovery files (F5)")) find_recoveries=true;
                for(const auto& item:recovery_candidates) {
                    ImGui::PushID(item.path.c_str());
                    auto label=recovery_label(item);
                    if(ImGui::Selectable(label.c_str(),item.path==recovery_path)) {
                        copy_recovery_path(recovery_path,item.path); copy_recovery_path(recovery_base_path,item.associated_base_path);
                    }
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",item.path.c_str());
                    if(item.base_status=="changed") ImGui::TextWrapped("Saved project changed; recovered work needs a new destination.");
                    if(item.base_status=="relocated") ImGui::TextWrapped("Checkpoint found by project identity at this location.");
                    if(!item.error.empty()) ImGui::TextWrapped("%s",item.error.c_str());
                    ImGui::PopID();
                }
                ui::SetNextItemWidth(240); ui::InputTextWithHint("##Restore recovery","Recovery file to open",recovery_path,sizeof(recovery_path));
                ImGui::TextUnformatted("Primary location (optional)");
                ui::SetNextItemWidth(240); ui::InputTextWithHint("##Recovery primary","Use recorded location when empty",recovery_base_path,sizeof(recovery_base_path));
                ImGui::BeginDisabled(autosave.busy() || project_load.valid() || save_job.busy() || maintenance_job.valid());
                if(ui::Button("Open recovery (F11)")) recover_requested=true;
                if(ui::Button("Delete selected checkpoint...")) review_checkpoint=true;
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+Shift+Backspace: review checkpoint deletion");
                ImGui::EndDisabled();
                if(maintenance_job.valid()) ui::TextDisabled("Working on recovery file...");
                if(!maintenance_error.empty()) ImGui::TextWrapped("%s",maintenance_error.c_str());
            }
            if(find_recoveries && !recovery_scan.valid()) {
                auto base=options.project->path(); recovery_scan_base=base; recovery_scan_generation=options.project->generation(); recovery_scan_epoch=workspace_epoch; recovery_scan_revision=recovery_catalog_revision;
                recovery_scan=std::async(std::launch::async,[base]{return Project::find_recoveries(base);});
            }
            if(recover_requested && !autosave.busy() && !project_load.valid() && !save_job.busy() && !maintenance_job.valid()) {
                try {
                    project_load_cancelled=false; loading_recovery=true;
                    project_load=std::async(std::launch::async,load_project,std::string(recovery_path),true,std::string(recovery_base_path));
                    project_error.clear();
                } catch(const std::exception& e) { project_error=e.what(); }
            }
            if(review_checkpoint && !autosave.busy() && !project_load.valid() && !save_job.busy() && !maintenance_job.valid()) {
                const std::string path=recovery_path; maintenance_epoch=workspace_epoch; maintenance_error.clear();
                maintenance_job=std::async(std::launch::async,[path] {
                    MaintenanceResult result;
                    try {result.reviewed=Project::review_recovery(path);} catch(const std::exception& e) {result.error=e.what();}
                    return result;
                });
            }
            if(!project_error.empty()) ImGui::TextWrapped("%s",project_error.c_str());
        }
        if(workspace.enabled()) {ImGui::End();workspace.begin("Image and display",workspace.display);}
        ImDrawList* overview_draw=nullptr;
        ImVec2 overview_clip_min{},overview_clip_max{};
        if(has_image) {
        if(has_interchange() && ImGui::CollapsingHeader("Import / export",options.interchange_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)) {
            ui::SetNextItemWidth(240); ui::InputTextWithHint("##Interchange path",".geojson or .csv destination",interchange_path,sizeof(interchange_path));
            ImGui::TextWrapped("Full-resolution image pixels, top-left origin.");
            ui::Checkbox("Assume image pixels",&interchange_pixels);
            ui::Checkbox("Allow lossy conversion",&interchange_losses);
            ImGui::BeginDisabled(!has_geos());
            ui::Checkbox("Match QuPath ellipse bounds",&qupath_ellipse_precision);
            ImGui::EndDisabled();
            if(qupath_ellipse_precision) ImGui::TextWrapped("Coordinate changes require Allow lossy conversion. Native analytic hints remain exact.");
            ui::Checkbox("Selected objects only",&interchange_selected);
            auto export_ids=[&]() {
                auto ids=interchange_selected?selection->ids():std::vector<std::uint64_t>{};
                if(interchange_selected && ids.empty()) throw std::invalid_argument("Select objects before exporting a subset");
                return ids;
            };
            auto status=[&](const InterchangeReport& report,const char* action) {
                interchange_status=std::string(action)+" "+std::to_string(report.features)+" objects";
                for(const auto& issue:report.issues) interchange_status+="\n"+issue.message+" ("+std::to_string(issue.count)+")";
            };
            if(ui::Button("Export GeoJSON (F6)") || (!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F6))) {
                try { status(save_geojson(document,interchange_path,export_ids(),interchange_losses),"Exported"); }
                catch(const std::exception& e) { interchange_status=e.what(); }
            }
            if(ui::Button("Import GeoJSON (F7)") || (!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F7))) {
                try { auto report=load_geojson(document,interchange_path,interchange_pixels,interchange_losses,qupath_ellipse_precision); selection->replace(report.imported_ids); selected=selection->primary(); status(report,"Imported"); }
                catch(const std::exception& e) { interchange_status=e.what(); }
            }
            ui::Checkbox("Include mean RGB",&csv_intensity);
            if(ui::Button("Export measurements CSV")) {
                try { save_measurements_csv(document,interchange_path,export_ids(),csv_intensity); interchange_status="Exported measurements"; }
                catch(const std::exception& e) { interchange_status=e.what(); }
            }
            if(!interchange_status.empty()) ImGui::TextWrapped("%s",interchange_status.c_str());
        }
        ImGui::Text("IMAGE"); ImGui::Text("%lld x %lld pixels",static_cast<long long>(image.width),static_cast<long long>(image.height));
        ImGui::Text("%zu pyramid levels",image.levels.size());
        ui::SeparatorText("Overview");
        overview_size.x=std::min(250.f*ui_scale.logical,ui::available());overview_size.y=overview_size.x*.6f;
        overview_offset=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("overview navigation",overview_size);
        overview_draw=ImGui::GetWindowDrawList();
        overview_clip_min=overview_draw->GetClipRectMin(); overview_clip_max=overview_draw->GetClipRectMax();
        overview_view.fit(image.width,image.height,overview_size.x,overview_size.y);
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
            auto center=overview_view.screen_to_image({io.MousePos.x-overview_offset.x,io.MousePos.y-overview_offset.y});
            overview_center=Point{std::clamp(center.x,0.,static_cast<double>(image.width)),std::clamp(center.y,0.,static_cast<double>(image.height))};
        }
        ui::SeparatorText("Display");
        int mode=static_cast<int>(display.mode);
        if (ui::Combo("Channels",&mode,"RGB\0Grayscale\0Red\0Green\0Blue\0")) display.mode=static_cast<DisplayMode>(mode);
        float black=static_cast<float>(display.black),white=static_cast<float>(display.white),gamma=static_cast<float>(display.gamma);
        if (ui::SliderFloat("Black",&black,0,254,"%.0f")) display.black=std::min<double>(black,std::max(0.,display.white-.001));
        if (ui::SliderFloat("White",&white,1,255,"%.0f")) display.white=std::max<double>(white,std::min(255.,display.black+.001));
        if (ui::SliderFloat("Gamma",&gamma,.1f,5,"%.2f")) display.gamma=gamma;
        if (ui::Button("Reset display")) display=DisplaySettings{};
        display_reset_offset=ImGui::GetItemRectMin();
        auto display_reset_max=ImGui::GetItemRectMax(); display_reset_size={display_reset_max.x-display_reset_offset.x,display_reset_max.y-display_reset_offset.y}; mark_target(GuiInputTarget::DisplayReset);
        tiles->set_display(display);
        overview.update(active_document->server(),display);
        if (!document.pixel_width_um() || !document.pixel_height_um()) ui::TextDisabled("Physical calibration unknown");
        if (calibration_revision!=document.calibration_revision()) {
            draft_calibration[0]=document.pixel_width_um().value_or(0);
            draft_calibration[1]=document.pixel_height_um().value_or(0);
            calibration_revision=document.calibration_revision();
        }
        ui::SetNextItemWidth(120);
        ui::InputDouble("Pixel width (um)",&draft_calibration[0],0,0,"%.6g");
        ui::SetNextItemWidth(120);
        ui::InputDouble("Pixel height (um)",&draft_calibration[1],0,0,"%.6g");
        if (ui::Button("Apply calibration")) {
            try { document.set_calibration(draft_calibration[0], draft_calibration[1]); error.clear(); }
            catch (const std::exception& e) { error = e.what(); }
        }
        ImGui::SameLine();
        if (ui::Button("Clear")) {
            try { document.clear_calibration(); error.clear(); } catch(const std::exception& e) { error=e.what(); }
        }
        if(workspace.enabled()) {ImGui::End();workspace.begin("Project and objects",workspace.browser);}
        ImGui::Spacing(); ui::SeparatorText("Objects");
        if (list_revision!=document.structure_revision()) {
            list_ids.clear(); list_ids.reserve(document.object_count());
            for (const auto& [id,a]:document.objects()) list_ids.push_back(id);
            list_revision=document.structure_revision();
        }
        ImGui::BeginChild("Object list",{0,90},ImGuiChildFlags_Borders);
        list_offset=ImGui::GetCursorScreenPos(); list_size=ImGui::GetContentRegionAvail();
        ImGuiListClipper clipper; clipper.Begin(static_cast<int>(list_ids.size()));
        while(clipper.Step()) for (int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
            const auto& a=document.objects().at(list_ids[row]);
            auto label = a.name + "##" + std::to_string(a.id);
            if (ImGui::Selectable(label.c_str(), selection->contains(a.id))) {
                if(io.KeyShift && list_anchor && document.objects().count(list_anchor)) selection->select_range(list_anchor,a.id,io.KeyCtrl);
                else {
                    if(io.KeyCtrl) selection->toggle(a.id); else selection->replace({a.id});
                    list_anchor=a.id;
                }
                selected=selection->primary();
            }
        }
        ImGui::EndChild();
        if(ImGui::CollapsingHeader("Classes")) {
            ui::SetNextItemWidth(180);
            if(ui::BeginCombo("Registry",class_key.empty()?"New class":class_key.c_str())) {
                if(ImGui::Selectable("New class",class_key.empty())) { class_key.clear(); class_name[0]=0; }
                for(const auto& [name,c]:document.classes()) if(ImGui::Selectable(name.c_str(),name==class_key)) {
                    class_key=name; std::memcpy(class_name,name.c_str(),name.size()+1);
                    class_color[0]=((c.color>>16)&255)/255.f; class_color[1]=((c.color>>8)&255)/255.f; class_color[2]=(c.color&255)/255.f;
                }
                ui::EndCombo();
            }
            ui::SetNextItemWidth(180); ui::InputText("Class name",class_name,sizeof(class_name));
            ui::SetNextItemWidth(180); ui::ColorEdit3("RGB",class_color);
            if(ui::Button("Save color")) {
                try {
                    // Rename is a separate explicit command so one button is
                    // always one journal step and cannot partially rename.
                    std::string name=class_key.empty()?class_name:class_key;
                    auto byte=[](float v){ return static_cast<unsigned>(std::lround(std::clamp(v,0.f,1.f)*255)); };
                    document.set_class(name,(byte(class_color[0])<<16)|(byte(class_color[1])<<8)|byte(class_color[2]));
                    class_key=name; error.clear();
                } catch(const std::exception& e) { error=e.what(); }
            }
            if(!class_key.empty()) {
                ImGui::SameLine();
                if(ui::Button("Rename")) {
                    try { document.rename_class(class_key,class_name); class_key=class_name; error.clear(); }
                    catch(const std::exception& e) { error=e.what(); }
                }
                ImGui::Text("Assigned: %zu",document.class_count(class_key));
                if(ui::Button("Remove unused class")) {
                    try { document.remove_class(class_key); class_key.clear(); class_name[0]=0; error.clear(); }
                    catch(const std::exception& e) { error=e.what(); }
                }
            }
        }
        ImGui::Text("Selected: %zu",selection->size());
        if(selected && ui::Button("Delete selected")) delete_selection();
        if(selected) { ImGui::SameLine(); if(ui::Button("Clear selection")) { selection->clear(); selected=0; } }
        if(selection->size()>1) {
            if(ui::BeginCombo("Batch class",batch_class.empty()?"Unclassified":batch_class.c_str())) {
                if(ImGui::Selectable("Unclassified",batch_class.empty())) batch_class.clear();
                for(const auto& [name,c]:document.classes()) if(ImGui::Selectable(name.c_str(),name==batch_class)) batch_class=name;
                ui::EndCombo();
            }
            if(ui::Button("Classify selected")) {
                try { document.classify_objects(selection->ids(),batch_class); error.clear(); }
                catch(const std::exception& e) { error=e.what(); }
            }
            if(has_geos()) {
                ui::SetNextItemWidth(140);
                ui::Combo("Boolean",&boolean_operation,"Union\0Intersection\0Difference\0XOR\0");
                ui::TextDisabled("Target: primary; operands retained");
                ImGui::TextWrapped("Ellipses use 512-edge approximations.");
                if(ui::Button("Apply Boolean (B)") || (!io.WantTextInput && !editing_blocked && !io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_B))) {
                    try {
                        auto ids=selection->ids(); ids.erase(std::remove(ids.begin(),ids.end(),selected),ids.end());
                        document.apply_boolean(selected,ids,static_cast<BooleanOp>(boolean_operation)); error.clear();
                    } catch(const std::exception& e) { error=e.what(); }
                }
            }
            ui::TextDisabled("Inspector edits the primary object");
        }
        if (selected) {
            if (editor_id!=selected || editor_revision!=document.revision()) {
                auto a=document.annotation(selected); draft_geometry=a.geometry; draft_roi=a.geometry.bounds(); draft_parent=a.parent_id;
                std::memcpy(draft_name,a.name.c_str(),a.name.size()+1); std::memcpy(draft_class,a.class_name.c_str(),a.class_name.size()+1);
                editor_id=selected; editor_revision=document.revision();
            }
            const auto object=document.annotation(selected);
            if(tool==6) ImGui::TextWrapped("Drag a vertex; I near an edge inserts; Delete removes the active vertex. Escape cancels a drag.");
            if(tool==7) ImGui::TextWrapped("Click an interior ring, then Enter to add a hole. Escape cancels.");
            ImGui::Text("Kind: %s",object.kind==ObjectKind::Cell?"Cell":(object.kind==ObjectKind::Detection?"Detection":"Annotation"));
            if(object.kind==ObjectKind::Annotation && has_geos() &&
                (object.geometry.kind()==RoiKind::Rectangle || object.geometry.kind()==RoiKind::Ellipse)) {
                if(object.geometry.kind()==RoiKind::Ellipse) ImGui::TextWrapped("Polygon conversion uses 512 edges.");
                if(ui::Button("Convert to polygon")) {
                    try { document.update_roi(selected,object.geometry.as_polygon()); error.clear(); }
                    catch(const std::exception& e) { error=e.what(); }
                }
            }
            if(object.kind==ObjectKind::Cell) ui::TextDisabled(object.nucleus?"Nucleus ROI present":"No nucleus ROI");
            ui::InputText("Name",draft_name,sizeof(draft_name));
            if(ui::BeginCombo("Class",draft_class[0]?draft_class:"Unclassified")) {
                if(ImGui::Selectable("Unclassified",!draft_class[0])) draft_class[0]=0;
                for(const auto& [name,c]:document.classes()) if(ImGui::Selectable(name.c_str(),name==draft_class))
                    std::memcpy(draft_class,name.c_str(),name.size()+1);
                ui::EndCombo();
            }
            ImGui::BeginDisabled(object.is_detection());
            ui::InputDouble("X",&draft_roi.x); ui::InputDouble("Y",&draft_roi.y);
            ImGui::BeginDisabled(draft_geometry.kind()!=RoiKind::Rectangle && draft_geometry.kind()!=RoiKind::Ellipse);
            ui::InputDouble("Width",&draft_roi.width); ui::InputDouble("Height",&draft_roi.height); ImGui::EndDisabled(); ImGui::EndDisabled();
            ui::InputScalar("Parent ID",ImGuiDataType_U64,&draft_parent);
            if (ui::Button("Apply object edit")) {
                try {
                    auto bounds=draft_geometry.bounds();
                    auto geometry=draft_geometry.kind()==RoiKind::Rectangle?Roi::rectangle(draft_roi):
                        (draft_geometry.kind()==RoiKind::Ellipse?Roi::ellipse(draft_roi):draft_geometry.translated(draft_roi.x-bounds.x,draft_roi.y-bounds.y));
                    document.execute({ObjectEdit::replace_geometry(selected,geometry,draft_name,draft_parent,draft_class,object.kind,object.nucleus)},"Edit annotation"); error.clear();
                }
                catch(const std::exception& e) { error=e.what(); }
            }
        }
        if(workspace.enabled()) {ImGui::End();workspace.begin("Measurements and jobs",workspace.measurements);}
        ui::SeparatorText(selection->size()>1?"Measurements (primary object)":"Measurements");
        mean.poll(selected,document.revision());
        auto analysis_status=analysis.poll(document,selected);
        if(pending_analysis_recheck && !analysis.busy()) {
            if(analysis_status.state=="ready") {
                try {analysis.verify_result(document,pending_analysis_recheck);}
                catch(const std::exception& e) {error=e.what();}
            }
            pending_analysis_recheck=0;
        }
        if (selected) {
            try {
                const auto features=document.shape_measurements(selected);
                ImGui::Text("Area: %.6g px^2",features.roi.area_px2);
                ImGui::Text("Length: %.6g px",features.roi.length_px);
                if(features.roi.area_um2) {
                    ImGui::Text("Area: %.6g um^2",*features.roi.area_um2);
                    ImGui::Text("Length: %.6g um",*features.roi.length_um);
                } else ui::TextDisabled("Physical measurements: set calibration first");
                if(features.nucleus) {
                    ImGui::Text("Nucleus area: %.6g px^2",features.nucleus->area_px2);
                    ImGui::Text("Nucleus length: %.6g px",features.nucleus->length_px);
                    if(features.nucleus->area_um2) {
                        ImGui::Text("Nucleus area: %.6g um^2",*features.nucleus->area_um2);
                        ImGui::Text("Nucleus length: %.6g um",*features.nucleus->length_um);
                    }
                }
            } catch(const std::exception& e) {ImGui::TextWrapped("Measurements: %s",e.what());}
            const auto measured_cell=document.annotation(selected);
            if(measured_cell.cell_feature_link) {
                const auto link=*measured_cell.cell_feature_link;
                ui::SeparatorText("Captured cell features");
                ImGui::Text("Run: %llu / label: %u",static_cast<unsigned long long>(link.record_id),link.label);
                const auto status=document.cell_object_status(selected);ImGui::Text("Feature status: %s",status.c_str());
                if(status!="archive_missing" && status!="invalid_label") {
                    const auto f=document.cell_object_feature(selected);
                    ImGui::Text("Nucleus samples: %llu",static_cast<unsigned long long>(f.nucleus_pixels));
                    ImGui::Text("Cell samples: %llu",static_cast<unsigned long long>(f.cell_pixels));
                    ImGui::Text("Nucleus RGB: %.5g / %.5g / %.5g",f.nucleus_rgb[0],f.nucleus_rgb[1],f.nucleus_rgb[2]);
                    ImGui::Text("Cell RGB: %.5g / %.5g / %.5g",f.cell_rgb[0],f.cell_rgb[1],f.cell_rgb[2]);
                    ImGui::TextWrapped("Historical samples from the saved run. Geometry edits do not recompute these values.");
                }
            }
            if (mean.measured==selected && mean.value) ImGui::Text("Mean RGB: %.3f",*mean.value);
            ImGui::BeginDisabled(mean.active());
            if (ui::Button("Measure mean RGB")) mean.start(document.server(),document.annotation(selected),document.revision());
            ImGui::EndDisabled();
            if (mean.active()) ui::TextDisabled("Measuring...");
            if (!mean.error.empty()) ImGui::TextWrapped("%s",mean.error.c_str());
            ui::SeparatorText("Region analysis");
            if(ui::Button("Object classifiers (F4)")) classifier.open();
            if(ui::Button("Pixel classifiers (F5)")) pixel_classifier.open();
            bool analysis_changed=ui::Combo("Features",&analysis_kind,"RGB optical density\0Supplied stains\0");
            analysis_changed=ui::Checkbox("Use nucleus",&analysis_nucleus) || analysis_changed;
            analysis_changed=ui::InputScalarN("White RGB",ImGuiDataType_Double,analysis_white.data(),3,nullptr,nullptr,"%.5g") || analysis_changed;
            if(analysis_kind==1) {
                analysis_changed=ui::InputScalarN("Stain 1",ImGuiDataType_Double,analysis_vectors.data(),3,nullptr,nullptr,"%.5g") || analysis_changed;
                analysis_changed=ui::InputScalarN("Stain 2",ImGuiDataType_Double,analysis_vectors.data()+3,3,nullptr,nullptr,"%.5g") || analysis_changed;
                ImGui::TextWrapped("Supply validated OD directions. Channel 3 is a signed residual.");
            }
            if(analysis_changed) {pixel_classifier.invalidate_training();analysis.cancel();tissue.cancel();cell_job.cancel();stain_estimate.cancel();stain_estimate_applied=false;}
            if(ImGui::CollapsingHeader("Stain estimation settings")) {
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
            const bool estimate_key=!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F3,false);
            ImGui::BeginDisabled(stain_estimate.busy());
            const bool estimate_clicked=ui::Button("Estimate stains (F3)") || (estimate_key && !io.KeyCtrl && !io.KeyShift);
            ImGui::EndDisabled();
            if(estimate_clicked && !stain_estimate.busy()) {try {stain_estimate.request(StainEstimateRequest(document,selected,stain_estimate_parameters,{analysis_white[0],analysis_white[1],analysis_white[2]},analysis_nucleus));stain_estimate_applied=false;stain_estimate_error.clear();error.clear();} catch(const std::exception& e) {stain_estimate_error=e.what();}}
            if((estimate_key && io.KeyCtrl) || (stain_estimate.busy() && ui::Button("Cancel estimate (Ctrl+F3)"))) {stain_estimate.cancel();stain_estimate_applied=false;}
            const auto estimate_status=stain_estimate.poll(document,selected);
            if(stain_estimate.busy()) ImGui::Text("Stain tiles: %llu / %llu",static_cast<unsigned long long>(estimate_status.completed_tiles),static_cast<unsigned long long>(estimate_status.total_tiles));
            if(const auto estimate=stain_estimate.result()) {
                ImGui::Text("Stain separation: %.2f deg",estimate->angle_degrees);ImGui::Text("Plane residual: %.4g",estimate->residual_fraction);
                const bool use=ui::Button("Use estimated stains (Shift+F3)") || (estimate_key && io.KeyShift && !io.KeyCtrl);
                if(use) {
                    estimated_basis=estimate->stains;estimated_white=estimate->white;analysis_kind=1;
                    for(std::size_t c=0;c<3;++c) {analysis_vectors[c]=estimate->stains[0][c];analysis_vectors[c+3]=estimate->stains[1][c];analysis_white[c]=estimate->white[c];}
                    analysis.cancel();tissue.cancel();cell_job.cancel();stain_estimate_applied=true;
                }
                if(stain_estimate_applied) ImGui::TextWrapped("Estimated vectors are active for subsequent analysis and detection.");
            }
            if(!stain_estimate_error.empty()) ImGui::TextWrapped("%s",stain_estimate_error.c_str());
            if(!estimate_status.error.empty()) ImGui::TextWrapped("Stain estimate: %s",estimate_status.error.c_str());
            if(estimate_status.state=="stale") ImGui::TextWrapped("Stain estimation inputs changed; estimate again.");
            const bool analysis_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_M);
            ImGui::BeginDisabled(analysis.busy());
            const bool start_analysis=ui::Button("Analyze region (Ctrl+M)") || (analysis_key && !io.KeyShift);
            ImGui::EndDisabled();
            if(start_analysis && !analysis.busy()) {
                try {analysis.request(AnalysisRequest(document,selected,make_transform(analysis_kind==1),analysis_kind==0?AnalysisKind::OpticalDensityMeans:AnalysisKind::StainMeans,analysis_nucleus));error.clear();}
                catch(const std::exception& e) {error=e.what();}
            }
            if(analysis.busy()) {
                ImGui::Text("Tiles: %llu / %llu",static_cast<unsigned long long>(analysis_status.completed_tiles),static_cast<unsigned long long>(analysis_status.total_tiles));
                if(ui::Button("Cancel analysis") || (analysis_key && io.KeyShift)) analysis.cancel();
            }
            if(const auto result=analysis.result()) {
                const char* od_labels[]={"Mean OD R","Mean OD G","Mean OD B"};
                const char* stain_labels[]={"Mean stain 1","Mean stain 2","Mean residual"};
                for(std::size_t c=0;c<3;++c) ImGui::Text("%s: %.6g",analysis_kind==0?od_labels[c]:stain_labels[c],result->means[c]);
                ImGui::Text("%llu samples / %.3f ms",static_cast<unsigned long long>(result->samples),result->elapsed_ms);
            }
            if(!analysis_status.error.empty()) ImGui::TextWrapped("Analysis: %s",analysis_status.error.c_str());
            if(analysis_status.state=="stale") ui::TextDisabled("Inputs changed; run analysis again.");
            if(analysis_status.state=="cancelled") ui::TextDisabled("Analysis cancelled.");
            const bool archive_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K);
            const bool can_keep=analysis_status.state=="ready" && !analysis.busy() && kept_analysis_generation!=analysis_status.generation;
            ImGui::BeginDisabled(!can_keep);
            const bool keep_clicked=ui::Button("Keep result (Ctrl+K)") || (archive_key && !io.KeyShift);
            ImGui::EndDisabled();
            if(can_keep && keep_clicked) {
                try {analysis.keep_result(document,selected);kept_analysis_generation=analysis_status.generation;error.clear();}
                catch(const std::exception& e) {error=e.what();}
            }
            if(const auto saved=document.latest_analysis(selected)) {
                provenance_info(saved->result.provenance);
                const auto state=document.analysis_status(saved->id);
                ui::SeparatorText("Saved analysis");
                const char* label=state=="current"?"Verified for this session":state=="unverified"?"Needs recheck":
                    state=="object_changed"?"Object changed":state=="source_changed"?"Source changed":state=="source_closed"?"Image closed":"Recheck failed";
                ImGui::TextUnformatted(label);
                ImGui::Text("%s / %s",saved->result.kind==AnalysisKind::StainMeans?"Stains":"Optical density",saved->result.nucleus?"Nucleus":"Body");
                for(std::size_t c=0;c<3;++c) ImGui::Text("Saved channel %d: %.6g",static_cast<int>(c+1),saved->result.means[c]);
                const bool can_recheck=!analysis.busy() && state!="object_changed" && state!="source_closed";
                ImGui::BeginDisabled(!can_recheck);
                const bool recheck_clicked=ui::Button("Recheck saved (Ctrl+Shift+K)") || (archive_key && io.KeyShift);
                ImGui::EndDisabled();
                if(can_recheck && recheck_clicked) {
                    try {
                        auto replay=AnalysisRequest::replay(document,saved->id);
                        analysis.request(replay);pending_analysis_recheck=saved->id;
                        analysis_kind=saved->result.kind==AnalysisKind::StainMeans?1:0;analysis_nucleus=saved->result.nucleus;
                        analysis_white.assign(saved->result.white.begin(),saved->result.white.end());
                        for(std::size_t i=0;i<2;++i) for(std::size_t c=0;c<3;++c) analysis_vectors[i*3+c]=saved->result.stains[i][c];
                        error.clear();
                    } catch(const std::exception& e) {error=e.what();}
                }
                if(ImGui::TreeNode("Saved provenance")) {
                    ImGui::Text("Record: %llu",static_cast<unsigned long long>(saved->id));
                    ImGui::TextWrapped("Algorithm: %s",saved->result.algorithm.c_str());
                    ImGui::TextWrapped("Input SHA-256: %s",saved->result.input_sha256.c_str());ImGui::TreePop();
                }
            }
            ui::SeparatorText("Threshold tissue mask");
            const auto tissue_status=tissue.poll(document,selected);
            ImGui::PushItemWidth(132.f);
            bool tissue_changed=ui::Combo("Tissue feature",&tissue_feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0");
            const auto level_label=tissue_parameters.level<0?std::string("Auto (budget)"):"Level "+std::to_string(tissue_parameters.level);
            if(ui::BeginCombo("Tissue level",level_label.c_str())) {
                if(ImGui::Selectable("Auto (work budget)",tissue_parameters.level<0)) {tissue_parameters.level=-1;tissue_changed=true;}
                for(std::size_t i=0;i<image.levels.size();++i) {
                    const auto label="Level "+std::to_string(i)+" / "+std::to_string(image.levels[i].downsample)+"x";
                    if(ImGui::Selectable(label.c_str(),tissue_parameters.level==static_cast<std::int32_t>(i))) {tissue_parameters.level=static_cast<std::int32_t>(i);tissue_changed=true;}
                }
                ui::EndCombo();
            }
            tissue_changed=ui::InputDouble("Threshold >=",&tissue_parameters.threshold,0,0,"%.5g") || tissue_changed;
            tissue_changed=ui::Combo("Connectivity",&tissue_connectivity,"4 neighbors\0 8 neighbors\0") || tissue_changed;
            tissue_changed=ui::InputScalar("Min pixels",ImGuiDataType_U64,&tissue_parameters.min_pixels) || tissue_changed;
            tissue_changed=ui::InputScalar("Max (0=off)",ImGuiDataType_U64,&tissue_parameters.max_pixels) || tissue_changed;
            tissue_changed=ui::InputInt("Opening radius",&tissue_parameters.open_radius) || tissue_changed;
            tissue_changed=ui::InputInt("Closing radius",&tissue_parameters.close_radius) || tissue_changed;
            tissue_changed=ui::Checkbox("Fill enclosed holes",&tissue_parameters.fill_holes) || tissue_changed;
            if(tissue_parameters.fill_holes) tissue_changed=ui::InputScalar("Hole max (0=all)",ImGuiDataType_U64,&tissue_parameters.max_hole_pixels) || tissue_changed;
            ImGui::PopItemWidth();
            ui::TextDisabled("Radii and sizes use sampled pixels");
            if(tissue_changed) tissue.cancel();
            const bool tissue_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D,false);
            ImGui::BeginDisabled(tissue.busy());
            const bool detect_clicked=ui::Button("Detect tissue (Ctrl+D)") || (tissue_key && !io.KeyShift);
            ImGui::EndDisabled();
            if(detect_clicked && !tissue.busy()) {
                try {
                    tissue_parameters.feature=static_cast<TissueFeature>(tissue_feature);tissue_parameters.connectivity=tissue_connectivity?8:4;
                    tissue.request(TissueRequest(document,selected,make_transform(tissue_feature!=0),tissue_parameters,analysis_nucleus));tissue_annotations_added=0;error.clear();
                } catch(const std::exception& e) {error=e.what();}
            }
            if(tissue.busy()) {
                ui::TextDisabled("%s",tissue_status.phase=="contours"?"Tracing tissue contours...":tissue_status.phase=="cleanup"?"Cleaning tissue mask...":tissue_status.phase=="components"?"Finding components...":"Reading and thresholding...");
                if(ui::Button("Cancel tissue") || (tissue_key && io.KeyShift)) tissue.cancel();
            }
            ui::Checkbox("Show tissue mask",&show_tissue);
            if(const auto result=tissue.result()) {
                ImGui::Text("Level %d / %.5gx / %lld x %lld",result->level(),result->downsample(),static_cast<long long>(result->width()),static_cast<long long>(result->height()));
                ui::TextDisabled("Size filters count sampled pixels");
                ImGui::Text("Components: %zu",result->components().size());
                ImGui::Text("Kept pixels: %llu",static_cast<unsigned long long>(result->foreground_pixels()));
                ImGui::Text("Cleaned: %llu / filled: %llu",static_cast<unsigned long long>(result->cleaned_pixels()),static_cast<unsigned long long>(result->filled_pixels()));
                ImGui::Text("Samples: %llu / %.3f ms",static_cast<unsigned long long>(result->samples()),result->elapsed_ms());
                if(show_tissue && !tissue_preview.ready()) ui::TextDisabled("Preparing mask preview...");
            }
            const bool contour_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_J,false);
            const bool can_trace=has_geos() && !tissue.busy() && bool(tissue.result());
            ImGui::BeginDisabled(!can_trace);
            const bool trace_clicked=ui::Button("Trace contours (Ctrl+J)") || (contour_key && !io.KeyShift);
            ImGui::EndDisabled();
            if(trace_clicked && can_trace) {try {tissue.request_contours();tissue_annotations_added=0;error.clear();} catch(const std::exception& e) {error=e.what();}}
            const auto contours=tissue.contours();
            const bool can_keep_contours=contours && !contours->components().empty() && !tissue.busy() && !tissue.contours_published();
            ImGui::BeginDisabled(!can_keep_contours);
            const bool keep_contours_clicked=ui::Button("Add annotations (Ctrl+Shift+J)") || (contour_key && io.KeyShift);
            ImGui::EndDisabled();
            if(keep_contours_clicked && can_keep_contours) {try {tissue_annotations_added=tissue.keep_contours(document,selected).size();error.clear();} catch(const std::exception& e) {error=e.what();}}
            if(contours) ImGui::Text("Contours: %zu / %zu vertices",contours->components().size(),contours->vertex_count());
            if(tissue.contours_published()) ImGui::Text("Added %zu annotations (Undo available)",tissue_annotations_added);
            if(!has_geos()) ui::TextDisabled("Contour creation requires GEOS");
            const bool tissue_archive_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_U,false);
            const bool can_archive=tissue_status.state=="ready" && !tissue.busy() && bool(tissue.result()) && kept_tissue_generation!=tissue_status.generation;
            ImGui::BeginDisabled(!can_archive);
            const bool archive_clicked=ui::Button("Keep tissue mask (Ctrl+U)") || (tissue_archive_key && !io.KeyShift);
            ImGui::EndDisabled();
            if(can_archive && archive_clicked) {try {tissue.keep_result(document,selected);kept_tissue_generation=tissue_status.generation;error.clear();} catch(const std::exception& e) {error=e.what();}}
            if(const auto saved=document.latest_tissue(selected)) {
                provenance_info(saved->result->provenance());
                const auto state=document.tissue_status(saved->id);
                const char* saved_label=state=="current"?"Verified for this session":state=="unverified"?"Needs recheck":state=="object_changed"?"Object changed":state=="source_closed"?"Image closed":state=="source_changed"?"Source changed":"Recheck failed";
                ImGui::Text("Saved mask: %s",saved_label);
                ImGui::Text("Saved components: %zu",saved->result->components().size());
                const bool can_recheck_tissue=!tissue.busy() && state!="object_changed" && state!="source_closed";
                ImGui::BeginDisabled(!can_recheck_tissue);
                const bool recheck=ui::Button("Recheck saved mask (Ctrl+Shift+U)") || (tissue_archive_key && io.KeyShift);
                ImGui::EndDisabled();
                if(recheck && can_recheck_tissue) {try {
                    tissue.request(TissueRequest::replay(document,saved->id));verifying_tissue=saved->id;
                    tissue_parameters=saved->result->parameters();tissue_feature=static_cast<int>(tissue_parameters.feature);tissue_connectivity=tissue_parameters.connectivity==8?1:0;
                    analysis_nucleus=saved->result->nucleus();analysis_white.assign(saved->result->white().begin(),saved->result->white().end());
                    for(std::size_t i=0;i<2;++i) for(std::size_t c=0;c<3;++c) analysis_vectors[i*3+c]=saved->result->stains()[i][c];
                    error.clear();
                } catch(const std::exception& e) {error=e.what();}}
                ImGui::BeginDisabled(tissue.busy());
                if(ui::Button("Remove saved mask")) {try {document.remove_tissue(saved->id);error.clear();} catch(const std::exception& e) {error=e.what();}}
                ImGui::EndDisabled();
            }
            if(!tissue_status.error.empty()) ImGui::TextWrapped("Tissue: %s",tissue_status.error.c_str());
            if(!tissue_preview.error().empty()) ImGui::TextWrapped("Mask: %s",tissue_preview.error().c_str());
            ui::SeparatorText("Watershed cells");
            ImGui::PushItemWidth(118);
            ui::SetNextItemWidth(132);bool cell_changed=ui::Combo("Cell feature",&cell_feature,"OD sum\0Stain 1\0Stain 2\0Stain 3\0");
            cell_changed=ui::InputDouble("Cell threshold",&cell_parameters.threshold,0,0,"%.5g") || cell_changed;
            cell_changed=ui::InputDouble("Blur sigma",&cell_parameters.sigma,0,0,"%.3g") || cell_changed;
            cell_changed=ui::InputDouble("Peak radius >=",&cell_parameters.min_peak_radius,0,0,"%.3g") || cell_changed;
            cell_changed=ui::InputDouble("Seed spacing",&cell_parameters.min_seed_distance,0,0,"%.3g") || cell_changed;
            cell_changed=ui::InputDouble("Cell expansion",&cell_parameters.cell_expansion,0,0,"%.3g") || cell_changed;
            cell_changed=ui::InputScalar("Nucleus min px",ImGuiDataType_U64,&cell_parameters.min_nucleus_pixels) || cell_changed;
            cell_changed=ui::InputScalar("Nucleus max (0=all)",ImGuiDataType_U64,&cell_parameters.max_nucleus_pixels) || cell_changed;
            cell_changed=ui::Combo("Cell neighbors",&cell_connectivity,"4 neighbors\0 8 neighbors\0") || cell_changed;
            const auto cell_level_label=cell_parameters.level<0?std::string("Auto (budget)"):"Level "+std::to_string(cell_parameters.level);
            if(ui::BeginCombo("Cell level",cell_level_label.c_str())) {
                if(ImGui::Selectable("Auto (budget)",cell_parameters.level<0)) {cell_parameters.level=-1;cell_changed=true;}
                for(std::size_t i=0;i<image.levels.size();++i) {const auto label="Level "+std::to_string(i);if(ImGui::Selectable(label.c_str(),cell_parameters.level==static_cast<std::int32_t>(i))) {cell_parameters.level=static_cast<std::int32_t>(i);cell_changed=true;}}
                ui::EndCombo();
            }
            ui::TextDisabled("Radii/sizes use sampled pixels");
            if(cell_changed) cell_job.cancel();
            const auto cell_status=cell_job.poll(document,selected);
            const bool cell_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G,false);
            ImGui::BeginDisabled(cell_job.busy());
            const bool detect_cells_clicked=ui::Button("Detect cells (Ctrl+G)") || (cell_key && !io.KeyShift && !io.KeyAlt);
            ImGui::EndDisabled();
            if(detect_cells_clicked && !cell_job.busy()) {try {
                cell_parameters.feature=static_cast<TissueFeature>(cell_feature);cell_parameters.connectivity=cell_connectivity?8:4;
                cell_job.request(CellRequest(document,selected,make_transform(cell_feature!=0),cell_parameters));cell_annotations_added=0;error.clear();
            } catch(const std::exception& e) {error=e.what();}}
            if(cell_job.busy()) {ui::TextDisabled("Cells: %s",cell_status.phase.c_str());if(ui::Button("Cancel cells (Ctrl+Alt+G)")) cell_job.cancel();}
            if(cell_key && io.KeyAlt) cell_job.cancel();
            ui::Checkbox("Show cell preview",&show_cells);ui::Combo("Cell preview",&cell_preview_body,"Nuclei\0Cell bodies\0");
            ImGui::PopItemWidth();
            if(const auto result=cell_job.result()) {
                ImGui::Text("Detected cells: %zu",result->features().size());ImGui::Text("Markers: %zu / %.3f ms",result->markers().size(),result->elapsed_ms());
                ImGui::Text("Cell level: %d / %.5gx",result->nuclei()->level(),result->nuclei()->downsample());
            }
            const bool can_add_cells=has_geos() && !cell_job.busy() && bool(cell_job.result()) && !cell_job.result()->features().empty() && !cell_job.published();
            ImGui::BeginDisabled(!can_add_cells);const bool add_cells=ui::Button("Add cells (Ctrl+Shift+G)") || (cell_key && io.KeyShift && !io.KeyAlt);ImGui::EndDisabled();
            if(add_cells && can_add_cells) {try {cell_annotations_added=cell_job.keep_result(document,selected).size();error.clear();} catch(const std::exception& e) {error=e.what();}}
            if(cell_job.published()) ImGui::Text("Added %zu cells (Undo available)",cell_annotations_added);
            const bool cell_record_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C,false);
            const bool can_keep_cells=cell_status.state=="ready" && !cell_job.busy() && bool(cell_job.result()) && kept_cell_generation!=cell_status.generation;
            ImGui::BeginDisabled(!can_keep_cells);
            const bool keep_cells=ui::Button("Keep cell run (Ctrl+Shift+C)") || (cell_record_key && io.KeyShift && !io.KeyAlt);
            ImGui::EndDisabled();
            if(can_keep_cells && keep_cells) {try {cell_job.keep_record(document,selected);kept_cell_generation=cell_status.generation;error.clear();} catch(const std::exception& e) {error=e.what();}}
            if(const auto saved=document.latest_cells(selected)) {
                provenance_info(saved->result->nuclei()->provenance());
                const auto state=document.cell_status(saved->id);ImGui::Text("Saved cell run: %llu",static_cast<unsigned long long>(saved->id));
                ImGui::Text("Saved cells: %zu",saved->result->features().size());ImGui::Text("Cell record: %s",state.c_str());
                ImGui::BeginDisabled(cell_job.busy() || state=="object_changed" || state=="source_closed");
                const bool recheck=ui::Button("Recheck cells (Ctrl+Alt+C)") || (cell_record_key && io.KeyAlt && !io.KeyShift);
                ImGui::EndDisabled();
                if(recheck && !cell_job.busy() && state!="object_changed" && state!="source_closed") {try {
                    cell_job.request(CellRequest::replay(document,saved->id));verifying_cells=saved->id;error.clear();
                } catch(const std::exception& e) {error=e.what();}}
                ImGui::BeginDisabled(state!="current" || !has_geos());
                if(ui::Button("Add saved cells")) {try {document.publish_cell_record(saved->id);error.clear();} catch(const std::exception& e) {error=e.what();}}
                ImGui::EndDisabled();
                const bool can_export=!cell_table_options.published_objects && !cell_table_job.busy() && (!cell_table_options.require_current || state=="current");
                ImGui::BeginDisabled(!can_export);
                const bool export_table=ui::Button("Export cell table (Ctrl+Alt+X)") || (!io.WantTextInput && !editing_blocked && io.KeyCtrl && io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_X,false));
                ImGui::EndDisabled();
                if(can_export && export_table) {try {CellTableRequest request(document,{saved->id},cell_table_options);cell_table_job.request(request,cell_table_path);cell_table_error.clear();} catch(const std::exception& e) {cell_table_error=e.what();}}
            }
            if(!has_geos()) ui::TextDisabled("Cell objects require GEOS");
            if(!cell_status.error.empty()) ImGui::TextWrapped("Cells: %s",cell_status.error.c_str());
            if(!cell_preview.error().empty()) ImGui::TextWrapped("Cell preview: %s",cell_preview.error().c_str());

        } else ui::TextDisabled("Select an annotation");
        } else ui::TextDisabled("Select or add an image to use image tools.");
        if(!has_image && workspace.enabled()) {
            ImGui::End();workspace.begin("Project and objects",workspace.browser);ui::TextDisabled("No active image");
            ImGui::End();workspace.begin("Measurements and jobs",workspace.measurements);
        }
        if(has_image || options.project) {
            ui::SeparatorText("Cell table export");
            ui::SetNextItemWidth(240);ui::InputTextWithHint("##Cell table path","Cell table .csv destination",cell_table_path,sizeof(cell_table_path));
            ui::Checkbox("Require current measurements",&cell_table_options.require_current);
            ui::Checkbox("Join live cell objects",&cell_table_options.published_objects);
            if(cell_table_options.published_objects) {
                ImGui::TextWrapped("Current names, classes and geometry; archived feature values. Missing measurements remain blank.");
                ImGui::BeginDisabled(cell_table_job.busy() || !has_image);
                const bool export_objects=ui::Button("Export image cells (Ctrl+Alt+X)") || (!io.WantTextInput && !editing_blocked && io.KeyCtrl && io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_X,false));
                ImGui::EndDisabled();
                if(export_objects && !cell_table_job.busy() && has_image) {try {CellTableRequest request(document,{},cell_table_options);cell_table_job.request(request,cell_table_path);cell_table_error.clear();} catch(const std::exception& e) {cell_table_error=e.what();}}
            }
        }
        if(options.project) {
            ImGui::BeginDisabled(cell_table_job.busy());
            const bool export_project=ui::Button("Export project cells (Ctrl+Shift+X)") || (!io.WantTextInput && !editing_blocked && io.KeyCtrl && io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_X,false));
            ImGui::EndDisabled();
            if(export_project && !cell_table_job.busy()) {try {CellTableRequest request(*options.project,{},cell_table_options);cell_table_job.request(request,cell_table_path);cell_table_error.clear();} catch(const std::exception& e) {cell_table_error=e.what();}}
        }
        const auto table_status=cell_table_job.poll();
        if(table_status.state!="idle") {
            ImGui::Text("Cell table: %s",table_status.state.c_str());
            ImGui::Text("Cell rows: %llu / %llu",static_cast<unsigned long long>(table_status.rows),static_cast<unsigned long long>(table_status.total_rows));
            if(cell_table_job.busy() && ui::Button("Cancel table (Ctrl+Alt+Shift+X)")) cell_table_job.cancel();
            if(!table_status.error.empty()) ImGui::TextWrapped("%s",table_status.error.c_str());
        }
        if(!cell_table_error.empty()) ImGui::TextWrapped("%s",cell_table_error.c_str());
        const bool measurement_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q,false);
        bool export_measurement_image=measurement_key && io.KeyAlt && !io.KeyShift;
        bool export_measurement_project=measurement_key && io.KeyShift && !io.KeyAlt;
        if(measurement_key && io.KeyShift && io.KeyAlt) measurement_job.cancel();
        if(export_measurement_image || export_measurement_project) ImGui::SetNextItemOpen(true);
        if(ImGui::CollapsingHeader("ROI and tissue tables",options.measurement_table_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)) {
            ui::SetNextItemWidth(150);ui::Combo("Measurements",&measurement_kind,"ROI channel means\0Tissue components\0");
            ui::SetNextItemWidth(240);ui::InputTextWithHint("##Measurement destination","Measurement .csv destination",measurement_path,sizeof(measurement_path));
            ui::Checkbox("Require current ROI / tissue runs",&measurement_options.require_current);
            ImGui::TextWrapped("Archived results and capture-time calibration; empty tissue runs have a summary row.");
            ImGui::BeginDisabled(measurement_job.busy() || !has_image);
            export_measurement_image=ui::Button("Export image (Ctrl+Alt+Q)") || export_measurement_image;ImGui::EndDisabled();
            ImGui::BeginDisabled(measurement_job.busy() || !options.project);
            export_measurement_project=ui::Button("Export project (Ctrl+Shift+Q)") || export_measurement_project;ImGui::EndDisabled();
            const auto status=measurement_job.poll();
            if(status.state!="idle") {ImGui::Text("Measurements: %s",status.state.c_str());ImGui::Text("Rows: %llu / %llu",static_cast<unsigned long long>(status.rows),static_cast<unsigned long long>(status.total_rows));}
            if(measurement_job.busy() && ui::Button("Cancel export (Ctrl+Alt+Shift+Q)")) measurement_job.cancel();
            if(!status.error.empty()) ImGui::TextWrapped("%s",status.error.c_str());
            if(!measurement_error.empty()) ImGui::TextWrapped("%s",measurement_error.c_str());
        }
        if(!measurement_job.busy() && ((export_measurement_image && has_image) || (export_measurement_project && options.project))) {
            try {
                const auto kind=measurement_kind==0?MeasurementTableKind::Roi:MeasurementTableKind::Tissue;
                if(export_measurement_project) measurement_job.request(MeasurementTableRequest(*options.project,kind,{},measurement_options),measurement_path);
                else measurement_job.request(MeasurementTableRequest(document,kind,{},measurement_options),measurement_path);
                measurement_error.clear();
            } catch(const std::exception& e) {measurement_error=e.what();}
        }
        const bool fingerprint_key=!io.WantTextInput && !editing_blocked && io.KeyCtrl && io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F,false);
        const bool fingerprint_cancel=!io.WantTextInput && !editing_blocked && io.KeyCtrl && io.KeyShift && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F,false);
        bool fingerprint_start=fingerprint_key;
        if(fingerprint_key) ImGui::SetNextItemOpen(true);
        if(ImGui::CollapsingHeader("File checksum",options.file_fingerprint_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)) {
            ui::Checkbox("Associate active image",&fingerprint_associate);
            ImGui::BeginDisabled(fingerprint_associate);
            ui::SetNextItemWidth(240);ui::InputTextWithHint("##Fingerprint input","File to hash",fingerprint_path,sizeof(fingerprint_path));
            if(has_image && ui::Button("Use image file")) {
                try {const auto path=document.server()->source_locator();if(path.empty()) throw std::invalid_argument("This image has no file locator; enter a path.");if(path.size()>32768) throw std::length_error("image locator is too long");std::fill(std::begin(fingerprint_path),std::end(fingerprint_path),0);std::copy(path.begin(),path.end(),fingerprint_path);} catch(const std::exception& e) {fingerprint_error=e.what();}
            }
            ImGui::EndDisabled();
            if(fingerprint_associate) ImGui::TextWrapped("Hashes the active image path; JSON includes its reader generation.");
            ui::SetNextItemWidth(240);ui::InputTextWithHint("##Fingerprint output","Optional .json destination",fingerprint_output,sizeof(fingerprint_output));
            ImGui::TextWrapped("Hashes one file. Multi-file slides need separate entries.");
            ImGui::BeginDisabled(fingerprint_job.busy());if(ui::Button("Hash file (Ctrl+Shift+F)")) fingerprint_start=true;ImGui::EndDisabled();
            if(fingerprint_job.busy() && ui::Button("Cancel file hash")) fingerprint_job.cancel();
            const auto status=fingerprint_job.poll();ImGui::Text("File checksum: %s",status.state.c_str());
            if(status.state!="idle") {ImGui::TextWrapped("Bytes: %llu / %llu",static_cast<unsigned long long>(status.bytes),static_cast<unsigned long long>(status.total_bytes));ImGui::TextWrapped("%s",status.path.c_str());}
            if(const auto value=fingerprint_job.result()) {ImGui::TextUnformatted("SHA-256:");ImGui::TextUnformatted(value->sha256.substr(0,32).c_str());ImGui::TextUnformatted(value->sha256.substr(32).c_str());ImGui::TextWrapped("Observed file metadata was stable while reading.");}
            if(const auto association=fingerprint_job.association()) {ImGui::TextWrapped("Reader match: %s",association->status.c_str());ImGui::TextWrapped("Scope: %s",association->source.scope.c_str());ImGui::TextWrapped("Metadata comparison; not proof of historical decoder bytes.");}
            if(!status.error.empty()) ImGui::TextWrapped("%s",status.error.c_str());
            if(!fingerprint_error.empty()) ImGui::TextWrapped("%s",fingerprint_error.c_str());
        } else fingerprint_job.poll();
        if(fingerprint_cancel) fingerprint_job.cancel();
        if(fingerprint_start && !fingerprint_job.busy()) {try {if(fingerprint_associate) {
            if(!has_image) throw std::invalid_argument("select an image to associate its source");
            const auto source=document.server()->reader_source();if(!source) throw std::invalid_argument("reader source identity is unavailable");
            fingerprint_job.request_source(*source,fingerprint_output,fingerprint_options);
        } else fingerprint_job.request(fingerprint_path,fingerprint_output,fingerprint_options);fingerprint_error.clear();} catch(const std::exception& e) {fingerprint_error=e.what();}}
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        if(workspace.enabled()) {
            ImGui::End();canvas_visible=workspace.begin("Slide",workspace.canvas,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        } else {
            ImGui::EndChild(); ImGui::SameLine();
            ImGui::BeginChild("Canvas", {0,0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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
            drawing=marquee=vertex_dragging=false;active_vertex.reset();draft_points.clear();
        }
        if(canvas_visible) {previous_canvas=offset; previous_canvas_size=size;}
        if(has_image && canvas_visible) {
        if (fit) { viewport.fit(image.width, image.height, size.x, size.y); fit = false; }
        if(restore_view) {
            viewport.zoom=restore_view->zoom; viewport.center_on(restore_view->center,size.x,size.y);
            restore_view.reset(); published_view_inputs=view_inputs(size.x,size.y);
        }
        if (overview_center) { viewport.center_on(*overview_center,size.x,size.y); overview_center.reset(); }
        Point mouse{io.MousePos.x - offset.x, io.MousePos.y - offset.y};
        bool hovered = ImGui::IsItemHovered();
        if (hovered && io.MouseWheel != 0) viewport.zoom_at(mouse, std::pow(1.2, io.MouseWheel));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            viewport.origin.x += io.MouseDelta.x; viewport.origin.y += io.MouseDelta.y;
        }
        if(tool==6 && selected && document.annotation(selected).kind==ObjectKind::Annotation) {
            const auto& object=document.objects().at(selected);
            if(vertex_id!=selected || vertex_revision!=object.revision) {
                if(vertex_dragging) { vertex_dragging=false; error="Vertex edit cancelled: object changed"; }
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
                    if(!selected || document.annotation(selected).kind!=ObjectKind::Annotation || !document.annotation(selected).geometry.is_area())
                        error="Select an area annotation before drawing a hole";
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
            else if (!tool) {
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
            if(ImGui::IsKeyPressed(ImGuiKey_Escape)) vertex_dragging=false;
            else if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                try { document.edit_vertex(vertex_id,active_vertex->first,active_vertex->second,vertex_cursor); error.clear(); }
                catch(const std::exception& e) { error=e.what(); }
                vertex_dragging=false;
            }
        }
        if(tool==6 && !io.WantTextInput && !editing_blocked && !vertex_dragging) {
            if(active_vertex && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                try { document.remove_vertex(vertex_id,active_vertex->first,active_vertex->second); active_vertex.reset(); error.clear(); }
                catch(const std::exception& e) { error=e.what(); }
            }
            if(hovered && vertex_id && ImGui::IsKeyPressed(ImGuiKey_I)) {
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
                        if(!edge) throw std::invalid_argument("Point near an edge to insert a vertex");
                        document.insert_vertex(vertex_id,edge->first,edge->second,end);
                    }
                    active_vertex.reset(); error.clear();
                } catch(const std::exception& e) { error=e.what(); }
            }
        }
        if(marquee && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            try {
                auto p=viewport.screen_to_image(mouse);
                if(std::abs(p.x-marquee_anchor.x)*viewport.zoom<3 && std::abs(p.y-marquee_anchor.y)*viewport.zoom<3) {
                    auto hits=document.pick(p,4/viewport.zoom);
                    if(!hits.empty()) selection->toggle(hits.back());
                } else selection->select_region({std::min(p.x,marquee_anchor.x),std::min(p.y,marquee_anchor.y),
                    std::abs(p.x-marquee_anchor.x),std::abs(p.y-marquee_anchor.y)},marquee_additive);
                selected=selection->primary(); error.clear();
            } catch(const std::exception& e) { error=e.what(); }
            marquee=false;
        }
        if (drawing && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            Rect r{std::min(anchor.x,end.x), std::min(anchor.y,end.y), std::abs(anchor.x-end.x), std::abs(anchor.y-end.y)};
            if (r.width * viewport.zoom >= 2 && r.height * viewport.zoom >= 2) {
                try {
                    auto name="Region "+std::to_string(document.object_count()+1);
                    selected=tool==2?document.add_roi(Roi::ellipse(r),name):document.add_rectangle(r,name); selection->replace({selected}); error.clear();
                }
                catch (const std::exception& e) { error = e.what(); }
            }
            drawing = false;
        }
        if(!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if(!drawing && draft_points.empty() && !marquee && tool!=6) { selection->clear(); selected=0; }
            drawing=false; marquee=false; draft_points.clear();
        }
        if(((tool>=3 && tool<=5) || tool==7) && !draft_points.empty() && !io.WantTextInput && !editing_blocked && (ImGui::IsKeyPressed(ImGuiKey_Enter) || (hovered && ImGui::IsMouseDoubleClicked(0)))) {
            try {
                if(tool==7) {
                    if(selected!=hole_id || !document.objects().count(hole_id) || document.annotation(hole_id).revision!=hole_revision)
                        throw std::invalid_argument("Hole edit cancelled: target changed");
                    auto geometry=document.annotation(hole_id).geometry.as_polygon();
                    auto hole=Roi::polygon(draft_points); auto parts=geometry.polygon_parts(); bool added=false;
                    for(std::size_t i=0;i<parts.size();++i) if(Roi::polygon(parts[i].shell,parts[i].holes).covers(hole)) {
                        document.add_hole(hole_id,i,draft_points); added=true; break;
                    }
                    if(!added) throw std::invalid_argument("Hole must lie inside one polygon part without overlapping existing holes");
                } else {
                    auto geometry=tool==3?Roi::polygon(draft_points):(tool==4?Roi::polyline(draft_points):Roi::points(draft_points));
                    selected=document.add_roi(geometry,"Region "+std::to_string(document.object_count()+1)); selection->replace({selected});
                }
                draft_points.clear(); error.clear();
            } catch(const std::exception& e) { error=e.what(); }
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
        const OverlayKey next_overlay_key{&document,selection.get(),document.render_revision(),selection->revision(),selected,viewport.zoom,viewport.origin.x,viewport.origin.y,
            offset.x,offset.y,size.x,size.y,clip.x,clip.y,clip.z,clip.w,ImGui::GetFont(),ImGui::GetFontSize(),ImGui::GetIO().Fonts->TexID,
            draw->Flags,shared->TexUvWhitePixel.x,shared->TexUvWhitePixel.y,shared->CurveTessellationTol,shared->CircleSegmentMaxError,
            tiles->uploads(),tiles->generations(),tissue.poll(document,selected).generation,cell_job.poll(document,selected).generation,tiles->bytes(),tissue.result().get(),detected.get(),
            show_tissue,show_cells,tissue_preview.ready(),cell_preview.ready(),cell_preview_body,pixel_classifier.preview().generation(),pixel_classifier.show_overlay(),pixel_classifier.preview_ready()};
        const bool overlay_hit=overlay_cache.prepare(next_overlay_key,clip,max_texture,options.overlay_cache_budget);profile.cache_hit=overlay_hit;profile.cache_ms=profile_lap();
        if(overlay_hit) overlay_cache.draw(draw);
        else {
        for (const auto& tile : plan.tiles) if (auto texture=tiles->texture(tile)) {
            draw->AddImage(static_cast<ImTextureID>(texture),screen(viewport,{tile.bounds.x,tile.bounds.y},offset),
                screen(viewport,{tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height},offset));
        }
        if(show_tissue) tissue_preview.draw(draw,viewport,offset,size);
        pixel_classifier.draw_preview(draw,viewport,offset,size);
        if(show_cells) cell_preview.draw(draw,viewport,offset,size);
        auto visible_ids=document.query_region({visible_min.x,visible_min.y,visible_max.x-visible_min.x,visible_max.y-visible_min.y});
        visible_count=visible_ids.size();profile.query_ms=profile_lap();
        // Dense selections use a single sorted walk. Sparse views retain tree
        // lookups so a tiny viewport never copies a large off-screen selection.
        const auto selection_count=selection->size();
        const bool all_selected=selection_count==document.objects().size();
        const bool merge_selection=!all_selected && selection_count>=64 && visible_ids.size()>=64 && selection_count/4<=visible_ids.size();
        const auto selected_ids=merge_selection?selection->ids():std::vector<std::uint64_t>{};
        auto selected_cursor=selected_ids.begin();
        for (auto id:visible_ids) {
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
            draw_shape(draw,a.geometry,viewport,offset,color);
            if(a.nucleus) draw_shape(draw,*a.nucleus,viewport,offset,IM_COL32(195,145,255,255));
            // Detection labels at low zoom overwhelm the canvas and scale with
            // every visible detection. Keep annotation names and selected labels.
            if(!a.is_detection() || a.id==selected) draw->AddText(p0,color,a.name.c_str());
        }
        profile.geometry_ms=profile_lap();
        overlay_cache.capture(draw);
        }
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
                if(vertex_dragging) draw->AddPolyline(preview.data(),static_cast<int>(preview.size()),IM_COL32(255,120,100,255),document.annotation(vertex_id).geometry.is_area()?ImDrawFlags_Closed:0,2);
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
        std::string status=tiles->error().empty() ? (tiles->pending() ? "Loading image..." : "") : tiles->error();
        if (plan.truncated) status="View exceeds tile limit; zoom in for complete coverage";
        if(!draft_points.empty()) status="Enter or double-click: finish shape | Escape: cancel";
        if (!status.empty()) draw->AddText({offset.x+12,offset.y+12},IM_COL32(255,220,130,255),status.c_str());
        if (document.pixel_width_um()) {
            auto target=120/viewport.zoom * *document.pixel_width_um();
            if (target>0 && std::isfinite(target)) {
            auto magnitude=std::pow(10.,std::floor(std::log10(target)));
            auto nice=std::floor(target/magnitude)*magnitude;
            auto length=static_cast<float>(nice/ *document.pixel_width_um()*viewport.zoom);
            ImVec2 p{offset.x+20,offset.y+size.y-30};
            draw->AddLine(p,{p.x+length,p.y},IM_COL32_WHITE,3);
            auto label=std::to_string(nice); label.erase(label.find_last_not_of('0')+1); if (!label.empty() && label.back()=='.') label.pop_back();
            label+=" um"; draw->AddText({p.x,p.y-20},IM_COL32_WHITE,label.c_str());
            }
        }
        } else if(!has_image) {
            overview.update(nullptr,display); // Reap a cancelled old-source job in an empty workspace.
            overlay_cache.clear();overlay_display.reset();
            visible_count=0;
            ImGui::GetWindowDrawList()->AddText({offset.x+32,offset.y+32},IM_COL32(190,205,220,255),project_images.empty()?"No images in this project.\nAdd an image in Project images, or open another project.":"No image selected.\nSelect an image in Project images.");
        }
        if(!canvas_visible) visible_count=0;
        if(has_image && overview_draw) {
        // Draw into the inspector window's own draw list, keeping the image and
        // viewport outline clipped to the overview, not the main canvas.
        overview_draw->PushClipRect({std::max(overview_offset.x,overview_clip_min.x),std::max(overview_offset.y,overview_clip_min.y)},
            {std::min(overview_offset.x+overview_size.x,overview_clip_max.x),std::min(overview_offset.y+overview_size.y,overview_clip_max.y)},false);
        overview_draw->AddRectFilled(overview_offset,{overview_offset.x+overview_size.x,overview_offset.y+overview_size.y},IM_COL32(18,21,27,255));
        if(overview.ready()) overview_draw->AddImage(static_cast<ImTextureID>(overview.texture()),
            screen(overview_view,{0,0},overview_offset),screen(overview_view,{double(image.width),double(image.height)},overview_offset));
        else overview_draw->AddText({overview_offset.x+4,overview_offset.y+4},IM_COL32(210,215,225,255),
            overview.state()=="failed"?"Overview unavailable":"Loading overview...");
        auto top_left=viewport.screen_to_image({0,0}),bottom_right=viewport.screen_to_image({previous_canvas_size.x,previous_canvas_size.y});
        overview_draw->AddRect(screen(overview_view,top_left,overview_offset),screen(overview_view,bottom_right,overview_offset),IM_COL32(255,205,85,255),0,0,2);
        overview_draw->PopClipRect();
        }
        if(workspace.enabled()) ImGui::End(); else ImGui::EndChild();
        ImGui::EndDisabled();
        if(maintenance_popup_requested) {ImGui::OpenPopup("Delete recovery checkpoint?"); maintenance_popup_requested=false;}
        if(ui::BeginPopupModal("Delete recovery checkpoint?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            if(reviewed_checkpoint) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(520.f*ui_scale.logical,ui::available()));
                ImGui::TextWrapped("Permanently delete this recovery checkpoint? This does not discard the live workspace or change the primary project.");
                ImGui::TextWrapped("%s",reviewed_checkpoint->path.c_str());
                ImGui::TextWrapped("%s / %zu images",recovery_label(*reviewed_checkpoint).c_str(),reviewed_checkpoint->images);
                ImGui::TextWrapped("Automatic recovery saving or another open window may create another checkpoint later.");
                ImGui::PopTextWrapPos();
                if(ui::Button("Permanently delete checkpoint")) {
                    auto reviewed=*reviewed_checkpoint; maintenance_epoch=workspace_epoch; maintenance_error.clear();
                    maintenance_job=std::async(std::launch::async,[reviewed] {
                        MaintenanceResult result;
                        try {Project::discard_recovery_checked(reviewed); result.deleted=reviewed.path;} catch(const std::exception& e) {result.error=e.what();}
                        return result;
                    });
                    reviewed_checkpoint.reset(); maintenance_dialog_active=false; ImGui::CloseCurrentPopup();
                }
                confirm_checkpoint_offset=ImGui::GetItemRectMin(); auto confirm_max=ImGui::GetItemRectMax(); confirm_checkpoint_size={confirm_max.x-confirm_checkpoint_offset.x,confirm_max.y-confirm_checkpoint_offset.y}; mark_target(GuiInputTarget::ConfirmDiscardCheckpoint);
                ImGui::SameLine();
                if(ui::Button("Cancel")) {reviewed_checkpoint.reset(); maintenance_dialog_active=false; ImGui::CloseCurrentPopup();}
                cancel_checkpoint_offset=ImGui::GetItemRectMin(); auto cancel_max=ImGui::GetItemRectMax(); cancel_checkpoint_size={cancel_max.x-cancel_checkpoint_offset.x,cancel_max.y-cancel_checkpoint_offset.y}; mark_target(GuiInputTarget::CancelDiscardCheckpoint);
            }
            ImGui::EndPopup();
        }
        if(remove_popup_requested) { ImGui::OpenPopup("Remove image?"); remove_popup_requested=false; }
        if(ui::BeginPopupModal("Remove image?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            const auto item=std::find_if(project_images.begin(),project_images.end(),[&](const auto& image){return image.id==active_image;});
            ImGui::TextWrapped("Remove %s from this project?",item==project_images.end()?"this image":item->name.c_str());
            ImGui::TextWrapped("This removes its %zu objects, view and editing history from the workspace. Undo cannot restore the entry. Save project to persist the removal. The source image file stays on disk.",document.object_count());
            if(ui::Button("Remove image")) { pending_remove=active_image; remove_dialog_active=false; ImGui::CloseCurrentPopup(); }
            confirm_remove_offset=ImGui::GetItemRectMin(); auto confirm_max=ImGui::GetItemRectMax(); confirm_remove_size={confirm_max.x-confirm_remove_offset.x,confirm_max.y-confirm_remove_offset.y}; mark_target(GuiInputTarget::ConfirmRemoveImage);
            ImGui::SameLine();
            if(ui::Button("Cancel")) { remove_dialog_active=false; ImGui::CloseCurrentPopup(); }
            cancel_remove_offset=ImGui::GetItemRectMin(); auto cancel_max=ImGui::GetItemRectMax(); cancel_remove_size={cancel_max.x-cancel_remove_offset.x,cancel_max.y-cancel_remove_offset.y}; mark_target(GuiInputTarget::CancelRemoveImage);
            ImGui::EndPopup();
        }
        if(recovery_popup_requested) { ImGui::OpenPopup("Replace workspace?"); recovery_popup_requested=false; }
        if(ui::BeginPopupModal("Replace workspace?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            if(close_recovery_popup) { ImGui::CloseCurrentPopup(); close_recovery_popup=false; recovery_dialog_active=false; }
            else {
                ImGui::TextWrapped(staged_is_recovery?"Replace this workspace with the selected recovery? Save or discard its current unsaved changes first.":"Replace this workspace with the opened project? Save or discard its current unsaved changes first.");
                if(staged_project) ImGui::TextWrapped("Open: %s",staged_is_recovery?staged_project->recovery_source().c_str():(staged_project->path().empty()?"New empty project":staged_project->path().c_str()));
                if(staged_project && staged_is_recovery && !staged_project->path().empty()) ImGui::TextWrapped("Saved project location: %s",staged_project->path().c_str());
                ui::SetNextItemWidth(400); ui::InputTextWithHint("##Save before recovery","Current project destination",project_path,sizeof(project_path));
                ImGui::BeginDisabled(restore_pending || restore_after_save || save_job.busy());
                if(ui::Button(staged_is_recovery?"Save current and recover":"Save current and open")) { save_project_requested=true; restore_after_save=true; }
                save_recovery_offset=ImGui::GetItemRectMin(); auto sr=ImGui::GetItemRectMax(); save_recovery_size={sr.x-save_recovery_offset.x,sr.y-save_recovery_offset.y}; mark_target(GuiInputTarget::SaveRecoveryReplacement);
                if(ui::Button(staged_is_recovery?"Discard current and recover":"Discard current and open")) { autosave.cancel(); restore_pending=true; }
                discard_recovery_offset=ImGui::GetItemRectMin(); auto dr=ImGui::GetItemRectMax(); discard_recovery_size={dr.x-discard_recovery_offset.x,dr.y-discard_recovery_offset.y}; mark_target(GuiInputTarget::DiscardRecoveryReplacement);
                if(ui::Button("Cancel")) {
                    staged_project.reset();
                    recovery_dialog_active=false; ImGui::CloseCurrentPopup();
                }
                cancel_recovery_offset=ImGui::GetItemRectMin(); auto cr=ImGui::GetItemRectMax(); cancel_recovery_size={cr.x-cancel_recovery_offset.x,cr.y-cancel_recovery_offset.y}; mark_target(GuiInputTarget::CancelRecoveryReplacement);
                ImGui::EndDisabled();
                if(restore_pending) ui::TextDisabled("Preparing workspace...");
                if(!project_error.empty()) ImGui::TextWrapped("%s",project_error.c_str());
            }
            ImGui::EndPopup();
        }
        if(close_popup_requested) { ImGui::OpenPopup("Unsaved project"); close_popup_requested=false; }
        if(ui::BeginPopupModal("Unsaved project",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Save this project, keep a recovery copy, or discard its unsaved changes before closing.");
            ui::SetNextItemWidth(400); ui::InputTextWithHint("##Close save path","Project destination",project_path,sizeof(project_path));
            ui::SetNextItemWidth(400); ui::InputTextWithHint("##Close recovery path","Automatic recovery destination",autosave_path,sizeof(autosave_path));
            ImGui::BeginDisabled(close_action!=0);
            if(ui::Button("Save and close")) { save_project_requested=true; close_action=1; close_wait_only=false; }
            save_close_offset=ImGui::GetItemRectMin(); auto sc=ImGui::GetItemRectMax(); save_close_size={sc.x-save_close_offset.x,sc.y-save_close_offset.y}; mark_target(GuiInputTarget::SaveClose);
            if(ui::Button("Keep recovery and close")) { close_action=2; close_keep_started=false; }
            keep_close_offset=ImGui::GetItemRectMin(); auto kc=ImGui::GetItemRectMax(); keep_close_size={kc.x-keep_close_offset.x,kc.y-keep_close_offset.y}; mark_target(GuiInputTarget::KeepRecoveryClose);
            if(ui::Button("Discard and close")) { autosave.clear(); close_action=3; }
            discard_close_offset=ImGui::GetItemRectMin(); auto dc=ImGui::GetItemRectMax(); discard_close_size={dc.x-discard_close_offset.x,dc.y-discard_close_offset.y}; mark_target(GuiInputTarget::DiscardClose);
            if(ui::Button("Cancel")) { close_dialog_active=false; ImGui::CloseCurrentPopup(); }
            cancel_close_offset=ImGui::GetItemRectMin(); auto cc=ImGui::GetItemRectMax(); cancel_close_size={cc.x-cancel_close_offset.x,cc.y-cancel_close_offset.y}; mark_target(GuiInputTarget::CancelClose);
            ImGui::EndDisabled();
            if(close_action) ui::TextDisabled("Finishing save...");
            if(!project_error.empty()) ImGui::TextWrapped("%s",project_error.c_str());
            if(!autosave_ui_error.empty()) ImGui::TextWrapped("%s",autosave_ui_error.c_str());
            ImGui::EndPopup();
        }
        ImGui::End();
        batch.draw(options.project.get(),active_image,selection.get(),editing_blocked,tissue_parameters,cell_parameters,static_cast<AnalysisKind>(analysis_kind),analysis_nucleus,make_transform);
        classifier.draw(document,*selection,editing_blocked,has_image,mark_target);
        pixel_classifier.draw(document,*selection,editing_blocked,has_image,mark_target,make_transform);
        service_project(save_project_requested,autosave_requested,editing_blocked,true);
        profile.other_ms=profile_lap();
        ImGui::Render(); int w,h; glfwGetFramebufferSize(session.window,&w,&h); last_width=w; last_height=h; glViewport(0,0,w,h);
        glClearColor(.07f,.08f,.1f,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        profile.render_ms=profile_lap();profile.visible=visible_count;
        rendered_window_width=static_cast<int>(io.DisplaySize.x);rendered_window_height=static_cast<int>(io.DisplaySize.y);
        auto cpu_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count();
        profile.total_ms=cpu_ms;profiler.add(profile);
        if (frames>=10 || startup_requested) {
            if (frame_times.size()==4096) frame_times.erase(frame_times.begin(),frame_times.begin()+2048);
            frame_times.push_back(cpu_ms);
        }
        replay.finish_frame(!settling);
        if(settling) ++settle_frames;
        const bool tiles_settled=pixel_classifier.settled() && !startup_active && !project_load.valid() && !restore_pending && (!canvas_visible || (!tiles->pending() && tiles->error().empty())) && (!has_image || overview.settled());
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
                auto kind=options.startup_kind,path=options.startup_path,base=options.recovery_base_path;
                const auto image_id=options.startup_image_id;
                project_load=std::async(std::launch::async,[kind,path,base,image_id,progress=startup_progress] {
                    const auto start=std::chrono::steady_clock::now();
                    std::shared_ptr<Project> candidate;
                    if(kind=="image") {
                        if(progress->cancelled.load()) throw std::runtime_error("image opening cancelled");
                        candidate=std::make_shared<Project>(); candidate->add_image(path);
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
        if(final_frame) break;
    }
    // Finite embedded runs also return a durable save result and reconcile the
    // caller's project. No live authority or Python-owned objects enter workers.
    batch.finish();
    classifier.finish(*active_document,*selection,frame_limit>0);
    pixel_classifier.finish(*active_document,*selection,frame_limit>0);
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
    const bool layout_saved=workspace.save();
    return {frames,previous_uploads+tiles->uploads(),std::max(previous_peak_bytes,tiles->peak_bytes()),previous_generations+tiles->generations(),p95,tiles->pending(),empty?0:viewport.zoom,empty?Point{}:viewport.origin,
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
        tissue_status.elapsed_ms,tissue_preview.ready(),tissue_result?tissue_result->level():-1,tissue_result?tissue_result->downsample():0,
        tissue_result?tissue_result->width():0,tissue_result?tissue_result->height():0,tissue_result?tissue_result->cleaned_pixels():0,
        tissue_result?tissue_result->filled_pixels():0,tissue_result?tissue_result->cleanup_ms():0,
        replay.frame(),replay.waits(),settle_frames,replay.complete(),!startup_active && !project_load.valid() && !restore_pending && (!canvas_visible || (!tiles->pending() && tiles->error().empty())) && (empty || overview.settled()),
        tissue_contours?tissue_contours->components().size():0,tissue_contours?tissue_contours->edge_count():0,tissue_contours?tissue_contours->vertex_count():0,
        tissue.contours_published()?tissue_annotations_added:0,tissue_contours?tissue_contours->elapsed_ms():0,tissue.contours_published(),
        saved_tissue?saved_tissue->id:0,saved_tissue?active_document->tissue_status(saved_tissue->id):std::string{},
        final_cells.state,final_cells.error.empty()?cell_preview.error():final_cells.error,cell_job.result(),cell_job.published()?cell_annotations_added:0,cell_preview.peak_bytes(),cell_preview.ready(),cell_job.published(),
        overlay_cache.bytes(),overlay_cache.peak_bytes,overlay_cache.builds,overlay_cache.hits,overlay_cache.fallbacks,
        saved_cells?saved_cells->id:0,saved_cells?active_document->cell_status(saved_cells->id):std::string{},
        final_table.state,cell_table_error.empty()?final_table.error:cell_table_error,final_table.path,final_table.rows,final_table.bytes,final_table.capture_ms,final_table.write_ms,
        final_fingerprint.state,final_fingerprint.error.empty()?fingerprint_error:final_fingerprint.error,fingerprint_job.result(),final_fingerprint.bytes,final_fingerprint.elapsed_ms,
        final_measurement.state,measurement_error.empty()?final_measurement.error:measurement_error,final_measurement.rows,final_measurement.capture_ms,final_measurement.write_ms,fingerprint_job.association(),final_estimate.state,stain_estimate_error.empty()?final_estimate.error:stain_estimate_error,estimate_result,stain_estimate_applied,batch.stats().state,batch.error(),batch.result(),suspended_iterations,suspended_work_ms,suspended_wait_ms,bool(glfwGetWindowAttrib(session.window,GLFW_ICONIFIED)),overview.state(),overview.error(),overview.result(),overview.bytes(),overview.peak_bytes(),workspace.enabled(),workspace.loaded(),layout_saved,workspace.error(),workspace.browser,workspace.display,workspace.measurements,workspace.canvas,canvas_visible,ui_scale.monitor,ui_scale.framebuffer,ui_scale.logical,font_rebuilds,ui::layout_stats.stacked_fields,ui::layout_stats.wrapped_buttons,ui::layout_stats.overflow_items,ui::layout_stats.max_overflow,ui::layout_stats.first_overflow,classifier.stats().state,classifier.error(),classifier.io_state(),classifier.io_error(),classifier.model(),classifier.result(),classifier.changed(),classifier.capture_ms(),classifier.record_id(),pixel_classifier.training_stats().state,pixel_classifier.prediction_stats().state,pixel_classifier.error(),pixel_classifier.io_state(),pixel_classifier.io_error(),pixel_classifier.model(),pixel_classifier.table(),pixel_classifier.result(),pixel_classifier.preview_ready(),canvas_visible && pixel_classifier.overlay_visible(),pixel_classifier.preview().bytes(),pixel_classifier.preview().peak_bytes(),pixel_classifier.preview().completed_tiles(),pixel_classifier.preview().generation(),pixel_classifier.capture_ms(),pixel_classifier.catalog_inspected(),pixel_classifier.catalog_max_step(),pixel_classifier.catalog_complete(),Rect{previous_canvas.x,previous_canvas.y,previous_canvas_size.x,previous_canvas_size.y},pixel_classifier.contour_state(),pixel_classifier.traced(),pixel_classifier.published_ids(),pixel_classifier.contours_published()};
}
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
