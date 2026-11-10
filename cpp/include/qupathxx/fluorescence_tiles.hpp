#pragma once
#include "qupathxx/fluorescence_cells.hpp"
namespace qpx {
struct FluorescencePixelSpan {
    std::int64_t y=0,x=0,width=0;
};
struct FluorescenceOwnedCell {
    // Stable across tile layouts/order: source/plane/recipe and first global
    // row-major nucleus pixel. Local watershed labels are never global IDs.
    std::string id;
    std::int64_t anchor_x=0,anchor_y=0;
    RawCellMeasurements measurements;
    std::vector<FluorescencePixelSpan> nucleus,cell;
};
struct FluorescenceTileResult {
    std::vector<FluorescenceOwnedCell> cells;
    // Includes cut foreground before area filtering, even if no label survives.
    std::uint64_t unresolved_foreground_pixels_in_core=0;
    std::uint64_t finite_segmentation_pixels_in_core=0;
    std::vector<std::uint32_t> unresolved_labels;
    std::string input_sha256,recipe_sha256;
    std::size_t accounted_work_bytes=0;
    bool complete() const{return !unresolved_foreground_pixels_in_core && unresolved_labels.empty();}
};
// One bounded raw tile, with an expanded segmentation domain and half-open
// ownership core in global level-zero coordinates. samples describes the raw
// buffer, analysis_bounds the whole selected ROI bounding box. Domain is zero
// outside that ROI. Interior margins must cover Gaussian support +2*expansion.
// Callers resolve any global Otsu/normalization once before this explicit-
// threshold operation; per-tile Otsu refuses. source_sha256 must bind the source
// and coordinate calibration. IDs also bind channel block/Z/T and the recipe.
// Unresolved candidates are reported, never returned as complete cells. A caller
// must not publish a successful complete tile when complete()==false.
FluorescenceTileResult segment_fluorescence_tile(const RawPixelBuffer&,
    const std::vector<std::uint8_t>& domain,const RawRegionRequest& samples,
    RegionRequest ownership_core,RegionRequest analysis_bounds,const std::string& source_sha256,
    FluorescenceCellOptions={},std::optional<std::array<double,2>> micrometres_per_pixel={},
    const std::atomic<bool>* cancelled=nullptr);
}
