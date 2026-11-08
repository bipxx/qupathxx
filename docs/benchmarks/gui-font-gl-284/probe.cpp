#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "gui_fonts.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) {if(!value) throw std::runtime_error(message);}
std::uint64_t rss() {
    PROCESS_MEMORY_COUNTERS counters{};
    require(GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0,"Cannot sample process working set");
    return counters.WorkingSetSize;
}
struct Measurements {
    std::map<GLuint,std::uint64_t> live;
    std::set<GLuint> names;
    std::uint64_t peak_gl=0, peak_cpu=0, peak_rss=0, readback_bytes=0;
    int creates=0, updates=0, destroys=0, readbacks=0;
    void sample() {
        std::uint64_t bytes=0;for(const auto& v:live) bytes+=v.second;
        peak_gl=std::max(peak_gl,bytes);peak_rss=std::max(peak_rss,rss());
    }
    void textures() {
        auto* textures=ImGui::GetDrawData()->Textures;
        require(textures!=nullptr,"Missing texture protocol");
        std::uint64_t cpu=0;
        for(const auto* tex:*textures) if(tex->Pixels) cpu+=tex->GetSizeInBytes();
        peak_cpu=std::max(peak_cpu,cpu);
        for(auto* tex:*textures) {
            const auto before=tex->Status;
            const auto old=tex->GetTexID();
            if(before!=ImTextureStatus_OK) ImGui_ImplOpenGL3_UpdateTexture(tex);
            if(before==ImTextureStatus_WantCreate && tex->Status==ImTextureStatus_OK) ++creates;
            if(before==ImTextureStatus_WantUpdates && tex->Status==ImTextureStatus_OK) ++updates;
            if(tex->Status==ImTextureStatus_Destroyed && old!=ImTextureID_Invalid) {
                const auto id=static_cast<GLuint>(old);
                require(!glIsTexture(id),"Retired atlas texture still exists");
                live.erase(id);++destroys;
            }
            if(tex->GetTexID()!=ImTextureID_Invalid && tex->Status!=ImTextureStatus_Destroyed) {
                const auto id=static_cast<GLuint>(tex->GetTexID());
                require(glIsTexture(id)!=0,"Missing live GL atlas");
                names.insert(id);
                GLint previous=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);
                glBindTexture(GL_TEXTURE_2D,id);
                GLint width=0,height=0,r=0,g=0,b=0,a=0;
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_RED_SIZE,&r);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_GREEN_SIZE,&g);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_BLUE_SIZE,&b);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_ALPHA_SIZE,&a);
                require(width==tex->Width && height==tex->Height,"GL texture dimensions differ from CPU atlas");
                require(width<=2048 && height<=2048 && r==8 && g==8 && b==8 && a==8,"Unexpected atlas storage or quota");
                const auto bytes=std::uint64_t(width)*height*(r+g+b+a)/8;
                live[id]=bytes;
                // Backend uploads RGBA32; authenticate every byte after each create/update.
                if((before==ImTextureStatus_WantCreate || before==ImTextureStatus_WantUpdates) && tex->Pixels) {
                    std::vector<unsigned char> actual(static_cast<std::size_t>(bytes));
                    GLint packing=0;glGetIntegerv(GL_PACK_ALIGNMENT,&packing);glPixelStorei(GL_PACK_ALIGNMENT,1);
                    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
                    glPixelStorei(GL_PACK_ALIGNMENT,packing);
                    const auto* expected=static_cast<const unsigned char*>(tex->GetPixels());
                    require(std::equal(actual.begin(),actual.end(),expected),"GL atlas readback differs from CPU pixels");
                    ++readbacks;readback_bytes+=bytes;
                }
                glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previous));
            }
            require(glGetError()==GL_NO_ERROR,"OpenGL texture protocol error");
            sample();
        }
    }
};
void session(const char* name,float logical,float density,bool stress) {
    const auto rss_before=rss();
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(int(640*density),int(480*density),"qpx font resource probe",nullptr,nullptr);
    require(window!=nullptr,"Cannot create OpenGL3.3 window");glfwMakeContextCurrent(window);glfwSwapInterval(0);
    const auto* version_ptr=reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* vendor_ptr=reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* renderer_ptr=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    require(version_ptr && vendor_ptr && renderer_ptr,"Missing OpenGL driver identity");
    const std::string version(version_ptr),vendor(vendor_ptr),renderer(renderer_ptr);
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    require(ImGui_ImplGlfw_InitForOpenGL(window,true),"Cannot initialize GLFW backend");
    require(ImGui_ImplOpenGL3_Init("#version 330 core"),"Cannot initialize OpenGL backend");
    GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    qpx::detail::configure_gui_fonts(*io.Fonts,maximum);
    ImGui::GetStyle().FontSizeBase=13*logical;
    Measurements m;int present=0;
    const ImWchar samples[]={0x41,0xe9,0x3b1,0x539f,0x59cb,0x96ea,0x56fe,0x50cf,0x65e5,0x672c,0x8a9e,
        0x3042,0x30a2,0xd55c,0xad6d,0xc5b4,0x2000b,0x1f52c,0x1f469,0x301,0x200d};
    int actual_width=0,actual_height=0;
    for(int frame=0;frame<8;++frame) {
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();
        glfwGetFramebufferSize(window,&actual_width,&actual_height);
        io.DisplaySize={640,480};io.DisplayFramebufferScale={actual_width/640.f,actual_height/480.f};io.DeltaTime=1.f/60;
        require(std::abs(io.DisplayFramebufferScale.x-density)<.01f,"Requested synthetic framebuffer density not delivered");
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({620,450});
        ImGui::Begin("Dynamic fonts",nullptr,ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted(u8"A_e 原始 雪 图像 日本語 あア 한국어 𠀋 🔬 👩 é");
        auto* baked=ImGui::GetFontBaked();
        if(frame==1) for(const auto code:samples) require(baked->FindGlyphNoFallback(code)!=nullptr,"Missing sampled glyph");
        if(frame==2 && stress) for(unsigned code=0x4e00;code<0x4e00+1600;++code) {
            require(ImGui::GetFont()->IsGlyphInFont(code),"Unsupported stress glyph");
            const auto* glyph=baked->FindGlyphNoFallback(code);present+=glyph && glyph->Codepoint==code;
        }
        ImGui::End();ImGui::Render();m.textures();
        // We have already executed every update, so suppress the renderer's duplicate traversal.
        auto* data=ImGui::GetDrawData();auto* textures=data->Textures;data->Textures=nullptr;
        glViewport(0,0,actual_width,actual_height);glClearColor(.05f,.06f,.07f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(data);data->Textures=textures;
        glFinish();require(glGetError()==GL_NO_ERROR,"OpenGL draw error");m.sample();glfwSwapBuffers(window);
    }
    if(stress) require(present>0 && present<1600 && io.Fonts->TexData->Width==2048 && io.Fonts->TexData->Height==2048,"Quota exhaustion was not exercised");
    require(m.creates>0 && m.readbacks>0,"No actual texture upload observed");
    const auto live_before=m.live.size();
    ImGui_ImplOpenGL3_Shutdown();
    for(const auto id:m.names) require(!glIsTexture(id),"Atlas GL object leaked at renderer shutdown");
    ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);
    std::cout<<"{\"name\":"<<std::quoted(name)<<",\"logical_scale\":"<<logical<<",\"synthetic_framebuffer_density\":"<<density
        <<",\"framebuffer_width\":"<<actual_width<<",\"framebuffer_height\":"<<actual_height
        <<",\"gl_version\":"<<std::quoted(version)<<",\"gl_vendor\":"<<std::quoted(vendor)<<",\"gl_renderer\":"<<std::quoted(renderer)
        <<",\"cpu_atlas_peak_bytes\":"<<m.peak_cpu<<",\"queried_gl_storage_peak_bytes\":"<<m.peak_gl
        <<",\"creates\":"<<m.creates<<",\"updates\":"<<m.updates<<",\"retired_during_frames\":"<<m.destroys
        <<",\"readbacks\":"<<m.readbacks<<",\"readback_bytes\":"<<m.readback_bytes<<",\"stress_glyphs_present\":"<<present
        <<",\"live_before_shutdown\":"<<live_before<<",\"live_after_shutdown\":0,\"rss_before_session\":"<<rss_before
        <<",\"rss_observed_peak\":"<<m.peak_rss<<",\"rss_after_session\":"<<rss()<<"}\n"<<std::flush;
}
}
int main() {
    try {
        static_assert(sizeof(ImWchar)==4,"32-bit ImGui ABI required");
        require(glfwInit()!=0,"Cannot initialize GLFW");
        session("normal",1,1,false);session("fractional",1.5f,1.25f,false);
        session("large",3,2,false);session("maximal",6,4,false);
        session("quota",3,2,true);session("repeat_normal",1,1,false);
        glfwTerminate();return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';glfwTerminate();return 1;}
}
