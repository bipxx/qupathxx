// SPDX-License-Identifier: AGPL-3.0-only
// Independent native interpretation of the pinned public ImageJ ROI layout.
#include "roi.hpp"
#include <array>
#include <cmath>
#include <xmmintrin.h>

namespace qpx_imagej {
namespace {
using Encoder = qpx_annotation_source::GeometryEncoder;
int ij_short(Bytes b, std::size_t at) {
  const auto v = b.be16(at);
  auto n = v >= 32768 ? int(v) - 65536 : int(v);
  return n < -5000 ? int(v) : n;
}
void plane(Row &row, std::uint32_t c, std::uint32_t z, std::uint32_t t,
           std::uint32_t legacy) {
  valid(c <= 65536 && z <= 65536 && t <= 65536 && legacy <= 65536,
        "ImageJ plane exceeds native bounds");
  if (c || z || t) {
    row.c = int(c) - 1;
    row.z = int(z) - 1;
    row.t = int(t) - 1;
  } else
    row.linear = legacy;
}
void copy_facts(Facts &out, const Facts &in) {
  for (const auto &[key, value] : in)
    fact(out, key, value);
}
void rotated_rectangle(Encoder &encoder, Bytes source) {
  require((_mm_getcsr() & 0xe040u) == 0 &&
              (_mm_getcsr() & _MM_MASK_MASK) == _MM_MASK_MASK,
          "ImageJ rotated rectangles require nearest rounding, preserved "
          "subnormals and masked SSE exceptions");
  const double x1 = source.real(18), y1 = source.real(22);
  const double x2 = source.real(26), y2 = source.real(30);
  const double width = source.real(52);
  const double dx = x2 - x1, dy = y2 - y1;
  const double length = std::sqrt(dx * dx + dy * dy);
  valid(length > 0 && width > 0,
        "ImageJ rotated rectangle requires distinct endpoints and positive width");
  const double offset_x = width / 2 * dy / length;
  const double offset_y = -width / 2 * dx / length;
  const auto coordinate = [](double value) {
    valid(value >= -2147483648.0 && value < 2147483648.0,
          "ImageJ rotated rectangle corner exceeds the signed image-pixel range");
    const float out = static_cast<float>(value);
    valid(out < 2147483648.0f, "ImageJ rounded corner exceeds the image-pixel range");
    return out;
  };
  const std::array<float, 4> x{coordinate(x1 + offset_x), coordinate(x2 + offset_x),
                               coordinate(x2 - offset_x), coordinate(x1 - offset_x)};
  const std::array<float, 4> y{coordinate(y1 + offset_y), coordinate(y2 + offset_y),
                               coordinate(y2 - offset_y), coordinate(y1 - offset_y)};
  const float origin_x = std::floor(*std::min_element(x.begin(), x.end()));
  const float origin_y = std::floor(*std::min_element(y.begin(), y.end()));
  encoder.header(2);
  encoder.u32(1);
  encoder.u32(4);
  for (std::size_t i = 0; i < x.size(); ++i) {
    // ImageJ stores binary32 vertices relative to its integer bounds and then
    // restores image coordinates in binary32 when returning the polygon.
    const float local_x = x[i] - origin_x, local_y = y[i] - origin_y;
    const float image_x = local_x + origin_x, image_y = local_y + origin_y;
    encoder.real(image_x);
    encoder.real(image_y);
  }
  encoder.u32(0);
}
} // namespace
void roi(Handle &h, Bytes b, std::string_view id, std::string_view fallback,
         const Facts &archive, qpx_reader_cancel_v1 cancel) {
  poll(cancel);
  valid(b.size >= 64 && b.size <= 8u * 1024 * 1024 &&
            b.sub(0, 4).view() == "Iout",
        "Invalid ImageJ ROI header or size");
  auto *r = &h.arena.memory;
  const auto version = b.be16(4);
  const auto type = b.data[6];
  const auto subtype = b.be16(48), options = b.be16(50);
  valid(version >= 217 && version <= 229,
        "ImageJ ROI version outside selected 217..229 profile");
  const bool rotated = type == 7 && subtype == 5;
  if ((subtype && !rotated) || (options & 1) || (type == 1 && b.be16(54)))
    fail("ImageJ text/image/arrow/specialized/spline/rounded geometry requires an "
         "unsupported profile",
         QPX_READER_UNSUPPORTED);
  valid(type <= 10 && type != 6, "Unknown ImageJ ROI type");
  const auto shape = b.be32(36), header = b.be32(60), legacy = b.be32(56);
  const bool subpixel = version >= 222 && (options & 128);
  auto n = std::uint32_t(b.be16(16));
  if (rotated)
    valid(!shape && n == 4, "ImageJ rotated rectangle requires four stored vertices");
  if (!n && (type == 0 || type >= 4))
    n = b.be32(18);
  valid(n <= 65535,
        "ImageJ coordinate count exceeds selected 65535-vertex profile");
  Facts common(r);
  copy_facts(common, archive);
  number(common, "imagej.version", version);
  number(common, "imagej.type", type);
  number(common, "imagej.subtype", subtype);
  number(common, "imagej.options", options);
  number(common, "imagej.legacy-position", legacy);
  hex_facts(common, "imagej.header", b.sub(0, 64));
  std::pmr::string name(fallback, r), properties(r);
  std::uint32_t c = 0, z = 0, t = 0, group = 0, counters = 0;
  std::size_t payload_end = 64;
  if (shape) {
    valid(type == 1 && shape <= 262144, "Invalid composite ImageJ ROI");
    payload_end += std::size_t(shape) * 4;
  } else if (type == 0 || type >= 4) {
    valid(n > 0, "ImageJ coordinate ROI has no vertices");
    payload_end += std::size_t(n) * (subpixel ? 12 : 4);
  }
  b.sub(0, payload_end);
  if (rotated)
    hex_facts(common, "imagej.rotated.vertex-array", b.sub(64, payload_end - 64));
  struct Interval {
    std::size_t first, last;
  };
  std::array<Interval, 5> intervals{};
  std::size_t intervals_used = 0;
  auto interval = [&](std::size_t at, std::size_t bytes) {
    if (!bytes)
      return;
    b.sub(at, bytes);
    valid(at >= payload_end, "ImageJ metadata overlaps geometry");
    for (std::size_t i = 0; i < intervals_used; ++i)
      valid(at >= intervals[i].last || at + bytes <= intervals[i].first,
            "ImageJ metadata fields overlap");
    valid(intervals_used < intervals.size(),
          "Too many ImageJ metadata intervals");
    intervals[intervals_used++] = {at, at + bytes};
  };
  if (header) {
    interval(header, 64);
    hex_facts(common, "imagej.header2", b.sub(header, 64));
    c = b.be32(header + 4);
    z = b.be32(header + 8);
    t = b.be32(header + 12);
    number(common, "imagej.c-position", c);
    number(common, "imagej.z-position", z);
    number(common, "imagej.t-position", t);
    group = b.data[header + 30];
    if (version >= 229 && !group)
      group = b.be16(header + 52);
    valid(!b.be32(header + 32),
          "Embedded ImageJ image payload is not admitted");
    const auto stroke = b.real(header + 36);
    valid(stroke >= 0, "Negative ImageJ floating stroke width");
    fact(common, "imagej.float-stroke-width", std::to_string(stroke));
    auto field = [&](std::size_t offset, std::size_t length) {
      const auto at = b.be32(header + offset), count = b.be32(header + length);
      valid((!at) == (!count), "ImageJ UTF-16 offset/length disagree");
      valid(count <= 262144, "ImageJ text field exceeds bounds");
      interval(at, std::size_t(count) * 2);
      return utf16(b.sub(at, std::size_t(count) * 2), r, cancel);
    };
    auto source_name = field(16, 20);
    if (!source_name.empty())
      name = std::move(source_name);
    properties = field(40, 44);
    counters = b.be32(header + 48);
    if (counters) {
      valid(type == 10 && version >= 227,
            "ImageJ counters require point ROI version227+");
      interval(counters, std::size_t(n) * 4);
    }
  }
  valid(name.size() <= 4096, "ImageJ ROI name exceeds native 4096-byte limit");
  utf8(name, cancel, true);
  chunks(common, "imagej.properties", properties);
  number(common, "imagej.group", group);
  number(common, "imagej.stroke-argb", b.be32(40));
  number(common, "imagej.fill-argb", b.be32(44));
  number(common, "imagej.stroke-width", b.be16(34));
  // Preserve uninterpreted source padding, without assigning it geometry
  // meaning.
  std::sort(intervals.begin(), intervals.begin() + intervals_used,
            [](auto a, auto v) { return a.first < v.first; });
  auto end = payload_end;
  std::size_t padding = 0;
  for (std::size_t i = 0; i <= intervals_used; ++i) {
    auto next = i < intervals_used ? intervals[i].first : b.size;
    if (next > end) {
      valid(next - end <= 32768,
            "ImageJ uninterpreted padding exceeds profile");
      hex_facts(common, "imagej.padding." + std::to_string(padding++),
                b.sub(end, next - end));
    }
    if (i < intervals_used)
      end = intervals[i].last;
  }
  const auto object_count = type == 10 ? n : 1u;
  valid(h.rows.size() + object_count <= 100000,
        "ImageJ collection exceeds 100000 native objects");
  auto point = [&](std::uint32_t i) {
    poll(cancel);
    double x, y;
    if (subpixel) {
      x = b.real(64 + 4 * std::size_t(n) + 4 * std::size_t(i));
      y = b.real(64 + 8 * std::size_t(n) + 4 * std::size_t(i));
    } else {
      const auto dx = ij_short(b, 64 + 2 * std::size_t(i)),
                 dy = ij_short(b, 64 + 2 * std::size_t(n) + 2 * std::size_t(i));
      valid(dx >= 0 && dy >= 0,
            "ImageJ negative local coordinates would require clamping");
      x = ij_short(b, 10) + dx;
      y = ij_short(b, 8) + dy;
    }
    valid(std::isfinite(x) && std::isfinite(y) && x >= 0 && y >= 0,
          "Invalid ImageJ point");
    return std::pair<double, double>{x, y};
  };
  for (std::uint32_t index = 0; index < object_count; ++index) {
    poll(cancel);
    h.rows.emplace_back(r);
    auto &row = h.rows.back();
    row.id = id;
    if (type == 10) {
      row.id += "#point/";
      row.id += std::to_string(index);
    }
    valid(!row.id.empty() && row.id.size() <= 256,
          "ImageJ stable source ID exceeds native 256-byte limit");
    row.name = name;
    copy_facts(row.facts, common);
    plane(row, c, z, t, legacy);
    if (group) {
      row.classification = "group/";
      row.classification += std::to_string(group);
    }
    const auto color = b.be32(40);
    if (color && (color >> 24) == 255) {
      row.flags = 1;
      row.color = color & 0xffffff;
    }
    Encoder e{row.geometry};
    if (shape) {
      row.encoding = 2;
      row.geometry = {'Q', 'E', 'C', 1};
      e.u32(0);
      std::uint32_t rings = 0, vertices = 0;
      std::size_t count_at = 0;
      bool active = false;
      auto close = [&]() {
        valid(active && vertices >= 3,
              "ImageJ composite contour has fewer than three vertices");
        for (unsigned j = 0; j < 4; ++j)
          row.geometry[count_at + j] = std::uint8_t(vertices >> (8 * j));
        active = false;
      };
      for (std::size_t i = 0; i < shape;) {
        poll(cancel);
        const auto command = b.real(64 + 4 * i++);
        valid(command == 0 || command == 1 || command == 4,
              "ImageJ composite curves require unsupported flattening");
        if (command == 4) {
          close();
          continue;
        }
        if (command == 0) {
          if (active)
            close();
          valid(++rings <= 1024, "ImageJ composite has too many contours");
          count_at = row.geometry.size();
          e.u32(0);
          vertices = 0;
          active = true;
        } else
          valid(active, "Composite line has no starting point");
        valid(shape - i >= 2, "Truncated ImageJ composite coordinate");
        e.real(b.real(64 + 4 * i));
        e.real(b.real(64 + 4 * (i + 1)));
        i += 2;
        ++vertices;
      }
      if (active)
        close();
      valid(rings > 0, "Empty ImageJ composite");
      for (unsigned j = 0; j < 4; ++j)
        row.geometry[4 + j] = std::uint8_t(rings >> (8 * j));
    } else if (rotated) {
      rotated_rectangle(e, b);
    } else if (type == 1 || type == 2) {
      e.header(type == 1 ? 0 : 1);
      const bool floating = subpixel && version >= 223;
      const double x = floating ? b.real(18) : ij_short(b, 10),
                   y = floating ? b.real(22) : ij_short(b, 8);
      const double width = floating ? b.real(26) : ij_short(b, 14) - x,
                   height = floating ? b.real(30) : ij_short(b, 12) - y;
      valid(width > 0 && height > 0,
            "ImageJ rectangle/ellipse has nonpositive extent");
      e.real(x);
      e.real(y);
      e.real(width);
      e.real(height);
    } else if (type == 3) {
      e.header(4);
      e.u32(2);
      for (auto at : {18u, 22u, 26u, 30u})
        e.real(b.real(at));
    } else if (type == 10) {
      e.header(5);
      e.u32(1);
      const auto [x, y] = point(index);
      e.real(x);
      e.real(y);
      number(row.facts, "imagej.point-index", index);
      number(row.facts, "imagej.point-count", n);
      if (counters) {
        const auto counter = b.be32(counters + 4 * std::size_t(index));
        number(row.facts, "imagej.point-counter", counter & 255);
        number(row.facts, "imagej.point-position", counter >> 8);
        // Per-point counters carry their own positions, including zero ('all').
        // A legacy ROI-wide position must not turn that zero into a fixed
        // plane. Simultaneous positive flat and hyperstack constraints cannot
        // be mapped to a single native plane without interpreting a
        // target-specific index.
        valid(!(counter >> 8) || !(c || z || t),
              "ImageJ point has simultaneous flat and C/Z/T position "
              "constraints");
        row.linear = counter >> 8;
        valid(row.linear <= 65536,
              "ImageJ point stack index exceeds native axis bounds");
        row.classification = group ? "group/" + std::to_string(group) +
                                         "/counter/" +
                                         std::to_string(counter & 255)
                                   : "counter/" + std::to_string(counter & 255);
      }
    } else {
      const bool area = type == 0 || type == 7 || type == 8;
      valid(n >= (area ? 3u : 2u) && (type != 9 || n == 3),
            "ImageJ path has invalid vertex count");
      e.header(area ? 2 : 4);
      if (area)
        e.u32(1);
      e.u32(n);
      for (std::uint32_t i = 0; i < n; ++i) {
        const auto [x, y] = point(i);
        e.real(x);
        e.real(y);
      }
      if (area)
        e.u32(0);
    }
    valid(row.geometry.size() <= 1024u * 1024,
          "ImageJ native geometry exceeds one MiB");
  }
}
} // namespace qpx_imagej
