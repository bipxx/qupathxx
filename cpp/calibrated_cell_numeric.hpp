#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <initializer_list>
#include <optional>
#include <stdexcept>

namespace qpx::detail {
inline void validate_cell_area_limits(std::optional<double> minimum,std::optional<double> maximum) {
    for(const auto& value:{minimum,maximum}) if(value && (!std::isfinite(*value) || *value<0))
        throw std::invalid_argument("physical nucleus area bounds must be finite and nonnegative");
    if(minimum && maximum && *minimum>*maximum) throw std::invalid_argument("invalid physical nucleus area range");
}
inline void validate_tissue_area_limits(std::optional<double> minimum,std::optional<double> maximum) {
    for(const auto& value:{minimum,maximum}) if(value && (!std::isfinite(*value) || *value<0))
        throw std::invalid_argument("physical tissue area bounds must be finite and nonnegative");
    if(minimum && maximum && *minimum>*maximum) throw std::invalid_argument("invalid physical tissue area range");
}
// Preserve the ordinary multiplication order. Normalize when intermediate
// overflow/underflow would lose a representable final value. A range-only caller
// can retain zero/infinity to compare against finite positive physical bounds.
inline double calibrated_cell_product(std::initializer_list<double> factors,bool allow_range=false) {
    double result=1;bool zero=false,subnormal=false;
    for(auto factor:factors) {
        if(!std::isfinite(factor) || factor<0) throw std::invalid_argument("invalid calibrated cell factor");
        zero=zero || factor==0;result*=factor;subnormal=subnormal || std::fpclassify(result)==FP_SUBNORMAL;
    }
    if(zero) return 0.;
    if(!std::isfinite(result) || result==0 || subnormal) {
        double mantissa=1;int exponent=0;
        for(auto factor:factors) {
            int part=0,adjust=0;mantissa*=std::frexp(factor,&part);
            mantissa=std::frexp(mantissa,&adjust);exponent+=part+adjust;
        }
        result=std::scalbn(mantissa,exponent);
    }
    if(!allow_range && (!std::isfinite(result) || result==0))
        throw std::overflow_error("calibrated cell feature is not representable");
    return result;
}
// Resolve inclusive physical bounds against the captured sampling lattice.
// Return count+1/0 for an impossible interval; zero remains the unlimited
// maximum sentinel in the existing pixel-count recipe.
template<class Area>
inline std::pair<std::uint64_t,std::uint64_t> calibrated_area_count_limits(
    std::uint64_t count,Area area_for_count,std::optional<double> minimum,
    std::optional<double> maximum,std::uint64_t pixel_min,std::uint64_t pixel_max) {
    if(count==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("sampling grid is not representable");
    auto first=[&](auto predicate) {
        std::uint64_t lo=1,hi=count+1;
        while(lo<hi) {const auto mid=lo+(hi-lo)/2;
            const auto area=area_for_count(mid);
            if(predicate(area)) hi=mid;else lo=mid+1;
        }
        return lo;
    };
    const auto lower=minimum?first([&](double area){return area>=*minimum;}):1;
    // Every nonempty component has positive mathematical area, even when the
    // final floating point product underflows. A zero maximum excludes all.
    const auto upper=maximum?(*maximum==0?0:first([&](double area){return area>*maximum;})-1):count;
    const auto effective_min=std::max(pixel_min,lower);
    const auto effective_max=std::min(pixel_max?pixel_max:count,upper);
    if(effective_min>effective_max) return {count+1,0};
    return {effective_min,upper<count && (!pixel_max || upper<pixel_max)?upper:pixel_max};
}

inline std::pair<std::uint64_t,std::uint64_t> calibrated_area_pixel_limits(
    std::uint64_t count,double scale,double x,double y,std::optional<double> minimum,
    std::optional<double> maximum,std::uint64_t pixel_min,std::uint64_t pixel_max) {
    return calibrated_area_count_limits(count,[&](std::uint64_t n) {
        return calibrated_cell_product({double(n),scale,scale,x,y},true);
    },minimum,maximum,pixel_min,pixel_max);
}

}
