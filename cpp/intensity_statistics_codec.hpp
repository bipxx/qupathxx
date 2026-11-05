#pragma once
#include "qupathxx/cell_statistics.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
namespace qpx::detail {
inline void validate_intensity_compartment(const CompartmentIntensityStatistics& compartment,std::uint64_t count,const ColorVector& rgb_mean,const ColorVector& channel_mean,const char* context="cell intensity statistics") {
        if(!count || compartment.samples!=count) throw std::invalid_argument(std::string(context)+" count differs");
        auto channel=[&](const ChannelIntensityStatistics& values,const ColorVector& mean,bool rgb) {
            if(bool(values.sample_standard_deviation)!=(count>1)) throw std::invalid_argument(std::string(context)+" SD availability differs");
            for(std::size_t c=0;c<3;++c) {
                const auto lo=values.minimum[c],hi=values.maximum[c],sum=values.sum[c];
                const auto tolerance=1e-12*(1+std::abs(lo)+std::abs(hi)+std::abs(mean[c]));
                if(!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(sum) || lo>hi ||
                   (rgb && (lo<0 || hi>255)) || mean[c]<lo-tolerance || mean[c]>hi+tolerance ||
                   std::abs(sum-mean[c]*double(count))>1e-12*(1+std::abs(sum)+std::abs(mean[c]*double(count))))
                    throw std::invalid_argument(std::string("invalid ")+context+" extrema or sum");
                if(values.sample_standard_deviation) {
                    const auto sd=(*values.sample_standard_deviation)[c];
                    if(!std::isfinite(sd) || sd<0 || (lo==hi && sd!=0) ||
                       sd>(hi-lo)*std::sqrt(double(count)/double(count-1))+tolerance)
                        throw std::invalid_argument(std::string("invalid ")+context+" deviation");
                }
            }
        };
        channel(compartment.rgb,rgb_mean,true);channel(compartment.channels,channel_mean,false);
}
template<class Writer> void write_intensity_compartment(Writer& w,const CompartmentIntensityStatistics& value) {
    for(const auto* channel:{&value.rgb,&value.channels}) {
        for(const auto* vector:{&channel->sum,&channel->minimum,&channel->maximum}) for(auto v:*vector) w.real(v);
        if(channel->sample_standard_deviation) for(auto v:*channel->sample_standard_deviation) w.real(v);
    }
}
template<class Reader> CompartmentIntensityStatistics read_intensity_compartment(Reader& r,std::uint64_t count) {
    CompartmentIntensityStatistics value;value.samples=count;
    for(auto* channel:{&value.rgb,&value.channels}) {
        for(auto* vector:{&channel->sum,&channel->minimum,&channel->maximum}) for(auto& v:*vector) v=r.real();
        if(count>1) {channel->sample_standard_deviation=ColorVector{};for(auto& v:*channel->sample_standard_deviation) v=r.real();}
    }
    return value;
}
}
