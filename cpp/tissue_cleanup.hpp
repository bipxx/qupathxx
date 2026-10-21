#pragma once
#include "qupathxx/tissue.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
namespace qpx::tissue_detail {
inline bool enabled(const TissueParameters& p) {return p.open_radius || p.close_radius || p.fill_holes;}
inline void checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("tissue detection cancelled");
}
constexpr auto foreground=std::numeric_limits<std::uint32_t>::max();
// One exact square primitive: separable sliding counts, constant-zero padding.
// Workspace is the same reserved uint32 buffer later used by flood filling.
inline void square(std::vector<std::uint32_t>& labels,std::vector<std::uint32_t>& workspace,
    const std::vector<std::uint8_t>& domain,std::int64_t width,std::int64_t height,int radius,bool dilate,const std::atomic<bool>* cancelled) {
    workspace.resize(labels.size());const auto span=static_cast<std::uint32_t>(2*radius+1);
    for(std::int64_t y=0;y<height;++y) {
        checkpoint(cancelled);const auto base=static_cast<std::size_t>(y*width);std::uint32_t sum=0;
        for(std::int64_t x=0;x<=radius && x<width;++x) sum+=labels[base+x]!=0;
        for(std::int64_t x=0;x<width;++x) {
            if((x&4095)==0) checkpoint(cancelled);
            workspace[base+x]=(dilate?sum!=0:sum==span)?foreground:0;
            if(x>=radius) sum-=labels[base+x-radius]!=0;
            if(x+radius+1<width) sum+=labels[base+x+radius+1]!=0;
        }
    }
    // Block columns so vertical reads/writes stay contiguous within cache lines.
    for(std::int64_t block=0;block<width;block+=64) {
        checkpoint(cancelled);const auto columns=std::min<std::int64_t>(64,width-block);
        std::array<std::uint32_t,64> sums{};
        for(std::int64_t y=0;y<=radius && y<height;++y)
            for(std::int64_t x=0;x<columns;++x) sums[x]+=workspace[static_cast<std::size_t>(y*width+block+x)]!=0;
        for(std::int64_t y=0;y<height;++y) {
            if((y&63)==0) checkpoint(cancelled);
            for(std::int64_t x=0;x<columns;++x) {
                const auto i=static_cast<std::size_t>(y*width+block+x);
                labels[i]=domain[i] && (dilate?sums[x]!=0:sums[x]==span)?foreground:0;
                if(y>=radius) sums[x]-=workspace[static_cast<std::size_t>((y-radius)*width+block+x)]!=0;
                if(y+radius+1<height) sums[x]+=workspace[static_cast<std::size_t>((y+radius+1)*width+block+x)]!=0;
            }
        }
    }
}
inline std::uint64_t fill(std::vector<std::uint32_t>& labels,std::vector<std::uint32_t>& queue,
    std::vector<std::uint8_t>& domain,std::int64_t width,std::int64_t height,int foreground_connectivity,
    std::uint64_t maximum,const std::atomic<bool>* cancelled) {
    std::uint64_t filled=0;const int background_connectivity=foreground_connectivity==8?4:8;
    for(std::size_t seed=0;seed<labels.size();++seed) {
        if((seed&4095)==0) checkpoint(cancelled);
        if(labels[seed] || domain[seed]!=1) continue;
        queue.clear();queue.push_back(static_cast<std::uint32_t>(seed));domain[seed]=2;bool exterior=false;
        for(std::size_t cursor=0;cursor<queue.size();++cursor) {
            if((cursor&4095)==0) checkpoint(cancelled);
            const auto row=queue[cursor]/width,col=queue[cursor]%width;
            if(!row || !col || row==height-1 || col==width-1) exterior=true;
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                if((!dx && !dy) || (background_connectivity==4 && dx && dy)) continue;
                const auto x=col+dx,y=row+dy;if(x<0 || y<0 || x>=width || y>=height) continue;
                const auto next=static_cast<std::size_t>(y*width+x);
                if(!domain[next]) exterior=true;
                else if(!labels[next] && domain[next]==1) {domain[next]=2;queue.push_back(static_cast<std::uint32_t>(next));}
            }
        }
        if(exterior || (maximum && queue.size()>maximum)) continue;
        filled+=queue.size();
        for(std::size_t i=0;i<queue.size();++i) {if((i&4095)==0) checkpoint(cancelled);labels[queue[i]]=foreground;}
    }
    return filled;
}
}
