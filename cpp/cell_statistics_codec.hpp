#pragma once
#include "qupathxx/cells.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qpx::cell_detail {
inline void validate_cell_statistics(const CellIntensityStatistics& stats,const CellFeature& feature) {
    auto check=[&](const CompartmentIntensityStatistics& compartment,std::uint64_t count,const ColorVector& rgb_mean,const ColorVector& channel_mean) {
        if(!count || compartment.samples!=count) throw std::invalid_argument("cell intensity statistics count differs");
        auto channel=[&](const ChannelIntensityStatistics& values,const ColorVector& mean,bool rgb) {
            if(bool(values.sample_standard_deviation)!=(count>1)) throw std::invalid_argument("cell intensity statistics SD availability differs");
            for(std::size_t c=0;c<3;++c) {
                const auto lo=values.minimum[c],hi=values.maximum[c],sum=values.sum[c];
                const auto tolerance=1e-12*(1+std::abs(lo)+std::abs(hi)+std::abs(mean[c]));
                if(!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(sum) || lo>hi ||
                   (rgb && (lo<0 || hi>255)) || mean[c]<lo-tolerance || mean[c]>hi+tolerance ||
                   std::abs(sum-mean[c]*double(count))>1e-12*(1+std::abs(sum)+std::abs(mean[c]*double(count))))
                    throw std::invalid_argument("invalid cell intensity statistics extrema or sum");
                if(values.sample_standard_deviation) {
                    const auto sd=(*values.sample_standard_deviation)[c];
                    if(!std::isfinite(sd) || sd<0 || (lo==hi && sd!=0) ||
                       sd>(hi-lo)*std::sqrt(double(count)/double(count-1))+tolerance)
                        throw std::invalid_argument("invalid cell intensity statistics deviation");
                }
            }
        };
        channel(compartment.rgb,rgb_mean,true);channel(compartment.channels,channel_mean,false);
    };
    if(feature.cell_pixels<feature.nucleus_pixels) throw std::invalid_argument("invalid cell statistics compartment counts");
    check(stats.nucleus,feature.nucleus_pixels,feature.nucleus_rgb,feature.nucleus_channels);
    check(stats.cell,feature.cell_pixels,feature.cell_rgb,feature.cell_channels);
    const auto cyto=feature.cell_pixels-feature.nucleus_pixels;
    if(bool(stats.cytoplasm)!=(cyto!=0) || bool(feature.cytoplasm_rgb)!=(cyto!=0) || bool(feature.cytoplasm_channels)!=(cyto!=0))
        throw std::invalid_argument("cell intensity statistics cytoplasm availability differs");
    if(cyto) check(*stats.cytoplasm,cyto,*feature.cytoplasm_rgb,*feature.cytoplasm_channels);
}
template<class Writer> void write_cell_statistics(Writer& w,const CellIntensityStatistics& stats,const CellFeature& feature) {
    validate_cell_statistics(stats,feature);
    auto compartment=[&](const auto& value) {
        for(const auto* channel:{&value.rgb,&value.channels}) {
            for(const auto* vector:{&channel->sum,&channel->minimum,&channel->maximum}) for(auto v:*vector) w.real(v);
            if(channel->sample_standard_deviation) for(auto v:*channel->sample_standard_deviation) w.real(v);
        }
    };
    compartment(stats.nucleus);compartment(stats.cell);if(stats.cytoplasm) compartment(*stats.cytoplasm);
}
template<class Reader> CellIntensityStatistics read_cell_statistics(Reader& r,const CellFeature& feature) {
    CellIntensityStatistics stats;
    auto compartment=[&](auto& value,std::uint64_t count) {
        value.samples=count;
        for(auto* channel:{&value.rgb,&value.channels}) {
            for(auto* vector:{&channel->sum,&channel->minimum,&channel->maximum}) for(auto& v:*vector) v=r.real();
            if(count>1) {channel->sample_standard_deviation=ColorVector{};for(auto& v:*channel->sample_standard_deviation) v=r.real();}
        }
    };
    compartment(stats.nucleus,feature.nucleus_pixels);compartment(stats.cell,feature.cell_pixels);
    if(feature.cell_pixels>feature.nucleus_pixels) {stats.cytoplasm=CompartmentIntensityStatistics{};compartment(*stats.cytoplasm,feature.cell_pixels-feature.nucleus_pixels);}
    validate_cell_statistics(stats,feature);return stats;
}
}
