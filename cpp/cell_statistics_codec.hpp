#pragma once
#include "qupathxx/cells.hpp"
#include "intensity_statistics_codec.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qpx::cell_detail {
inline void validate_cell_statistics(const CellIntensityStatistics& stats,const CellFeature& feature) {
    if(feature.cell_pixels<feature.nucleus_pixels) throw std::invalid_argument("invalid cell statistics compartment counts");
    detail::validate_intensity_compartment(stats.nucleus,feature.nucleus_pixels,feature.nucleus_rgb,feature.nucleus_channels);
    detail::validate_intensity_compartment(stats.cell,feature.cell_pixels,feature.cell_rgb,feature.cell_channels);
    const auto cyto=feature.cell_pixels-feature.nucleus_pixels;
    if(bool(stats.cytoplasm)!=(cyto!=0) || bool(feature.cytoplasm_rgb)!=(cyto!=0) || bool(feature.cytoplasm_channels)!=(cyto!=0))
        throw std::invalid_argument("cell intensity statistics cytoplasm availability differs");
    if(cyto) detail::validate_intensity_compartment(*stats.cytoplasm,cyto,*feature.cytoplasm_rgb,*feature.cytoplasm_channels);
}
template<class Writer> void write_cell_statistics(Writer& w,const CellIntensityStatistics& stats,const CellFeature& feature) {
    validate_cell_statistics(stats,feature);
    auto compartment=[&](const auto& value) {detail::write_intensity_compartment(w,value);};
    compartment(stats.nucleus);compartment(stats.cell);if(stats.cytoplasm) compartment(*stats.cytoplasm);
}
template<class Reader> CellIntensityStatistics read_cell_statistics(Reader& r,const CellFeature& feature) {
    CellIntensityStatistics stats;
    auto compartment=[&](auto& value,std::uint64_t count) {value=detail::read_intensity_compartment(r,count);};
    compartment(stats.nucleus,feature.nucleus_pixels);compartment(stats.cell,feature.cell_pixels);
    if(feature.cell_pixels>feature.nucleus_pixels) {stats.cytoplasm=CompartmentIntensityStatistics{};compartment(*stats.cytoplasm,feature.cell_pixels-feature.nucleus_pixels);}
    validate_cell_statistics(stats,feature);return stats;
}
}
