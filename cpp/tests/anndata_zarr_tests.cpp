// SPDX-License-Identifier: AGPL-3.0-only
// Independent repository-authored Zarr2/AnnData fixtures and literal oracles.
// Native zlib only serializes fixtures; product interpretation stays native.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "format_plugin_runtime.hpp"
#include "format_plugin_loader.hpp"
#include "expression_matrix_external.hpp"
#include "qupathxx/directory_store.hpp"
#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/format_import.hpp"
#include "qupathxx/format_plugin.h"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <numeric>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <zlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace qpx;
using Json = nlohmann::json;
namespace {
constexpr std::size_t mib = 1024u * 1024;
constexpr const char* provider = "org.qupathxx.anndata-zarr";
constexpr const char* control_provider = "org.qupathxx.test.matrix-store";
const std::array<std::string, 3> feature_ids{"0002", "2", u8"g-\u03b2"};
const std::array<std::string, 3> feature_names{"First gene", u8"\u03b2 expression", ""};
const std::array<std::string, 4> observation_ids{
    "0001", "1", "18446744073709551615", u8"spot-\u03bc"};
// Literal source is observation-major; the indexed result is feature-major.
const std::array<std::uint64_t, 12> count_rows{
    UINT64_MAX, UINT64_C(9007199254740992), 0,
    0, UINT64_C(9007199254740993), 13, 7, 4, 0, 11, 5, 17};
const std::array<double, 12> real_rows{
    -0.0, 2.5, 1e-300, -1.25, 0.0, -std::numeric_limits<double>::min(),
    std::numeric_limits<double>::denorm_min(), 11, std::numeric_limits<double>::min(),
    3.5, -2, 99.125};

void check(bool okay, const std::string& message) {
    if (!okay) throw std::runtime_error(message);
}
template<class E = std::exception, class F> std::string refuses(F&& function) {
    try { function(); }
    catch (const E& error) {
        check(!std::string(error.what()).empty(), "Native refusal omitted its explanation");
        return error.what();
    }
    throw std::runtime_error("Missing native AnnData/store refusal");
}
std::string bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    check(bool(input), "Read owned fixture: " + path.u8string());
    return {std::istreambuf_iterator<char>(input), {}};
}
void write(const fs::path& path, const std::string& value) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(value.data(), std::streamsize(value.size()));
    output.close();
    check(bool(output), "Write owned fixture: " + path.u8string());
}
void write_json(const fs::path& path, const Json& value) {
    write(path, value.dump(2) + "\n");
}
Json json(const fs::path& path) { return Json::parse(bytes(path)); }
std::string sha(const fs::path& path) { return fingerprint_file(path.u8string())->sha256; }
std::uint64_t bits(double value) {
    std::uint64_t word; std::memcpy(&word, &value, sizeof(word)); return word;
}
void no_stage(const fs::path& directory) {
    for (const auto& entry : fs::directory_iterator(directory))
        check(entry.path().filename().u8string().rfind(".qpx-expression-", 0) != 0,
              "Failed store import leaked a private QMX stage");
}
void fresh(const fs::path& directory) {
    check(directory.is_absolute() && !fs::exists(directory), "Use a fresh absolute test directory");
    check(fs::create_directories(directory), "Create owned native test directory");
}

Json report{{"schema", "qpx.anndata-zarr-native-tests/1"}, {"passed", false},
            {"checks", Json::array()}};
fs::path report_path;
template<class F> void run(const std::string& name, F&& function) {
    Json row{{"name", name}, {"passed", false}};
    try {
        function(); row["passed"] = true;
    } catch (const std::exception& error) {
        row["error"] = error.what(); report["checks"].push_back(row);
        if (!report_path.empty()) write_json(report_path, report);
        throw;
    }
    report["checks"].push_back(row);
    if (!report_path.empty()) write_json(report_path, report);
}

