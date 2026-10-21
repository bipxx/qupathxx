#include "qupathxx/core.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qpx {
namespace {
ImVec2 screen(const Viewport& v, Point p, ImVec2 offset) {
    auto s = v.image_to_screen(p); return {offset.x + static_cast<float>(s.x), offset.y + static_cast<float>(s.y)};
}
struct Session {
    GLFWwindow* window = nullptr;
    GLuint texture = 0;
    bool context = false, platform = false, renderer = false;
    ~Session() {
        if (texture) glDeleteTextures(1, &texture);
        if (renderer) ImGui_ImplOpenGL3_Shutdown();
        if (platform) ImGui_ImplGlfw_Shutdown();
        if (context) ImGui::DestroyContext();
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }
};
}
void run_gui(Document& document, int frame_limit) {
    if (frame_limit < 0) throw std::invalid_argument("frame_limit must be nonnegative");
    if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
    Session session;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    session.window = glfwCreateWindow(1440, 900, "qupathxx | Bioimage analysis", nullptr, nullptr);
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
    const auto& image = document.image();
    GLint max_texture = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    if (image.width() > max_texture || image.height() > max_texture)
        throw std::runtime_error("image exceeds GPU texture limit; tiled whole-slide support is not implemented");
    glGenTextures(1, &session.texture); glBindTexture(GL_TEXTURE_2D, session.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.width(), image.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, image.pixels().data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("image texture upload failed");
    Viewport viewport;
    bool fit = true, drawing = false, rectangle_tool = false;
    Point anchor;
    std::uint64_t selected = 0;
    std::string error;
    int frames = 0;
    while (!glfwWindowShouldClose(session.window) && (!frame_limit || frames < frame_limit)) {
        glfwPollEvents(); ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        const auto& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored({.3f,.8f,.75f,1}, "qupathxx"); ImGui::SameLine(); ImGui::TextDisabled("/  Research workspace");
        ImGui::Separator();
        ImGui::Checkbox("Rectangle tool", &rectangle_tool); ImGui::SameLine();
        if (ImGui::Button("Fit image")) fit = true;
        ImGui::SameLine(); ImGui::TextDisabled("Wheel: zoom  |  Right drag: pan  |  Left drag: annotate");
        ImGui::BeginChild("Inspector", {285,0}, ImGuiChildFlags_Borders);
        ImGui::Text("IMAGE"); ImGui::Text("%d x %d pixels / RGB", image.width(), image.height());
        ImGui::TextDisabled("In-memory image; no WSI reader yet");
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
        if (selected) {
            if (document.pixel_width_um() && document.pixel_height_um()) ImGui::Text("Area: %.3f um^2", document.area_um2(selected));
            else ImGui::TextDisabled("Area: set calibration first");
            try { ImGui::Text("Mean RGB: %.3f", document.mean_intensity(selected)); }
            catch (const std::invalid_argument&) { ImGui::TextDisabled("ROI contains no pixel centers"); }
        } else ImGui::TextDisabled("Select an annotation");
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        ImGui::EndChild(); ImGui::SameLine();
        ImGui::BeginChild("Canvas", {0,0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        auto offset = ImGui::GetCursorScreenPos(); auto size = ImGui::GetContentRegionAvail();
        size.x = std::max(size.x, 1.f); size.y = std::max(size.y, 1.f);
        ImGui::InvisibleButton("image canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if (fit) { viewport.fit(image.width(), image.height(), size.x, size.y); fit = false; }
        Point mouse{io.MousePos.x - offset.x, io.MousePos.y - offset.y};
        bool hovered = ImGui::IsItemHovered();
        if (hovered && io.MouseWheel != 0) viewport.zoom_at(mouse, std::pow(1.2, io.MouseWheel));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            viewport.origin.x += io.MouseDelta.x; viewport.origin.y += io.MouseDelta.y;
        }
        auto clamp_image = [&](Point p) { return Point{std::clamp(p.x, 0., static_cast<double>(image.width())), std::clamp(p.y, 0., static_cast<double>(image.height()))}; };
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            auto p = viewport.screen_to_image(mouse);
            if (rectangle_tool && p.x >= 0 && p.y >= 0 && p.x < image.width() && p.y < image.height()) { anchor = p; drawing = true; }
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
        draw->AddImage(static_cast<ImTextureID>(session.texture), screen(viewport,{0,0},offset), screen(viewport,{static_cast<double>(image.width()),static_cast<double>(image.height())},offset));
        for (const auto& a : document.annotations()) {
            auto p0 = screen(viewport,{a.roi.x,a.roi.y},offset), p1 = screen(viewport,{a.roi.x+a.roi.width,a.roi.y+a.roi.height},offset);
            auto color = a.id == selected ? IM_COL32(255,205,85,255) : IM_COL32(65,225,190,255);
            draw->AddRect(p0,p1,color,0,0,2); draw->AddText(p0,color,a.name.c_str());
        }
        if (drawing) draw->AddRect(screen(viewport,anchor,offset),screen(viewport,end,offset),IM_COL32(255,205,85,255),0,0,2);
        draw->PopClipRect();
        ImGui::EndChild(); ImGui::End();
        ImGui::Render(); int w,h; glfwGetFramebufferSize(session.window,&w,&h); glViewport(0,0,w,h);
        glClearColor(.07f,.08f,.1f,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glfwSwapBuffers(session.window); ++frames;
    }
}
}
