// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/label_mask_archive.hpp"
#include "label_analysis_internal.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace qpx;
using Json = nlohmann::json;
namespace {
const std::string literal_description = R"({"schema":"qpx.label-mask-description/1","format":"ome-ngff-labels-0.4-zarr2/1","label_group":"labels/cells","reference_group":"","source_reference":{"path":"../../","origin":"explicit"},"level":0,"z":0,"t":0,"sample_type":"uint8","label_frame":{"axes":[{"name":"y","unit":"","extent":"1","scale":1,"translation":0},{"name":"x","unit":"","extent":"2","scale":1,"translation":0}]},"reference_frame":{"axes":[{"name":"y","unit":"","extent":"1","scale":1,"translation":0},{"name":"x","unit":"","extent":"2","scale":1,"translation":0}]},"label_metadata":"{\"original\":1}","reference_metadata":"{\"reference\":1}","label_properties":[]})";
const std::string literal_header = R"({"code_count":"1","content_sha256":"9ac63df7d201eab55b5f768bbd41d97274a5bddab7095c285e1fbefed2dddfdd","input_sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","observed_store_sha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","parameters_json":"{}","plane":{"label_t":"0","label_z":"0","reference_t":"0","reference_z":"0"},"reader_sha256":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","record_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","sample_type":"uint8","schema":"qpx.label-mask-archive/1","target":{"crop":{"height":"1","level":"0","width":"2","x":"0","y":"0"},"height":"1","pixel_height_um":null,"pixel_width_um":null,"t":"0","time_interval_seconds":null,"width":"2","z":"0","z_spacing_um":null}})";

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Exception = std::exception, class F> void refuses(F&& call) {
    try { call(); }
    catch (const Exception& error) { check(!std::string(error.what()).empty(), "Archive refusal has no explanation"); return; }
    throw std::runtime_error("Missing label-mask archive refusal");
}
template<class F> void refuses_containing(F&& call, const char* expected) {
    try { call(); }
    catch (const std::exception& error) {
        check(std::string(error.what()).find(expected) != std::string::npos,
            "Archive refusal did not exercise the intended trust boundary");
        return;
    }
    throw std::runtime_error("Missing archive trust-boundary refusal");
}
std::vector<std::uint8_t> unhex(const std::string& value) {
    check(value.size() % 2 == 0, "Malformed literal hex oracle");
    auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < value.size(); i += 2)
        bytes.push_back(std::uint8_t(digit(value[i]) * 16 + digit(value[i + 1])));
    return bytes;
}
void add(std::vector<std::uint8_t>& out, const std::string& text) { out.insert(out.end(), text.begin(), text.end()); }
void integer(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out.push_back(std::uint8_t(value >> (i * 8)));
}
std::vector<std::uint8_t> raster_oracle() {
    return unhex("5150584c4142454c01000000020000000000000001000000000000000100000000000000000000000100000038323534383866663238643531653030666165363934623562663064333230386234626137356362303837303364616365643332663636663939363536336532");
}
std::vector<std::uint8_t> literal_wire() {
    // Independent .NET SHA256 + BinaryWriter little-endian oracle. Neither the
    // expected header, prefix, embedded raster nor final digest uses this codec.
    auto bytes = unhex("51504c4d310000002403000000000000780200000000000002000000000000006c00000000000000");
    add(bytes, literal_header);
    add(bytes, literal_description);
    add(bytes, "7\n");
    const auto embedded = raster_oracle();
    bytes.insert(bytes.end(), embedded.begin(), embedded.end());
    add(bytes, "0af49a19d0dac8ea5ba50bada45758de1035a3e859df00a16eb93b350f32486c");
    return bytes;
}
// Negative controls deliberately receive a valid outer checksum, so their
// semantic/structural refusals cannot pass merely by detecting stale hash bytes.
std::vector<std::uint8_t> pack(const std::string& header, const std::string& description,
    const std::string& codes, const std::vector<std::uint8_t>& raster = raster_oracle()) {
    auto out = unhex("51504c4d31000000");
    for (const auto size : {header.size(), description.size(), codes.size(), raster.size()}) integer(out, size);
    add(out, header); add(out, description); add(out, codes);
    out.insert(out.end(), raster.begin(), raster.end());
    Sha256 checksum; checksum.update(out.data(), out.size()); add(out, checksum.finish());
    return out;
}
LabelMaskSourceDescription descriptor(SampleType type, std::int64_t width, std::int64_t height = 1) {
    auto value = Json::parse(literal_description);
    value["sample_type"] = sample_name(type);
    for (const auto* key : {"label_frame", "reference_frame"}) {
        value[key]["axes"][0]["extent"] = std::to_string(height);
        value[key]["axes"][1]["extent"] = std::to_string(width);
    }
    return decode_label_mask_description(value.dump());
}
LabelMaskArchiveContext context(std::int64_t width = 2, std::int64_t height = 1) {
    LabelMaskArchiveContext result;
    result.record_sha256 = std::string(64, 'a');
    result.input_sha256 = std::string(64, 'b');
    result.observed_store_sha256 = std::string(64, 'c');
    result.reader_sha256 = std::string(64, 'd');
    result.parameters_json = "{}";
    result.target.width = width; result.target.height = height;
    result.target.crop = {0, 0, 0, width, height};
    return result;
}
std::string content(const RawPixelBuffer& raw, const LabelMaskSourceDescription& description,
    const LabelMaskArchiveContext& context) {
    const auto linked = validate_link_label_mask_frames(description.label_frame, description.reference_frame,
        context.plane, context.target);
    Sha256 result;
    detail::label_hash_text(result, "qpx.ngff-label-content/1");
    detail::label_hash_text(result, description.sha256);
    // The existing raw authority is the reference for reconstructed samples.
    detail::label_hash_text(result, detail::label_raw_input(raw, nullptr));
    for (const auto value : {linked.source_crop.x, linked.source_crop.y,
             linked.source_crop.width, linked.source_crop.height}) result.integer(std::uint64_t(value));
    return result.finish();
}
RawPixelBuffer raw_values(SampleType type, bool zero = false, std::int64_t width = 4) {
    RawPixelBuffer raw(width, 1, 1, type);
    std::fill(raw.writable_valid().begin(), raw.writable_valid().end(), 1);
    std::visit([&](auto& values) {
        using T = typename std::decay_t<decltype(values)>::value_type;
        if constexpr (std::is_integral_v<T>) {
            if (zero) std::fill(values.begin(), values.end(), 0);
            else if (width != 4) std::fill(values.begin(), values.end(), 1);
            else if constexpr (std::is_signed_v<T>) {
                values = {std::numeric_limits<T>::min(), T(-1), T(0), std::numeric_limits<T>::max()};
                if constexpr (sizeof(T) == 8) values[1] = T(-INT64_C(9007199254740993));
            } else {
                values = {T(0), T(1), std::numeric_limits<T>::max(), T(1)};
                if constexpr (sizeof(T) == 8) values = {0, UINT64_C(9007199254740992), UINT64_C(9007199254740993), UINT64_MAX};
            }
        } else throw std::runtime_error("Integral-only archive fixture");
    }, raw.writable_storage());
    return raw;
}
void same_map(const LabelCodeMap& a, const LabelCodeMap& b) {
    check(a.storage().index() == b.storage().index() && a.size() == b.size(), "Compact source map type/count changed");
    for (std::size_t i = 0; i < a.size(); ++i) check(a.at(i) == b.at(i), "Exact source code changed");
}

