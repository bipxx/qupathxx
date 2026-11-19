// SPDX-License-Identifier: AGPL-3.0-only
// Authored transport fixture, not an NGFF reader or a shipped format fallback.
#include "qupathxx/reader_plugin.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
namespace {
namespace fs = std::filesystem;
std::atomic<unsigned> live{0}, switches{0};
std::atomic<std::uint64_t> last_work{0};
struct Handle {
  fs::path root;
  std::string options;
};
bool text(qpx_reader_text_v1 &out, const std::string &s) {
  out.size = s.size();
  if (out.capacity < s.size() || (!s.empty() && !out.data))
    return false;
  if (!s.empty())
    std::memcpy(out.data, s.data(), s.size());
  return true;
}
bool cancelled(qpx_reader_cancel_v1 c) {
  return c.is_cancelled && c.is_cancelled(c.context);
}
bool fault(const std::string &options, const char *name) {
  return options.find(std::string("\"fault\":\"") + name + "\"") !=
         std::string::npos;
}
void mutate(const fs::path &root) {
  std::ofstream(root / "data/appeared", std::ios::binary) << 'x';
}
template <class F>
qpx_reader_status_v1 boundary(qpx_reader_text_v1 *error, F f) noexcept {
  if (error)
    error->size = 0;
  try {
    return f();
  } catch (...) {
    if (error)
      text(*error, "Authored store fixture failure");
    return QPX_READER_IO_ERROR;
  }
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1 *out,
                                              qpx_reader_text_v1 *e) {
  return boundary(e, [&] {
    if (!out || out->struct_size != sizeof(*out))
      return QPX_READER_INVALID_REQUEST;
    out->abi_version = 1;
    out->capabilities =
        QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW | QPX_READER_CAP_SOURCE_STORE;
#ifdef QPX_TEST_LABEL_DESCRIPTION
    out->capabilities |= QPX_READER_CAP_LABEL_DESCRIPTION;
    bool ok = text(out->provider_id, "org.qupathxx.test-label-description");
    ok = text(out->display_name, "Authored label-description fixture") && ok;
    ok = text(out->version, "1.0.0") && ok;
    ok = text(out->contract, "authored-store-label-description/1") && ok;
#else
    bool ok = text(out->provider_id, "org.qupathxx.test-store");
    ok = text(out->display_name, "Authored directory-store fixture") && ok;
    ok = text(out->version, "1.0.0") && ok;
    ok = text(out->contract, "authored-store-uint64/1") && ok;
#endif
    return ok ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
  });
}
qpx_reader_status_v1 QPX_READER_CALL primary(const qpx_reader_open_v1 *r,
                                             qpx_reader_text_v1 *out,
                                             qpx_reader_text_v1 *e) {
  return boundary(e, [&] {
    if (!r || !out || r->struct_size != sizeof(*r))
      return QPX_READER_INVALID_REQUEST;
    if (cancelled(r->cancel))
      return QPX_READER_CANCELLED;
    const std::string options(r->options.data ? r->options.data : "",
                              std::size_t(r->options.size));
    std::string key = "primary/metadata.json";
    if (fault(options, "escape"))
      key = "../outside.json";
    if (fault(options, "absolute"))
      key = "C:/outside.json";
    if (fault(options, "backslash"))
      key = "primary\\metadata.json";
    if (fault(options, "dot"))
      key = "primary/./metadata.json";
    if (fault(options, "empty"))
      key.clear();
    if (fault(options, "missing"))
      key = "primary/missing.json";
    if (fault(options, "switch") && ++switches > 1)
      key = "primary/other.json";
    if (fault(options, "primary-mutates")) {
      std::ofstream(
          fs::u8path(std::string(r->path.data, std::size_t(r->path.size))) /
              "primary/metadata.json",
          std::ios::app)
          << ' ';
    }
    if (fault(options, "primary-request"))
      const_cast<qpx_reader_open_v1 *>(r)->work_bytes++;
    return text(*out, key) ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
  });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1 *r,
                                          void **out, qpx_reader_text_v1 *e) {
  if (out)
    *out = nullptr;
  return boundary(e, [&] {
    if (!r || !out || r->struct_size != sizeof(*r) || r->reserved ||
        r->work_bytes < 4096)
      return QPX_READER_INVALID_REQUEST;
    if (cancelled(r->cancel))
      return QPX_READER_CANCELLED;
    auto h = std::make_unique<Handle>();
    h->root = fs::u8path(std::string(r->path.data, std::size_t(r->path.size)));
    if (r->options.size)
      h->options.assign(r->options.data, std::size_t(r->options.size));
    last_work = r->work_bytes;
    if (fault(h->options, "open-mutates"))
      mutate(h->root);
    if (fault(h->options, "open-request"))
      const_cast<qpx_reader_open_v1 *>(r)->work_bytes++;
    *out = h.release();
    ++live;
    return QPX_READER_OK;
  });
}
void QPX_READER_CALL destroy(void *p) {
  if (p) {
    delete static_cast<Handle *>(p);
    --live;
  }
}
qpx_reader_status_v1 QPX_READER_CALL metadata(void *p,
                                              qpx_reader_metadata_v1 *out,
                                              qpx_reader_cancel_v1 c,
                                              qpx_reader_text_v1 *e) {
  return boundary(e, [&] {
    if (!p || !out || out->struct_size != sizeof(*out))
      return QPX_READER_INVALID_REQUEST;
    if (cancelled(c))
      return QPX_READER_CANCELLED;
    auto vendor = out->vendor, order = out->dimension_order;
    *out = {};
    out->struct_size = sizeof(*out);
    out->level_count = 1;
    out->width = out->height = 2;
    out->has_raw = 1;
    out->channels = 1;
    out->sample_type = QPX_READER_UINT64;
    out->z_planes = out->timepoints = out->series_count = 1;
    out->vendor = vendor;
    out->dimension_order = order;
    const auto *h = static_cast<Handle *>(p);
#ifdef QPX_TEST_LABEL_DESCRIPTION
    if (fault(h->options, "label-no-raw"))
      out->has_raw = 0;
#endif
    if (fault(h->options, "metadata-mutates"))
      mutate(h->root);
    bool ok = text(out->vendor, "Authored directory fixture");
    ok = text(out->dimension_order, "YX") && ok;
    return ok ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
  });
}
qpx_reader_status_v1 QPX_READER_CALL level(void *p, std::uint32_t i,
                                           qpx_reader_level_v1 *out,
                                           qpx_reader_text_v1 *) {
  if (!p || i || !out || out->struct_size != sizeof(*out))
    return QPX_READER_INVALID_REQUEST;
  *out = {sizeof(*out), 0, 2, 2, 1};
  return QPX_READER_OK;
}
qpx_reader_status_v1 QPX_READER_CALL channel(void *p, std::uint32_t i,
                                             qpx_reader_text_v1 *out,
                                             qpx_reader_text_v1 *) {
  if (!p || i || !out)
    return QPX_READER_INVALID_REQUEST;
  return text(*out, "Labels") ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
}
qpx_reader_status_v1 pixels(void *p, const qpx_reader_region_v1 *r,
                            qpx_reader_pixels_v1 *out, qpx_reader_cancel_v1 c,
                            qpx_reader_text_v1 *e, bool raw) {
  return boundary(e, [&] {
    if (!p || !r || !out || r->struct_size != sizeof(*r) ||
        out->struct_size != sizeof(*out) || r->level || r->width < 1 ||
        r->height < 1 || r->width > 16 || r->height > 16 || r->first_channel ||
        r->z || r->t || r->channels != (raw ? 1u : 0u))
      return QPX_READER_INVALID_REQUEST;
    if (cancelled(c))
      return QPX_READER_CANCELLED;
    const auto *h = static_cast<Handle *>(p);
    std::uint64_t values[4]{};
    std::ifstream file(h->root / "data/pixels", std::ios::binary);
    if (!file.read(reinterpret_cast<char *>(values), sizeof(values)))
      return QPX_READER_IO_ERROR;
    const auto n = std::uint64_t(r->width * r->height),
               bytes = n * (raw ? 8u : 4u);
    if (out->capacity < bytes || (raw && out->validity_capacity < n))
      return QPX_READER_INVALID_REQUEST;
    std::memset(out->data, 0, std::size_t(bytes));
    if (raw)
      std::memset(out->validity, 0, std::size_t(n));
    for (std::int64_t y = 0; y < r->height; ++y)
      for (std::int64_t x = 0; x < r->width; ++x) {
        if (cancelled(c))
          return QPX_READER_CANCELLED;
        const auto sx = r->x + x, sy = r->y + y;
        if (sx < 0 || sy < 0 || sx >= 2 || sy >= 2)
          continue;
        const auto index = std::size_t(y * r->width + x);
        const auto value = values[std::size_t(sy * 2 + sx)];
        if (raw) {
          std::memcpy(out->data + index * 8, &value, 8);
          out->validity[index] = 1;
        } else {
          out->data[index * 4] = std::uint8_t(value & 255);
          out->data[index * 4 + 3] = 255;
        }
      }
    out->written = bytes;
    out->validity_written = raw ? n : 0;
    if (fault(h->options, "read-mutates") ||
        fault(h->options, "failed-read-mutates"))
      mutate(h->root);
    if (fault(h->options, "failed-read-mutates"))
      return QPX_READER_IO_ERROR;
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_READER_CALL rgba(void *p,
                                          const qpx_reader_region_v1 *r,
                                          qpx_reader_pixels_v1 *o,
                                          qpx_reader_text_v1 *e) {
  return pixels(p, r, o, {}, e, false);
}
qpx_reader_status_v1 QPX_READER_CALL raw(void *p, const qpx_reader_region_v1 *r,
                                         qpx_reader_pixels_v1 *o,
                                         qpx_reader_cancel_v1 c,
                                         qpx_reader_text_v1 *e) {
  return pixels(p, r, o, c, e, true);
}
} // namespace
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1 *out,
                    qpx_reader_text_v1 *) {
  if (abi != 1 || !out || out->struct_size != sizeof(*out))
    return QPX_READER_INVALID_REQUEST;
  *out = {sizeof(*out), 1,     describe, open, destroy,
          metadata,     level, channel,  rgba, raw};
  return QPX_READER_OK;
}
#ifndef QPX_TEST_MISSING_STORE
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_store_entry_v1(std::uint32_t abi, qpx_reader_store_api_v1 *out,
                          qpx_reader_text_v1 *) {
  if (abi != 1 || !out || out->struct_size != sizeof(*out))
    return QPX_READER_INVALID_REQUEST;
  *out = {sizeof(*out), 1, primary};
#ifdef QPX_TEST_BAD_STORE
  out->abi_version = 2;
#endif
  return QPX_READER_OK;
}
#endif
extern "C" __declspec(dllexport) unsigned QPX_READER_CALL
qpx_test_live_handles() {
  return live.load();
}
extern "C" __declspec(dllexport) std::uint64_t QPX_READER_CALL
qpx_test_last_work() {
  return last_work.load();
}
