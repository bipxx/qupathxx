#pragma once
#include "qupathxx/image_server.hpp"

namespace qpx {
// Geometry admission tolerances only. Original descriptors/transforms and their
// exact persisted identity remain the responsibility of the capturing workflow.
inline constexpr double label_mask_frame_relative_tolerance = 1e-12;
inline constexpr double label_mask_frame_index_tolerance = 1e-9;
// Matches the existing native OME-Zarr translation-precision domain. Extents and
// crop coordinates retain their checked integer types and never pass through double.
inline constexpr std::int64_t label_mask_frame_origin_limit = 4503599627370496LL;

struct LabelMaskAxisFrame {
    std::string name, unit;
    std::uint64_t extent = 0;
    double scale = 1, translation = 0;
};
struct LabelMaskFrame {
    // Ordered subset of t,c,z,y,x, with required trailing y,x (2..5 axes).
    // X/Y extents fit Int64; Z/T extents fit UInt32. Label C is singleton;
    // reference C may have1..4096 channels. C is unitless identity in either.
    std::vector<LabelMaskAxisFrame> axes;
};
struct LabelMaskPlaneSelection {
    std::uint32_t label_z = 0, label_t = 0, reference_z = 0, reference_t = 0;
};
struct LabelMaskTargetFrame {
    std::int64_t width = 0, height = 0;
    RegionRequest crop{0, 0, 0, 1, 1};
    std::optional<double> pixel_width_um, pixel_height_um;
    std::optional<double> z_spacing_um, time_interval_seconds;
    std::uint32_t z = 0, t = 0;
};
struct LabelMaskCropLink {
    RegionRequest source_crop{0, 0, 0, 1, 1};
    // Label pixel(0,0) lies at this integer location in the reference lattice.
    std::int64_t label_origin_x_in_reference = 0, label_origin_y_in_reference = 0;
};

// Validate and link already-composed level-zero transforms without decoding,
// resizing, padding or modifying a frame. The target crop is a nonempty native
// LabelRaster-sized window wholly inside both the reference image and label array.
// Reference full X/Y extents and effective micrometre calibration must match the
// target. Label/reference X/Y units must both be absent or explicitly convertible;
// relative scale must be1 within the relative tolerance, and the origin shift
// must be integral within the index tolerance. Physical space units admitted:
// micrometer, nanometer, millimeter, centimeter, meter. Time units admitted:
// second, millisecond, microsecond, minute, hour. Empty units remain unitless.
//
// Present reference Z/T axes also require the same explicit calibration
// availability and effective step as the target (micrometres/seconds). Absent
// reference Z/T axes do not invent an axis or forbid a valid target calibration.
// Reference selected Z/T indices must equal target Z/T. Present label axes use
// explicit in-range source indices and must select the same physical coordinate
// within the index tolerance, measured in reference index units. An absent label
// axis requires source index0 and accepts the explicitly declared reference plane;
// it never infers that plane. An absent reference axis is a unitless singleton at
// index0, so a present label axis must map coherently to that coordinate.
// Invalid descriptors, selections, geometry or calibration throw invalid_argument.
LabelMaskCropLink validate_link_label_mask_frames(const LabelMaskFrame& label,
                                                   const LabelMaskFrame& reference,
                                                   const LabelMaskPlaneSelection&,
                                                   const LabelMaskTargetFrame&);
} // namespace qpx
