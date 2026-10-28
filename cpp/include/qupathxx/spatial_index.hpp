#pragma once
#include "qupathxx/geometry.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace qpx {
struct SpatialEntry { std::uint64_t id; Rect bounds; };
struct SpatialNeighbor { std::uint64_t id; double distance; };
bool bounds_intersect(Rect a,Rect b);
void validate_query_bounds(Rect);
class SpatialIndex {
public:
    explicit SpatialIndex(std::vector<SpatialEntry>);
    std::vector<std::uint64_t> query(Rect) const;
    // Only zero-size point entries are eligible. Equal distances choose the
    // smallest ID; exclusion compares identity, retaining coincident objects.
    std::optional<SpatialNeighbor> nearest_point(Point,double x_scale=1,double y_scale=1,
        std::optional<std::uint64_t> exclude_id={},const std::atomic<bool>* cancelled=nullptr) const;
    std::size_t size() const { return entries_.size(); }
private:
    struct Node { Rect bounds; double max_x,max_y; std::size_t begin,end,left,right; };
    std::vector<SpatialEntry> entries_;
    std::vector<Node> nodes_;
    bool points_only_=true;
    std::size_t build(std::size_t begin,std::size_t end);
};
struct SpatialStats { std::uint64_t builds; std::size_t base_entries,dirty_entries; };
}
