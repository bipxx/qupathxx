#pragma once
#include "qupathxx/fluorescence_cells.hpp"
namespace qpx::detail {
std::size_t fluorescence_cell_work_bytes(std::int64_t width,std::int64_t height,std::uint32_t channels,SampleType,
    const FluorescenceCellOptions&,std::optional<std::array<double,2>> calibration,
    std::size_t raw_capacity_bytes=0,std::size_t domain_capacity_bytes=0,std::size_t fixed_bytes=0);
FluorescenceCellResult segment_fluorescence_cells_reserved(const RawPixelBuffer&,const std::vector<std::uint8_t>&,
    FluorescenceCellOptions,std::optional<std::array<double,2>>,std::size_t fixed_bytes,const std::atomic<bool>*);
}
