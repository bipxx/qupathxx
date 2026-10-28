#pragma once
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>

namespace qpx {
struct OverlaySurface {
    int x,y,width,height,frame_width,frame_height;
    ImVec2 top_left,bottom_right;
    std::size_t bytes() const {return std::size_t(width)*height*4;}
    auto values() const {return std::tie(x,y,width,height,frame_width,frame_height,top_left.x,top_left.y,bottom_right.x,bottom_right.y);}
    bool operator==(const OverlaySurface& other) const {return values()==other.values();}
};
inline std::optional<OverlaySurface> overlay_surface(ImVec4 clip,ImVec2 display,ImVec2 scale,int max_texture,std::size_t budget) {
    // Match the backend's float multiplication and nearest-pixel rounding.
    const double fw=display.x*scale.x,fh=display.y*scale.y;
    if(!std::isfinite(fw) || !std::isfinite(fh) || fw<=0 || fh<=0 || fw>std::numeric_limits<int>::max() || fh>std::numeric_limits<int>::max() ||
       !std::isfinite(scale.x) || !std::isfinite(scale.y) || scale.x<=0 || scale.y<=0 ||
       !std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) || !std::isfinite(clip.w)) return std::nullopt;
    const int width=static_cast<int>(static_cast<float>(fw)+.5f),height=static_cast<int>(static_cast<float>(fh)+.5f);
    const auto left=int(std::clamp(std::floor(double(clip.x)*scale.x),0.,double(width)));
    const auto top=int(std::clamp(std::floor(double(clip.y)*scale.y),0.,double(height)));
    const auto right=int(std::clamp(std::ceil(double(clip.z)*scale.x),0.,double(width)));
    const auto bottom=int(std::clamp(std::ceil(double(clip.w)*scale.y),0.,double(height)));
    if(right<=left || bottom<=top || right-left>max_texture || bottom-top>max_texture || std::uint64_t(right-left)*std::uint64_t(bottom-top)>budget/4) return std::nullopt;
    return OverlaySurface{left,height-bottom,right-left,bottom-top,width,height,
        {left/scale.x,top/scale.y},{right/scale.x,bottom/scale.y}};
}
struct OverlayKey {
    const void* document;const void* selection;std::uint64_t revision,selection_revision,primary;
    double zoom,x,y;float ox,oy,sx,sy,cx,cy,cz,cw;
    ImFont* font;float font_size;ImTextureID texture;int flags;float uvx,uvy,curve,circle;
    std::uint64_t uploads,generation,tissue_generation,cell_generation;std::size_t image_bytes;const void* tissue;const void* cells;
    bool show_tissue,show_cells,tissue_ready,cells_ready;int cell_layer;
    std::uint64_t pixel_generation=0;bool show_pixels=false,pixels_ready=false;
    std::uint64_t raw_pixel_generation=0;bool show_raw_pixels=false,raw_pixels_ready=false;
    ImGuiID font_baked_id=0;int font_glyphs=0;
    auto values() const {return std::tie(document,selection,revision,selection_revision,primary,zoom,x,y,ox,oy,sx,sy,cx,cy,cz,cw,font,font_size,texture,flags,uvx,uvy,curve,circle,
        uploads,generation,tissue_generation,cell_generation,image_bytes,tissue,cells,show_tissue,show_cells,tissue_ready,cells_ready,cell_layer,pixel_generation,show_pixels,pixels_ready,raw_pixel_generation,show_raw_pixels,raw_pixels_ready,font_baked_id,font_glyphs);}
    bool operator==(const OverlayKey& other) const {return values()==other.values();}
};
// Copy the opaque base canvas immediately after its draw commands execute.
// Subsequent frames reuse those exact framebuffer bytes, before transient tools.
class OverlayCache {
    GLuint texture_=0;
    std::optional<OverlaySurface> surface_;
    std::optional<OverlayKey> key_;
    bool valid_=false,failed_=false;
    static void copy_callback(const ImDrawList*,const ImDrawCmd* command) {
        auto& self=*static_cast<OverlayCache*>(command->UserCallbackData);
        if(!self.texture_ || !self.surface_) return;
        const auto& s=*self.surface_;GLint viewport[4],binding=0;
        glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
        if(viewport[0]!=0 || viewport[1]!=0 || viewport[2]!=s.frame_width || viewport[3]!=s.frame_height) {self.failed_=true;return;}
        glBindTexture(GL_TEXTURE_2D,self.texture_);
        glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,s.x,s.y,s.width,s.height);
        self.valid_=glGetError()==GL_NO_ERROR;self.failed_=!self.valid_;
        glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(binding));
        if(self.valid_) ++self.builds;
    }
    void release() {if(texture_) glDeleteTextures(1,&texture_);texture_=0;surface_.reset();valid_=false;}
public:
    std::uint64_t builds=0,hits=0,fallbacks=0;std::size_t peak_bytes=0;
    ~OverlayCache() {release();}
    std::size_t bytes() const {return texture_ && surface_?surface_->bytes():0;}
    void clear() {release();key_.reset();failed_=false;}
    bool prepare(const OverlayKey& key,ImVec4 clip,int max_texture,std::size_t budget) {
        const auto& io=ImGui::GetIO();const auto surface=overlay_surface(clip,io.DisplaySize,io.DisplayFramebufferScale,max_texture,budget);
        if(!surface) {clear();if(budget) ++fallbacks;return false;}
        const bool same=key_ && *key_==key && surface_ && *surface_==*surface;
        if(same && valid_) {++hits;return true;}
        if(same && failed_) {++fallbacks;return false;}
        valid_=false;failed_=false;key_=key;
        if(!texture_ || !surface_ || surface_->width!=surface->width || surface_->height!=surface->height) {
            release();GLint binding=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
            glGenTextures(1,&texture_);glBindTexture(GL_TEXTURE_2D,texture_);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA, surface->width,surface->height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            failed_=!texture_ || glGetError()!=GL_NO_ERROR;glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(binding));
            if(failed_) {release();surface_=surface;++fallbacks;return false;}
        }
        surface_=surface;peak_bytes=std::max(peak_bytes,bytes());return false;
    }
    void draw(ImDrawList* draw) const {draw->AddImage(static_cast<ImTextureID>(texture_),surface_->top_left,surface_->bottom_right,{0,1},{1,0});}
    void capture(ImDrawList* draw) {if(texture_ && !failed_) draw->AddCallback(copy_callback,this);}
};
}
