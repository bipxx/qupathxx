#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace qpx::detail {
template<class Checkpoint> void masked_gaussian(std::vector<double>& values,const std::vector<std::uint8_t>& domain,std::size_t width,std::size_t height,double sigma,Checkpoint checkpoint) {
    if(sigma==0) return;const auto radius=static_cast<int>(std::ceil(3*sigma));std::vector<double> kernel(2*radius+1);
    for(int i=-radius;i<=radius;++i) {const auto scaled=double(i)/sigma;kernel[i+radius]=i==0?1:std::exp(-.5*scaled*scaled);}
    std::vector<double> weights(domain.begin(),domain.end()),temporary(values.size()),temporary_weights(values.size());
    for(std::size_t y=0;y<height;++y) {
        checkpoint();
        for(std::size_t x=0;x<width;++x) {
            if((x&4095)==0) checkpoint();double sum=0,weight=0;
            const auto first=-static_cast<int>(std::min<std::size_t>(radius,x));
            const auto last=static_cast<int>(std::min<std::size_t>(radius,width-1-x));
            const auto* source=values.data()+y*width+x;
            const auto* source_weights=weights.data()+y*width+x;
            for(int d=first;d<=last;++d) {
                sum+=source[d]*kernel[d+radius];weight+=source_weights[d]*kernel[d+radius];
            }
            temporary[y*width+x]=sum;temporary_weights[y*width+x]=weight;
        }
    }
    values.swap(temporary);weights.swap(temporary_weights);
    // Accumulate short contiguous row spans instead of a strided dot product
    // per output pixel. Each output still adds offsets in ascending order;
    // truncation, normalization and floating-point summation order are unchanged.
    constexpr std::size_t block_width=256;
    for(std::size_t y=0;y<height;++y) {
        checkpoint();
        const auto first=-static_cast<int>(std::min<std::size_t>(radius,y));
        const auto last=static_cast<int>(std::min<std::size_t>(radius,height-1-y));
        auto* output=temporary.data()+y*width;
        auto* output_weights=temporary_weights.data()+y*width;
        if(radius<=6) {
            // Short kernels avoid repeated output-array traffic and loop setup.
            for(std::size_t x=0;x<width;++x) {
                if((x&4095)==0) checkpoint();double sum=0,weight=0;
                const auto* source=values.data()+y*width+x;
                const auto* source_weights=weights.data()+y*width+x;
                for(int d=first;d<=last;++d) {
                    const auto offset=static_cast<std::int64_t>(d)*static_cast<std::int64_t>(width);
                    sum+=source[offset]*kernel[d+radius];weight+=source_weights[offset]*kernel[d+radius];
                }
                output[x]=sum;output_weights[x]=weight;
            }
            continue;
        }
        std::fill_n(output,width,0.);std::fill_n(output_weights,width,0.);
        for(std::size_t block=0;block<width;block+=block_width) {
            checkpoint();const auto end=std::min(width,block+block_width);
            for(int d=first;d<=last;++d) {
                const auto row=static_cast<std::size_t>(static_cast<std::int64_t>(y)+d)*width;
                const auto* source=values.data()+row;
                const auto* source_weights=weights.data()+row;
                const auto coefficient=kernel[d+radius];
                for(std::size_t x=block;x<end;++x) {
                    output[x]+=source[x]*coefficient;output_weights[x]+=source_weights[x]*coefficient;
                }
            }
        }
    }
    values.swap(temporary);weights.swap(temporary_weights);
    for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0) checkpoint();values[i]=weights[i]>0?values[i]/weights[i]:0;}
}
}
