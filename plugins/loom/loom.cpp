// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/hdf5_input.hpp"
#include "../format-common/json_tree.hpp"
#include "format_plugin_profile.hpp"
#include <qupathxx/format_plugin.h>

static_assert(H5_VERS_MAJOR == 2 && H5_VERS_MINOR == 2 && H5_VERS_RELEASE == 0,
              "Use the admitted HDF5 2.2.0 header profile");
static_assert(ZLIB_VERNUM == 0x1320,
              "Use the admitted paired prefixed zlib 1.3.2 headers");
#ifndef Z_PREFIX
#error Loom requires the frozen SDK's paired prefixed zlib headers
#endif

namespace {
using namespace qpx_binary;
namespace hdf = qpx_hdf5;
namespace json = qpx_native_json;
constexpr std::string_view provider_id = "org.qupathxx.loom";
constexpr std::string_view contract = "loom3-dense-typed/1";
constexpr std::size_t minimum_work = 16u * hdf::mib;
constexpr std::size_t maximum_work = 256u * hdf::mib;
constexpr std::uint64_t maximum_file = UINT64_C(16) * 1024 * 1024 * 1024;
constexpr std::uint64_t maximum_axis = 1000000;
constexpr std::size_t chunk_limit = hdf::mib;

struct Options {
  std::string matrix, features, observations, names, value_domain;
  std::uint32_t value_type = 0;
};
bool child_path(std::string_view value, std::string_view parent) {
  if (value.substr(0, parent.size()) != parent)
    return false;
  const auto leaf = value.substr(parent.size());
  return !leaf.empty() && leaf.size() <= 256 && leaf != "." && leaf != ".." &&
         leaf.find_first_of("/\\") == std::string_view::npos;
}
Options options(qpx_reader_string_v1 input, qpx_reader_cancel_v1 cancel) {
  require(input.data && input.size && input.size <= 8192,
          "Loom requires explicit bounded interpretation JSON");
  try {
    // Only the existing strict native JSON tree owns parsing. The complete
    // small option tree is charged inside the provider's fixed work reserve.
    Arena arena(64u * 1024);
    const std::string_view text(input.data, std::size_t(input.size));
    json::Tree tree(text, &arena.memory, cancel, 8, 16, 2);
    const auto &root = tree.root();
    tree.keys(root, {"matrix", "feature_ids", "observation_ids",
                     "feature_names", "domain", "value_domain"});
    Options result;
    result.matrix = json::string(tree.get(root, "matrix"));
    result.features = json::string(tree.get(root, "feature_ids"));
    result.observations = json::string(tree.get(root, "observation_ids"));
    result.value_domain = json::string(tree.get(root, "value_domain"));
    const auto domain = json::string(tree.get(root, "domain"));
    if (const auto *names = tree.find(root, "feature_names")) {
      result.names = json::string(*names);
      require(!result.names.empty(),
              "Loom feature_names must name a dataset when supplied");
    }
    for (const auto *value :
         {&result.matrix, &result.features, &result.observations, &result.names,
          &result.value_domain})
      utf8(*value, cancel);
    require(result.matrix == "/matrix" || child_path(result.matrix, "/layers/"),
            "Loom requires /matrix or one explicit /layers/NAME dataset");
    require(
        child_path(result.features, "/row_attrs/") &&
            child_path(result.observations, "/col_attrs/") &&
            (result.names.empty() || child_path(result.names, "/row_attrs/")),
        "Loom IDs and names require explicit row/column attribute datasets");
    require(!result.value_domain.empty() && result.value_domain.size() <= 128,
            "Loom value-domain label exceeds native limits");
    if (domain == "counts") {
      require(result.value_domain == "raw-counts",
              "Loom counts require raw-counts value domain");
      result.value_type = QPX_FORMAT_COUNTS_UINT64;
    } else {
      require(domain == "continuous" && result.value_domain != "raw-counts",
              "Loom requires explicit counts or continuous domain");
      result.value_type = QPX_FORMAT_CONTINUOUS_FLOAT64;
    }
    return result;
  } catch (const Failure &failure) {
    if (failure.status == QPX_READER_CANCELLED)
      throw;
    fail(failure.message, QPX_READER_INVALID_REQUEST);
  }
}

struct Data {
  // Member order ensures all HDF5 objects close before the source lease. Every
  // construction/use/destruction is inside this DLL's HDF5 instance gate.
  hdf::ReadLease lease;
  hdf::ChunkWorkspace scratch;
  hdf::Handle file;
  hdf::Dataset matrix, features, observations, names;
  Options selected;
  hdf::NumericKind numeric_kind = hdf::NumericKind::Unsigned;
  std::uint64_t feature_count = 0, observation_count = 0;

