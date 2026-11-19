// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "annotation_source.hpp"
#include "json_tree.hpp"
#include <array>

namespace qpx_point_columns {
using namespace qpx_annotation_source;
inline constexpr std::size_t text_limit(std::size_t role) { return role == 3 ? 4096 : 256; }
inline constexpr std::array<std::string_view, 5> roles = {"id", "x", "y", "name", "class"};

using Columns = std::array<std::string_view, roles.size()>;
using Indices = std::array<int, roles.size()>;

inline Columns select(const qpx_native_json::Tree& tree) {
    using qpx_native_json::Kind;
    Columns names{};
    bool pixel_space = false;
    require(tree.nodes[0].kind == Kind::Object, "Point options must be an object");
    for (auto child : tree.nodes[0].children) {
        const auto& item = tree.nodes[child];
        require(item.kind == Kind::String && !item.value.empty() && item.value.size() <= 256,
                "Point selections require nonempty strings of at most 256 bytes");
        if (item.key == "coordinate_space") {
            pixel_space = item.value == "image-pixels";
        } else {
            const auto role = std::find(roles.begin(), roles.end(), std::string_view(item.key));
            require(role != roles.end(), "Unknown Point option");
            require(std::find(names.begin(), names.end(), std::string_view(item.value)) == names.end(),
                    "Point selections must use distinct columns");
            names[std::size_t(role - roles.begin())] = item.value;
        }
    }
    require(pixel_space && !names[0].empty() && !names[1].empty() && !names[2].empty(),
            "Points require image-pixels and explicit id, x and y columns");
    return names;
}

inline double exact_coordinate(std::uint64_t integer) {
    const auto converted = static_cast<double>(integer);
    valid(converted < 0x1p64 && static_cast<std::uint64_t>(converted) == integer,
          "Point coordinate is not exactly representable as binary64");
    return converted;
}

inline void begin_point(Row& row) {
    GeometryEncoder wire{row.geometry};
    wire.header(5);
    wire.u32(1);
}
} // namespace qpx_point_columns
