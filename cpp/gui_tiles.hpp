#pragma once
#include "qupathxx/tile_scheduler.hpp"
#include "qupathxx/view_tiles.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <chrono>

namespace qpx {
// All methods and destruction run on the OpenGL context thread.
class GuiTiles {
    struct Texture { GLuint id; std::size_t bytes; std::list<TileKey>::iterator lru; };
    TileScheduler scheduler_;
    std::map<TileKey,Texture> textures_;
    std::list<TileKey> lru_;
    std::map<TileKey,std::uint64_t> pending_;
    std::set<TileKey> desired_, failed_;
    std::size_t bytes_=0;
    const std::size_t budget_=64*1024*1024;
    std::uint64_t uploaded_=0;
    std::size_t peak_=0;
    std::string error_;

    void erase(std::map<TileKey,Texture>::iterator it) {
        glDeleteTextures(1,&it->second.id); bytes_-=it->second.bytes;
        lru_.erase(it->second.lru); textures_.erase(it);
    }
    void upload(const TileKey& key, const PixelBuffer& pixels) {
        auto count=pixels.bytes().size();
        if (count>budget_) throw std::runtime_error("tile exceeds GPU budget");
        while (bytes_>budget_-count && !lru_.empty()) {
            auto victim=textures_.end();
            for (auto it=lru_.rbegin();it!=lru_.rend();++it) if (!desired_.count(*it)) { victim=textures_.find(*it); break; }
            if (victim==textures_.end()) victim=textures_.find(lru_.back());
            erase(victim);
        }
        GLuint texture=0; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F); // GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(pixels.width()),static_cast<GLsizei>(pixels.height()),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.bytes().data());
        if (!texture || glGetError()!=GL_NO_ERROR) { if (texture) glDeleteTextures(1,&texture); throw std::runtime_error("tile texture upload failed"); }
        try {
            lru_.push_front(key);
            try { textures_.emplace(key,Texture{texture,count,lru_.begin()}); }
            catch (...) { lru_.pop_front(); throw; }
        } catch (...) { glDeleteTextures(1,&texture); throw; }
        bytes_+=count; peak_=std::max(peak_,bytes_); ++uploaded_;
    }
public:
    explicit GuiTiles(std::shared_ptr<ImageServer> server) : scheduler_(std::move(server)) {}
    ~GuiTiles() {
        scheduler_.shutdown();
        for (auto& [key,texture] : textures_) glDeleteTextures(1,&texture.id);
    }
    void update(const TilePlan& plan) {
        std::set<TileKey> wanted;
        for (const auto& tile : plan.tiles) wanted.insert(tile_key(tile.request));
        if (wanted!=desired_) {
            scheduler_.advance_generation();
            for (const auto& [key,ticket] : pending_) scheduler_.take(ticket);
            pending_.clear(); failed_.clear(); error_.clear(); desired_=std::move(wanted);
        }
        int uploads=0;
        auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(4);
        for (auto it=pending_.begin();it!=pending_.end() && uploads<8;) {
            if (uploads && std::chrono::steady_clock::now()>=deadline) break;
            auto result=scheduler_.take(it->second);
            if (result.status==TileStatus::Queued || result.status==TileStatus::Running) { ++it; continue; }
            if (result.status==TileStatus::Ready && result.generation==scheduler_.generation() && desired_.count(it->first)) {
                try { upload(it->first,*result.pixels); ++uploads; }
                catch (const std::exception& e) { failed_.insert(it->first); error_=e.what(); }
            } else if (result.status==TileStatus::Failed) { failed_.insert(it->first); error_=result.error; }
            it=pending_.erase(it);
        }
        // Submit highest-priority tiles first; workers may start before the rest
        // of this frame's request set has entered the queue.
        std::vector<const ViewTile*> ordered;
        for (const auto& tile : plan.tiles) ordered.push_back(&tile);
        std::stable_sort(ordered.begin(),ordered.end(),[](auto* a,auto* b){return a->priority>b->priority;});
        for (auto* tile : ordered) {
            auto key=tile_key(tile->request);
            if (!textures_.count(key) && !pending_.count(key) && !failed_.count(key)) {
                try { pending_.emplace(key,scheduler_.submit(tile->request,tile->priority)); }
                catch (const std::exception& e) { failed_.insert(key); error_=e.what(); }
            }
        }
    }
    GLuint texture(const ViewTile& tile) {
        auto it=textures_.find(tile_key(tile.request)); if (it==textures_.end()) return 0;
        lru_.splice(lru_.begin(),lru_,it->second.lru); return it->second.id;
    }
    std::size_t pending() const { return pending_.size(); }
    std::size_t bytes() const { return bytes_; }
    std::size_t peak_bytes() const { return peak_; }
    std::uint64_t uploads() const { return uploaded_; }
    std::uint64_t generations() const { return scheduler_.generation(); }
    const std::string& error() const { return error_; }
};
}
