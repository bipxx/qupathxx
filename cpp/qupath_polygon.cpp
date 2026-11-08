#include "qupath_polygon.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#ifdef QUPATHXX_HAS_GEOS
#include <geos_c.h>
#endif

namespace qpx::detail {
Roi qupath_saved_polygon(Ring input,bool& normalized,const std::atomic<bool>* cancelled) {
    normalized=false;
    auto poll=[&] {if(cancelled && cancelled->load())throw std::runtime_error("QuPath import cancelled");};
    poll();
    try {return Roi::polygon(input);}
    catch(const std::invalid_argument&) {}
#ifndef QUPATHXX_HAS_GEOS
    throw std::runtime_error("saved polygon topology requires GEOS");
#else
    if(input.size()>1 && input.front().x==input.back().x && input.front().y==input.back().y)input.pop_back();
    if(input.size()<3 || input.size()>100000)throw std::invalid_argument("saved polygon normalization exceeds3..100000 vertices");
    struct Context {
        GEOSContextHandle_t handle=GEOS_init_r();
        ~Context() {if(handle)GEOS_finish_r(handle);}
    } context;
    if(!context.handle)throw std::bad_alloc();
    struct Cycle {Ring ring;Roi roi;int sign=0,winding=0;std::size_t owner=0;};
    std::vector<Cycle> cycles;
    Ring stack;
    std::map<std::pair<double,double>,std::size_t> positions;
    for(std::size_t i=0;i<=input.size();++i) {
        if(i%128==0)poll();
        const auto point=input[i%input.size()];
        if(!std::isfinite(point.x) || !std::isfinite(point.y))throw std::invalid_argument("nonfinite saved polygon coordinate");
        const auto key=std::make_pair(point.x,point.y);
        const auto found=positions.find(key);
        if(found==positions.end()) {positions.emplace(key,stack.size());stack.push_back(point);continue;}
        const auto begin=found->second;
        if(stack.size()-begin<3)throw std::invalid_argument("saved polygon has an adjacent duplicate or backtracking spike");
        if(cycles.size()>=1024)throw std::length_error("saved polygon normalization exceeds1024 cycles");
        Ring ring(stack.begin()+begin,stack.end());
        auto roi=Roi::polygon(ring); // Crossings without repeated vertices remain invalid.
        // A simple ring's lexicographic minimum is a convex extreme. Use the
        // robust local orientation, avoiding cancellation in a global area sum.
        const auto extreme=std::min_element(ring.begin(),ring.end(),[](auto a,auto b){return a.x<b.x || (a.x==b.x && a.y<b.y);})-ring.begin();
        const auto a=ring[(extreme+ring.size()-1)%ring.size()],b=ring[extreme],c=ring[(extreme+1)%ring.size()];
        const auto sign=GEOSOrientationIndex_r(context.handle,a.x,a.y,b.x,b.y,c.x,c.y);
        if(sign!=1 && sign!=-1)throw std::invalid_argument("invalid saved polygon winding");
        cycles.push_back({std::move(ring),std::move(roi),sign,0,0});
        for(std::size_t j=begin+1;j<stack.size();++j)positions.erase({stack[j].x,stack[j].y});
        stack.resize(begin+1);
    }
    if(stack.size()!=1 || cycles.size()<2)throw std::invalid_argument("saved polygon has no valid vertex-touch decomposition");
    if(input.size()>2000000/cycles.size())throw std::length_error("saved polygon cycle-pair work exceeds limit");
    std::stable_sort(cycles.begin(),cycles.end(),[](const auto& a,const auto& b){return a.roi.area()>b.roi.area();});
    std::vector<PolygonPart> parts;
    for(std::size_t i=0;i<cycles.size();++i) {
        poll();
        auto& cycle=cycles[i];
        std::size_t parent=cycles.size();
        for(std::size_t j=0;j<i;++j) {
            poll();
            if(cycles[j].roi.covers(cycle.roi))parent=j;
            else if(cycles[j].roi.boolean_operation(cycle.roi,BooleanOp::Intersection))
                throw std::invalid_argument("saved polygon cycles cross without containment");
        }
        const auto before=parent==cycles.size()?0:cycles[parent].winding;
        cycle.winding=before+cycle.sign;
        if(before==0 && cycle.winding!=0) {
            cycle.owner=parts.size();parts.emplace_back(cycle.ring);
        } else {
            if(parent==cycles.size())throw std::invalid_argument("invalid saved polygon cycle parent");
            cycle.owner=cycles[parent].owner;
            if(cycle.winding==0)parts.at(cycle.owner).holes.push_back(cycle.ring);
        }
    }
    // GEOS validates the entire shell/hole/part arrangement, including crossings
    // between individually simple cycles. No coordinate snapping or MakeValid.
    auto result=Roi::multipolygon(std::move(parts));
    normalized=true;
    return result;
#endif
}
}