enum class Codec { Raw, Zlib, Gzip };
struct Layout {
    Codec codec = Codec::Raw;
    bool big_endian = false, fortran = false, slash = false;
    std::vector<std::uint64_t> chunks{2, 2};
};
Json compressor(Codec codec) {
    if (codec == Codec::Raw) return nullptr;
    return Json{{"id", codec == Codec::Gzip ? "gzip" : "zlib"}, {"level", 5}};
}
std::string compressed(const std::string& raw, Codec codec) {
    if (codec == Codec::Raw) return raw;
    z_stream stream{};
    check(deflateInit2(&stream, 5, Z_DEFLATED, codec == Codec::Gzip ? 31 : 15, 8,
                       Z_DEFAULT_STRATEGY) == Z_OK, "Initialize native fixture compression");
    struct End { z_stream* value; ~End() { deflateEnd(value); } } end{&stream};
    std::string result(deflateBound(&stream, uLong(raw.size())), '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(raw.data()));
    stream.avail_in = uInt(raw.size());
    stream.next_out = reinterpret_cast<Bytef*>(result.data());
    stream.avail_out = uInt(result.size());
    check(deflate(&stream, Z_FINISH) == Z_STREAM_END && stream.total_in == raw.size(),
          "Serialize complete native compressed fixture");
    result.resize(std::size_t(stream.total_out));
    return result;
}
void append_u32(std::string& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(char((value >> shift) & 255));
}
std::string chunk_key(const std::vector<std::uint64_t>& index, bool slash) {
    std::string key;
    for (const auto value : index) {
        if (!key.empty()) key += slash ? '/' : '.';
        key += std::to_string(value);
    }
    return key;
}
void group(const fs::path& path, const Json& attrs) {
    fs::create_directories(path);
    write_json(path / ".zgroup", Json{{"zarr_format", 2}});
    write_json(path / ".zattrs", attrs);
}
void strings(const fs::path& path, const std::vector<std::string>& values,
             Codec codec = Codec::Raw, std::size_t chunk = 2) {
    fs::create_directories(path);
    write_json(path / ".zarray", Json{{"zarr_format", 2}, {"shape", {values.size()}},
        {"chunks", {chunk}}, {"dtype", "|O"}, {"order", "C"},
        {"compressor", compressor(codec)}, {"filters", Json::array({Json{{"id", "vlen-utf8"}}})},
        {"fill_value", nullptr}, {"dimension_separator", "."}});
    write_json(path / ".zattrs", Json{{"encoding-type", "string-array"}, {"encoding-version", "0.2.0"}});
    for (std::size_t first = 0; first < values.size(); first += chunk) {
        std::string data; append_u32(data, std::uint32_t(chunk));
        for (std::size_t index = 0; index < chunk; ++index) {
            const auto text = first + index < values.size() ? values[first + index] : "";
            append_u32(data, std::uint32_t(text.size())); data += text;
        }
        write(path / std::to_string(first / chunk), compressed(data, codec));
    }
}
template<class T> std::string dtype(bool big) {
    return std::string(sizeof(T) == 1 ? "|" : big ? ">" : "<") +
        (std::is_floating_point_v<T> ? "f" : std::is_signed_v<T> ? "i" : "u") +
        std::to_string(sizeof(T));
}
template<class T> void numeric(const fs::path& path, const std::vector<T>& values,
        const std::vector<std::uint64_t>& shape, Layout layout = {}, Json fill = 0) {
    check(shape.size() == 1 || shape.size() == 2, "Authored numeric rank");
    if (shape.size() == 1 && layout.chunks.size() != 1) layout.chunks = {3};
    check(layout.chunks.size() == shape.size(), "Authored chunk rank");
    fs::create_directories(path);
    write_json(path / ".zarray", Json{{"zarr_format", 2}, {"shape", shape},
        {"chunks", layout.chunks}, {"dtype", dtype<T>(layout.big_endian)},
        {"order", layout.fortran ? "F" : "C"}, {"compressor", compressor(layout.codec)},
        {"filters", nullptr}, {"fill_value", fill}, {"dimension_separator", layout.slash ? "/" : "."}});
    write_json(path / ".zattrs", Json{{"encoding-type", "array"}, {"encoding-version", "0.2.0"}});
    if (std::find(shape.begin(), shape.end(), 0) != shape.end()) {
        check(values.empty(), "Empty child array has stored values");
        return; // A zero-length sparse vector has no chunk files.
    }
    std::vector<std::uint64_t> grid(shape.size()), index(shape.size(), 0);
    std::size_t count = 1;
    for (std::size_t axis = 0; axis < shape.size(); ++axis) {
        grid[axis] = (shape[axis] + layout.chunks[axis] - 1) / layout.chunks[axis];
        count *= std::size_t(layout.chunks[axis]);
    }
    const std::uint16_t marker = 1;
    const bool host_big = *reinterpret_cast<const std::uint8_t*>(&marker) == 0;
    bool more = true;
    while (more) {
        std::string data(count * sizeof(T), '\0');
        for (std::size_t local = 0; local < count; ++local) {
            const auto row = shape.size() == 1 ? local : local / std::size_t(layout.chunks[1]);
            const auto column = shape.size() == 1 ? 0 : local % std::size_t(layout.chunks[1]);
            const auto global_row = index[0] * layout.chunks[0] + row;
            const auto global_column = shape.size() == 1 ? 0 : index[1] * layout.chunks[1] + column;
            const auto global = shape.size() == 1 ? global_row : global_row * shape[1] + global_column;
            T value{};
            if (global_row < shape[0] && (shape.size() == 1 || global_column < shape[1])) {
                check(global < values.size(), "Authored value count differs from shape");
                value = values[std::size_t(global)];
            }
            auto offset = local;
            if (layout.fortran && shape.size() == 2) offset = row + std::size_t(layout.chunks[0]) * column;
            std::array<char, sizeof(T)> word{}; std::memcpy(word.data(), &value, sizeof(T));
            if (layout.big_endian != host_big) std::reverse(word.begin(), word.end());
            std::memcpy(data.data() + offset * sizeof(T), word.data(), sizeof(T));
        }
        write(path / fs::u8path(chunk_key(index, layout.slash)), compressed(data, layout.codec));
        more = false;
        for (std::size_t axis = index.size(); axis-- > 0;) {
            if (++index[axis] < grid[axis]) { more = true; break; }
            index[axis] = 0;
        }
    }
}
enum class Encoding { Dense, Csr, Csc };
template<class T> void matrix(const fs::path& path, const std::vector<T>& rows,
                             Encoding encoding, Layout layout) {
    if (encoding == Encoding::Dense) { numeric(path, rows, {4, 3}, layout); return; }
    const bool csr = encoding == Encoding::Csr;
    group(path, Json{{"encoding-type", csr ? "csr_matrix" : "csc_matrix"},
                     {"encoding-version", "0.1.0"}, {"shape", {4, 3}}});
    std::vector<T> data;
    std::vector<std::uint64_t> indices, pointers{0};
    for (std::uint64_t major = 0; major < (csr ? 4u : 3u); ++major) {
        for (std::uint64_t minor = 0; minor < (csr ? 3u : 4u); ++minor) {
            const auto row = csr ? major : minor, column = csr ? minor : major;
            // Explicit zeros are legal and must retain signed-zero bits.
            data.push_back(rows[std::size_t(row * 3 + column)]);
            indices.push_back(minor);
        }
        pointers.push_back(data.size());
    }
    layout.chunks = {3};
    numeric(path / "data", data, {data.size()}, layout);
    numeric(path / "indices", indices, {indices.size()}, layout);
    numeric(path / "indptr", pointers, {pointers.size()}, layout);
}
template<class T> void fixture(const fs::path& path, const std::vector<T>& rows,
        Encoding encoding = Encoding::Dense, Layout layout = {}) {
    check(!fs::exists(path), "Authored fixture already exists");
    group(path, Json{{"encoding-type", "anndata"}, {"encoding-version", "0.1.0"}});
    group(path / "obs", Json{{"encoding-type", "dataframe"}, {"encoding-version", "0.2.0"},
                            {"_index", "ids"}, {"column-order", Json::array()}});
    group(path / "var", Json{{"encoding-type", "dataframe"}, {"encoding-version", "0.2.0"},
                            {"_index", "ids"}, {"column-order", {"names"}}});
    strings(path / "obs/ids", {observation_ids.begin(), observation_ids.end()}, layout.codec);
    strings(path / "var/ids", {feature_ids.begin(), feature_ids.end()}, layout.codec);
    strings(path / "var/names", {feature_names.begin(), feature_names.end()}, layout.codec);
    matrix(path / "X", rows, encoding, layout);
}
Json interpretation(bool continuous = false, bool names = true) {
    Json value{{"matrix", "X"}, {"observation_ids", "obs/ids"}, {"feature_ids", "var/ids"},
               {"domain", continuous ? "continuous" : "counts"},
               {"value_domain", continuous ? "literal-signed" : "raw-counts"}};
    if (names) value["feature_names"] = "var/names";
    return value;
}
MatrixStorePluginImportOptions options() {
    MatrixStorePluginImportOptions value;
    value.primary_key = ".zattrs";
    value.matrix.frame.id = u8"anndata-frame-\u03c6";
    value.matrix.frame.source_id = "independent-authored-positions";
    value.matrix.frame.unit = CoordinateUnit::Pixel;
    value.matrix.frame.plane = ObjectPlane(-1, 0, 0);
    value.matrix.frame.image_level_zero = true;
    value.matrix.frame.width = 64; value.matrix.frame.height = 64;
    value.matrix.frame.micrometres_per_pixel = std::array<double, 2>{.25, .5};
    value.matrix.plugin_work_bytes = 16 * mib;
    value.matrix.limits.work_bytes = 64 * mib;
    value.matrix.limits.store_bytes = 64 * mib;
    value.inventory.inventory_bytes = mib;
    value.inventory.max_files = 10000;
    return value;
}
std::string position_text() {
    std::string value = "observation_id\tx_px\ty_px\n";
    for (const auto index : {2u, 0u, 3u, 1u})
        value += observation_ids[index] + "\t" + std::to_string(1.25 + 3 * index) + "\t" +
                 std::to_string(2.5 + 2 * index) + "\n";
    return value;
}
ExpressionMatrixImportResult import(const fs::path& input, const fs::path& positions,
        const fs::path& output, const Json& selection,
        const MatrixStorePluginImportOptions& limits = options(),
        const std::atomic<bool>* cancelled = nullptr) {
    return import_expression_matrix_store_plugin(provider, input.u8string(), positions.u8string(),
                                                output.u8string(), limits, selection.dump(), cancelled);
}
template<class T> std::vector<T> feature_major(const std::vector<T>& rows) {
    check(rows.size() == 12, "Literal oracle has wrong size");
    std::vector<T> result;
    for (std::size_t feature = 0; feature < 3; ++feature)
        for (std::size_t observation = 0; observation < 4; ++observation)
            result.push_back(rows[observation * 3 + feature]);
    return result;
}
void assert_axes(const ExpressionMatrixSource& source, bool names = true) {
    const auto features = source.feature_page(0, 3);
    check(features.size() == 3, "Feature axis size changed");
    for (std::size_t index = 0; index < features.size(); ++index)
        check(features[index].index == index && features[index].id == feature_ids[index] &&
              features[index].name == (names && !feature_names[index].empty() ? feature_names[index] : feature_ids[index]) &&
              features[index].type == "feature", "Stable feature IDs/names/order changed");
    check(source.observation_page(0, 4) == std::vector<std::string>(observation_ids.begin(), observation_ids.end()),
          "Observation IDs were sorted or numerically normalized");
    const auto positions = source.spatial_page(0, 4);
    check(positions.size() == 4, "Embedded position count changed");
    for (std::size_t index = 0; index < positions.size(); ++index)
        check(positions[index].id == observation_ids[index] &&
              positions[index].position.x == 1.25 + 3 * index &&
              positions[index].position.y == 2.5 + 2 * index,
              "Coordinates were joined by row instead of exact IDs");
}
template<class T> void assert_values(const ExpressionMatrixSource& source,
        const std::vector<T>& rows, bool names = true,
        const std::optional<std::vector<std::uint64_t>>& stored = {}) {
    assert_axes(source, names);
    const auto& metadata = source.metadata();
    const bool real = std::is_floating_point_v<T>;
    check(metadata.features == 3 && metadata.observations == 4 && metadata.entries == (stored ? stored->size() : 12) &&
          metadata.input_profile == (stored ? "external-sparse-store/1" : "external-dense-store/1") &&
          metadata.domain == (real ? ExpressionValueDomain::Continuous : ExpressionValueDomain::Counts) &&
          metadata.value_domain == (real ? "literal-signed" : "raw-counts"),
          "Native matrix schema/domain/profile differs");
    const auto block = source.read({{0, 1, 2}, std::nullopt});
    std::vector<std::uint64_t> offsets{0}, observations;
    std::vector<T> expected;
    for (std::uint64_t feature = 0; feature < 3; ++feature) {
        for (std::uint64_t observation = 0; observation < 4; ++observation) {
            const auto linear = observation * 3 + feature;
            if (stored && std::find(stored->begin(), stored->end(), linear) == stored->end()) continue;
            observations.push_back(observation);
            expected.push_back(rows[linear]);
        }
        offsets.push_back(expected.size());
    }
    check(block.features == std::vector<std::uint64_t>{0, 1, 2} &&
          block.offsets == offsets && block.observation_indices == observations,
          "Array orientation or explicit zeros changed");
    if constexpr (std::is_floating_point_v<T>) {
        check(block.counts.empty() && block.continuous.size() == expected.size(), "Continuous used count storage");
        for (std::size_t index = 0; index < expected.size(); ++index)
            check(bits(block.continuous[index]) == bits(double(expected[index])), "Continuous or signed-zero bits changed");
    } else {
        check(block.continuous.empty() && block.counts.size() == expected.size(), "Counts used floating storage");
        for (std::size_t index = 0; index < expected.size(); ++index)
            check(block.counts[index] == std::uint64_t(expected[index]), "Integral count bits changed");
    }
    const auto empty = source.read({{1, 0}, std::vector<std::uint64_t>{}});
    check(empty.counts.empty() && empty.continuous.empty() && empty.offsets == std::vector<std::uint64_t>{0, 0, 0},
          "Explicit empty cohort became all observations");
}
void receipt(const ExpressionMatrixSource& source, const fs::path& input,
             const fs::path& positions, const Json& selected, const MatrixStorePluginImportOptions& limits) {
    const auto captured = external_expression_matrix_store_receipt(source);
    check(bool(captured), "Store receipt missing");
    const auto fingerprint = DirectoryStoreSnapshot(input.u8string(), limits.inventory).content_fingerprint(limits.content);
    const auto& base = captured->matrix;
    check(base.reader.provider.provider_id == provider && base.reader.provider.abi == 4 &&
          base.reader.provider.contract == "anndata-zarr2-typed/1" &&
          captured->primary_key == ".zattrs" && captured->observed_sha256.size() == 64 &&
          captured->content_sha256 == fingerprint.content_sha256 &&
          captured->files == fingerprint.files && captured->bytes == fingerprint.bytes &&
          base.reader.source_sha256 == captured->content_sha256 &&
          base.positions_sha256 == sha(positions) && Json::parse(base.reader.options) == selected,
          "Native receipt confused content, observed identity, options or positions");
    check(captured->observed_sha256 != captured->content_sha256 &&
          source.metadata().spatial_frame &&
          source.metadata().spatial_frame->sha256() == limits.matrix.frame.sha256(),
          "Store content/observation or exact frame semantics were lost");
    const auto common = external_expression_matrix_receipt(source);
    check(common && format_plugin_receipt_identity(common->reader) == format_plugin_receipt_identity(base.reader),
          "Common receipt compatibility differs from typed store receipt");
}
fs::path copy_store(const fs::path& source, const fs::path& target) {
    check(!fs::exists(target), "Test copy destination already exists");
    fs::copy(source, target, fs::copy_options::recursive);
    return target;
}

template<class T> void check_fixture(const fs::path& input, const fs::path& positions,
        const fs::path& output, const std::vector<T>& rows, bool names = true,
        const Json& selected = Json(), const std::optional<std::vector<std::uint64_t>>& stored = {}) {
    const auto choice = selected.is_null() ? interpretation(std::is_floating_point_v<T>, names) : selected;
    const auto limits = options();
    const auto indexed = import(input, positions, output, choice, limits);
    const auto source = open_expression_matrix(indexed.path, indexed.file_sha256);
    assert_values(*source, rows, names, stored);
    receipt(*source, input, positions, choice, limits);
}
template<class T> void small_type(const fs::path& work, const fs::path& positions, const std::string& code) {
    std::vector<T> values(12);
    for (std::size_t index = 0; index < values.size(); ++index) values[index] = T(index + 1);
    const auto input = work / (code + ".zarr");
    fixture(input, values);
    run("numeric-" + code, [&] { check_fixture(input, positions, work / (code + ".qmx"), values); });
    if constexpr (std::is_floating_point_v<T>) {
        run("floating-exact-counts-" + code, [&] {
            const auto indexed = import(input, positions, work / (code + "-counts.qmx"), interpretation());
            std::vector<std::uint64_t> expected(12); std::iota(expected.begin(), expected.end(), 1);
            assert_values(*open_expression_matrix(indexed.path, indexed.file_sha256), expected);
        });
    } else {
        run("integral-exact-continuous-" + code, [&] {
            const auto indexed = import(input, positions, work / (code + "-continuous.qmx"), interpretation(true));
            std::vector<double> expected(12); std::iota(expected.begin(), expected.end(), 1.0);
            assert_values(*open_expression_matrix(indexed.path, indexed.file_sha256), expected);
        });
    }
}
void many_files(const fs::path& work) {
    const auto input = work / "many-chunks.zarr";
    fixture(input, std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    std::vector<std::string> ids;
    std::vector<std::string> features, names;
    for (std::size_t feature = 0; feature < 40; ++feature) {
        features.push_back("gene-" + std::to_string(feature));
        names.push_back("Gene " + std::to_string(feature));
    }
    std::string positions = "observation_id\tx_px\ty_px\n";
    std::vector<std::uint64_t> values;
    for (std::size_t index = 0; index < 128; ++index) {
        ids.push_back("cell-" + std::to_string(index));
        positions += ids.back() + "\t" + std::to_string(.25 + index % 32) + "\t" +
                     std::to_string(.5 + index / 32) + "\n";
        for (std::size_t feature = 0; feature < 40; ++feature) values.push_back(index * 40 + feature);
    }
    strings(input / "obs/ids", ids);
    strings(input / "var/ids", features);
    strings(input / "var/names", names);
    Layout layout; layout.chunks = {8, 8};
    numeric(input / "X", values, {128, 40}, layout);
    write(work / "many-positions.tsv", positions);
    const auto indexed = import(input, work / "many-positions.tsv", work / "many-chunks.qmx", interpretation());
    const auto source = open_expression_matrix(indexed.path, indexed.file_sha256);
    const auto captured = external_expression_matrix_store_receipt(*source);
    check(captured && captured->files > 64 && source->metadata().observations == 128 &&
          source->metadata().features == 40 && source->metadata().entries == 5120,
          "Large selected chunk set was truncated to a companion list");
    const auto block = source->read({{39, 0}, std::nullopt});
    check(block.counts.size() == 256 && block.offsets == std::vector<std::uint64_t>{0, 128, 256},
          "Many-chunk indexed read shape changed");
    for (std::size_t index = 0; index < 128; ++index)
        check(block.counts[index] == index * 40 + 39 && block.counts[index + 128] == index * 40,
              "Many-chunk boundaries changed literal values");
}
void project_route(const fs::path& work) {
    Project project;
    const auto image = project.add_document(std::make_shared<Document>(
        Image(64, 64, std::vector<std::uint8_t>(64 * 64 * 3, 30))), "Native AnnData association");
    const auto counts = open_expression_matrix((work / "counts.qmx").u8string());
    const auto real = open_expression_matrix((work / "continuous.qmx").u8string());
    const auto count_id = keep_expression_matrix(project, image, counts, true);
    const auto real_id = keep_expression_matrix(project, image, real, true);
    check(count_id != real_id && expression_matrix_ids(project, image).size() == 2,
          "Counts and continuous project associations collided");
    project.save_new((work / "owned.qpx").u8string());
    auto reopened = Project::open((work / "owned.qpx").u8string());
    for (const auto& id : {count_id, real_id}) {
        const auto association = load_expression_matrix_association(*reopened, image, id);
        check(association.owned, "Project did not retain owned QMX storage");
        const auto source = reopen_expression_matrix(*reopened, image, id);
        check(external_expression_matrix_store_receipt(*source).has_value(), "Owned store receipt was lost");
        if (source->metadata().domain == ExpressionValueDomain::Counts)
            assert_values(*source, std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
        else assert_values(*source, std::vector<double>(real_rows.begin(), real_rows.end()));
    }
    write_json(work / "association-ids.json", Json{{"image", image}, {"counts", count_id}, {"continuous", real_id}});
}
void cli_sdk_fixtures(const fs::path& work) {
    const auto image_path = work / "image.ppm", project_path = work / "input.qpx";
    write(image_path, "P6\n64 64\n255\n" + std::string(64 * 64 * 3, char(30)));
    Project project;
    const auto image = project.add_image(image_path.u8string(), "auto", "AnnData input");
    const auto frame = image_coordinate_frame(*project.document(image), "qpx.format-import-cli-image/1", ObjectPlane{});
    project.save_new(project_path.u8string());
    const auto reopened = Project::open(project_path.u8string());
    check(image_coordinate_frame(*reopened->document(image), "qpx.format-import-cli-image/1", ObjectPlane{}).sha256() == frame.sha256(),
          "External-image frame changed through project persistence");
    MatrixStorePluginImportOptions native;
    native.primary_key = ".zattrs"; native.matrix.frame = frame;
    Json reference{{"schema", "qpx.anndata-zarr-native-reference/1"}, {"image", image_path.u8string()},
        {"input_project", project_path.u8string()}, {"image_id", image},
        {"positions", (work / "positions.tsv").u8string()}, {"primary_key", ".zattrs"},
        {"frame_sha256", frame.sha256()}, {"plugin_work_bytes", native.matrix.plugin_work_bytes},
        {"host_work_bytes", native.matrix.limits.work_bytes},
        {"inventory_bytes", native.inventory.inventory_bytes},
        {"file_bytes", native.content.file_bytes}, {"total_bytes", native.content.total_bytes}};
    for (const bool continuous : {false, true}) {
        const std::string kind = continuous ? "continuous" : "counts";
        const auto input = work / (kind + ".zarr"), output = work / ("cli-" + kind + ".qmx");
        const auto selected = interpretation(continuous);
        const auto indexed = import(input, work / "positions.tsv", output, selected, native);
        const auto source = open_expression_matrix(indexed.path, indexed.file_sha256);
        const auto captured = external_expression_matrix_store_receipt(*source);
        check(captured && source->metadata().spatial_frame &&
              source->metadata().spatial_frame->sha256() == frame.sha256(), "Native reference frame/receipt missing");
        const auto& base = captured->matrix;
        reference[kind] = Json{{"input", input.u8string()}, {"path", indexed.path},
            {"file_sha256", indexed.file_sha256}, {"source_sha256", indexed.metadata.source_sha256},
            {"features", indexed.metadata.features}, {"observations", indexed.metadata.observations},
            {"entries", indexed.metadata.entries}, {"domain", kind},
            {"value_domain", indexed.metadata.value_domain}, {"input_profile", indexed.metadata.input_profile},
            {"options", selected}, {"options_json", base.reader.options},
            {"reader_receipt_json", format_plugin_receipt_json(base.reader)},
            {"reader_receipt_sha256", format_plugin_receipt_identity(base.reader)},
            {"positions_sha256", base.positions_sha256}, {"store_content_sha256", captured->content_sha256},
            {"store_observed_sha256", captured->observed_sha256}, {"store_files", captured->files},
            {"store_bytes", captured->bytes}};
    }
    write_json(work / "native-reference.json", reference);
}
void positive(const fs::path& work) {
    write(work / "positions.tsv", position_text());
    fixture(work / "counts.zarr", std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    fixture(work / "continuous.zarr", std::vector<double>(real_rows.begin(), real_rows.end()));
    run("dense-exact-counts", [&] {
        check_fixture(work / "counts.zarr", work / "positions.tsv", work / "counts.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    });
    run("dense-continuous-signed-zero", [&] {
        check_fixture(work / "continuous.zarr", work / "positions.tsv", work / "continuous.qmx",
                      std::vector<double>(real_rows.begin(), real_rows.end()));
    });
    run("schema6-dense-entry-product-is-an-invariant", [&] {
        const auto source = open_expression_matrix((work / "counts.qmx").u8string());
        auto metadata = source->metadata();
        auto expected = metadata;
        expected.source_sha256 = detail::matrix::store_external_source_hash(metadata);
        detail::matrix::bind_file_identity(expected, source->file_sha256());
        check(expected.source_sha256 == metadata.source_sha256,
              "Valid schema6 metadata cannot compute its native source identity");
        --metadata.entries;
        // No saved checksum is compared here: the authority must refuse the
        // logically inconsistent shape before returning any newly computed hash.
        refuses<std::invalid_argument>([&] { detail::matrix::store_external_source_hash(metadata); });
    });
    std::size_t number = 0;
    for (const auto codec : {Codec::Raw, Codec::Zlib, Codec::Gzip})
        for (const auto big : {false, true}) for (const auto fortran : {false, true})
            for (const auto slash : {false, true}) {
                const auto name = "layout-" + std::to_string(number++);
                run(name, [&] {
                    const auto input = work / (name + ".zarr");
                    Layout layout; layout.codec = codec; layout.big_endian = big;
                    layout.fortran = fortran; layout.slash = slash;
                    fixture(input, std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), Encoding::Dense, layout);
                    check_fixture(input, work / "positions.tsv", work / (name + ".qmx"),
                                  std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
                });
            }
    const std::vector<std::uint64_t> all_stored{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    for (const auto encoding : {Encoding::Csr, Encoding::Csc})
        for (const auto codec : {Codec::Raw, Codec::Zlib, Codec::Gzip}) {
            const auto name = std::string(encoding == Encoding::Csr ? "csr-" : "csc-") + std::to_string(int(codec));
            run(name, [&] {
                Layout layout; layout.codec = codec; layout.big_endian = true; layout.fortran = true;
                fixture(work / (name + ".zarr"), std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), encoding, layout);
                check_fixture(work / (name + ".zarr"), work / "positions.tsv", work / (name + ".qmx"),
                              std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), true, {}, all_stored);
            });
            run(name + "-continuous", [&] {
                Layout layout; layout.codec = codec;
                fixture(work / (name + "-continuous.zarr"), std::vector<double>(real_rows.begin(), real_rows.end()), encoding, layout);
                check_fixture(work / (name + "-continuous.zarr"), work / "positions.tsv", work / (name + "-continuous.qmx"),
                              std::vector<double>(real_rows.begin(), real_rows.end()), true, {}, all_stored);
            });
        }
    for (const auto encoding : {Encoding::Csr, Encoding::Csc}) {
        const auto name = std::string(encoding == Encoding::Csr ? "csr-" : "csc-") + "zero-stored-entries";
        run(name, [&] {
            const auto input = work / (name + ".zarr");
            fixture(input, std::vector<std::uint64_t>(12, 0), encoding);
            check(input.is_absolute() && input.parent_path() == work, "Zero-entry fixture escaped owned test directory");
            for (const auto* child : {"data", "indices"}) {
                numeric(input / "X" / child, std::vector<std::uint64_t>{}, {0});
                for (const auto* chunk : {"0", "1", "2", "3"})
                    check(fs::remove(input / "X" / child / chunk), "Remove superseded owned sparse chunk");
                for (const auto& entry : fs::directory_iterator(input / "X" / child))
                    check(entry.path().filename() == ".zarray" || entry.path().filename() == ".zattrs",
                          "Empty sparse vector retained an unexpected chunk");
            }
            const auto pointers = encoding == Encoding::Csr ? 5u : 4u;
            numeric(input / "X/indptr", std::vector<std::uint64_t>(pointers, 0), {pointers});
            check_fixture(input, work / "positions.tsv", work / (name + ".qmx"),
                          std::vector<std::uint64_t>(12, 0), true, {}, std::vector<std::uint64_t>{});
        });
    }
    run("sparse-internal-vector-attrs-optional", [&] {
        const auto input = copy_store(work / "csr-0.zarr", work / "sparse-no-vector-attrs.zarr");
        for (const auto* key : {"data", "indices", "indptr"})
            check(fs::remove(input / "X" / key / ".zattrs"), "Remove optional owned vector attrs");
        check_fixture(input, work / "positions.tsv", work / "sparse-no-vector-attrs.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), true, {}, all_stored);
    });
    run("unsorted-unique-csr-coordinates", [&] {
        const auto input = copy_store(work / "csr-0.zarr", work / "unsorted-csr.zarr");
        auto values = std::vector<std::uint64_t>(count_rows.begin(), count_rows.end());
        std::vector<std::uint64_t> indices{0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
        std::swap(values[0], values[2]); std::swap(indices[0], indices[2]);
        numeric(input / "X/data", values, {12}); numeric(input / "X/indices", indices, {12});
        check_fixture(input, work / "positions.tsv", work / "unsorted-csr.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), true, {}, all_stored);
    });
    run("sparse-absent-coordinates-are-measured-zeros", [&] {
        const auto input = copy_store(work / "csr-0.zarr", work / "sparse-implicit-zero.zarr");
        numeric(input / "X/data", std::vector<std::uint64_t>{UINT64_MAX, UINT64_C(9007199254740992),
            UINT64_C(9007199254740993), 13, 7, 4, 11, 5, 17}, {9});
        numeric(input / "X/indices", std::vector<std::uint64_t>{0, 1, 1, 2, 0, 1, 0, 1, 2}, {9});
        numeric(input / "X/indptr", std::vector<std::uint64_t>{0, 2, 4, 6, 9}, {5});
        check(fs::remove(input / "X/data/3") && fs::remove(input / "X/indices/3"), "Remove superseded owned chunks");
        check_fixture(input, work / "positions.tsv", work / "sparse-implicit-zero.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), true, {},
                      std::vector<std::uint64_t>{0, 1, 4, 5, 6, 7, 9, 10, 11});
    });
    small_type<std::uint8_t>(work, work / "positions.tsv", "u1");
    small_type<std::int8_t>(work, work / "positions.tsv", "i1");
    small_type<std::uint16_t>(work, work / "positions.tsv", "u2");
    small_type<std::int16_t>(work, work / "positions.tsv", "i2");
    small_type<std::uint32_t>(work, work / "positions.tsv", "u4");
    small_type<std::int32_t>(work, work / "positions.tsv", "i4");
    small_type<std::uint64_t>(work, work / "positions.tsv", "u8");
    small_type<std::int64_t>(work, work / "positions.tsv", "i8");
    small_type<float>(work, work / "positions.tsv", "f4");
    small_type<double>(work, work / "positions.tsv", "f8");
    run("selected-layer-and-omitted-unselected-data", [&] {
        const auto input = copy_store(work / "counts.zarr", work / "layer.zarr");
        group(input / "layers", Json{{"encoding-type", "dict"}, {"encoding-version", "0.1.0"}});
        fs::copy(input / "X", input / "layers/raw", fs::copy_options::recursive);
        write(input / "unselected/opaque.bin", "Repository-authored omitted payload; still identity-bound\n");
        auto selected = interpretation(); selected["matrix"] = "layers/raw";
        check_fixture(input, work / "positions.tsv", work / "layer.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), true, selected);
    });
    run("explicit-name-omission", [&] {
        check_fixture(work / "counts.zarr", work / "positions.tsv", work / "unnamed.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()), false);
    });
    run("unicode-native-directory-path", [&] {
        const auto input = copy_store(work / "counts.zarr", work / fs::u8path(u8"store-\u03bc-\u56fe.zarr"));
        check_fixture(input, work / "positions.tsv", work / "unicode-directory.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    });
    run("selected-store-over64-chunk-files", [&] { many_files(work); });
    run("content-hash-distinct-from-observed-identity", [&] {
        const auto copy = copy_store(work / "counts.zarr", work / "byte-identical-copy.zarr");
        DirectoryStoreSnapshot first((work / "counts.zarr").u8string()), second(copy.u8string());
        check(first.observed_sha256() != second.observed_sha256() &&
              first.content_fingerprint().content_sha256 == second.content_fingerprint().content_sha256,
              "Content fingerprint depends on absolute root/file versions or observation lost them");
        const auto positions = work / "positions.tsv";
        const auto original = open_expression_matrix((work / "counts.qmx").u8string());
        const auto repeated = import(work / "counts.zarr", positions, work / "repeat.qmx", interpretation());
        const auto relocated = import(copy, positions, work / "relocated.qmx", interpretation());
        check(repeated.file_sha256 == original->file_sha256() &&
              repeated.metadata.source_sha256 == original->metadata().source_sha256,
              "A repeat import changed QMX or association identity");
        const auto moved = open_expression_matrix(relocated.path, relocated.file_sha256);
        const auto a = external_expression_matrix_store_receipt(*original);
        const auto b = external_expression_matrix_store_receipt(*moved);
        check(a && b && a->content_sha256 == b->content_sha256 &&
              a->observed_sha256 != b->observed_sha256 &&
              moved->metadata().source_sha256 != original->metadata().source_sha256,
              "Relocation failed to retain content identity and distinguish the capture");
        assert_values(*moved, std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    });
    run("unselected-member-content-still-binds-source", [&] {
        const auto input = copy_store(work / "counts.zarr", work / "opaque-member.zarr");
        const auto original = DirectoryStoreSnapshot(input.u8string()).content_fingerprint();
        write(input / "unselected/opaque.bin", "Own uninterpreted member bytes\n");
        check_fixture(input, work / "positions.tsv", work / "opaque-member.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
        const auto source = open_expression_matrix((work / "opaque-member.qmx").u8string());
        const auto captured = external_expression_matrix_store_receipt(*source);
        check(captured && captured->content_sha256 != original.content_sha256 && captured->files == original.files + 1,
              "Whole-store content identity omitted an unselected member");
    });
    run("owned-project-save-reopen", [&] { project_route(work); });
    run("native-cli-sdk-reference", [&] { cli_sdk_fixtures(work); });
}

template<class F> void negative_copy(const fs::path& work, const std::string& name, F&& mutate,
        Json selected = interpretation(), MatrixStorePluginImportOptions limits = options(),
        const fs::path& original = {}) {
    run(name, [&] {
        const auto input = copy_store(original.empty() ? work / "counts.zarr" : original,
                                      work / ("invalid-" + name + ".zarr"));
        mutate(input);
        const auto output = work / ("invalid-" + name + ".qmx");
        refuses([&] { import(input, work / "positions.tsv", output, selected, limits); });
        check(!fs::exists(output), "Failed import published a QMX: " + name);
        no_stage(work);
    });
}
void negative(const fs::path& work) {
    const auto change = [](const fs::path& path, const char* key, const Json& value) {
        auto metadata = json(path); metadata[key] = value; write_json(path, metadata);
    };
    negative_copy(work, "root-version", [&](const auto& p) { change(p / ".zattrs", "encoding-version", "9.0.0"); });
    negative_copy(work, "root-zarr3", [&](const auto& p) { change(p / ".zgroup", "zarr_format", 3); });
    negative_copy(work, "matrix-encoding", [&](const auto& p) { change(p / "X/.zattrs", "encoding-type", "unknown"); });
    negative_copy(work, "dataframe-index-mismatch", [&](const auto& p) { change(p / "obs/.zattrs", "_index", "other"); });
    negative_copy(work, "dataframe-missing-column-order", [&](const auto& p) {
        auto value = json(p / "var/.zattrs"); value.erase("column-order"); write_json(p / "var/.zattrs", value);
    });
    negative_copy(work, "dtype-complex", [&](const auto& p) { change(p / "X/.zarray", "dtype", "<c16"); });
    negative_copy(work, "dtype-boolean", [&](const auto& p) { change(p / "X/.zarray", "dtype", "|b1"); });
    negative_copy(work, "dtype-no-multibyte-order", [&](const auto& p) { change(p / "X/.zarray", "dtype", "|u8"); });
    negative_copy(work, "shape-axis-mismatch", [&](const auto& p) { change(p / "X/.zarray", "shape", Json::array({4, 4})); });
    negative_copy(work, "chunk-rank", [&](const auto& p) { change(p / "X/.zarray", "chunks", Json::array({1})); });
    negative_copy(work, "chunk-zero", [&](const auto& p) { change(p / "X/.zarray", "chunks", Json::array({0, 1})); });
    negative_copy(work, "decoded-chunk-limit", [&](const auto& p) { change(p / "X/.zarray", "chunks", Json::array({1, 131073})); });
    negative_copy(work, "unreviewed-codec", [&](const auto& p) { change(p / "X/.zarray", "compressor", Json{{"id", "unreviewed"}}); });
    negative_copy(work, "unreviewed-filter", [&](const auto& p) { change(p / "X/.zarray", "filters", Json::array({Json{{"id", "delta"}}})); });
    negative_copy(work, "invalid-order", [&](const auto& p) { change(p / "X/.zarray", "order", "A"); });
    negative_copy(work, "invalid-dimension-separator", [&](const auto& p) { change(p / "X/.zarray", "dimension_separator", "\\"); });
    negative_copy(work, "raw-chunk-truncated", [&](const auto& p) {
        auto value = bytes(p / "X/0.0"); value.pop_back(); write(p / "X/0.0", value);
    });
    negative_copy(work, "raw-chunk-trailing", [&](const auto& p) { write(p / "X/0.0", bytes(p / "X/0.0") + "x"); });
    negative_copy(work, "zlib-chunk-truncated", [&](const auto& p) {
        auto value = bytes(p / "X/0.0"); value.pop_back(); write(p / "X/0.0", value);
    }, interpretation(), options(), work / "layout-8.zarr");
    negative_copy(work, "gzip-chunk-trailing", [&](const auto& p) { write(p / "X/0.0", bytes(p / "X/0.0") + "x"); },
                  interpretation(), options(), work / "layout-16.zarr");
    negative_copy(work, "decoded-compressed-size-mismatch", [&](const auto& p) {
        write(p / "X/0.0", compressed(std::string(128, '\0'), Codec::Zlib));
    }, interpretation(), options(), work / "layout-8.zarr");
    negative_copy(work, "encoded-chunk-limit", [&](const auto& p) { write(p / "X/0.0", std::string(mib + 65536 + 1, 'x')); });
    negative_copy(work, "metadata-limit", [&](const auto& p) { write(p / "X/.zarray", bytes(p / "X/.zarray") + std::string(mib, ' ')); });
    negative_copy(work, "integral-floating-fill", [&](const auto& p) { change(p / "X/.zarray", "fill_value", 0.0); });
    negative_copy(work, "integral-negative-fill", [&](const auto& p) { change(p / "X/.zarray", "fill_value", -1); });
    negative_copy(work, "null-missing-chunk", [&](const auto& p) {
        change(p / "X/.zarray", "fill_value", nullptr); check(fs::remove(p / "X/0.0"), "Remove owned numeric chunk");
    });
    negative_copy(work, "missing-object-id-chunk", [&](const auto& p) { check(fs::remove(p / "obs/ids/0"), "Remove owned ID chunk"); });
    negative_copy(work, "vlen-count-mismatch", [&](const auto& p) {
        auto value = bytes(p / "obs/ids/0"); value[0] = 1; write(p / "obs/ids/0", value);
    });
    negative_copy(work, "vlen-length-overflow", [&](const auto& p) {
        auto value = bytes(p / "obs/ids/0"); for (std::size_t i = 4; i < 8; ++i) value[i] = char(255);
        write(p / "obs/ids/0", value);
    });
    negative_copy(work, "vlen-truncated", [&](const auto& p) {
        auto value = bytes(p / "obs/ids/0"); value.pop_back(); write(p / "obs/ids/0", value);
    });
    negative_copy(work, "vlen-trailing", [&](const auto& p) { write(p / "obs/ids/0", bytes(p / "obs/ids/0") + "x"); });
    negative_copy(work, "arbitrary-object-filter", [&](const auto& p) { change(p / "obs/ids/.zarray", "filters", Json::array({Json{{"id", "pickle"}}})); });
    negative_copy(work, "empty-id", [&](const auto& p) {
        strings(p / "obs/ids", {"", observation_ids[1], observation_ids[2], observation_ids[3]});
    });
    negative_copy(work, "duplicate-id", [&](const auto& p) {
        strings(p / "obs/ids", {observation_ids[0], observation_ids[0], observation_ids[2], observation_ids[3]});
    });
    negative_copy(work, "invalid-utf8-id", [&](const auto& p) {
        strings(p / "obs/ids", {std::string("\xc0\xaf", 2), observation_ids[1], observation_ids[2], observation_ids[3]});
    });
    negative_copy(work, "id-native-length", [&](const auto& p) {
        strings(p / "obs/ids", {std::string(257, 'x'), observation_ids[1], observation_ids[2], observation_ids[3]});
    });
    for (const auto& pair : std::vector<std::pair<std::string, Json>>{
             {"missing-explicit-domain", Json()}, {"unknown-value-domain", "raw-counts"}}) {
        auto selected = interpretation(pair.first == "unknown-value-domain");
        if (pair.first == "missing-explicit-domain") selected.erase("domain");
        else selected["value_domain"] = pair.second;
        negative_copy(work, pair.first, [](const auto&) {}, selected);
    }
    auto choice = interpretation(); choice["feature_ids"] = "var/../obs/ids";
    negative_copy(work, "option-path-escape", [](const auto&) {}, choice);
    auto limits = options(); limits.primary_key = "../.zattrs";
    negative_copy(work, "primary-path-escape", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.primary_key = "X/../.zattrs";
    negative_copy(work, "primary-dot-alias", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.primary_key = "missing";
    negative_copy(work, "missing-primary", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.inventory.max_files = 1;
    negative_copy(work, "inventory-file-limit", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.inventory.inventory_bytes = 1;
    negative_copy(work, "inventory-work-limit", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.inventory.max_depth = 1;
    negative_copy(work, "inventory-depth-limit", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.content.file_bytes = 8;
    negative_copy(work, "store-member-file-limit", [](const auto&) {}, interpretation(), limits);
    limits = options();
    const auto complete_size = DirectoryStoreSnapshot((work / "counts.zarr").u8string()).content_fingerprint();
    std::uint64_t largest_member = 0;
    for (const auto& entry : fs::recursive_directory_iterator(work / "counts.zarr"))
        if (entry.is_regular_file()) largest_member = std::max(largest_member, std::uint64_t(entry.file_size()));
    check(largest_member && complete_size.bytes > largest_member + 1, "Authored cumulative-limit precondition");
    limits.content.file_bytes = largest_member; limits.content.total_bytes = complete_size.bytes - 1;
    negative_copy(work, "store-total-byte-limit", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.matrix.plugin_work_bytes = 1;
    negative_copy(work, "provider-work-limit", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.matrix.limits.work_bytes = 8 * mib;
    negative_copy(work, "composed-host-work-limit", [](const auto&) {}, interpretation(), limits);
    limits = options(); limits.matrix.limits.max_entries = 11;
    negative_copy(work, "logical-cell-limit", [](const auto&) {}, interpretation(), limits);
    for (const auto encoding : {Encoding::Csr, Encoding::Csc}) {
        const auto base = work / (std::string(encoding == Encoding::Csr ? "csr-" : "csc-") + "0.zarr");
        const auto prefix = std::string(encoding == Encoding::Csr ? "csr-" : "csc-");
        negative_copy(work, prefix + "duplicate-coordinate", [&](const auto& p) {
            std::vector<std::uint64_t> indices;
            for (std::size_t major = 0; major < (encoding == Encoding::Csr ? 4u : 3u); ++major)
                for (std::size_t minor = 0; minor < (encoding == Encoding::Csr ? 3u : 4u); ++minor) indices.push_back(minor);
            indices[1] = indices[0]; numeric(p / "X/indices", indices, {12});
        }, interpretation(), options(), base);
        negative_copy(work, prefix + "index-out-of-range", [&](const auto& p) {
            std::vector<std::uint64_t> indices(12, 0); indices[0] = 99; numeric(p / "X/indices", indices, {12});
        }, interpretation(), options(), base);
        negative_copy(work, prefix + "nonintegral-indices", [&](const auto& p) {
            numeric(p / "X/indices", std::vector<double>(12, 0.0), {12});
        }, interpretation(), options(), base);
        negative_copy(work, prefix + "invalid-indptr", [&](const auto& p) {
            const auto n = encoding == Encoding::Csr ? 5u : 4u;
            std::vector<std::uint64_t> pointers(n, 0); pointers.back() = 13;
            numeric(p / "X/indptr", pointers, {n});
        }, interpretation(), options(), base);
    }
    negative_copy(work, "negative-count", [&](const auto& p) { numeric(p / "X", std::vector<std::int64_t>(12, -1), {4, 3}); });
    negative_copy(work, "fractional-count", [&](const auto& p) { numeric(p / "X", std::vector<double>(12, 1.25), {4, 3}); });
    negative_copy(work, "overflowing-floating-count", [&](const auto& p) { numeric(p / "X", std::vector<double>(12, std::ldexp(1., 64)), {4, 3}); });
    negative_copy(work, "nonfinite-continuous", [&](const auto& p) { numeric(p / "X", std::vector<double>(12, std::numeric_limits<double>::infinity()), {4, 3}); }, interpretation(true));
    negative_copy(work, "inexact-integral-continuous", [](const auto&) {}, interpretation(true));
    run("null-filled-populated-array", [&] {
        const auto input = copy_store(work / "counts.zarr", work / "null-present.zarr");
        change(input / "X/.zarray", "fill_value", nullptr);
        check_fixture(input, work / "positions.tsv", work / "null-present.qmx",
                      std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    });
    run("missing-chunk-has-declared-measured-zero", [&] {
        const auto input = copy_store(work / "counts.zarr", work / "missing-zero.zarr");
        check(fs::remove(input / "X/0.0"), "Remove owned zero-fill chunk");
        auto expected = std::vector<std::uint64_t>(count_rows.begin(), count_rows.end());
        for (const auto index : {0u, 1u, 3u, 4u}) expected[index] = 0;
        check_fixture(input, work / "positions.tsv", work / "missing-zero.qmx", expected);
    });
    run("output-cannot-enter-input-store", [&] {
        const auto input = work / "counts.zarr";
        const auto before = DirectoryStoreSnapshot(input.u8string()).content_fingerprint();
        refuses([&] { import(input, work / "positions.tsv", input / "forbidden.qmx", interpretation()); });
        check(!fs::exists(input / "forbidden.qmx") &&
              DirectoryStoreSnapshot(input.u8string()).content_fingerprint().content_sha256 == before.content_sha256,
              "Rejected output changed its source directory");
    });
    run("existing-output-protected", [&] {
        const auto output = work / "existing.qmx"; write(output, "User-owned output sentinel\n");
        const auto before = sha(output);
        refuses([&] { import(work / "counts.zarr", work / "positions.tsv", output, interpretation()); });
        check(sha(output) == before, "Import overwrote an existing user output"); no_stage(work);
    });
    run("cancel-before-open", [&] {
        const std::atomic<bool> cancelled{true}; const auto output = work / "cancelled.qmx";
        refuses<WorkflowCancelled>([&] { import(work / "counts.zarr", work / "positions.tsv", output, interpretation(), options(), &cancelled); });
        check(!fs::exists(output), "Cancelled import published output"); no_stage(work);
    });
    run("duplicate-decoded-option-keys", [&] {
        auto selected = interpretation().dump();
        selected.insert(1, "\"dom\\u0061in\":\"counts\",");
        const auto output = work / "duplicate-options.qmx";
        const auto explanation = refuses([&] {
            import_expression_matrix_store_plugin(provider, (work / "counts.zarr").u8string(),
                (work / "positions.tsv").u8string(), output.u8string(), options(), selected);
        });
        check(!fs::exists(output) && explanation.find("duplicate") != std::string::npos,
              "Duplicate semantic key reached lossy option canonicalization");
    });
    for (const auto& value : std::vector<std::pair<std::string, std::string>>{
             {"missing-position", "observation_id\tx_px\ty_px\n0001\t1\t2\n"},
             {"duplicate-position", position_text() + "0001\t3\t4\n"},
             {"extra-position", position_text() + "unknown\t3\t4\n"}}) {
        run(value.first, [&] {
            const auto positions = work / (value.first + ".tsv"), output = work / (value.first + ".qmx");
            write(positions, value.second);
            refuses([&] { import(work / "counts.zarr", positions, output, interpretation()); });
            check(!fs::exists(output), "Invalid complete-ID join published output"); no_stage(work);
        });
    }
}

struct Error {
    std::array<char, 8192> buffer{};
    qpx_reader_text_v1 text{buffer.data(), buffer.size(), 0};
};
int32_t QPX_READER_CALL always_cancelled(void*) { return 1; }
void direct_abi(const fs::path& work) {
    const qpx_matrix_format_api_v3* selected_api = nullptr;
    for (const auto& item : detail::format_plugin_catalog().providers)
        if (item->info.provider_id == provider) selected_api = &item->matrix_store;
    check(selected_api && selected_api->struct_size == sizeof(*selected_api) && selected_api->abi_version == 3,
          "Actual native provider did not negotiate its complete ABI3 table");
    const auto& api = *selected_api;
    for (const bool real : {false, true}) {
        const auto path = (work / (real ? "continuous.zarr" : "counts.zarr")).u8string();
        const std::string primary = ".zattrs", selection = interpretation(real).dump();
        qpx_matrix_open_v3 request{sizeof(request), 0, {path.data(), path.size()},
            {primary.data(), primary.size()}, {selection.data(), selection.size()},
            16 * mib, UINT64_C(1024) * mib, UINT64_C(16) * 1024 * mib, {}};
        const auto original = request;
        void* handle = nullptr; Error error;
        struct Close {
            const qpx_matrix_format_api_v3& api; void*& value;
            ~Close() { if (value) api.destroy(value); }
        } close{api, handle};
        check(api.open(&request, &handle, &error.text) == QPX_READER_OK && handle &&
              std::memcmp(&request, &original, sizeof(request)) == 0,
              "Actual ABI3 open mutated its request or returned no handle");
        std::array<char, 130> label; label.fill('!');
        qpx_matrix_metadata_v2 metadata{}; metadata.struct_size = sizeof(metadata);
        metadata.value_domain = {label.data() + 1, 0, 0};
        check(api.metadata(handle, &metadata, {}, &error.text) == QPX_READER_BUFFER_TOO_SMALL &&
              metadata.value_domain.size && metadata.value_domain.size <= 128 &&
              std::all_of(label.begin(), label.end(), [](char value) { return value == '!'; }),
              "Actual metadata probe wrote text or returned invalid required length");
        metadata.value_domain.capacity = 128;
        check(api.metadata(handle, &metadata, {}, &error.text) == QPX_READER_OK &&
              metadata.features == 3 && metadata.observations == 4 && !metadata.reserved[0] && !metadata.reserved[1] &&
              metadata.value_domain.data == label.data() + 1 && metadata.value_domain.capacity == 128 &&
              label.front() == '!' && label.back() == '!', "Actual ABI3 metadata fields or output canaries changed");
        std::array<std::uint64_t, 9> counts; counts.fill(UINT64_C(0xa1b2c3d4e5f67890));
        std::array<double, 9> values; values.fill(-789.25);
        qpx_matrix_values_request_v1 page{sizeof(page), 0, 1, 7}; const auto original_page = page;
        qpx_matrix_values_buffer_v2 out{sizeof(out), real ? QPX_FORMAT_CONTINUOUS_FLOAT64 : QPX_FORMAT_COUNTS_UINT64,
            real ? nullptr : counts.data() + 1, real ? values.data() + 1 : nullptr, 7, 0, 0};
        check(api.values(handle, &page, &out, {}, &error.text) == QPX_READER_OK && out.written == 7 &&
              out.capacity == 7 && !out.reserved && std::memcmp(&page, &original_page, sizeof(page)) == 0,
              "Actual typed page mutated inputs or returned wrong size");
        for (std::size_t index = 0; index < 7; ++index)
            check(real ? bits(values[index + 1]) == bits(real_rows[index + 1]) : counts[index + 1] == count_rows[index + 1],
                  "Actual partial typed page changed source orientation/precision");
        check(counts.front() == UINT64_C(0xa1b2c3d4e5f67890) && counts.back() == UINT64_C(0xa1b2c3d4e5f67890) &&
              values.front() == -789.25 && values.back() == -789.25, "Actual typed page overwrote canaries");
        const auto saved_counts = counts; const auto saved_values = values;
        out.capacity = 1; out.written = 0;
        check(api.values(handle, &page, &out, {}, &error.text) == QPX_READER_BUFFER_TOO_SMALL && out.written == 7 &&
              counts == saved_counts && values == saved_values, "Actual too-small page wrote partial output");
        out.capacity = 7;
        check(api.values(handle, &page, &out, {always_cancelled, nullptr}, &error.text) == QPX_READER_CANCELLED &&
              counts == saved_counts && values == saved_values, "Cancelled actual provider wrote output");
    }
}

void inspect(const fs::path& work) {
    assert_values(*open_expression_matrix((work / "counts.qmx").u8string()),
                  std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    assert_values(*open_expression_matrix((work / "continuous.qmx").u8string()),
                  std::vector<double>(real_rows.begin(), real_rows.end()));
    const auto ids = json(work / "association-ids.json");
    const auto project = Project::open((work / "owned.qpx").u8string());
    assert_values(*reopen_expression_matrix(*project, ids.at("image").get<std::uint64_t>(), ids.at("counts").get<std::string>()),
                  std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    assert_values(*reopen_expression_matrix(*project, ids.at("image").get<std::uint64_t>(), ids.at("continuous").get<std::string>()),
                  std::vector<double>(real_rows.begin(), real_rows.end()));
    check(!fs::exists(work / "offline-input.zarr") && !fs::exists(work / "offline-positions.tsv"),
          "Offline project test requires absent original inputs");
    const auto offline = Project::open((work / "offline-owned.qpx").u8string());
    const auto ids_offline = json(work / "offline-association.json");
    const auto retained = reopen_expression_matrix(*offline, ids_offline.at("image").get<std::uint64_t>(),
                                                    ids_offline.at("association").get<std::string>());
    check(retained->file_sha256() == ids_offline.at("file_sha256"), "Offline managed QMX identity changed");
    assert_values(*retained, std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
}
void offline_originals(const fs::path& work) {
    const auto input = copy_store(work / "counts.zarr", work / "offline-input.zarr");
    const auto positions = work / "offline-positions.tsv", output = work / "offline-input.qmx";
    write(positions, position_text());
    const auto indexed = import(input, positions, output, interpretation());
    const auto source = open_expression_matrix(indexed.path, indexed.file_sha256);
    const auto original_receipt = external_expression_matrix_store_receipt(*source);
    Project project;
    const auto image = project.add_document(std::make_shared<Document>(
        Image(64,64,std::vector<std::uint8_t>(64*64*3,30))), "Offline AnnData Zarr");
    const auto association = keep_expression_matrix(project, image, source, true);
    project.save_new((work / "offline-owned.qpx").u8string());
    write_json(work / "offline-association.json", {{"image", image}, {"association", association},
                                                  {"file_sha256", source->file_sha256()}});
    check(input.parent_path() == work && positions.parent_path() == work, "Offline paths escaped owned test directory");
    fs::rename(input, work / "offline-input-retained.zarr");
    fs::rename(positions, work / "offline-positions-retained.tsv");
    assert_values(*source, std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    const auto reopened = open_expression_matrix(indexed.path, indexed.file_sha256);
    const auto restored_receipt = external_expression_matrix_store_receipt(*reopened);
    assert_values(*reopened, std::vector<std::uint64_t>(count_rows.begin(), count_rows.end()));
    check(original_receipt && restored_receipt &&
          original_receipt->content_sha256 == restored_receipt->content_sha256 &&
          original_receipt->observed_sha256 == restored_receipt->observed_sha256,
          "Provider-independent QMX changed exact original-store identities");
    // Leave the authored copies under their retained names for the separate
    // provider-free and loader-disabled processes.
}

fs::path control_store(const fs::path& work, const std::string& mode) {
    const auto input = work / (mode + ".store");
    check(!fs::exists(input) && fs::create_directory(input), "Controlled store already exists");
    write(input / ".zattrs", mode + "\n");
    write(input / "payload.bin", "Independent native callback control\n");
    return input;
}
void control(const fs::path& manifest, const fs::path& work) {
    fresh(work); report_path = work / "native-tests.json";
    configure_format_plugins(manifest.u8string());
    const auto positions = work / "positions.tsv";
    write(positions, "observation_id\tx_px\ty_px\ncell-c\t3\t4\ncell-a\t1\t2\ncell-b\t2\t3\n");
    for (const auto& mode : std::vector<std::string>{"valid-counts", "valid-continuous", "open-request", "open-error-handle",
         "metadata-type", "metadata-zero", "metadata-huge", "metadata-reserved", "metadata-size", "metadata-capacity",
         "metadata-pointer", "metadata-length", "metadata-count-label", "metadata-continuous-label", "metadata-empty-label",
         "metadata-utf8-label", "metadata-long-label", "metadata-io", "axis-short", "axis-long", "axis-capacity", "axis-pointer",
         "axis-index", "axis-offset", "axis-duplicate", "axis-observation-name", "values-short", "values-long",
         "values-capacity", "values-type", "values-size", "values-reserved", "values-count-pointer", "values-continuous-pointer",
         "values-request", "values-io", "values-nan", "values-inf", "values-source-add"}) {
        run("abi3-" + mode, [&] {
            const auto input = control_store(work, mode), output = work / (mode + ".qmx");
            const auto call = [&] {
                return import_expression_matrix_store_plugin(control_provider, input.u8string(), positions.u8string(),
                                                             output.u8string(), options(), "{}");
            };
            if (mode.rfind("valid-", 0) == 0) {
                const auto result = call(); const auto source = open_expression_matrix(result.path, result.file_sha256);
                check(source->metadata().features == 2 && source->metadata().observations == 3 &&
                      source->metadata().input_profile == "external-dense-store/1", "Valid controlled metadata changed");
                const auto block = source->read({{0, 1}, std::nullopt});
                if (mode == "valid-counts")
                    check(block.counts == std::vector<std::uint64_t>{UINT64_MAX, 0, 11, UINT64_C(9007199254740993), 7, 13},
                          "ABI3 controlled exact count transport changed");
                else {
                    const std::array<double, 6> expected{-0.0, 2.5, 11, -1.25, 0.0, std::numeric_limits<double>::denorm_min()};
                    check(block.continuous.size() == expected.size(), "Controlled float page length");
                    for (std::size_t index = 0; index < expected.size(); ++index)
                        check(bits(block.continuous[index]) == bits(expected[index]), "Controlled float bits changed");
                }
            } else {
                refuses([&] { call(); }); check(!fs::exists(output), "Bad ABI3 callback published QMX");
            }
            const auto events = bytes(fs::u8path(input.u8string() + ".events"));
            check(events.rfind("open\n", 0) == 0 && events.size() >= 13 &&
                  events.substr(events.size() - 8) == "destroy\n" &&
                  events.find("destroy\n") == events.size() - 8, "ABI3 handle did not close exactly once");
            if (mode == "values-source-add")
                check(fs::exists(input / "added-during-callback.bin") && events.find("member-add-succeeded") != std::string::npos,
                      "Store-change test did not actually add a source member");
            no_stage(work);
        });
    }
    run("abi3-independent-file-and-provider-work-budget", [&] {
        const auto input = control_store(work, "large-file"); write(input / ".zattrs", "valid-counts\n");
        write(input / "payload.bin", std::string(256 * 1024, 'x'));
        auto limits = options(); limits.content.file_bytes = 512 * 1024; limits.content.total_bytes = mib;
        auto session = detail::MatrixPluginSession::open_store(control_provider, input.u8string(), ".zattrs", "{}", 65536,
            limits.inventory, limits.content, 16 * mib);
        check(session->values(0, 1) == std::vector<std::uint64_t>{UINT64_MAX}, "Store file allowance was coupled to provider work");
        session.reset(); limits.content.file_bytes = 256 * 1024 - 1;
        refuses([&] { detail::MatrixPluginSession::open_store(control_provider, input.u8string(), ".zattrs", "{}", 65536,
            limits.inventory, limits.content, 16 * mib); });
    });
    run("abi3-between-callback-membership-currentness", [&] {
        const auto input = control_store(work, "between-calls"); write(input / ".zattrs", "valid-counts\n");
        auto limits = options();
        auto session = detail::MatrixPluginSession::open_store(control_provider, input.u8string(), ".zattrs", "{}", 16 * mib,
            limits.inventory, limits.content, 64 * mib);
        const auto initial = session->store_source();
        check(initial && initial->content_sha256.size() == 64 && initial->observed_sha256.size() == 64,
              "Store session omitted distinct source identities");
        write(input / "actual-new-member.bin", "Actual source membership change\n");
        check(fs::exists(input / "actual-new-member.bin"), "Membership edit did not occur");
        refuses<WorkflowInputChanged>([&] { session->verify_source(); });
        std::vector<std::uint64_t> sentinel{12345, 67890};
        refuses<WorkflowInputChanged>([&] { sentinel = session->values(0, 2); });
        check(sentinel == std::vector<std::uint64_t>{12345, 67890}, "Stale callback returned caller-visible values");
    });
    run("abi3-active-cancellation", [&] {
        const auto input = control_store(work, "wait-values"), output = work / "cancelled-active.qmx";
        std::atomic<bool> cancelled{false};
        auto task = std::async(std::launch::async, [&] {
            return refuses<WorkflowCancelled>([&] {
                import_expression_matrix_store_plugin(control_provider, input.u8string(), positions.u8string(), output.u8string(),
                                                      options(), "{}", &cancelled);
            });
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        bool entered = false;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto log = fs::u8path(input.u8string() + ".events");
            if (fs::exists(log) && bytes(log).find("entered\n") != std::string::npos) { entered = true; break; }
            if (task.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        cancelled = true;
        check(entered, "Active cancel did not reach a live provider callback");
        check(!task.get().empty() && !fs::exists(output), "Active cancellation published output or lost failure");
        no_stage(work);
    });
    report["passed"] = true; write_json(report_path, report);
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "validate") {
            const auto manifest = fs::absolute(fs::u8path(argv[2])), work = fs::absolute(fs::u8path(argv[3]));
            fresh(work); report_path = work / "native-tests.json";
            configure_format_plugins(manifest.u8string());
            positive(work); run("actual-ABI3-buffer-and-cancel-contract", [&] { direct_abi(work); });
            negative(work); run("QMX-independent-of-original-store-and-positions", [&] { offline_originals(work); });
            report["passed"] = true; write_json(report_path, report);
            std::cout << "AnnData Zarr native scientific/store/persistence checks pass: " << report["checks"].size() << '\n';
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "inspect") {
            inspect(fs::absolute(fs::u8path(argv[2])));
            std::cout << "Provider-free schema6 counts/continuous/project metadata checks pass\n"; return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "unavailable") {
            const auto work = fs::absolute(fs::u8path(argv[2])), output = work / "unavailable.qmx";
            const auto explanation = refuses<std::invalid_argument>([&] {
                import(work / "counts.zarr", work / "positions.tsv", output, interpretation());
            });
            check(explanation.find(provider) != std::string::npos && !fs::exists(output),
                  "Missing-provider check failed before reaching selected provider availability");
            std::cout << explanation << '\n'; return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "control") {
            control(fs::absolute(fs::u8path(argv[2])), fs::absolute(fs::u8path(argv[3])));
            std::cout << "Matrix ABI3 trust/currentness/cancellation controls pass\n"; return 0;
        }
        throw std::invalid_argument("Usage: validate ABS_MANIFEST NEW_WORK | inspect WORK | unavailable WORK | control ABS_MANIFEST NEW_WORK");
    } catch (const std::exception& error) {
        report["error"] = error.what();
        if (!report_path.empty()) write_json(report_path, report);
        std::cerr << error.what() << '\n'; return 1;
    }
}