void oracle() {
    check(literal_header.size() == 804 && literal_description.size() == 632,
        "Literal independent context/description byte lengths changed");
    const auto description = decode_label_mask_description(literal_description);
    check(description.sha256 == "fd5c62aeb8a1a0b43af4f30cb6f71eb04f2723585ced6c47b4c4f7ddf484f503",
        "Independent descriptor hash changed");
    auto info = context();
    info.content_sha256 = "9ac63df7d201eab55b5f768bbd41d97274a5bddab7095c285e1fbefed2dddfdd";
    LabelRaster raster(2, 1, {0, 1});
    LabelCodeMap codes(std::vector<std::uint32_t>{7});
    const auto expected = literal_wire();
    const auto encoded = encode_label_mask_archive(raster, codes, description, info);
    check(expected.size() == 1650 && encoded == expected, "QPLM1 bytes differ from independent literal wire/hash oracle");
    check(encode_label_raster(raster) == raster_oracle(), "Embedded legacy raster bytes changed");
    const auto decoded = decode_label_mask_archive(expected.data(), expected.size());
    check(decoded.raster.sha256() == raster.sha256() && decoded.description->json == literal_description &&
        decoded.context.content_sha256 == info.content_sha256, "Independent wire failed native decode");
    same_map(decoded.source_codes, codes);
    const auto legacy = raster_oracle();
    refuses([&] { decode_label_mask_archive(legacy.data(), legacy.size()); });
    refuses([&] { decode_label_raster(expected.data(), expected.size()); });
}

