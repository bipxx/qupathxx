#pragma once
#include "image_server.hpp"

namespace qpx {
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