  Data(const std::filesystem::path &path, std::uint64_t file_bytes, Options p,
       qpx_reader_cancel_v1 cancel)
      : lease(path, file_bytes), file(hdf::open_file(path.u8string())),
        selected(std::move(p)) {
    lease.verify_file(file);
    poll(cancel);
    for (const auto *required :
         {"/attrs", "/row_attrs", "/col_attrs", "/row_graphs", "/col_graphs"}) {
      auto group = hdf::group(file, required);
      poll(cancel);
    }
    {
      hdf::Dataset version(file, "/attrs/LOOM_SPEC_VERSION", chunk_limit);
      valid(version.plan.dimensions.rank == 0,
            "Loom3 specification marker must be a scalar UTF-8 dataset");
      hdf::strings(
          version, lease, scratch, 0, 1, true, false,
          [](std::size_t, std::string_view text) {
            valid(text == "3.0.0",
                  "Loom specification marker must equal 3.0.0");
          },
          cancel);
    }
    {
      // The main dataset owns the Loom row/column frame even when a layer is
      // selected. Its unselected values/filters are never decoded or imported.
      auto main = hdf::dataset(file, "/matrix");
      const auto shape = hdf::shape(main);
      hdf::Handle type(H5Dget_type(main));
      (void)hdf::numeric_type(type);
      valid(shape.rank == 2 && shape.extent[0] && shape.extent[1] &&
                shape.extent[0] <= maximum_axis &&
                shape.extent[1] <= maximum_axis,
            "Loom matrix requires bounded nonempty feature x observation axes");
      feature_count = shape.extent[0];
      observation_count = shape.extent[1];
    }
    if (selected.matrix != "/matrix") {
      auto layers = hdf::group(file, "/layers");
    }
    matrix = hdf::Dataset(file, selected.matrix, chunk_limit);
    valid(matrix.plan.dimensions.rank == 2 &&
              matrix.plan.dimensions.extent[0] == feature_count &&
              matrix.plan.dimensions.extent[1] == observation_count,
          "Loom selected layer shape differs from main matrix frame");
    {
      hdf::Handle type(H5Dget_type(matrix));
      numeric_kind = hdf::numeric_type(type);
    }
    features = hdf::Dataset(file, selected.features, chunk_limit);
    observations = hdf::Dataset(file, selected.observations, chunk_limit);
    validate_axis(features, feature_count);
    validate_axis(observations, observation_count);
    if (!selected.names.empty()) {
      names = hdf::Dataset(file, selected.names, chunk_limit);
      validate_axis(names, feature_count);
    }
    poll(cancel);
  }
  static void validate_axis(hdf::Dataset &data, std::uint64_t count) {
    valid(data.plan.dimensions.rank == 1 &&
              data.plan.dimensions.extent[0] == count,
          "Loom selected ID/name axis differs from matrix frame");
    hdf::Handle type(H5Dget_type(data));
    hdf::utf8_string_type(type);
  }
};
Data &handle(void *value) {
  require(value != nullptr, "Loom handle is null");
  return *static_cast<Data *>(value);
}
std::uint64_t page(std::uint64_t first, std::uint64_t count,
                   std::uint64_t total, std::uint64_t limit) {
  require(first <= total && count > 0 && count <= limit,
          "Loom page is outside declared source range/bound");
  return std::min(count, total - first);
}
qpx_reader_status_v1 too_small(qpx_reader_text_v1 *error, const char *message) {
  if (error)
    text(*error, message);
  return QPX_READER_BUFFER_TOO_SMALL;
}

qpx_reader_status_v1 QPX_FORMAT_CALL
describe(qpx_format_info_v1 *output, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(output && output->struct_size >= sizeof(*output),
            "Loom descriptor structure is too small");
    output->abi_version = QPX_MATRIX_ABI_V2;
    output->domain = QPX_FORMAT_MATRIX;
    output->reserved = 0;
    bool complete = text(output->provider_id, provider_id);
    complete = text(output->format_id, "loom") && complete;
    complete =
        text(output->display_name, "Loom typed expression matrix") && complete;
    complete = text(output->version, "1.0.0") && complete;
    complete = text(output->contract, contract) && complete;
    complete = text(output->profile, QPX_FORMAT_PLUGIN_PROFILE_ID) && complete;
    return complete ? QPX_READER_OK
                    : too_small(error, "Loom descriptor buffers are too small");
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_matrix_open_v2 *request,
                                          void **output,
                                          qpx_reader_text_v1 *error) noexcept {
  if (output)
    *output = nullptr;
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(request && output && request->struct_size >= sizeof(*request) &&
                !request->reserved,
            "Loom open structure/output is invalid");
    require(request->work_bytes >= minimum_work &&
                request->work_bytes <= maximum_work && request->file_bytes &&
                request->file_bytes <= maximum_file,
            "Loom work/file allowances exceed admitted bounds");
    require(request->path.data && request->path.size &&
                request->path.size <= 32768,
            "Loom source path is absent or too long");
    const std::string_view source(request->path.data,
                                  std::size_t(request->path.size));
    utf8(source, request->cancel);
    const auto path = std::filesystem::u8path(source);
    require(path.is_absolute(), "Loom source path must be absolute");
    auto selected = options(request->options, request->cancel);
    auto gate = hdf::library_lock(request->cancel);
    hdf::Errors errors;
    auto result = std::make_unique<Data>(path, request->file_bytes,
                                         std::move(selected), request->cancel);
    poll(request->cancel);
    *output = result.release();
    return QPX_READER_OK;
  });
}
void QPX_FORMAT_CALL destroy(void *value) noexcept {
  if (!value)
    return;
  try {
    auto gate = hdf::library_lock({});
    hdf::Errors errors(false);
    delete static_cast<Data *>(value);
  } catch (...) {
    // Destruction cannot report or retain caller pointers. No HDF5 operation
    // normally throws here; closure return codes are deliberately infallible.
  }
}
qpx_reader_status_v1 QPX_FORMAT_CALL
metadata(void *value, qpx_matrix_metadata_v2 *output,
         qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &data = handle(value);
    require(output && output->struct_size >= sizeof(*output),
            "Loom metadata structure is too small");
    require(!output->value_domain.capacity || output->value_domain.data,
            "Loom metadata text pointer is null");
    poll(cancel);
    output->value_type = data.selected.value_type;
    output->features = data.feature_count;
    output->observations = data.observation_count;
    output->reserved[0] = output->reserved[1] = 0;
    if (!text(output->value_domain, data.selected.value_domain))
      return too_small(error, "Loom metadata value-domain buffer is too small");
    return QPX_READER_OK;
  });
}

