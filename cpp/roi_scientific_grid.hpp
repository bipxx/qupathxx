#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/scientific_grid.hpp"
#include <cmath>
namespace qpx::detail {
inline ScientificGrid roi_scientific_grid(const ImageMetadata& metadata,const Roi& roi,std::uint32_t nx,std::uint32_t ny,std::uint32_t cx,std::uint32_t cy,std::size_t work) {
    if(!nx || !ny || nx>1000000 || ny>1000000 || cx>4096 || cy>4096)
        throw std::invalid_argument("ROI grid requires 1..1000000 samples and 0..4096 context per axis");
    const auto b=roi.bounds();constexpr auto limit=ScientificGridAxis::coordinate_limit;
    if(!roi.is_area() || metadata.width>limit || metadata.height>limit || b.x<-limit || b.y<-limit || b.x+b.width>limit || b.y+b.height>limit ||
       (b.x<0 || b.y<0 || b.x+b.width>metadata.width || b.y+b.height>metadata.height))
        throw std::invalid_argument("grid ROI must be an area within supported coordinates and inside its source");
    const auto x=std::int64_t(std::floor(b.x)),y=std::int64_t(std::floor(b.y));
    const auto right=std::int64_t(std::ceil(b.x+b.width)),bottom=std::int64_t(std::ceil(b.y+b.height));
    return ScientificGrid(metadata.width,metadata.height,{x,right-x,nx,cx},{y,bottom-y,ny,cy},work);
}
}
