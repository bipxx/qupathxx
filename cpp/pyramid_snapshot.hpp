#pragma once
#include "qupathxx/image_server.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace qpx::detail {
inline void pyramid_checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load(std::memory_order_relaxed))
        throw std::runtime_error("pyramid snapshot cancelled");
}
inline std::uint64_t pyramid_snapshot_work_bytes(const ImageMetadata& metadata,std::int32_t level) {
    if(level<=0 || static_cast<std::size_t>(level)>=metadata.levels.size())
        throw std::invalid_argument("canonical pyramid requires an explicit level above zero");
    const auto& l=metadata.levels[level];
    const auto bytes=region_byte_count(l.width,l.height);
    if(bytes>std::numeric_limits<std::uint64_t>::max()/2)
        throw std::length_error("pyramid snapshot size overflow");
    // RGBA output and the decoder's simultaneous ARGB staging buffer.
    return std::uint64_t(bytes)*2;
}
inline PixelBuffer read_pyramid_snapshot(const ImageServer& source,std::int32_t level,const std::atomic<bool>* cancelled) {
    pyramid_checkpoint(cancelled);const auto& metadata=source.metadata();
    pyramid_snapshot_work_bytes(metadata,level);const auto& l=metadata.levels[level];
    auto pixels=source.read_region({0,0,level,l.width,l.height});
    pyramid_checkpoint(cancelled);
    if(pixels.width()!=l.width || pixels.height()!=l.height || pixels.bytes().size()!=region_byte_count(l.width,l.height))
        throw std::runtime_error("reader returned incorrect pyramid snapshot dimensions");
    return pixels;
}
inline PixelBuffer pyramid_snapshot_tile(const PixelBuffer& source,std::int64_t x,std::int64_t y,std::int64_t w,std::int64_t h) {
    if(x<0 || y<0 || w<=0 || h<=0 || x>source.width()-w || y>source.height()-h)
        throw std::invalid_argument("pyramid tile exceeds snapshot");
    PixelBuffer tile(w,h);
    for(std::int64_t row=0;row<h;++row)
        std::memcpy(tile.writable_bytes().data()+static_cast<std::size_t>(row*w*4),
                    source.bytes().data()+static_cast<std::size_t>(((y+row)*source.width()+x)*4),static_cast<std::size_t>(w*4));
    return tile;
}
inline std::string pyramid_snapshot_sha256(const ImageMetadata& metadata,std::int32_t level,const PixelBuffer& pixels,
    std::int64_t x0,std::int64_t y0,std::int64_t x1,std::int64_t y1,const std::atomic<bool>* cancelled) {
    Sha256 digest;const char format[]="qpx.rgba-pyramid-origin-zero/1";
    digest.update(format,sizeof(format)-1);digest.integer(metadata.width);digest.integer(metadata.height);
    const auto& l=metadata.levels.at(static_cast<std::size_t>(level));std::uint64_t bits;
    static_assert(sizeof(bits)==sizeof(l.downsample));std::memcpy(&bits,&l.downsample,sizeof(bits));
    digest.integer(level);digest.integer(l.width);digest.integer(l.height);digest.integer(bits);
    for(auto n:{x0,y0,x1,y1}) digest.integer(n);
    const auto& bytes=pixels.bytes();
    for(std::size_t at=0;at<bytes.size();) {
        pyramid_checkpoint(cancelled);const auto n=std::min<std::size_t>(65536,bytes.size()-at);
        digest.update(bytes.data()+at,n);at+=n;
    }
    return digest.finish();
}
}
