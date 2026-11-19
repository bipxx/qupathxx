#include "qupathxx/label_mask_frame.hpp"
#include "label_analysis_internal.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
constexpr int time_axis = 0, channel_axis = 1, z_axis = 2, y_axis = 3, x_axis = 4;
void require(bool valid, const char* message) {
    if (!valid)
        throw std::invalid_argument(message);
}
int axis_index(const std::string& name) {
    if (name == "t")
        return time_axis;
    if (name == "c")
        return channel_axis;
    if (name == "z")
        return z_axis;
    if (name == "y")
        return y_axis;
    if (name == "x")
        return x_axis;
    throw std::invalid_argument("Label-mask frame has an unsupported axis name");
}
double unit_factor(const std::string& unit, bool time) {
    if (unit.empty())
        return 1;
    // This is the already-admitted native NGFF unit vocabulary. No aliases,
    // omitted-unit defaults or conversions through display calibration.
    if (time) {
        if (unit == "second")
            return 1;
        if (unit == "millisecond")
            return .001;
        if (unit == "microsecond")
            return .000001;
        if (unit == "minute")
            return 60;
        if (unit == "hour")
            return 3600;
    } else {
        if (unit == "micrometer")
            return 1;
        if (unit == "nanometer")
            return .001;
        if (unit == "millimeter")
            return 1000;
        if (unit == "centimeter")
            return 10000;
        if (unit == "meter")
            return 1000000;
    }
    throw std::invalid_argument("Label-mask frame has an unsupported declared axis unit");
}
struct Axis {
    const LabelMaskAxisFrame* descriptor = nullptr;
    double scale = 1, translation = 0;
    bool physical = false;
};
using Frame = std::array<Axis, 5>;
Frame prepare_frame(const LabelMaskFrame& source, bool label) {
    require(source.axes.size() >= 2 && source.axes.size() <= 5,
            "Label-mask frame requires2..5 ordered axes");
    Frame result;
    int previous = -1;
    for (const auto& axis : source.axes) {
        const auto index = axis_index(axis.name);
        require(index > previous, "Label-mask frame axes repeat or violate t,c,z,y,x order");
        previous = index;
        require(axis.extent > 0 && std::isfinite(axis.scale) && axis.scale > 0 &&
                    std::isfinite(axis.translation),
                "Label-mask frame has an invalid extent, scale or translation");
        if (index == x_axis || index == y_axis)
            require(axis.extent <= std::uint64_t(INT64_MAX),
                    "Label-mask spatial extent exceeds the Int64 geometry domain");
        else if (index == channel_axis) {
            require(axis.extent <= (label ? 1u : 4096u),
                    "Label C must be singleton and reference C must fit4096 channels");
            require(axis.unit.empty() && axis.scale == 1 && axis.translation == 0,
                    "Label-mask channel transform must be unitless identity");
        } else
            require(axis.extent <= UINT32_MAX,
                    "Label-mask Z/T extent exceeds the UInt32 plane-count domain");
        const auto factor = unit_factor(axis.unit, index == time_axis);
        auto& converted = result[std::size_t(index)];
        converted.descriptor = &axis;
        converted.physical = !axis.unit.empty();
        converted.scale = axis.scale * factor;
        converted.translation = axis.translation * factor;
        require(std::isfinite(converted.scale) && converted.scale > 0 &&
                    std::isfinite(converted.translation) &&
                    (axis.translation == 0 || converted.translation != 0),
                "Label-mask unit conversion is not finite and representable");
    }
    require(result[x_axis].descriptor && result[y_axis].descriptor,
            "Label-mask frame requires trailing y,x spatial axes");
    return result;
}
bool relative_equal(double a, double b) {
    const auto ratio = a / b;
    return std::isfinite(ratio) && ratio > 0 &&
           std::abs(ratio - 1) <=
               label_mask_frame_relative_tolerance * std::max(1., std::abs(ratio));
}
void require_compatible_units(const Axis& label, const Axis& reference) {
    require(label.physical == reference.physical,
            "Label/reference axes require both unitless or explicitly compatible physical units");
}
double translation_offset(const Axis& label, const Axis& reference) {
    const auto difference = label.translation - reference.translation;
    const auto offset = difference / reference.scale;
    require(std::isfinite(difference) && std::isfinite(offset) &&
                (difference == 0 || offset != 0),
            "Label-mask relative translation is not finite and representable");
    return offset;
}
std::int64_t spatial_origin(const Axis& label, const Axis& reference) {
    require_compatible_units(label, reference);
    require(relative_equal(label.scale, reference.scale),
            "Label/reference pixel scales differ; label resizing is unsupported");
    const auto offset = translation_offset(label, reference);
    require(std::abs(offset) <= double(label_mask_frame_origin_limit),
            "Label-mask origin exceeds the exact native translation domain");
    const auto integer = std::round(offset);
    require(std::abs(offset - integer) <= label_mask_frame_index_tolerance,
            "Label-mask origin is not aligned to the reference pixel lattice");
    return std::int64_t(integer); // bounded by2^52 before conversion
}
void calibration(const Axis& reference, const std::optional<double>& target) {
    require(!target || (std::isfinite(*target) && *target > 0),
            "Target label-mask axis calibration must be finite and positive");
    require(reference.physical == bool(target),
            "Reference and target must declare the same physical calibration availability");
    require(!target || relative_equal(reference.scale, *target),
            "Reference and target axis calibration differ");
}
void plane(const Axis& label, const Axis& reference, std::uint32_t label_index,
           std::uint32_t reference_index, std::uint32_t target_index) {
    require(reference_index == target_index,
            "Declared reference Z/T plane differs from the target image plane");
    require(reference.descriptor ? reference_index < reference.descriptor->extent
                                 : reference_index == 0,
            "Reference Z/T selection is outside its declared frame");
    if (!label.descriptor) {
        require(label_index == 0, "An absent label Z/T axis requires source index0");
        return; // The caller explicitly declares the reference plane for a2D mask.
    }
    require(label_index < label.descriptor->extent,
            "Label Z/T selection is outside its declared frame");
    // An absent reference axis retains Axis's unitless singleton identity frame.
    require_compatible_units(label, reference);
    const auto ratio = label.scale / reference.scale;
    require(std::isfinite(ratio) && ratio > 0,
            "Label/reference Z/T scale ratio is not finite and representable");
    const auto offset = translation_offset(label, reference);
    const auto mapped = std::fma(ratio, double(label_index), offset);
    require(std::isfinite(mapped) &&
                std::abs(mapped - double(reference_index)) <= label_mask_frame_index_tolerance,
            "Selected label/reference Z/T coordinates do not coincide");
}
std::int64_t source_origin(std::int64_t target, std::int64_t origin) {
    if (origin < 0)
        require(target <= INT64_MAX + origin,
                "Label-mask source-crop coordinate subtraction overflows Int64");
    // target is nonnegative, and positive origins are at most2^52, so the
    // remaining subtraction cannot underflow. Negative origins were checked above.
    const auto source = target - origin;
    require(source >= 0, "Target crop starts before the label-mask lattice");
    return source;
}
void source_bounds(std::int64_t source, std::int64_t size, std::uint64_t extent) {
    const auto start = std::uint64_t(source);
    require(start <= extent && std::uint64_t(size) <= extent - start,
            "Target crop is not wholly inside the label-mask lattice");
}
} // namespace
LabelMaskCropLink validate_link_label_mask_frames(const LabelMaskFrame& label,
                                                   const LabelMaskFrame& reference,
                                                   const LabelMaskPlaneSelection& selection,
                                                   const LabelMaskTargetFrame& target) {
    const auto labels = prepare_frame(label, true);
    const auto image = prepare_frame(reference, false);
    require(target.width > 0 && target.height > 0,
            "Target label-mask image requires positive Int64 dimensions");
    require(image[x_axis].descriptor->extent == std::uint64_t(target.width) &&
                image[y_axis].descriptor->extent == std::uint64_t(target.height),
            "Reference frame full dimensions differ from the declared target image");
    detail::label_pixels(target.crop.width, target.crop.height);
    require(target.crop.level == 0 && target.crop.x >= 0 && target.crop.y >= 0 &&
                target.crop.x <= target.width && target.crop.y <= target.height &&
                target.crop.width <= target.width - target.crop.x &&
                target.crop.height <= target.height - target.crop.y,
            "Target label-mask crop must be a level-zero window inside the full image");
    calibration(image[x_axis], target.pixel_width_um);
    calibration(image[y_axis], target.pixel_height_um);
    if (image[z_axis].descriptor)
        calibration(image[z_axis], target.z_spacing_um);
    else
        require(!target.z_spacing_um ||
                    (std::isfinite(*target.z_spacing_um) && *target.z_spacing_um > 0),
                "Target label-mask Z spacing must be finite and positive");
    if (image[time_axis].descriptor)
        calibration(image[time_axis], target.time_interval_seconds);
    else
        require(!target.time_interval_seconds ||
                    (std::isfinite(*target.time_interval_seconds) && *target.time_interval_seconds > 0),
                "Target label-mask time interval must be finite and positive");
    const auto origin_x = spatial_origin(labels[x_axis], image[x_axis]);
    const auto origin_y = spatial_origin(labels[y_axis], image[y_axis]);
    plane(labels[z_axis], image[z_axis], selection.label_z, selection.reference_z, target.z);
    plane(labels[time_axis], image[time_axis], selection.label_t, selection.reference_t, target.t);
    const auto source_x = source_origin(target.crop.x, origin_x);
    const auto source_y = source_origin(target.crop.y, origin_y);
    source_bounds(source_x, target.crop.width, labels[x_axis].descriptor->extent);
    source_bounds(source_y, target.crop.height, labels[y_axis].descriptor->extent);
    return {{source_x, source_y, 0, target.crop.width, target.crop.height}, origin_x, origin_y};
}
} // namespace qpx
