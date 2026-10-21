#include "qupathxx/image_server.hpp"
#include <cmath>
#include <locale>
#include <mutex>
#include <sstream>
#include <stdexcept>
#ifdef QUPATHXX_HAS_OPENSLIDE
#include <openslide.h>

namespace qpx {
namespace {
void check(openslide_t* handle) {
    if (const char* error = openslide_get_error(handle)) throw std::runtime_error(std::string("OpenSlide: ") + error);
}
std::optional<double> calibration(openslide_t* handle, const char* key) {
    const char* value = openslide_get_property_value(handle, key); check(handle);
    if (!value) return std::nullopt;
    std::istringstream stream(value); stream.imbue(std::locale::classic()); double result;
    if (!(stream >> result) || !std::isfinite(result) || result <= 0) return std::nullopt;
    stream >> std::ws;
    return stream.eof() ? std::optional<double>(result) : std::nullopt;
}
class OpenSlideServer final : public ImageServer {
public:
    explicit OpenSlideServer(const std::string& path) {
        std::unique_ptr<openslide_t, decltype(&openslide_close)> pending(openslide_open(path.c_str()), &openslide_close);
        if (!pending) throw std::runtime_error("OpenSlide cannot open or recognize: " + path);
        check(pending.get());
        auto count = openslide_get_level_count(pending.get()); check(pending.get());
        if (count <= 0) throw std::runtime_error("OpenSlide returned no pyramid levels");
        for (std::int32_t i = 0; i < count; ++i) {
            std::int64_t w, h; openslide_get_level_dimensions(pending.get(),i,&w,&h); check(pending.get());
            double scale = openslide_get_level_downsample(pending.get(),i); check(pending.get());
            if (w <= 0 || h <= 0 || !std::isfinite(scale) || scale <= 0) throw std::runtime_error("invalid pyramid metadata");
            metadata_.levels.push_back({w,h,scale});
        }
        metadata_.width = metadata_.levels[0].width; metadata_.height = metadata_.levels[0].height;
        metadata_.pixel_width_um = calibration(pending.get(), OPENSLIDE_PROPERTY_NAME_MPP_X);
        metadata_.pixel_height_um = calibration(pending.get(), OPENSLIDE_PROPERTY_NAME_MPP_Y);
        const char* vendor = openslide_get_property_value(pending.get(), OPENSLIDE_PROPERTY_NAME_VENDOR); check(pending.get());
        metadata_.vendor = vendor ? vendor : "unknown";
        handle_ = pending.release();
    }
    ~OpenSlideServer() override { if (handle_) openslide_close(handle_); }
    const ImageMetadata& metadata() const override { return metadata_; }
    PixelBuffer read_region(const RegionRequest& r) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!handle_) throw std::runtime_error("image server is closed");
        validate_region(metadata_,r); PixelBuffer result(r.width,r.height);
        std::vector<std::uint32_t> argb(result.bytes().size()/4);
        openslide_read_region(handle_,argb.data(),r.x,r.y,r.level,r.width,r.height); check(handle_);
        unpack_argb(argb,result); return result;
    }
    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle_) { openslide_close(handle_); handle_ = nullptr; }
    }
    bool closed() const override { std::lock_guard<std::mutex> lock(mutex_); return !handle_; }
private:
    openslide_t* handle_ = nullptr;
    ImageMetadata metadata_;
    mutable std::mutex mutex_;
};
}
bool has_openslide() { return true; }
std::shared_ptr<ImageServer> open_slide(const std::string& path) { return std::make_shared<OpenSlideServer>(path); }
}
#else
namespace qpx {
bool has_openslide() { return false; }
std::shared_ptr<ImageServer> open_slide(const std::string&) { throw std::runtime_error("rebuild with QUPATHXX_BUILD_OPENSLIDE=ON and OpenSlide_ROOT pointing to an SDK"); }
}
#endif
