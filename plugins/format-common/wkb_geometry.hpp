// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "annotation_source.hpp"
#include <array>
#include <limits>

namespace qpx_wkb {
using namespace qpx_annotation_source;
inline constexpr std::array<const char *, 8> names{
    "",           "POINT",           "LINESTRING",   "POLYGON",
    "MULTIPOINT", "MULTILINESTRING", "MULTIPOLYGON", "GEOMETRYCOLLECTION"};
struct Point {
  double x, y;
};
struct Path {
  std::pmr::vector<Point> points;
  explicit Path(std::pmr::memory_resource *r) : points(r) {}
};
struct Polygon {
  std::pmr::vector<Path> rings;
  explicit Polygon(std::pmr::memory_resource *r) : rings(r) {}
};
struct Node {
  unsigned type = 0;
  std::pmr::vector<Point> points;
  std::pmr::vector<Polygon> polygons;
  std::pmr::vector<Node> children;
  explicit Node(std::pmr::memory_resource *r)
      : points(r), polygons(r), children(r) {}
};
template<class Geometry, class Visit>
void visit_points(Geometry& node, qpx_reader_cancel_v1 cancel, Visit& visit) {
    for (auto& point : node.points) {
        poll(cancel);
        visit(point);
    }
    for (auto& polygon : node.polygons)
        for (auto& ring : polygon.rings)
            for (auto& point : ring.points) {
                poll(cancel);
                visit(point);
            }
    for (auto& child : node.children)
        visit_points(child, cancel, visit);
}

struct Bounds {
    double x_min = std::numeric_limits<double>::infinity();
    double y_min = x_min, x_max = -x_min, y_max = -x_min;
    void add(Point point) {
        x_min = std::min(x_min, point.x);
        y_min = std::min(y_min, point.y);
        x_max = std::max(x_max, point.x);
        y_max = std::max(y_max, point.y);
    }
    void include(const Node& node, qpx_reader_cancel_v1 cancel) {
        auto include_point = [&](Point point) { add(point); };
        visit_points(node, cancel, include_point);
    }
    std::array<double, 4> xy() const { return {x_min, y_min, x_max, y_max}; }
};

struct Limits {
  std::size_t points = 0, nodes = 0;
  qpx_reader_cancel_v1 cancel;
  bool signed_coordinates = false;
  void node(unsigned depth) {
    poll(cancel);
    valid(depth < 32 && ++nodes <= 100000,
          "Geometry nesting/node count exceeds profile");
  }
  Point point(double x, double y) {
    poll(cancel);
    valid(
        ++points <= 1000000 && std::isfinite(x) && std::isfinite(y) &&
            (signed_coordinates || (x >= 0 && y >= 0)),
        signed_coordinates ? "Geometry requires finite XY within the point-count budget"
                           : "WKT/WKB requires finite nonnegative XY within the point-count budget");
    return {x, y};
  }
};
inline void ring(const Path &path) {
  valid(
      path.points.size() >= 4 &&
          path.points.front().x == path.points.back().x &&
          path.points.front().y == path.points.back().y,
      "Polygon rings must contain at least four explicitly closed coordinates");
}
struct Wkb {
  Bytes bytes;
  std::size_t at = 0;
  std::pmr::memory_resource *resource;
  Limits limits;
  std::uint64_t integer(unsigned count, bool little) {
    const auto result = bytes.number(at, count, !little);
    at += count;
    return result;
  }
  double real(bool little) {
    const auto bits = integer(8, little);
    double out;
    std::memcpy(&out, &bits, 8);
    return out;
  }
  Point point(bool little) {
    const auto x = real(little), y = real(little);
    return limits.point(x, y);
  }
  std::uint32_t count(bool little, std::size_t minimum, std::uint32_t maximum) {
    const auto n = std::uint32_t(integer(4, little));
    valid(n && n <= maximum && n <= (bytes.size - at) / minimum,
          "WKB empty, truncated or excessive geometry count");
    return n;
  }
  Path path(bool little, bool closed) {
    Path out(resource);
    const auto n = count(little, 16, 1000000);
    valid(n <= 1000000 - limits.points, "WKB coordinate count exceeds profile");
    out.points.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i)
      out.points.push_back(point(little));
    if (closed)
      ring(out);
    else
      valid(n >= 2, "LineString requires at least two points");
    return out;
  }
  Polygon polygon(bool little) {
    Polygon out(resource);
    const auto n = count(little, 4, 65535);
    out.rings.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i)
      out.rings.push_back(path(little, true));
    return out;
  }
  Node value(unsigned depth = 0, unsigned expected = 0) {
    limits.node(depth);
    const auto order = integer(1, false);
    valid(order <= 1, "WKB byte order must be0 or1");
    const bool little = order == 1;
    const auto kind = integer(4, little);
    valid(kind >= 1 && kind <= 7,
          "WKB supports only basic2D types without Z/M/SRID/extension flags");
    valid(!expected || kind == expected,
          "WKB multipart member has an incompatible type");
    Node out(resource);
    out.type = unsigned(kind);
    if (kind == 1)
      out.points.push_back(point(little));
    else if (kind == 2)
      out.points = std::move(path(little, false).points);
    else if (kind == 3)
      out.polygons.push_back(polygon(little));
    else {
      const auto n = count(little, 5, 100000);
      for (std::uint32_t i = 0; i < n; ++i) {
        auto child = value(depth + 1, kind == 7 ? 0 : unsigned(kind - 3));
        if (kind == 4)
          out.points.push_back(child.points.front());
        else if (kind == 6)
          out.polygons.push_back(std::move(child.polygons.front()));
        else
          out.children.push_back(std::move(child));
      }
    }
    return out;
  }
};
inline void copy(Facts &out, const Facts &in) {
  for (const auto &[key, value] : in)
    fact(out, key, value);
}
inline void emit(Handle &h, const Node &node, std::string_view path,
          const Facts &ancestors, std::string_view display,
          qpx_reader_cancel_v1 cancel, std::string_view id_prefix = "geometry", unsigned depth = 0) {
  poll(cancel);
  auto *resource = &h.arena.memory;
  if (node.type == 5 || node.type == 7) {
    Facts group(resource);
    copy(group, ancestors);
    const auto key = "geometry.container." + std::to_string(depth);
    fact(group, key + ".type", names[node.type]);
    fact(group, key + ".path", path.empty() ? "/" : path);
    number(group, key + ".children", node.children.size());
    for (std::size_t i = 0; i < node.children.size(); ++i)
      emit(h, node.children[i], std::string(path) + "/" + std::to_string(i),
           group, display, cancel, id_prefix, depth + 1);
    return;
  }
  valid(h.rows.size() < 100000,
        "Geometry collection exceeds100000 native objects");
  h.rows.emplace_back(resource);
  auto &row = h.rows.back();
  row.id = id_prefix;
  row.id += path;
  row.name = display;
  copy(row.facts, ancestors);
  fact(row.facts, "geometry.type", names[node.type]);
  fact(row.facts, "geometry.path", path.empty() ? "/" : path);
  number(row.facts, "geometry.components",
         node.type == 3 || node.type == 6 ? node.polygons.size()
                                          : node.points.size());
  GeometryEncoder wire{row.geometry};
  if (node.type == 1 || node.type == 4 || node.type == 2) {
    wire.header(node.type == 2 ? 4 : 5);
    wire.u32(std::uint32_t(node.points.size()));
    for (const auto &point : node.points) {
      poll(cancel);
      wire.real(point.x);
      wire.real(point.y);
    }
  } else {
    wire.header(node.polygons.size() == 1 ? 2 : 3);
    wire.u32(std::uint32_t(node.polygons.size()));
    for (const auto &polygon : node.polygons) {
      poll(cancel);
      for (std::size_t i = 0; i < polygon.rings.size(); ++i) {
        if (i == 1)
          wire.u32(std::uint32_t(polygon.rings.size() - 1));
        const auto &points = polygon.rings[i].points;
        wire.u32(std::uint32_t(points.size() - 1));
        for (std::size_t j = 0; j + 1 < points.size(); ++j) {
          poll(cancel);
          wire.real(points[j].x);
          wire.real(points[j].y);
        }
      }
      if (polygon.rings.size() == 1)
        wire.u32(0);
    }
  }
  valid(row.id.size() <= 256 && row.name.size() <= 4096 &&
            row.geometry.size() <= 1024u * 1024,
        "WKT/WKB native value exceeds ID/name/one-MiB geometry limits");
}
} // namespace qpx_wkb
