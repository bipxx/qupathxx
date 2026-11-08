#pragma once
#include "qupathxx/stains.hpp"
#include "stain_matrix.hpp"

namespace qpx::detail {
// Shared float64 arithmetic for area-RGB and explicit nominal-RGB cell input.
inline void scientific_channel_values(const std::vector<double>& rgb,const std::vector<std::uint8_t>& valid,
    const StainTransform& transform,bool stains,std::vector<double>& output,const std::atomic<bool>* cancelled) {
    const auto inverse=stain_inverse(transform.stains());
    for(std::size_t i=0;i<valid.size();++i) {
        if((i&4095)==0 && cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("scientific channel transform cancelled");
        if(!valid[i]) continue;ColorVector od;
        for(unsigned c=0;c<3;++c) od[c]=optical_density(rgb[i*3+c],transform.white()[c]);
        if(stains) for(unsigned stain=0;stain<3;++stain) {
            const auto r=od[0]*inverse.cofactors[stain][0]/inverse.determinant;
            const auto g=od[1]*inverse.cofactors[stain][1]/inverse.determinant;
            const auto b=od[2]*inverse.cofactors[stain][2]/inverse.determinant;
            const auto value=(r+g)+b;
            if(!std::isfinite(value)) throw std::overflow_error("scientific stain channel is not representable");
            output[i*3+stain]=value;
        } else for(unsigned c=0;c<3;++c) output[i*3+c]=od[c];
    }
}
}