void integral_roundtrips() {
    for (unsigned i = 0; i < 8; ++i) for (const bool zero : {false, true}) {
        const auto type = SampleType(i);
        const auto raw = raw_values(type, zero);
        auto normalized = normalize_label_codes(raw);
        auto description = descriptor(type, raw.width());
        auto info = context(raw.width());
        info.content_sha256 = content(raw, description, info);
        auto encoded = encode_label_mask_archive(normalized.raster, normalized.source_codes, description, info);
        auto decoded = decode_label_mask_archive(encoded.data(), encoded.size());
        check(decoded.raster.labels() == normalized.raster.labels() && decoded.raster.sha256() == normalized.raster.sha256() &&
            decoded.description->json == description.json && decoded.description->sample_type == type &&
            decoded.context.record_sha256 == info.record_sha256 && decoded.context.input_sha256 == info.input_sha256 &&
            decoded.context.content_sha256 == info.content_sha256, "Integral/zero-only archive roundtrip changed identity");
        same_map(decoded.source_codes, normalized.source_codes);
        const auto retained_json = decoded.description->json;
        const auto retained_labels = decoded.raster.labels();
        std::fill(encoded.begin(), encoded.end(), 0);
        description.json.assign(1, 'x'); info.parameters_json.assign(1, 'x');
        check(decoded.description->json == retained_json && decoded.raster.labels() == retained_labels &&
            decoded.context.parameters_json == "{}", "Decoded archive aliases mutable caller storage");
    }
}

void metadata_boundaries() {
    auto description_text = literal_description;
    description_text.append(label_mask_description_limit - description_text.size(), ' ');
    const auto description = decode_label_mask_description(description_text);
    RawPixelBuffer raw(2, 1, 1, SampleType::UInt8);
    raw.writable_valid() = {1, 1};
    std::get<std::vector<std::uint8_t>>(raw.writable_storage()) = {0, 7};
    auto info = context();
    info.parameters_json = R"({"p":")";
    info.parameters_json.append(4096 - info.parameters_json.size() - 2, 'x');
    info.parameters_json += "\"}";
    info.content_sha256 = content(raw, description, info);
    const auto bytes = encode_label_mask_archive(LabelRaster(2, 1, {0, 1}),
        LabelCodeMap(std::vector<std::uint32_t>{7}), description, info);
    const auto decoded = decode_label_mask_archive(bytes.data(), bytes.size());
    check(decoded.description->json.size() == 65536 && decoded.description->json == description_text &&
        decoded.context.parameters_json.size() == 4096 && decoded.context.parameters_json == info.parameters_json,
        "Exact metadata/recipe limits were narrowed or text normalized");
}

