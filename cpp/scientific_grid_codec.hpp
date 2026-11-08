#pragma once
#include "qupathxx/scientific_grid.hpp"
#include <array>
#include <cstring>
namespace qpx {
// Frozen ten-integer area-grid prefix shared by cell and tissue archives.
// Signed coordinate bit patterns use the existing little-endian integer codec.
template<class Writer> void scientific_grid_write(Writer& w,const ScientificGrid& g) {
    for(auto v:{g.image_width(),g.image_height(),g.x().origin(),g.y().origin(),g.x().extent(),g.y().extent(),g.x().samples(),g.y().samples(),g.x().context(),g.y().context()}) w.integer(std::uint64_t(v));
}
template<class Reader> ScientificGrid scientific_grid_read(Reader& r,std::size_t work_bytes) {
    std::array<std::int64_t,10> v;
    for(auto& n:v) {const auto bits=r.integer();std::memcpy(&n,&bits,sizeof(n));}
    return ScientificGrid(v[0],v[1],{v[2],v[4],v[6],v[8]},{v[3],v[5],v[7],v[9]},work_bytes);
}
inline bool scientific_grid_equal(const ScientificGrid& a,const ScientificGrid& b) {
    auto axis=[](const ScientificGridAxis& x,const ScientificGridAxis& y) {
        return x.origin()==y.origin() && x.extent()==y.extent() && x.samples()==y.samples() && x.context()==y.context();
    };
    return a.image_width()==b.image_width() && a.image_height()==b.image_height() && axis(a.x(),b.x()) && axis(a.y(),b.y());
}
}
