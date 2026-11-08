#pragma once
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
struct GLFWwindow;
namespace qpx::ui {
// Presentation contract only. Neither renderer receives a Document, Project,
// image server, job or mutable workflow pointer.
enum class ShellCommand { Project, Image, Annotations, Workflow, Select, Rectangle,
    Ellipse, Polygon, Polyline, Points, Vertices, Hole, Fit, Undo, Redo, Spatial,
    Channels, ResetLayout, Count };
inline constexpr std::size_t shell_command_count=static_cast<std::size_t>(ShellCommand::Count);
struct ShellSnapshot {
    int pane=0,tool=0;
    bool has_image=false,can_undo=false,can_redo=false,raw_channels=false,blocked=false;
    bool enabled(ShellCommand command) const;
};
struct ShellRect {float x=0,y=0,width=0,height=0;};
struct ShellInput {
    float x=0,y=0,wheel=0;
    bool down=false,up=false,tab=false,enter=false,space=false,shift=false,ctrl=false;
};
class RmlShell {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit RmlShell(GLFWwindow*,std::filesystem::path resources={});
    ~RmlShell();
    RmlShell(const RmlShell&)=delete;
    RmlShell& operator=(const RmlShell&)=delete;
    void update(int width,int height,float scale,bool light,const ShellSnapshot&,
        const std::array<std::string,shell_command_count>& labels,float framebuffer_scale=1);
    void input(const ShellInput&);
    std::vector<ShellCommand> take_commands();
    float rail_width() const;
    void place_rail(float x,float y,float height);
    // RCSS owns wrapping, spacing and height; the native image starts below it.
    float place_toolbar(float x,float y,float width);
    ShellRect bounds(ShellCommand) const;
    bool contains(float x,float y) const;
    bool keyboard_active() const;
    bool reload(); // transactional; retain last valid document on failure
    const std::string& error() const;
    unsigned generation() const;
    void render(int framebuffer_width,int framebuffer_height);
};
}
