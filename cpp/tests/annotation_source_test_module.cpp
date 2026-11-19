// SPDX-License-Identifier: AGPL-3.0-only
// Controlled contract provider, never a shipped format implementation.
#include "format_plugin_profile.hpp"
#include "qupathxx/format_plugin.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Cancelled {};
void poll(qpx_reader_cancel_v1 c) {
  if (c.is_cancelled && c.is_cancelled(c.context))
    throw Cancelled{};
}
template <class Call>
qpx_reader_status_v1 boundary(qpx_reader_text_v1 *error, Call &&call) noexcept {
  if (error)
    error->size = 0;
  try {
    return call();
  } catch (const Cancelled &) {
    return QPX_READER_CANCELLED;
  } catch (const std::exception &e) {
    if (error) {
      const auto n = std::strlen(e.what());
      error->size = n;
      if (error->data && error->capacity >= n)
        std::memcpy(error->data, e.what(), n);
    }
    return QPX_READER_INVALID_REQUEST;
  } catch (...) {
    return QPX_READER_INTERNAL_ERROR;
  }
}
void require(bool b) {
  if (!b)
    throw std::invalid_argument("Invalid controlled source contract request");
}
struct Row {
  std::string id, name, classification;
  std::map<std::string, std::string> facts;
  std::vector<std::uint8_t> geometry;
};
struct Handle {
  std::string mode;
  std::filesystem::path events;
  std::array<Row, 2> rows;
};
void event(const Handle &h, const char *value) {
  std::ofstream out(h.events, std::ios::app | std::ios::binary);
  out << value << '\n';
}
void integer(std::vector<std::uint8_t> &out, std::uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    out.push_back(std::uint8_t(n >> (8 * i)));
}
void real(std::vector<std::uint8_t> &out, double value) {
  std::uint64_t n;
  std::memcpy(&n, &value, 8);
  for (unsigned i = 0; i < 8; ++i)
    out.push_back(std::uint8_t(n >> (8 * i)));
}
std::vector<std::uint8_t> rectangle() {
  std::vector<std::uint8_t> out{'Q', 'R', 'O', 1, 0};
  for (auto v : {1.25, 2.5, 4., 3.})
    real(out, v);
  return out;
}
std::vector<std::uint8_t> contours(bool crossing) {
  std::vector<std::uint8_t> out{'Q', 'E', 'C', 1};
  integer(out, crossing ? 2 : 4);
  const std::array<std::array<double, 4>, 4> boxes =
      crossing
          ? std::array<std::array<double, 4>, 4>{{{1, 1, 7, 7},
                                                  {4, 4, 9, 9},
                                                  {0, 0, 0, 0},
                                                  {0, 0, 0, 0}}}
          : std::array<std::array<double, 4>, 4>{
                {{1, 1, 9, 9}, {2, 2, 8, 8}, {3, 3, 7, 7}, {12, 12, 15, 15}}};
  for (unsigned i = 0; i < (crossing ? 2u : 4u); ++i) {
    const auto &b = boxes[i];
    integer(out, 4);
    for (auto p : std::array<std::array<double, 2>, 4>{
             {{b[0], b[1]}, {b[2], b[1]}, {b[2], b[3]}, {b[0], b[3]}}}) {
      real(out, p[0]);
      real(out, p[1]);
    }
  }
  return out;
}
bool put(qpx_reader_text_v1 &out, const std::string &value) {
  out.size = value.size();
  if (out.capacity < value.size())
    return false;
  if (!value.empty()) {
    require(out.data);
    std::memcpy(out.data, value.data(), value.size());
  }
  return true;
}
qpx_reader_status_v1 QPX_FORMAT_CALL
describe(qpx_format_info_v1 *out, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(out && out->struct_size >= sizeof(*out));
    out->abi_version = 3;
    out->domain = 2;
    out->reserved = 0;
    bool complete =
        put(out->provider_id, "org.qupathxx.test.annotation-source");
    complete = put(out->format_id, "test-annotation-source") && complete;
    complete =
        put(out->display_name, "Controlled annotation source") && complete;
    complete = put(out->version, "1.0.0") && complete;
    complete = put(out->contract, "controlled-source-facts/1") && complete;
    complete = put(out->profile, QPX_FORMAT_PLUGIN_PROFILE_ID) && complete;
    return complete ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_reader_open_v1 *request,
                                          void **out,
                                          qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(out);
    *out = nullptr;
    require(request && request->struct_size >= sizeof(*request) &&
            !request->reserved && request->path.data &&
            !request->options.size && request->work_bytes >= 65536);
    poll(request->cancel);
    const auto path = std::filesystem::u8path(
        std::string(request->path.data, std::size_t(request->path.size)));
    std::ifstream in(path, std::ios::binary);
    require(bool(in));
    auto h = std::make_unique<Handle>();
    std::getline(in, h->mode);
    h->events = std::filesystem::u8path(path.u8string() + ".events");
    for (unsigned i = 0; i < 2; ++i) {
      auto &row = h->rows[i];
      row.id = i ? "1" : "0001";
      row.name = "Shared display μ";
      row.classification = "N";
      row.facts = {
          {"note", h->mode == "value-b" ? "changed α\nβ\tμ" : "α\nβ\tμ"},
          {"source_id", "opaque source fact"}};
      row.geometry = rectangle();
    }
    if (h->mode == "duplicate-id")
      h->rows[1].id = h->rows[0].id;
    if (h->mode == "contours" || h->mode == "crossing")
      h->rows[0].geometry = contours(h->mode == "crossing");
    poll(request->cancel);
    event(*h, "open");
    *out = h.release();
    return QPX_READER_OK;
  });
}
void QPX_FORMAT_CALL destroy(void *value) noexcept {
  if (value) {
    auto *h = static_cast<Handle *>(value);
    try {
      event(*h, "destroy");
    } catch (...) {
    }
    delete h;
  }
}
qpx_reader_status_v1 QPX_FORMAT_CALL
metadata(void *value, qpx_annotation_metadata_v3 *out,
         qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(value && out && out->struct_size >= sizeof(*out));
    poll(cancel);
    const auto &h = *static_cast<Handle *>(value);
    out->coordinate_frame = 1;
    out->objects = 2;
    out->geometry_bytes = out->source_bytes = out->properties = 0;
    out->geometry_encodings =
        (h.mode == "contours" || h.mode == "crossing") ? 3 : 1;
    out->reserved[0] = out->reserved[1] = 0;
    for (const auto &row : h.rows) {
      out->geometry_bytes += row.geometry.size();
      out->source_bytes += row.name.size();
      out->properties += row.facts.size();
      for (const auto &[key, data] : row.facts)
        out->source_bytes += key.size() + data.size();
    }
    if (h.mode == "metadata-source-total")
      ++out->source_bytes;
    if (h.mode == "metadata-property-total")
      ++out->properties;
    if (h.mode == "metadata-source-huge")
      out->source_bytes = UINT64_MAX;
    if (h.mode == "metadata-encoding")
      out->geometry_encodings = 8;
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL
objects(void *value, const qpx_annotation_objects_request_v2 *request,
        qpx_annotation_objects_buffer_v3 *out, qpx_reader_cancel_v1 cancel,
        qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(value && request && request->struct_size >= sizeof(*request) &&
            !request->reserved && request->count && request->first <= 2);
    require(out && out->struct_size >= sizeof(*out) && !out->reserved);
    require((!out->capacity || out->objects) &&
            (!out->text.capacity || out->text.data) &&
            (!out->geometry.capacity || out->geometry.data) &&
            (!out->property_capacity || out->properties));
    const auto &h = *static_cast<Handle *>(value);
    const auto n = std::min<std::uint64_t>(request->count, 2 - request->first);
    poll(cancel);
    std::uint64_t nt = 0, ng = 0, np = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
      const auto &row = h.rows[std::size_t(request->first + i)];
      nt += row.id.size() + row.classification.size() + row.name.size();
      ng += row.geometry.size();
      np += row.facts.size();
      for (const auto &[key, data] : row.facts)
        nt += key.size() + data.size();
    }
    out->written = n;
    out->text.size = nt;
    out->geometry.size = ng;
    out->properties_written = np;
    if (out->capacity < n || out->text.capacity < nt ||
        out->geometry.capacity < ng || out->property_capacity < np) {
      if (h.mode == "probe-properties-huge")
        out->properties_written = 257;
      if (h.mode == "probe-properties-pointer")
        out->properties =
            reinterpret_cast<qpx_annotation_property_v3 *>(std::uintptr_t(1));
      return QPX_READER_BUFFER_TOO_SMALL;
    }
    std::uint64_t at = 0, geometry_at = 0, property_at = 0;
    auto text = [&](const std::string &s) {
      const auto offset = at;
      if (!s.empty())
        std::memcpy(out->text.data + at, s.data(), s.size());
      at += s.size();
      return offset;
    };
    for (std::uint64_t i = 0; i < n; ++i) {
      poll(cancel);
      const auto index = request->first + i;
      const auto &row = h.rows[std::size_t(index)];
      auto &record = out->objects[i];
      record = {};
      record.value.c = record.value.z = record.value.t = -1;
      record.value.source_id_offset = text(row.id);
      record.value.source_id_size = row.id.size();
      record.value.class_offset = text(row.classification);
      record.value.class_size = row.classification.size();
      record.value.geometry_offset = geometry_at;
      record.value.geometry_size = row.geometry.size();
      std::memcpy(out->geometry.data + geometry_at, row.geometry.data(),
                  row.geometry.size());
      geometry_at += row.geometry.size();
      record.name_offset = text(row.name);
      record.name_size = row.name.size();
      record.first_property = property_at;
      record.properties = row.facts.size();
      record.geometry_encoding =
          (index == 0 && (h.mode == "contours" || h.mode == "crossing")) ? 2
                                                                         : 1;
      for (const auto &[key, data] : row.facts) {
        auto &property = out->properties[property_at++];
        property.key_offset = text(key);
        property.key_size = key.size();
        property.value_offset = text(data);
        property.value_size = data.size();
      }
      if (index == 0) {
        record.flags = QPX_ANNOTATION_OBJECT_COLOR_RGB;
        record.color_rgb = 0x123456;
      }
      if (h.mode == "linear") {
        record.plane_kind = QPX_ANNOTATION_PLANE_LINEAR_POSITION;
        record.linear_position = 1;
      }
      if (h.mode == "explicit") {
        record.value.c = 0;
        record.value.z = 1;
        record.value.t = 1;
      }
    }
    if (n) {
      auto &row = out->objects[0];
      if (h.mode == "property-key-offset")
        out->properties[0].key_offset = UINT64_MAX;
      if (h.mode == "property-value-size")
        out->properties[0].value_size = 4097;
      if (h.mode == "property-duplicate")
        out->properties[1] = out->properties[0];
      if (h.mode == "property-utf8")
        out->text.data[out->properties[0].value_offset] = char(0xff);
      if (h.mode == "property-nul")
        out->text.data[out->properties[0].value_offset] = 0;
      if (h.mode == "property-span")
        row.first_property = 1;
      if (h.mode == "property-fill-count")
        --out->properties_written;
      if (h.mode == "property-fill-pointer")
        ++out->properties;
      if (h.mode == "name-offset")
        row.name_offset = UINT64_MAX;
      if (h.mode == "name-utf8")
        out->text.data[row.name_offset] = char(0xff);
      if (h.mode == "color-overflow")
        row.color_rgb = 0x1000000;
      if (h.mode == "color-without-flag")
        row.flags = 0;
      if (h.mode == "row-encoding")
        row.geometry_encoding = 2;
      if (h.mode == "linear-zero") {
        row.plane_kind = 1;
        row.linear_position = 0;
      }
      if (h.mode == "linear-czt") {
        row.plane_kind = 1;
        row.linear_position = 1;
        row.value.z = 0;
      }
      if (h.mode == "plane-kind")
        row.plane_kind = 9;
      if (h.mode == "row-reserved")
        row.reserved = 1;
    }
    poll(cancel);
    return QPX_READER_OK;
  });
}
} // namespace
extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t requested,
                               qpx_annotation_format_api_v3 *out,
                               qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(requested == 3 && out && out->struct_size >= sizeof(*out));
    *out = {sizeof(*out), 3, describe, open, destroy, metadata, objects};
    return QPX_READER_OK;
  });
}
