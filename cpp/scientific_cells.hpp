#pragma once
#include "qupathxx/cells.hpp"
#include "qupathxx/analysis.hpp"

namespace qpx {
struct ScientificCellContext {
    AnalysisRequest source;
    ScientificGrid grid;
    GridCellParameters parameters;
    ResolvedGridCellParameters resolved;
};
struct ScientificCellKernel {
    static std::shared_ptr<const CellResult> run(const CellRequest&,const std::atomic<bool>*,std::atomic<std::uint64_t>*,std::atomic<int>*);
};
}
