#pragma once
#include "qupathxx/core.hpp"
#include <tuple>

namespace qpx {
using TileKey = std::tuple<std::int64_t,std::int64_t,std::int32_t,std::int64_t,std::int64_t>;
inline TileKey tile_key(const RegionRequest& r) { return {r.x,r.y,r.level,r.width,r.height}; }
struct ViewTile {
    RegionRequest request;
    Rect bounds;
    int priority;
    bool fallback;
};
struct TilePlan {
    std::vector<ViewTile> tiles; // Coarse fallback first, detail last.
    std::int32_t detail_level = 0;
    bool truncated = false;
};
TilePlan plan_view_tiles(const ImageMetadata&, const Viewport&, double canvas_width,
    double canvas_height, double framebuffer_scale = 1, int tile_size = 256,
    std::size_t max_tiles = 192);
}
