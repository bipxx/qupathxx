#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <stdexcept>
namespace qpx::detail {
inline void raster_boundary_checkpoint(const std::atomic<bool>* flag) {
    if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("tissue contour conversion cancelled");
}
// Ordered adjacent-label pairs preserve junctions; equal labels emit no edge.
// Horizontal sides are north/south; vertical sides are west/east.
template<class Mask,class Emit> void for_each_label_boundary_run(const Mask& mask,Emit&& emit,const std::atomic<bool>* cancelled) {
    const auto w=mask.width(),h=mask.height();const auto& labels=mask.labels();
    for(std::int64_t y=0;y<=h;++y) {
        raster_boundary_checkpoint(cancelled);std::uint32_t a=0,b=0;std::int64_t start=0;
        for(std::int64_t x=0;x<=w;++x) {
            if((x&4095)==0) raster_boundary_checkpoint(cancelled);
            auto next_a=x<w && y?labels[static_cast<std::size_t>((y-1)*w+x)]:0;
            auto next_b=x<w && y<h?labels[static_cast<std::size_t>(y*w+x)]:0;
            if(next_a==next_b) next_a=next_b=0;
            if(a!=next_a || b!=next_b) {if(a || b) emit(start,y,x,y,a,b);start=x;a=next_a;b=next_b;}
        }
    }
    struct Active {std::uint32_t a=0,b=0;std::int64_t start=0;};
    for(std::int64_t block=0;block<=w;block+=64) {
        std::array<Active,64> active{};const auto columns=std::min<std::int64_t>(64,w+1-block);
        for(std::int64_t y=0;y<=h;++y) {
            if((y&63)==0) raster_boundary_checkpoint(cancelled);
            for(std::int64_t offset=0;offset<columns;++offset) {
                const auto x=block+offset;
                auto a=y<h && x?labels[static_cast<std::size_t>(y*w+x-1)]:0;
                auto b=y<h && x<w?labels[static_cast<std::size_t>(y*w+x)]:0;
                if(a==b) a=b=0;
                auto& prior=active[offset];
                if(prior.a!=a || prior.b!=b) {if(prior.a || prior.b) emit(x,prior.start,x,y,prior.a,prior.b);prior={a,b,y};}
            }
        }
    }
}
}
