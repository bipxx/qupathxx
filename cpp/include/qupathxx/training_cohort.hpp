#pragma once
#include "qupathxx/raw_pixel_tiled_training.hpp"
namespace qpx {
// All labelled area objects, in native ID order, regardless of object kind.
// Request capture retains authority for geometry, planes, classes and budgets.
RawPixelTiledTrainingRequest capture_labelled_raw_pixel_training(const Document&,
    ScientificRawLattice,ScientificRawOptions,RawPixelFeatureOptions,
    RawPixelTrainingOptions={},RawPixelTileOptions={},bool physical_sigmas=false);
}
