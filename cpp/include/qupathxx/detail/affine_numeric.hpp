// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace qpx::detail {
// Row-major [a,b,tx,c,d,ty].
inline std::array<double, 2> affine_map_xy(const std::array<double, 6>& m, double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        throw std::invalid_argument("Nonfinite coordinate");
    const std::array<double, 2> result{
        std::fma(m[0], x, std::fma(m[1], y, m[2])),
        std::fma(m[3], x, std::fma(m[4], y, m[5]))};
    if (!std::isfinite(result[0]) || !std::isfinite(result[1]))
        throw std::invalid_argument("Affine coordinate overflow");
    return result;
}

inline std::array<double, 4> affine_inverse_linear(const std::array<double, 6>& m) {
    for (auto value : m)
        if (!std::isfinite(value))
            throw std::invalid_argument("Nonfinite affine coefficient");
    const auto scale = std::max({std::abs(m[0]), std::abs(m[1]), std::abs(m[3]), std::abs(m[4])});
    if (!scale)
        throw std::invalid_argument("Singular affine transform");
    const auto a = m[0] / scale, b = m[1] / scale, c = m[3] / scale, d = m[4] / scale;
    const auto determinant = a * d - b * c;
    if (!determinant)
        throw std::invalid_argument("Singular affine transform");
    const auto norm = std::max(std::abs(a) + std::abs(b), std::abs(c) + std::abs(d));
    const auto inv_norm = std::max(std::abs(d) + std::abs(b), std::abs(c) + std::abs(a)) / std::abs(determinant);
    if (!std::isfinite(inv_norm) || norm * inv_norm > 1e12)
        throw std::invalid_argument("Ill-conditioned affine transform");
    const std::array<double, 4> inverse{
        d / determinant / scale, -b / determinant / scale, -c / determinant / scale, a / determinant / scale};
    for (auto value : inverse)
        if (!std::isfinite(value))
            throw std::invalid_argument("Affine inverse overflows");
    return inverse;
}
} // namespace qpx::detail
