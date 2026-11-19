// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/label_mask_archive.hpp"
#include "label_analysis_internal.hpp"
#include "reader_file_guard.hpp"
#include "filesystem_access.hpp"
#include "strict_json.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>
#include <utility>

namespace qpx {
namespace {
using Json = nlohmann::json;
constexpr std::array<std::uint8_t, 8> magic{{'Q', 'P', 'L', 'M', '1', 0, 0, 0}};
constexpr std::size_t prefix_bytes = 40, digest_bytes = 64;
constexpr std::size_t header_limit = 1024u * 1024, codes_limit = 32u * 1024 * 1024;
constexpr std::size_t scratch_bytes = 65536;

void require(bool valid, const char* message) {
    if (!valid)
        throw std::invalid_argument(message);
}
void charge(std::size_t& used, std::size_t count, std::size_t each, std::size_t limit) {
    detail::label_charge(used, count, each, limit);
}
bool digest(const std::string& value) {
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
std::size_t context_payload(const LabelMaskArchiveContext& context) {
    auto used = sizeof(context);
    for (const auto* text : {&context.record_sha256, &context.input_sha256, &context.content_sha256,
             &context.observed_store_sha256, &context.reader_sha256, &context.parameters_json})
        used += text->capacity();
    return used;
}
void validate_context(const LabelMaskArchiveContext& context) {
    for (const auto* value : {&context.record_sha256, &context.input_sha256, &context.content_sha256,
             &context.observed_store_sha256, &context.reader_sha256})
        require(digest(*value), "Label-mask archive requires exact lowercase SHA256 identities");
    detail::bounded_utf8(context.parameters_json, 4096);
    require(!context.parameters_json.empty(), "Label-mask archive requires a parameter recipe");
    for (const auto& value : {context.target.pixel_width_um, context.target.pixel_height_um,
             context.target.z_spacing_um, context.target.time_interval_seconds})
        require(!value || (std::isfinite(*value) && *value > 0), "Invalid archive target calibration");
}
void keys(const Json& value, std::initializer_list<const char*> expected) {
    require(value.is_object() && value.size() == expected.size(), "Unsupported archive JSON members");
    for (const auto* name : expected)
        require(value.contains(name), "Missing archive JSON member");
}
std::string text(const Json& value, std::size_t maximum) {
    require(value.is_string(), "Archive JSON member must be text");
    auto result = value.get<std::string>();
    detail::bounded_utf8(result, maximum);
    return result;
}
std::uint64_t integer(const Json& value, std::uint64_t maximum) {
    const auto token = text(value, 20);
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), result);
    require(!token.empty() && parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size() &&
        result <= maximum && std::to_string(result) == token, "Noncanonical archive integer");
    return result;
}
Json optional(const std::optional<double>& value) { return value ? Json(*value) : Json(nullptr); }
std::optional<double> optional(const Json& value) {
    if (value.is_null())
        return {};
    require(value.is_number(), "Archive target calibration must be numeric or null");
    const auto result = value.get<double>();
    require(std::isfinite(result) && result > 0, "Invalid archive target calibration");
    return result;
}
std::string encode_header(const LabelMaskArchiveContext& context, SampleType type, std::size_t count) {
    const auto& target = context.target;
    const auto& crop = target.crop;
    return Json({{"schema", "qpx.label-mask-archive/1"}, {"record_sha256", context.record_sha256},
        {"input_sha256", context.input_sha256}, {"content_sha256", context.content_sha256},
        {"observed_store_sha256", context.observed_store_sha256}, {"reader_sha256", context.reader_sha256},
        {"parameters_json", context.parameters_json}, {"sample_type", sample_name(type)},
        {"code_count", std::to_string(count)},
        {"target", {{"width", std::to_string(target.width)}, {"height", std::to_string(target.height)},
            {"crop", {{"x", std::to_string(crop.x)}, {"y", std::to_string(crop.y)},
                {"level", std::to_string(crop.level)}, {"width", std::to_string(crop.width)},
                {"height", std::to_string(crop.height)}}},
            {"pixel_width_um", optional(target.pixel_width_um)}, {"pixel_height_um", optional(target.pixel_height_um)},
            {"z_spacing_um", optional(target.z_spacing_um)}, {"time_interval_seconds", optional(target.time_interval_seconds)},
            {"z", std::to_string(target.z)}, {"t", std::to_string(target.t)}}},
        {"plane", {{"label_z", std::to_string(context.plane.label_z)}, {"label_t", std::to_string(context.plane.label_t)},
            {"reference_z", std::to_string(context.plane.reference_z)},
            {"reference_t", std::to_string(context.plane.reference_t)}}}}).dump();
}

struct HeaderSax final : detail::StrictJson {
    std::size_t nodes = 0;
    const std::atomic<bool>* cancelled;
    explicit HeaderSax(const std::atomic<bool>* c) : StrictJson("label-mask archive context"), cancelled(c) {}
    void tick() {
        workflow_checkpoint(cancelled);
        if (++nodes > 256)
            throw WorkflowCaptureLimit("Archive context exceeds node limit");
    }
    bool null() override { tick(); return true; }
    bool boolean(bool) override { tick(); return true; }
    bool number_integer(number_integer_t) override { tick(); return true; }
    bool number_unsigned(number_unsigned_t) override { tick(); return true; }
    bool number_float(number_float_t value, const string_t&) override {
        tick(); require(std::isfinite(value), "Nonfinite archive JSON number"); return true;
    }
    bool string(string_t& value) override { tick(); detail::bounded_utf8(value, 4096); return true; }
    bool key(string_t& value) override { tick(); detail::bounded_utf8(value, 64); return StrictJson::key(value); }
    bool start_object(std::size_t count) override {
        tick(); require(frames.size() < 8, "Archive context exceeds depth limit"); return StrictJson::start_object(count);
    }
    bool start_array(std::size_t count) override {
        tick(); require(frames.size() < 8, "Archive context exceeds depth limit"); return StrictJson::start_array(count);
    }
};
struct Header {
    LabelMaskArchiveContext context;
    SampleType type = SampleType::UInt8;
    std::size_t count = 0;
};
Header decode_header(const std::uint8_t* bytes, std::size_t size, LabelRasterLimits limits,
    const std::atomic<bool>* cancelled) {
    const std::string encoded(reinterpret_cast<const char*>(bytes), size);
    detail::bounded_utf8(encoded, header_limit);
    HeaderSax sax(cancelled);
    require(Json::sax_parse(encoded, &sax), "Invalid archive context JSON");
    const auto value = Json::parse(encoded);
    keys(value, {"schema", "record_sha256", "input_sha256", "content_sha256", "observed_store_sha256",
        "reader_sha256", "parameters_json", "sample_type", "code_count", "target", "plane"});
    require(value.at("schema") == "qpx.label-mask-archive/1", "Unsupported label-mask archive schema");
    Header result;
    auto& context = result.context;
    context.record_sha256 = text(value.at("record_sha256"), 64);
    context.input_sha256 = text(value.at("input_sha256"), 64);
    context.content_sha256 = text(value.at("content_sha256"), 64);
    context.observed_store_sha256 = text(value.at("observed_store_sha256"), 64);
    context.reader_sha256 = text(value.at("reader_sha256"), 64);
    context.parameters_json = text(value.at("parameters_json"), 4096);
    const auto type_name = text(value.at("sample_type"), 16);
    bool found = false;
    for (unsigned i = 0; i < 8; ++i)
        if (sample_name(SampleType(i)) == type_name) {
            result.type = SampleType(i);
            found = true;
        }
    require(found, "Archive source sample type must be integral");
    result.count = std::size_t(integer(value.at("code_count"), limits.max_labels));
    const auto& target = value.at("target");
    keys(target, {"width", "height", "crop", "pixel_width_um", "pixel_height_um", "z_spacing_um",
        "time_interval_seconds", "z", "t"});
    context.target.width = std::int64_t(integer(target.at("width"), INT64_MAX));
    context.target.height = std::int64_t(integer(target.at("height"), INT64_MAX));
    const auto& crop = target.at("crop");
    keys(crop, {"x", "y", "level", "width", "height"});
    context.target.crop = {std::int64_t(integer(crop.at("x"), INT64_MAX)),
        std::int64_t(integer(crop.at("y"), INT64_MAX)), std::int32_t(integer(crop.at("level"), INT32_MAX)),
        std::int64_t(integer(crop.at("width"), INT64_MAX)), std::int64_t(integer(crop.at("height"), INT64_MAX))};
    context.target.pixel_width_um = optional(target.at("pixel_width_um"));
    context.target.pixel_height_um = optional(target.at("pixel_height_um"));
    context.target.z_spacing_um = optional(target.at("z_spacing_um"));
    context.target.time_interval_seconds = optional(target.at("time_interval_seconds"));
    context.target.z = std::uint32_t(integer(target.at("z"), UINT32_MAX));
    context.target.t = std::uint32_t(integer(target.at("t"), UINT32_MAX));
    const auto& plane = value.at("plane");
    keys(plane, {"label_z", "label_t", "reference_z", "reference_t"});
    context.plane = {std::uint32_t(integer(plane.at("label_z"), UINT32_MAX)),
        std::uint32_t(integer(plane.at("label_t"), UINT32_MAX)),
        std::uint32_t(integer(plane.at("reference_z"), UINT32_MAX)),
        std::uint32_t(integer(plane.at("reference_t"), UINT32_MAX))};
    validate_context(context);
    return result;
}

unsigned storage_index(SampleType type) {
    switch (type) {
    case SampleType::UInt8: case SampleType::UInt16: case SampleType::UInt32: return 0;
    case SampleType::Int8: case SampleType::Int16: case SampleType::Int32: return 1;
    case SampleType::UInt64: return 2;
    case SampleType::Int64: return 3;
    default: throw std::invalid_argument("Archive source sample type must be integral");
    }
}
void code_range(const LabelCode& code, SampleType type) {
    const auto bits = unsigned(sample_bytes(type) * 8);
    const bool signed_type = type == SampleType::Int8 || type == SampleType::Int16 ||
        type == SampleType::Int32 || type == SampleType::Int64;
    if (signed_type) {
        const auto negative_maximum = std::uint64_t(1) << (bits - 1);
        require(code.magnitude() <= negative_maximum - std::uint64_t(!code.negative()),
            "Archive code exceeds the declared signed source type");
    } else {
        require(!code.negative() && (bits == 64 || code.magnitude() <= ((std::uint64_t(1) << bits) - 1)),
            "Archive code exceeds the declared unsigned source type");
    }
}
std::string reconstructed_content(const LabelRaster& raster, const LabelCodeMap& codes,
    const LabelMaskSourceDescription& description, const LabelMaskCropLink& linked,
    const std::atomic<bool>* cancelled) {
    // Same exact mathematical-integer hash domain as detail::label_raw_input;
    // reconstruct samples as a stream, retaining no second full raw image.
    Sha256 raw;
    detail::label_hash_text(raw, "qpx.label-analysis-raw/1");
    raw.integer(raster.width()); raw.integer(raster.height()); raw.integer(1);
    raw.integer(std::uint64_t(description.sample_type));
    std::array<std::uint8_t, 4096> validity;
    validity.fill(1);
    for (std::size_t at = 0; at < raster.labels().size();) {
        workflow_checkpoint(cancelled);
        const auto count = std::min(validity.size(), raster.labels().size() - at);
        raw.update(validity.data(), count);
        at += count;
    }
    for (std::size_t i = 0; i < raster.labels().size(); ++i) {
        if (!(i & 4095)) workflow_checkpoint(cancelled);
        const auto label = raster.labels()[i];
        const auto code = label ? codes.at(label - 1) : LabelCode();
        raw.integer(code.negative() ? std::uint64_t(0) - code.magnitude() : code.magnitude());
    }
    Sha256 content;
    detail::label_hash_text(content, "qpx.ngff-label-content/1");
    detail::label_hash_text(content, description.sha256);
    detail::label_hash_text(content, raw.finish());
    for (const auto value : {linked.source_crop.x, linked.source_crop.y,
             linked.source_crop.width, linked.source_crop.height})
        content.integer(std::uint64_t(value));
    return content.finish();
}
void validate_components(const LabelRaster& raster, const LabelCodeMap& codes,
    const LabelMaskSourceDescription& description, const LabelMaskArchiveContext& context,
    LabelRasterLimits limits, const std::atomic<bool>* cancelled) {
    validate_context(context);
    require(codes.size() == raster.regions().size() && codes.size() <= limits.max_labels,
        "Archive mapping count differs from dense raster regions");
    require(raster.width() == context.target.crop.width && raster.height() == context.target.crop.height,
        "Archive raster dimensions differ from the target crop");
    require(description.z == context.plane.label_z && description.t == context.plane.label_t,
        "Archive selected label plane differs from its source description");
    require(codes.storage().index() == storage_index(description.sample_type),
        "Archive mapping storage differs from the source integral type");
    for (std::size_t i = 0; i < codes.size(); ++i) {
        if (!(i & 4095)) workflow_checkpoint(cancelled);
        code_range(codes.at(i), description.sample_type);
    }
    const auto linked = validate_link_label_mask_frames(description.label_frame, description.reference_frame,
        context.plane, context.target);
    require(reconstructed_content(raster, codes, description, linked, cancelled) == context.content_sha256,
        "Archive decoded-content identity differs");
}
bool same_frame(const LabelMaskFrame& a, const LabelMaskFrame& b) {
    if (a.axes.size() != b.axes.size()) return false;
    for (std::size_t i = 0; i < a.axes.size(); ++i) {
        const auto& x = a.axes[i]; const auto& y = b.axes[i];
        if (x.name != y.name || x.unit != y.unit || x.extent != y.extent ||
            std::memcmp(&x.scale, &y.scale, sizeof(double)) ||
            std::memcmp(&x.translation, &y.translation, sizeof(double))) return false;
    }
    return true;
}
void same_description(const LabelMaskSourceDescription& a, const LabelMaskSourceDescription& b,
    const std::atomic<bool>* cancelled) {
    require(a.format == b.format && a.label_group == b.label_group && a.reference_group == b.reference_group &&
        a.source_reference == b.source_reference && a.default_reference == b.default_reference &&
        a.level == b.level && a.z == b.z && a.t == b.t && a.sample_type == b.sample_type &&
        same_frame(a.label_frame, b.label_frame) && same_frame(a.reference_frame, b.reference_frame) &&
        a.label_metadata == b.label_metadata && a.reference_metadata == b.reference_metadata &&
        a.json == b.json && a.sha256 == b.sha256 && a.label_properties.size() == b.label_properties.size(),
        "Archive source description differs from its exact JSON authority");
    for (std::size_t i = 0; i < a.label_properties.size(); ++i) {
        if (!(i & 1023)) workflow_checkpoint(cancelled);
        const auto& x = a.label_properties[i]; const auto& y = b.label_properties[i];
        require(x.code == y.code && x.properties == y.properties && x.rgba == y.rgba,
            "Archive source properties differ from their exact JSON authority");
    }
}

void put(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) bytes.push_back(std::uint8_t(value >> (8 * i)));
}
std::uint64_t get(const std::uint8_t* bytes) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= std::uint64_t(bytes[i]) << (8 * i);
    return value;
}
std::string hash(const std::uint8_t* bytes, std::size_t size, const std::atomic<bool>* cancelled) {
    Sha256 result;
    for (std::size_t at = 0; at < size;) {
        workflow_checkpoint(cancelled);
        const auto count = std::min<std::size_t>(65536, size - at);
        result.update(bytes + at, count);
        at += count;
    }
    return result.finish();
}
void append(std::vector<std::uint8_t>& result, const void* bytes, std::size_t size,
    const std::atomic<bool>* cancelled) {
    const auto* first = static_cast<const std::uint8_t*>(bytes);
    for (std::size_t at = 0; at < size;) {
        workflow_checkpoint(cancelled);
        const auto count = std::min<std::size_t>(65536, size - at);
        result.insert(result.end(), first + at, first + at + count);
        at += count;
    }
}
template<class T> LabelCodeMap decode_codes(const std::uint8_t* bytes, std::size_t size,
    std::size_t count, SampleType type, const std::atomic<bool>* cancelled) {
    std::vector<T> values;
    values.reserve(count);
    std::size_t at = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!(i & 1023)) workflow_checkpoint(cancelled);
        const auto start = at;
        while (at < size && bytes[at] != '\n' && at - start <= 20) ++at;
        require(at < size && bytes[at] == '\n' && at > start && at - start <= 20,
            "Archive codes require canonical decimal/newline records");
        const auto code = LabelCode::from_decimal(std::string(reinterpret_cast<const char*>(bytes + start), at - start));
        require(code.magnitude() != 0, "Archive code map cannot include zero background");
        code_range(code, type);
        if constexpr (std::is_signed_v<T>) {
            if (code.negative() && code.magnitude() == (std::uint64_t(1) << 63))
                values.push_back(std::numeric_limits<T>::min());
            else values.push_back(T(code.negative() ? -std::int64_t(code.magnitude()) : std::int64_t(code.magnitude())));
        } else values.push_back(T(code.magnitude()));
        ++at;
    }
    require(at == size, "Archive code count differs from its text");
    return LabelCodeMap(std::move(values), cancelled);
}
LabelCodeMap decode_codes(const std::uint8_t* bytes, std::size_t size, const Header& header,
    const std::atomic<bool>* cancelled) {
    switch (storage_index(header.type)) {
    case 0: return decode_codes<std::uint32_t>(bytes, size, header.count, header.type, cancelled);
    case 1: return decode_codes<std::int32_t>(bytes, size, header.count, header.type, cancelled);
    case 2: return decode_codes<std::uint64_t>(bytes, size, header.count, header.type, cancelled);
    default: return decode_codes<std::int64_t>(bytes, size, header.count, header.type, cancelled);
    }
}

}

