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

namespace qpx {
namespace {
ImVec2 screen(const Viewport& v, Point p, ImVec2 offset) {
    auto s = v.image_to_screen(p); return {offset.x + static_cast<float>(s.x), offset.y + static_cast<float>(s.y)};
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
public:
    std::uint64_t measured=0;
    std::optional<double> value;
    std::string error;
    ~MeanJob() { if (cancelled_) cancelled_->store(true); }
    bool active() const { return future_.valid(); }
    void poll(std::uint64_t selection) {
        if (ticket_!=selection && cancelled_) cancelled_->store(true);
        if (active() && future_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try { auto result=future_.get(); if (ticket_==selection) { measured=ticket_; value=result; error.clear(); } }
            catch(const std::exception& e) { if (ticket_==selection) error=e.what(); }
        }
    }
    void start(std::shared_ptr<ImageServer> server, const Annotation& a) {
        ticket_=a.id; value.reset(); error.clear(); cancelled_=std::make_shared<std::atomic<bool>>(false);
        future_=std::async(std::launch::async,[server=std::move(server),roi=a.roi,cancelled=cancelled_] { return mean_rgb(*server,roi,cancelled.get()); });
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
    bool fit = true, drawing = false, rectangle_tool = false;
    Point anchor;
    std::uint64_t selected = 0;
    std::string error;
    int frames = 0;
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
        if (!options.inputs.empty()) input.AddKeyEvent(ImGuiKey_R,false);
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
                default: break;
            }
        }
        ImGui::NewFrame();
        const auto& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({.3f,.8f,.75f,1}, "qupathxx"); ImGui::SameLine(); ImGui::TextDisabled("/  Research workspace");
        ImGui::Separator();
        if (ImGui::IsKeyPressed(ImGuiKey_R) && !io.WantTextInput) rectangle_tool=!rectangle_tool;
        ImGui::Checkbox("Rectangle tool", &rectangle_tool); ImGui::SameLine();
        if (ImGui::Button("Fit image")) fit = true;
        ImGui::SameLine(); ImGui::TextDisabled("Wheel: zoom  |  Right drag: pan  |  Left drag: annotate");
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
        float calibration[2] = {static_cast<float>(document.pixel_width_um().value_or(0)), static_cast<float>(document.pixel_height_um().value_or(0))};
        if (ImGui::InputFloat2("um / pixel", calibration)) {
            try { document.set_calibration(calibration[0], calibration[1]); error.clear(); }
            catch (const std::exception& e) { error = e.what(); }
        }
        ImGui::Spacing(); ImGui::SeparatorText("Annotations");
        for (const auto& a : document.annotations()) {
            auto label = a.name + "##" + std::to_string(a.id);
            if (ImGui::Selectable(label.c_str(), selected == a.id)) selected = a.id;
        }
        if (selected && ImGui::Button("Delete selected")) { document.remove_annotation(selected); selected = 0; }
        ImGui::SeparatorText("Measurements");
        mean.poll(selected);
        if (selected) {
            if (document.pixel_width_um() && document.pixel_height_um()) ImGui::Text("Area: %.3f um^2", document.area_um2(selected));
            else ImGui::TextDisabled("Area: set calibration first");
            if (mean.measured==selected && mean.value) ImGui::Text("Mean RGB: %.3f",*mean.value);
            ImGui::BeginDisabled(mean.active());
            if (ImGui::Button("Measure mean RGB")) for (const auto& a : document.annotations()) if (a.id==selected) mean.start(document.server(),a);
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
            if (rectangle_tool && p.x >= 0 && p.y >= 0 && p.x < image.width && p.y < image.height) { anchor = p; drawing = true; }
            else if (!rectangle_tool) {
                selected = 0;
                for (const auto& a : document.annotations()) if (a.roi.contains(p)) selected = a.id;
            }
        }
        auto end = clamp_image(viewport.screen_to_image(mouse));
        if (drawing && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            Rect r{std::min(anchor.x,end.x), std::min(anchor.y,end.y), std::abs(anchor.x-end.x), std::abs(anchor.y-end.y)};
            if (r.width * viewport.zoom >= 2 && r.height * viewport.zoom >= 2) {
                try { selected = document.add_rectangle(r, "Region " + std::to_string(document.annotations().size()+1)); error.clear(); }
                catch (const std::exception& e) { error = e.what(); }
            }
            drawing = false;
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
        for (const auto& a : document.annotations()) {
            auto p0 = screen(viewport,{a.roi.x,a.roi.y},offset), p1 = screen(viewport,{a.roi.x+a.roi.width,a.roi.y+a.roi.height},offset);
            auto color = a.id == selected ? IM_COL32(255,205,85,255) : IM_COL32(65,225,190,255);
            draw->AddRect(p0,p1,color,0,0,2); draw->AddText(p0,color,a.name.c_str());
        }
        if (drawing) draw->AddRect(screen(viewport,anchor,offset),screen(viewport,end,offset),IM_COL32(255,205,85,255),0,0,2);
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
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)),reinterpret_cast<const char*>(glGetString(GL_VERSION))};
}
}
