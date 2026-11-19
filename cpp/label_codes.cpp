#include "qupathxx/label_codes.hpp"
#include "label_analysis_internal.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <type_traits>

namespace qpx {
LabelCode::LabelCode() : LabelCode(false, 0) {}
LabelCode::LabelCode(bool negative, std::uint64_t magnitude)
    : negative_(negative && magnitude != 0), magnitude_(magnitude) {
    if (negative_ && magnitude_ > (std::uint64_t(1) << 63))
        throw std::invalid_argument("Negative label code exceeds the Int64 domain");
}
LabelCode LabelCode::from_signed(std::int64_t value) {
    return value < 0 ? LabelCode(true, std::uint64_t(-(value + 1)) + 1)
                     : LabelCode(false, std::uint64_t(value));
}
LabelCode LabelCode::from_unsigned(std::uint64_t value) {
    return {false, value};
}
LabelCode LabelCode::from_decimal(const std::string& text) {
    if (text.empty() || text.size() > 20)
        throw std::invalid_argument("Invalid exact label-code decimal");
    const bool negative = text.front() == '-';
    const auto* first = text.data() + (negative ? 1 : 0);
    const auto* last = text.data() + text.size();
    std::uint64_t magnitude = 0;
    const auto parsed = std::from_chars(first, last, magnitude);
    if (first == last || parsed.ec != std::errc{} || parsed.ptr != last)
        throw std::invalid_argument("Invalid exact label-code decimal");
    LabelCode result(negative, magnitude);
    if (result.decimal() != text)
        throw std::invalid_argument("Noncanonical exact label-code decimal");
    return result;
}
bool LabelCode::negative() const {
    return negative_;
}
std::uint64_t LabelCode::magnitude() const {
    return magnitude_;
}
std::string LabelCode::decimal() const {
    return (negative_ ? "-" : "") + std::to_string(magnitude_);
}
std::uint64_t LabelCode::stable_hash() const {
    std::uint64_t hash = 14695981039346656037ull;
    hash = (hash ^ std::uint8_t(negative_)) * 1099511628211ull;
    for (unsigned shift = 0; shift < 64; shift += 8)
        hash = (hash ^ std::uint8_t(magnitude_ >> shift)) * 1099511628211ull;
    return hash;
}
bool operator==(const LabelCode& a, const LabelCode& b) {
    return a.negative_ == b.negative_ && a.magnitude_ == b.magnitude_;
}
bool operator!=(const LabelCode& a, const LabelCode& b) {
    return !(a == b);
}
bool operator<(const LabelCode& a, const LabelCode& b) {
    if (a.negative_ != b.negative_)
        return a.negative_;
    return a.negative_ ? a.magnitude_ > b.magnitude_ : a.magnitude_ < b.magnitude_;
}
bool operator>(const LabelCode& a, const LabelCode& b) {
    return b < a;
}
bool operator<=(const LabelCode& a, const LabelCode& b) {
    return !(b < a);
}
bool operator>=(const LabelCode& a, const LabelCode& b) {
    return !(a < b);
}
LabelCodeMap::LabelCodeMap(LabelCodeStorage codes, const std::atomic<bool>* cancelled)
    : codes_(std::move(codes)) {
    workflow_checkpoint(cancelled);
    std::visit(
        [&](const auto& values) {
            if (values.size() > 1000000)
                throw std::invalid_argument("Label code map exceeds the label count domain");
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                if (!values[i] || (i && values[i - 1] >= values[i]))
                    throw std::invalid_argument(
                        "Label code map must be increasing, unique and exclude background zero");
            }
        },
        codes_);
}
std::size_t LabelCodeMap::size() const {
    return std::visit([](const auto& values) { return values.size(); }, codes_);
}
LabelCode LabelCodeMap::at(std::size_t index) const {
    return std::visit(
        [&](const auto& values) {
            using T = typename std::decay_t<decltype(values)>::value_type;
            if constexpr (std::is_signed_v<T>)
                return LabelCode::from_signed(values.at(index));
            else
                return LabelCode::from_unsigned(values.at(index));
        },
        codes_);
}
const LabelCodeStorage& LabelCodeMap::storage() const {
    return codes_;
}
std::size_t LabelCodeMap::payload_bytes() const {
    return std::visit(
        [](const auto& values) {
            return values.capacity() * sizeof(typename std::decay_t<decltype(values)>::value_type);
        },
        codes_);
}
std::vector<std::uint32_t> LabelCodeMap::release_uint32() && {
    if (auto* values = std::get_if<std::vector<std::uint32_t>>(&codes_))
        return std::move(*values);
    throw std::invalid_argument("Legacy source labels require an unsigned UInt32 code map");
}
namespace {
template <class T>
NormalizedLabelCodes normalize_integral(const RawPixelBuffer& raw, const std::vector<T>& values,
                                        LabelRasterLimits limits,
                                        const std::atomic<bool>* cancelled) {
    static_assert(std::is_integral_v<T>);
    using MapValue = std::conditional_t<
        (sizeof(T) > 4), T,
        std::conditional_t<std::is_signed_v<T>, std::int32_t, std::uint32_t>>;
    const auto n = values.size();
    constexpr std::size_t scratch = 65536;
    std::size_t base = scratch;
    detail::label_charge(base, raw.payload_bytes(), 1, limits.work_bytes);
    auto sorting = base;
    detail::label_charge(sorting, n, sizeof(T), limits.work_bytes);
    std::vector<T> sorted(values);
    sorting = base;
    detail::label_charge(sorting, sorted.capacity(), sizeof(T), limits.work_bytes);
    std::uint64_t comparisons = 0;
    std::sort(sorted.begin(), sorted.end(), [&](auto a, auto b) {
        if (!(comparisons++ & 4095))
            workflow_checkpoint(cancelled);
        return a < b;
    });
    const auto end = std::unique(sorted.begin(), sorted.end(), [&](auto a, auto b) {
        if (!(comparisons++ & 4095))
            workflow_checkpoint(cancelled);
        return a == b;
    });
    const bool has_zero = std::binary_search(sorted.begin(), end, T(0));
    const auto count = std::size_t(end - sorted.begin()) - std::size_t(has_zero);
    if (count > limits.max_labels)
        throw WorkflowCaptureLimit("External mask exceeds the region limit");
    // Charge the full sorting copy while its exact compact mapping is produced.
    detail::label_charge(sorting, count, sizeof(MapValue), limits.work_bytes);
    std::vector<MapValue> mapping;
    mapping.reserve(count);
    auto allocated_peak = base;
    detail::label_charge(allocated_peak, sorted.capacity(), sizeof(T), limits.work_bytes);
    detail::label_charge(allocated_peak, mapping.capacity(), sizeof(MapValue), limits.work_bytes);
    for (auto at = sorted.begin(); at != end; ++at) {
        if (!(std::size_t(at - sorted.begin()) & 4095))
            workflow_checkpoint(cancelled);
        if (*at != T(0))
            mapping.push_back(MapValue(*at));
    }
    // Sorting storage is retired before dense labels/region storage coexist.
    std::vector<T>().swap(sorted);
    auto retained = base;
    detail::label_charge(retained, mapping.capacity(), sizeof(MapValue), limits.work_bytes);
    auto dense = retained;
    detail::label_charge(dense, n, sizeof(std::uint32_t), limits.work_bytes);
    std::vector<std::uint32_t> labels(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        if (values[i] != T(0))
            labels[i] = std::uint32_t(
                std::lower_bound(mapping.begin(), mapping.end(), MapValue(values[i])) -
                mapping.begin() + 1);
    }
    if (retained >= limits.work_bytes || limits.work_bytes - retained < 4096)
        throw WorkflowCaptureLimit("External mask retention exceeds work");
    LabelRaster raster(raw.width(), raw.height(), std::move(labels),
                       {limits.max_labels, limits.work_bytes - retained}, cancelled);
    return {std::move(raster), LabelCodeMap(std::move(mapping), cancelled)};
}
} // namespace
NormalizedLabelCodes normalize_label_codes(const RawPixelBuffer& raw, LabelRasterLimits limits,
                                           const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    detail::require_raw_lattice(raw);
    if (raw.channels() != 1)
        throw std::invalid_argument("External mask requires exactly one raw channel");
    if (raw.sample_type() == SampleType::Float32 || raw.sample_type() == SampleType::Float64)
        throw std::invalid_argument("External mask label codes require an integral sample type");
    for (std::size_t i = 0; i < raw.valid().size(); ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        if (!raw.valid()[i])
            throw std::invalid_argument("External mask contains missing pixels");
    }
    return std::visit(
        [&](const auto& values) -> NormalizedLabelCodes {
            using T = typename std::decay_t<decltype(values)>::value_type;
            if constexpr (std::is_integral_v<T>)
                return normalize_integral(raw, values, limits, cancelled);
            else
                throw std::invalid_argument("External mask label codes require integral storage");
        },
        raw.storage());
}
} // namespace qpx
