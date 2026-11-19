// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/zarr_input.hpp"
#include "format_plugin_profile.hpp"
#include <chrono>
#include <mutex>

static_assert(ZLIB_VERNUM == 0x1320, "Use the admitted paired zlib1.3.2 headers");
#ifndef Z_PREFIX
#error AnnData Zarr needs the frozen prefixed zlib interface
#endif
static_assert(sizeof(qpx_matrix_open_v3) == 96, "Directory matrix ABI3 layout differs");
static_assert(sizeof(qpx_matrix_format_api_v3) == 56, "Directory matrix ABI3 table differs");

namespace {
using namespace qpx_binary;
namespace zarr = qpx_zarr;
namespace json = qpx_native_json;
constexpr std::string_view provider_id = "org.qupathxx.anndata-zarr";
constexpr std::string_view contract = "anndata-zarr2-typed/1";
constexpr std::uint64_t maximum_file = UINT64_C(16) * 1024 * 1024 * 1024;
constexpr std::uint64_t maximum_axis = 1000000;
constexpr std::uint64_t maximum_logical = 100000000;
constexpr std::size_t minimum_work = 16u * zarr::mib;
constexpr std::size_t maximum_work = 256u * zarr::mib;
// Nonoverlapping phases reserve: two metadata bytes/4MiB JSON arena pairs
// for selected consolidated agreement; or at most
// three1MiB caches, a replacement1MiB chunk,1MiB+64KiB encoded input,512KiB
// explicitly bounded zlib allocations or at most ~3MiB single-threaded Blosc
// scratch, paths/scalars/sort stack. Before axis
// calls numeric caches are released; before value calls string caches release.
// Text is measured first and then written directly to host buffers; no hidden
// full catalog/name page or full dense matrix is retained by this module.
constexpr std::size_t fixed_workspace = 12u * zarr::mib;

bool child(std::string_view path, std::string_view parent) {
  if (path.substr(0, parent.size()) != parent) return false;
  const auto leaf = path.substr(parent.size());
  return !leaf.empty() && leaf.size() <= 256 && leaf != "." && leaf != ".." &&
      leaf.find_first_of("/\\:") == std::string_view::npos;
}
struct Options {
  std::string matrix, observations, features, names, value_domain;
  std::uint32_t value_type = 0;
};
Options options(qpx_reader_string_v1 input, qpx_reader_cancel_v1 cancel) {
  require(input.data && input.size && input.size <= 8192, "AnnData Zarr requires explicit bounded interpretation JSON");
  try {
    Arena arena(64u * 1024);
    json::Tree tree({input.data, std::size_t(input.size)}, &arena.memory, cancel, 8, 16, 2);
    const auto &root = tree.root();
    tree.keys(root, {"matrix", "observation_ids", "feature_ids", "feature_names", "domain", "value_domain"});
    Options result;
    result.matrix = json::string(tree.get(root, "matrix"));
    result.observations = json::string(tree.get(root, "observation_ids"));
    result.features = json::string(tree.get(root, "feature_ids"));
    result.value_domain = json::string(tree.get(root, "value_domain"));
    if (const auto *names = tree.find(root, "feature_names")) {
      result.names = json::string(*names);
      require(!result.names.empty(), "AnnData feature_names must explicitly select a column");
    }
    for (const auto *value : {&result.matrix, &result.observations, &result.features, &result.names, &result.value_domain}) utf8(*value, cancel);
    require(result.matrix == "X" || child(result.matrix, "layers/"), "AnnData matrix must explicitly select X or layers/NAME");
    require(child(result.observations, "obs/") && child(result.features, "var/") &&
                (result.names.empty() || child(result.names, "var/")), "AnnData ID/name paths must explicitly select obs/var columns");
    require(!result.value_domain.empty() && result.value_domain.size() <= 128, "AnnData value-domain label exceeds native bounds");
    const auto domain = json::string(tree.get(root, "domain"));
    if (domain == "counts") {
      require(result.value_domain == "raw-counts", "Count matrices require raw-counts value domain");
      result.value_type = QPX_FORMAT_COUNTS_UINT64;
    } else {
      require(domain == "continuous" && result.value_domain != "raw-counts", "AnnData requires explicit counts/continuous domain");
      result.value_type = QPX_FORMAT_CONTINUOUS_FLOAT64;
    }
    return result;
  } catch (const Failure &failure) {
    if (failure.status == QPX_READER_CANCELLED) throw;
    fail(failure.message, QPX_READER_INVALID_REQUEST);
  }
}

void group(zarr::Store &store, const std::string &prefix, qpx_reader_cancel_v1 cancel) {
  store.metadata(prefix.empty() ? ".zgroup" : prefix + "/.zgroup", cancel,
      [&](const json::Tree &tree, const json::Node &root) {
        tree.keys(root, {"zarr_format"});
        valid(json::integer(tree.get(root, "zarr_format")) == 2, "Requires a Zarr2 group");
        return 0;
      });
}
void encoding(zarr::Store &store, const std::string &prefix, std::string_view type,
              std::string_view version, qpx_reader_cancel_v1 cancel) {
  store.metadata(prefix.empty() ? ".zattrs" : prefix + "/.zattrs", cancel,
      [&](const json::Tree &tree, const json::Node &root) {
        valid(json::string(tree.get(root, "encoding-type")) == type &&
                  json::string(tree.get(root, "encoding-version")) == version,
              "Unsupported AnnData element encoding/version");
        return 0;
      });
}
void dataframe(zarr::Store &store, const std::string &name, const std::string &selected,
               const std::string &display, qpx_reader_cancel_v1 cancel) {
  group(store, name, cancel);
  store.metadata(name + "/.zattrs", cancel, [&](const json::Tree &tree, const json::Node &root) {
    valid(json::string(tree.get(root, "encoding-type")) == "dataframe" &&
              json::string(tree.get(root, "encoding-version")) == "0.2.0", "Requires dataframe0.2.0 catalog");
    const auto index = json::string(tree.get(root, "_index"));
    valid(child(name + "/" + std::string(index), name + "/") && selected == name + "/" + std::string(index),
          "Explicit catalog path differs from encoded dataframe index");
    bool named = display.empty() || display == selected;
    const auto &columns = tree.array(tree.get(root, "column-order"));
    for (std::size_t position = 0; position < columns.size(); ++position) {
      poll(cancel); const auto column = json::string(tree.nodes[columns[position]]);
      valid(child(name + "/" + std::string(column), name + "/"), "Dataframe column key is unsupported");
      for (std::size_t earlier = 0; earlier < position; ++earlier)
        valid(column != json::string(tree.nodes[columns[earlier]]), "Duplicate dataframe column-order key");
      if (!display.empty() && display == name + "/" + std::string(column)) named = true;
    }
    valid(named, "Explicit display-name column is absent from dataframe column-order");
    return 0;
  });
}

using Entry = qpx_matrix_sparse_entry_v1;
static_assert(sizeof(Entry) == 16, "Sparse index reservation differs");
struct Data {
  zarr::Store store;
  Options selected;
  std::unique_ptr<zarr::Array> dense, data, indices, indptr, features, observations, names;
  std::unique_ptr<Entry[]> sparse;
  std::uint64_t feature_count = 0, observation_count = 0, sparse_count = 0;
  std::timed_mutex mutex;

