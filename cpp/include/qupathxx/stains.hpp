#pragma once
#include "qupathxx/image_server.hpp"
#include <array>
#include <atomic>

namespace qpx {
using ColorVector = std::array<double,3>;

// 8-bit device RGB convention, with a measured per-channel white in [1,255].
double optical_density(double intensity, double white=255);

// Owned interleaved RGB OD / three-stain float32 results. No source references.
class FloatRgbImage {
public:
    std::int64_t width() const { return width_; }
    std::int64_t height() const { return height_; }
    std::int64_t row_stride() const { return width_*3*sizeof(float); }
    const std::vector<float>& values() const { return values_; }
private:
    friend class StainTransform;
    FloatRgbImage(std::int64_t width,std::int64_t height);
    std::int64_t width_,height_;
    std::vector<float> values_;
};

// Immutable, reusable and safe for concurrent transforms of immutable buffers.
// Input vectors are normalized. Two vectors derive a signed cross-product
// residual; three vectors specify the complete basis. Signed outputs are kept.
class StainTransform {
public:
    explicit StainTransform(const std::vector<ColorVector>& stains, ColorVector white={255,255,255});
    static StainTransform from_normalized(const std::array<ColorVector,3>&,ColorVector white);
    static void validate_normalized(const std::array<ColorVector,3>&,ColorVector white);
    const std::array<ColorVector,3>& stains() const { return stains_; }
    const ColorVector& white() const { return white_; }
    FloatRgbImage optical_densities(const PixelBuffer&,const std::atomic<bool>* cancelled=nullptr) const;
    FloatRgbImage deconvolve(const PixelBuffer&,const std::atomic<bool>* cancelled=nullptr) const;
private:
    StainTransform(const std::vector<ColorVector>&,ColorVector,bool normalized_input);
    FloatRgbImage transform(const PixelBuffer&,bool separate,const std::atomic<bool>*) const;
    std::array<ColorVector,3> stains_;
    ColorVector white_;
    std::array<std::array<double,256>,3> od_;
    std::array<std::array<ColorVector,256>,3> contributions_;
};
}
