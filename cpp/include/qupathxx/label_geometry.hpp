#pragma once
#include "qupathxx/label_raster.hpp"
#include "qupathxx/scientific_grid.hpp"
#include "qupathxx/tissue.hpp"
namespace qpx {
// All label producers share the existing native raster contour authority and
// rational grid-to-world mapping. No per-algorithm polygonization or UI geometry.
std::shared_ptr<const TissueContours>
trace_label_raster(const LabelRaster&, const ScientificGrid&, const Roi& clip,
                   TissueContourOptions = {}, const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
