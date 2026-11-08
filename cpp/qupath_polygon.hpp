#pragma once
#include "qupathxx/geometry.hpp"
#include <atomic>
namespace qpx::detail {
// Only repeated-vertex walks are decomposed; every resulting topology is validated.
Roi qupath_saved_polygon(Ring, bool& normalized,const std::atomic<bool>* cancelled=nullptr);
}