struct TextPage {
  std::array<char, hdf::string_page * hdf::string_bytes> bytes{};
  std::array<std::uint32_t, hdf::string_page> offset{}, length{};
  std::size_t used = 0;
  void add(std::size_t index, std::string_view text) {
    require(index < offset.size() && text.size() <= bytes.size() - used,
            "Loom string page exceeds bounded workspace");
    offset[index] = std::uint32_t(used);
    length[index] = std::uint32_t(text.size());
    std::memcpy(bytes.data() + used, text.data(), text.size());
    used += text.size();
  }
  std::string_view at(std::size_t index) const {
    return {bytes.data() + offset[index], length[index]};
  }
};
qpx_reader_status_v1 QPX_FORMAT_CALL
axis(void *value, const qpx_matrix_axis_request_v1 *request,
     qpx_matrix_axis_buffer_v1 *output, qpx_reader_cancel_v1 cancel,
     qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &data = handle(value);
    require(output && output->struct_size >= sizeof(*output),
            "Loom axis output structure is too small");
    output->written = 0;
    output->text.size = 0;
    require(request && request->struct_size >= sizeof(*request) &&
                !output->reserved &&
                (request->axis == QPX_FORMAT_FEATURE_AXIS ||
                 request->axis == QPX_FORMAT_OBSERVATION_AXIS),
            "Loom axis request/reserved field is invalid");
    require((!output->capacity || output->rows) &&
                (!output->text.capacity || output->text.data),
            "Loom axis host buffer pointer is null");
    const bool feature = request->axis == QPX_FORMAT_FEATURE_AXIS;
    const auto count =
        page(request->first, request->count,
             feature ? data.feature_count : data.observation_count,
             hdf::string_page);
    auto gate = hdf::library_lock(cancel);
    hdf::Errors errors;
    auto &ids = feature ? data.features : data.observations;
    const bool named = feature && data.names.data.present();
    std::uint64_t bytes = 0;
    if (count) {
      auto measure = [&](std::size_t, std::string_view text) {
        bytes += text.size();
      };
      hdf::strings(ids, data.lease, data.scratch, request->first,
                   std::size_t(count), false, false, measure, cancel);
      if (named)
        hdf::strings(data.names, data.lease, data.scratch, request->first,
                     std::size_t(count), false, true, measure, cancel);
    }
    if (output->capacity < count || output->text.capacity < bytes) {
      output->written = count;
      output->text.size = bytes;
      return too_small(error, "Loom axis host buffers are too small");
    }
    if (count) {
      TextPage id_page, name_page;
      hdf::strings(
          ids, data.lease, data.scratch, request->first, std::size_t(count),
          false, false,
          [&](std::size_t index, std::string_view text) {
            id_page.add(index, text);
          },
          cancel);
      if (named)
        hdf::strings(
            data.names, data.lease, data.scratch, request->first,
            std::size_t(count), false, true,
            [&](std::size_t index, std::string_view text) {
              name_page.add(index, text);
            },
            cancel);
      std::uint64_t offset = 0;
      for (std::size_t index = 0; index < count; ++index) {
        poll(cancel);
        const auto id = id_page.at(index);
        const auto name = named ? name_page.at(index) : std::string_view{};
        output->rows[index] = {request->first + index, offset, id.size(),
                               offset + id.size(), name.size()};
        std::memcpy(output->text.data + offset, id.data(), id.size());
        offset += id.size();
        if (!name.empty())
          std::memcpy(output->text.data + offset, name.data(), name.size());
        offset += name.size();
      }
    }
    poll(cancel);
    output->written = count;
    output->text.size = bytes;
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL
values(void *value, const qpx_matrix_values_request_v1 *request,
       qpx_matrix_values_buffer_v2 *output, qpx_reader_cancel_v1 cancel,
       qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &data = handle(value);
    require(output && output->struct_size >= sizeof(*output),
            "Loom values output structure is too small");
    output->written = 0;
    require(request && request->struct_size >= sizeof(*request) &&
                !request->reserved && !output->reserved &&
                output->value_type == data.selected.value_type,
            "Loom values request/value kind/reserved field is invalid");
    const bool counts = data.selected.value_type == QPX_FORMAT_COUNTS_UINT64;
    require(
        counts ? (!output->continuous && (!output->capacity || output->counts))
               : (!output->counts && (!output->capacity || output->continuous)),
        "Loom values host pointers differ from selected domain");
    const auto count =
        page(request->first, request->count,
             data.feature_count * data.observation_count, hdf::max_page);
    poll(cancel);
    if (output->capacity < count) {
      output->written = count;
      return too_small(error, "Loom values host buffer is too small");
    }
    auto gate = hdf::library_lock(cancel);
    hdf::Errors errors;
    std::uint64_t done = 0;
    while (done < count) {
      poll(cancel);
      const auto linear = request->first + done;
      const auto observation = linear / data.feature_count;
      const auto first_feature = linear % data.feature_count;
      const auto take = std::min<std::uint64_t>(
          4096, std::min(count - done, data.feature_count - first_feature));
      hdf::numbers(
          data.matrix, data.lease, data.scratch, data.numeric_kind,
          first_feature, observation, std::size_t(take),
          [&](std::size_t index, const hdf::Number &number) {
            if (counts)
              output->counts[done + index] = hdf::count(number);
            else
              output->continuous[done + index] = hdf::continuous(number);
          },
          cancel);
      done += take;
    }
    poll(cancel);
    output->written = count;
    return QPX_READER_OK;
  });
}
} // namespace

extern "C" QPX_MATRIX_TYPED_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_matrix_format_entry_v2(std::uint32_t requested,
                           qpx_matrix_format_api_v2 *output,
                           qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(requested == QPX_MATRIX_ABI_V2 && output &&
                output->struct_size >= sizeof(*output),
            "Loom requires the separately negotiated typed matrix ABI2");
    *output = {sizeof(*output), QPX_MATRIX_ABI_V2, describe, open,
               destroy,         metadata,          axis,     values};
    return QPX_READER_OK;
  });
}
