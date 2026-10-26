#pragma once
#include "qupathxx/pixel_prediction.hpp"
#include "qupathxx/raw_pixel_classifier.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <stdexcept>
namespace qpx::detail {
// Context-thread only. At most four 256px tiles (1 MiB) per frame;
// reveal only a complete preview. Generation/readiness invalidate canvas caches.
class GuiPixelPreview {
    struct Tile {GLuint texture;Rect bounds,local;};
    std::shared_ptr<const PixelPredictionResult> owner_;
    std::shared_ptr<const RawPixelWindowClassification> raw_owner_;
    auto width() const {return owner_?owner_->width():raw_owner_->window().core_width();}
    auto height() const {return owner_?owner_->height():raw_owner_->window().core_height();}
    const auto& labels() const {return owner_?owner_->labels():raw_owner_->labels();}
    std::vector<Tile> tiles_;std::vector<std::uint32_t> palette_;
    std::size_t cursor_=0,total_=0,bytes_=0,peak_=0;std::uint64_t generation_=0;
    std::string error_;
public:
    ~GuiPixelPreview() {clear();}
    void clear() {for(auto& tile:tiles_) glDeleteTextures(1,&tile.texture);tiles_.clear();owner_.reset();raw_owner_.reset();palette_.clear();cursor_=total_=bytes_=0;error_.clear();++generation_;}
    bool ready() const {return (owner_ || raw_owner_) && error_.empty() && cursor_==total_;}
    const std::string& error() const {return error_;}
    std::size_t bytes() const {return bytes_;}
    std::size_t peak_bytes() const {return peak_;}
    std::size_t completed_tiles() const {return cursor_;}
    std::uint64_t generation() const {return generation_;}
    void update(std::shared_ptr<const PixelPredictionResult> result,const std::vector<std::uint32_t>& palette,bool show,std::size_t limit) {update_impl(std::move(result),{},palette,show,limit);}
    void update(std::shared_ptr<const RawPixelWindowClassification> result,const std::vector<std::uint32_t>& palette,bool show,std::size_t limit) {update_impl({},std::move(result),palette,show,limit);}
    void update_impl(std::shared_ptr<const PixelPredictionResult> result,std::shared_ptr<const RawPixelWindowClassification> raw,const std::vector<std::uint32_t>& palette,bool show,std::size_t limit) {
        if(owner_!=result || raw_owner_!=raw || palette_!=palette) {
            clear();owner_=std::move(result);raw_owner_=std::move(raw);palette_=palette;if(!owner_ && !raw_owner_) return;
            const auto w=width(),h=height();total_=static_cast<std::size_t>(((w+255)/256)*((h+255)/256));
            if(palette_.size()!=(owner_?owner_->model().classes().size():raw_owner_->class_counts().size()) || labels().size()>limit/4 || total_>4096) {error_="Pixel mask exceeds preview budget; choose a smaller area or raise the preview limit";return;}
            try {tiles_.reserve(total_);}catch(const std::exception& e) {error_=e.what();return;}
            if(owner_ && !owner_->classified_pixels()) cursor_=total_;
        }
        if((!owner_ && !raw_owner_) || !show || ready() || !error_.empty()) return;
        try {
            const auto width=this->width(),height=this->height(),columns=(width+255)/256;const auto origin=owner_?owner_->bounds():Rect{};const auto scale=owner_?owner_->downsample():1.;std::vector<std::uint8_t> rgba;
            for(unsigned budget=0;budget<4 && cursor_<total_;++budget,++cursor_) {
                const auto x=static_cast<std::int64_t>(cursor_%columns)*256,y=static_cast<std::int64_t>(cursor_/columns)*256,w=std::min<std::int64_t>(256,width-x),h=std::min<std::int64_t>(256,height-y);rgba.resize(static_cast<std::size_t>(w*h*4));bool any=false;
                for(std::int64_t yy=0;yy<h;++yy) for(std::int64_t xx=0;xx<w;++xx) {const auto label=labels()[static_cast<std::size_t>((y+yy)*width+x+xx)];const auto i=static_cast<std::size_t>((yy*w+xx)*4);const auto color=label?palette_.at(label-1):0;any=any || label;for(unsigned c=0;c<4;++c) rgba[i+c]=static_cast<std::uint8_t>(color>>(c*8));}
                if(!any) continue;GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(w),static_cast<GLsizei>(h),0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());if(!texture || glGetError()!=GL_NO_ERROR) {if(texture) glDeleteTextures(1,&texture);throw std::runtime_error("pixel preview upload failed");}
                Rect bounds{origin.x+x*scale,origin.y+y*scale,w*scale,h*scale};
                if(raw_owner_) {const auto& grid=raw_owner_->window();const auto gx=grid.x().first_core_index()+x,gy=grid.y().first_core_index()+y;const auto left=grid.x().edge(gx).binary64(),top=grid.y().edge(gy).binary64();bounds={left,top,grid.x().edge(gx+w).binary64()-left,grid.y().edge(gy+h).binary64()-top};}
                tiles_.push_back({texture,bounds,{double(x),double(y),double(w),double(h)}});bytes_+=rgba.size();peak_=std::max(peak_,bytes_);
            }glBindTexture(GL_TEXTURE_2D,0);
        }catch(const std::exception& e) {error_=e.what();}
    }
    void draw(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size) const {
        if(!ready()) return;for(const auto& tile:tiles_) {const auto a=view.image_to_screen({tile.bounds.x,tile.bounds.y}),b=view.image_to_screen({tile.bounds.x+tile.bounds.width,tile.bounds.y+tile.bounds.height});if(b.x<0 || b.y<0 || a.x>size.x || a.y>size.y) continue;draw->AddImage(static_cast<ImTextureID>(tile.texture),{offset.x+float(a.x),offset.y+float(a.y)},{offset.x+float(b.x),offset.y+float(b.y)});}
    }
    void draw_local(ImDrawList* draw,ImVec2 offset,ImVec2 size) const {
        if(!ready()) return;const auto scale=std::min(double(size.x)/width(),double(size.y)/height());
        const ImVec2 origin{offset.x+float((size.x-width()*scale)*.5),offset.y+float((size.y-height()*scale)*.5)};
        for(const auto& tile:tiles_) {const auto& b=tile.local;draw->AddImage(static_cast<ImTextureID>(tile.texture),{origin.x+float(b.x*scale),origin.y+float(b.y*scale)},{origin.x+float((b.x+b.width)*scale),origin.y+float((b.y+b.height)*scale)});}
    }
};
}
