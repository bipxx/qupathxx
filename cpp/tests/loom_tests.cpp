// SPDX-License-Identifier: AGPL-3.0-only
// Independent, repository-authored Loom files and literal scientific anchors.
// HDF5 only serializes fixtures; the optional provider and shared C++ services
// own interpretation, identity, joins, publication and persistence.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "format_plugin_runtime.hpp"
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
#include <hdf5.h>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <numeric>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;
using namespace qpx;
using Json = nlohmann::json;
namespace {
constexpr std::size_t mib = 1024u * 1024;
constexpr const char *provider = "org.qupathxx.loom";
constexpr const char *control_provider = "org.qupathxx.test.typed-matrix";
constexpr std::size_t feature_count = 3, observation_count = 4;
const std::array<std::string, feature_count> feature_ids{"0002", "2",
                                                         u8"g-\u03b2"};
const std::array<std::string, feature_count> feature_names{
    "First gene", u8"\u03b2 expression", ""};
const std::array<std::string, observation_count> observation_ids{
    "0001", "1", "18446744073709551615", u8"spot-\u03bc"};
// Literal file layout is features x observations, independently of the
// provider.
const std::array<std::uint64_t, 12> exact_counts{UINT64_MAX,
                                                 0,
                                                 7,
                                                 11,
                                                 UINT64_C(9007199254740992),
                                                 UINT64_C(9007199254740993),
                                                 4,
                                                 5,
                                                 0,
                                                 13,
                                                 0,
                                                 17};
const std::array<double, 12> exact_continuous{
    -0.0,
    -1.25,
    std::numeric_limits<double>::denorm_min(),
    3.5,
    2.5,
    0.0,
    11.0,
    -2.0,
    1e-300,
    -std::numeric_limits<double>::min(),
    std::numeric_limits<double>::min(),
    99.125};

void check(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class E = std::exception, class F> std::string refuses(F &&function) {
  try {
    function();
  } catch (const E &error) {
    check(!std::string(error.what()).empty(),
          "Native refusal has no explanation");
    return error.what();
  }
  throw std::runtime_error("Missing native Loom/typed-matrix refusal");
}
std::string bytes(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  check(bool(file), "Read owned test file: " + path.u8string());
  return {std::istreambuf_iterator<char>(file), {}};
}
void write(const fs::path &path, const std::string &value) {
  std::ofstream file(path, std::ios::binary);
  file.write(value.data(), std::streamsize(value.size()));
  file.close();
  check(bool(file), "Write owned test file: " + path.u8string());
}
std::string sha(const fs::path &path) {
  return fingerprint_file(path.u8string())->sha256;
}
std::uint64_t bits(double value) {
  std::uint64_t result = 0;
  std::memcpy(&result, &value, sizeof(result));
  return result;
}
template <class Container>
void same_reals(const std::vector<double> &actual, const Container &expected,
                const std::string &context) {
  check(actual.size() == expected.size(), context + ": value count");
  for (std::size_t i = 0; i < actual.size(); ++i)
    check(bits(actual[i]) == bits(expected[i]),
          context + ": exact binary64 value at " + std::to_string(i));
}
void no_stage(const fs::path &directory) {
  for (const auto &entry : fs::directory_iterator(directory))
    check(entry.path().filename().u8string().rfind(".qpx-expression-", 0) != 0,
          "Failed operation leaked a private QMX stage");
}
void fresh(const fs::path &directory) {
  check(directory.is_absolute() && !fs::exists(directory),
        "Use a fresh absolute test directory");
  check(fs::create_directories(directory),
        "Create owned native test directory");
}

void hstatus(herr_t result, const char *context) {
  check(result >= 0, std::string("Native fixture HDF5: ") + context);
}
struct H5 {
  hid_t value = -1;
  explicit H5(hid_t id) : value(id) {
    check(id >= 0, "Create/open native HDF5 fixture object");
  }
  H5(const H5 &) = delete;
  H5 &operator=(const H5 &) = delete;
  operator hid_t() const { return value; }
  ~H5() {
    switch (H5Iget_type(value)) {
    case H5I_FILE:
      H5Fclose(value);
      break;
    case H5I_GROUP:
      H5Gclose(value);
      break;
    case H5I_DATASET:
      H5Dclose(value);
      break;
    case H5I_DATATYPE:
      H5Tclose(value);
      break;
    case H5I_DATASPACE:
      H5Sclose(value);
      break;
    case H5I_GENPROP_LST:
      H5Pclose(value);
      break;
    default:
      break;
    }
  }
};
H5 utf8_type() {
  H5 type(H5Tcopy(H5T_C_S1));
  hstatus(H5Tset_size(type, H5T_VARIABLE), "vlen string type");
  hstatus(H5Tset_cset(type, H5T_CSET_UTF8), "UTF8 string type");
  return H5(H5Tcopy(type));
}
void group(hid_t file, const char *path) {
  H5 created(H5Gcreate2(file, path, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
}
void strings(hid_t file, const std::string &path,
             const std::vector<std::string> &values, bool scalar = false,
             bool compressed = false, hsize_t chunk_extent = 2,
             bool unlimited = false) {
  H5 type = utf8_type();
  const hsize_t size = values.size();
  const hsize_t maximum = H5S_UNLIMITED;
  H5 space(scalar ? H5Screate(H5S_SCALAR)
                  : H5Screate_simple(1, &size, unlimited ? &maximum : nullptr));
  H5 creation(H5Pcreate(H5P_DATASET_CREATE));
  if (compressed) {
    const hsize_t chunk = unlimited ? chunk_extent : std::min(chunk_extent, size);
    hstatus(H5Pset_chunk(creation, 1, &chunk), "string chunks");
    hstatus(H5Pset_deflate(creation, 1), "string Deflate");
  }
  H5 data(H5Dcreate2(file, path.c_str(), type, space, H5P_DEFAULT, creation,
                     H5P_DEFAULT));
  std::vector<const char *> text;
  for (const auto &value : values)
    text.push_back(value.c_str());
  hstatus(H5Dwrite(data, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, text.data()),
          "write UTF8 strings");
}
struct Layout {
  bool chunked = false, deflate = false, shuffle = false, fletcher = false;
  bool unlimited = false, unknown_filter = false;
  std::array<hsize_t, 2> chunk{2, 3};
};
void matrix(hid_t file, const std::string &path, hid_t file_type,
            hid_t memory_type, const void *values,
            std::array<hsize_t, 2> dimensions = {3, 4}, Layout layout = {}) {
  const std::array<hsize_t, 2> unlimited{H5S_UNLIMITED, H5S_UNLIMITED};
  H5 space(H5Screate_simple(2, dimensions.data(),
                            layout.unlimited ? unlimited.data() : nullptr));
  H5 creation(H5Pcreate(H5P_DATASET_CREATE));
  if (layout.chunked) {
    hstatus(H5Pset_chunk(creation, 2, layout.chunk.data()), "numeric chunks");
    if (layout.shuffle)
      hstatus(H5Pset_shuffle(creation), "numeric shuffle");
    if (layout.deflate)
      hstatus(H5Pset_deflate(creation, 1), "numeric Deflate");
    if (layout.fletcher)
      hstatus(H5Pset_fletcher32(creation), "numeric Fletcher32");
    if (layout.unknown_filter)
      hstatus(H5Pset_filter(creation, H5Z_filter_t(32017), H5Z_FLAG_OPTIONAL, 0,
                            nullptr),
              "unknown optional filter descriptor");
  }
  H5 data(H5Dcreate2(file, path.c_str(), file_type, space, H5P_DEFAULT,
                     creation, H5P_DEFAULT));
  if (values)
    hstatus(H5Dwrite(data, memory_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, values),
            "write numeric matrix");
}
void replace_matrix(hid_t file, hid_t file_type, hid_t memory_type,
                    const void *values,
                    std::array<hsize_t, 2> dimensions = {3, 4},
                    Layout layout = {}) {
  hstatus(H5Ldelete(file, "/matrix", H5P_DEFAULT), "replace main matrix");
  matrix(file, "/matrix", file_type, memory_type, values, dimensions, layout);
}
void fixture(const fs::path &path, bool continuous = false, Layout layout = {},
             bool compressed_ids = false, bool user_block = false) {
  H5 creation(H5Pcreate(H5P_FILE_CREATE));
  if (user_block)
    hstatus(H5Pset_userblock(creation, 512), "HDF5 user block");
  H5 file(
      H5Fcreate(path.u8string().c_str(), H5F_ACC_EXCL, creation, H5P_DEFAULT));
  for (const char *name : {"/attrs", "/layers", "/row_attrs", "/col_attrs",
                           "/row_graphs", "/col_graphs"})
    group(file, name);
  strings(file, "/attrs/LOOM_SPEC_VERSION", {"3.0.0"}, true);
  strings(file, "/row_attrs/ID", {feature_ids.begin(), feature_ids.end()},
          false, compressed_ids);
  strings(file, "/row_attrs/Name", {feature_names.begin(), feature_names.end()},
          false, compressed_ids);
  strings(file, "/col_attrs/ID",
          {observation_ids.begin(), observation_ids.end()}, false,
          compressed_ids);
  strings(file, "/attrs/ignored-note",
          {"Unselected metadata is not an imported measurement"}, true);
  if (continuous)
    matrix(file, "/matrix", H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE,
           exact_continuous.data(), {3, 4}, layout);
  else
    matrix(file, "/matrix", H5T_STD_U64LE, H5T_NATIVE_UINT64,
           exact_counts.data(), {3, 4}, layout);
  std::array<std::uint64_t, 12> layer{};
  for (std::size_t i = 0; i < layer.size(); ++i)
    layer[i] = 101 + i;
  matrix(file, "/layers/selected", H5T_STD_U64BE, H5T_NATIVE_UINT64,
         layer.data(), {3, 4}, layout);
}
void sql(const fs::path &path, const std::string &statement) {
  sqlite3 *database = nullptr;
  check(sqlite3_open_v2(path.u8string().c_str(), &database,
                        SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
        "Open independently copied QMX corruption fixture");
  const auto rc =
      sqlite3_exec(database, statement.c_str(), nullptr, nullptr, nullptr);
  const auto close = sqlite3_close(database);
  check(rc == SQLITE_OK && close == SQLITE_OK,
        "Write independent QMX corruption fixture");
}

// Independent RFC1950/1951 literal stored block: exactly128 zero bytes. A tiny
// logical32-byte numeric/ID chunk must refuse this size mismatch before H5Dread;
// no large expansion or allocation is needed to prove that exact bound.
std::vector<unsigned char> mismatched_deflate() {
  std::vector<unsigned char> out{0x78, 0x01, 0x01, 0x80, 0x00, 0x7f, 0xff};
  out.resize(7 + 128, 0);
  out.insert(out.end(), {0x00, 0x80, 0x00, 0x01}); // Adler32 of128 zero bytes.
  check(out.size() == 139, "Literal chunk fixture has the wrong encoded size");
  return out;
}
void raw_chunk_payload(hid_t file, const char *path,
                       const std::vector<unsigned char> &encoded) {
  H5 data(H5Dopen2(file, path, H5P_DEFAULT));
  const std::array<hsize_t, 2> origin{0, 0};
  hstatus(H5Dwrite_chunk(data, H5P_DEFAULT, 0, origin.data(), encoded.size(),
                       encoded.data()), "write independent chunk-size fixture");
}

Json interpretation(bool continuous = false, bool names = true) {
  Json value{{"matrix", "/matrix"},
             {"feature_ids", "/row_attrs/ID"},
             {"observation_ids", "/col_attrs/ID"},
             {"domain", continuous ? "continuous" : "counts"},
             {"value_domain", continuous ? "literal-signed" : "raw-counts"}};
  if (names)
    value["feature_names"] = "/row_attrs/Name";
  return value;
}
MatrixPluginImportOptions options() {
  MatrixPluginImportOptions value;
  value.frame.id = u8"loom-frame-\u03c6";
  value.frame.source_id = "independent-authored-positions";
  value.frame.unit = CoordinateUnit::Pixel;
  value.frame.plane = ObjectPlane(-1, 0, 0);
  value.frame.image_level_zero = true;
  value.frame.width = 64;
  value.frame.height = 64;
  value.frame.micrometres_per_pixel = std::array<double, 2>{.25, .5};
  value.plugin_work_bytes = 16 * mib;
  value.limits.work_bytes = 64 * mib;
  value.limits.store_bytes = 64 * mib;
  return value;
}
std::string position_text() {
  std::string text = "observation_id\tx_px\ty_px\n";
  for (const auto index : {2u, 0u, 3u, 1u})
    text += observation_ids[index] + "\t" + std::to_string(1.25 + 3 * index) +
            "\t" + std::to_string(2.5 + 2 * index) + "\n";
  return text;
}
ExpressionMatrixImportResult
import(const fs::path &file, const fs::path &positions, const fs::path &output,
       const Json &selection,
       const MatrixPluginImportOptions &limits = options(),
       const std::atomic<bool> *cancelled = nullptr) {
  return import_expression_matrix_plugin(
      provider, file.u8string(), positions.u8string(), output.u8string(),
      limits, selection.dump(), cancelled);
}
void assert_axes(const ExpressionMatrixSource &source, bool names = true) {
  const auto features = source.feature_page(0, 3);
  check(features.size() == 3, "Loom feature axis size changed");
  for (std::size_t i = 0; i < features.size(); ++i) {
    check(features[i].index == i && features[i].id == feature_ids[i] &&
              features[i].name == (names && !feature_names[i].empty()
                                       ? feature_names[i]
                                       : feature_ids[i]) &&
              features[i].type == "feature",
          "Loom source feature ID/name/order was inferred or normalized");
  }
  check(source.observation_page(0, 4) ==
            std::vector<std::string>(observation_ids.begin(),
                                     observation_ids.end()),
        "Loom observation IDs were normalized, sorted or converted to numbers");
  const auto positions = source.spatial_page(0, 4);
  check(positions.size() == 4, "Loom embedded coordinate count");
  for (std::size_t i = 0; i < positions.size(); ++i)
    check(positions[i].id == observation_ids[i] &&
              positions[i].position.x == 1.25 + 3 * i &&
              positions[i].position.y == 2.5 + 2 * i,
          "Loom coordinate join used source row order instead of exact IDs");
}
ExpressionSparseBlock all(const ExpressionMatrixSource &source) {
  return source.read({{0, 1, 2}, std::nullopt});
}
void assert_shape(const ExpressionSparseBlock &block) {
  check(block.features == std::vector<std::uint64_t>{0, 1, 2} &&
            block.offsets == std::vector<std::uint64_t>{0, 4, 8, 12} &&
            block.observation_indices ==
                std::vector<std::uint64_t>{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
        "Dense Loom orientation/explicit zero entries changed in indexed "
        "storage");
}
void assert_counts(const ExpressionMatrixSource &source) {
  assert_axes(source);
  const auto &metadata = source.metadata();
  check(metadata.features == 3 && metadata.observations == 4 &&
            metadata.entries == 12 &&
            metadata.domain == ExpressionValueDomain::Counts &&
            metadata.value_domain == "raw-counts" &&
            metadata.input_profile == "external-dense-typed/1",
        "Typed count metadata/profile differs");
  const auto block = all(source);
  assert_shape(block);
  check(block.domain == ExpressionValueDomain::Counts &&
            block.continuous.empty() &&
            block.counts == std::vector<std::uint64_t>(exact_counts.begin(),
                                                       exact_counts.end()),
        "Exact UInt64 values/adjacent values above2^53/orientation changed");
  const auto subset = source.read({{1, 0}, std::vector<std::uint64_t>{1, 3}});
  check(subset.counts == std::vector<std::uint64_t>{UINT64_C(9007199254740993),
                                                    5, 0, 11} &&
            subset.offsets == std::vector<std::uint64_t>{0, 2, 4},
        "Typed counts changed in explicit feature/cohort projection");
  const auto empty = source.read({{1, 0}, std::vector<std::uint64_t>{}});
  check(empty.counts.empty() &&
            empty.offsets == std::vector<std::uint64_t>{0, 0, 0},
        "Explicit empty cohort was replaced by all observations");
}
void assert_continuous(const ExpressionMatrixSource &source) {
  assert_axes(source);
  const auto &metadata = source.metadata();
  check(metadata.domain == ExpressionValueDomain::Continuous &&
            metadata.value_domain == "literal-signed" &&
            metadata.input_profile == "external-dense-typed/1",
        "Continuous native domain/value label changed");
  const auto block = all(source);
  assert_shape(block);
  check(block.counts.empty() &&
            block.domain == ExpressionValueDomain::Continuous,
        "Continuous values used a count transport/storage path");
  same_reals(block.continuous, exact_continuous,
             "Loom continuous/signed-zero/subnormal storage");
  same_reals(source.read({{1, 0}, std::vector<std::uint64_t>{1, 3}}).continuous,
             std::array<double, 4>{0.0, -2.0, -1.25, 3.5},
             "Loom continuous cohort");
}
void receipt(const ExpressionMatrixSource &source, const fs::path &input,
             const fs::path &positions, const Json &selection) {
  const auto value = external_expression_matrix_receipt(source);
  check(value && value->reader.provider.provider_id == provider &&
            value->reader.provider.abi == 2 &&
            value->reader.provider.contract == "loom3-dense-typed/1" &&
            value->reader.source_sha256 == sha(input) &&
            value->positions_sha256 == sha(positions) &&
            value->position_columns ==
                std::array<std::string, 3>{"observation_id", "x_px", "y_px"} &&
            Json::parse(value->reader.options) == selection,
        "Native Loom receipt lost exact implementation/input/options/positions "
        "identity");
  check(source.metadata().spatial_frame &&
            source.metadata().spatial_frame->sha256() ==
                options().frame.sha256(),
        "Explicit coordinate frame/plane/calibration did not round trip");
}

struct Error {
  std::array<char, 8192> bytes{};
  qpx_reader_text_v1 text{bytes.data(), bytes.size(), 0};
  std::string message() const {
    return {bytes.data(),
            std::size_t(std::min<std::uint64_t>(bytes.size(), text.size))};
  }
};
void status(qpx_reader_status_v1 actual, qpx_reader_status_v1 expected,
            const Error &error, const std::string &context) {
  check(actual == expected, context + ": status" + std::to_string(actual) +
                                " expected" + std::to_string(expected) + "; " +
                                error.message());
}
struct Module {
  HMODULE value = nullptr;
  explicit Module(const fs::path &path)
      : value(LoadLibraryExW(path.c_str(), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                 LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
    check(value != nullptr, "Load exact Loom C ABI module");
  }
  ~Module() {
    if (value)
      FreeLibrary(value);
  }
};
struct AbiHandle {
  const qpx_matrix_format_api_v2 &api;
  void *value = nullptr;
  explicit AbiHandle(const qpx_matrix_format_api_v2 &table) : api(table) {}
  ~AbiHandle() {
    if (value)
      api.destroy(value);
  }
};
struct Cancellation {
  DWORD thread = GetCurrentThreadId();
  unsigned calls = 0, threshold = std::numeric_limits<unsigned>::max();
  bool active = true, wrong_thread = false, retained = false;
  static int32_t QPX_READER_CALL poll(void *raw) {
    auto &value = *static_cast<Cancellation *>(raw);
    value.wrong_thread |= GetCurrentThreadId() != value.thread;
    value.retained |= !value.active;
    return ++value.calls >= value.threshold;
  }
  qpx_reader_cancel_v1 token() { return {poll, this}; }
};
void direct_abi(const fs::path &manifest, const fs::path &counts,
                const fs::path &continuous) {
  const auto config = Json::parse(bytes(manifest));
  fs::path module_path;
  for (const auto &item : config.at("plugins"))
    if (item.at("provider_id") == provider)
      module_path = manifest.parent_path() /
                    fs::u8path(item.at("module").at("path").get<std::string>());
  check(!module_path.empty(), "Actual Loom provider is absent from manifest");
  Module module(module_path);
  const auto entry = reinterpret_cast<qpx_matrix_format_entry_fn_v2>(
      GetProcAddress(module.value, "qpx_matrix_format_entry_v2"));
  check(entry != nullptr, "Actual provider lacks its separate typed entry");
  Error error;
  qpx_matrix_format_api_v2 api{};
  api.struct_size = sizeof(api);
  check(entry(1, &api, &error.text) != QPX_READER_OK,
        "Typed entry silently accepted ABI1");
  status(entry(2, nullptr, &error.text), QPX_READER_INVALID_REQUEST, error,
         "Null typed table");
  api.struct_size = 4;
  status(entry(2, &api, &error.text), QPX_READER_INVALID_REQUEST, error,
         "Short typed table");
  api.struct_size = sizeof(api);
  status(entry(2, &api, &error.text), QPX_READER_OK, error,
         "Actual typed table");
  check(api.struct_size == sizeof(api) && api.abi_version == 2 &&
            api.describe && api.open && api.destroy && api.metadata &&
            api.axis && api.values,
        "Typed C table is incomplete");
  for (const bool real : {false, true}) {
    const auto path = (real ? continuous : counts).u8string();
    const auto selection = interpretation(real).dump();
    Cancellation token;
    qpx_matrix_open_v2 request{sizeof(request),
                               0,
                               {path.data(), path.size()},
                               {selection.data(), selection.size()},
                               16 * mib,
                               1024ull * mib,
                               token.token()};
    const auto saved = request;
    AbiHandle handle(api);
    status(api.open(&request, &handle.value, &error.text), QPX_READER_OK, error,
           "Actual typed open");
    check(handle.value && std::memcmp(&request, &saved, sizeof(request)) == 0,
          "Typed open mutated request or omitted handle");
    token.active = false;
    std::array<char, 132> label;
    label.fill('!');
    qpx_matrix_metadata_v2 metadata{};
    metadata.struct_size = sizeof(metadata);
    metadata.value_domain = {label.data() + 1, 0, 0};
    status(api.metadata(handle.value, &metadata, {}, &error.text),
           QPX_READER_BUFFER_TOO_SMALL, error, "Typed domain label probe");
    check(metadata.value_domain.size > 0 && metadata.value_domain.size <= 128 &&
              std::all_of(label.begin(), label.end(),
                          [](char c) { return c == '!'; }),
          "Typed metadata probe wrote payload or unbounded text");
    metadata.value_domain.capacity = 128;
    status(api.metadata(handle.value, &metadata, {}, &error.text),
           QPX_READER_OK, error, "Typed domain label fill");
    check(metadata.struct_size == sizeof(metadata) && metadata.features == 3 &&
              metadata.observations == 4 && !metadata.reserved[0] &&
              !metadata.reserved[1] &&
              metadata.value_type == (real ? QPX_FORMAT_CONTINUOUS_FLOAT64
                                           : QPX_FORMAT_COUNTS_UINT64) &&
              std::string(metadata.value_domain.data,
                          std::size_t(metadata.value_domain.size)) ==
                  (real ? "literal-signed" : "raw-counts") &&
              metadata.value_domain.data == label.data() + 1 &&
              metadata.value_domain.capacity == 128 && label.front() == '!' &&
              label.back() == '!',
          "Typed metadata/domain label/pointer/canary differs");
    std::array<std::uint64_t, 10> integers;
    integers.fill(UINT64_C(0xa1b2c3d4e5f67890));
    std::array<double, 10> reals;
    reals.fill(-789.25);
    qpx_matrix_values_request_v1 page{sizeof(page), 0, 1, 7};
    qpx_matrix_values_buffer_v2 values{sizeof(values),
                                       metadata.value_type,
                                       real ? nullptr : integers.data() + 1,
                                       real ? reals.data() + 1 : nullptr,
                                       7,
                                       0,
                                       0};
    const auto requested = page;
    status(api.values(handle.value, &page, &values, {}, &error.text),
           QPX_READER_OK, error, "Typed partial value page");
    check(values.written == 7 && values.capacity == 7 && !values.reserved &&
              values.counts == (real ? nullptr : integers.data() + 1) &&
              values.continuous == (real ? reals.data() + 1 : nullptr) &&
              std::memcmp(&page, &requested, sizeof(page)) == 0,
          "Typed value page mutated inputs or returned wrong length");
    for (std::size_t i = 0; i < 7; ++i) {
      const auto linear = i + 1, observation = linear / feature_count,
                 feature = linear % feature_count;
      const auto index = feature * observation_count + observation;
      if (real)
        check(bits(reals[i + 1]) == bits(exact_continuous[index]),
              "Typed partial float page transposed/rounded");
      else
        check(integers[i + 1] == exact_counts[index],
              "Typed partial UInt64 page transposed/rounded");
    }
    check(integers.front() == UINT64_C(0xa1b2c3d4e5f67890) &&
              integers[8] == UINT64_C(0xa1b2c3d4e5f67890) &&
              integers.back() == UINT64_C(0xa1b2c3d4e5f67890) &&
              reals.front() == -789.25 && reals[8] == -789.25 &&
              reals.back() == -789.25,
          "Typed values overwrote output canaries");
    const auto before_integers = integers;
    const auto before_reals = reals;
    values.capacity = 1;
    values.written = 0;
    status(api.values(handle.value, &page, &values, {}, &error.text),
           QPX_READER_BUFFER_TOO_SMALL, error, "Typed value capacity probe");
    check(values.written == 7 && integers == before_integers &&
              reals == before_reals,
          "Typed value probe wrote partial payload");
    values.capacity = 7;
    values.value_type =
        real ? QPX_FORMAT_COUNTS_UINT64 : QPX_FORMAT_CONTINUOUS_FLOAT64;
    status(api.values(handle.value, &page, &values, {}, &error.text),
           QPX_READER_INVALID_REQUEST, error, "Mismatched typed value buffer");
    values.value_type = metadata.value_type;
    page.first = 13;
    status(api.values(handle.value, &page, &values, {}, &error.text),
           QPX_READER_INVALID_REQUEST, error, "Typed page outside range");
    page.first = 1;
    page.count = 0;
    status(api.values(handle.value, &page, &values, {}, &error.text),
           QPX_READER_INVALID_REQUEST, error, "Zero-size typed page request");
    page = requested;
    Cancellation cancelled;
    cancelled.threshold = 1;
    status(api.values(handle.value, &page, &values, cancelled.token(),
                      &error.text),
           QPX_READER_CANCELLED, error, "Cancelled typed value page");
    check(!cancelled.wrong_thread && !token.wrong_thread && !token.retained &&
              std::memcmp(integers.data(), before_integers.data(), sizeof(integers)) == 0 &&
              std::memcmp(reals.data(), before_reals.data(), sizeof(reals)) == 0,
          "Provider retained or moved the caller's cancellation callback");
  }
}

void project_route(
    const fs::path &work,
    const std::shared_ptr<const ExpressionMatrixSource> &counts,
    const std::shared_ptr<const ExpressionMatrixSource> &continuous) {
  Project project;
  auto document = std::make_shared<Document>(
      Image(64, 64, std::vector<std::uint8_t>(64 * 64 * 3, 30)));
  document->set_calibration(.25, .5);
  const auto image =
      project.add_document(document, "Independent native Loom association");
  const auto count_id = keep_expression_matrix(project, image, counts, true);
  const auto real_id = keep_expression_matrix(project, image, continuous, true);
  check(count_id != real_id &&
            expression_matrix_ids(project, image).size() == 2,
        "Counts/continuous native associations collided");
  project.save_new((work / "owned.qpx").u8string());
  auto reopened = Project::open((work / "owned.qpx").u8string());
  for (const auto &id : {count_id, real_id}) {
    const auto association =
        load_expression_matrix_association(*reopened, image, id);
    check(association.owned,
          "Saved native association did not retain owned storage");
    const auto source = reopen_expression_matrix(*reopened, image, id);
    if (source->metadata().domain == ExpressionValueDomain::Counts)
      assert_counts(*source);
    else
      assert_continuous(*source);
  }
  write(work / "association-ids.json",
        Json{{"image", image}, {"counts", count_id}, {"continuous", real_id}}
                .dump(2) +
            "\n");
}

// Ordinary external-image fixture shared with fresh-process CLI/SDK checks.
// These reference captures reuse the already asserted numerical inputs above;
// consumers compare native artifacts rather than reimplementing their oracles.
void cli_sdk_fixtures(const fs::path &work) {
  const auto image_path = work / "image.ppm";
  const auto project_path = work / "input.qpx";
  write(image_path, "P6\n64 64\n255\n" + std::string(64 * 64 * 3, char(30)));
  Project input;
  const auto image = input.add_image(image_path.u8string(), "auto", "Loom input");
  const auto direct_frame = image_coordinate_frame(
      *input.document(image), "qpx.format-import-cli-image/1", ObjectPlane{});
  input.save_new(project_path.u8string());
  const auto reopened = Project::open(project_path.u8string());
  const auto project_frame = image_coordinate_frame(
      *reopened->document(image), "qpx.format-import-cli-image/1", ObjectPlane{});
  check(direct_frame.sha256() == project_frame.sha256(),
        "External image frame changed through native project persistence");

  MatrixPluginImportOptions native_options;
  native_options.frame = direct_frame;
  Json reference{{"schema", "qpx.loom-native-reference/1"},
                 {"image", image_path.u8string()},
                 {"input_project", project_path.u8string()},
                 {"image_id", image},
                 {"positions", (work / "positions.tsv").u8string()},
                 {"frame_sha256", direct_frame.sha256()},
                 {"plugin_work_bytes", native_options.plugin_work_bytes},
                 {"host_work_bytes", native_options.limits.work_bytes}};
  for (const bool continuous : {false, true}) {
    const std::string kind = continuous ? "continuous" : "counts";
    const auto source_path = work / (kind + ".loom");
    const auto output = work / ("cli-" + kind + ".qmx");
    const auto selected = interpretation(continuous);
    const auto indexed = import_expression_matrix_plugin(
        provider, source_path.u8string(), (work / "positions.tsv").u8string(),
        output.u8string(), native_options, selected.dump());
    const auto source = open_expression_matrix(indexed.path, indexed.file_sha256);
    const auto captured = external_expression_matrix_receipt(*source);
    check(captured && source->metadata().spatial_frame &&
              source->metadata().spatial_frame->sha256() ==
                  direct_frame.sha256() &&
              indexed.metadata.source_sha256 ==
                  source->metadata().source_sha256 &&
              source->metadata().domain ==
                  (continuous ? ExpressionValueDomain::Continuous
                              : ExpressionValueDomain::Counts) &&
              Json::parse(captured->reader.options) == selected,
          "Native CLI/SDK reference lost its frame/domain/options/receipt");
    reference[kind] = {
        {"input", source_path.u8string()},
        {"path", indexed.path},
        {"file_sha256", indexed.file_sha256},
        {"source_sha256", indexed.metadata.source_sha256},
        {"features", indexed.metadata.features},
        {"observations", indexed.metadata.observations},
        {"entries", indexed.metadata.entries},
        {"domain", kind},
        {"value_domain", indexed.metadata.value_domain},
        {"input_profile", indexed.metadata.input_profile},
        {"options", selected},
        {"options_json", captured->reader.options},
        {"reader_receipt_json", format_plugin_receipt_json(captured->reader)},
        {"reader_receipt_sha256",
         format_plugin_receipt_identity(captured->reader)},
        {"positions_sha256", captured->positions_sha256}};
  }
  write(work / "native-reference.json", reference.dump(2) + "\n");
}

void positive(const fs::path &manifest, const fs::path &work) {
  fixture(work / "counts.loom");
  fixture(work / "continuous.loom", true);
  write(work / "positions.tsv", position_text());
  direct_abi(manifest, work / "counts.loom", work / "continuous.loom");
  const auto indexed = import(work / "counts.loom", work / "positions.tsv",
                              work / "counts.qmx", interpretation());
  const auto real_indexed =
      import(work / "continuous.loom", work / "positions.tsv",
             work / "continuous.qmx", interpretation(true));
  const auto counts = open_expression_matrix(indexed.path, indexed.file_sha256);
  const auto continuous =
      open_expression_matrix(real_indexed.path, real_indexed.file_sha256);
  assert_counts(*counts);
  assert_continuous(*continuous);
  receipt(*counts, work / "counts.loom", work / "positions.tsv",
          interpretation());
  receipt(*continuous, work / "continuous.loom", work / "positions.tsv",
          interpretation(true));
  auto selection = interpretation();
  selection["matrix"] = "/layers/selected";
  const auto layer_result = import(work / "counts.loom", work / "positions.tsv",
                                   work / "layer.qmx", selection);
  const auto layer =
      open_expression_matrix(layer_result.path, layer_result.file_sha256);
  std::vector<std::uint64_t> literal_layer(12);
  std::iota(literal_layer.begin(), literal_layer.end(), UINT64_C(101));
  check(all(*layer).counts == literal_layer,
        "Explicit selected layer lost rows/columns or used main matrix");
  check(layer->metadata().source_sha256 != counts->metadata().source_sha256,
        "Different layer recipe aliased the original source");
  auto no_names = interpretation(false, false);
  const auto unnamed_result =
      import(work / "counts.loom", work / "positions.tsv",
             work / "no-names.qmx", no_names);
  const auto unnamed =
      open_expression_matrix(unnamed_result.path, unnamed_result.file_sha256);
  assert_axes(*unnamed, false);
  check(unnamed->metadata().source_sha256 != counts->metadata().source_sha256,
        "Explicit ID-only feature selection aliased named feature recipe");
  const auto unicode = work / fs::u8path(u8"unicode-\u03bc.loom");
  fs::copy_file(work / "counts.loom", unicode);
  const auto unicode_result = import(unicode, work / "positions.tsv",
                                     work / "unicode.qmx", interpretation());
  check(unicode_result.file_sha256 == indexed.file_sha256,
        "Relocating exact file bytes to a Unicode locator changed QMX content "
        "identity");
  const std::array<std::pair<hid_t, hid_t>, 8> integral_types{
      {{H5T_STD_U8LE, H5T_NATIVE_UINT64},
       {H5T_STD_U16BE, H5T_NATIVE_UINT64},
       {H5T_STD_U32LE, H5T_NATIVE_UINT64},
       {H5T_STD_U64BE, H5T_NATIVE_UINT64},
       {H5T_STD_I8LE, H5T_NATIVE_INT64},
       {H5T_STD_I16BE, H5T_NATIVE_INT64},
       {H5T_STD_I32LE, H5T_NATIVE_INT64},
       {H5T_STD_I64BE, H5T_NATIVE_INT64}}};
  std::array<std::uint64_t, 12> small_unsigned{};
  std::array<std::int64_t, 12> small_signed{};
  for (std::size_t i = 0; i < 12; ++i)
    small_unsigned[i] = std::uint64_t(small_signed[i] = std::int64_t(i + 1));
  for (std::size_t i = 0; i < integral_types.size(); ++i) {
    const auto input = work / ("integer-" + std::to_string(i) + ".loom");
    fs::copy_file(work / "counts.loom", input);
    {
      H5 file(H5Fopen(input.u8string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT));
      replace_matrix(file, integral_types[i].first, integral_types[i].second,
                     i < 4 ? static_cast<const void *>(small_unsigned.data())
                           : small_signed.data());
    }
    const auto result = import(input, work / "positions.tsv",
                               work / ("integer-" + std::to_string(i) + ".qmx"),
                               interpretation());
    check(
        all(*open_expression_matrix(result.path, result.file_sha256)).counts ==
            std::vector<std::uint64_t>(small_unsigned.begin(),
                                       small_unsigned.end()),
        "Integral type/endian conversion changed exact nonnegative values");
  }
  std::array<float, 12> float32{};
  for (std::size_t i = 0; i < float32.size(); ++i)
    float32[i] = float(int(i) - 5) / 4;
  const auto single = work / "float32.loom";
  fs::copy_file(work / "continuous.loom", single);
  {
    H5 file(H5Fopen(single.u8string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT));
    replace_matrix(file, H5T_IEEE_F32BE, H5T_NATIVE_FLOAT, float32.data());
  }
  const auto single_result = import(single, work / "positions.tsv",
                                    work / "float32.qmx", interpretation(true));
  std::array<double, 12> double32{};
  for (std::size_t i = 0; i < 12; ++i)
    double32[i] = float32[i];
  same_reals(all(*open_expression_matrix(single_result.path,
                                         single_result.file_sha256))
                 .continuous,
             double32, "Exact Float32 to native continuous values");
  const auto half = work / "float16.loom";
  fs::copy_file(work / "continuous.loom", half);
  const std::array<std::uint16_t, 12> half_bits{0x8000, 0xbc00, 0x0001, 0x3c00,
                                                0x4000, 0x0000, 0xc000, 0x3800,
                                                0x0400, 0x8400, 0x7bff, 0xfbff};
  const std::array<double, 12> half_values{-0.0,
                                           -1,
                                           std::ldexp(1., -24),
                                           1,
                                           2,
                                           0.0,
                                           -2,
                                           .5,
                                           std::ldexp(1., -14),
                                           -std::ldexp(1., -14),
                                           65504,
                                           -65504};
  {
    H5 file(H5Fopen(half.u8string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT));
    replace_matrix(file, H5T_IEEE_F16LE, H5T_IEEE_F16LE, half_bits.data());
  }
  const auto half_result = import(half, work / "positions.tsv",
                                  work / "float16.qmx", interpretation(true));
  same_reals(
      all(*open_expression_matrix(half_result.path, half_result.file_sha256))
          .continuous,
      half_values, "Independent IEEE16 bit patterns");
  for (unsigned mask = 1; mask < 8; ++mask) {
    Layout layout;
    layout.chunked = true;
    layout.deflate = (mask & 1) != 0;
    layout.shuffle = (mask & 2) != 0;
    layout.fletcher = (mask & 4) != 0;
    const auto input = work / ("codec-" + std::to_string(mask) + ".loom");
    fixture(input, false, layout, true);
    const auto result = import(
        input, work / "positions.tsv",
        work / ("codec-" + std::to_string(mask) + ".qmx"), interpretation());
    assert_counts(*open_expression_matrix(result.path, result.file_sha256));
  }
  Layout user_block_layout;
  user_block_layout.chunked = user_block_layout.deflate = true;
  fixture(work / "user-block.loom", false, user_block_layout, true, true);
  const auto user_block =
      import(work / "user-block.loom", work / "positions.tsv",
             work / "user-block.qmx", interpretation());
  assert_counts(
      *open_expression_matrix(user_block.path, user_block.file_sha256));
  const auto omitted = work / "unselected-metadata.loom";
  fs::copy_file(work / "counts.loom", omitted);
  {
    H5 file(H5Fopen(omitted.u8string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT));
    strings(file, "/attrs/unselected-large-text", {std::string(65536, 'q')},
            true);
  }
  const auto omitted_result =
      import(omitted, work / "positions.tsv", work / "unselected-metadata.qmx",
             interpretation());
  assert_counts(
      *open_expression_matrix(omitted_result.path, omitted_result.file_sha256));
  check(omitted_result.metadata.source_sha256 !=
            counts->metadata().source_sha256,
        "Ignored metadata was inferred into values or omitted from whole-file "
        "source identity");
  auto session = detail::MatrixPluginSession::open_with_file_limit(
      provider, (work / "counts.loom").u8string(), interpretation().dump(),
      16 * mib, 1024ull * mib);
  check(session->metadata().domain == ExpressionValueDomain::Counts,
        "Typed count session metadata");
  const auto tail = session->values(12, 4);
  check(tail.empty(), "Typed first==total did not return an empty page");
  refuses<std::invalid_argument>([&] { session->continuous_values(0, 1); });
  session.reset();
  project_route(work, counts, continuous);
  cli_sdk_fixtures(work);
  no_stage(work);
  std::cout << "Loom positive types/layers/codecs/frames/native QMX/project "
               "routes pass\n";
}

template <class F>
void rejected_file(const fs::path &work, const std::string &name, F &&mutate,
                   Json selection = interpretation(), const std::string &expected_error = {}) {
  const auto input = work / (name + ".loom"), output = work / (name + ".qmx");
  fs::copy_file(work / "counts.loom", input);
  {
    H5 file(H5Fopen(input.u8string().c_str(), H5F_ACC_RDWR, H5P_DEFAULT));
    mutate(file);
  }
  const auto original = sha(input);
  const auto explanation = refuses([&] { import(input, work / "positions.tsv", output, selection); });
  if (!expected_error.empty())
    check(explanation.find(expected_error) != std::string::npos,
          "Loom refusal did not reach its named native trust boundary: " + name + ": " + explanation);
  check(!fs::exists(output) && sha(input) == original,
        "Refused Loom import changed input or published output: " + name);
  no_stage(work);
}
void negative(const fs::path &work) {
  rejected_file(work, "missing-marker", [](hid_t f) {
    hstatus(H5Ldelete(f, "/attrs/LOOM_SPEC_VERSION", H5P_DEFAULT),
            "remove marker");
  });
  for (const auto *marker : {"2.0.1", "v3.0.0", "3.0.1"}) {
    rejected_file(work, std::string("marker-") + marker, [&](hid_t f) {
      hstatus(H5Ldelete(f, "/attrs/LOOM_SPEC_VERSION", H5P_DEFAULT),
              "replace marker");
      strings(f, "/attrs/LOOM_SPEC_VERSION", {marker}, true);
    });
  }
  rejected_file(work, "marker-rank", [](hid_t f) {
    hstatus(H5Ldelete(f, "/attrs/LOOM_SPEC_VERSION", H5P_DEFAULT),
            "replace marker rank");
    strings(f, "/attrs/LOOM_SPEC_VERSION", {"3.0.0"});
  });
  rejected_file(work, "missing-graphs", [](hid_t f) {
    hstatus(H5Ldelete(f, "/row_graphs", H5P_DEFAULT), "remove required group");
  });
  rejected_file(work, "matrix-shape", [](hid_t f) {
    replace_matrix(f, H5T_STD_U64LE, H5T_NATIVE_UINT64, exact_counts.data(),
                   {2, 4});
  });
  for (const auto *axis : {"/row_attrs/ID", "/col_attrs/ID"}) {
    const bool feature = std::string(axis).find("row") != std::string::npos;
    rejected_file(
        work, feature ? "duplicate-features" : "duplicate-observations",
        [&](hid_t f) {
          hstatus(H5Ldelete(f, axis, H5P_DEFAULT), "replace duplicate ID axis");
          strings(f, axis,
                  feature ? std::vector<std::string>{"same", "same", "other"}
                          : std::vector<std::string>{"same", "same", "other",
                                                     "last"});
        });
    rejected_file(
        work, feature ? "empty-feature" : "empty-observation", [&](hid_t f) {
          hstatus(H5Ldelete(f, axis, H5P_DEFAULT), "replace empty ID axis");
          strings(f, axis,
                  feature ? std::vector<std::string>{"", "second", "third"}
                          : std::vector<std::string>{"", "second", "third",
                                                     "fourth"});
        });
    rejected_file(
        work, feature ? "oversized-feature" : "oversized-observation",
        [&](hid_t f) {
          hstatus(H5Ldelete(f, axis, H5P_DEFAULT), "replace oversized ID axis");
          strings(f, axis,
                  feature ? std::vector<std::string>{std::string(257, 'x'), "b",
                                                     "c"}
                          : std::vector<std::string>{std::string(257, 'x'), "b",
                                                     "c", "d"});
        });
  }
  rejected_file(work, "invalid-utf8-id", [](hid_t f) {
    hstatus(H5Ldelete(f, "/row_attrs/ID", H5P_DEFAULT), "replace UTF8 axis");
    strings(f, "/row_attrs/ID", {std::string("\xc0\xaf", 2), "b", "c"});
  });
  for (const auto kind :
       {"negative", "fractional", "nan", "inf", "uint64-overflow"}) {
    rejected_file(work, std::string("count-") + kind, [&](hid_t f) {
      std::array<double, 12> values{};
      if (std::string(kind) == "negative")
        values[0] = -1;
      if (std::string(kind) == "fractional")
        values[0] = .25;
      if (std::string(kind) == "nan")
        values[0] = std::numeric_limits<double>::quiet_NaN();
      if (std::string(kind) == "inf")
        values[0] = std::numeric_limits<double>::infinity();
      if (std::string(kind) == "uint64-overflow")
        values[0] = std::ldexp(1., 64);
      replace_matrix(f, H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE, values.data());
    });
  }
  for (const auto kind : {"nan", "inf"}) {
    rejected_file(
        work, std::string("continuous-") + kind,
        [&](hid_t f) {
          auto values = exact_continuous;
          values[0] = std::string(kind) == "nan"
                          ? std::numeric_limits<double>::quiet_NaN()
                          : -std::numeric_limits<double>::infinity();
          replace_matrix(f, H5T_IEEE_F64LE, H5T_NATIVE_DOUBLE, values.data());
        },
        interpretation(true));
  }
  rejected_file(
      work, "continuous-inexact-integer", [](hid_t) {}, interpretation(true));
  rejected_file(work, "soft-matrix", [](hid_t f) {
    hstatus(H5Lmove(f, "/matrix", f, "/saved-matrix", H5P_DEFAULT, H5P_DEFAULT),
            "move matrix");
    hstatus(
        H5Lcreate_soft("/saved-matrix", f, "/matrix", H5P_DEFAULT, H5P_DEFAULT),
        "soft selected matrix");
  });
  rejected_file(work, "soft-ids", [](hid_t f) {
    hstatus(H5Lmove(f, "/row_attrs/ID", f, "/row_attrs/saved", H5P_DEFAULT,
                    H5P_DEFAULT),
            "move ID data");
    hstatus(H5Lcreate_soft("/row_attrs/saved", f, "/row_attrs/ID", H5P_DEFAULT,
                           H5P_DEFAULT),
            "soft selected ID data");
  });
  rejected_file(work, "external-link", [&](hid_t f) {
    hstatus(H5Ldelete(f, "/matrix", H5P_DEFAULT),
            "remove matrix for external link");
    hstatus(H5Lcreate_external((work / "counts.loom").u8string().c_str(),
                               "/matrix", f, "/matrix", H5P_DEFAULT,
                               H5P_DEFAULT),
            "external selected matrix");
  });
  rejected_file(work, "external-storage", [](hid_t f) {
    hstatus(H5Ldelete(f, "/matrix", H5P_DEFAULT),
            "remove matrix for external storage");
    const std::array<hsize_t, 2> dimensions{3, 4};
    H5 space(H5Screate_simple(2, dimensions.data(), nullptr));
    H5 creation(H5Pcreate(H5P_DATASET_CREATE));
    hstatus(H5Pset_external(creation, "must-not-open-external-payload.bin", 0,
                            H5F_UNLIMITED),
            "external raw storage descriptor");
    H5 data(H5Dcreate2(f, "/matrix", H5T_STD_U64LE, space, H5P_DEFAULT,
                       creation, H5P_DEFAULT));
  });
  check(!fs::exists(work / "must-not-open-external-payload.bin"),
        "Reader accessed external raw storage");
  rejected_file(work, "virtual-storage", [&](hid_t f) {
    hstatus(H5Ldelete(f, "/matrix", H5P_DEFAULT),
            "remove matrix for virtual storage");
    const std::array<hsize_t, 2> dimensions{3, 4};
    H5 space(H5Screate_simple(2, dimensions.data(), nullptr));
    H5 source(H5Scopy(space));
    H5 creation(H5Pcreate(H5P_DATASET_CREATE));
    hstatus(H5Pset_virtual(creation, space,
                           (work / "counts.loom").u8string().c_str(), "/matrix",
                           source),
            "virtual dataset descriptor");
    H5 data(H5Dcreate2(f, "/matrix", H5T_STD_U64LE, space, H5P_DEFAULT,
                       creation, H5P_DEFAULT));
  });
  rejected_file(work, "unknown-filter", [](hid_t f) {
    Layout layout;
    layout.chunked = true;
    layout.unknown_filter = true;
    replace_matrix(f, H5T_STD_U64LE, H5T_NATIVE_UINT64, nullptr, {3, 4},
                   layout);
  });
  rejected_file(work, "oversized-chunk", [](hid_t f) {
    Layout layout;
    layout.chunked = true;
    layout.unlimited = true;
    layout.chunk = {512, 512};
    replace_matrix(f, H5T_STD_U64LE, H5T_NATIVE_UINT64, nullptr, {3, 4},
                   layout);
  });
  rejected_file(work, "encoded-chunk-limit", [](hid_t f) {
    Layout layout;
    layout.chunked = true;
    layout.deflate = true;
    layout.chunk = {2, 2};
    replace_matrix(f, H5T_STD_U64LE, H5T_NATIVE_UINT64, nullptr, {3, 4},
                   layout);
    raw_chunk_payload(f, "/matrix", std::vector<unsigned char>(32 + 65536 + 1));
  }, interpretation(), "HDF5 encoded chunk exceeds allowance or has invalid filter mask");
  rejected_file(work, "deflate-overexpansion", [](hid_t f) {
    Layout layout;
    layout.chunked = layout.deflate = layout.unlimited = true;
    layout.chunk = {2, 2};
    replace_matrix(f, H5T_STD_U64LE, H5T_NATIVE_UINT64, nullptr, {3, 4}, layout);
    raw_chunk_payload(f, "/matrix", mismatched_deflate());
  }, interpretation(), "HDF5 Deflate stream is invalid or exceeds declared decoded chunk");
  rejected_file(work, "id-deflate-overexpansion", [](hid_t f) {
    hstatus(H5Ldelete(f, "/row_attrs/ID", H5P_DEFAULT),
            "replace compressed ID chunks");
    strings(f, "/row_attrs/ID", {feature_ids.begin(), feature_ids.end()}, false,
            true, 2, true);
    raw_chunk_payload(f, "/row_attrs/ID", mismatched_deflate());
  }, interpretation(), "HDF5 Deflate stream is invalid or exceeds declared decoded chunk");
  for (unsigned failure = 0; failure < 7; ++failure) {
    auto selection = interpretation();
    if (failure == 0)
      selection.erase("domain");
    if (failure == 1)
      selection["domain"] = "automatic";
    if (failure == 2)
      selection["value_domain"] = "guessed-count-label";
    if (failure == 3)
      selection["matrix"] = "/matrix/../layers/selected";
    if (failure == 4)
      selection["unknown"] = true;
    if (failure == 5)
      selection["feature_ids"] = "/col_attrs/ID";
    if (failure == 6)
      selection["observation_ids"] = "/row_attrs/ID";
    const auto output =
        work / ("options-refused-" + std::to_string(failure) + ".qmx");
    refuses([&] {
      import(work / "counts.loom", work / "positions.tsv", output, selection);
    });
    check(!fs::exists(output),
          "Invalid/implicit provider interpretation published output");
    no_stage(work);
  }
  const auto duplicate_json =
      R"({"matrix":"/matrix","matrix":"/layers/selected","feature_ids":"/row_attrs/ID","observation_ids":"/col_attrs/ID","domain":"counts","value_domain":"raw-counts"})";
  refuses([&] {
    import_expression_matrix_plugin(provider, (work / "counts.loom").u8string(),
                                    (work / "positions.tsv").u8string(),
                                    (work / "duplicate-option.qmx").u8string(),
                                    options(), std::string(duplicate_json));
  });
  check(!fs::exists(work / "duplicate-option.qmx"),
        "Duplicate provider options published output");
  for (const auto kind : {"missing", "extra", "duplicate"}) {
    auto text = position_text();
    if (std::string(kind) == "missing")
      text = "observation_id\tx_px\ty_px\n0001\t1.25\t2.5\n";
    if (std::string(kind) == "extra")
      text += "foreign\t1\t2\n";
    if (std::string(kind) == "duplicate")
      text += "0001\t1.25\t2.5\n";
    const auto positions = work / (std::string("positions-") + kind + ".tsv");
    const auto output = work / (std::string("positions-") + kind + ".qmx");
    write(positions, text);
    refuses([&] {
      import(work / "counts.loom", positions, output, interpretation());
    });
    check(!fs::exists(output),
          "Incomplete/ambiguous position join published output");
    no_stage(work);
  }
  for (unsigned failure = 0; failure < 7; ++failure) {
    auto limits = options();
    if (failure == 0)
      limits.limits.max_features = 2;
    if (failure == 1)
      limits.limits.max_observations = 3;
    if (failure == 2)
      limits.limits.max_entries = 11;
    if (failure == 3)
      limits.limits.file_bytes = fs::file_size(work / "counts.loom") - 1;
    if (failure == 4)
      limits.limits.store_bytes = 4096;
    if (failure == 5)
      limits.plugin_work_bytes = 15 * mib;
    if (failure == 6)
      limits.limits.work_bytes = 4 * mib + limits.plugin_work_bytes;
    const auto output =
        work / ("limit-refused-" + std::to_string(failure) + ".qmx");
    refuses([&] {
      import(work / "counts.loom", work / "positions.tsv", output,
             interpretation(), limits);
    });
    check(!fs::exists(output),
          "Native byte/count/work quota failure published output");
    no_stage(work);
  }
  std::atomic<bool> cancelled{true};
  refuses<WorkflowCancelled>([&] {
    import(work / "counts.loom", work / "positions.tsv", work / "cancelled.qmx",
           interpretation(), options(), &cancelled);
  });
  check(!fs::exists(work / "cancelled.qmx"),
        "Cancelled native Loom request published output");
  const auto existing = bytes(work / "counts.qmx");
  refuses<WorkflowOutputExists>([&] {
    import(work / "counts.loom", work / "positions.tsv", work / "counts.qmx",
           interpretation());
  });
  check(bytes(work / "counts.qmx") == existing,
        "Existing native QMX output was replaced");
  for (const auto &output : {work / "counts.loom", work / "positions.tsv"}) {
    const auto original = bytes(output);
    refuses([&] {
      import(work / "counts.loom", work / "positions.tsv", output,
             interpretation());
    });
    check(bytes(output) == original,
          "Existing/protected destination was replaced");
  }
  for (unsigned corruption = 0; corruption < 5; ++corruption) {
    const auto path = work / ("corrupt-" + std::to_string(corruption) + ".qmx");
    fs::copy_file(work / "continuous.qmx", path);
    if (corruption == 0) {
      sql(path, "UPDATE counts SET value=x'000000000000f87f' WHERE feature=0 "
                "AND observation=0");
      const auto source = open_expression_matrix(path.u8string());
      refuses([&] { source->read({{0}, std::nullopt}); });
    } else {
      if (corruption == 1)
        sql(path, "UPDATE metadata SET value='raw-counts' WHERE key='domain'");
      if (corruption == 2)
        sql(path, "UPDATE metadata SET value='{}' WHERE key='external_reader'");
      if (corruption == 3)
        sql(path,
            "UPDATE metadata SET value='{}' WHERE key='external_positions'");
      if (corruption == 4)
        sql(path, "UPDATE metadata SET "
                  "value='"
                  "000000000000000000000000000000000000000000000000000000000000"
                  "0000' WHERE key='source_sha256'");
      refuses([&] { open_expression_matrix(path.u8string()); });
    }
  }
  const auto changed = work / "changed-source.loom";
  fs::copy_file(work / "counts.loom", changed);
  auto captured = detail::MatrixPluginSession::open_with_file_limit(
      provider, changed.u8string(), interpretation().dump(), 16 * mib,
      1024ull * mib);
  const auto captured_sha = sha(changed);
  const auto writer =
      CreateFileW(changed.c_str(), GENERIC_WRITE,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  const auto write_error = GetLastError();
  if (writer != INVALID_HANDLE_VALUE)
    CloseHandle(writer);
  check(writer == INVALID_HANDLE_VALUE &&
            write_error == ERROR_SHARING_VIOLATION &&
            sha(changed) == captured_sha,
        "Active provider did not deny mutation under its native read lease");
  captured->verify_source();
  check(captured->values(0, 1) == std::vector<std::uint64_t>{UINT64_MAX},
        "Denied source mutation invalidated the unchanged native source");
  captured.reset();
  {
    std::ofstream changed_file(changed, std::ios::binary | std::ios::app);
    changed_file << "owned bytes changed after provider close\n";
    changed_file.close();
    check(bool(changed_file),
          "Released provider lease still prevents owner writes");
  }
  check(sha(changed) != captured_sha,
        "Post-close source mutation did not change exact bytes");
  const auto replacement =
      import(changed, work / "positions.tsv", work / "changed-source-new.qmx",
             interpretation());
  const auto original =
      open_expression_matrix((work / "counts.qmx").u8string());
  check(replacement.metadata.source_sha256 !=
                original->metadata().source_sha256 &&
            replacement.file_sha256 != original->file_sha256(),
        "Changed original bytes aliased an old result despite equal decoded "
        "values");
  no_stage(work);
  std::cout
      << "Loom malformed/type/identity/link/filter/chunk/budget/cancel/output "
         "refusals pass\n";
}

void inspect(const fs::path &work) {
  auto counts = open_expression_matrix((work / "counts.qmx").u8string());
  auto continuous =
      open_expression_matrix((work / "continuous.qmx").u8string());
  assert_counts(*counts);
  assert_continuous(*continuous);
  check(external_expression_matrix_receipt(*counts)->reader.provider.abi == 2 &&
            external_expression_matrix_receipt(*continuous)
                    ->reader.provider.abi == 2,
        "Provider-free schema5 receipt inspection failed");
  const auto ids = Json::parse(bytes(work / "association-ids.json"));
  const auto project = Project::open((work / "owned.qpx").u8string());
  assert_counts(*reopen_expression_matrix(*project,
                                          ids.at("image").get<std::uint64_t>(),
                                          ids.at("counts").get<std::string>()));
  assert_continuous(
      *reopen_expression_matrix(*project, ids.at("image").get<std::uint64_t>(),
                                ids.at("continuous").get<std::string>()));
  std::cout << Json{{"state", "indexed-metadata-and-owned-project"},
                    {"count_file", counts->file_sha256()},
                    {"count_source", counts->metadata().source_sha256},
                    {"continuous_file", continuous->file_sha256()},
                    {"continuous_source", continuous->metadata().source_sha256}}
                   .dump()
            << '\n';
}
void inspect_without_originals(const fs::path &work) {
  const std::array<fs::path, 2> inputs{work / "counts.loom", work / "continuous.loom"};
  std::array<std::string, 2> fingerprints;
  for (std::size_t i = 0; i < inputs.size(); ++i) {
    fingerprints[i] = sha(inputs[i]);
    const auto offline = fs::u8path(inputs[i].u8string() + ".offline");
    check(!fs::exists(offline), "Preserve any earlier offline fixture");
    fs::rename(inputs[i], offline);
  }
  inspect(work);
  for (std::size_t i = 0; i < inputs.size(); ++i) {
    const auto offline = fs::u8path(inputs[i].u8string() + ".offline");
    fs::rename(offline, inputs[i]);
    check(sha(inputs[i]) == fingerprints[i],
          "Provider-independent inspection changed original fixture bytes");
  }
}
void legacy(const fs::path &manifest, const fs::path &input,
            const fs::path &positions, const fs::path &baseline,
            const fs::path &output) {
  configure_format_plugins(manifest.u8string());
  const auto original = open_expression_matrix(baseline.u8string());
  check(original->metadata().input_profile == "external-dense-counts/1" &&
            original->metadata().spatial_frame,
        "Frozen witness is not a schema4 external-counts source");
  MatrixPluginImportOptions limits;
  limits.frame = *original->metadata().spatial_frame;
  const auto result = import_expression_matrix_plugin(
      "org.qupathxx.counts-tsv", input.u8string(), positions.u8string(),
      output.u8string(), limits);
  const auto current = open_expression_matrix(result.path, result.file_sha256);
  check(bytes(baseline) == bytes(output) &&
            original->file_sha256() == current->file_sha256() &&
            original->metadata().source_sha256 ==
                current->metadata().source_sha256 &&
            format_plugin_receipt_identity(
                external_expression_matrix_receipt(*original)->reader) ==
                format_plugin_receipt_identity(
                    external_expression_matrix_receipt(*current)->reader),
        "Old ABI1/schema4 QMX bytes/identity changed");
  std::cout << "Frozen ABI1/schema4 matrix output remains byte-exact\n";
}
void control(const fs::path &manifest, const fs::path &work) {
  fresh(work);
  configure_format_plugins(manifest.u8string());
  const auto positions = work / "positions.tsv";
  write(
      positions,
      "observation_id\tx_px\ty_px\ncell-c\t3\t6\ncell-a\t1\t2\ncell-b\t2\t4\n");
  const auto run = [&](const std::string &mode, bool expected) {
    const auto input = work / (mode + ".txt"), output = work / (mode + ".qmx");
    write(input, mode + "\n");
    const auto call = [&] {
      return import_expression_matrix_plugin(control_provider, input.u8string(),
                                             positions.u8string(),
                                             output.u8string(), options());
    };
    if (expected) {
      const auto result = call();
      const auto source =
          open_expression_matrix(result.path, result.file_sha256);
      const auto block = source->read({{0, 1}, std::nullopt});
      if (mode == "valid-counts")
        check(block.counts ==
                  std::vector<std::uint64_t>{UINT64_MAX, 0, 11,
                                             UINT64_C(9007199254740993), 7, 13},
              "Controlled typed counts were converted through floating point");
      else
        same_reals(
            block.continuous,
            std::array<double, 6>{-0.0, 2.5, 11, -1.25, 0.0,
                                  std::numeric_limits<double>::denorm_min()},
            "Controlled typed continuous values");
    } else {
      refuses(call);
      check(!fs::exists(output),
            "Malformed typed callback published native QMX");
    }
    const auto events = bytes(fs::u8path(input.u8string() + ".events"));
    check(events == "open\ndestroy\n",
          "Typed host did not destroy the handle exactly once: " + mode);
    no_stage(work);
  };
  run("valid-counts", true);
  run("valid-continuous", true);
  for (const auto *mode : {"open-error-handle",
                           "metadata-type",
                           "metadata-zero",
                           "metadata-huge",
                           "metadata-reserved",
                           "metadata-size",
                           "metadata-capacity",
                           "metadata-pointer",
                           "metadata-length",
                           "metadata-count-label",
                           "metadata-continuous-label",
                           "metadata-empty-label",
                           "metadata-utf8-label",
                           "metadata-long-label",
                           "metadata-io",
                           "axis-short",
                           "axis-long",
                           "axis-capacity",
                           "axis-pointer",
                           "axis-index",
                           "axis-offset",
                           "axis-duplicate",
                           "axis-observation-name",
                           "values-short",
                           "values-long",
                           "values-capacity",
                           "values-type",
                           "values-size",
                           "values-reserved",
                           "values-count-pointer",
                           "values-continuous-pointer",
                           "values-request",
                           "values-io",
                           "values-nan",
                           "values-inf"})
    run(mode, false);
  const auto large = work / "independent-file-budget.txt";
  write(large, "valid-counts\n" + std::string(96 * 1024, 'x'));
  auto streamed = detail::MatrixPluginSession::open_with_file_limit(
      control_provider, large.u8string(), std::nullopt, 65536, 128 * 1024);
  check(streamed->values(0, 1) == std::vector<std::uint64_t>{UINT64_MAX},
        "Independent file allowance did not permit a bounded streamed page");
  streamed.reset();
  refuses([&] {
    detail::MatrixPluginSession::open_with_file_limit(
        control_provider, large.u8string(), std::nullopt, 65536,
        fs::file_size(large) - 1);
  });
  const auto wait_input = work / "wait-values.txt",
             wait_output = work / "wait-values.qmx";
  write(wait_input, "wait-values\n");
  std::atomic<bool> cancelled{false};
  auto future = std::async(std::launch::async, [&] {
    return refuses<WorkflowCancelled>([&] {
      import_expression_matrix_plugin(
          control_provider, wait_input.u8string(), positions.u8string(),
          wait_output.u8string(), options(), std::nullopt, &cancelled);
    });
  });
  const auto events_path = fs::u8path(wait_input.u8string() + ".events");
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while ((!fs::exists(events_path) ||
          bytes(events_path).find("entered") == std::string::npos) &&
         std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  check(fs::exists(events_path) &&
            bytes(events_path).find("entered") != std::string::npos,
        "Controlled active callback never entered");
  cancelled = true;
  check(!future.get().empty() && !fs::exists(wait_output),
        "Active callback cancellation published output");
  check(bytes(events_path) == "open\nentered\ndestroy\n",
        "Active cancellation did not close the controlled handle once");
  const auto mutate_input = work / "mutate-source.txt";
  write(mutate_input, "mutate-source\n");
  const auto initial = sha(mutate_input);
  const auto original_bytes = bytes(mutate_input);
  const auto guarded_result = import_expression_matrix_plugin(
      control_provider, mutate_input.u8string(), positions.u8string(),
      (work / "mutate-source.qmx").u8string(), options());
  check(sha(mutate_input) == initial &&
            bytes(mutate_input) == original_bytes,
        "Publication lease control: attempted write changed source bytes");
  const auto guarded_source =
      open_expression_matrix(guarded_result.path, guarded_result.file_sha256);
  const auto guarded_values = guarded_source->read({{0, 1}, std::nullopt});
  const auto guarded_receipt =
      external_expression_matrix_receipt(*guarded_source);
  check(guarded_source->metadata().domain == ExpressionValueDomain::Counts &&
            guarded_values.counts ==
                std::vector<std::uint64_t>{
                    UINT64_MAX, 0, 11, UINT64_C(9007199254740993), 7, 13} &&
            guarded_values.continuous.empty() && guarded_receipt &&
            guarded_receipt->reader.source_sha256 == initial,
        "Publication lease control: unchanged source produced different "
        "values or input identity");
  check(bytes(fs::u8path(mutate_input.u8string() + ".events")) ==
            "open\nsource-write-denied\ndestroy\n",
        "Publication lease control: native guard did not deny the write or "
        "close the handle once");

  // A direct session has no publication lease between calls. Establish that
  // the owner edit actually succeeds before testing the captured version.
  const auto edited_input = work / "between-call-edit.txt";
  const std::string initial_text = "valid-counts\n";
  const std::string edit = "owner edit between completed native calls\n";
  write(edited_input, initial_text);
  const auto edit_initial_sha = sha(edited_input);
  auto changed_session = detail::MatrixPluginSession::open_with_file_limit(
      control_provider, edited_input.u8string(), std::nullopt, 65536,
      128 * 1024);
  check(changed_session->values(0, 6) ==
            std::vector<std::uint64_t>{
                UINT64_MAX, UINT64_C(9007199254740993), 0, 7, 11, 13},
        "Between-call source edit: valid session page differs before edit");
  {
    std::ofstream changed(edited_input, std::ios::binary | std::ios::app);
    changed.write(edit.data(), std::streamsize(edit.size()));
    changed.close();
    check(bool(changed),
          "Between-call source edit: owner append was denied or incomplete");
  }
  check(bytes(edited_input) == initial_text + edit &&
            fs::file_size(edited_input) == initial_text.size() + edit.size() &&
            sha(edited_input) != edit_initial_sha,
        "Between-call source edit: edit did not change actual bytes and SHA");
  refuses<WorkflowInputChanged>([&] { changed_session->verify_source(); });
  const std::vector<std::uint64_t> sentinel{12345, 67890};
  auto returned_values = sentinel;
  refuses<WorkflowInputChanged>(
      [&] { returned_values = changed_session->values(0, 6); });
  check(returned_values == sentinel,
        "Between-call source edit: stale read returned caller-visible data");
  changed_session.reset();
  check(bytes(fs::u8path(edited_input.u8string() + ".events")) ==
            "open\ndestroy\n",
        "Between-call source edit: changed input did not close handle once");
  no_stage(work);
  std::cout << "Typed ABI2 callback trust/file-vs-work/cancel/currentness "
               "controls pass\n";
}
} // namespace

int main(int argc, char **argv) {
  try {
    hstatus(H5PLset_loading_state(0),
            "disable development fixture plugin loading");
    hstatus(H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr),
            "own fixture error reporting");
    if (argc == 4 && std::string(argv[1]) == "validate") {
      const auto manifest = fs::absolute(fs::u8path(argv[2]));
      const auto work = fs::absolute(fs::u8path(argv[3]));
      fresh(work);
      configure_format_plugins(manifest.u8string());
      positive(manifest, work);
      negative(work);
      inspect_without_originals(work);
      std::cout
          << "Actual optional Loom native workflow integration tests pass\n";
      return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "inspect") {
      inspect(fs::absolute(fs::u8path(argv[2])));
      return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "unavailable") {
      const auto work = fs::absolute(fs::u8path(argv[2]));
      const auto explanation = refuses<std::invalid_argument>([&] {
        import(work / "counts.loom", work / "positions.tsv",
               work / "unavailable.qmx", interpretation());
      });
      check(explanation.find(provider) != std::string::npos &&
                !fs::exists(work / "unavailable.qmx"),
            "Missing-provider check did not reach the actual selected provider "
            "refusal");
      std::cout << explanation << '\n';
      return 0;
    }
    if (argc == 7 && std::string(argv[1]) == "legacy") {
      legacy(
          fs::absolute(fs::u8path(argv[2])), fs::absolute(fs::u8path(argv[3])),
          fs::absolute(fs::u8path(argv[4])), fs::absolute(fs::u8path(argv[5])),
          fs::absolute(fs::u8path(argv[6])));
      return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "control") {
      control(fs::absolute(fs::u8path(argv[2])),
              fs::absolute(fs::u8path(argv[3])));
      return 0;
    }
    throw std::invalid_argument(
        "Usage: validate ABS_MANIFEST NEW_WORK | inspect WORK | unavailable "
        "WORK | legacy ABS_MANIFEST COUNTS POSITIONS BASELINE NEW_QMX | "
        "control ABS_MANIFEST NEW_WORK");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
