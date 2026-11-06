#pragma once
#include "qupathxx/spatial_density.hpp"
namespace qpx {
struct SpatialDensityRequest::Data {
    SpatialDistanceRequest capture;
    SpatialDensityOptions options;
    std::vector<SpatialDensityAreaSnapshot> windows,tissue,excluded;
    std::size_t bytes=0,work=0;
    double capture_ms=0;
    std::string digest;
    Data(SpatialDistanceRequest value,SpatialDensityOptions parameters):capture(std::move(value)),options(parameters) {}
    std::string input_digest() const;
    bool same_geometry(const Data&) const;
    std::size_t work_bound() const;
};
}