std::vector<std::uint8_t> encode_label_mask_archive(const LabelRaster& raster, const LabelCodeMap& codes,
    const LabelMaskSourceDescription& description, const LabelMaskArchiveContext& context,
    LabelRasterLimits limits, const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    validate_context(context);
    detail::bounded_utf8(description.json, label_mask_description_limit);
    require(description.label_frame.axes.size() <= 5 && description.reference_frame.axes.size() <= 5 &&
        description.label_properties.size() <= 4096, "Archive description exceeds structural limits");
    auto base = scratch_bytes;
    charge(base, raster.payload_bytes(), 1, limits.work_bytes);
    charge(base, codes.payload_bytes(), 1, limits.work_bytes);
    charge(base, description.payload_bytes(), 1, limits.work_bytes);
    charge(base, context_payload(context), 1, limits.work_bytes);
    auto parsing = base;
    charge(parsing, description.json.size(), 96, limits.work_bytes);
    charge(parsing, scratch_bytes, 1, limits.work_bytes);
    {
        const auto decoded = decode_label_mask_description(description.json, cancelled);
        same_description(description, decoded, cancelled);
        validate_components(raster, codes, decoded, context, limits, cancelled);
    }
    auto header_work = base;
    charge(header_work, context.parameters_json.size() + 4096, 16, limits.work_bytes);
    const auto header = encode_header(context, description.sample_type, codes.size());
    require(!header.empty() && header.size() <= header_limit, "Archive context exceeds1MiB");
    std::size_t code_size = 0;
    for (std::size_t i = 0; i < codes.size(); ++i) {
        if (!(i & 1023)) workflow_checkpoint(cancelled);
        charge(code_size, codes.at(i).decimal().size() + 1, 1, codes_limit);
    }
    auto code_work = base;
    charge(code_work, header.capacity(), 1, limits.work_bytes);
    charge(code_work, code_size, 3, limits.work_bytes); // append/reallocation overlap
    const auto code_text = label_code_map_text(codes, codes_limit, cancelled);
    require(code_text.size() == code_size, "Archive code serialization size changed");
    auto retained = base;
    charge(retained, header.capacity() + code_text.capacity(), 1, limits.work_bytes);
    if (retained >= limits.work_bytes || limits.work_bytes - retained < 4096)
        throw WorkflowCaptureLimit("Archive leaves no raster encoder workspace");
    const auto embedded = encode_label_raster(raster,
        {limits.max_labels, limits.work_bytes - (retained - raster.payload_bytes())}, cancelled);
    auto total = prefix_bytes + digest_bytes;
    for (const auto size : {header.size(), description.json.size(), code_text.size(), embedded.size()})
        charge(total, size, 1, limits.work_bytes);
    auto peak = retained;
    charge(peak, embedded.capacity(), 1, limits.work_bytes);
    charge(peak, total, 1, limits.work_bytes);
    std::vector<std::uint8_t> result;
    result.reserve(total);
    charge(peak, result.capacity() - total, 1, limits.work_bytes);
    result.insert(result.end(), magic.begin(), magic.end());
    for (const auto size : {header.size(), description.json.size(), code_text.size(), embedded.size()}) put(result, size);
    append(result, header.data(), header.size(), cancelled);
    append(result, description.json.data(), description.json.size(), cancelled);
    append(result, code_text.data(), code_text.size(), cancelled);
    append(result, embedded.data(), embedded.size(), cancelled);
    const auto checksum = hash(result.data(), result.size(), cancelled);
    append(result, checksum.data(), checksum.size(), cancelled);
    workflow_checkpoint(cancelled);
    return result;
}

