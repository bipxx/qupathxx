#pragma once
#include "cell_numeric.hpp"
#include <tuple>

namespace qpx::cell_detail {
// Private compatibility primitive. Regional maxima use eight-connected
// plateaus, excluding those connected to the image border. Four-connected
// seed labels grow by raw float32 height, with row-major initial frontier and
// FIFO ties. This is separate from the shipped quantitative basin recipe.
inline std::vector<std::uint32_t> qupath070_response_basins(
    const std::vector<float>& response,std::size_t width,std::size_t height,
    std::size_t maximum_basins,std::size_t work_bytes,
    const std::atomic<bool>* cancelled=nullptr) {
    checkpoint(cancelled);
    if(!width || !height || width>(1ULL<<26)/height || width*height!=response.size() ||
       !maximum_basins || maximum_basins>1000000)
        throw std::invalid_argument("invalid compatibility response dimensions or basin limit");
    const auto n=response.size();
    if(work_bytes<65536 || n>(work_bytes-65536)/64)
        throw std::length_error("compatibility response exceeds work budget");
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(!std::isfinite(response[i])) throw std::invalid_argument("nonfinite compatibility response");
    }
    auto four=[&](std::size_t i,auto visit) {
        const auto x=i%width,y=i/width;
        if(y) visit(i-width);if(x) visit(i-1);
        if(x+1<width) visit(i+1);if(y+1<height) visit(i+width);
    };
    auto eight=[&](std::size_t i,auto visit) {
        const auto x=i%width,y=i/width;
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            if((!dx && !dy) || (dx<0 && !x) || (dy<0 && !y) ||
               (dx>0 && x+1==width) || (dy>0 && y+1==height)) continue;
            visit(std::size_t(std::int64_t(i)+dy*std::int64_t(width)+dx));
        }
    };
    std::vector<std::uint8_t> seen(n),peaks(n),queued(n);
    std::vector<std::uint32_t> component;component.reserve(n);
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(seen[i] || response[i]<.001f) continue;
        component.clear();component.push_back(std::uint32_t(i));seen[i]=1;bool accepted=true;
        for(std::size_t at=0;at<component.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            const auto j=std::size_t(component[at]),x=j%width,y=j/width;
            if(!x || !y || x+1==width || y+1==height) accepted=false;
            eight(j,[&](std::size_t k) {
                if(response[k]>response[i]) accepted=false;
                if(!seen[k] && response[k]==response[i]) {seen[k]=1;component.push_back(std::uint32_t(k));}
            });
        }
        if(accepted) for(auto j:component) peaks[j]=1;
    }
    std::vector<std::uint32_t> labels(n);std::uint32_t count=0;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(!peaks[i] || labels[i]) continue;
        if(count==maximum_basins) throw std::length_error("compatibility basin limit exceeded");
        ++count;component.clear();component.push_back(std::uint32_t(i));labels[i]=count;
        for(std::size_t at=0;at<component.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            four(component[at],[&](std::size_t j) {
                if(peaks[j] && !labels[j]) {labels[j]=count;component.push_back(std::uint32_t(j));}
            });
        }
    }
    struct Entry {float value;std::uint64_t sequence;std::uint32_t index;};
    auto later=[](const Entry& a,const Entry& b) {
        return a.value!=b.value?a.value<b.value:a.sequence>b.sequence;
    };
    std::vector<Entry> heap;heap.reserve(n);std::uint64_t sequence=0;
    auto enqueue=[&](std::size_t i) {
        if(queued[i]) return;queued[i]=1;
        heap.push_back({response[i],sequence++,std::uint32_t(i)});std::push_heap(heap.begin(),heap.end(),later);
    };
    for(std::size_t i=0;i<n;++i) queued[i]=labels[i]!=0 || response[i]<=0;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);if(queued[i]) continue;
        bool adjacent=false;four(i,[&](std::size_t j){adjacent=adjacent || labels[j]!=0;});
        if(adjacent) enqueue(i);
    }
    std::size_t processed=0;
    while(!heap.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);
        std::pop_heap(heap.begin(),heap.end(),later);const auto i=heap.back().index;heap.pop_back();
        std::uint32_t owner=0;bool collision=false;
        four(i,[&](std::size_t j){if(labels[j]) {if(owner && owner!=labels[j]) collision=true;else owner=labels[j];}});
        if(!owner || collision) continue;
        labels[i]=owner;four(i,enqueue);
    }
    checkpoint(cancelled);return labels;
}
}
