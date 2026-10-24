#pragma once
#include "cell_numeric.hpp"
#include <tuple>

namespace qpx::cell_detail {
struct BodyExpansion {
    std::vector<std::uint32_t> labels;
    std::size_t queued_pixels=0,peak_queue=0;
};
struct ExpansionFront {
    float distance;
    std::uint32_t index;
    std::uint64_t sequence;
};
inline std::uint64_t body_expansion_work_bytes(std::size_t width,std::size_t height) {
    if(!width || !height || width>std::numeric_limits<std::uint32_t>::max()/height)
        throw std::length_error("cell expansion grid exceeds index range");
    // Output, EDT, float priorities, visitation and one frontier entry/pixel,
    // plus the separable transform's bounded line workspace. Caller inputs
    // are not included; stages share this conservative admission allowance.
    return (17ull+sizeof(ExpansionFront))*width*height+40ull*std::max(width,height)+4096;
}
template<class Function> inline void expansion_neighbors(std::size_t i,std::size_t width,std::size_t height,Function f) {
    const auto x=i%width,y=i/width;
    if(y) f(i-width);
    if(x) f(i-1);
    if(x+1<width) f(i+1);
    if(y+1<height) f(i+width);
}
inline BodyExpansion expand_cells_watershed(const std::vector<std::uint32_t>& nuclei,
    const std::vector<std::uint8_t>& domain,std::size_t width,std::size_t height,double radius,
    std::size_t work_bytes=128u*1024*1024,const std::atomic<bool>* cancelled=nullptr) {
    const auto charge=body_expansion_work_bytes(width,height);
    const auto n=width*height;
    if(nuclei.size()!=n || domain.size()!=n || !std::isfinite(radius) || radius<0 || radius>1000000)
        throw std::invalid_argument("invalid watershed cell expansion input");
    if(charge>work_bytes) throw std::length_error("cell expansion scratch exceeds memory budget");
    bool seeded=false;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(nuclei[i] && !domain[i]) throw std::invalid_argument("cell expansion seed lies outside its domain");
        seeded=seeded || nuclei[i]!=0;
    }
    BodyExpansion result;result.labels=nuclei;
    if(!seeded || radius==0) return result;
    std::vector<double> squared(n);
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        squared[i]=nuclei[i]?0:infinity;
    }
    distance(squared,width,height,false,nullptr,cancelled);
    std::vector<float> priorities(n);
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        priorities[i]=static_cast<float>(std::sqrt(squared[i]));
    }
    std::vector<double>().swap(squared);
    std::vector<std::uint8_t> visited(n);
    std::vector<ExpansionFront> frontier;frontier.reserve(n);
    auto later=[](const ExpansionFront& a,const ExpansionFront& b) {
        return std::tie(a.distance,a.sequence)>std::tie(b.distance,b.sequence);
    };
    std::uint64_t sequence=0;
    auto enqueue=[&](std::size_t i) {
        if(visited[i]) return;
        visited[i]=1;frontier.push_back({priorities[i],static_cast<std::uint32_t>(i),sequence++});
        std::push_heap(frontier.begin(),frontier.end(),later);
        ++result.queued_pixels;result.peak_queue=std::max(result.peak_queue,frontier.size());
    };
    // Strict cutoff on the float distance field. Initialize every seed before
    // row-major boundary admission; equal priorities then use stable FIFO.
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        visited[i]=!domain[i] || nuclei[i] || !(priorities[i]<radius);
    }
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(visited[i]) continue;
        bool adjacent=false;
        expansion_neighbors(i,width,height,[&](std::size_t j){adjacent=adjacent || nuclei[j]!=0;});
        if(adjacent) enqueue(i);
    }
    std::size_t processed=0;
    while(!frontier.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);
        std::pop_heap(frontier.begin(),frontier.end(),later);const auto next=frontier.back();frontier.pop_back();
        std::uint32_t owner=0;bool collision=false;
        expansion_neighbors(next.index,width,height,[&](std::size_t j) {
            const auto label=result.labels[j];
            if(label) {if(owner && owner!=label) collision=true;else owner=label;}
        });
        if(collision || !owner) continue;
        result.labels[next.index]=owner;
        expansion_neighbors(next.index,width,height,enqueue);
    }
    checkpoint(cancelled);return result;
}
}
