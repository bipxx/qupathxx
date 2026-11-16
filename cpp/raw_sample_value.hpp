#pragma once
#include <cstdint>
#include <stdexcept>
#include <type_traits>
namespace qpx::detail {
// Shared exact admission for a raw sample entering a binary64 reducer/detector.
// This does not change or round the original typed storage.
template <class T> double exact_raw_binary64(T x) {
    if constexpr (std::is_integral_v<T> && sizeof(T) == 8) {
        if constexpr (std::is_signed_v<T>) {
            if (x < -(std::int64_t(1) << 53) || x > (std::int64_t(1) << 53))
                throw std::invalid_argument(
                    "Raw cell intensity exceeds exact binary64 integer admission");
        } else if (x > (std::uint64_t(1) << 53))
            throw std::invalid_argument(
                "Raw cell intensity exceeds exact binary64 integer admission");
    }
    return double(x);
}
} // namespace qpx::detail
