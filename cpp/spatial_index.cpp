#include "qupathxx/spatial_index.hpp"
#include "qupathxx/core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx {
void validate_query_bounds(Rect b) {
    if(!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.width) || !std::isfinite(b.height) ||
        b.width<0 || b.height<0 || !std::isfinite(b.x+b.width) || !std::isfinite(b.y+b.height))
        throw std::invalid_argument("spatial bounds must be finite with nonnegative size");
}
bool bounds_intersect(Rect a,Rect b) {
    return a.x<=b.x+b.width && b.x<=a.x+a.width && a.y<=b.y+b.height && b.y<=a.y+a.height;
}
SpatialIndex::SpatialIndex(std::vector<SpatialEntry> entries) : entries_(std::move(entries)) {
    if(entries_.size()>std::numeric_limits<std::size_t>::max()/2) throw std::length_error("spatial index size overflow");
    nodes_.reserve(entries_.size()/2+1);
    for(const auto& e:entries_) validate_query_bounds(e.bounds);
    if(!entries_.empty()) build(0,entries_.size());
}
std::size_t SpatialIndex::build(std::size_t begin,std::size_t end) {
    auto b=entries_[begin].bounds;
    double left=b.x,top=b.y,right=b.x+b.width,bottom=b.y+b.height;
    for(auto i=begin+1;i<end;++i) {
        b=entries_[i].bounds; left=std::min(left,b.x); top=std::min(top,b.y); right=std::max(right,b.x+b.width); bottom=std::max(bottom,b.y+b.height);
    }
    Rect extent{left,top,right-left,bottom-top}; validate_query_bounds(extent);
    auto index=nodes_.size(); nodes_.push_back({extent,begin,end,0,0});
    if(end-begin>8) {
        const bool horizontal=right-left>=bottom-top; auto middle=begin+(end-begin)/2;
        std::nth_element(entries_.begin()+begin,entries_.begin()+middle,entries_.begin()+end,[&](const auto& a,const auto& b) {
            auto ac=horizontal?a.bounds.x+a.bounds.width/2:a.bounds.y+a.bounds.height/2;
            auto bc=horizontal?b.bounds.x+b.bounds.width/2:b.bounds.y+b.bounds.height/2;
            return ac==bc?a.id<b.id:ac<bc;
        });
        auto l=build(begin,middle),r=build(middle,end); nodes_[index].left=l; nodes_[index].right=r;
    }
    return index;
}
std::vector<std::uint64_t> SpatialIndex::query(Rect bounds) const {
    validate_query_bounds(bounds); std::vector<std::uint64_t> result;
    if(nodes_.empty()) return result;
    std::vector<std::size_t> work{0};
    while(!work.empty()) {
        auto i=work.back(); work.pop_back(); const auto& node=nodes_[i];
        if(!bounds_intersect(node.bounds,bounds)) continue;
        if(node.end-node.begin<=8) {
            for(auto e=node.begin;e<node.end;++e) if(bounds_intersect(entries_[e].bounds,bounds)) result.push_back(entries_[e].id);
        } else { work.push_back(node.left); work.push_back(node.right); }
    }
    return result;
}
void Document::refresh_spatial() const {
    if(!spatial_rebuild_ && spatial_) return;
    std::vector<SpatialEntry> entries; entries.reserve(objects_.size());
    for(const auto& [id,a]:objects_) entries.push_back({id,a.geometry.bounds()});
    auto next=std::make_shared<SpatialIndex>(std::move(entries));
    spatial_=std::move(next); spatial_dirty_.clear(); spatial_rebuild_=false; ++spatial_builds_;
}
std::vector<std::uint64_t> Document::query_region(Rect bounds) const {
    validate_query_bounds(bounds); refresh_spatial();
    auto result=spatial_->query(bounds);
    result.erase(std::remove_if(result.begin(),result.end(),[&](auto id){return spatial_dirty_.count(id)!=0;}),result.end());
    for(const auto& [id,current]:spatial_dirty_) if(current && bounds_intersect(*current,bounds)) result.push_back(id);
    std::sort(result.begin(),result.end()); result.erase(std::unique(result.begin(),result.end()),result.end()); return result;
}
std::vector<std::uint64_t> Document::pick(Point point,double tolerance) const {
    if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(tolerance) || tolerance<0)
        throw std::invalid_argument("picking requires finite coordinates and nonnegative tolerance");
    auto result=query_region({point.x-tolerance,point.y-tolerance,2*tolerance,2*tolerance});
    result.erase(std::remove_if(result.begin(),result.end(),[&](auto id){return find(id).geometry.distance(point)>tolerance;}),result.end());
    return result;
}
SpatialStats Document::spatial_stats() const { return {spatial_builds_,spatial_?spatial_->size():0,spatial_dirty_.size()}; }
}
