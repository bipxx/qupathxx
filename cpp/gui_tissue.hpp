#pragma once
#include "qupathxx/tissue.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <stdexcept>
namespace qpx {
// Context-thread preview. Prepare at most four 256-pixel tiles per frame and
// publish the overlay only when the entire bounded preview is ready.
class TissuePreview {
    struct Tile {GLuint texture;Rect bounds;};
    std::shared_ptr<const TissueResult> owner_;
    std::vector<Tile> tiles_;
    std::size_t cursor_=0,total_=0,bytes_=0,peak_=0;
    bool failed_=false;
    std::string error_;
public:
    ~TissuePreview() {clear();}
    void clear() {
        for(const auto& tile:tiles_) glDeleteTextures(1,&tile.texture);
        tiles_.clear();owner_.reset();cursor_=total_=bytes_=0;failed_=false;error_.clear();
    }
    bool ready() const {return owner_ && !failed_ && cursor_==total_;}
    std::size_t peak_bytes() const {return peak_;}
    std::size_t completed_tiles() const {return cursor_;}
    std::size_t total_tiles() const {return total_;}
    const std::string& error() const {return error_;}
    void update(std::shared_ptr<const TissueResult> result,bool show) {
        if(owner_!=result) {
            clear();owner_=std::move(result);if(!owner_) return;
            const auto width=owner_->width(),height=owner_->height();
            total_=static_cast<std::size_t>(((width+255)/256)*((height+255)/256));
            if(owner_->labels().size()>32u*1024u*1024u/4 || total_>4096) {failed_=true;error_="Tissue mask exceeds preview budget";return;}
            try {tiles_.reserve(total_);} catch(const std::exception& e) {failed_=true;error_=e.what();return;}
            if(!owner_->foreground_pixels()) cursor_=total_;
        }
        if(!owner_ || !show || ready() || failed_) return;
        try {
            const auto width=owner_->width(),height=owner_->height(),columns=(width+255)/256;
            const auto origin=owner_->bounds();const auto scale=(owner_->scientific_grid() || owner_->nominal_sampling())?0.:owner_->downsample();std::vector<std::uint8_t> rgba;
            for(unsigned budget=0;budget<4 && cursor_<total_;++budget,++cursor_) {
                const auto x=static_cast<std::int64_t>(cursor_%columns)*256,y=static_cast<std::int64_t>(cursor_/columns)*256;
                const auto w=std::min<std::int64_t>(256,width-x),h=std::min<std::int64_t>(256,height-y);
                rgba.resize(static_cast<std::size_t>(w*h*4));bool any=false;
                for(std::int64_t row=0;row<h;++row) for(std::int64_t col=0;col<w;++col) {
                    const bool positive=owner_->labels()[static_cast<std::size_t>((y+row)*width+x+col)]!=0;any=any || positive;
                    const auto i=static_cast<std::size_t>((row*w+col)*4);
                    rgba[i]=positive?255:0;rgba[i+1]=positive?140:0;rgba[i+2]=positive?40:0;rgba[i+3]=positive?110:0;
                }
                if(!any) continue;
                GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(w),static_cast<GLsizei>(h),0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
                if(!texture || glGetError()!=GL_NO_ERROR) {if(texture) glDeleteTextures(1,&texture);throw std::runtime_error("tissue preview upload failed");}
                // Capacity was reserved before GPU allocation; this cannot allocate.
                Rect bounds{origin.x+x*scale,origin.y+y*scale,double(w)*scale,double(h)*scale};
                if(owner_->scientific_grid() || owner_->nominal_sampling()) {const auto left=owner_->sample_edge_x(x),top=owner_->sample_edge_y(y);
                    bounds={left,top,owner_->sample_edge_x(x+w)-left,owner_->sample_edge_y(y+h)-top};}
                tiles_.push_back({texture,bounds});bytes_+=rgba.size();peak_=std::max(peak_,bytes_);
            }
            glBindTexture(GL_TEXTURE_2D,0);
        } catch(const std::exception& e) {failed_=true;error_=e.what();}
    }
    void draw(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size) const {
        if(!ready()) return;
        for(const auto& tile:tiles_) {
            const auto a=view.image_to_screen({tile.bounds.x,tile.bounds.y}),b=view.image_to_screen({tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height});
            if(b.x<0 || b.y<0 || a.x>size.x || a.y>size.y) continue;
            draw->AddImage(static_cast<ImTextureID>(tile.texture),{offset.x+float(a.x),offset.y+float(a.y)},{offset.x+float(b.x),offset.y+float(b.y)});
        }
    }
};
}
