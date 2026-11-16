// SPDX-License-Identifier: AGPL-3.0-only
#include "omero_mask.hpp"
#include "qupathxx/label_geometry.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_registry.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
namespace qpx::detail {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
std::int64_t coordinate(const Json& j) {
    if (!j.is_number())
        throw std::invalid_argument("Mask coordinates must be numeric");
    const auto d = j.get<double>();
    if (!std::isfinite(d) || d != std::floor(d) || d < 0 || d > 1000000000000.)
        throw std::invalid_argument("Mask raster requires nonnegative integral image coordinates");
    return std::int64_t(d);
}
std::string hash(const std::vector<std::uint8_t>& b) {
    Sha256 h;
    h.update(b.data(), b.size());
    return h.finish();
}
class TemporaryPng {
    fs::path directory, file;

  public:
    explicit TemporaryPng(const std::vector<std::uint8_t>& bytes) {
        std::random_device r;
        for (unsigned i = 0; i < 32; ++i) {
            directory = fs::temp_directory_path() /
                        ("qpx-omero-mask-" + std::to_string(r()) + "-" + std::to_string(r()));
            if (fs::create_directory(directory)) {
                file = directory / "mask.png";
                break;
            }
        }
        if (file.empty())
            throw std::runtime_error("Cannot create private mask decoder input");
        std::ofstream f(file, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        f.close();
        if (!f) {
            std::error_code e;
            fs::remove(file, e);
            fs::remove(directory, e);
            throw std::runtime_error("Cannot write mask decoder input");
        }
    }
    ~TemporaryPng() {
        std::error_code e;
        fs::remove(file, e);
        fs::remove(directory, e);
    }
    std::string path() const {
        return file.u8string();
    }
};
} // namespace
std::uint32_t omero_rgba_int(const Json& j) {
    if (j.is_number_unsigned()) {
        const auto n = j.get<std::uint64_t>();
        if (n <= INT32_MAX)
            return std::uint32_t(n);
    } else if (j.is_number_integer()) {
        const auto n = j.get<std::int64_t>();
        if (n >= INT32_MIN && n <= INT32_MAX)
            return std::uint32_t(n);
    }
    throw std::invalid_argument("OMERO color must be an exact signed RGBA Int32");
}
std::size_t omero_mask_pixels(const Json& j, const OmeroImageInfo& image) {
    const auto x = coordinate(j.at("X")), y = coordinate(j.at("Y")), w = coordinate(j.at("Width")),
               h = coordinate(j.at("Height"));
    if (!w || !h || w > 1000000 || h > 1000000 || std::uint64_t(x) > image.width ||
        std::uint64_t(y) > image.height || std::uint64_t(w) > image.width - x ||
        std::uint64_t(h) > image.height - y ||
        std::uint64_t(w) > 16u * 1024 * 1024 / std::uint64_t(h))
        throw std::invalid_argument("OMERO mask exceeds its original-image raster/budget domain");
    return std::size_t(w * h);
}
void decode_omero_mask(OmeroShape& shape, const OmeroImageInfo& image,
                       const std::vector<std::uint8_t>& png, const std::atomic<bool>* cancelled) {
    const auto j = Json::parse(shape.source_json);
    const auto count = omero_mask_pixels(j, image);
    const auto x = coordinate(j.at("X")), y = coordinate(j.at("Y")), w = coordinate(j.at("Width")),
               h = coordinate(j.at("Height"));
    const std::uint32_t color =
        j.contains("FillColor") ? omero_rgba_int(j.at("FillColor")) : 0xffff00ffu;
    const std::array<std::uint8_t, 4> foreground = {std::uint8_t(color >> 24),
                                                    std::uint8_t(color >> 16),
                                                    std::uint8_t(color >> 8), std::uint8_t(color)};
    if (!foreground[3])
        throw std::invalid_argument(
            "Transparent OMERO mask rendering cannot establish binary coverage");
    TemporaryPng temporary(png);
    ReaderFileGuard guard(temporary.path());
    FileFingerprintOptions hashing;
    hashing.byte_limit = png.size();
    hashing.buffer_bytes = 65536;
    if (fingerprint_file(temporary.path(), hashing, cancelled)->sha256 != hash(png))
        throw WorkflowInputChanged("Mask decoder input differs from owned bytes");
    guard.verify();
    const auto selected = select_reader(temporary.path(), "org.qupathxx.png");
    const auto reader = open_reader(temporary.path(), selected, {}, cancelled);
    const auto meta = reader->raw_metadata(cancelled);
    if (!meta || meta->width != w || meta->height != h || meta->channels != 4 ||
        meta->sample_type != SampleType::UInt8)
        throw std::invalid_argument("OMERO mask endpoint must return exact-size RGBA8 PNG samples");
    const auto raw = reader->read_raw({{0, 0, 0, w, h}, 0, 4, 0, 0}, cancelled);
    const auto& samples = std::get<std::vector<std::uint8_t>>(raw.storage());
    std::vector<std::uint32_t> labels(count);
    shape.mask_bits.resize((count + 7) / 8);
    bool any = false;
    for (std::size_t i = 0; i < count; ++i) {
        if ((i & 65535) == 0)
            workflow_checkpoint(cancelled);
        if (!raw.valid()[i])
            throw std::invalid_argument("OMERO mask contains invalid PNG samples");
        const auto* p = samples.data() + 4 * i;
        if (std::equal(foreground.begin(), foreground.end(), p)) {
            labels[i] = 1;
            shape.mask_bits[i / 8] |= std::uint8_t(128u >> (i % 8));
            any = true;
        } else if (p[0] || p[1] || p[2] || p[3])
            throw std::invalid_argument(
                "OMERO mask PNG has unexpected colors/alpha; binary coverage is ambiguous");
    }
    if (!any)
        throw std::invalid_argument("Empty OMERO mask has no native area geometry");
    guard.verify();
    const LabelRaster raster(w, h, std::move(labels));
    const ScientificGrid grid(std::int64_t(image.width), std::int64_t(image.height), {x, w, w},
                              {y, h, h});
    const auto contours = trace_label_raster(
        raster, grid, Roi::rectangle({double(x), double(y), double(w), double(h)}), {}, cancelled);
    if (contours->components().empty())
        throw std::invalid_argument("OMERO mask contour has no area");
    if (contours->components().size() == 1)
        shape.geometry = contours->components()[0].geometry;
    else {
        std::vector<PolygonPart> parts;
        for (const auto& c : contours->components()) {
            const auto p = c.geometry.polygon_parts();
            parts.insert(parts.end(), p.begin(), p.end());
        }
        shape.geometry = Roi::multipolygon(std::move(parts));
    }
    if (shape.geometry.area() != double(raster.foreground_pixels()))
        throw std::invalid_argument("OMERO mask contour does not conserve exact pixel area");
    shape.mask_png_sha256 = hash(png);
    const auto catalog = reader_catalog();
    const auto found = std::find_if(catalog.begin(), catalog.end(), [](const auto& d) {
        return d.provider_id == "org.qupathxx.png";
    });
    if (found == catalog.end() || !found->available || found->module_sha256.size() != 64)
        throw std::invalid_argument("PNG plugin identity unavailable");
    shape.mask_decoder = found->provider_id + ":" + found->version + ":" + found->contract + ":" +
                         found->module_sha256;
}
Json omero_mask_value(const OmeroShape& shape) {
    if (shape.mask_bits.empty())
        return nullptr;
    static constexpr char hex[] = "0123456789abcdef";
    std::string bits;
    bits.reserve(shape.mask_bits.size() * 2);
    for (auto b : shape.mask_bits) {
        bits += hex[b >> 4];
        bits += hex[b & 15];
    }
    return {{"schema", "qpx.omero-binary-mask/1"},
            {"representation", "style-verified-binary-coverage-from-mask-png"},
            {"packing", "row-major-msb-first-zero-tail"},
            {"bits_hex", bits},
            {"sha256", hash(shape.mask_bits)},
            {"png_sha256", shape.mask_png_sha256},
            {"decoder", shape.mask_decoder}};
}
} // namespace qpx::detail
