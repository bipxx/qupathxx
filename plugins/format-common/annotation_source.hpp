// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "binary_input.hpp"
#include <utility>
namespace qpx_annotation_source {
using namespace qpx_binary;
using Facts = std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>>;
struct Row {
  std::pmr::string id, name, classification;
  std::pmr::vector<std::uint8_t> geometry;
  Facts facts;
  std::int32_t c = -1, z = -1, t = -1;
  std::uint32_t encoding = 1, color = 0, flags = 0;
  std::uint64_t linear = 0;
  explicit Row(std::pmr::memory_resource *r)
      : id(r), name(r), classification(r), geometry(r), facts(r) {}
};
inline void fact(Facts &out, std::string_view key, std::string_view value) {
  valid(!key.empty() && key.size() <= 256 && value.size() <= 4096 &&
            out.size() < 256,
        "Annotation source facts exceed native limits");
  auto *r = out.get_allocator().resource();
  out.emplace_back(std::pmr::string(key, r), std::pmr::string(value, r));
}
inline void number(Facts &out, std::string_view key, std::uint64_t value) {
  fact(out, key, std::to_string(value));
}
inline void chunks(Facts &out, std::string_view key, std::string_view value) {
  number(out, std::string(key) + ".bytes", value.size());
  std::size_t at = 0, index = 0;
  while (at < value.size()) {
    auto n = std::min<std::size_t>(4096, value.size() - at);
    while (at + n < value.size() && (std::uint8_t(value[at + n]) & 192) == 128)
      --n;
    fact(out, std::string(key) + "." + std::to_string(index++),
         value.substr(at, n));
    at += n;
  }
}
inline void hex_facts(Facts &out, std::string_view key, Bytes b) {
  number(out, std::string(key) + ".bytes", b.size);
  for (std::size_t at = 0, index = 0; at < b.size; ++index) {
    char data[4096];
    const auto n = std::min<std::size_t>(2048, b.size - at);
    for (std::size_t i = 0; i < n; ++i) {
      data[2 * i] = "0123456789abcdef"[b.data[at + i] >> 4];
      data[2 * i + 1] = "0123456789abcdef"[b.data[at + i] & 15];
    }
    fact(out, std::string(key) + "." + std::to_string(index), {data, 2 * n});
    at += n;
  }
}
struct GeometryEncoder {
  std::pmr::vector<std::uint8_t> &out;
  void u32(std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
      out.push_back(std::uint8_t(n >> (8 * i)));
  }
  void real(double x) {
    valid(std::isfinite(x) && x >= 0,
          "Annotation geometry requires finite nonnegative coordinates");
    std::uint64_t v;
    std::memcpy(&v, &x, 8);
    for (unsigned i = 0; i < 8; ++i)
      out.push_back(std::uint8_t(v >> (8 * i)));
  }
  void header(unsigned kind) { out = {'Q', 'R', 'O', 1, std::uint8_t(kind)}; }
};
struct Handle {
  Arena arena;
  std::pmr::vector<Row> rows;
  std::uint64_t canvas_width = 0, canvas_height = 0;
  explicit Handle(std::size_t work) : arena(work), rows(&arena.memory) {}
};

// Shared bounded ABI3 value ownership and callbacks. Definition supplies
// descriptor strings and decode(Handle&, Bytes, request); native host owns all
// publication.
inline constexpr char default_provider_version[] = "1.0.0";
template <class Definition, unsigned Version = 3,
          const char *ProviderVersion = default_provider_version,
          std::size_t PrivateWork = 0>
struct Plugin {
  static_assert(Version >= 3 && Version <= 6, "Unsupported source ABI");
  static qpx_reader_status_v1 QPX_FORMAT_CALL
  describe(qpx_format_info_v1 *out, qpx_reader_text_v1 *error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
      require(out && out->struct_size >= sizeof(*out),
              "Invalid Annotation source descriptor structure");
      out->abi_version = Version;
      out->domain = QPX_FORMAT_ANNOTATION;
      out->reserved = 0;
      bool ok = text(out->provider_id, Definition::id);
      ok = text(out->format_id, Definition::format) && ok;
      ok = text(out->display_name, Definition::name) && ok;
      ok = text(out->version, ProviderVersion) && ok;
      ok = text(out->contract, Definition::contract) && ok;
      ok = text(out->profile, Definition::profile) && ok;
      return ok ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
  }
  static qpx_reader_status_v1 QPX_FORMAT_CALL
  open(const qpx_reader_open_v1 *request, void **out,
       qpx_reader_text_v1 *error) noexcept {
    if (out)
      *out = nullptr;
    return boundary(error, [&]() -> qpx_reader_status_v1 {
      require(out && request && request->struct_size >= sizeof(*request) &&
                  !request->reserved,
              "Invalid Annotation source open structure");
      require(request->options.size <= 65536 &&
                  (!request->options.size || request->options.data),
              "Invalid annotation source options");
      Definition::validate_options(request->options);
      require(request->work_bytes >= 256 * 1024 &&
                  request->work_bytes <= 256u * 1024 * 1024,
              "Annotation source work reservation must be 256 KiB to 256 MiB");
      require(request->path.data && request->path.size &&
                  request->path.size <= 32768,
              "Invalid annotation source path");
      const auto opening = UINT64_C(65536) + 8 * request->path.size + PrivateWork;
      require(request->work_bytes > opening,
              "Annotation source work reservation cannot cover path/opening "
              "overhead");
      poll(request->cancel);
      // One fixed, bounded arena owns all input, parse tables, strings and
      // geometry. Opening includes64 KiB handle/platform overhead and a
      // conservative UTF-8/UTF-16 path conversion/copy allowance.
      auto h =
          std::make_unique<Handle>(std::size_t(request->work_bytes - opening));
      const auto input = read(*request, &h->arena.memory);
      Definition::decode(*h, input, *request);
      poll(request->cancel);
      *out = h.release();
      return QPX_READER_OK;
    });
  }
  static void QPX_FORMAT_CALL destroy(void *value) noexcept {
    delete static_cast<Handle *>(value);
  }
  static qpx_reader_status_v1 QPX_FORMAT_CALL
  metadata(void *value, qpx_annotation_metadata_v3 *out, qpx_reader_cancel_v1 c,
           qpx_reader_text_v1 *error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
      require(value && out && out->struct_size >= sizeof(*out),
              "Invalid Annotation source metadata structure");
      const auto &h = *static_cast<Handle *>(value);
      *out = {sizeof(*out),
              QPX_FORMAT_IMAGE_PIXELS,
              std::uint64_t(h.rows.size()),
              0,
              0,
              0,
              0,
              {0, 0}};
      for (const auto &row : h.rows) {
        poll(c);
        out->geometry_bytes += row.geometry.size();
        out->geometry_encodings |= UINT64_C(1) << (row.encoding - 1);
        out->source_bytes += row.name.size();
        out->properties += row.facts.size();
        for (const auto &[key, v] : row.facts)
          out->source_bytes += key.size() + v.size();
      }
      return QPX_READER_OK;
    });
  }
  static qpx_reader_status_v1 QPX_FORMAT_CALL
  objects(void *value, const qpx_annotation_objects_request_v2 *request,
          qpx_annotation_objects_buffer_v3 *out, qpx_reader_cancel_v1 c,
          qpx_reader_text_v1 *error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
      require(value && request && request->struct_size >= sizeof(*request) &&
                  !request->reserved && out &&
                  out->struct_size >= sizeof(*out) && !out->reserved,
              "Invalid Annotation source object page");
      const auto &h = *static_cast<Handle *>(value);
      require(request->count && request->first <= h.rows.size(),
              "Invalid Annotation source page range");
      require((!out->capacity || out->objects) &&
                  (!out->text.capacity || out->text.data) &&
                  (!out->geometry.capacity || out->geometry.data) &&
                  (!out->property_capacity || out->properties),
              "Null Annotation source output storage");
      const auto n = std::min<std::uint64_t>(request->count,
                                             h.rows.size() - request->first);
      out->written = n;
      out->text.size = out->geometry.size = out->properties_written = 0;
      for (std::uint64_t i = 0; i < n; ++i) {
        poll(c);
        const auto &row = h.rows[std::size_t(request->first + i)];
        out->geometry.size += row.geometry.size();
        out->properties_written += row.facts.size();
        out->text.size +=
            row.id.size() + row.classification.size() + row.name.size();
        for (const auto &[key, v] : row.facts)
          out->text.size += key.size() + v.size();
      }
      if (out->capacity < n || out->text.capacity < out->text.size ||
          out->geometry.capacity < out->geometry.size ||
          out->property_capacity < out->properties_written)
        return QPX_READER_BUFFER_TOO_SMALL;
      std::uint64_t at = 0, geom = 0, properties = 0;
      auto write = [&](std::string_view s) {
        const auto start = at;
        if (!s.empty())
          std::memcpy(out->text.data + at, s.data(), s.size());
        at += s.size();
        return start;
      };
      for (std::uint64_t i = 0; i < n; ++i) {
        poll(c);
        const auto &row = h.rows[std::size_t(request->first + i)];
        auto &dto = out->objects[i];
        dto = {};
        dto.value.c = row.c;
        dto.value.z = row.z;
        dto.value.t = row.t;
        dto.value.source_id_offset = write(row.id);
        dto.value.source_id_size = row.id.size();
        dto.value.class_offset = write(row.classification);
        dto.value.class_size = row.classification.size();
        dto.value.geometry_offset = geom;
        dto.value.geometry_size = row.geometry.size();
        std::memcpy(out->geometry.data + geom, row.geometry.data(),
                    row.geometry.size());
        geom += row.geometry.size();
        dto.name_offset = write(row.name);
        dto.name_size = row.name.size();
        dto.first_property = properties;
        dto.properties = row.facts.size();
        dto.geometry_encoding = row.encoding;
        dto.plane_kind = row.linear ? 1 : 0;
        dto.linear_position = row.linear;
        dto.flags = row.flags;
        dto.color_rgb = row.color;
        for (const auto &[key, v] : row.facts) {
          auto &p = out->properties[properties++];
          p.key_offset = write(key);
          p.key_size = key.size();
          p.value_offset = write(v);
          p.value_size = v.size();
        }
      }
      poll(c);
      return QPX_READER_OK;
    });
  }
  static constexpr qpx_annotation_format_api_v3 source_table() {
    return {sizeof(qpx_annotation_format_api_v3), Version, describe, open, destroy, metadata, objects};
  }
  static qpx_reader_status_v1 entry(std::uint32_t abi,
                                    qpx_annotation_format_api_v3 *out,
                                    qpx_reader_text_v1 *error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
      require(abi == Version && out && out->struct_size >= sizeof(*out),
              "Annotation source ABI mismatch");
      *out = source_table();
      return QPX_READER_OK;
    });
  }
};
} // namespace qpx_annotation_source
