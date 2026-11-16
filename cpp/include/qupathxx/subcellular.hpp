#pragma once
#include "qupathxx/label_raster.hpp"
#include "qupathxx/fluorescence_cells.hpp"

namespace qpx {
enum class SpotConnectivity { Four, Eight };
struct SubcellularOptions {
    std::uint32_t channel = 0;
    double threshold = 128, sigma_x = 0, sigma_y = 0, background_sigma_x = 0,
           background_sigma_y = 0;
    std::uint64_t min_pixels = 1, max_pixels = 10000;
    SpotConnectivity connectivity = SpotConnectivity::Eight;
    std::size_t max_spots = 100000, work_bytes = 256u * 1024 * 1024;
    void validate() const;
};
struct SubcellularSpot {
    std::uint32_t label = 0, parent_label = 0;
    RawCellCompartment raw;
};
struct SubcellularCellCount {
    std::uint32_t parent_label = 0;
    std::uint64_t spots = 0, spot_pixels = 0;
};
class SubcellularResult {
  public:
    const LabelRaster& raster() const {
        return raster_;
    }
    const SubcellularOptions& options() const {
        return options_;
    }
    const std::vector<SubcellularSpot>& spots() const {
        return spots_;
    }
    const std::vector<SubcellularCellCount>& cells() const {
        return cells_;
    }
    const std::string& input_sha256() const {
        return input_;
    }
    const std::string& recipe_sha256() const {
        return recipe_;
    }
    const std::string& sha256() const {
        return result_;
    }
    std::uint64_t rejected_small() const {
        return small_;
    }
    std::uint64_t rejected_large() const {
        return large_;
    }
    std::uint64_t nonfinite_pixels() const {
        return nonfinite_;
    }
    std::uint64_t invalid_pixels() const {
        return invalid_;
    }

  private:
    LabelRaster raster_;
    SubcellularOptions options_;
    std::vector<SubcellularSpot> spots_;
    std::vector<SubcellularCellCount> cells_;
    std::string input_, recipe_, result_;
    std::uint64_t small_ = 0, large_ = 0, nonfinite_ = 0, invalid_ = 0;
    SubcellularResult(LabelRaster, SubcellularOptions);
    friend SubcellularResult detect_subcellular_spots(const RawPixelBuffer&, const LabelRaster&,
                                                      SubcellularOptions, const std::atomic<bool>*);
    friend struct LabelAnalysisKernel;
};
// Thresholded connected spots within the supplied parent-cell labels; adjacent
// different parents never merge. Optional nearest-edge ImageJ float32 Gaussian/
// difference-of-Gaussian processing is explicit, in processing-pixel units.
// Gaussian mode requires a wholly valid finite crop; unfiltered mode excludes
// invalid/nonfinite detection samples and reports their counts. Raw measurements
// always use original values via the shared native raw-intensity accumulator.
// Oversized components are rejected/reported, never relabeled as one small spot
// or silently split into an estimated biological count. Parent zero is outside.
SubcellularResult detect_subcellular_spots(const RawPixelBuffer&, const LabelRaster& parents,
                                           SubcellularOptions = {},
                                           const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
