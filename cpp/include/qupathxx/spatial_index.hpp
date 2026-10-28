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
    // An optional caller-owned counter bounds actual distance comparisons
    // across queries; exhaustion throws rather than returning a partial answer.
    std::optional<SpatialNeighbor> nearest_point(Point,double x_scale=1,double y_scale=1,
        std::optional<std::uint64_t> exclude_id={},const std::atomic<bool>* cancelled=nullptr,
        std::uint64_t* remaining_distance_tests=nullptr) const;
    std::size_t size() const { return entries_.size(); }
    // Owned vector payload, not allocator/process overhead. The estimate assumes
    // an input vector reserved to its size; check actual payload after building.
    static std::size_t estimated_bytes(std::size_t entry_count);
    std::size_t payload_bytes() const;
private:
    struct Node { Rect bounds; double max_x,max_y; std::size_t begin,end,left,right; };
    std::vector<SpatialEntry> entries_;
    std::vector<Node> nodes_;
    bool points_only_=true;
    std::size_t build(std::size_t begin,std::size_t end);
};
struct SpatialStats { std::uint64_t builds; std::size_t base_entries,dirty_entries; };
}
