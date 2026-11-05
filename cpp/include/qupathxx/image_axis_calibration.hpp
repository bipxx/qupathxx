#pragma once
#include <cstddef>
#include <optional>
#include <vector>
namespace qpx {
// Owned physical interpretation of image axes, separate from reader acquisition
// metadata. Presence with unknown values is distinct from no document override.
class ImageAxisCalibration {
public:
    static constexpr std::size_t max_timepoints=65536;
    explicit ImageAxisCalibration(std::optional<double> z_spacing_um={},std::vector<double> timepoints_seconds={});
    std::optional<double> z_spacing_um() const {return z_;}
    const std::vector<double>& timepoints_seconds() const {return times_;}
    std::optional<double> timepoint_seconds(std::size_t index) const;
    bool equals(const ImageAxisCalibration& b) const {return z_==b.z_ && times_==b.times_;}
    std::size_t payload_bytes() const {return sizeof(*this)+times_.size()*sizeof(double);}
private:
    std::optional<double> z_;
    std::vector<double> times_;
};
}
