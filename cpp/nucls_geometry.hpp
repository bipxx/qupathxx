#pragma once
#include "qupathxx/geometry.hpp"
#include <map>
namespace qpx::detail {
struct NuclsGeometry {Roi outline;Point centroid;std::size_t vertices=0;};
NuclsGeometry nucls_geometry(const std::map<std::string,std::string>&);
double nucls_accurate_sum(const std::vector<double>&);
}
