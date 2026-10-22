#pragma once
#include "qupathxx/geometry.hpp"
namespace qpx::detail {
// QuPath v0.7.0: reduce polygon precision to 0.01 pixels, remove collapsed
// components, then store coordinates in a float sequence before its envelope.
Rect qupath_ellipse_bounds(const Ring& shell);
}