  Data(const std::filesystem::path &path, std::uint64_t file_bytes, std::size_t work,
       Options chosen, qpx_reader_cancel_v1 cancel)
      : store(path, file_bytes, cancel), selected(std::move(chosen)) {
    group(store, "", cancel); encoding(store, "", "anndata", "0.1.0", cancel);
    dataframe(store, "obs", selected.observations, "", cancel);
    dataframe(store, "var", selected.features, selected.names, cancel);
    observations = string_array(selected.observations, cancel);
    features = string_array(selected.features, cancel);
    observation_count = observations->shape[0]; feature_count = features->shape[0];
    valid(feature_count && observation_count && feature_count <= maximum_axis && observation_count <= maximum_axis,
          "AnnData matrix exceeds selected axis limits");
    if (!selected.names.empty()) {
      names = string_array(selected.names, cancel);
      valid(names->shape[0] == feature_count, "Display-name column differs from feature catalog");
    }
    if (selected.matrix != "X") { group(store, "layers", cancel); encoding(store, "layers", "dict", "0.1.0", cancel); }
    std::string kind;
    std::array<std::uint64_t, 2> shape{};
    store.metadata(selected.matrix + "/.zattrs", cancel, [&](const json::Tree &tree, const json::Node &root) {
      kind = json::string(tree.get(root, "encoding-type"));
      const auto version = json::string(tree.get(root, "encoding-version"));
      if (kind == "array") valid(version == "0.2.0", "Requires dense array0.2.0");
      else {
        valid((kind == "csr_matrix" || kind == "csc_matrix") && version == "0.1.0", "Unsupported matrix encoding/version");
        const auto &dims = tree.array(tree.get(root, "shape"));
        valid(dims.size() == 2, "Sparse AnnData shape must have two dimensions");
        shape = {json::integer(tree.nodes[dims[0]]), json::integer(tree.nodes[dims[1]])};
      }
      return 0;
    });
    const bool counts = selected.value_type == QPX_FORMAT_COUNTS_UINT64;
    if (kind == "array") {
      valid(feature_count <= maximum_logical / observation_count, "AnnData dense matrix exceeds logical-cell limit");
      valid(!store.exists(selected.matrix + "/.zgroup", cancel), "Dense matrix cannot also be a group");
      dense = std::make_unique<zarr::Array>(store, selected.matrix, cancel);
      valid(dense->rank == 2 && dense->shape[0] == observation_count && dense->shape[1] == feature_count,
            "Dense matrix shape differs from obs/var catalogs");
      dense->validate_domain(counts);
    } else {
      group(store, selected.matrix, cancel);
      valid(!store.exists(selected.matrix + "/.zarray", cancel) && shape[0] == observation_count && shape[1] == feature_count,
            "Sparse matrix shape/storage differs from catalogs");
      data = numeric_vector(selected.matrix + "/data", cancel);
      indices = numeric_vector(selected.matrix + "/indices", cancel);
      indptr = numeric_vector(selected.matrix + "/indptr", cancel);
      data->validate_domain(counts); indices->validate_index_fill(); indptr->validate_index_fill();
      sparse_count = data->shape[0];
      const auto major = kind == "csr_matrix" ? observation_count : feature_count;
      const auto minor = kind == "csr_matrix" ? feature_count : observation_count;
      valid(indices->shape[0] == sparse_count && indptr->shape[0] == major + 1 && sparse_count <= feature_count * observation_count,
            "Sparse data/indices/indptr lengths differ");
      require(sparse_count <= (work - fixed_workspace) / sizeof(Entry), "Sparse coordinate index exceeds provider work allowance");
      if (sparse_count) sparse.reset(new Entry[std::size_t(sparse_count)]);
      std::uint64_t written = 0;
      valid(zarr::index(indptr->number(0, cancel)) == 0, "Sparse indptr must start at zero");
      for (std::uint64_t column = 0; column < major; ++column) {
        poll(cancel);
        const auto end = zarr::index(indptr->number(column + 1, cancel));
        valid(end >= written && end <= sparse_count, "Sparse indptr is decreasing or outside data");
        for (; written < end; ++written) {
          if (!(written & 1023)) poll(cancel);
          const auto at = zarr::index(indices->number(written, cancel));
          valid(at < minor, "Sparse coordinate index is outside matrix shape");
          auto &entry = sparse[std::size_t(written)];
          entry.linear = kind == "csr_matrix" ? column * feature_count + at : at * feature_count + column;
          const auto number = data->number(written, cancel);
          if (counts) entry.value_bits = zarr::count(number);
          else { const auto real = zarr::continuous(number); std::memcpy(&entry.value_bits, &real, sizeof(real)); }
        }
      }
      valid(written == sparse_count, "Sparse indptr does not end at data length");
      data->clear_cache(); indices->clear_cache(); indptr->clear_cache();
      if (sparse_count > 1) {
        std::size_t comparisons = 0;
        std::sort(sparse.get(), sparse.get() + std::size_t(sparse_count), [&](const Entry &a, const Entry &b) {
          if (!(comparisons++ & 4095)) poll(cancel);
          return a.linear < b.linear;
        });
      }
      for (std::uint64_t at = 1; at < sparse_count; ++at) {
        if (!(at & 4095)) poll(cancel);
        valid(sparse[std::size_t(at - 1)].linear != sparse[std::size_t(at)].linear, "Duplicate sparse coordinates are not summed");
      }
    }
    poll(cancel);
  }
  std::unique_ptr<zarr::Array> string_array(const std::string &path, qpx_reader_cancel_v1 cancel) {
    encoding(store, path, "string-array", "0.2.0", cancel);
    auto result = std::make_unique<zarr::Array>(store, path, cancel);
    valid(result->kind == zarr::Kind::Utf8 && result->rank == 1, "Catalog requires a VLenUTF8 string-array"); return result;
  }
  std::unique_ptr<zarr::Array> numeric_vector(const std::string &path, qpx_reader_cancel_v1 cancel) {
    // AnnData's sparse writer creates these internal vector arrays directly;
    // their standalone encoding attrs are optional, unlike X and catalogs.
    if (store.exists(path + "/.zattrs", cancel)) encoding(store, path, "array", "0.2.0", cancel);
    auto result = std::make_unique<zarr::Array>(store, path, cancel);
    valid(result->kind != zarr::Kind::Utf8 && result->rank == 1, "Sparse storage requires numeric vectors"); return result;
  }
  void clear_strings() { features->clear_cache(); observations->clear_cache(); if (names) names->clear_cache(); }
  void clear_numbers() {
    if (dense) dense->clear_cache();
    if (data) data->clear_cache(); if (indices) indices->clear_cache(); if (indptr) indptr->clear_cache();
  }
};

Data &handle(void *value) { require(value != nullptr, "AnnData Zarr handle is null"); return *static_cast<Data *>(value); }
std::unique_lock<std::timed_mutex> lock(Data &data, qpx_reader_cancel_v1 cancel) {
  std::unique_lock<std::timed_mutex> result(data.mutex, std::defer_lock);
  while (!result.try_lock_for(std::chrono::milliseconds(10))) poll(cancel);
  poll(cancel); return result;
}
std::uint64_t page(std::uint64_t first, std::uint64_t count, std::uint64_t total, std::uint64_t maximum) {
  require(first <= total && count && count <= maximum, "AnnData page exceeds source range/bounded allowance");
  return std::min(count, total - first);
}
qpx_reader_status_v1 too_small(qpx_reader_text_v1 *error, const char *message) {
  if (error) text(*error, message); return QPX_READER_BUFFER_TOO_SMALL;
}
qpx_reader_status_v1 QPX_FORMAT_CALL describe(qpx_format_info_v1 *output, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(output && output->struct_size >= sizeof(*output), "AnnData descriptor structure is too small");
    output->abi_version = QPX_MATRIX_ABI_V4; output->domain = QPX_FORMAT_MATRIX; output->reserved = 0;
    const std::array<std::pair<qpx_reader_text_v1 *, std::string_view>, 6> fields{{
        {&output->provider_id, provider_id}, {&output->format_id, "anndata-zarr"},
        {&output->display_name, "AnnData Zarr typed expression matrix"},
        {&output->version, QPX_FORMAT_PLUGIN_PROVIDER_VERSION},
        {&output->contract, contract}, {&output->profile, QPX_FORMAT_PLUGIN_PROFILE_ID}}};
    bool complete = true;
    for (const auto &field : fields) {
      require(!field.first->capacity || field.first->data, "AnnData descriptor text pointer is null");
      field.first->size = field.second.size();
      complete = field.first->capacity >= field.second.size() && complete;
    }
    if (!complete) return too_small(error, "AnnData descriptor buffers are too small");
    for (const auto &field : fields) (void)text(*field.first, field.second);
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_matrix_open_v3 *request, void **output,
                                         qpx_reader_text_v1 *error) noexcept {
  if (output) *output = nullptr;
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(request && output && request->struct_size >= sizeof(*request) && !request->reserved,
            "AnnData store open structure/output is invalid");
    require(request->work_bytes >= minimum_work && request->work_bytes <= maximum_work &&
                request->file_bytes && request->file_bytes <= maximum_file &&
                request->total_bytes && request->total_bytes <= maximum_file,
            "AnnData store work/file/total limits exceed admitted bounds");
    require(request->path.data && request->path.size && request->path.size <= 4096 &&
                request->primary_key.data && request->primary_key.size == 7,
            "AnnData store path/primary metadata key is absent or too long");
    const std::string_view path(request->path.data, std::size_t(request->path.size));
    const std::string_view primary(request->primary_key.data, std::size_t(request->primary_key.size));
    require(primary == ".zattrs", "AnnData primary metadata must explicitly select .zattrs");
    utf8(path, request->cancel);
    auto selected = options(request->options, request->cancel);
    auto result = std::make_unique<Data>(std::filesystem::u8path(path), std::min(request->file_bytes, request->total_bytes),
                                        std::size_t(request->work_bytes), std::move(selected), request->cancel);
    poll(request->cancel); *output = result.release(); return QPX_READER_OK;
  });
}
void QPX_FORMAT_CALL destroy(void *value) noexcept { delete static_cast<Data *>(value); }
qpx_reader_status_v1 QPX_FORMAT_CALL metadata(void *value, qpx_matrix_metadata_v2 *output,
                                             qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &data = handle(value); auto guard = lock(data, cancel);
    require(output && output->struct_size >= sizeof(*output) && (!output->value_domain.capacity || output->value_domain.data),
            "AnnData metadata structure/text buffer is invalid");
    output->value_type = data.selected.value_type; output->features = data.feature_count; output->observations = data.observation_count;
    output->reserved[0] = output->reserved[1] = 0;
    return text(output->value_domain, data.selected.value_domain) ? QPX_READER_OK : too_small(error, "AnnData value-domain buffer is too small");
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL axis(void *value, const qpx_matrix_axis_request_v1 *request,
                                        qpx_matrix_axis_buffer_v1 *output, qpx_reader_cancel_v1 cancel,
                                        qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &data = handle(value); auto guard = lock(data, cancel);
    require(output && output->struct_size >= sizeof(*output), "AnnData axis output structure is too small");
    output->written = 0; output->text.size = 0;
    require(request && request->struct_size >= sizeof(*request) && !output->reserved &&
                (request->axis == QPX_FORMAT_FEATURE_AXIS || request->axis == QPX_FORMAT_OBSERVATION_AXIS) &&
                (!output->capacity || output->rows) && (!output->text.capacity || output->text.data),
            "AnnData axis request/buffer/reserved field is invalid");
    const bool feature = request->axis == QPX_FORMAT_FEATURE_AXIS;
    const auto count = page(request->first, request->count, feature ? data.feature_count : data.observation_count, 256);
    auto &ids = feature ? *data.features : *data.observations;
    const bool named = feature && data.names;
    data.clear_numbers();
    std::uint64_t bytes = 0;
    ids.strings(request->first, std::size_t(count), 256, false,
                [&](std::size_t, std::string_view text) { bytes += text.size(); }, cancel);
    if (named) data.names->strings(request->first, std::size_t(count), 4096, true,
                                   [&](std::size_t, std::string_view text) { bytes += text.size(); }, cancel);
    if (output->capacity < count || output->text.capacity < bytes) {
      output->written = count; output->text.size = bytes; return too_small(error, "AnnData axis host buffers are too small");
    }
    std::uint64_t offset = 0;
    ids.strings(request->first, std::size_t(count), 256, false, [&](std::size_t index, std::string_view text) {
      output->rows[index] = {request->first + index, offset, text.size(), 0, 0};
      if (!text.empty()) std::memcpy(output->text.data + offset, text.data(), text.size()); offset += text.size();
    }, cancel);
    if (named) data.names->strings(request->first, std::size_t(count), 4096, true, [&](std::size_t index, std::string_view text) {
      output->rows[index].name_offset = offset; output->rows[index].name_size = text.size();
      if (!text.empty()) std::memcpy(output->text.data + offset, text.data(), text.size()); offset += text.size();
    }, cancel);
    poll(cancel); output->written = count; output->text.size = bytes; return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL values(void *value, const qpx_matrix_values_request_v1 *request,
                                          qpx_matrix_values_buffer_v2 *output, qpx_reader_cancel_v1 cancel,
                                          qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &data = handle(value); auto guard = lock(data, cancel);
    require(output && output->struct_size >= sizeof(*output), "AnnData values output structure is too small");
    output->written = 0;
    require(request && request->struct_size >= sizeof(*request) && !request->reserved && !output->reserved &&
                output->value_type == data.selected.value_type, "AnnData values request/domain/reserved field is invalid");
    const bool counts = data.selected.value_type == QPX_FORMAT_COUNTS_UINT64;
    require(counts ? (!output->continuous && (!output->capacity || output->counts))
                   : (!output->counts && (!output->capacity || output->continuous)), "AnnData value pointers differ from selected domain");
    const auto count = page(request->first, request->count, data.feature_count * data.observation_count, 4096);
    if (output->capacity < count) { output->written = count; return too_small(error, "AnnData value host buffer is too small"); }
    data.clear_strings();
    if (data.dense) {
      for (std::uint64_t at = 0; at < count; ++at) {
        if (!(at & 255)) poll(cancel);
        const auto linear = request->first + at;
        const auto number = data.dense->number(linear / data.feature_count, linear % data.feature_count, cancel);
        if (counts) output->counts[at] = zarr::count(number); else output->continuous[at] = zarr::continuous(number);
      }
    } else {
      if (count) {
        if (counts) std::fill_n(output->counts, std::size_t(count), std::uint64_t(0));
        else std::fill_n(output->continuous, std::size_t(count), 0.);
      }
      if (data.sparse_count) {
        const auto begin = data.sparse.get(), end = begin + std::size_t(data.sparse_count);
        auto entry = std::lower_bound(begin, end, request->first, [](const Entry &entry, std::uint64_t first) { return entry.linear < first; });
        for (; entry != end && entry->linear < request->first + count; ++entry) {
          poll(cancel); const auto at = entry->linear - request->first;
          if (counts) output->counts[at] = entry->value_bits;
          else std::memcpy(output->continuous + at, &entry->value_bits, sizeof(double));
        }
      }
    }
    poll(cancel); output->written = count; return QPX_READER_OK;
  });
}

