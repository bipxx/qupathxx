#pragma once
#include <cstdint>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace qpx {
class Image;
struct ReaderSource;
enum class PixelType { UInt8 };
enum class PixelLayout { RGBA };
struct ImageLevel {
    std::int64_t width, height;
    double downsample;
};
struct ImageMetadata {
    std::int64_t width = 0, height = 0;
    std::vector<ImageLevel> levels;
    std::optional<double> pixel_width_um, pixel_height_um;
    std::string vendor;
    PixelType pixel_type = PixelType::UInt8;
    PixelLayout layout = PixelLayout::RGBA;
};
// x,y are level-zero coordinates; width,height count output pixels at level.
struct RegionRequest {
    std::int64_t x, y;
    std::int32_t level;
    std::int64_t width, height;
    RegionRequest(std::int64_t x_, std::int64_t y_, std::int32_t l, std::int64_t w, std::int64_t h)
        : x(x_), y(y_), level(l), width(w), height(h) {}
};
class PixelBuffer {
public:
    PixelBuffer(std::int64_t width, std::int64_t height);
    std::int64_t width() const { return width_; }
    std::int64_t height() const { return height_; }
    std::int64_t row_stride() const { return width_ * 4; }
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    std::vector<std::uint8_t>& writable_bytes() { return bytes_; }
private:
    std::int64_t width_, height_;
    std::vector<std::uint8_t> bytes_;
};
// All reads are synchronous in this packet. Buffers own their memory. OOB pixels
// are transparent black; RGBA bytes are straight alpha in device color space.
// Preserve factory-created native ownership when a server round-trips through
// nanobind. A Python-owned proxy deleter must not enter native reader/save jobs.
class ImageServer : public std::enable_shared_from_this<ImageServer> {
public:
    virtual ~ImageServer() = default;
    virtual const ImageMetadata& metadata() const = 0;
    virtual std::string reader_id() const {return "external";}
    virtual std::string source_locator() const {return {};}
    virtual std::shared_ptr<const ReaderSource> reader_source() const {return {};}
    virtual PixelBuffer read_region(const RegionRequest& request) const = 0;
    virtual void close() = 0;
    virtual bool closed() const = 0;
};
std::shared_ptr<ImageServer> dense_image_server(Image image);
const Image& dense_image(const ImageServer& server);
std::shared_ptr<ImageServer> open_slide(const std::string& path);
bool has_openslide();
// Shared validation also bounds a single output allocation to 256 MiB.
std::size_t region_byte_count(std::int64_t width, std::int64_t height);
void validate_region(const ImageMetadata&, const RegionRequest&);
// Converts OpenSlide's integer premultiplied ARGB into platform-independent bytes.
void unpack_argb(const std::vector<std::uint32_t>& input, PixelBuffer& output);
}
