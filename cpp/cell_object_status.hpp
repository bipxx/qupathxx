#pragma once
#include "qupathxx/cells.hpp"
namespace qpx::detail {
inline std::string cell_geometry_status(const Annotation& a,const CellResult& r,const std::string& source_status) {
    const auto label=a.cell_feature_link->label;
    if(!label || label>r.features().size()) return "invalid_label";
    if(!r.cell_contours() || !r.nucleus_contours()) return "geometry_unavailable";
    if(a.kind!=ObjectKind::Cell || !a.nucleus || !a.geometry.equals(r.cell_contours()->components().at(label-1).geometry) ||
       !a.nucleus->equals(r.nucleus_contours()->components().at(label-1).geometry)) return "geometry_changed";
    return source_status;
}
}
