#pragma once
#include "qupathxx/scientific_raw_sampling.hpp"
#include "qupathxx/classifier.hpp"

namespace qpx {
struct RawPixelFeatureScale {double x=0,y=0;};
struct RawPixelFeatureOptions {
    std::vector<double> offsets,scales;
    std::vector<RawPixelFeatureScale> sigmas{{0,0}};
    bool gradient_magnitude=false,laplacian=false;
    std::size_t work_bytes=256u*1024*1024;
    void validate(std::uint32_t channels) const;
};
}