void contexts() {
    auto raw = raw_values(SampleType::UInt64);
    auto normalized = normalize_label_codes(raw);
    auto description = descriptor(SampleType::UInt64, INT64_C(9007199254740993));
    auto info = context();
    info.target.width = INT64_C(9007199254740993);
    info.target.crop = {INT64_C(9007199254740989), 0, 0, 4, 1};
    info.parameters_json = R"({"recipe":"exact typed target","id":"9007199254740993"})";
    info.content_sha256 = content(raw, description, info);
    auto encoded = encode_label_mask_archive(normalized.raster, normalized.source_codes, description, info);
    auto decoded = decode_label_mask_archive(encoded.data(), encoded.size());
    check(decoded.context.target.width == info.target.width && decoded.context.target.crop.x == info.target.crop.x &&
        decoded.context.target.crop.width == 4 && decoded.context.parameters_json == info.parameters_json,
        "Typed target/crop integers lost precision above2^53");
    auto physical = Json::parse(descriptor(SampleType::UInt64, 4).json);
    for (const auto* key : {"label_frame", "reference_frame"}) {
        auto& axes = physical[key]["axes"];
        axes[0]["unit"] = "micrometer"; axes[0]["scale"] = .25;
        axes[1]["unit"] = "micrometer"; axes[1]["scale"] = .5;
        axes.insert(axes.begin(), Json{{"name", "z"}, {"unit", "micrometer"}, {"extent", "3"}, {"scale", 4}, {"translation", 0}});
        axes.insert(axes.begin(), Json{{"name", "t"}, {"unit", "second"}, {"extent", "3"}, {"scale", 2}, {"translation", 0}});
    }
    physical["z"] = 1; physical["t"] = 2;
    description = decode_label_mask_description(physical.dump());
    info = context(4);
    info.target.pixel_width_um = .5; info.target.pixel_height_um = .25;
    info.target.z_spacing_um = 4; info.target.time_interval_seconds = 2;
    info.target.z = 1; info.target.t = 2; info.plane = {1, 2, 1, 2};
    info.content_sha256 = content(raw, description, info);
    encoded = encode_label_mask_archive(normalized.raster, normalized.source_codes, description, info);
    decoded = decode_label_mask_archive(encoded.data(), encoded.size());
    check(decoded.context.target.pixel_width_um == .5 && decoded.context.target.pixel_height_um == .25 &&
        decoded.context.target.z_spacing_um == 4 && decoded.context.target.time_interval_seconds == 2 &&
        decoded.context.target.z == 1 && decoded.context.target.t == 2 && decoded.context.plane.label_t == 2,
        "Target calibrations or selected planes changed");
    auto bad = info;
    bad.target.crop.width = 3;
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, description, bad); });
    bad = info; bad.plane.reference_z = 0;
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, description, bad); });
    bad = info; bad.target.z_spacing_um = 8;
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, description, bad); });
    bad = info; bad.content_sha256 = std::string(64, '0');
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, description, bad); });
    bad = info; bad.record_sha256 = std::string(64, 'A');
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, description, bad); });
    bad = info; bad.parameters_json.assign(4097, 'x');
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, description, bad); });
    bad = info; bad.parameters_json.clear();
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, description, bad); });
    auto forged = description; forged.label_frame.axes.back().translation += 1;
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, forged, info); });
    forged = description; forged.json += " ";
    refuses([&] { encode_label_mask_archive(normalized.raster, normalized.source_codes, forged, info); });
}

