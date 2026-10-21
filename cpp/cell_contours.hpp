#pragma once
#include "qupathxx/cells.hpp"
namespace qpx {
// Rebuild derived geometry on a private detector/loader worker, before publication.
void restore_cell_contours(CellResult&,const std::atomic<bool>* cancelled=nullptr);
}
