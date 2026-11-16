#include "qupathxx/label_raster.hpp"
#include "label_analysis_internal.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace qpx {
namespace {
constexpr std::uint8_t magic[]{'Q', 'P', 'X', 'L', 'A', 'B', 'E', 'L', 1, 0, 0, 0};
constexpr std::size_t header = sizeof(magic) + 24, trailer = 64;
void put(std::vector<std::uint8_t>& out, std::uint64_t n, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
        out.push_back(std::uint8_t(n >> (8 * i)));
}
std::uint64_t get(const std::uint8_t* p, unsigned bytes) {
    std::uint64_t n = 0;
    for (unsigned i = 0; i < bytes; ++i)
        n |= std::uint64_t(p[i]) << (8 * i);
    return n;
}
std::string bytes_hash(const void* p, std::size_t n) {
    Sha256 h;
    h.update(p, n);
    return h.finish();
}
} // namespace
struct LabelRaster::Data {
    std::int64_t width, height;
    std::vector<std::uint32_t> labels;
    std::vector<LabelRasterRegion> regions;
    std::uint64_t foreground = 0;
    std::string hash;
};
void LabelRasterLimits::validate() const {
    detail::label_work_limit(work_bytes);
    if (!max_labels || max_labels > 1000000)
        throw std::invalid_argument("Label count limit must be1..1000000");
}
LabelRaster::LabelRaster(std::int64_t w, std::int64_t h, std::vector<std::uint32_t> labels,
                         LabelRasterLimits limits, const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    const auto n = detail::label_pixels(w, h);
    if (labels.size() != n)
        throw std::invalid_argument("Label raster sample count differs from dimensions");
    std::size_t used = sizeof(Data) + 256;
    detail::label_charge(used, labels.capacity(), sizeof(std::uint32_t), limits.work_bytes);
    std::uint32_t maximum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        maximum = std::max(maximum, labels[i]);
    }
    if (maximum > limits.max_labels || maximum > n)
        throw std::invalid_argument("Label raster exceeds admitted label count");
    detail::label_charge(used, maximum, sizeof(LabelRasterRegion), limits.work_bytes);
    auto d = std::make_shared<Data>();
    d->width = w;
    d->height = h;
    d->labels = std::move(labels);
    d->regions.resize(maximum);
    Sha256 identity;
    detail::label_hash_text(identity, "qpx.label-raster/1");
    identity.integer(w);
    identity.integer(h);
    for (std::size_t i = 0; i < n; ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        const auto id = d->labels[i];
        identity.integer(id);
        if (!id)
            continue;
        auto& r = d->regions[id - 1];
        const auto x = std::int64_t(i % std::size_t(w)), y = std::int64_t(i / std::size_t(w));
        if (!r.pixels) {
            r.label = id;
            r.left = x;
            r.top = y;
            r.right = x + 1;
            r.bottom = y + 1;
        } else {
            r.left = std::min(r.left, x);
            r.top = std::min(r.top, y);
            r.right = std::max(r.right, x + 1);
            r.bottom = std::max(r.bottom, y + 1);
        }
        ++r.pixels;
        ++d->foreground;
    }
    for (const auto& r : d->regions)
        if (!r.pixels)
            throw std::invalid_argument("Label raster contains a missing positive label");
    d->hash = identity.finish();
    data_ = std::move(d);
}
std::int64_t LabelRaster::width() const {
    return data_->width;
}
std::int64_t LabelRaster::height() const {
    return data_->height;
}
const std::vector<std::uint32_t>& LabelRaster::labels() const {
    return data_->labels;
}
const std::vector<LabelRasterRegion>& LabelRaster::regions() const {
    return data_->regions;
}
std::uint64_t LabelRaster::foreground_pixels() const {
    return data_->foreground;
}
const std::string& LabelRaster::sha256() const {
    return data_->hash;
}
std::size_t LabelRaster::payload_bytes() const {
    return sizeof(*data_) + data_->labels.capacity() * sizeof(std::uint32_t) +
           data_->regions.capacity() * sizeof(LabelRasterRegion) + data_->hash.capacity();
}
std::vector<std::uint8_t> encode_label_raster(const LabelRaster& raster, LabelRasterLimits limits,
                                              const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    if (raster.regions().size() > limits.max_labels)
        throw std::invalid_argument("Label archive exceeds label limit");
    std::size_t used = raster.payload_bytes();
    detail::label_charge(used, header + trailer, 1, limits.work_bytes);
    detail::label_charge(used, raster.labels().size(), 4, limits.work_bytes);
    std::vector<std::uint8_t> out;
    out.reserve(header + trailer + raster.labels().size() * 4);
    out.insert(out.end(), std::begin(magic), std::end(magic));
    put(out, raster.width(), 8);
    put(out, raster.height(), 8);
    put(out, raster.regions().size(), 8);
    for (std::size_t i = 0; i < raster.labels().size(); ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        put(out, raster.labels()[i], 4);
    }
    const auto hash = bytes_hash(out.data(), out.size());
    out.insert(out.end(), hash.begin(), hash.end());
    return out;
}
LabelRaster decode_label_raster(const void* bytes, std::size_t size, LabelRasterLimits limits,
                                const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    if (!bytes || size < header + trailer || size > limits.work_bytes)
        throw std::invalid_argument("Invalid label archive length");
    const auto* p = static_cast<const std::uint8_t*>(bytes);
    if (std::memcmp(p, magic, sizeof(magic)))
        throw std::invalid_argument("Unsupported label archive version");
    const auto w = get(p + sizeof(magic), 8), h = get(p + sizeof(magic) + 8, 8),
               k = get(p + sizeof(magic) + 16, 8);
    if (w > 1000000 || h > 1000000 || k > limits.max_labels)
        throw std::invalid_argument("Invalid label archive dimensions/count");
    const auto n = detail::label_pixels(std::int64_t(w), std::int64_t(h));
    if (n > (size - header - trailer) / 4 || size != header + trailer + n * 4)
        throw std::invalid_argument("Label archive length differs from dimensions");
    if (bytes_hash(p, size - trailer) !=
        std::string(reinterpret_cast<const char*>(p + size - trailer), trailer))
        throw std::invalid_argument("Label archive digest differs");
    std::size_t used = size + 4096; // file bytes remain borrowed throughout validation
    detail::label_charge(used, n, 4, limits.work_bytes);
    detail::label_charge(used, k, sizeof(LabelRasterRegion), limits.work_bytes);
    std::vector<std::uint32_t> labels;
    labels.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        const auto label = std::uint32_t(get(p + header + i * 4, 4));
        if (label > k)
            throw std::invalid_argument("Label archive sample exceeds declared label count");
        labels.push_back(label);
    }
    LabelRaster result(std::int64_t(w), std::int64_t(h), std::move(labels), limits, cancelled);
    if (result.regions().size() != k)
        throw std::invalid_argument("Label archive declared label count differs");
    return result;
}
void save_label_raster(const LabelRaster& raster, const std::string& path, LabelRasterLimits limits,
                       const std::atomic<bool>* cancelled) {
    const auto data = encode_label_raster(raster, limits, cancelled);
    detail::AtomicExportFile file(path, data.size(), cancelled);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.commit(false);
}
LabelRaster open_label_raster(const std::string& path, LabelRasterLimits limits,
                              const std::atomic<bool>* cancelled) {
    limits.validate();
    workflow_path_argument(path);
    workflow_checkpoint(cancelled);
    detail::ReaderFileGuard guard(path);
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!in)
        throw std::invalid_argument("Cannot open label archive");
    const auto end = in.tellg();
    if (end < 0 || std::uint64_t(end) > limits.work_bytes)
        throw WorkflowCaptureLimit("Label archive exceeds read budget");
    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
    in.seekg(0);
    for (std::size_t at = 0; at < data.size();) {
        workflow_checkpoint(cancelled);
        const auto count = std::min<std::size_t>(65536, data.size() - at);
        if (!in.read(reinterpret_cast<char*>(data.data() + at), count))
            throw std::invalid_argument("Truncated label archive");
        at += count;
    }
    guard.verify();
    return decode_label_raster(data.data(), data.size(), limits, cancelled);
}
} // namespace qpx
