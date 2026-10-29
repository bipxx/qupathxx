#pragma once
#include "qupathxx/cells.hpp"

namespace qpx::cell_detail {
inline const char* processing_name(CellProcessingPolicy p) {
    return p==CellProcessingPolicy::QuPath070?"qupath070/1":p==CellProcessingPolicy::ImageJ154p?"imagej154p/1":"quantitative/1";
}
inline const char* nominal_algorithm(RgbNominalSampling sampling,CellProcessingPolicy p) {
    if(sampling==RgbNominalSampling::AffineInverse) {
        if(p==CellProcessingPolicy::QuPath070) return "qpx.nominal-affine-rgb-qupath070-cell-bodies/1";
        if(p==CellProcessingPolicy::ImageJ154p) return "qpx.nominal-affine-rgb-imagej154p-log-watershed-bodies/1";
        return "qpx.nominal-affine-rgb-log-watershed-bodies/1";
    }
    if(p==CellProcessingPolicy::QuPath070) return "qpx.nominal-rgb-qupath070-cell-bodies/1";
    if(p==CellProcessingPolicy::ImageJ154p) return "qpx.nominal-rgb-imagej154p-log-watershed-bodies/1";
    return "qpx.nominal-rgb-log-watershed-bodies/1";
}
}
