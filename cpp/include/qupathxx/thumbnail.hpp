#pragma once
#include "image_server.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace qpx {
struct ThumbnailOptions {
    std::int32_t max_width=256,max_height=256;
    std::uint64_t source_pixel_limit=4*1024*1024,output_pixel_limit=1024*1024;
    void validate() const;
};
struct ThumbnailPlan {
    std::int32_t level=0;
    std::int64_t width=0,height=0,source_width=0,source_height=0;
    std::uint64_t source_pixels=0,output_pixels=0;
    double level_downsample=1,downsample_x=1,downsample_y=1;
};
struct ThumbnailResult {
    ThumbnailPlan plan;
    PixelBuffer pixels;
    double elapsed_ms=0;
    ThumbnailResult(ThumbnailPlan p,PixelBuffer image,double ms):plan(p),pixels(std::move(image)),elapsed_ms(ms) {}
};
// No upscaling. Area filtering covers a complete adequate natural pyramid level.
// Both allocations are planned/bounded before one origin-zero region read.
ThumbnailPlan plan_thumbnail(const ImageMetadata&,ThumbnailOptions={});
std::shared_ptr<const ThumbnailResult> read_thumbnail(const ImageServer&,ThumbnailOptions={},const std::atomic<bool>* cancelled=nullptr);
struct ThumbnailStats {
    std::string state="idle",error;
    std::uint64_t completed=0,source_pixels=0,output_pixels=0;
    double elapsed_ms=0;
};
// Single-owner controller. Cancellation cannot interrupt a synchronous decode;
// it prevents publication and is checked throughout the area filter.
class ThumbnailJob {
public:
    ThumbnailJob();~ThumbnailJob();ThumbnailJob(const ThumbnailJob&)=delete;
    bool request(std::shared_ptr<ImageServer>,ThumbnailOptions={});
    ThumbnailStats poll();ThumbnailStats wait();void cancel();bool busy() const;
    std::shared_ptr<const ThumbnailResult> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
