#pragma once
#include "qupathxx/image_server.hpp"

namespace qpx::detail {
// Private, version1 opaque-RGB sampling contract. Coordinates and extents are
// integer source pixels; output shape rounds extent/nominal_downsample half-up.
// A plan is produced only by plan_rgb_nominal, then used unchanged by the kernel.
// The returned source window includes interpolation context beyond a crop.
struct RgbNominalPlan {
    std::int64_t image_width,image_height,x,y,output_width,output_height;
    std::uint64_t start,stride;
    bool nearest;
    RegionRequest window;
    bool empty() const {return window.width==0 || window.height==0;}
};
RgbNominalPlan plan_rgb_nominal(std::int64_t image_width,std::int64_t image_height,
    std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,
    double nominal_downsample,const std::atomic<bool>* cancelled=nullptr);
// The window must have exactly plan.window dimensions and opaque RGBA bytes.
// Pass nullptr only for an empty plan. Output is owned, opaque RGBA, including
// black outside-image samples. No default quantitative sampling policy changes.
PixelBuffer sample_rgb_nominal(const RgbNominalPlan&,const PixelBuffer* window,
    const std::atomic<bool>* cancelled=nullptr);
}
