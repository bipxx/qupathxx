#pragma once
#include "qupathxx/stains.hpp"
#include <cmath>
#include <stdexcept>
namespace qpx::detail {
inline ColorVector stain_cross(const ColorVector& a,const ColorVector& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
inline double stain_dot(const ColorVector& a,const ColorVector& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
struct StainInverse {std::array<ColorVector,3> cofactors;double determinant;};
inline StainInverse stain_inverse(const std::array<ColorVector,3>& basis) {
    const auto r0=stain_cross(basis[1],basis[2]),r1=stain_cross(basis[2],basis[0]),r2=stain_cross(basis[0],basis[1]);
    const auto determinant=stain_dot(basis[0],r0);
    if(!std::isfinite(determinant) || std::abs(determinant)<1e-8) throw std::invalid_argument("stain basis is singular or nearly singular");
    return {{r0,r1,r2},determinant};
}
}
