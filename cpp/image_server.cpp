#include "reader_provenance.hpp"
#include "reader_file_guard.hpp"
#include <filesystem>
#include "qupathxx/image_server.hpp"
#include "qupathxx/core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace qpx {
std::size_t region_byte_count(std::int64_t w, std::int64_t h) {
    constexpr std::int64_t budget = 256 * 1024 * 1024;
    if (w <= 0 || h <= 0 || w > budget / 4 || h > budget / (4 * w))
        throw std::invalid_argument("region must have positive dimensions and fit the 256 MiB output limit");
    return static_cast<std::size_t>(w * h * 4);
}
PixelBuffer::PixelBuffer(std::int64_t w, std::int64_t h) : width_(w), height_(h), bytes_(region_byte_count(w,h), 0) {}
PixelBuffer::PixelBuffer(std::int64_t w,std::int64_t h,std::vector<std::uint8_t> rgba) : width_(w),height_(h),bytes_(std::move(rgba)) {if(bytes_.size()!=region_byte_count(w,h)) throw std::invalid_argument("expected width * height * 4 RGBA bytes");}
void validate_region(const ImageMetadata& m, const RegionRequest& r) {
    region_byte_count(r.width, r.height);
    if (r.level < 0 || static_cast<std::size_t>(r.level) >= m.levels.size()) throw std::out_of_range("invalid pyramid level");
    // Bound arithmetic in downstream readers, including level-to-level0 extents.
    const long double scale = m.levels[r.level].downsample;
    const auto limit = static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    const auto extent_x = std::ceil(r.width * scale), extent_y = std::ceil(r.height * scale);
    if (!std::isfinite(extent_x) || !std::isfinite(extent_y) || extent_x >= limit || extent_y >= limit)
        throw std::invalid_argument("region coordinate overflow");
    // Integer subtraction is exact even on MSVC, where long double is double.
    if (r.x > std::numeric_limits<std::int64_t>::max() - static_cast<std::int64_t>(extent_x) ||
        r.y > std::numeric_limits<std::int64_t>::max() - static_cast<std::int64_t>(extent_y))
        throw std::invalid_argument("region coordinate overflow");
}
void unpack_argb(const std::vector<std::uint32_t>& input, PixelBuffer& output) {
    auto& bytes = output.writable_bytes();
    if (input.size() != bytes.size() / 4) throw std::invalid_argument("ARGB buffer size mismatch");
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto p = input[i]; auto alpha = (p >> 24) & 255u;
        for (unsigned c = 0; c < 3; ++c) {
            auto v = (p >> (16 - 8 * c)) & 255u;
            bytes[i*4+c] = alpha ? static_cast<std::uint8_t>(std::min(255u, (v*255u + alpha/2) / alpha)) : 0;
        }
        bytes[i*4+3] = static_cast<std::uint8_t>(alpha);
    }
}
namespace {
class DenseImageServer final : public ImageServer {
public:
    explicit DenseImageServer(Image image,std::string locator={},std::shared_ptr<const FileVersion> version={}) : image_(std::move(image)),locator_(std::move(locator)) {
        source_=make_reader_source(locator_.empty()?"qpx.dense-rgb/1":"ppm",locator_,locator_.empty()?"memory":"single_file",std::move(version));
        metadata_.width = image_.width(); metadata_.height = image_.height();
        metadata_.levels.push_back({image_.width(), image_.height(), 1}); metadata_.vendor = "dense-rgb";
    }
    const ImageMetadata& metadata() const override { return metadata_; }
    std::string reader_id() const override {return locator_.empty()?"qpx.dense-rgb/1":"ppm";}
    std::string source_locator() const override {return locator_;}
    std::shared_ptr<const ReaderSource> reader_source() const override {return source_;}
    const Image& image() const { return image_; }
    PixelBuffer read_region(const RegionRequest& r) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) throw std::runtime_error("image server is closed");
        validate_region(metadata_, r); PixelBuffer result(r.width, r.height);
        auto& bytes = result.writable_bytes();
        for (std::int64_t row = 0; row < r.height; ++row) for (std::int64_t col = 0; col < r.width; ++col) {
            auto x = r.x + col, y = r.y + row;
            if (x < 0 || y < 0 || x >= metadata_.width || y >= metadata_.height) continue;
            auto source = static_cast<std::size_t>((y * metadata_.width + x) * 3);
            auto dest = static_cast<std::size_t>((row * r.width + col) * 4);
            std::copy_n(image_.pixels().data()+source, 3, bytes.data()+dest); bytes[dest+3] = 255;
        }
        return result;
    }
    void close() override { std::lock_guard<std::mutex> lock(mutex_); closed_ = true; }
    bool closed() const override { return closed_.load(); }
private:
    Image image_;
    std::string locator_;
    std::shared_ptr<const ReaderSource> source_;
    ImageMetadata metadata_;
    mutable std::mutex mutex_;
    std::atomic<bool> closed_{false};
};
}
std::shared_ptr<ImageServer> dense_image_server(Image image) { return std::make_shared<DenseImageServer>(std::move(image)); }
std::shared_ptr<ImageServer> ppm_image_server(const std::string& path) {
    const auto locator=std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().u8string();
    detail::ReaderFileGuard guard(locator);auto image=Image::read_ppm(locator);guard.verify();
    return std::make_shared<DenseImageServer>(std::move(image),locator,guard.version());
}
const Image& dense_image(const ImageServer& server) {
    auto* dense = dynamic_cast<const DenseImageServer*>(&server);
    if (!dense) throw std::runtime_error("desktop slide rendering requires the upcoming tiled viewer; use read_region for now");
    if (dense->closed()) throw std::runtime_error("image server is closed");
    return dense->image();
}
}
