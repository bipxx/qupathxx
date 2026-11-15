#pragma once
#include <array>
#include "gui_commands.hpp"
#include "gui_i18n.hpp"
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
    Channels, Objects, ResetLayout, Settings, Count };
inline constexpr std::size_t shell_command_count=static_cast<std::size_t>(ShellCommand::Count);
inline constexpr std::array shell_commands={CommandId::Shell_Project,CommandId::Shell_Image,CommandId::Shell_Annotations,CommandId::Shell_Workflow,CommandId::Shell_Select,CommandId::Shell_Rectangle,CommandId::Shell_Ellipse,CommandId::Shell_Polygon,CommandId::Shell_Polyline,CommandId::Shell_Points,CommandId::Shell_Vertices,CommandId::Shell_Hole,CommandId::Shell_Fit,CommandId::Shell_Undo,CommandId::Shell_Redo,CommandId::Shell_Spatial,CommandId::Shell_Channels,CommandId::Shell_Objects,CommandId::Shell_ResetLayout,CommandId::Shell_Settings};
static_assert(shell_commands.size()==shell_command_count);
inline constexpr CommandId command_id(ShellCommand id) {return shell_commands.at(static_cast<std::size_t>(id));}
inline constexpr auto shell_names=[] {std::array<const char*,shell_command_count> values{};for(std::size_t i=0;i<values.size();++i)values[i]=command(shell_commands[i]).caption;return values;}();
inline const auto shell_chords=[] {std::array<const char*,shell_command_count> values{};for(std::size_t i=0;i<values.size();++i)values[i]=command_chord(shell_commands[i]);return values;}();
inline const std::array<std::string,shell_command_count>& shell_labels() {
    i18n::SurfaceTrace translation_surface("shell");
    static std::array<std::string,shell_command_count> labels;
    static int language=-1;
    if(language!=i18n::state.effective) {
        for(std::size_t i=0;i<labels.size();++i) {
            labels[i]=i18n::text(command(shell_commands[i]).caption);
            const auto* chord=command_chord(shell_commands[i]);
            if(*chord)labels[i]+=" ("+std::string(chord)+")";
        }
        language=i18n::state.effective;
    }
    return labels;
}
struct ShellSnapshot {
    int pane=0,tool=0;
    bool has_image=false,can_undo=false,can_redo=false,raw_channels=false,blocked=false;
    bool objects_visible=true,gesture_active=false;
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
    void set_toolbar_visible(bool visible);
    ShellRect bounds(ShellCommand) const;
    bool contains(float x,float y) const;
    bool keyboard_active() const;
    bool reload(); // transactional; retain last valid document on failure
    const std::string& error() const;
    unsigned generation() const;
    void render(int framebuffer_width,int framebuffer_height);
};
}
