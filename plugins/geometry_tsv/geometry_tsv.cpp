// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/strict_tsv.hpp"
#include "format_plugin_profile.hpp"
#include <array>

namespace {
using namespace qpx_tsv;
// Encoding only: the host's existing native QRO/Roi authority owns geometry
// interpretation, topology, bounds and publication. No C++ geometry crosses
// ABI.
struct Encoder {
  std::uint8_t *bytes = nullptr;
  std::uint64_t capacity = 0, size = 0;
  void byte(std::uint8_t value) {
    valid(size < UINT64_MAX, "Geometry encoding size overflows");
    if (bytes) {
      valid(size < capacity, "Geometry encoding buffer is too small");
      bytes[size] = value;
    }
    ++size;
  }
  void u32(std::uint64_t value) {
    valid(value <= 1000000, "Geometry count exceeds one million");
    for (unsigned i = 0; i < 4; ++i)
      byte(static_cast<std::uint8_t>(value >> (i * 8)));
  }
  void real(double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&bits, &value, sizeof(bits));
    for (unsigned i = 0; i < 8; ++i)
      byte(static_cast<std::uint8_t>(bits >> (i * 8)));
  }
};
template <class Function>
std::uint64_t segments(std::string_view value, char separator,
                       qpx_reader_cancel_v1 cancel, Function &&operation) {
  std::uint64_t count = 0;
  for (std::size_t first = 0;;) {
    checkpoint(cancel);
    const auto end = value.find(separator, first);
    const auto piece =
        value.substr(first, end == value.npos ? end : end - first);
    valid(!piece.empty(), "Geometry contains an empty component");
    operation(piece);
    ++count;
    if (end == value.npos)
      return count;
    first = end + 1;
  }
}
std::uint64_t count_segments(std::string_view value, char separator,
                             qpx_reader_cancel_v1 cancel) {
  return segments(value, separator, cancel, [](auto) {});
}
void ring(Encoder &output, std::string_view value, std::uint64_t minimum,
          qpx_reader_cancel_v1 cancel) {
  const auto count = count_segments(value, ';', cancel);
  valid(count >= minimum, "Geometry path has too few vertices");
  output.u32(count);
  segments(value, ';', cancel, [&](auto pair) {
    const auto n = segments(pair, ',', cancel, [&](auto number) {
      output.real(coordinate(number, cancel));
    });
    valid(n == 2, "Geometry vertex requires exactly x,y");
  });
}
void polygon(Encoder &output, std::string_view value,
             qpx_reader_cancel_v1 cancel) {
  const auto rings = count_segments(value, '|', cancel);
  const auto first = value.find('|');
  ring(output, value.substr(0, first), 3, cancel);
  output.u32(rings - 1);
  if (first != value.npos)
    segments(value.substr(first + 1), '|', cancel,
             [&](auto hole) { ring(output, hole, 3, cancel); });
}
void encode(Encoder &output, std::string_view kind, std::string_view value,
            qpx_reader_cancel_v1 cancel) {
  const std::array<std::string_view, 6> kinds{
      "rectangle", "ellipse", "polygon", "multipolygon", "polyline", "points"};
  const auto found = std::find(kinds.begin(), kinds.end(), kind);
  valid(found != kinds.end(), "Unsupported geometry kind");
  const auto code = static_cast<std::uint8_t>(found - kinds.begin());
  for (auto byte : std::array<std::uint8_t, 5>{'Q', 'R', 'O', 1, code})
    output.byte(byte);
  if (code <= 1) {
    unsigned index = 0;
    const auto n = segments(value, ',', cancel, [&](auto number) {
      const auto v = coordinate(number, cancel);
      valid(index < 2 || v > 0, "Geometry width/height must be positive");
      output.real(v);
      ++index;
    });
    valid(n == 4, "Rectangle/ellipse geometry requires x,y,width,height");
  } else if (code == 4 || code == 5)
    ring(output, value, code == 4 ? 2 : 1, cancel);
  else {
    const auto parts = count_segments(value, '/', cancel);
    valid(code == 2 ? parts == 1 : parts >= 2,
          "Polygon/multipolygon part count differs");
    output.u32(parts);
    segments(value, '/', cancel,
             [&](auto part) { polygon(output, part, cancel); });
  }
  checkpoint(cancel);
}
struct Row {
  Span id, classification;
  std::uint64_t offset, size;
  std::int32_t c, z, t;
};
struct Counts {
  std::uint64_t objects = 0, geometry = 0;
};
struct Handle {
  std::unique_ptr<char[]> bytes;
  std::unique_ptr<Row[]> rows;
  std::unique_ptr<std::uint8_t[]> geometry;
  Counts count;
};
Counts scan(const char *bytes, std::uint64_t size, qpx_reader_cancel_v1 cancel,
            Handle *output = nullptr) {
  Cursor cursor{bytes, size, 0, cancel};
  constexpr std::array<std::string_view, 7> header{
      "id", "kind", "geometry", "c", "z", "t", "class"};
  for (std::size_t i = 0; i < header.size(); ++i) {
    if (!cursor.has())
      fail(QPX_READER_UNSUPPORTED, "Geometry TSV requires the exact "
                                   "id,kind,geometry,c,z,t,class header");
    const auto token = cursor.next();
    if (view(bytes, token.span) != header[i] ||
        (i + 1 < header.size() ? token.end != End::tab : token.end == End::tab))
      fail(QPX_READER_UNSUPPORTED, "Geometry TSV requires the exact "
                                   "id,kind,geometry,c,z,t,class header");
    valid(cursor.position <= header_limit,
          "Geometry TSV header exceeds 64 KiB");
  }
  Counts count;
  while (cursor.has()) {
    std::array<Span, 7> fields{};
    for (std::size_t i = 0; i < fields.size(); ++i) {
      const auto token = cursor.next();
      fields[i] = token.span;
      valid(i + 1 < fields.size() ? token.end == End::tab
                                  : token.end != End::tab,
            "Geometry TSV row must have exactly seven columns");
    }
    label(bytes, fields[0], cancel);
    label(bytes, fields[6], cancel, true);
    const auto c = plane(view(bytes, fields[3]), cancel),
               z = plane(view(bytes, fields[4]), cancel),
               t = plane(view(bytes, fields[5]), cancel);
    valid(z >= 0 && t >= 0,
          "Geometry TSV Z/T must be explicit nonnegative indices");
    Encoder geometry;
    if (output)
      geometry = {output->geometry.get() + count.geometry,
                  output->count.geometry - count.geometry, 0};
    encode(geometry, view(bytes, fields[1]), view(bytes, fields[2]), cancel);
    valid(geometry.size <= UINT64_MAX - count.geometry,
          "Geometry byte total overflows");
    if (output) {
      valid(count.objects < output->count.objects,
            "Geometry row count changed");
      output->rows[count.objects] = {
          fields[0], fields[6], count.geometry, geometry.size, c, z, t};
    }
    count.geometry += geometry.size;
    ++count.objects;
  }
  valid(count.objects > 0, "Geometry TSV requires at least one object");
  checkpoint(cancel);
  return count;
}
const Handle &handle(void *value) {
  require(value != nullptr, "Geometry TSV handle is null");
  return *static_cast<Handle *>(value);
}
qpx_reader_status_v1 QPX_FORMAT_CALL
describe(qpx_format_info_v1 *output, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    bool complete = false;
    descriptor(output, QPX_FORMAT_ANNOTATION, "org.qupathxx.geometry-tsv",
               "geometry-tsv", "Pixel geometry TSV",
               "pixel-geometry-annotation-tsv/1", QPX_FORMAT_PLUGIN_PROFILE_ID,
               complete);
    output->abi_version = QPX_ANNOTATION_ABI_V2;
    return complete
               ? QPX_READER_OK
               : failure(error, QPX_READER_BUFFER_TOO_SMALL,
                         "Geometry TSV descriptor text buffer is too small");
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_reader_open_v1 *request,
                                          void **output,
                                          qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(output != nullptr, "Geometry TSV output handle is null");
    *output = nullptr;
    auto input = source(request, sizeof(Handle));
    const auto count = scan(input.bytes.get(), input.size, request->cancel);
    auto result = std::make_unique<Handle>();
    result->count = count;
    result->rows = input.budget.array<Row>(count.objects);
    result->geometry = input.budget.array<std::uint8_t>(count.geometry);
    auto index = input.budget.array<std::uint64_t>(count.objects);
    result->bytes = std::move(input.bytes);
    const auto actual =
        scan(result->bytes.get(), input.size, request->cancel, result.get());
    valid(actual.objects == count.objects && actual.geometry == count.geometry,
          "Geometry scan changed");
    unique_ids(
        result->bytes.get(), count.objects, index.get(),
        [&](auto i) { return result->rows[i].id; }, request->cancel);
    checkpoint(request->cancel);
    *output = result.release();
    return QPX_READER_OK;
  });
}
void QPX_FORMAT_CALL destroy(void *value) noexcept {
  try {
    delete static_cast<Handle *>(value);
  } catch (...) {
  }
}
qpx_reader_status_v1 QPX_FORMAT_CALL
metadata(void *value, qpx_annotation_metadata_v2 *output,
         qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    const auto &source = handle(value);
    require(output && output->struct_size >= sizeof(*output),
            "Geometry TSV metadata structure is too small");
    checkpoint(cancel);
    output->coordinate_frame = QPX_FORMAT_IMAGE_PIXELS;
    output->geometry_encoding = QPX_FORMAT_GEOMETRY_QRO1;
    output->reserved = 0;
    output->objects = source.count.objects;
    output->geometry_bytes = source.count.geometry;
    output->reserved64[0] = output->reserved64[1] = 0;
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_FORMAT_CALL
objects(void *value, const qpx_annotation_objects_request_v2 *request,
        qpx_annotation_objects_buffer_v2 *output, qpx_reader_cancel_v1 cancel,
        qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    const auto &source = handle(value);
    require(output && output->struct_size >= sizeof(*output),
            "Geometry TSV output structure is too small");
    output->written = output->text.size = output->geometry.size = 0;
    require(request && request->struct_size >= sizeof(*request),
            "Geometry TSV request structure is too small");
    require(!request->reserved && !output->reserved,
            "Geometry TSV reserved fields must be zero");
    require((!output->capacity || output->objects) &&
                (!output->text.capacity || output->text.data) &&
                (!output->geometry.capacity || output->geometry.data),
            "Geometry TSV output pointer is null");
    const auto count =
        page(request->first, request->count, source.count.objects);
    std::uint64_t text = 0, geometry = 0;
    for (std::uint64_t i = 0; i < count; ++i) {
      checkpoint(cancel);
      const auto &row = source.rows[request->first + i];
      text += row.id.size + row.classification.size;
      geometry += row.size;
    }
    if (output->capacity < count || output->text.capacity < text ||
        output->geometry.capacity < geometry) {
      output->written = count;
      output->text.size = text;
      output->geometry.size = geometry;
      return failure(error, QPX_READER_BUFFER_TOO_SMALL,
                     "Geometry TSV output buffers are too small");
    }
    std::uint64_t text_at = 0, geometry_at = 0;
    for (std::uint64_t i = 0; i < count; ++i) {
      checkpoint(cancel);
      const auto &row = source.rows[request->first + i];
      output->objects[i] = {row.c,
                            row.z,
                            row.t,
                            0,
                            text_at,
                            row.id.size,
                            text_at + row.id.size,
                            row.classification.size,
                            geometry_at,
                            row.size};
      std::memcpy(output->text.data + text_at,
                  source.bytes.get() + row.id.offset,
                  static_cast<std::size_t>(row.id.size));
      text_at += row.id.size;
      if (row.classification.size)
        std::memcpy(output->text.data + text_at,
                    source.bytes.get() + row.classification.offset,
                    static_cast<std::size_t>(row.classification.size));
      text_at += row.classification.size;
      std::memcpy(output->geometry.data + geometry_at,
                  source.geometry.get() + row.offset,
                  static_cast<std::size_t>(row.size));
      geometry_at += row.size;
    }
    checkpoint(cancel);
    output->written = count;
    output->text.size = text;
    output->geometry.size = geometry;
    return QPX_READER_OK;
  });
}
} // namespace
extern "C" QPX_ANNOTATION_GEOMETRY_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v2(std::uint32_t requested,
                               qpx_annotation_format_api_v2 *output,
                               qpx_reader_text_v1 *error) {
  return qpx_tsv::boundary(error, [&]() -> qpx_reader_status_v1 {
    qpx_tsv::require(requested == QPX_ANNOTATION_ABI_V2,
                     "Geometry TSV requires annotation ABI2");
    qpx_tsv::require(output && output->struct_size >= sizeof(*output),
                     "Geometry TSV API structure is too small");
    *output = {sizeof(*output), QPX_ANNOTATION_ABI_V2,
               describe,        open,
               destroy,         metadata,
               objects};
    return QPX_READER_OK;
  });
}