qpx_reader_status_v1 QPX_FORMAT_CALL sparse_metadata(void *value, qpx_matrix_sparse_metadata_v1 *output,
    qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    const auto &data = handle(value);
    poll(cancel);
    require(output && output->struct_size >= sizeof(*output) && !output->reserved,
            "AnnData sparse metadata structure is invalid");
    output->sparse = data.dense ? 0 : 1;
    output->entries = data.sparse_count;
    return QPX_READER_OK;
  });
}

qpx_reader_status_v1 QPX_FORMAT_CALL sparse_values(void *value, const qpx_matrix_values_request_v1 *request,
    qpx_matrix_sparse_buffer_v1 *output, qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    const auto &data = handle(value);
    require(output && output->struct_size >= sizeof(*output), "AnnData sparse output structure is invalid");
    output->written = 0;
    require(!data.dense && request && request->struct_size >= sizeof(*request) && !request->reserved &&
                !output->reserved && (!output->capacity || output->entries), "AnnData sparse page request is invalid");
    const auto count = page(request->first, request->count, data.sparse_count, 4096);
    if (output->capacity < count) {
      output->written = count;
      return too_small(error, "AnnData sparse host buffer is too small");
    }
    for (std::uint64_t i = 0; i < count; ++i) {
      if (!(i & 255)) poll(cancel);
      output->entries[i] = data.sparse[std::size_t(request->first + i)];
    }
    poll(cancel); output->written = count; return QPX_READER_OK;
  });
}
} // namespace

extern "C" QPX_MATRIX_SPARSE_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_matrix_format_entry_v4(std::uint32_t requested, qpx_matrix_format_api_v4 *output,
                           qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(requested == QPX_MATRIX_ABI_V4 && output && output->struct_size >= sizeof(*output),
            "AnnData Zarr requires the separately negotiated sparse matrix ABI4");
    *output = {sizeof(*output), QPX_MATRIX_ABI_V4,
        {sizeof(qpx_matrix_format_api_v3), QPX_MATRIX_ABI_V3, describe, open, destroy, metadata, axis, values},
        sparse_metadata, sparse_values};
    return QPX_READER_OK;
  });
}
