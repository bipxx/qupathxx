#include "qupathxx/core.hpp"
#include "gui_tiles.hpp"
#include <imgui.h>
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

namespace qpx {
namespace {
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
    if(options.start_view && (!std::isfinite(options.start_view->zoom) || options.start_view->zoom<=0 ||
        !std::isfinite(options.start_view->origin.x) || !std::isfinite(options.start_view->origin.y))) throw std::invalid_argument("invalid initial viewport");
    if (options.width<640 || options.width>8192 || options.height<480 || options.height>8192) throw std::invalid_argument("invalid window dimensions");
    for (const auto& event:options.inputs) if (event.frame<2 || event.frame>=frame_limit ||
        !std::isfinite(event.x) || !std::isfinite(event.y) || event.x<0 || event.x>1 || event.y<0 || event.y>1 ||
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
    const auto& image = document.metadata();
    GLint max_texture = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    if (max_texture<256) throw std::runtime_error("GPU does not support 256-pixel tile textures");
    GuiTiles tiles(document.server());
    MeanJob mean;
    Viewport viewport;
    if(options.start_view) viewport=*options.start_view;
    bool fit = true, drawing = false;
    if(options.start_view) fit=false;
    int tool=0,previous_tool=0;
    Ring draft_points;
    Point anchor;
    std::uint64_t selected = 0;
    std::string error;
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
    ImVec2 previous_canvas{}, previous_canvas_size{}, overview_offset{}, overview_size{250,150};
    std::optional<Point> overview_center;
    int last_width=0,last_height=0;
    std::optional<std::tuple<double,double,double,float,float,float>> plan_inputs;
    while (!glfwWindowShouldClose(session.window) && (!frame_limit || frames < frame_limit)) {
        auto frame_start=std::chrono::steady_clock::now();
        glfwPollEvents(); ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame();
        // Test/benchmark replay enters the same ImGui event queue as GLFW input.
        auto& input=ImGui::GetIO();
        if (!options.inputs.empty()) {
            input.AddKeyEvent(ImGuiKey_R,false); input.AddKeyEvent(ImGuiKey_Z,false); input.AddKeyEvent(ImGuiKey_Y,false);
            input.AddKeyEvent(ImGuiMod_Ctrl,false); input.AddKeyEvent(ImGuiKey_Delete,false);
            for(auto key:{ImGuiKey_E,ImGuiKey_P,ImGuiKey_L,ImGuiKey_T,ImGuiKey_Enter}) input.AddKeyEvent(key,false);
        }
        for (const auto& event:options.inputs) if (event.frame==frames) {
            auto origin=event.target==GuiInputTarget::Canvas?previous_canvas:overview_offset;
            auto extent=event.target==GuiInputTarget::Canvas?previous_canvas_size:overview_size;
            input.AddMousePosEvent(origin.x+static_cast<float>(event.x)*extent.x,origin.y+static_cast<float>(event.y)*extent.y);
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
                default: break;
            }
        }
        ImGui::NewFrame();
        const auto& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({.3f,.8f,.75f,1}, "qupathxx"); ImGui::SameLine(); ImGui::TextDisabled("/  Research workspace");
        ImGui::Separator();
        if (ImGui::IsKeyPressed(ImGuiKey_R) && !io.WantTextInput) tool=tool==1?0:1;
        if(!io.WantTextInput) {
            if(ImGui::IsKeyPressed(ImGuiKey_E)) tool=2;
            if(ImGui::IsKeyPressed(ImGuiKey_P)) tool=3;
            if(ImGui::IsKeyPressed(ImGuiKey_L)) tool=4;
            if(ImGui::IsKeyPressed(ImGuiKey_T)) tool=5;
        }
        ImGui::SetNextItemWidth(125);
        ImGui::Combo("Tool",&tool,"Select\0Rectangle\0Ellipse\0Polygon\0Polyline\0Points\0"); ImGui::SameLine();
        if(tool!=previous_tool) { drawing=false; draft_points.clear(); previous_tool=tool; }
        if (ImGui::Button("Fit image")) fit = true;
        auto history_action=[&](bool redo) { try { if(redo) document.redo(); else document.undo(); error.clear(); } catch(const std::exception& e) { error=e.what(); } };
        if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) history_action(false);
        if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) history_action(true);
        ImGui::SameLine(); ImGui::BeginDisabled(!document.can_undo()); if (ImGui::Button("Undo")) history_action(false); ImGui::EndDisabled();
        ImGui::SameLine(); ImGui::BeginDisabled(!document.can_redo()); if (ImGui::Button("Redo")) history_action(true); ImGui::EndDisabled();
        if (selected && !document.objects().count(selected)) selected=0;
        if (selected && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            try { document.remove_annotation(selected); selected=0; error.clear(); } catch(const std::exception& e) { error=e.what(); }
        }
        ImGui::SameLine(); ImGui::TextDisabled("Wheel: zoom | Right drag: pan | R/E: drag | P/L/T: click, Enter: finish");
        ImGui::BeginChild("Inspector", {285,0}, ImGuiChildFlags_Borders);
        ImGui::Text("IMAGE"); ImGui::Text("%lld x %lld pixels",static_cast<long long>(image.width),static_cast<long long>(image.height));
        ImGui::Text("%zu pyramid levels",image.levels.size());
        ImGui::SeparatorText("Overview");
        overview_offset=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("overview navigation",overview_size);
        auto* overview_draw=ImGui::GetWindowDrawList();
        auto overview_clip_min=overview_draw->GetClipRectMin(),overview_clip_max=overview_draw->GetClipRectMax();
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
        tiles.set_display(display);
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
        ImGuiListClipper clipper; clipper.Begin(static_cast<int>(list_ids.size()));
        while(clipper.Step()) for (int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
            const auto& a=document.objects().at(list_ids[row]);
            auto label = a.name + "##" + std::to_string(a.id);
            if (ImGui::Selectable(label.c_str(), selected == a.id)) selected = a.id;
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
        if (selected && ImGui::Button("Delete selected")) {
            try { document.remove_annotation(selected); selected=0; error.clear(); }
            catch(const std::exception& e) { error=e.what(); }
        }
        if (selected) {
            if (editor_id!=selected || editor_revision!=document.revision()) {
                auto a=document.annotation(selected); draft_geometry=a.geometry; draft_roi=a.geometry.bounds(); draft_parent=a.parent_id;
                std::memcpy(draft_name,a.name.c_str(),a.name.size()+1); std::memcpy(draft_class,a.class_name.c_str(),a.class_name.size()+1);
                editor_id=selected; editor_revision=document.revision();
            }
            const auto object=document.annotation(selected);
            ImGui::Text("Kind: %s",object.kind==ObjectKind::Cell?"Cell":(object.kind==ObjectKind::Detection?"Detection":"Annotation"));
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
        ImGui::EndChild(); ImGui::SameLine();
        ImGui::BeginChild("Canvas", {0,0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        auto offset = ImGui::GetCursorScreenPos(); auto size = ImGui::GetContentRegionAvail();
        size.x = std::max(size.x, 1.f); size.y = std::max(size.y, 1.f);
        ImGui::InvisibleButton("image canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        previous_canvas=offset; previous_canvas_size=size;
        if (fit) { viewport.fit(image.width, image.height, size.x, size.y); fit = false; }
        if (overview_center) { viewport.center_on(*overview_center,size.x,size.y); overview_center.reset(); }
        Point mouse{io.MousePos.x - offset.x, io.MousePos.y - offset.y};
        bool hovered = ImGui::IsItemHovered();
        if (hovered && io.MouseWheel != 0) viewport.zoom_at(mouse, std::pow(1.2, io.MouseWheel));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            viewport.origin.x += io.MouseDelta.x; viewport.origin.y += io.MouseDelta.y;
        }
        auto clamp_image = [&](Point p) { return Point{std::clamp(p.x, 0., static_cast<double>(image.width)), std::clamp(p.y, 0., static_cast<double>(image.height))}; };
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            auto p = viewport.screen_to_image(mouse);
            if ((tool==1 || tool==2) && p.x >= 0 && p.y >= 0 && p.x < image.width && p.y < image.height) { anchor = p; drawing = true; }
            else if(tool>=3 && p.x>=0 && p.y>=0 && p.x<image.width && p.y<image.height) {
                if(draft_points.empty() || draft_points.back().x!=p.x || draft_points.back().y!=p.y) draft_points.push_back(p);
            }
            else if (!tool) {
                auto hits=document.pick(p,4/viewport.zoom); selected=hits.empty()?0:hits.back();
            }
        }
        auto end = clamp_image(viewport.screen_to_image(mouse));
        if (drawing && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            Rect r{std::min(anchor.x,end.x), std::min(anchor.y,end.y), std::abs(anchor.x-end.x), std::abs(anchor.y-end.y)};
            if (r.width * viewport.zoom >= 2 && r.height * viewport.zoom >= 2) {
                try {
                    auto name="Region "+std::to_string(document.object_count()+1);
                    selected=tool==2?document.add_roi(Roi::ellipse(r),name):document.add_rectangle(r,name); error.clear();
                }
                catch (const std::exception& e) { error = e.what(); }
            }
            drawing = false;
        }
        if(!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape)) { drawing=false; draft_points.clear(); }
        if(tool>=3 && !draft_points.empty() && !io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Enter) || (hovered && ImGui::IsMouseDoubleClicked(0)))) {
            try {
                auto geometry=tool==3?Roi::polygon(draft_points):(tool==4?Roi::polyline(draft_points):Roi::points(draft_points));
                selected=document.add_roi(geometry,"Region "+std::to_string(document.object_count()+1)); draft_points.clear(); error.clear();
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
        tiles.update(combined);
        for (const auto& tile : plan.tiles) if (auto texture=tiles.texture(tile)) {
            draw->AddImage(static_cast<ImTextureID>(texture),screen(viewport,{tile.bounds.x,tile.bounds.y},offset),
                screen(viewport,{tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height},offset));
        }
        const auto visible_min=viewport.screen_to_image({0,0}),visible_max=viewport.screen_to_image({size.x,size.y});
        auto visible_ids=document.query_region({visible_min.x,visible_min.y,visible_max.x-visible_min.x,visible_max.y-visible_min.y});
        visible_count=visible_ids.size();
        for (auto id:visible_ids) {
            const auto& a=document.objects().at(id); const auto r=a.geometry.bounds();
            auto p0 = screen(viewport,{r.x,r.y},offset);
            auto color = a.id == selected ? IM_COL32(255,205,85,255) : IM_COL32(65,225,190,255);
            if(a.id!=selected && !a.class_name.empty()) {
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
        for (const auto& tile:overview_plan.tiles) if (auto texture=tiles.texture(tile)) overview_draw->AddImage(static_cast<ImTextureID>(texture),
            screen(overview_view,{tile.bounds.x,tile.bounds.y},overview_offset),screen(overview_view,{tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height},overview_offset));
        auto top_left=viewport.screen_to_image({0,0}),bottom_right=viewport.screen_to_image({size.x,size.y});
        overview_draw->AddRect(screen(overview_view,top_left,overview_offset),screen(overview_view,bottom_right,overview_offset),IM_COL32(255,205,85,255),0,0,2);
        overview_draw->PopClipRect();
        std::string status=tiles.error().empty() ? (tiles.pending() ? "Loading image..." : "") : tiles.error();
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
        ImGui::EndChild(); ImGui::End();
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
    std::sort(frame_times.begin(),frame_times.end());
    auto p95=frame_times.empty()?0:frame_times[static_cast<std::size_t>((frame_times.size()-1)*.95)];
    return {frames,tiles.uploads(),tiles.peak_bytes(),tiles.generations(),p95,tiles.pending(),viewport.zoom,viewport.origin,
        viewport.screen_to_image({previous_canvas_size.x/2,previous_canvas_size.y/2}),last_width,last_height,
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)),reinterpret_cast<const char*>(glGetString(GL_VERSION)),visible_count};
}
}
