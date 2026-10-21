#pragma once
#include "qupathxx/geometry.hpp"
#include <cstdint>

namespace qpx {
struct SpatialEntry { std::uint64_t id; Rect bounds; };
bool bounds_intersect(Rect a,Rect b);
void validate_query_bounds(Rect);
class SpatialIndex {
public:
    explicit SpatialIndex(std::vector<SpatialEntry>);
    std::vector<std::uint64_t> query(Rect) const;
    std::size_t size() const { return entries_.size(); }
private:
    struct Node { Rect bounds; std::size_t begin,end,left,right; };
    std::vector<SpatialEntry> entries_;
    std::vector<Node> nodes_;
    std::size_t build(std::size_t begin,std::size_t end);
};
struct SpatialStats { std::uint64_t builds; std::size_t base_entries,dirty_entries; };
}
