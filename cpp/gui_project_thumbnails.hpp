#pragma once
#include "gui_layout.hpp"
#include "qupathxx/thumbnail.hpp"
#include "qupathxx/display.hpp"
#include <GLFW/glfw3.h>
#include <map>
#include <limits>

namespace qpx {
// Visible-row preview cache: one native decode job, <=64 small GL textures.
// Native thumbnail planning bounds source reads; no analysis or project writes.
class GuiProjectThumbnails {
    struct Entry {
        std::shared_ptr<ImageServer> source;
        DisplaySettings display;
        std::shared_ptr<const ThumbnailResult> raw;
        GLuint texture=0;int width=0,height=0;
        std::uint64_t used=0;
        std::string state="idle",error;
    };
    std::map<std::uint64_t,Entry> entries_;
    ThumbnailJob job_;
    std::shared_ptr<ImageServer> pending_source_;
    std::uint64_t pending_=0,epoch_=std::numeric_limits<std::uint64_t>::max(),frame_=0;
    static void clear(Entry& entry) {if(entry.texture)glDeleteTextures(1,&entry.texture);entry.texture=0;}
    static void upload(Entry& entry) {
        if(!entry.raw)return;
        try {
            const auto transformed=entry.display==DisplaySettings{}?std::vector<std::uint8_t>{}:display_pixels(entry.raw->pixels,entry.display);
            const auto* pixels=transformed.empty()?entry.raw->pixels.bytes().data():transformed.data();
            clear(entry);glGenTextures(1,&entry.texture);glBindTexture(GL_TEXTURE_2D,entry.texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            entry.width=static_cast<int>(entry.raw->pixels.width());entry.height=static_cast<int>(entry.raw->pixels.height());
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,entry.width,entry.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
            if(!entry.texture || glGetError()!=GL_NO_ERROR)throw std::runtime_error("thumbnail upload failed");
            glBindTexture(GL_TEXTURE_2D,0);entry.state="succeeded";entry.error.clear();
        } catch(const std::exception& e) {clear(entry);entry.state="failed";entry.error=e.what();}
    }
    void poll() {
        const auto stats=job_.poll();if(!pending_ || job_.busy())return;
        const auto found=entries_.find(pending_);
        if(found!=entries_.end() && found->second.source==pending_source_) {
            auto& entry=found->second;entry.state=stats.state;entry.error=stats.error;
            if(const auto result=job_.result();stats.state=="succeeded" && result) {
                entry.raw=result;upload(entry);
            }
        }
        pending_=0;pending_source_.reset();
    }
public:
    ~GuiProjectThumbnails() {reset();}
    void reset() {job_.cancel();pending_=0;pending_source_.reset();for(auto& pair:entries_)clear(pair.second);entries_.clear();}
    void begin_frame(std::uint64_t epoch) {++frame_;if(epoch_!=epoch) {reset();epoch_=epoch;}poll();}
    bool settled() const {
        if(job_.busy())return false;
        for(const auto& pair:entries_)if(pair.second.used==frame_ && pair.second.state=="idle")return false;
        return true;
    }
    void draw(std::uint64_t id,std::shared_ptr<ImageServer> source,const DisplaySettings& display,ImVec2 lo,ImVec2 hi) {
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(lo,hi,ImGui::GetColorU32(ui::design::color(ui::design::colors().bg_tertiary)),ui::design::metrics.control_radius*ui::design::scale());
        Entry* entry=nullptr;
        if(source) {
            auto found=entries_.find(id);
            if(found==entries_.end() && entries_.size()>=64) {
                auto oldest=entries_.end();for(auto it=entries_.begin();it!=entries_.end();++it)
                    if(it->first!=pending_ && it->second.used!=frame_ && (oldest==entries_.end() || it->second.used<oldest->second.used))oldest=it;
                if(oldest!=entries_.end()) {clear(oldest->second);entries_.erase(oldest);}
            }
            if(found!=entries_.end() || entries_.size()<64) {
                entry=&entries_[id];
                if(entry->source!=source) {clear(*entry);*entry=Entry{};entry->source=std::move(source);entry->display=display;}
                else if(!(entry->display==display)) {entry->display=display;if(entry->raw)upload(*entry);}
                entry->used=frame_;
                if(entry->state=="idle" && !job_.busy() && !pending_) {
                    try {
                        ThumbnailOptions options;options.max_width=options.max_height=64;options.source_pixel_limit=8*1024*1024;options.output_pixel_limit=4096;
                        if(job_.request(entry->source,options)) {pending_=id;pending_source_=entry->source;entry->state="running";}
                    } catch(const std::exception& e) {entry->state="failed";entry->error=e.what();}
                }
            }
        }
        if(entry && entry->texture) {
            const float fit=std::min((hi.x-lo.x)/entry->width,(hi.y-lo.y)/entry->height);
            const ImVec2 start{(lo.x+hi.x-entry->width*fit)*.5f,(lo.y+hi.y-entry->height*fit)*.5f};
            draw->AddImage(static_cast<ImTextureID>(entry->texture),start,{start.x+entry->width*fit,start.y+entry->height*fit});
        } else ui::draw_icon(ui::Icon::Image,lo,hi,ImGui::GetColorU32(ImGuiCol_TextDisabled));
        if(entry && !entry->error.empty() && ImGui::IsMouseHoveringRect(lo,hi))ImGui::SetTooltip("%s",entry->error.c_str());
    }
};
}
