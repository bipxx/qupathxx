#pragma once
#include "qupathxx/cells.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace qpx::cell_detail {
inline const char* algorithm_id(CellAlgorithm algorithm) {
    switch(algorithm) {
    case CellAlgorithm::DistanceWatershed:return "qpx.distance-watershed/1";
    case CellAlgorithm::LogWatershed:return "qpx.log-watershed/1";
    case CellAlgorithm::LogWatershedBodies:return "qpx.log-watershed-bodies/1";
    }
    throw std::invalid_argument("unknown cell algorithm");
}
inline const char* input_format(CellAlgorithm algorithm,bool pyramid) {
    if(algorithm==CellAlgorithm::LogWatershedBodies) return pyramid?"qpx.log-bodies-rgba-level-tiles/1":"qpx.log-bodies-rgba-tiles/1";
    if(algorithm==CellAlgorithm::LogWatershed) return pyramid?"qpx.log-rgba-level-tiles/1":"qpx.log-rgba-tiles/1";
    if(algorithm==CellAlgorithm::DistanceWatershed) return pyramid?"qpx.rgba-level-tiles/1":"qpx.rgba-tiles/1";
    throw std::invalid_argument("unknown cell algorithm");
}
inline const char* mask_algorithm(CellAlgorithm algorithm,bool body) {
    if(algorithm==CellAlgorithm::LogWatershedBodies) return body?"qpx.log-watershed-cell-bodies/1":"qpx.log-bodies-cell-nuclei/1";
    if(algorithm==CellAlgorithm::LogWatershed) return body?"qpx.log-cell-bodies/1":"qpx.log-cell-nuclei/1";
    if(algorithm==CellAlgorithm::DistanceWatershed) return body?"qpx.cell-bodies/1":"qpx.cell-nuclei/1";
    throw std::invalid_argument("unknown cell algorithm");
}
inline Rect processing_grid(Rect roi,const ImageLevel& level,const CellParameters& parameters) {
    if(parameters.algorithm==CellAlgorithm::DistanceWatershed) return roi;
    const auto padding=static_cast<std::int64_t>(parameters.context_padding);
    const auto x=static_cast<std::int64_t>(roi.x),y=static_cast<std::int64_t>(roi.y);
    const auto right=x+static_cast<std::int64_t>(roi.width),bottom=y+static_cast<std::int64_t>(roi.height);
    const auto left=std::max<std::int64_t>(0,x-padding),top=std::max<std::int64_t>(0,y-padding);
    return {double(left),double(top),double(right+std::min(padding,level.width-right)-left),double(bottom+std::min(padding,level.height-bottom)-top)};
}
inline std::uint64_t work_base(Rect grid,const CellParameters& p) {
    const auto width=static_cast<std::uint64_t>(grid.width),height=static_cast<std::uint64_t>(grid.height);
    if(!width || !height || width>std::numeric_limits<std::uint32_t>::max()/height) throw std::length_error("cell context grid exceeds index range");
    const auto n=width*height;
    return (p.algorithm!=CellAlgorithm::DistanceWatershed?160ull:80ull)*n+40ull*std::max(width,height)+1024ull*std::min<std::uint64_t>(n,p.max_cells)+16ull*p.tile_size*p.tile_size+65536;
}
}
