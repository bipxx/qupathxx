#pragma once
#include "qupathxx/label_raster.hpp"
#include "qupathxx/fluorescence_cells.hpp"
namespace qpx {
struct LabelRegionMeasurements {
    std::uint32_t label = 0;
    std::uint64_t pixels = 0, valid_pixels = 0;
    std::vector<RawChannelIntensity> channels;
};
// Shares the native raw intensity accumulator/exact numeric admission with cell
// compartments, without inventing nucleus/body semantics for generic regions.
std::vector<LabelRegionMeasurements>
measure_raw_label_regions(const RawPixelBuffer&, const LabelRaster&,
                          std::size_t work_bytes = 256u * 1024 * 1024,
                          const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
