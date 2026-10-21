#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_recovery.hpp"
#include "qupathxx/project_save.hpp"
#include "qupathxx/interchange.hpp"
#include "gui_tiles.hpp"
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
    if (frame_limit < 0) throw std::invalid_argument("frame_limit must be nonnegative");
    if (!capture_path.empty() && !frame_limit) throw std::invalid_argument("capture_path requires a positive frame_limit");
    options.display.validate();
    if(!std::isfinite(options.autosave_interval_seconds) || options.autosave_interval_seconds<0 || options.autosave_interval_seconds>86400)
        throw std::invalid_argument("autosave interval must be between 0 and 86400 seconds");
    if(options.project && options.project->active_image() && options.project->document(options.project->active_image()).get()!=&document)
        throw std::invalid_argument("GUI document must be the project's active image");
    if(options.start_view && (!std::isfinite(options.start_view->zoom) || options.start_view->zoom<=0 ||
        !std::isfinite(options.start_view->origin.x) || !std::isfinite(options.start_view->origin.y))) throw std::invalid_argument("invalid initial viewport");
    if (options.width<640 || options.width>8192 || options.height<480 || options.height>8192) throw std::invalid_argument("invalid window dimensions");
    for (const auto& event:options.inputs) if (event.frame<2 || event.frame>=frame_limit ||
        !std::isfinite(event.x) || !std::isfinite(event.y) ||
        (event.target!=GuiInputTarget::Image && (event.x<0 || event.x>1 || event.y<0 || event.y>1)) ||
        !std::isfinite(event.wheel) || std::abs(event.wheel)>100)
        throw std::invalid_argument("invalid replay event; frame must be inside finite run, starting at 2");
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
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); session.context = true;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle(); style.FrameRounding = 4; style.WindowRounding = 0;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.075f, .085f, .105f, 1);
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
    if(options.project && !frame_limit) {
        auto base=options.project->path(); recovery_scan_base=base; recovery_scan_generation=options.project->generation();
        recovery_scan=std::async(std::launch::async,[base]{return Project::find_recoveries(base);});
    }
    std::shared_ptr<Project> staged_project;
    bool staged_is_recovery=true;
    struct LoadedProject { std::shared_ptr<Project> project; double ms=0; bool recovery=false; };
    auto load_project=[](const std::string& path,bool recovery,const std::string& base_path) {
        const auto start=std::chrono::steady_clock::now();
        auto candidate=recovery?Project::recover(path,base_path):Project::open(path);
        return LoadedProject{std::move(candidate),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),recovery};
    };
    std::future<LoadedProject> project_load;
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
    bool interchange_pixels=false,interchange_losses=false,interchange_selected=false,csv_intensity=false;
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
    std::size_t visible_count=0;
    std::vector<double> frame_times;
    TilePlan plan;
    TilePlan overview_plan;
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
    while (!exit_requested && (!frame_limit || frames < frame_limit)) {
        auto frame_start=std::chrono::steady_clock::now();
        if(retired_workspace.valid() && retired_workspace.wait_for(std::chrono::seconds(0))==std::future_status::ready)
            max_retirement_ms=std::max(max_retirement_ms,retired_workspace.get());
        poll_maintenance();
        auto previous_autosave_state=autosave_stats.state;
        autosave_stats=autosave.poll();
        if(autosave_stats.state=="error" && previous_autosave_state!="error")
            next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(std::max(5.,autosave_interval));
        if(project_load.valid() && !close_dialog_active && !remove_dialog_active && project_load.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {
                auto loaded=project_load.get();
                if(loaded.recovery) recovery_load_ms=loaded.ms; else project_load_ms=loaded.ms;
                if(!project_load_cancelled) {
                    staged_project=std::move(loaded.project); staged_is_recovery=loaded.recovery;
                    drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
                    if(options.project->dirty()) recovery_popup_requested=recovery_dialog_active=true;
                    else { autosave.cancel(); restore_pending=true; }
                    project_error.clear();
                }
            } catch(const std::exception& e) { if(!project_load_cancelled) project_error=e.what(); }
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
        if(glfwWindowShouldClose(session.window) && !restore_pending && !pending_remove) {
            project_load_cancelled=true;
            glfwSetWindowShouldClose(session.window,false);
            if(save_job.busy()) { close_popup_requested=close_dialog_active=true; close_action=1; close_wait_only=true; }
            else if(options.project && options.project->dirty()) { close_popup_requested=close_dialog_active=true; }
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
                active_image=options.project->active_image(); pending_image=pending_remove=0; refresh_active_image=false;
                restore_view=active_image?options.project->view(active_image):std::nullopt; viewport={}; display=restore_view?restore_view->display:options.display; fit=!restore_view;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); vertex_paths.clear(); vertex_id=hole_id=editor_id=selected=list_anchor=0;
                tool=previous_tool=0; class_key.clear(); batch_class.clear(); error.clear(); interchange_status.clear();
                list_ids.clear(); list_revision=calibration_revision=std::numeric_limits<std::uint64_t>::max();
                plan={}; overview_plan={}; plan_inputs.reset(); published_view_inputs.reset(); overview_center.reset();
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
                ++project_replacements; close_recovery_popup=recovery_dialog_active;
                project_filter[0]=project_image_path[0]=interchange_path[0]=0;
                next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
            } catch(const std::exception& e) { project_error=e.what(); }
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
                    plan={}; overview_plan={}; plan_inputs.reset(); overview_center.reset();
                    max_image_switch_ms=std::max(max_image_switch_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count());
                }
                refresh_active_image=false; project_error.clear();
            } catch(const std::exception& e) { project_error=e.what(); refresh_active_image=false; }
        }
        auto& document=*active_document;
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
        glfwPollEvents(); ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame();
        // Test/benchmark replay enters the same ImGui event queue as GLFW input.
        auto& input=ImGui::GetIO();
        if (!options.inputs.empty()) {
            input.AddKeyEvent(ImGuiKey_R,false); input.AddKeyEvent(ImGuiKey_Z,false); input.AddKeyEvent(ImGuiKey_Y,false);
            input.AddKeyEvent(ImGuiMod_Ctrl,false); input.AddKeyEvent(ImGuiMod_Shift,false); input.AddKeyEvent(ImGuiKey_Delete,false);
            for(auto key:{ImGuiKey_E,ImGuiKey_P,ImGuiKey_L,ImGuiKey_T,ImGuiKey_Enter,ImGuiKey_A,ImGuiKey_Escape,ImGuiKey_V,ImGuiKey_H,ImGuiKey_I,ImGuiKey_B,ImGuiKey_S,ImGuiKey_O,ImGuiKey_N,ImGuiKey_Backspace,ImGuiKey_F5,ImGuiKey_F6,ImGuiKey_F7,ImGuiKey_PageDown,ImGuiKey_PageUp,ImGuiKey_F8,ImGuiKey_F9,ImGuiKey_F10,ImGuiKey_F11,ImGuiKey_F12}) input.AddKeyEvent(key,false);
        }
        for (const auto& event:options.inputs) if (event.frame==frames) {
            auto origin=event.target==GuiInputTarget::Canvas?previous_canvas:(event.target==GuiInputTarget::Overview?overview_offset:list_offset);
            auto extent=event.target==GuiInputTarget::Canvas?previous_canvas_size:(event.target==GuiInputTarget::Overview?overview_size:list_size);
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
                default: break;
            }
        }
        ImGui::NewFrame();
        bool save_project_requested=false;
        bool autosave_requested=false;
        const bool editing_blocked=close_dialog_active || recovery_dialog_active || restore_pending || remove_dialog_active || maintenance_dialog_active;
        const auto& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginDisabled(editing_blocked);
        ImGui::TextColored({.3f,.8f,.75f,1}, "qupathxx"); ImGui::SameLine(); ImGui::TextDisabled("/  Research workspace");
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
        ImGui::SetNextItemWidth(125);
        ImGui::Combo("Tool",&tool,"Select\0Rectangle\0Ellipse\0Polygon\0Polyline\0Points\0Vertices\0Hole\0"); ImGui::SameLine();
        if(tool!=previous_tool) { vertex_dragging=false; active_vertex.reset(); marquee=false; drawing=false; draft_points.clear(); previous_tool=tool; }
        if (ImGui::Button("Fit image")) fit = true;
        auto history_action=[&](bool redo) { try { if(redo) document.redo(); else document.undo(); error.clear(); } catch(const std::exception& e) { error=e.what(); } };
        if (!io.WantTextInput && !editing_blocked && has_image && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) history_action(false);
        if (!io.WantTextInput && !editing_blocked && has_image && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) history_action(true);
        ImGui::SameLine(); ImGui::BeginDisabled(!document.can_undo()); if (ImGui::Button("Undo")) history_action(false); ImGui::EndDisabled();
        ImGui::SameLine(); ImGui::BeginDisabled(!document.can_redo()); if (ImGui::Button("Redo")) history_action(true); ImGui::EndDisabled();
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
        ImGui::SameLine(); ImGui::TextDisabled("Ctrl+click: toggle | Shift+drag: select box | Ctrl+A: all | Right drag: pan");
        ImGui::EndDisabled();
        ImGui::BeginChild("Inspector", {285,0}, ImGuiChildFlags_Borders);
        if(options.project) {
            // Route these chords so ImGui's own PageUp/PageDown navigation
            // cannot also focus/activate an inspector text field.
            const bool next_image=ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_PageDown,ImGuiInputFlags_RouteGlobal);
            const bool previous_image=ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_PageUp,ImGuiInputFlags_RouteGlobal);
            ImGui::SeparatorText(options.project->dirty()?"Project - unsaved":"Project");
            ImGui::BeginDisabled(save_job.busy());
            ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##Project path","Destination .qpxproj file",project_path,sizeof(project_path));
            if(ImGui::Button("Save project (Ctrl+S)") || (!editing_blocked && !save_job.busy() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))) {
                save_project_requested=true;
            }
            ImGui::EndDisabled();
            if(save_job.busy()) ImGui::TextDisabled("Saving project...");
            ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##Open project path","Project file to open",project_open_path,sizeof(project_open_path));
            ImGui::BeginDisabled(project_load.valid() || save_job.busy() || maintenance_job.valid());
            if(ImGui::Button("Open project (Ctrl+O)") || (!editing_blocked && !project_load.valid() && !save_job.busy() && !maintenance_job.valid() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))) {
                const std::string path=project_open_path;
                project_load_cancelled=false; loading_recovery=false; project_error.clear();
                project_load=std::async(std::launch::async,load_project,path,false,std::string{});
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(project_load.valid() || save_job.busy() || maintenance_job.valid());
            if(ImGui::Button("New project (Ctrl+N)") || (!editing_blocked && !project_load.valid() && !save_job.busy() && !maintenance_job.valid() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))) {
                staged_project=std::make_shared<Project>(); staged_is_recovery=false;
                drawing=marquee=vertex_dragging=false; active_vertex.reset(); draft_points.clear(); tool=previous_tool=0;
                if(options.project->dirty()) recovery_popup_requested=recovery_dialog_active=true;
                else { autosave.cancel(); restore_pending=true; }
                project_error.clear();
            }
            ImGui::EndDisabled();
            if(project_load.valid()) {
                ImGui::TextDisabled(project_load_cancelled?"Finishing cancelled load...":(loading_recovery?"Opening recovery...":"Opening project..."));
                ImGui::BeginDisabled(project_load_cancelled);
                if(ImGui::Button("Cancel opening")) project_load_cancelled=true;
                cancel_load_offset=ImGui::GetItemRectMin(); auto cl=ImGui::GetItemRectMax(); cancel_load_size={cl.x-cancel_load_offset.x,cl.y-cancel_load_offset.y};
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
                ImGui::SetNextItemWidth(170);
                if(ImGui::InputTextWithHint("##Image filter","Filter image names",project_filter,sizeof(project_filter))) project_filter_dirty=true;
                ImGui::SameLine(); if(ImGui::SmallButton("Refresh")) project_catalog_dirty=true;
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
                ImGui::TextDisabled("%zu of %zu images",project_rows.size(),project_images.size());
                ImGui::BeginDisabled(!has_image || project_load.valid());
                if(ImGui::Button("Remove image (F12)")) remove_image=true;
                ImGui::EndDisabled();
                if(ImGui::TreeNodeEx("Image source / name",options.project_image_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SetNextItemWidth(240); ImGui::InputText("Image name",project_image_name,sizeof(project_image_name));
                    ImGui::BeginDisabled(!has_image);
                    if(ImGui::Button("Rename image")) {
                        try { options.project->rename_image(active_image,project_image_name); project_catalog_dirty=true; project_error.clear(); }
                        catch(const std::exception& e) { project_error=e.what(); }
                    }
                    ImGui::EndDisabled();
                    ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##Image source","Image file to add or relink",project_image_path,sizeof(project_image_path));
                    if(ImGui::Button("Add image (F8)")) add_image=true;
                    auto current=std::find_if(project_images.begin(),project_images.end(),[&](const auto& item){return item.id==active_image;});
                    ImGui::BeginDisabled(current==project_images.end() || current->embedded);
                    if(ImGui::Button("Relink image (F9)")) relink_image=true;
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
            autosave_requested=!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F10);
            if(reveal_recoveries) {ImGui::SetNextItemOpen(true); reveal_recoveries=false;}
            if(ImGui::CollapsingHeader("Recovery",(!options.recovery_path.empty() || !recovery_candidates.empty() || !options.project->recovery_source().empty())?ImGuiTreeNodeFlags_DefaultOpen:0)) {
                if(!options.project->recovery_source().empty() && options.project->dirty()) ImGui::TextWrapped("Recovered workspace. Save this project to retain its changes.");
                ImGui::TextWrapped("Recovery preserves unsaved work separately from Save project.");
                ImGui::SetNextItemWidth(110);
                if(ImGui::InputDouble("Interval (seconds)",&autosave_interval,10,60,"%.1f")) {
                    if(!std::isfinite(autosave_interval) || autosave_interval<0 || autosave_interval>86400) autosave_interval=60;
                    next_autosave=std::chrono::steady_clock::now()+std::chrono::duration<double>(autosave_interval);
                }
                ImGui::TextDisabled("0 disables periodic recovery.");
                ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##Autosave path","Automatic destination",autosave_path,sizeof(autosave_path));
                ImGui::BeginDisabled(autosave.busy());
                if(ImGui::Button("Save recovery now (F10)")) autosave_requested=true;
                ImGui::EndDisabled();
                if(autosave.busy()) ImGui::TextDisabled("Writing recovery...");
                else if(autosave_stats.state=="ready") ImGui::TextDisabled("Recovery saved; project still unsaved.");
                if(!autosave_stats.error.empty()) ImGui::TextWrapped("%s",autosave_stats.error.c_str());
                if(!autosave_stats.work_cleanup_error.empty()) ImGui::TextWrapped("Recovery work cleanup: %s",autosave_stats.work_cleanup_error.c_str());
                bool retention_changed=ImGui::Checkbox("Remove old inactive checkpoints",&retention.enabled);
                if(retention.enabled) {
                    int keep=static_cast<int>(retention.keep_latest);
                    if(ImGui::InputInt("Keep newest",&keep)) {retention.keep_latest=static_cast<std::size_t>(std::clamp(keep,1,1024)); retention_changed=true;}
                    if(ImGui::InputDouble("Minimum age (days)",&retention.max_age_days,1,7,"%.1f")) {
                        if(!std::isfinite(retention.max_age_days)) retention.max_age_days=30;
                        retention.max_age_days=std::clamp(retention.max_age_days,1.,36500.); retention_changed=true;
                    }
                    ImGui::TextWrapped("After recovery saves, keep recent checkpoints and active sessions; collect at most 32 old inactive files.");
                }
                if(retention_changed) autosave.set_retention(retention);
                if(!autosave_stats.retention_error.empty()) ImGui::TextWrapped("Recovery retention: %s",autosave_stats.retention_error.c_str());
                if(!autosave_ui_error.empty()) ImGui::TextWrapped("%s",autosave_ui_error.c_str());
                if(ImGui::Button("Find recovery files (F5)")) find_recoveries=true;
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
                ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##Restore recovery","Recovery file to open",recovery_path,sizeof(recovery_path));
                ImGui::TextUnformatted("Primary location (optional)");
                ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##Recovery primary","Use recorded location when empty",recovery_base_path,sizeof(recovery_base_path));
                ImGui::BeginDisabled(autosave.busy() || project_load.valid() || save_job.busy() || maintenance_job.valid());
                if(ImGui::Button("Open recovery (F11)")) recover_requested=true;
                if(ImGui::Button("Delete selected checkpoint...")) review_checkpoint=true;
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+Shift+Backspace: review checkpoint deletion");
                ImGui::EndDisabled();
                if(maintenance_job.valid()) ImGui::TextDisabled("Working on recovery file...");
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
        ImDrawList* overview_draw=nullptr;
        ImVec2 overview_clip_min{},overview_clip_max{};
        if(has_image) {
        if(has_interchange() && ImGui::CollapsingHeader("Import / export",options.interchange_path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SetNextItemWidth(240); ImGui::InputTextWithHint("##Interchange path",".geojson or .csv destination",interchange_path,sizeof(interchange_path));
            ImGui::TextWrapped("Full-resolution image pixels, top-left origin.");
            ImGui::Checkbox("Assume image pixels",&interchange_pixels);
            ImGui::Checkbox("Allow lossy conversion",&interchange_losses);
            ImGui::Checkbox("Selected objects only",&interchange_selected);
            auto export_ids=[&]() {
                auto ids=interchange_selected?selection->ids():std::vector<std::uint64_t>{};
                if(interchange_selected && ids.empty()) throw std::invalid_argument("Select objects before exporting a subset");
                return ids;
            };
            auto status=[&](const InterchangeReport& report,const char* action) {
                interchange_status=std::string(action)+" "+std::to_string(report.features)+" objects";
                for(const auto& issue:report.issues) interchange_status+="\n"+issue.message+" ("+std::to_string(issue.count)+")";
            };
            if(ImGui::Button("Export GeoJSON (F6)") || (!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F6))) {
                try { status(save_geojson(document,interchange_path,export_ids(),interchange_losses),"Exported"); }
                catch(const std::exception& e) { interchange_status=e.what(); }
            }
            if(ImGui::Button("Import GeoJSON (F7)") || (!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_F7))) {
                try { auto report=load_geojson(document,interchange_path,interchange_pixels,interchange_losses); selection->replace(report.imported_ids); selected=selection->primary(); status(report,"Imported"); }
                catch(const std::exception& e) { interchange_status=e.what(); }
            }
            ImGui::Checkbox("Include mean RGB",&csv_intensity);
            if(ImGui::Button("Export measurements CSV")) {
                try { save_measurements_csv(document,interchange_path,export_ids(),csv_intensity); interchange_status="Exported measurements"; }
                catch(const std::exception& e) { interchange_status=e.what(); }
            }
            if(!interchange_status.empty()) ImGui::TextWrapped("%s",interchange_status.c_str());
        }
        ImGui::Text("IMAGE"); ImGui::Text("%lld x %lld pixels",static_cast<long long>(image.width),static_cast<long long>(image.height));
        ImGui::Text("%zu pyramid levels",image.levels.size());
        ImGui::SeparatorText("Overview");
        overview_offset=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("overview navigation",overview_size);
        overview_draw=ImGui::GetWindowDrawList();
        overview_clip_min=overview_draw->GetClipRectMin(); overview_clip_max=overview_draw->GetClipRectMax();
        overview_view.fit(image.width,image.height,overview_size.x,overview_size.y);
        if (overview_plan.tiles.empty()) overview_plan=plan_view_tiles(image,overview_view,overview_size.x,overview_size.y,1,256,128);
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
            auto center=overview_view.screen_to_image({io.MousePos.x-overview_offset.x,io.MousePos.y-overview_offset.y});
            overview_center=Point{std::clamp(center.x,0.,static_cast<double>(image.width)),std::clamp(center.y,0.,static_cast<double>(image.height))};
        }
        if (overview_plan.truncated) ImGui::TextDisabled("Overview limited by source pyramid");
        ImGui::SeparatorText("Display");
        int mode=static_cast<int>(display.mode);
        if (ImGui::Combo("Channels",&mode,"RGB\0Grayscale\0Red\0Green\0Blue\0")) display.mode=static_cast<DisplayMode>(mode);
        float black=static_cast<float>(display.black),white=static_cast<float>(display.white),gamma=static_cast<float>(display.gamma);
        if (ImGui::SliderFloat("Black",&black,0,254,"%.0f")) display.black=std::min<double>(black,std::max(0.,display.white-.001));
        if (ImGui::SliderFloat("White",&white,1,255,"%.0f")) display.white=std::max<double>(white,std::min(255.,display.black+.001));
        if (ImGui::SliderFloat("Gamma",&gamma,.1f,5,"%.2f")) display.gamma=gamma;
        if (ImGui::Button("Reset display")) display=DisplaySettings{};
        display_reset_offset=ImGui::GetItemRectMin();
        auto display_reset_max=ImGui::GetItemRectMax(); display_reset_size={display_reset_max.x-display_reset_offset.x,display_reset_max.y-display_reset_offset.y};
        tiles->set_display(display);
        if (!document.pixel_width_um() || !document.pixel_height_um()) ImGui::TextDisabled("Physical calibration unknown");
        if (calibration_revision!=document.revision()) {
            draft_calibration[0]=document.pixel_width_um().value_or(0);
            draft_calibration[1]=document.pixel_height_um().value_or(0);
            calibration_revision=document.revision();
        }
        ImGui::SetNextItemWidth(120);
        ImGui::InputDouble("Pixel width (um)",&draft_calibration[0],0,0,"%.6g");
        ImGui::SetNextItemWidth(120);
        ImGui::InputDouble("Pixel height (um)",&draft_calibration[1],0,0,"%.6g");
        if (ImGui::Button("Apply calibration")) {
            try { document.set_calibration(draft_calibration[0], draft_calibration[1]); error.clear(); }
            catch (const std::exception& e) { error = e.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            try { document.clear_calibration(); error.clear(); } catch(const std::exception& e) { error=e.what(); }
        }
        ImGui::Spacing(); ImGui::SeparatorText("Objects");
        if (list_revision!=document.revision()) {
            list_ids.clear(); list_ids.reserve(document.object_count());
            for (const auto& [id,a]:document.objects()) list_ids.push_back(id);
            list_revision=document.revision();
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
            ImGui::SetNextItemWidth(180);
            if(ImGui::BeginCombo("Registry",class_key.empty()?"New class":class_key.c_str())) {
                if(ImGui::Selectable("New class",class_key.empty())) { class_key.clear(); class_name[0]=0; }
                for(const auto& [name,c]:document.classes()) if(ImGui::Selectable(name.c_str(),name==class_key)) {
                    class_key=name; std::memcpy(class_name,name.c_str(),name.size()+1);
                    class_color[0]=((c.color>>16)&255)/255.f; class_color[1]=((c.color>>8)&255)/255.f; class_color[2]=(c.color&255)/255.f;
                }
                ImGui::EndCombo();
            }
            ImGui::SetNextItemWidth(180); ImGui::InputText("Class name",class_name,sizeof(class_name));
            ImGui::SetNextItemWidth(180); ImGui::ColorEdit3("RGB",class_color);
            if(ImGui::Button("Save color")) {
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
                if(ImGui::Button("Rename")) {
                    try { document.rename_class(class_key,class_name); class_key=class_name; error.clear(); }
                    catch(const std::exception& e) { error=e.what(); }
                }
                ImGui::Text("Assigned: %zu",document.class_count(class_key));
                if(ImGui::Button("Remove unused class")) {
                    try { document.remove_class(class_key); class_key.clear(); class_name[0]=0; error.clear(); }
                    catch(const std::exception& e) { error=e.what(); }
                }
            }
        }
        ImGui::Text("Selected: %zu",selection->size());
        if(selected && ImGui::Button("Delete selected")) delete_selection();
        if(selected) { ImGui::SameLine(); if(ImGui::Button("Clear selection")) { selection->clear(); selected=0; } }
        if(selection->size()>1) {
            if(ImGui::BeginCombo("Batch class",batch_class.empty()?"Unclassified":batch_class.c_str())) {
                if(ImGui::Selectable("Unclassified",batch_class.empty())) batch_class.clear();
                for(const auto& [name,c]:document.classes()) if(ImGui::Selectable(name.c_str(),name==batch_class)) batch_class=name;
                ImGui::EndCombo();
            }
            if(ImGui::Button("Classify selected")) {
                try { document.classify_objects(selection->ids(),batch_class); error.clear(); }
                catch(const std::exception& e) { error=e.what(); }
            }
            if(has_geos()) {
                ImGui::SetNextItemWidth(140);
                ImGui::Combo("Boolean",&boolean_operation,"Union\0Intersection\0Difference\0XOR\0");
                ImGui::TextDisabled("Target: primary; operands retained");
                ImGui::TextWrapped("Ellipses use 512-edge approximations.");
                if(ImGui::Button("Apply Boolean (B)") || (!io.WantTextInput && !editing_blocked && ImGui::IsKeyPressed(ImGuiKey_B))) {
                    try {
                        auto ids=selection->ids(); ids.erase(std::remove(ids.begin(),ids.end(),selected),ids.end());
                        document.apply_boolean(selected,ids,static_cast<BooleanOp>(boolean_operation)); error.clear();
                    } catch(const std::exception& e) { error=e.what(); }
                }
            }
            ImGui::TextDisabled("Inspector edits the primary object");
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
                if(ImGui::Button("Convert to polygon")) {
                    try { document.update_roi(selected,object.geometry.as_polygon()); error.clear(); }
                    catch(const std::exception& e) { error=e.what(); }
                }
            }
            if(object.kind==ObjectKind::Cell) ImGui::TextDisabled(object.nucleus?"Nucleus ROI present":"No nucleus ROI");
            ImGui::InputText("Name",draft_name,sizeof(draft_name));
            if(ImGui::BeginCombo("Class",draft_class[0]?draft_class:"Unclassified")) {
                if(ImGui::Selectable("Unclassified",!draft_class[0])) draft_class[0]=0;
                for(const auto& [name,c]:document.classes()) if(ImGui::Selectable(name.c_str(),name==draft_class))
                    std::memcpy(draft_class,name.c_str(),name.size()+1);
                ImGui::EndCombo();
            }
            ImGui::BeginDisabled(object.is_detection());
            ImGui::InputDouble("X",&draft_roi.x); ImGui::InputDouble("Y",&draft_roi.y);
            ImGui::BeginDisabled(draft_geometry.kind()!=RoiKind::Rectangle && draft_geometry.kind()!=RoiKind::Ellipse);
            ImGui::InputDouble("Width",&draft_roi.width); ImGui::InputDouble("Height",&draft_roi.height); ImGui::EndDisabled(); ImGui::EndDisabled();
            ImGui::InputScalar("Parent ID",ImGuiDataType_U64,&draft_parent);
            if (ImGui::Button("Apply object edit")) {
                try {
                    auto bounds=draft_geometry.bounds();
                    auto geometry=draft_geometry.kind()==RoiKind::Rectangle?Roi::rectangle(draft_roi):
                        (draft_geometry.kind()==RoiKind::Ellipse?Roi::ellipse(draft_roi):draft_geometry.translated(draft_roi.x-bounds.x,draft_roi.y-bounds.y));
                    document.execute({ObjectEdit::replace_geometry(selected,geometry,draft_name,draft_parent,draft_class,object.kind,object.nucleus)},"Edit annotation"); error.clear();
                }
                catch(const std::exception& e) { error=e.what(); }
            }
        }
        ImGui::SeparatorText("Measurements");
        mean.poll(selected,document.revision());
        if (selected) {
            if (document.pixel_width_um() && document.pixel_height_um()) ImGui::Text("Area: %.3f um^2", document.area_um2(selected));
            else ImGui::TextDisabled("Area: set calibration first");
            if(document.annotation(selected).nucleus && document.pixel_width_um() && document.pixel_height_um())
                ImGui::Text("Nucleus: %.3f um^2",document.nucleus_area_um2(selected));
            if (mean.measured==selected && mean.value) ImGui::Text("Mean RGB: %.3f",*mean.value);
            ImGui::BeginDisabled(mean.active());
            if (ImGui::Button("Measure mean RGB")) mean.start(document.server(),document.annotation(selected),document.revision());
            ImGui::EndDisabled();
            if (mean.active()) ImGui::TextDisabled("Measuring...");
            if (!mean.error.empty()) ImGui::TextWrapped("%s",mean.error.c_str());
        } else ImGui::TextDisabled("Select an annotation");
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        } else ImGui::TextDisabled("Select or add an image to use image tools.");
        ImGui::EndChild(); ImGui::SameLine();
        ImGui::BeginChild("Canvas", {0,0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        auto offset = ImGui::GetCursorScreenPos(); auto size = ImGui::GetContentRegionAvail();
        size.x = std::max(size.x, 1.f); size.y = std::max(size.y, 1.f);
        ImGui::InvisibleButton("image canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        previous_canvas=offset; previous_canvas_size=size;
        if(has_image) {
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
        draw->AddRectFilled(offset, {offset.x+size.x,offset.y+size.y}, IM_COL32(18,21,27,255));
        auto dpi=std::max(io.DisplayFramebufferScale.x,io.DisplayFramebufferScale.y);
        auto inputs=std::make_tuple(viewport.origin.x,viewport.origin.y,viewport.zoom,size.x,size.y,dpi);
        if (!plan_inputs || *plan_inputs!=inputs) {
            plan=plan_view_tiles(image,viewport,size.x,size.y,dpi,256,128); plan_inputs=inputs;
        }
        auto combined=plan; combined.tiles.insert(combined.tiles.end(),overview_plan.tiles.begin(),overview_plan.tiles.end());
        tiles->update(combined);
        for (const auto& tile : plan.tiles) if (auto texture=tiles->texture(tile)) {
            draw->AddImage(static_cast<ImTextureID>(texture),screen(viewport,{tile.bounds.x,tile.bounds.y},offset),
                screen(viewport,{tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height},offset));
        }
        const auto visible_min=viewport.screen_to_image({0,0}),visible_max=viewport.screen_to_image({size.x,size.y});
        auto visible_ids=document.query_region({visible_min.x,visible_min.y,visible_max.x-visible_min.x,visible_max.y-visible_min.y});
        visible_count=visible_ids.size();
        for (auto id:visible_ids) {
            const auto& a=document.objects().at(id); const auto r=a.geometry.bounds();
            auto p0 = screen(viewport,{r.x,r.y},offset);
            auto color = selection->contains(a.id) ? IM_COL32(255,205,85,255) : IM_COL32(65,225,190,255);
            if(!selection->contains(a.id) && !a.class_name.empty()) {
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
        // Draw into the inspector window's own draw list, keeping the image and
        // viewport outline clipped to the overview, not the main canvas.
        overview_draw->PushClipRect({std::max(overview_offset.x,overview_clip_min.x),std::max(overview_offset.y,overview_clip_min.y)},
            {std::min(overview_offset.x+overview_size.x,overview_clip_max.x),std::min(overview_offset.y+overview_size.y,overview_clip_max.y)},false);
        overview_draw->AddRectFilled(overview_offset,{overview_offset.x+overview_size.x,overview_offset.y+overview_size.y},IM_COL32(18,21,27,255));
        for (const auto& tile:overview_plan.tiles) if (auto texture=tiles->texture(tile)) overview_draw->AddImage(static_cast<ImTextureID>(texture),
            screen(overview_view,{tile.bounds.x,tile.bounds.y},overview_offset),screen(overview_view,{tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height},overview_offset));
        auto top_left=viewport.screen_to_image({0,0}),bottom_right=viewport.screen_to_image({size.x,size.y});
        overview_draw->AddRect(screen(overview_view,top_left,overview_offset),screen(overview_view,bottom_right,overview_offset),IM_COL32(255,205,85,255),0,0,2);
        overview_draw->PopClipRect();
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
        } else {
            visible_count=0;
            ImGui::GetWindowDrawList()->AddText({offset.x+32,offset.y+32},IM_COL32(190,205,220,255),project_images.empty()?"No images in this project.\nAdd an image in Project images, or open another project.":"No image selected.\nSelect an image in Project images.");
        }
        ImGui::EndChild(); ImGui::EndDisabled();
        if(maintenance_popup_requested) {ImGui::OpenPopup("Delete recovery checkpoint?"); maintenance_popup_requested=false;}
        if(ImGui::BeginPopupModal("Delete recovery checkpoint?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            if(reviewed_checkpoint) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+520);
                ImGui::TextWrapped("Permanently delete this recovery checkpoint? This does not discard the live workspace or change the primary project.");
                ImGui::TextWrapped("%s",reviewed_checkpoint->path.c_str());
                ImGui::TextWrapped("%s / %zu images",recovery_label(*reviewed_checkpoint).c_str(),reviewed_checkpoint->images);
                ImGui::TextWrapped("Automatic recovery saving or another open window may create another checkpoint later.");
                ImGui::PopTextWrapPos();
                if(ImGui::Button("Permanently delete checkpoint")) {
                    auto reviewed=*reviewed_checkpoint; maintenance_epoch=workspace_epoch; maintenance_error.clear();
                    maintenance_job=std::async(std::launch::async,[reviewed] {
                        MaintenanceResult result;
                        try {Project::discard_recovery_checked(reviewed); result.deleted=reviewed.path;} catch(const std::exception& e) {result.error=e.what();}
                        return result;
                    });
                    reviewed_checkpoint.reset(); maintenance_dialog_active=false; ImGui::CloseCurrentPopup();
                }
                confirm_checkpoint_offset=ImGui::GetItemRectMin(); auto confirm_max=ImGui::GetItemRectMax(); confirm_checkpoint_size={confirm_max.x-confirm_checkpoint_offset.x,confirm_max.y-confirm_checkpoint_offset.y};
                ImGui::SameLine();
                if(ImGui::Button("Cancel")) {reviewed_checkpoint.reset(); maintenance_dialog_active=false; ImGui::CloseCurrentPopup();}
                cancel_checkpoint_offset=ImGui::GetItemRectMin(); auto cancel_max=ImGui::GetItemRectMax(); cancel_checkpoint_size={cancel_max.x-cancel_checkpoint_offset.x,cancel_max.y-cancel_checkpoint_offset.y};
            }
            ImGui::EndPopup();
        }
        if(remove_popup_requested) { ImGui::OpenPopup("Remove image?"); remove_popup_requested=false; }
        if(ImGui::BeginPopupModal("Remove image?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            const auto item=std::find_if(project_images.begin(),project_images.end(),[&](const auto& image){return image.id==active_image;});
            ImGui::TextWrapped("Remove %s from this project?",item==project_images.end()?"this image":item->name.c_str());
            ImGui::TextWrapped("This removes its %zu objects, view and editing history from the workspace. Undo cannot restore the entry. Save project to persist the removal. The source image file stays on disk.",document.object_count());
            if(ImGui::Button("Remove image")) { pending_remove=active_image; remove_dialog_active=false; ImGui::CloseCurrentPopup(); }
            confirm_remove_offset=ImGui::GetItemRectMin(); auto confirm_max=ImGui::GetItemRectMax(); confirm_remove_size={confirm_max.x-confirm_remove_offset.x,confirm_max.y-confirm_remove_offset.y};
            ImGui::SameLine();
            if(ImGui::Button("Cancel")) { remove_dialog_active=false; ImGui::CloseCurrentPopup(); }
            cancel_remove_offset=ImGui::GetItemRectMin(); auto cancel_max=ImGui::GetItemRectMax(); cancel_remove_size={cancel_max.x-cancel_remove_offset.x,cancel_max.y-cancel_remove_offset.y};
            ImGui::EndPopup();
        }
        if(recovery_popup_requested) { ImGui::OpenPopup("Replace workspace?"); recovery_popup_requested=false; }
        if(ImGui::BeginPopupModal("Replace workspace?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            if(close_recovery_popup) { ImGui::CloseCurrentPopup(); close_recovery_popup=false; recovery_dialog_active=false; }
            else {
                ImGui::TextWrapped(staged_is_recovery?"Replace this workspace with the selected recovery? Save or discard its current unsaved changes first.":"Replace this workspace with the opened project? Save or discard its current unsaved changes first.");
                if(staged_project) ImGui::TextWrapped("Open: %s",staged_is_recovery?staged_project->recovery_source().c_str():(staged_project->path().empty()?"New empty project":staged_project->path().c_str()));
                if(staged_project && staged_is_recovery && !staged_project->path().empty()) ImGui::TextWrapped("Saved project location: %s",staged_project->path().c_str());
                ImGui::SetNextItemWidth(400); ImGui::InputTextWithHint("##Save before recovery","Current project destination",project_path,sizeof(project_path));
                ImGui::BeginDisabled(restore_pending || restore_after_save || save_job.busy());
                if(ImGui::Button(staged_is_recovery?"Save current and recover":"Save current and open")) { save_project_requested=true; restore_after_save=true; }
                save_recovery_offset=ImGui::GetItemRectMin(); auto sr=ImGui::GetItemRectMax(); save_recovery_size={sr.x-save_recovery_offset.x,sr.y-save_recovery_offset.y};
                if(ImGui::Button(staged_is_recovery?"Discard current and recover":"Discard current and open")) { autosave.cancel(); restore_pending=true; }
                discard_recovery_offset=ImGui::GetItemRectMin(); auto dr=ImGui::GetItemRectMax(); discard_recovery_size={dr.x-discard_recovery_offset.x,dr.y-discard_recovery_offset.y};
                if(ImGui::Button("Cancel")) {
                    staged_project.reset();
                    recovery_dialog_active=false; ImGui::CloseCurrentPopup();
                }
                cancel_recovery_offset=ImGui::GetItemRectMin(); auto cr=ImGui::GetItemRectMax(); cancel_recovery_size={cr.x-cancel_recovery_offset.x,cr.y-cancel_recovery_offset.y};
                ImGui::EndDisabled();
                if(restore_pending) ImGui::TextDisabled("Preparing workspace...");
                if(!project_error.empty()) ImGui::TextWrapped("%s",project_error.c_str());
            }
            ImGui::EndPopup();
        }
        if(close_popup_requested) { ImGui::OpenPopup("Unsaved project"); close_popup_requested=false; }
        if(ImGui::BeginPopupModal("Unsaved project",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Save this project, keep a recovery copy, or discard its unsaved changes before closing.");
            ImGui::SetNextItemWidth(400); ImGui::InputTextWithHint("##Close save path","Project destination",project_path,sizeof(project_path));
            ImGui::SetNextItemWidth(400); ImGui::InputTextWithHint("##Close recovery path","Automatic recovery destination",autosave_path,sizeof(autosave_path));
            ImGui::BeginDisabled(close_action!=0);
            if(ImGui::Button("Save and close")) { save_project_requested=true; close_action=1; close_wait_only=false; }
            save_close_offset=ImGui::GetItemRectMin(); auto sc=ImGui::GetItemRectMax(); save_close_size={sc.x-save_close_offset.x,sc.y-save_close_offset.y};
            if(ImGui::Button("Keep recovery and close")) { close_action=2; close_keep_started=false; }
            keep_close_offset=ImGui::GetItemRectMin(); auto kc=ImGui::GetItemRectMax(); keep_close_size={kc.x-keep_close_offset.x,kc.y-keep_close_offset.y};
            if(ImGui::Button("Discard and close")) { autosave.clear(); close_action=3; }
            discard_close_offset=ImGui::GetItemRectMin(); auto dc=ImGui::GetItemRectMax(); discard_close_size={dc.x-discard_close_offset.x,dc.y-discard_close_offset.y};
            if(ImGui::Button("Cancel")) { close_dialog_active=false; ImGui::CloseCurrentPopup(); }
            cancel_close_offset=ImGui::GetItemRectMin(); auto cc=ImGui::GetItemRectMax(); cancel_close_size={cc.x-cancel_close_offset.x,cc.y-cancel_close_offset.y};
            ImGui::EndDisabled();
            if(close_action) ImGui::TextDisabled("Finishing save...");
            if(!project_error.empty()) ImGui::TextWrapped("%s",project_error.c_str());
            if(!autosave_ui_error.empty()) ImGui::TextWrapped("%s",autosave_ui_error.c_str());
            ImGui::EndPopup();
        }
        ImGui::End();
        if(options.project) {
            bool recovery_operation=false;
            try {
                auto inputs=view_inputs(size.x,size.y);
                if(active_image && (!published_view_inputs || *published_view_inputs!=inputs)) {
                    options.project->set_view(active_image,{viewport.screen_to_image({size.x/2,size.y/2}),viewport.zoom,display});
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
        ImGui::Render(); int w,h; glfwGetFramebufferSize(session.window,&w,&h); last_width=w; last_height=h; glViewport(0,0,w,h);
        glClearColor(.07f,.08f,.1f,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        auto cpu_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_start).count();
        if (frames>=10) {
            if (frame_times.size()==4096) frame_times.erase(frame_times.begin(),frame_times.begin()+2048);
            frame_times.push_back(cpu_ms);
        }
        if (!capture_path.empty() && frames+1==frame_limit) capture(capture_path,w,h);
        glfwSwapBuffers(session.window); ++frames;
    }
    // Finite embedded runs also return a durable save result and reconcile the
    // caller's project. No live authority or Python-owned objects enter workers.
    if(save_job.busy()) save_job.wait();
    reconcile_save();
    if(maintenance_job.valid()) {maintenance_job.wait(); poll_maintenance(true);}
    if(retired_workspace.valid()) max_retirement_ms=std::max(max_retirement_ms,retired_workspace.get());
    std::sort(frame_times.begin(),frame_times.end());
    auto p95=frame_times.empty()?0:frame_times[static_cast<std::size_t>((frame_times.size()-1)*.95)];
    const bool empty=options.project && !active_image;
    return {frames,previous_uploads+tiles->uploads(),std::max(previous_peak_bytes,tiles->peak_bytes()),previous_generations+tiles->generations(),p95,tiles->pending(),empty?0:viewport.zoom,empty?Point{}:viewport.origin,
        empty?Point{}:viewport.screen_to_image({previous_canvas_size.x/2,previous_canvas_size.y/2}),last_width,last_height,
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)),reinterpret_cast<const char*>(glGetString(GL_VERSION)),visible_count,selection->ids(),selection->primary(),active_image,image_switches,max_image_switch_ms,project_error,
        autosave.stats().completed,autosave.stats().path,autosave.stats().error.empty()?autosave_ui_error:autosave.stats().error,autosave.stats().capture_ms,frame_times.empty()?0:frame_times.back(),project_replacements,project_load_ms,images_removed,
        save_job.stats().completed,save_job.stats().capture_ms,save_job.stats().write_ms,recovery_load_ms,checkpoints_deleted,maintenance_error,retired_workspaces,max_retirement_ms,
        autosave.stats().checkpoints_pruned,autosave.stats().retention_ms,autosave.stats().retention_error};
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
}