LabelMaskArchive decode_label_mask_archive(const void* input, std::size_t size,
    LabelRasterLimits limits, const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    require(input && size >= prefix_bytes + digest_bytes && size <= limits.work_bytes,
        "Invalid label-mask archive length");
    const auto* bytes = static_cast<const std::uint8_t*>(input);
    require(std::equal(magic.begin(), magic.end(), bytes), "Unsupported label-mask archive magic/version");
    std::array<std::size_t, 4> lengths{};
    auto total = prefix_bytes + digest_bytes;
    for (unsigned i = 0; i < lengths.size(); ++i) {
        const auto length = get(bytes + 8 + i * 8);
        require(length <= size && length <= std::numeric_limits<std::size_t>::max(), "Archive length exceeds domain");
        lengths[i] = std::size_t(length);
        charge(total, lengths[i], 1, size);
    }
    require(total == size && lengths[0] && lengths[0] <= header_limit && lengths[1] &&
        lengths[1] <= label_mask_description_limit && lengths[2] <= codes_limit && lengths[3],
        "Archive payload lengths differ from profile");
    require(hash(bytes, size - digest_bytes, cancelled) ==
        std::string(reinterpret_cast<const char*>(bytes + size - digest_bytes), digest_bytes),
        "Label-mask archive checksum differs");
    auto header_work = size;
    charge(header_work, lengths[0], 3, limits.work_bytes);
    charge(header_work, 2 * scratch_bytes, 1, limits.work_bytes);
    auto header = decode_header(bytes + prefix_bytes, lengths[0], limits, cancelled);
    require(header.count <= lengths[2] / 2 && lengths[2] <= header.count * 21,
        "Archive code count differs from its bounded text length");
    auto retained = size;
    charge(retained, context_payload(header.context) + scratch_bytes, 1, limits.work_bytes);
    auto description_work = retained;
    charge(description_work, lengths[1], 96, limits.work_bytes);
    charge(description_work, scratch_bytes, 1, limits.work_bytes);
    const auto* descriptor = bytes + prefix_bytes + lengths[0];
    auto description = std::make_shared<const LabelMaskSourceDescription>(decode_label_mask_description(
        std::string(reinterpret_cast<const char*>(descriptor), lengths[1]), cancelled));
    require(description->sample_type == header.type, "Archive type differs from the source description");
    charge(retained, description->payload_bytes(), 1, limits.work_bytes);
    const auto mapping_width = storage_index(header.type) < 2 ? 4u : 8u;
    auto map_work = retained;
    charge(map_work, header.count, mapping_width, limits.work_bytes);
    const auto* code_text = descriptor + lengths[1];
    auto codes = decode_codes(code_text, lengths[2], header, cancelled);
    charge(retained, codes.payload_bytes(), 1, limits.work_bytes);
    const auto external = retained - lengths[3]; // embedded decoder charges its borrowed subspan
    if (external >= limits.work_bytes || limits.work_bytes - external < 4096)
        throw WorkflowCaptureLimit("Archive leaves no raster decoder workspace");
    auto raster = decode_label_raster(code_text + lengths[2], lengths[3],
        {limits.max_labels, limits.work_bytes - external}, cancelled);
    charge(retained, raster.payload_bytes(), 1, limits.work_bytes);
    validate_components(raster, codes, *description, header.context, limits, cancelled);
    workflow_checkpoint(cancelled);
    return {std::move(raster), std::move(codes), std::move(description), std::move(header.context)};
}

LabelMaskArchive open_label_mask_archive(const std::string& path, LabelRasterLimits limits,
    const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    workflow_path_argument(path);
    detail::ReaderFileGuard guard(path);
    if (guard.version()->bytes > limits.work_bytes)
        throw WorkflowCaptureLimit("Label-mask archive file exceeds work budget");
    std::vector<std::uint8_t> bytes(std::size_t(guard.version()->bytes));
    std::ifstream file(detail::access_path(std::filesystem::u8path(guard.version()->path)), std::ios::binary);
    require(bool(file), "Cannot open label-mask archive file");
    for (std::size_t at = 0; at < bytes.size();) {
        workflow_checkpoint(cancelled);
        const auto count = std::min<std::size_t>(65536, bytes.size() - at);
        require(bool(file.read(reinterpret_cast<char*>(bytes.data() + at), std::streamsize(count))),
            "Truncated label-mask archive file");
        at += count;
    }
    guard.verify();
    auto archive = decode_label_mask_archive(bytes.data(), bytes.size(), limits, cancelled);
    guard.verify();
    return archive;
}

}
