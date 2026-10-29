#pragma once
#include "scientific_cell_numeric.hpp"
#include "log_watershed.hpp"
#include "cell_expansion.hpp"

namespace qpx::cell_detail {
// Negative discrete Laplacian in the captured metric after float64 Gaussian
// smoothing. Missing neighbours use the current center (zero normal flux).
inline std::vector<std::uint8_t> grid_log_positive(std::vector<double> score,const std::vector<std::uint8_t>& valid,
    std::size_t width,std::size_t height,const PhysicalGridMetric& metric,double sigma,std::size_t budget,
    const std::atomic<bool>* cancelled) {
    physical_gaussian(score,valid,width,height,metric,sigma,budget,cancelled);
    const auto xx=metric.x_um()*metric.x_um(),yy=metric.y_um()*metric.y_um();
    std::vector<std::uint8_t> positive(score.size());
    for(std::size_t y=0;y<height;++y) {
        checkpoint(cancelled);
        for(std::size_t x=0;x<width;++x) {
            const auto i=y*width+x;if(!valid[i]) continue;
            auto value=[&](std::size_t j){return valid[j]?score[j]:score[i];};
            const auto laplacian=((score[i]-value(y*width+(x?x-1:x)))+(score[i]-value(y*width+(x+1<width?x+1:x))))/xx+
                ((score[i]-value((y?y-1:y)*width+x))+(score[i]-value((y+1<height?y+1:y)*width+x)))/yy;
            if(!std::isfinite(laplacian)) throw std::overflow_error("grid LoG response overflow");
            positive[i]=laplacian>0;
        }
    }
    return positive;
}
inline LogResult grid_log_watershed(const std::vector<double>& score,const std::vector<std::uint8_t>& valid,
    const std::vector<std::uint8_t>& domain,std::size_t width,std::size_t height,const PhysicalGridMetric& metric,
    double sigma,const LogParameters& p,const std::atomic<bool>* cancelled,std::atomic<int>* phase,
    const std::vector<std::uint8_t>* supplied_positive=nullptr) {
    p.validate();checkpoint(cancelled);
    const auto n=scientific_cell_pixels(width,height);
    if(score.size()!=n || valid.size()!=n || domain.size()!=n) throw std::invalid_argument("invalid grid LoG buffers");
    scientific_cell_budget(128*n+65536,p.work_bytes);
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(!std::isfinite(score[i]) || domain[i]>1 || valid[i]>1 || (domain[i] && !valid[i])) throw std::invalid_argument("invalid grid LoG values or domain");
    }
    if(supplied_positive && supplied_positive->size()!=n) throw std::invalid_argument("invalid supplied grid LoG mask");
    auto positive=supplied_positive?*supplied_positive:grid_log_positive(score,valid,width,height,metric,sigma,p.work_bytes,cancelled);
    if(supplied_positive) for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(positive[i]>1 || (positive[i] && !valid[i])) throw std::invalid_argument("invalid supplied grid LoG support");
    }
    auto mask=log_regions_from_positive(score,positive,width,height,p,cancelled);
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);mask[i]=mask[i] && valid[i];}
    std::vector<std::uint8_t>().swap(positive);
    if(phase) phase->store(3);
    std::vector<double> distance(n);for(std::size_t i=0;i<n;++i) distance[i]=mask[i]?std::numeric_limits<double>::infinity():0;
    physical_distance(distance,width,height,metric,true,nullptr,p.work_bytes,cancelled);
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);distance[i]=std::sqrt(distance[i]);}
    return log_split_regions(score,domain,mask,distance,width,height,p,cancelled,phase,metric.x_um(),metric.y_um());
}
// Double squared-distance priorities and stable FIFO ties. No sampled-integer
// buckets, float32 priorities or one-pixel shortcut are valid for this metric.
inline BodyExpansion expand_grid_cells(const std::vector<std::uint32_t>& nuclei,const std::vector<std::uint8_t>& domain,
    std::size_t width,std::size_t height,const PhysicalGridMetric& metric,double radius,std::size_t budget,
    const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);const auto n=scientific_cell_pixels(width,height);
    scientific_cell_budget(64*n+40ULL*std::max(width,height)+4096,budget);
    if(nuclei.size()!=n || domain.size()!=n || !std::isfinite(radius) || radius<0 || radius>1000000) throw std::invalid_argument("invalid grid expansion input");
    bool seeded=false;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(domain[i]>1 || (nuclei[i] && !domain[i])) throw std::invalid_argument("grid expansion seed outside domain");
        seeded=seeded || nuclei[i]!=0;
    }
    BodyExpansion result;result.labels=nuclei;if(!seeded || radius==0) return result;
    std::vector<double> distance(n);for(std::size_t i=0;i<n;++i) distance[i]=nuclei[i]?0:std::numeric_limits<double>::infinity();
    physical_distance(distance,width,height,metric,false,nullptr,budget,cancelled);
    struct Front {double cost;std::uint64_t sequence;std::uint32_t index;};
    auto later=[](const Front& a,const Front& b){return std::tie(a.cost,a.sequence)>std::tie(b.cost,b.sequence);};
    std::vector<Front> heap;heap.reserve(n);std::vector<std::uint8_t> visited(n);std::uint64_t sequence=0;
    auto enqueue=[&](std::size_t i) {
        if(visited[i]) return;visited[i]=1;
        heap.push_back({distance[i],sequence++,static_cast<std::uint32_t>(i)});std::push_heap(heap.begin(),heap.end(),later);
        ++result.queued_pixels;result.peak_queue=std::max(result.peak_queue,heap.size());
    };
    const auto squared=radius*radius;
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);visited[i]=!domain[i] || nuclei[i] || !(distance[i]<squared);}
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);if(visited[i]) continue;bool adjacent=false;
        expansion_neighbors(i,width,height,[&](std::size_t j){adjacent=adjacent || nuclei[j]!=0;});if(adjacent) enqueue(i);
    }
    std::size_t processed=0;
    while(!heap.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);
        std::pop_heap(heap.begin(),heap.end(),later);const auto i=heap.back().index;heap.pop_back();
        std::uint32_t owner=0;bool collision=false;
        expansion_neighbors(i,width,height,[&](std::size_t j){const auto label=result.labels[j];if(label) {if(owner && owner!=label) collision=true;else owner=label;}});
        if(!owner || collision) continue;result.labels[i]=owner;expansion_neighbors(i,width,height,enqueue);
    }
    checkpoint(cancelled);return result;
}
}