void corruptions() {
    const auto original = literal_wire();
    auto decode = [](const std::vector<std::uint8_t>& bytes) { return decode_label_mask_archive(bytes.data(), bytes.size()); };
    refuses([&] { decode_label_mask_archive(nullptr, original.size()); });
    for (const auto length : {std::size_t(0), std::size_t(7), std::size_t(39), std::size_t(40), original.size() - 1})
        refuses([&] { decode_label_mask_archive(original.data(), length); });
    auto bad = original; bad.push_back(0); refuses([&] { decode(bad); });
    bad = original; bad[0] ^= 1; refuses([&] { decode(bad); });
    bad = original; bad[40 + 100] ^= 1; refuses([&] { decode(bad); });
    bad = original; std::fill(bad.begin() + 8, bad.begin() + 16, 255); refuses([&] { decode(bad); });
    for (const auto& codes : std::vector<std::string>{"0\n", "07\n", "+7\n", "-0\n", " 7\n", "7\r\n", "7", "7\n\n",
             "18446744073709551616\n", "256\n", "8\n"})
        refuses([&] { decode(pack(literal_header, literal_description, codes)); });
    refuses_containing([&] { decode(pack(literal_header, literal_description, "256\n")); },
        "declared unsigned source type");
    refuses_containing([&] { decode(pack(literal_header, literal_description, "0\n")); },
        "zero background");
    {
        const auto description = decode_label_mask_description(literal_description);
        const auto dense = encode_label_raster(LabelRaster(2, 1, {1, 2}));
        for (const auto& text : {std::string("7\n7\n"), std::string("8\n7\n")}) {
            RawPixelBuffer invalid_mapping_values(2, 1, 1, SampleType::UInt8);
            invalid_mapping_values.writable_valid() = {1, 1};
            std::get<std::vector<std::uint8_t>>(invalid_mapping_values.writable_storage()) =
                text.front() == '7' ? std::vector<std::uint8_t>{7, 7} : std::vector<std::uint8_t>{8, 7};
            auto info = context();
            info.content_sha256 = content(invalid_mapping_values, description, info);
            auto value = Json::parse(literal_header);
            value["code_count"] = "2"; value["content_sha256"] = info.content_sha256;
            // Every other count/identity is coherent; only map uniqueness/order
            // can refuse these deliberately invalid dense-to-source mappings.
            refuses_containing([&] { decode(pack(value.dump(), literal_description, text, dense)); },
                "increasing, unique");
        }
    }
    auto header = Json::parse(literal_header);
    header["code_count"] = "2";
    refuses([&] { decode(pack(header.dump(), literal_description, "7\n7\n")); });
    refuses([&] { decode(pack(header.dump(), literal_description, "7\n")); });
    header["code_count"] = "0";
    refuses([&] { decode(pack(header.dump(), literal_description, "")); });
    for (unsigned mode = 0; mode < 11; ++mode) {
        header = Json::parse(literal_header);
        if (mode == 0) header["extra"] = true;
        if (mode == 1) header.erase("reader_sha256");
        if (mode == 2) header["target"]["width"] = 2;
        if (mode == 3) header["target"]["width"] = "02";
        if (mode == 4) header["target"]["crop"]["x"] = "-1";
        if (mode == 5) header["target"]["crop"]["level"] = "1";
        if (mode == 6) header["target"]["crop"]["width"] = "1";
        if (mode == 7) header["sample_type"] = "float64";
        if (mode == 8) header["content_sha256"] = std::string(64, '0');
        if (mode == 9) header["parameters_json"] = std::string(4097, 'x');
        if (mode == 10) header["plane"]["label_z"] = "1";
        refuses([&] { decode(pack(header.dump(), literal_description, "7\n")); });
    }
    const auto duplicate = std::string("{\"schema\":\"qpx.label-mask-archive/1\",") + literal_header.substr(1);
    refuses([&] { decode(pack(duplicate, literal_description, "7\n")); });
    header = Json::parse(literal_header);
    Json deep = nullptr;
    for (unsigned i = 0; i < 12; ++i) deep = Json::array({deep});
    header["extra"] = deep;
    refuses([&] { decode(pack(header.dump(), literal_description, "7\n")); });
    header["extra"] = Json::array();
    for (unsigned i = 0; i < 300; ++i) header["extra"].push_back(0);
    refuses([&] { decode(pack(header.dump(), literal_description, "7\n")); });
    refuses([&] { decode(pack(std::string(1024 * 1024 + 1, ' '), literal_description, "7\n")); });
    refuses([&] { decode(pack(literal_header, literal_description + std::string(65537, ' '), "7\n")); });
    refuses([&] { decode(pack(literal_header, literal_description + " ", "7\n")); });
    auto malformed = literal_header;
    const auto where = malformed.find("\"pixel_width_um\":null");
    malformed.replace(where, std::string("\"pixel_width_um\":null").size(), "\"pixel_width_um\":1e9999");
    refuses([&] { decode(pack(malformed, literal_description, "7\n")); });
    const auto map = LabelCodeMap(std::vector<std::uint64_t>{7});
    const auto description = decode_label_mask_description(literal_description);
    auto info = context(); info.content_sha256 = "9ac63df7d201eab55b5f768bbd41d97274a5bddab7095c285e1fbefed2dddfdd";
    refuses([&] { encode_label_mask_archive(LabelRaster(2, 1, {0, 1}), map, description, info); });
}

