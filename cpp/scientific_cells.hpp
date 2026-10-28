#pragma once
#include "qupathxx/cells.hpp"
#include "qupathxx/analysis.hpp"
#include "cell_sampling_grid.hpp"

namespace qpx {
struct AnalysisProvenance;
ResolvedGridCellParameters resolve_grid_cell_parameters(const CellSamplingGrid&,const GridCellParameters&,const AnalysisProvenance&);
std::size_t grid_cell_work_bytes(const CellSamplingGrid&,const GridCellParameters&,const ResolvedGridCellParameters&,const Roi&,const AnalysisProvenance&,const ImageMetadata&,const std::atomic<bool>* cancelled=nullptr);
struct ScientificCellContext {
    AnalysisRequest source;
    CellSamplingGrid grid;
    GridCellParameters parameters;
    ResolvedGridCellParameters resolved;
};
struct ScientificCellKernel {
    static std::shared_ptr<const CellResult> run(const CellRequest&,const std::atomic<bool>*,std::atomic<std::uint64_t>*,std::atomic<int>*);
};
}
