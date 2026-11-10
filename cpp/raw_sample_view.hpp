#pragma once
#include "qupathxx/raw_image.hpp"
#include <type_traits>
#include <stdexcept>
namespace qpx::detail {
// Resolve type dispatch once per block. Precision admission compares integers
// as integers, including on platforms whose long double has binary64 precision.
struct RawSampleView {
    const void* data=nullptr;
    double (*at)(const void*,std::size_t)=nullptr;
    explicit RawSampleView(const RawPixelBuffer& p){std::visit([&](const auto& values){
        using T=typename std::decay_t<decltype(values)>::value_type;data=values.data();
        at=[](const void* ptr,std::size_t i){const auto value=static_cast<const T*>(ptr)[i];
            if constexpr(std::is_integral_v<T> && sizeof(T)==8){constexpr std::uint64_t limit=std::uint64_t(1)<<53;
                bool loss=false;if constexpr(std::is_signed_v<T>)loss=value>std::int64_t(limit) || value< -std::int64_t(limit);else loss=value>limit;
                if(loss)throw std::overflow_error("Quantitative derived channels refuse integer precision loss beyond2^53");}
            return double(value);
        };
    },p.storage());}
};
}
