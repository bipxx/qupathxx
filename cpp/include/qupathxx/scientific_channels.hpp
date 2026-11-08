#pragma once
#include "scientific_sampling.hpp"
#include "stains.hpp"
namespace qpx {
enum class ScientificChannelMode { OpticalDensity, Stains };
class ScientificChannels {
    ScientificGrid grid_;
    ScientificChannelMode mode_;
    ColorVector white_;
    std::array<ColorVector,3> stains_;
    std::vector<double> values_;
    std::vector<std::uint8_t> valid_;
    std::size_t work_bytes_;
    ScientificChannels(const ScientificRgbGrid&,const StainTransform&,ScientificChannelMode,std::size_t);
    friend struct ScientificChannelKernel;
public:
    const ScientificGrid& grid() const {return grid_;}
    ScientificChannelMode mode() const {return mode_;}
    const char* algorithm() const {return mode_==ScientificChannelMode::Stains?"qpx.grid-rgb-stains/1":"qpx.grid-rgb-od/1";}
    const ColorVector& white() const {return white_;}
    const std::array<ColorVector,3>& stains() const {return stains_;}
    const std::vector<double>& values() const {return values_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    std::int64_t width() const {return grid_.width();}
    std::int64_t height() const {return grid_.height();}
    std::size_t estimated_work_bytes() const {return work_bytes_;}
};
// Float64 means -> OD -> signed stains. Invalid centers retain zero + validity.
// Admission includes input/output arrays and the supplied transform's storage.
std::uint64_t scientific_channel_work_bytes(const ScientificRgbGrid&);
std::uint64_t scientific_channel_work_bytes(const ScientificGrid&);
ScientificChannels scientific_optical_densities(const ScientificRgbGrid&,const StainTransform&,
    std::size_t work_bytes=256ULL*1024*1024,const std::atomic<bool>* cancelled=nullptr);
ScientificChannels scientific_deconvolve(const ScientificRgbGrid&,const StainTransform&,
    std::size_t work_bytes=256ULL*1024*1024,const std::atomic<bool>* cancelled=nullptr);
}
