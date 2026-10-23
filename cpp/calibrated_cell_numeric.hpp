#pragma once
#include <cmath>
#include <initializer_list>
#include <optional>
#include <stdexcept>

namespace qpx::detail {
inline void validate_cell_area_limits(std::optional<double> minimum,std::optional<double> maximum) {
    for(const auto& value:{minimum,maximum}) if(value && (!std::isfinite(*value) || *value<0))
        throw std::invalid_argument("physical nucleus area bounds must be finite and nonnegative");
    if(minimum && maximum && *minimum>*maximum) throw std::invalid_argument("invalid physical nucleus area range");
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
}
