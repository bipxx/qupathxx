// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/wkb_geometry.hpp"
#include <arrow/array.h>
#include <arrow/type.h>

namespace qpx_geoarrow {
using namespace qpx_wkb;

// GeoParquet 1.1 uses separated XY coordinates. The vertices are struct
// values and each additional level is a list of a geometry component.
inline void check_type(const arrow::DataType& type, unsigned levels) {
    if (levels) {
        valid(type.id() == arrow::Type::LIST, "GeoArrow geometry nesting differs from its encoding");
        check_type(*static_cast<const arrow::ListType&>(type).value_type(), levels - 1);
        return;
    }
    valid(type.id() == arrow::Type::STRUCT && type.num_fields() == 2,
          "GeoArrow requires a separated XY struct");
    valid(type.field(0)->name() == "x" && type.field(1)->name() == "y" &&
              type.field(0)->type()->id() == arrow::Type::DOUBLE &&
              type.field(1)->type()->id() == arrow::Type::DOUBLE,
          "GeoArrow requires Float64 x and y fields");
}

inline unsigned list_depth(unsigned kind) {
    constexpr unsigned depths[] = {0, 0, 1, 2, 1, 2, 3};
    valid(kind >= 1 && kind <= 6, "Unsupported GeoArrow native geometry type");
    return depths[kind];
}

struct Elements {
    const arrow::Array& values;
    int64_t offset, size;
};

inline Elements elements(const arrow::Array& array, int64_t index, Limits& limits) {
    poll(limits.cancel);
    valid(!array.IsNull(index), "GeoArrow contains a null list");
    const auto& list = static_cast<const arrow::ListArray&>(array);
    const auto size = list.value_length(index);
    valid(size > 0 && size <= 1000000, "GeoArrow contains an empty or oversized list");
    return {*list.values(), list.value_offset(index), size};
}

inline Point point(const arrow::Array& array, int64_t index, Limits& limits) {
    valid(!array.IsNull(index), "GeoArrow contains a null vertex");
    const auto& xy = static_cast<const arrow::StructArray&>(array);
    const auto& x = static_cast<const arrow::DoubleArray&>(*xy.field(0));
    const auto& y = static_cast<const arrow::DoubleArray&>(*xy.field(1));
    valid(!x.IsNull(index) && !y.IsNull(index), "GeoArrow contains a null coordinate");
    return limits.point(x.Value(index), y.Value(index));
}

inline void points(std::pmr::vector<Point>& destination, const Elements& source, Limits& limits) {
    valid(std::uint64_t(source.size) <= 1000000 - limits.points,
          "GeoArrow geometry exceeds the point-count budget");
    destination.reserve(static_cast<std::size_t>(source.size));
    for (int64_t i = 0; i < source.size; ++i)
        destination.push_back(point(source.values, source.offset + i, limits));
}

inline void polygons(std::pmr::vector<Polygon>& destination, const Elements& source, Limits& limits,
                     std::pmr::memory_resource* memory) {
    valid(source.size <= 65535, "GeoArrow polygon exceeds the ring-count budget");
    auto& polygon = destination.emplace_back(memory);
    for (int64_t i = 0; i < source.size; ++i) {
        auto& path = polygon.rings.emplace_back(memory);
        points(path.points, elements(source.values, source.offset + i, limits), limits);
        ring(path);
    }
}

// Array structure and offsets are fully validated before this function is called.
inline Node geometry(const arrow::Array& array, int64_t index, unsigned kind,
                     std::pmr::memory_resource* memory, Limits& limits) {
    limits.node(0);
    Node node(memory);
    node.type = kind;
    if (kind == 1) {
        node.points.push_back(point(array, index, limits));
        return node;
    }
    const auto source = elements(array, index, limits);
    switch (kind) {
    case 2:
        valid(source.size >= 2, "GeoArrow LineString requires at least two points");
        [[fallthrough]];
    case 4:
        points(node.points, source, limits);
        break;
    case 3:
        polygons(node.polygons, source, limits, memory);
        break;
    case 5:
        for (int64_t i = 0; i < source.size; ++i) {
            limits.node(1);
            auto& line = node.children.emplace_back(memory);
            line.type = 2;
            const auto vertices = elements(source.values, source.offset + i, limits);
            valid(vertices.size >= 2, "GeoArrow LineString requires at least two points");
            points(line.points, vertices, limits);
        }
        break;
    case 6:
        for (int64_t i = 0; i < source.size; ++i) {
            limits.node(1);
            polygons(node.polygons, elements(source.values, source.offset + i, limits), limits, memory);
        }
        break;
    default:
        fail("Unsupported GeoArrow native geometry type");
    }
    return node;
}
} // namespace qpx_geoarrow
