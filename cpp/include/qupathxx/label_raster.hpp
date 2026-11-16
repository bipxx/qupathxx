#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace qpx {
struct LabelRasterLimits {
    std::size_t max_labels = 1000000, work_bytes = 256u * 1024 * 1024;
    void validate() const;
};
struct LabelRasterRegion {
    std::uint32_t label = 0;
    std::uint64_t pixels = 0;
    std::int64_t left = 0, top = 0, right = 0, bottom = 0;
};
// Owned, immutable local raster. Zero is background; positive labels are dense
// 1..N and each occurs. Connectivity and source/world coordinates belong to the
// producing algorithm/captured ScientificGrid, never guessed by this value type.
class LabelRaster {
  public:
    LabelRaster(std::int64_t width, std::int64_t height, std::vector<std::uint32_t> labels,
                LabelRasterLimits = {}, const std::atomic<bool>* cancelled = nullptr);
    std::int64_t width() const;
    std::int64_t height() const;
    const std::vector<std::uint32_t>& labels() const;
    const std::vector<LabelRasterRegion>& regions() const;
    std::uint64_t foreground_pixels() const;
    const std::string& sha256() const;
    std::size_t payload_bytes() const;

  private:
    struct Data;
    std::shared_ptr<const Data> data_;
};
// Canonical little-endian labels + SHA-256. Decoding validates length, digest,
// label density and work limits before publishing a value. No native structs on wire.
std::vector<std::uint8_t> encode_label_raster(const LabelRaster&, LabelRasterLimits = {},
                                              const std::atomic<bool>* cancelled = nullptr);
LabelRaster decode_label_raster(const void*, std::size_t, LabelRasterLimits = {},
                                const std::atomic<bool>* cancelled = nullptr);
void save_label_raster(const LabelRaster&, const std::string& path, LabelRasterLimits = {},
                       const std::atomic<bool>* cancelled = nullptr);
LabelRaster open_label_raster(const std::string& path, LabelRasterLimits = {},
                              const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
