#pragma once
#include "qupathxx/geometry.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace qpx::detail {
// The same represented rings own magnitude and sign, including hole boundaries.
// Every segment consumes one test and checks cancellation; no hidden O(V) work
// per source/target pair escapes the caller's limit.
inline double signed_boundary_distance(const Roi& geometry,const std::vector<Ring>& rings,
    Point point,double sx,double sy,std::uint64_t& remaining,const std::atomic<bool>* cancelled) {
    double best=std::numeric_limits<double>::infinity();
    for(const auto& ring:rings) for(std::size_t i=0;i<ring.size();++i) {
        if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("spatial distance analysis cancelled");
        if(!remaining) throw std::length_error("spatial boundary segment-test budget exhausted");
        --remaining;
        const auto a=ring[i],b=ring[(i+1)%ring.size()];
        const double dx=(b.x-a.x)*sx,dy=(b.y-a.y)*sy;
        const double vx=(point.x-a.x)*sx,vy=(point.y-a.y)*sy;
        const double length=std::hypot(dx,dy);
        if(!std::isfinite(length) || !std::isfinite(vx) || !std::isfinite(vy)) throw std::overflow_error("spatial boundary coordinates exceed finite metric range");
        double distance;
        // Exact collinearity in binary input coordinates, before scaling. Two
        // rounded products plus their FMA residuals avoid a tolerance-based
        // boundary snap (which could erase a small but real signed distance).
        const double px=(point.x-a.x)*(b.y-a.y),py=(point.y-a.y)*(b.x-a.x);
        // Keep product residuals away from underflow; refuse an unresolved
        // zero below rather than snapping a nonzero distance to a boundary.
        const bool stable_products=(std::abs(px)>=1e-200 || point.x==a.x || b.y==a.y) &&
            (std::abs(py)>=1e-200 || point.y==a.y || b.x==a.x);
        const bool on_edge=stable_products && std::isfinite(px) && std::isfinite(py) && px==py &&
            std::fma(point.x-a.x,b.y-a.y,-px)==std::fma(point.y-a.y,b.x-a.x,-py) &&
            point.x>=std::min(a.x,b.x) && point.x<=std::max(a.x,b.x) &&
            point.y>=std::min(a.y,b.y) && point.y<=std::max(a.y,b.y);
        if(on_edge) distance=0;
        else if(length==0) distance=std::hypot(vx,vy);
        else {
            const double ux=dx/length,uy=dy/length,projection=vx*ux+vy*uy;
            if(!std::isfinite(projection)) throw std::overflow_error("spatial boundary projection exceeds finite range");
            const double t=std::clamp(projection,0.,length);
            distance=std::hypot(vx-t*ux,vy-t*uy);
        }
        if(!std::isfinite(distance)) throw std::overflow_error("spatial boundary distance exceeds finite range");
        if(distance==0 && !on_edge) throw std::overflow_error("spatial boundary distance is below resolvable metric range");
        best=std::min(best,distance);
    }
    if(!std::isfinite(best)) throw std::invalid_argument("spatial boundary has no segments");
    if(best==0) return 0.;
    return geometry.contains(point)?-best:best;
}
}
