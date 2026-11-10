#include "../gui_shell.hpp"
#include "../gui_fonts.hpp"
#include "../gui_i18n.hpp"
#include "../gui_design_system.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx::ui;
static void check(bool ok,const std::string& reason) {if(!ok) throw std::runtime_error(reason);}
static std::string read(const std::filesystem::path& p) {std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const std::filesystem::path& p,const std::string& s) {std::ofstream f(p,std::ios::binary);f<<s;check(bool(f),"fixture write");}
static void click(RmlShell& shell,ShellCommand c) {
    const auto b=shell.bounds(c);check(b.width>0 && b.height>0,"empty button bounds");
    shell.input({b.x+b.width/2,b.y+b.height/2,0,true,false});
    shell.input({b.x+b.width/2,b.y+b.height/2,0,false,true});
}
static void shell_contract(const std::filesystem::path& directory) {
    check(glfwInit(),"GLFW initialization");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1100,800,"RmlUi contract",nullptr,nullptr);check(window,"GLFW window");glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().DisplaySize={1100,800};
    ImGui::GetIO().Fonts->AddFontDefault();unsigned char* pixels=nullptr;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ImGui::NewFrame();
    const auto source=qpx::detail::gui_font_directory().parent_path()/"ui";
    const auto resources=directory/"editable-ui";std::filesystem::create_directories(resources);
    const auto rml=read(source/"workspace.rml"),css=read(source/"workspace.rcss");
    write(resources/"workspace.rml",rml);write(resources/"workspace.rcss",css);
    {
        RmlShell shell(window,resources);ShellSnapshot state{0,0,true,true,true,true,false};
        std::array<std::string,shell_command_count> labels;labels.fill("Command");
        for(float framebuffer:{1.f,2.f}) for(float scale:{1.f,2.f}) for(float toolbar_width:{700.f,220.f}) {
            shell.update(1100,800,scale,false,state,labels,framebuffer);shell.place_rail(0,24*scale,700);
            const auto height=shell.place_toolbar(400,24*scale,toolbar_width);
            check(std::abs(shell.rail_width()-48*scale)<1,"RML rail DPI geometry");
            const auto primary=shell.bounds(ShellCommand::Project),utility=shell.bounds(ShellCommand::Settings);
            check(std::abs(primary.width-48*scale)<1 && std::abs(primary.height-48*scale)<1,"full-row navigation target");
            check(std::abs(utility.y+utility.height-(24*scale+700))<1,"settings stays at activity footer");
            check(height>=28*scale && height<(toolbar_width<700?300:140)*scale,"toolbar wrapping/height");
            for(int i=0;i<static_cast<int>(ShellCommand::Count);++i) {
                const auto c=static_cast<ShellCommand>(i);
                if(c>=ShellCommand::Select && c<=ShellCommand::ResetLayout) {
                    const auto b=shell.bounds(c);
                    check(b.x>=399.5f && b.x+b.width<=400+toolbar_width+.5f && b.y>=24*scale-.5f && b.y+b.height<=24*scale+height+.5f,"wrapped toolbar group clipped a command");
                }
                click(shell,c);auto commands=shell.take_commands();
                check(commands.size()==1 && commands.front()==c,"typed click command "+std::to_string(i));
            }
        }
        shell.update(1100,800,1,false,state,labels);shell.place_rail(0,24,700);shell.place_toolbar(400,24,700);
        // Inspect measured command bounds, independent of group gaps or order.
        const auto bg=qpx::ui::design::color(qpx::ui::design::dark.bg_secondary);
        glClearColor(bg.x,bg.y,bg.z,1);glClear(GL_COLOR_BUFFER_BIT);shell.render(1100,800);
        for(int i=static_cast<int>(ShellCommand::Select);i<=static_cast<int>(ShellCommand::ResetLayout);++i) {
            const auto b=shell.bounds(static_cast<ShellCommand>(i));const int x=static_cast<int>(b.x+2),y=static_cast<int>(b.y+2),w=static_cast<int>(b.width-4),h=static_cast<int>(b.height-4);
            std::vector<unsigned char> pixels(std::size_t(w)*h*4);glReadPixels(x,800-y-h,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            int ink=0;for(std::size_t p=0;p<pixels.size();p+=4)ink+=std::abs(int(pixels[p])-22)+std::abs(int(pixels[p+1])-22)+std::abs(int(pixels[p+2])-22)>100;
            check(ink>5,"invisible measured toolbar icon "+std::to_string(i));
        }
        shell.input({-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()});
        shell.input({std::numeric_limits<float>::quiet_NaN(),0});check(shell.take_commands().empty(),"off-window pointer sentinel");
        click(shell,ShellCommand::Project);shell.take_commands();check(shell.keyboard_active(),"shell keyboard focus");
        ShellInput key;key.x=22;key.y=42;key.tab=true;shell.input(key);key.tab=false;key.enter=true;shell.input(key);
        check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::Image},"Tab/Enter keyboard activation");
        key.enter=false;key.tab=true;key.shift=true;shell.input(key);key.tab=false;key.shift=false;key.space=true;shell.input(key);
        check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::Project},"Shift+Tab/Space keyboard activation");
        const auto original=shell.bounds(ShellCommand::Undo);const auto generation=shell.generation();
        write(resources/"workspace.rml","<rml><head></head><body id=\"broken\" /></rml>");
        check(!shell.reload() && shell.generation()==generation && !shell.error().empty(),"invalid RML must retain live document");
        click(shell,ShellCommand::Undo);check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::Undo},"failed reload retained event listeners");
        check(shell.bounds(ShellCommand::Undo).x==original.x,"failed reload retained geometry");
        write(resources/"workspace.rml",rml);write(resources/"workspace.rcss",css+"\n button { qupathxx-invalid: 7dp; }\n");
        check(!shell.reload() && shell.generation()==generation,"invalid RCSS must retain live document");
        click(shell,ShellCommand::Undo);check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::Undo},"failed stylesheet reload retained commands");
        write(resources/"workspace.rcss",css+"\n button { width: ${missing-role}dp; }\n");
        check(!shell.reload() && shell.generation()==generation,"unknown style token retains the live document");
        write(resources/"workspace.rml",rml);write(resources/"workspace.rcss",css+"\n.activity { width: 52dp; }\n");
        check(shell.reload() && shell.generation()==generation+1 && shell.error().empty(),"valid resource reload");
        check(std::abs(shell.rail_width()-52)<1,"reload actually changes layout");
        state.gesture_active=true;shell.update(1100,800,1,false,state,labels);click(shell,ShellCommand::Objects);check(shell.take_commands().empty(),"active gesture blocks visibility toggle");state.gesture_active=false;
        state.blocked=true;shell.update(1100,800,1,true,state,labels);click(shell,ShellCommand::Undo);check(shell.take_commands().empty(),"modal blocked commands");
        state.blocked=false;state.can_undo=false;state.has_image=false;shell.update(1100,800,1,true,state,labels);
        click(shell,ShellCommand::Rectangle);check(shell.take_commands().empty(),"empty document blocked scientific command");
        click(shell,ShellCommand::ResetLayout);check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::ResetLayout},"empty document retains layout command");
        click(shell,ShellCommand::Settings);check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::Settings},"settings available without an image");
        auto legacy=rml;const auto version=legacy.find("schema=\"3\"");check(version!=std::string::npos,"current resource schema");legacy.replace(version,10,"schema=\"1\"");
        const auto settings=legacy.find("<button id=\"settings\"");const auto settings_end=legacy.find("</button>",settings);
        check(settings!=std::string::npos && settings_end!=std::string::npos,"settings control in schema2");legacy.erase(settings,settings_end+9-settings);
        const auto eye=legacy.find("<button id=\"objects\"");const auto eye_end=legacy.find("</button>",eye);check(eye!=std::string::npos && eye_end!=std::string::npos,"objects control in schema3");legacy.erase(eye,eye_end+9-eye);
        write(resources/"workspace.rml",legacy);write(resources/"workspace.rcss",css);
        check(shell.reload() && shell.bounds(ShellCommand::Settings).width==0,"schema1 resources retain legacy command compatibility");
        click(shell,ShellCommand::Project);check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::Project},"legacy document routes native pane command");
        auto schema2=rml;schema2.replace(schema2.find("schema=\"3\""),10,"schema=\"2\"");const auto eye2=schema2.find("<button id=\"objects\"");schema2.erase(eye2,schema2.find("</button>",eye2)+9-eye2);write(resources/"workspace.rml",schema2);check(shell.reload() && shell.bounds(ShellCommand::Objects).width==0,"schema2 resources tolerate absent objects control");
        auto missing=rml;const auto missing_eye=missing.find("<button id=\"objects\"");missing.erase(missing_eye,missing.find("</button>",missing_eye)+9-missing_eye);write(resources/"workspace.rml",missing);check(!shell.reload(),"schema3 requires objects control");
        write(resources/"workspace.rml",rml);check(shell.reload(),"restore current resource schema");
        check(!state.enabled(static_cast<ShellCommand>(-1)) && !state.enabled(ShellCommand::Count),"unknown command refused");
        state.has_image=true;
        shell.update(1100,800,1,true,state,labels);click(shell,ShellCommand::Undo);check(shell.take_commands().empty(),"native undo availability");
        for(const auto& language:qpx::ui::i18n::languages) {
            labels.fill(language.native_name);shell.update(1100,800,1,true,state,labels);click(shell,ShellCommand::Project);shell.take_commands();shell.render(1100,800);
        }
        // A closed ellipse has no endpoint alpha. Its curved side edges must
        // retain intermediate coverage through the RmlUi bridge, at every DPI.
        state.has_image=true;state.tool=0;
        for(bool light:{false,true})for(float scale:{1.f,1.25f,1.5f,2.f}) {
            shell.update(1100,800,scale,light,state,labels);shell.place_rail(0,28*scale,700);shell.place_toolbar(400,28*scale,700);
            shell.input({-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()});
            const auto& p=qpx::ui::design::palette(light?qpx::ui::design::Theme::Light:qpx::ui::design::Theme::Dark);
            const auto bg=qpx::ui::design::color(p.bg_secondary);glClearColor(bg.x,bg.y,bg.z,1);glClear(GL_COLOR_BUFFER_BIT);shell.render(1100,800);
            const auto b=shell.bounds(ShellCommand::Ellipse);const int x=static_cast<int>(b.x+2*scale),y=static_cast<int>(b.y+2*scale);
            const int w=static_cast<int>(b.width-4*scale),h=static_cast<int>(b.height-4*scale);
            std::vector<unsigned char> pixels(std::size_t(w)*h*4);glReadPixels(x,800-y-h,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            const int a=(p.bg_secondary>>16)&255,z=(p.text_secondary>>16)&255,lo=std::min(a,z)+4,hi=std::max(a,z)-4;
            int partial=0,levels=0;bool seen[256]{};
            for(std::size_t i=0;i<pixels.size();i+=4)if(pixels[i]>lo && pixels[i]<hi) {++partial;seen[pixels[i]]=true;}
            for(bool value:seen)levels+=value;
            check(partial>8 && levels>=5,"ellipse side-edge coverage lost at scale "+std::to_string(scale));
        }
        const auto old_tool=shell.bounds(ShellCommand::Ellipse);
        shell.set_toolbar_visible(false);
        check(shell.bounds(ShellCommand::Ellipse).width==0 && !shell.contains(old_tool.x+old_tool.width/2,old_tool.y+old_tool.height/2),"hidden slide toolbar intercepts docked tool input");
        click(shell,ShellCommand::Project);check(shell.take_commands()==std::vector<ShellCommand>{ShellCommand::Project},"activity navigation lost when slide toolbar is hidden");
        check(shell.reload() && shell.bounds(ShellCommand::Ellipse).width==0,"reload exposed a hidden toolbar");
        shell.place_toolbar(400,28,700);check(shell.bounds(ShellCommand::Ellipse).width>0,"slide toolbar did not return");
        std::cout<<"RmlUi commands/DPI16 locales/reload/coverage/hidden-toolbar navigation passed\n";
    }
    ImGui::EndFrame();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
}
int main(int argc,char** argv) {
    if(!std::getenv("QUPATHXX_TEST_GUI")) {std::cout<<"RmlUi GL tests skipped: opt-in QUPATHXX_TEST_GUI\n";return 0;}
    try {
        const auto directory=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::temp_directory_path()/("qpx-rmlui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);shell_contract(directory);
        std::vector<std::uint8_t> pixels(1024*768*3);for(std::size_t i=0;i<pixels.size();i+=3) {pixels[i]=30;pixels[i+1]=120;pixels[i+2]=210;}
        auto doc=std::make_shared<qpx::Document>(qpx::Image(1024,768,std::move(pixels)));doc->add_rectangle({400,300,80,60});const auto objects=doc->object_count();
        auto project=std::make_shared<qpx::Project>();const auto image=project->add_document(doc);project->set_view(image,{{440,330},1.5,{}});
        const auto project_path=directory/"source.qpxproj";project->save_new(project_path.u8string());
        const auto before=read(project_path);const auto generation=project->generation();
        qpx::GuiOptions options;options.width=1100;options.height=800;options.docked_workspace=true;options.settle_tiles=true;options.layout_path=(directory/"workspace.ini").u8string();
        qpx::GuiInput down;down.frame=4;down.kind=qpx::GuiInputKind::LeftDown;down.target=qpx::GuiInputTarget::Window;down.x=24./1100;down.y=196./800;
        auto up=down;up.frame=5;up.kind=qpx::GuiInputKind::LeftUp;options.inputs={down,up};
        auto report=qpx::run_project_gui(project,16,(directory/"workspace.ppm").u8string(),options);
        check(report.replay_complete && report.canvas_visible && report.tiles_settled,"integrated workspace replay/canvas");
        check(read(directory/"workspace.ini").find("; qupathxx sidebar=workflow\n")!=std::string::npos,"activity row selects the intended pane");
        check(doc->object_count()==objects && project->generation()==generation,"shell navigation mutated native objects/project generation");
        check(before==read(project_path),"shell navigation rewrote saved project");
        check(std::abs(report.final_center.x-440)<1e-8 && std::abs(report.final_center.y-330)<1e-8 && report.final_zoom==1.5,"shell navigation changed camera");
        std::ifstream capture(directory/"workspace.ppm",std::ios::binary);std::string magic;int width,height,max;capture>>magic>>width>>height>>max;capture.get();
        check(magic=="P6" && width==1100 && height==800 && max==255,"native framebuffer capture");
        const std::vector<unsigned char> framebuffer{std::istreambuf_iterator<char>(capture),{}};
        check(framebuffer.size()==static_cast<std::size_t>(width*height*3),"complete framebuffer");
        const auto canvas=report.canvas_content_bounds;const int x=static_cast<int>(canvas.x+70),y=static_cast<int>(canvas.y+90);
        const auto* rgb=framebuffer.data()+(y*width+x)*3;
        check(rgb[0]==30 && rgb[1]==120 && rgb[2]==210,"RmlUi compositing altered scientific image pixels");
        check(read(directory/"workspace.ini").find("; qupathxx sidebar=workflow\n")!=std::string::npos,"integrated activity click");
        std::cout<<"Native shell/camera/project identity/exact RGB passed; frame p95 "<<report.cpu_frame_p95_ms<<" ms\n";
        if(argc>2) {
            const auto source=std::filesystem::u8path(argv[2]);const auto bytes=read(source);auto specimen=qpx::Project::open(source.u8string());
            options.inputs.clear();options.layout_path.clear();
            const auto preview=qpx::run_project_gui(specimen,20,(directory/"specimen.ppm").u8string(),options);
            check(preview.canvas_visible && preview.tiles_settled && preview.uploaded_tiles>0 && preview.project_error.empty(),"real specimen render");
            check(read(source)==bytes,"real specimen file identity");std::cout<<"Real specimen viewer/unchanged project passed\n";
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
