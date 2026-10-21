#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
namespace qpx::cell_detail {
inline void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("cell detection cancelled");}
constexpr double infinity=1e30;
// Lower envelopes of sampled parabolas, independently implemented from the
// squared-distance formulation in Felzenszwalb & Huttenlocher (2012).
struct LineTransform {
    std::vector<double> input,output,breaks;
    std::vector<std::uint32_t> sites,nearest;
    explicit LineTransform(std::size_t n):input(n),output(n),breaks(n+1),sites(n),nearest(n) {}
    void run(std::size_t n,const std::atomic<bool>* cancelled) {
        std::size_t used=0;
        for(std::size_t q=0;q<n;++q) {
            if((q&4095)==0) checkpoint(cancelled);if(input[q]>=infinity) continue;
            double crossing=-infinity;
            while(used) {
                const auto v=sites[used-1];
                crossing=((input[q]+double(q)*double(q))-(input[v]+double(v)*double(v)))/(2*(double(q)-double(v)));
                if(crossing>breaks[used-1]) break;--used;
            }
            sites[used]=static_cast<std::uint32_t>(q);breaks[used]=used?crossing:-infinity;++used;breaks[used]=infinity;
        }
        std::size_t k=0;
        for(std::size_t q=0;q<n;++q) {
            if((q&4095)==0) checkpoint(cancelled);
            if(!used) {output[q]=infinity;nearest[q]=std::numeric_limits<std::uint32_t>::max();continue;}
            while(k+1<used && breaks[k+1]<double(q)) ++k;
            nearest[q]=sites[k];const auto dx=double(q)-double(sites[k]);output[q]=dx*dx+input[sites[k]];
        }
    }
};
// In-place separable transform. Optional argument propagation identifies the
// nearest original site; ties prefer lower row, then lower column.
inline void distance(std::vector<double>& values,std::size_t width,std::size_t height,bool outside,
    std::vector<std::uint32_t>* nearest,const std::atomic<bool>* cancelled) {
    LineTransform line(std::max(width,height));
    if(nearest) nearest->resize(values.size());
    for(std::size_t y=0;y<height;++y) {
        checkpoint(cancelled);for(std::size_t x=0;x<width;++x) line.input[x]=values[y*width+x];line.run(width,cancelled);
        for(std::size_t x=0;x<width;++x) {
            const auto border=std::min(double(x+1)*double(x+1),double(width-x)*double(width-x));
            values[y*width+x]=outside?std::min(border,line.output[x]):line.output[x];
            if(nearest) (*nearest)[y*width+x]=line.nearest[x]==std::numeric_limits<std::uint32_t>::max()?line.nearest[x]:static_cast<std::uint32_t>(y*width+line.nearest[x]);
        }
    }
    std::vector<std::uint32_t> column(nearest?height:0);
    for(std::size_t x=0;x<width;++x) {
        checkpoint(cancelled);for(std::size_t y=0;y<height;++y) {line.input[y]=values[y*width+x];if(nearest) column[y]=(*nearest)[y*width+x];}
        line.run(height,cancelled);
        for(std::size_t y=0;y<height;++y) {
            const auto border=std::min(double(y+1)*double(y+1),double(height-y)*double(height-y));
            values[y*width+x]=outside?std::min(border,line.output[y]):line.output[y];
            if(nearest) (*nearest)[y*width+x]=line.nearest[y]==std::numeric_limits<std::uint32_t>::max()?line.nearest[y]:column[line.nearest[y]];
        }
    }
}
inline void gaussian(std::vector<double>& values,const std::vector<std::uint8_t>& domain,std::size_t width,std::size_t height,double sigma,const std::atomic<bool>* cancelled) {
    if(sigma==0) return;const auto radius=static_cast<int>(std::ceil(3*sigma));std::vector<double> kernel(2*radius+1);
    for(int i=-radius;i<=radius;++i) {const auto scaled=double(i)/sigma;kernel[i+radius]=i==0?1:std::exp(-.5*scaled*scaled);}
    std::vector<double> weights(domain.begin(),domain.end()),temporary(values.size()),temporary_weights(values.size());
    for(int axis=0;axis<2;++axis) {
        for(std::size_t y=0;y<height;++y) {
            checkpoint(cancelled);
            for(std::size_t x=0;x<width;++x) {
                if((x&4095)==0) checkpoint(cancelled);double sum=0,weight=0;
                for(int d=-radius;d<=radius;++d) {
                    const auto xx=std::int64_t(x)+(axis==0?d:0),yy=std::int64_t(y)+(axis==1?d:0);
                    if(xx<0 || yy<0 || xx>=std::int64_t(width) || yy>=std::int64_t(height)) continue;
                    const auto index=static_cast<std::size_t>(yy)*width+static_cast<std::size_t>(xx);sum+=values[index]*kernel[d+radius];weight+=weights[index]*kernel[d+radius];
                }
                temporary[y*width+x]=sum;temporary_weights[y*width+x]=weight;
            }
        }
        values.swap(temporary);weights.swap(temporary_weights);
    }
    for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0) checkpoint(cancelled);values[i]=weights[i]>0?values[i]/weights[i]:0;}
}
}
