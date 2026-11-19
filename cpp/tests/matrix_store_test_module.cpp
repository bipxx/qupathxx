// SPDX-License-Identifier: AGPL-3.0-only
// Repository-authored ABI3 directory-store boundary provider. Never included in a product.
#include "format_plugin_profile.hpp"
#include "qupathxx/format_plugin.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <thread>

namespace fs = std::filesystem;
namespace {
struct Context {
  std::string mode, path;
  bool continuous = false;
};
qpx_reader_status_v1 error(qpx_reader_text_v1 *out, qpx_reader_status_v1 code,
                           const std::string &message) {
  if (out) {
    out->size = message.size();
    if (out->data && out->capacity >= message.size())
      std::memcpy(out->data, message.data(), message.size());
  }
  return code;
}
template <class F>
qpx_reader_status_v1 boundary(qpx_reader_text_v1 *out, F &&function) noexcept {
  try {
    return function();
  } catch (const std::bad_alloc &) {
    return error(out, QPX_READER_INTERNAL_ERROR,
                 "Controlled matrix allocation failure");
  } catch (...) {
    return error(out, QPX_READER_INTERNAL_ERROR,
                 "Controlled matrix exception contained");
  }
}
bool cancelled(qpx_reader_cancel_v1 value) {
  return value.is_cancelled && value.is_cancelled(value.context);
}
void event(const Context &context, const char *value) {
  std::ofstream out(fs::u8path(context.path + ".events"),
                    std::ios::binary | std::ios::app);
  out << value << '\n';
}
qpx_reader_status_v1 QPX_FORMAT_CALL
describe(qpx_format_info_v1 *out, qpx_reader_text_v1 *err) noexcept {
  return boundary(err, [&] {
    if (!out || out->struct_size < sizeof(*out) || out->reserved)
      return error(err, QPX_READER_INVALID_REQUEST,
                   "Invalid controlled descriptor");
    const std::array<std::string, 6> values{"org.qupathxx.test.matrix-store",
                                            "store-test",
                                            "Matrix directory-store boundary control",
                                            "1.0.0",
                                            "test-matrix-store/1",
                                            QPX_FORMAT_PLUGIN_PROFILE_ID};
    std::array<qpx_reader_text_v1 *, 6> fields{
        &out->provider_id, &out->format_id, &out->display_name,
        &out->version,     &out->contract,  &out->profile};
    bool fits = true;
    for (std::size_t i = 0; i < fields.size(); ++i) {
      fields[i]->size = values[i].size();
      fits = fits && fields[i]->data && fields[i]->capacity >= values[i].size();
    }
    out->abi_version = QPX_MATRIX_ABI_V3;
    out->domain = QPX_FORMAT_MATRIX;
    if (!fits)
      return error(err, QPX_READER_BUFFER_TOO_SMALL,
                   "Controlled descriptor capacity");
    for (std::size_t i = 0; i < fields.size(); ++i)
      std::memcpy(fields[i]->data, values[i].data(), values[i].size());
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_matrix_open_v3 *request,
                                          void **handle,
                                          qpx_reader_text_v1 *err) noexcept {
  if (handle)
    *handle = nullptr;
  return boundary(err, [&] {
    if (!request || !handle || request->struct_size < sizeof(*request) ||
        request->reserved || !request->path.data || !request->path.size ||
        !request->primary_key.data || !request->primary_key.size ||
        !request->work_bytes || !request->file_bytes || !request->total_bytes ||
        request->file_bytes > request->total_bytes ||
        request->work_bytes > 256ull * 1024 * 1024)
      return error(err, QPX_READER_INVALID_REQUEST, "Invalid controlled open");
    if (cancelled(request->cancel))
      return error(err, QPX_READER_CANCELLED, "Controlled open cancelled");
    auto context = std::make_unique<Context>();
    context->path.assign(request->path.data, std::size_t(request->path.size));
    const std::string primary(request->primary_key.data, std::size_t(request->primary_key.size));
    if (primary != ".zattrs")
      return error(err, QPX_READER_INVALID_REQUEST, "Controlled primary selection differs");
    std::ifstream input(fs::u8path(context->path) / fs::u8path(primary), std::ios::binary);
    if (!input)
      return error(err, QPX_READER_IO_ERROR, "Controlled source unavailable");
    std::getline(input, context->mode);
    context->continuous =
        context->mode.find("continuous") != std::string::npos ||
        context->mode == "values-nan" || context->mode == "values-inf";
    event(*context, "open");
    if (context->mode == "open-request")
      ++const_cast<qpx_matrix_open_v3*>(request)->total_bytes;
    const auto fail = context->mode == "open-error-handle";
    *handle = context.release();
    return fail ? error(err, QPX_READER_IO_ERROR,
                        "Controlled open error with owned handle")
                : QPX_READER_OK;
  });
}
void QPX_FORMAT_CALL destroy(void *raw) noexcept {
  try {
    if (raw) {
      std::unique_ptr<Context> context(static_cast<Context *>(raw));
      event(*context, "destroy");
    }
  } catch (...) {
  }
}
qpx_reader_status_v1 QPX_FORMAT_CALL
metadata(void *raw, qpx_matrix_metadata_v2 *out, qpx_reader_cancel_v1 cancel,
         qpx_reader_text_v1 *err) noexcept {
  return boundary(err, [&] {
    if (!raw || !out || out->struct_size < sizeof(*out))
      return error(err, QPX_READER_INVALID_REQUEST,
                   "Invalid controlled metadata");
    if (cancelled(cancel))
      return error(err, QPX_READER_CANCELLED, "Controlled metadata cancelled");
    const auto &context = *static_cast<Context *>(raw);
    const auto &mode = context.mode;
    out->value_type = context.continuous ? QPX_FORMAT_CONTINUOUS_FLOAT64
                                         : QPX_FORMAT_COUNTS_UINT64;
    out->features = 2;
    out->observations = 3;
    std::string label =
        context.continuous ? "literal-continuous" : "raw-counts";
    if (mode == "metadata-count-label")
      label = "invented-count-label";
    if (mode == "metadata-continuous-label")
      label = "raw-counts";
    if (mode == "metadata-empty-label")
      label.clear();
    if (mode == "metadata-utf8-label")
      label = std::string("\xc0\xaf", 2);
    if (mode == "metadata-long-label")
      label.assign(129, 'x');
    out->value_domain.size = label.size();
    if (out->value_domain.capacity < label.size() ||
        (!label.empty() && !out->value_domain.data))
      return error(err, QPX_READER_BUFFER_TOO_SMALL,
                   "Controlled metadata text capacity");
    if (!label.empty())
      std::memcpy(out->value_domain.data, label.data(), label.size());
    if (mode == "metadata-type")
      out->value_type = 99;
    if (mode == "metadata-zero")
      out->features = 0;
    if (mode == "metadata-huge")
      out->features = UINT64_MAX;
    if (mode == "metadata-reserved")
      out->reserved[0] = 1;
    if (mode == "metadata-size")
      out->struct_size = 4;
    if (mode == "metadata-capacity")
      ++out->value_domain.capacity;
    if (mode == "metadata-pointer")
      out->value_domain.data = nullptr;
    if (mode == "metadata-length")
      out->value_domain.size = out->value_domain.capacity + 1;
    if (mode == "metadata-io")
      return error(err, QPX_READER_IO_ERROR, "Controlled metadata failure");
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL
axis(void *raw, const qpx_matrix_axis_request_v1 *request,
     qpx_matrix_axis_buffer_v1 *out, qpx_reader_cancel_v1 cancel,
     qpx_reader_text_v1 *err) noexcept {
  return boundary(err, [&] {
    if (!raw || !request || !out || request->struct_size < sizeof(*request) ||
        out->struct_size < sizeof(*out) || out->reserved || !request->count ||
        (request->axis != QPX_FORMAT_FEATURE_AXIS &&
         request->axis != QPX_FORMAT_OBSERVATION_AXIS))
      return error(err, QPX_READER_INVALID_REQUEST, "Invalid controlled axis");
    if (cancelled(cancel))
      return error(err, QPX_READER_CANCELLED, "Controlled axis cancelled");
    const auto &mode = static_cast<Context *>(raw)->mode;
    const std::array<std::string, 3> observations{"cell-a", "cell-b", "cell-c"};
    const std::array<std::string, 2> features{"feature-a", "feature-b"};
    const bool is_feature = request->axis == QPX_FORMAT_FEATURE_AXIS;
    const auto total = is_feature ? features.size() : observations.size();
    if (request->first > total)
      return error(err, QPX_READER_INVALID_REQUEST,
                   "Controlled axis outside range");
    const auto n =
        std::min<std::uint64_t>(request->count, total - request->first);
    std::uint64_t bytes = 0;
    for (std::uint64_t i = 0; i < n; ++i)
      bytes += (is_feature ? features[std::size_t(request->first + i)]
                           : observations[std::size_t(request->first + i)])
                   .size();
    out->written = n;
    out->text.size = bytes;
    if (out->capacity < n || out->text.capacity < bytes || (n && !out->rows) ||
        (bytes && !out->text.data))
      return error(err, QPX_READER_BUFFER_TOO_SMALL,
                   "Controlled axis capacity");
    std::uint64_t at = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
      const auto &id = is_feature
                           ? features[std::size_t(request->first + i)]
                           : observations[std::size_t(request->first + i)];
      out->rows[i] = {request->first + i, at, id.size(), at + id.size(), 0};
      std::memcpy(out->text.data + at, id.data(), id.size());
      at += id.size();
    }
    if (mode == "axis-short" && n)
      --out->written;
    if (mode == "axis-long")
      ++out->written;
    if (mode == "axis-capacity")
      ++out->capacity;
    if (mode == "axis-pointer")
      out->rows = nullptr;
    if (n && mode == "axis-index")
      out->rows[0].index = 99;
    if (n && mode == "axis-offset")
      out->rows[0].id_offset = UINT64_MAX;
    if (n > 1 && mode == "axis-duplicate") {
      out->rows[1].id_offset = out->rows[0].id_offset;
      out->rows[1].id_size = out->rows[0].id_size;
    }
    if (n && !is_feature && mode == "axis-observation-name") {
      out->rows[0].name_offset = out->rows[0].id_offset;
      out->rows[0].name_size = out->rows[0].id_size;
    }
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL
values(void *raw, const qpx_matrix_values_request_v1 *request,
       qpx_matrix_values_buffer_v2 *out, qpx_reader_cancel_v1 cancel,
       qpx_reader_text_v1 *err) noexcept {
  return boundary(err, [&] {
    if (!raw || !request || !out || request->struct_size < sizeof(*request) ||
        out->struct_size < sizeof(*out) || request->reserved || out->reserved ||
        !request->count || request->first > 6)
      return error(err, QPX_READER_INVALID_REQUEST,
                   "Invalid controlled values");
    auto &context = *static_cast<Context *>(raw);
    const auto expected = context.continuous ? QPX_FORMAT_CONTINUOUS_FLOAT64
                                             : QPX_FORMAT_COUNTS_UINT64;
    if (out->value_type != expected ||
        (context.continuous ? out->counts != nullptr
                            : out->continuous != nullptr))
      return error(err, QPX_READER_INVALID_REQUEST,
                   "Controlled value domain mismatch");
    if (cancelled(cancel))
      return error(err, QPX_READER_CANCELLED, "Controlled values cancelled");
    if (context.mode == "wait-values") {
      event(context, "entered");
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (!fs::exists(fs::u8path(context.path + ".release"))) {
        if (cancelled(cancel))
          return error(err, QPX_READER_CANCELLED,
                       "Controlled active values cancelled");
        if (std::chrono::steady_clock::now() >= deadline)
          return error(err, QPX_READER_IO_ERROR,
                       "Controlled release timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
    const auto n = std::min<std::uint64_t>(request->count, 6 - request->first);
    out->written = n;
    if (out->capacity < n ||
        (n && (context.continuous ? !out->continuous : !out->counts)))
      return error(err, QPX_READER_BUFFER_TOO_SMALL,
                   "Controlled values capacity");
    const std::array<std::uint64_t, 6> counts{
        UINT64_MAX, UINT64_C(9007199254740993), 0, 7, 11, 13};
    const std::array<double, 6> continuous{
        -0.0, -1.25, 2.5, 0.0, 11.0, std::numeric_limits<double>::denorm_min()};
    for (std::uint64_t i = 0; i < n; ++i) {
      if (context.continuous)
        out->continuous[i] = continuous[std::size_t(request->first + i)];
      else
        out->counts[i] = counts[std::size_t(request->first + i)];
    }
    if (context.mode == "values-short" && n)
      --out->written;
    if (context.mode == "values-long")
      ++out->written;
    if (context.mode == "values-capacity")
      ++out->capacity;
    if (context.mode == "values-type")
      out->value_type = 99;
    if (context.mode == "values-size")
      out->struct_size = 4;
    if (context.mode == "values-reserved")
      out->reserved = 1;
    if (context.mode == "values-count-pointer")
      out->counts = nullptr;
    if (context.mode == "values-continuous-pointer")
      out->continuous = nullptr;
    if (context.mode == "values-request")
      ++const_cast<qpx_matrix_values_request_v1 *>(request)->first;
    if (n && context.mode == "values-nan")
      out->continuous[0] = std::numeric_limits<double>::quiet_NaN();
    if (n && context.mode == "values-inf")
      out->continuous[0] = std::numeric_limits<double>::infinity();
    if (context.mode == "mutate-source") {
      std::ofstream changed(fs::u8path(context.path) / "payload.bin",
                            std::ios::binary | std::ios::app);
      changed << "source changed during the callback\n";
      changed.close();
      event(context,
            changed ? "source-write-succeeded" : "source-write-denied");
    }
    if (context.mode == "values-source-add") {
      const auto destination = fs::u8path(context.path) / "added-during-callback.bin";
      std::ofstream changed(destination, std::ios::binary);
      changed << "actual new store member after native fingerprint\n";
      changed.close();
      event(context, changed ? "member-add-succeeded" : "member-add-failed");
    }
    if (context.mode == "values-io")
      return error(err, QPX_READER_IO_ERROR,
                   "Controlled values failure after payload");
    return QPX_READER_OK;
  });
}
} // namespace

extern "C" QPX_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_matrix_format_entry_v3(std::uint32_t abi, qpx_matrix_format_api_v3 *out,
                           qpx_reader_text_v1 *error_text) {
  return boundary(error_text, [&] {
    if (abi != QPX_MATRIX_ABI_V3 || !out || out->struct_size < sizeof(*out))
      return error(error_text, QPX_READER_INVALID_REQUEST,
                   "Invalid controlled ABI3 entry");
    *out = {sizeof(*out), QPX_MATRIX_ABI_V3, describe, open,
            destroy,      metadata,          axis,     values};
    return QPX_READER_OK;
  });
}
