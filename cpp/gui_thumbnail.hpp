#pragma once
#include "qupathxx/thumbnail.hpp"
#include "qupathxx/display.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <optional>
#include <stdexcept>
namespace qpx {
// One bounded asynchronous request, one complete texture, no partial preview.
// This controller and all GL operations belong to the context thread.
class GuiThumbnail {
    ThumbnailJob job_;
    std::shared_ptr<ImageServer> source_,request_source_;
    std::shared_ptr<const ThumbnailResult> raw_;
    GLuint texture_=0;
    std::optional<DisplaySettings> display_;
    bool requested_=false;
    std::string state_="idle",error_;
    std::size_t bytes_=0,peak_=0;
    void clear_texture() {if(texture_) glDeleteTextures(1,&texture_);texture_=0;bytes_=0;display_.reset();}
public:
    ~GuiThumbnail() {clear_texture();}
    void reset() {job_.cancel();source_.reset();request_source_.reset();raw_.reset();clear_texture();requested_=false;state_="idle";error_.clear();}
    void update(std::shared_ptr<ImageServer> server,const DisplaySettings& display) {
        if(source_!=server) {reset();source_=std::move(server);}
        const auto stats=job_.poll();
        if(!requested_ && source_ && !job_.busy()) {
            try {
                ThumbnailOptions options;options.max_width=250;options.max_height=150;
                job_.request(source_,options);request_source_=source_;state_="running";
            } catch(const std::exception& error) {state_="failed";error_=error.what();}
            requested_=true;
        } else if(requested_ && request_source_==source_ && !job_.busy() && state_=="running") {
            state_=stats.state;error_=stats.error;raw_=job_.result();
        }
        if(!raw_ || (texture_ && display_ && *display_==display)) return;
        try {
            auto transformed=display==DisplaySettings{}?std::vector<std::uint8_t>{}:display_pixels(raw_->pixels,display);
            const auto* data=transformed.empty()?raw_->pixels.bytes().data():transformed.data();
            GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(raw_->pixels.width()),static_cast<GLsizei>(raw_->pixels.height()),0,GL_RGBA,GL_UNSIGNED_BYTE,data);
            if(!texture || glGetError()!=GL_NO_ERROR) {if(texture) glDeleteTextures(1,&texture);throw std::runtime_error("overview upload failed");}
            clear_texture();texture_=texture;display_=display;bytes_=raw_->pixels.bytes().size();peak_=std::max(peak_,bytes_);
            glBindTexture(GL_TEXTURE_2D,0);
        } catch(const std::exception& error) {clear_texture();raw_.reset();state_="failed";error_=error.what();}
    }
    bool settled() const {return state_=="failed" || (state_=="succeeded" && texture_);}
    bool ready() const {return state_=="succeeded" && texture_;}
    GLuint texture() const {return texture_;}
    const std::string& state() const {return state_;}
    const std::string& error() const {return error_;}
    std::size_t bytes() const {return bytes_;}
    std::size_t peak_bytes() const {return peak_;}
    std::shared_ptr<const ThumbnailResult> result() const {return raw_;}
};
}
