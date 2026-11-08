#pragma once
#include "qupathxx/cell_statistics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
namespace qpx::detail {
struct IntensityMoment {
    double sum=0,correction=0,mean=0,m2=0,minimum=0,maximum=0;
    void add(double value,std::uint64_t count,const char* context="cell intensity statistics") {
        if(!std::isfinite(value)) throw std::invalid_argument(std::string(context)+" require finite foreground samples");
        if(count==1) minimum=maximum=value;
        else {minimum=std::min(minimum,value);maximum=std::max(maximum,value);}
        const auto adjusted=value-correction,next=sum+adjusted;
        correction=(next-sum)-adjusted;sum=next;
        const auto delta=value-mean;mean+=delta/double(count);m2+=delta*(value-mean);
        if(!std::isfinite(sum) || !std::isfinite(mean) || !std::isfinite(m2))
            throw std::overflow_error(std::string(context)+" exceed finite numeric range");
    }
};
struct IntensityCompartment {
    std::uint64_t count=0;
    std::array<IntensityMoment,3> rgb,channels;
    void add(const ColorVector& colors,const ColorVector& transformed,const char* context="cell intensity statistics") {
        ++count;
        for(std::size_t c=0;c<3;++c) {rgb[c].add(colors[c],count,context);channels[c].add(transformed[c],count,context);}
    }
    CompartmentIntensityStatistics finish() const {
        if(!count) throw std::logic_error("empty compartment has no intensity statistics");
        CompartmentIntensityStatistics result;result.samples=count;
        auto copy=[&](const auto& moments,auto& output) {
            if(count>1) output.sample_standard_deviation=ColorVector{};
            for(std::size_t c=0;c<3;++c) {
                const auto& m=moments[c];output.sum[c]=m.sum;output.minimum[c]=m.minimum;output.maximum[c]=m.maximum;
                if(count>1) (*output.sample_standard_deviation)[c]=std::sqrt(std::max(0.,m.m2)/double(count-1));
            }
        };
        copy(rgb,result.rgb);copy(channels,result.channels);return result;
    }
};
}
