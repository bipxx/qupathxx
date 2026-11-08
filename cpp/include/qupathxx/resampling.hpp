#pragma once
#include "image_server.hpp"

namespace qpx {
enum class RgbNominalSampling { IntegerOrigin=0, AffineInverse=1 };
// Immutable source rectangle in level-zero pixels. Downsample is a dimensionless
// nominal source-pixels/output-pixel ratio; output shape rounds each extent/
// downsample half-up. Construction validates dimensions, finite positive ratio
// and buffer admission without reading pixels. This is an explicit RGB policy.
struct RgbNominalRequest {
    const std::int64_t x_px,y_px,width_px,height_px;
    const double downsample;
    const RgbNominalSampling sampling;
    RgbNominalRequest(std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,double step=1,
        RgbNominalSampling sampling=RgbNominalSampling::IntegerOrigin);
    const char* algorithm() const noexcept {return sampling==RgbNominalSampling::AffineInverse?"qpx.rgb-nominal-affine/1":"qpx.rgb-nominal/1";}
};
// Read the bounded source window, including interpolation context, at unit
// level zero. Source pixels must be opaque RGBA8. Returned pixels own their
// storage; outside-image centers are opaque black. Decode failures and invalid
// windows propagate without returning partial pixels. Cancellation cannot
// interrupt the underlying synchronous reader. Default area/scientific policies
// and archive/replay authority are unchanged by this separate operation.
PixelBuffer read_rgb_nominal_region(const ImageServer&,const RgbNominalRequest&,
    const std::atomic<bool>* cancelled=nullptr);

// Exact box-area filtering in source pixel coordinates, including enlargement.
// RGBA8 straight alpha is accumulated in premultiplied form; transparent output
// is canonical black. Each buffer <=256 MiB; at most 2^28 overlap visits.
PixelBuffer resample_area(const PixelBuffer&,std::int64_t width,std::int64_t height,
                          const std::atomic<bool>* cancelled=nullptr);
// Request remains on an explicit natural pyramid level. The complete requested
// rectangle maps to width*height output; no implicit level selection or crop.
PixelBuffer read_resampled_region(const ImageServer&,const RegionRequest&,
    std::int64_t width,std::int64_t height,const std::atomic<bool>* cancelled=nullptr);
}
