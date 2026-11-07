#pragma once
#include "qupathxx/cells.hpp"
namespace qpx {
// Exact unit-grid proof: validate actual contour boundaries/areas against both
// immutable rasters, then prove label-wise nucleus inclusion. Returns false for
// unsupported lattices/ROIs; malformed supported geometry throws.
bool verify_integer_cell_containment(const TissueResult&,const TissueContours&,
    const TissueResult&,const TissueContours&,const std::atomic<bool>* cancelled=nullptr);
// Rebuild derived geometry on a private detector/loader worker, before publication.
void restore_cell_contours(CellResult&,const std::atomic<bool>* cancelled=nullptr);
// Historical numeric-only archives may lack reconstructible geometry. Cancellation
// still fails the load; other established contour failures clear both contours.
void restore_archived_cell_contours(CellResult&,const std::atomic<bool>* cancelled=nullptr);
}
