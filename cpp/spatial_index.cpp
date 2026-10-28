#include "qupathxx/spatial_index.hpp"
#include "qupathxx/core.hpp"
#include <algorithm>
#include <array>
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
    for(const auto& e:entries_) {validate_query_bounds(e.bounds);points_only_=points_only_ && e.bounds.width==0 && e.bounds.height==0;}
    if(!entries_.empty()) build(0,entries_.size());
}
std::size_t SpatialIndex::build(std::size_t begin,std::size_t end) {
    auto b=entries_[begin].bounds;
    double left=b.x,top=b.y,right=b.x+b.width,bottom=b.y+b.height;
    for(auto i=begin+1;i<end;++i) {
        b=entries_[i].bounds; left=std::min(left,b.x); top=std::min(top,b.y); right=std::max(right,b.x+b.width); bottom=std::max(bottom,b.y+b.height);
    }
    Rect extent{left,top,right-left,bottom-top}; validate_query_bounds(extent);
    auto index=nodes_.size(); nodes_.push_back({extent,right,bottom,begin,end,0,0});
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
std::optional<SpatialNeighbor> SpatialIndex::nearest_point(Point point,double sx,double sy,
    std::optional<std::uint64_t> exclude,const std::atomic<bool>* cancelled) const {
    if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(sx) ||
       !std::isfinite(sy) || sx<=0 || sy<=0)
        throw std::invalid_argument("nearest point query requires finite coordinates and positive finite scales");
    if(!points_only_) throw std::invalid_argument("nearest point query requires a point-only spatial index");
    auto checkpoint=[&] {if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("spatial query cancelled");};
    checkpoint();if(nodes_.empty()) return {};
    auto delta=[](double a,double b,double scale) {
        const auto difference=a-b;
        // Opposite near-limit coordinates can overflow before a small physical
        // scale brings their separation back into the representable range.
        return std::abs(std::isfinite(difference)?difference*scale:a*scale-b*scale);
    };
    auto lower=[&](std::size_t i) {
        const auto& n=nodes_[i];
        const auto x=point.x<n.bounds.x?delta(n.bounds.x,point.x,sx):point.x>n.max_x?delta(point.x,n.max_x,sx):0.;
        const auto y=point.y<n.bounds.y?delta(n.bounds.y,point.y,sy):point.y>n.max_y?delta(point.y,n.max_y,sy):0.;
        // L-infinity is conservative for Euclidean pruning without squaring or
        // rounding a norm upward. Maxima retain actual coordinates, rather than
        // reconstructing a possibly rounded-inward origin + extent.
        return std::max(x,y);
    };
    struct Visit {std::size_t node;double lower;};
    // Median splitting limits depth to the address width; no per-query heap or
    // source-by-target distance matrix is required.
    std::array<Visit,std::numeric_limits<std::size_t>::digits+1> work{};
    std::size_t used=0;work[used++]={0,lower(0)};std::optional<SpatialNeighbor> best;
    while(used) {
        checkpoint();const auto visit=work[--used];
        if(best && visit.lower>best->distance) continue;
        const auto& node=nodes_[visit.node];
        if(node.end-node.begin<=8) {
            for(auto i=node.begin;i<node.end;++i) {
                const auto& e=entries_[i];if(exclude && e.id==*exclude) continue;
                const auto distance=std::hypot(delta(e.bounds.x,point.x,sx),delta(e.bounds.y,point.y,sy));
                if(!best || distance<best->distance || (distance==best->distance && e.id<best->id)) best=SpatialNeighbor{e.id,distance};
            }
        } else {
            const auto a=lower(node.left),b=lower(node.right);
            if(used+2>work.size()) throw std::length_error("spatial query traversal exceeds address depth");
            if(a<=b) {work[used++]={node.right,b};work[used++]={node.left,a};}
            else {work[used++]={node.left,a};work[used++]={node.right,b};}
        }
    }
    if(best && !std::isfinite(best->distance)) throw std::overflow_error("nearest point distance exceeds numeric range");
    return best;
}
void Document::refresh_spatial() const {
    if(!spatial_rebuild_ && spatial_) return;
    std::vector<SpatialEntry> entries; entries.reserve(objects_.size());
    for(const auto& [id,a]:objects_) entries.push_back({id,a.bounds()});
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
    result.erase(std::remove_if(result.begin(),result.end(),[&](auto id){const auto& a=find(id); return a.geometry.distance(point)>tolerance && (!a.nucleus || a.nucleus->distance(point)>tolerance);}),result.end());
    return result;
}
SpatialStats Document::spatial_stats() const { return {spatial_builds_,spatial_?spatial_->size():0,spatial_dirty_.size()}; }
}
