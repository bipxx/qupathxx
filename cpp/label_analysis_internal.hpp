#pragma once
#include "qupathxx/raw_image.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <cstring>
#include <type_traits>

namespace qpx::detail {
inline void label_work_limit(std::size_t work) {
    if (work < 4096 || work > 1024ull * 1024 * 1024)
        throw std::invalid_argument("Label analysis requires a4096-byte..1GiB work budget");
}
inline std::size_t label_pixels(std::int64_t width, std::int64_t height) {
    if (width < 1 || height < 1 || width > 1000000 || height > 1000000 ||
        std::uint64_t(width) > std::uint64_t(UINT32_MAX) / std::uint64_t(height))
        throw std::invalid_argument("Invalid label raster dimensions");
    return std::size_t(width) * std::size_t(height);
}
inline void label_charge(std::size_t& used, std::size_t count, std::size_t each,
                         std::size_t limit) {
    if (used > limit || (each && count > (limit - used) / each))
        throw WorkflowCaptureLimit("Label analysis work budget exceeded");
    used += count * each;
}
inline void label_hash_text(Sha256& h, const std::string& value) {
    h.integer(value.size());
    h.update(value.data(), value.size());
}
inline void label_hash_real(Sha256& h, double value) {
    std::uint64_t b;
    std::memcpy(&b, &value, sizeof(b));
    h.integer(b);
}
inline void require_raw_lattice(const RawPixelBuffer& p) {
    const auto n = label_pixels(p.width(), p.height());
    if (!p.channels() || p.channels() > 4096 ||
        p.storage().index() != std::size_t(p.sample_type()) || p.valid().size() != n ||
        std::visit([](const auto& v) { return v.size(); }, p.storage()) != n * p.channels())
        throw std::invalid_argument("Malformed raw label-analysis input lattice/type");
    for (auto b : p.valid())
        if (b > 1)
            throw std::invalid_argument("Invalid raw label-analysis validity flag");
}
inline std::string label_raw_input(const RawPixelBuffer& p, const std::atomic<bool>* cancelled) {
    require_raw_lattice(p);
    Sha256 h;
    label_hash_text(h, "qpx.label-analysis-raw/1");
    h.integer(p.width());
    h.integer(p.height());
    h.integer(p.channels());
    h.integer(std::uint64_t(p.sample_type()));
    h.update(p.valid().data(), p.valid().size());
    std::visit(
        [&](const auto& values) {
            using T = typename std::decay_t<decltype(values)>::value_type;
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                if constexpr (std::is_integral_v<T>)
                    h.integer(std::uint64_t(values[i]));
                else if constexpr (sizeof(T) == 4) {
                    std::uint32_t b;
                    std::memcpy(&b, &values[i], 4);
                    h.integer(b);
                } else
                    label_hash_real(h, values[i]);
            }
        },
        p.storage());
    return h.finish();
}
} // namespace qpx::detail
