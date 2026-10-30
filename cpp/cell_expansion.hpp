#pragma once
#include "cell_numeric.hpp"
#include <array>
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
// Small sampled distances have few distinct priorities. FIFO lists within
// each squared-distance bucket reproduce the heap's (float distance, sequence)
// order, including newly admitted pixels at a lower distance than the last pop.
class SmallExpansionFrontier {
    static constexpr auto absent=std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t,64> heads_,tails_;
    std::vector<std::uint32_t> next_;
    std::uint64_t occupied_=0;
    std::size_t size_=0;
    static unsigned first_bit(std::uint64_t bits) {
        unsigned at=0;
        if(!(bits&0xffffffffull)) {bits>>=32;at+=32;}
        if(!(bits&0xffffull)) {bits>>=16;at+=16;}
        if(!(bits&0xffull)) {bits>>=8;at+=8;}
        if(!(bits&0xfull)) {bits>>=4;at+=4;}
        if(!(bits&3ull)) {bits>>=2;at+=2;}
        return at+unsigned(!(bits&1ull));
    }
public:
    explicit SmallExpansionFrontier(std::size_t n):next_(n,absent) {heads_.fill(absent);tails_.fill(absent);}
    bool empty() const {return !occupied_;}
    std::size_t size() const {return size_;}
    void push(std::uint32_t index,float priority) {
        // For sqrt(k), k<64, float multiplication error is far below 0.5;
        // rounding recovers the exact integer squared distance. These 64 keys
        // have distinct increasing float square roots.
        const auto bin=static_cast<unsigned>(std::lround(priority*priority));
        if(bin>=heads_.size()) throw std::logic_error("small expansion priority exceeds bucket range");
        if(tails_[bin]!=absent) next_[tails_[bin]]=index;else heads_[bin]=index;
        tails_[bin]=index;next_[index]=absent;occupied_|=std::uint64_t(1)<<bin;++size_;
    }
    std::uint32_t pop() {
        if(!occupied_) throw std::logic_error("empty small expansion frontier");
        const auto bin=first_bit(occupied_),index=heads_[bin];heads_[bin]=next_[index];
        if(heads_[bin]==absent) {tails_[bin]=absent;occupied_&=~(std::uint64_t(1)<<bin);}
        --size_;return index;
    }
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
    std::size_t work_bytes=128u*1024*1024,const std::atomic<bool>* cancelled=nullptr,bool prefer_small_frontier=true,bool qupath_initial_boundary=false) {
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
    // Distinct sampled pixel centers are at least one unit apart. The strict
    // cutoff cannot admit a non-seed pixel at radii up to and including one.
    if(!seeded || radius<=1) return result;
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
    const bool small=prefer_small_frontier && radius<=8;
    SmallExpansionFrontier buckets(small?n:0);
    std::vector<ExpansionFront> frontier;if(!small) frontier.reserve(n);
    auto later=[](const ExpansionFront& a,const ExpansionFront& b) {
        return std::tie(a.distance,a.sequence)>std::tie(b.distance,b.sequence);
    };
    std::uint64_t sequence=0;
    auto enqueue=[&](std::size_t i) {
        if(visited[i]) return;
        visited[i]=1;
        if(small) buckets.push(static_cast<std::uint32_t>(i),priorities[i]);
        else {frontier.push_back({priorities[i],static_cast<std::uint32_t>(i),sequence++});std::push_heap(frontier.begin(),frontier.end(),later);}
        ++result.queued_pixels;result.peak_queue=std::max(result.peak_queue,small?buckets.size():frontier.size());
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
        // The public floating-label watershed admits the image boundary to its
        // initial queue: outside-label getPixelValue is NaN, whose !=0 is true.
        // Retain the original frontier for old recipes and saved replay.
        bool adjacent=qupath_initial_boundary && (i%width==0 || i/width==0 || i%width==width-1 || i/width==height-1);
        expansion_neighbors(i,width,height,[&](std::size_t j){adjacent=adjacent || nuclei[j]!=0;});
        if(adjacent) enqueue(i);
    }
    std::size_t processed=0;
    while(small?!buckets.empty():!frontier.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);
        std::uint32_t index;
        if(small) index=buckets.pop();
        else {std::pop_heap(frontier.begin(),frontier.end(),later);index=frontier.back().index;frontier.pop_back();}
        std::uint32_t owner=0;bool collision=false;
        expansion_neighbors(index,width,height,[&](std::size_t j) {
            const auto label=result.labels[j];
            if(label) {if(owner && owner!=label) collision=true;else owner=label;}
        });
        if(collision || !owner) continue;
        result.labels[index]=owner;
        expansion_neighbors(index,width,height,enqueue);
    }
    checkpoint(cancelled);return result;
}
}
