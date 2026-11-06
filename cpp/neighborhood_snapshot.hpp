#pragma once
#include "qupathxx/spatial_neighborhood.hpp"
namespace qpx {
struct SpatialNeighborhoodRequest::Data {
    SpatialDistanceRequest capture;
    SpatialNeighborhoodOptions options;
    std::size_t bytes=0,work=0;
    std::string digest;
    Data(SpatialDistanceRequest value,SpatialNeighborhoodOptions parameters):capture(std::move(value)),options(parameters) {}
};
}
