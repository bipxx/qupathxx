#pragma once
#include "qupathxx/raw_image.hpp"
#include <atomic>

namespace qpx {
struct RawChannelIntensity {
    std::uint64_t finite_samples=0,nonfinite_samples=0;
    std::optional<double> sum,mean,minimum,maximum,sample_standard_deviation;
};
struct RawCellCompartment {
    std::uint64_t pixels=0,valid_pixels=0;
    std::vector<RawChannelIntensity> channels;
};
struct RawCellMeasurements {
    std::uint32_t label=0;
    RawCellCompartment nucleus,cell;
    // Absent for an empty geometric compartment, including expansion radius0.
    std::optional<RawCellCompartment> cytoplasm;
};
// Original HWC samples on one lattice; body includes its own nucleus and cannot
// contain a foreign nucleus. Cytoplasm is body minus ALL nuclei. Invalid padding
// is distinct from nonfinite measured samples, zero and absent compartments.
// uint64/int64 measurements outside[-2^53,2^53] refuse double precision loss.
std::vector<RawCellMeasurements> measure_raw_cell_compartments(
    const RawPixelBuffer&,const std::vector<std::uint32_t>& nuclei,
    const std::vector<std::uint32_t>& bodies,std::size_t cells,
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);

enum class FluorescenceThreshold { Explicit, Otsu256 };
enum class FluorescenceCellUnits { Pixels, Micrometres };
struct FluorescenceCellOptions {
    std::uint32_t segmentation_channel=0;
    FluorescenceThreshold threshold_method=FluorescenceThreshold::Explicit;
    double threshold=128,sigma=0,peak_prominence=.5,min_nucleus_area=1,cell_expansion=0;
    std::optional<double> max_nucleus_area;
    FluorescenceCellUnits units=FluorescenceCellUnits::Pixels;
    std::size_t max_cells=10000,work_bytes=256u*1024*1024;
    void validate() const;
};
struct FluorescenceCellBoundary {
    bool nucleus=false,cell=false;
};
struct FluorescenceCellResult {
    std::int64_t width=0,height=0;
    FluorescenceCellOptions options;
    std::array<double,2> metric{1,1};
    double resolved_threshold=0;
    std::uint64_t segmentation_finite_pixels=0,segmentation_nonfinite_pixels=0,invalid_pixels=0;
    std::vector<std::uint32_t> nuclei,bodies;
    std::vector<RawCellMeasurements> measurements;
    // A label touching raster, ROI or invalid/nonfinite segmentation support
    // is retained and flagged. The caller must not claim a complete boundary.
    std::vector<FluorescenceCellBoundary> boundaries;
    std::string input_sha256,recipe_sha256;
};
// Level-zero raw pixel lattice. domain selects the ROI; valid context outside it
// may contribute only to smoothing. Threshold uses strict >. Otsu uses256 uniform
// bins over finite smoothed ROI values, with the lowest separating-bin tie.
// Nuclei are distance-watershed splits of foreground; expansion uses the shared
// calibrated kernel's strict distance<radius rule and deterministic collisions.
// Coordinates remain local to the caller's captured raw region. This pure service
// supplies no live-document publication authority or persisted source association.
FluorescenceCellResult segment_fluorescence_cells(const RawPixelBuffer&,
    const std::vector<std::uint8_t>& domain,FluorescenceCellOptions={},
    std::optional<std::array<double,2>> micrometres_per_pixel={},
    const std::atomic<bool>* cancelled=nullptr);
}