void files_budgets_cancel(const fs::path& root) {
    const auto bytes = literal_wire();
    const auto file = root / "owned-labels.qplm";
    auto write = [&](const std::vector<std::uint8_t>& value) {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(value.data()), std::streamsize(value.size()));
        out.close(); check(bool(out), "Cannot write authored archive control");
    };
    write(bytes);
    const auto first = open_label_mask_archive(file.u8string());
    auto changed = bytes; changed[100] ^= 1; write(changed);
    refuses([&] { open_label_mask_archive(file.u8string()); });
    check(first.description->json == literal_description && first.source_codes.at(0).decimal() == "7",
        "Saved archive observation aliases later file changes");
    write(bytes);
    refuses<WorkflowCaptureLimit>([&] { decode_label_mask_archive(bytes.data(), bytes.size(), {1000000, 4096}); });
    refuses<WorkflowCaptureLimit>([&] { open_label_mask_archive(file.u8string(), {1000000, 4096}); });
    const auto description = decode_label_mask_description(literal_description);
    auto info = context(); info.content_sha256 = "9ac63df7d201eab55b5f768bbd41d97274a5bddab7095c285e1fbefed2dddfdd";
    const LabelRaster raster(2, 1, {0, 1});
    const LabelCodeMap codes(std::vector<std::uint32_t>{7});
    refuses<WorkflowCaptureLimit>([&] { encode_label_mask_archive(raster, codes, description, info, {1000000, 4096}); });
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>([&] { encode_label_mask_archive(raster, codes, description, info, {}, &cancelled); });
    refuses<WorkflowCancelled>([&] { decode_label_mask_archive(bytes.data(), bytes.size(), {}, &cancelled); });
    refuses<WorkflowCancelled>([&] { open_label_mask_archive(file.u8string(), {}, &cancelled); });

    const auto large_raw = raw_values(SampleType::UInt8, false, 1000000);
    const auto normalized = normalize_label_codes(large_raw);
    const auto large_description = descriptor(SampleType::UInt8, large_raw.width());
    auto large_context = context(large_raw.width());
    large_context.content_sha256 = content(large_raw, large_description, large_context);
    const auto large = encode_label_mask_archive(normalized.raster, normalized.source_codes, large_description, large_context);
    auto cancel_during = [&](auto&& call) {
        std::atomic<bool> flag{false};
        std::thread setter([&] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); flag.store(true); });
        bool stopped = false; std::exception_ptr unexpected;
        try { call(&flag); } catch (const WorkflowCancelled&) { stopped = true; } catch (...) { unexpected = std::current_exception(); }
        setter.join();
        if (unexpected) std::rethrow_exception(unexpected);
        check(stopped, "Large archive operation ignored cancellation");
    };
    cancel_during([&](auto* flag) { encode_label_mask_archive(normalized.raster, normalized.source_codes, large_description, large_context, {}, flag); });
    cancel_during([&](auto* flag) { decode_label_mask_archive(large.data(), large.size(), {}, flag); });
}

int dispatch(int argc, char** argv) {
    try {
        check(argc <= 2, "Usage: label_mask_archive_tests [new-output-directory]");
        const auto root = argc == 2 ? fs::absolute(fs::u8path(argv[1])).lexically_normal()
            : fs::current_path() / ("label-mask-archive-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(!fs::exists(root), "Preserve earlier archive test evidence");
        fs::create_directories(root);
        oracle(); integral_roundtrips(); metadata_boundaries(); contexts(); corruptions(); files_budgets_cancel(root);
        std::cout << "{\"passed\":true,\"integral_types\":8,\"zero_only_types\":8,\"literal_wire_bytes\":1650,\"format\":\"QPLM1\"}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> values;
    std::vector<char*> pointers;
    for (int i = 0; i < argc; ++i) {
        const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return 2;
        std::string value(std::size_t(size), '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, value.data(), size, nullptr, nullptr)) return 2;
        value.pop_back(); values.push_back(std::move(value));
    }
    for (auto& value : values) pointers.push_back(value.data());
    return dispatch(argc, pointers.data());
}
#else
int main(int argc, char** argv) { return dispatch(argc, argv); }
#endif
