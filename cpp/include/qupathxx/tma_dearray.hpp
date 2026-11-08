#pragma once
#include "qptma.hpp"
#include "thumbnail.hpp"

namespace qpx {
enum class TmaPolarity { DarkOnLight, LightOnDark };
enum class TmaCoreUnits { Pixels, Micrometers };

// Pixel sizes use level-zero coordinates. Physical calibration is never guessed.
struct TmaDearrayOptions {
    std::size_t rows=3,columns=4;
    double core_width=80,core_height=80,bounds_scale=1.05;
    double missing_density=.05,min_valid_fraction=.95;
    double min_component_area=.4,max_component_area=2.0,min_component_extent=.45;
    double max_fit_error=.25,max_rotation_degrees=25;
    TmaPolarity polarity=TmaPolarity::DarkOnLight;
    TmaCoreUnits core_units=TmaCoreUnits::Pixels;
    // Micrometer requests may capture an explicit complete calibration pair.
    // Otherwise a server request uses its reader metadata; preview-only calls reject.
    std::optional<double> pixel_width_um,pixel_height_um;
    ThumbnailOptions thumbnail{1024,1024,4*1024*1024,1024*1024};
    std::size_t max_candidates=10000,work_bytes=64*1024*1024;
    std::uint64_t pair_checks=50000000;
    std::string name="Detected TMA";
    void validate() const;
};
struct TmaDearrayCoreSize {
    TmaCoreUnits units=TmaCoreUnits::Pixels;
    double width=0,height=0,width_pixels=0,height_pixels=0;
    std::optional<double> pixel_width_um,pixel_height_um;
    bool calibration_from_reader=false;
};
struct TmaDearrayCore {
    Point center;
    double tissue_density=0,valid_fraction=0;
    bool observed=false;
    std::optional<double> fit_error; // Core-diameter units; absent for inferred slots.
};
struct TmaDearrayResult {
    QptmaData grid;
    std::vector<TmaDearrayCore> cores;
    std::int64_t image_width=0,image_height=0,preview_width=0,preview_height=0;
    std::size_t candidates=0,estimated_work_bytes=0;
    double lattice_rmse=0;
    std::uint8_t threshold=0;
    std::string input_sha256,recipe_sha256;
    std::optional<ThumbnailPlan> thumbnail_plan;
    TmaDearrayCoreSize core_size;
    std::size_t payload_bytes() const;
};

// Owner-thread capture: honor document overrides, reject unknown or contradictory
// physical calibration, and return owning options. Does not edit the document.
TmaDearrayOptions tma_dearray_options_for_document(const Document&,TmaDearrayOptions={});

// Conservative admitted payload, including the supplied RGBA preview, masks,
// component queue, fitting scratch and returned grid. Reader caches/RSS are separate.
std::size_t tma_dearray_work_bytes(std::int64_t preview_width,std::int64_t preview_height,
                                  const TmaDearrayOptions&);
// Immutable proposal only. Blank, ambiguous or inadequately supported layouts reject.
std::shared_ptr<const TmaDearrayResult> detect_tma_grid(const PixelBuffer& preview,
    std::int64_t image_width,std::int64_t image_height,TmaDearrayOptions={},
    const std::atomic<bool>* cancelled=nullptr);
// One planned natural-level read and the existing exact area resampler. IO and
// working payload admission precede decoding; source metadata is checked before return.
std::shared_ptr<const TmaDearrayResult> detect_tma_grid(const ImageServer&,
    TmaDearrayOptions={},const std::atomic<bool>* cancelled=nullptr);
}
