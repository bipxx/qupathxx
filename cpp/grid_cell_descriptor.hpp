#pragma once
#include "qupathxx/cells.hpp"

namespace qpx::cell_detail {
// Shared QCL5/QCA5 requested-parameter prefix. Writer/Reader provide bounded
// little-endian integer and finite real operations; field order is frozen.
template<class Writer> void grid_parameters_write(Writer& w,const GridCellParameters& p,bool processing=false) {
    if(!processing && p.processing!=CellProcessingPolicy::Quantitative) throw std::invalid_argument("processing policy requires a versioned grid descriptor");
    w.integer(4);w.integer(std::uint64_t(p.units));w.integer(std::uint64_t(p.feature));w.integer(std::uint64_t(p.sampling_policy));
    for(auto v:{p.threshold,p.sigma,p.log_prominence,p.min_peak_radius,p.min_seed_distance,p.cell_expansion,p.min_nucleus_area}) w.real(v);
    w.integer(bool(p.max_nucleus_area));if(p.max_nucleus_area) w.real(*p.max_nucleus_area);
    for(auto v:{p.max_cells,p.work_bytes,std::size_t(p.contours.clip_to_roi),p.contours.max_components,p.contours.max_edges,p.contours.max_vertices,p.contours.work_bytes}) w.integer(v);
    if(processing) w.integer(std::uint64_t(p.processing));
}
template<class Reader> GridCellParameters grid_parameters_read(Reader& d,bool processing=false) {
    if(d.integer()!=4) throw std::invalid_argument("invalid grid cell descriptor algorithm");GridCellParameters p;
    p.units=GridCellUnits(d.integer(1));p.feature=TissueFeature(d.integer(3));p.sampling_policy=RoiSamplingPolicy(d.integer(1));
    p.threshold=d.real();p.sigma=d.real();p.log_prominence=d.real();p.min_peak_radius=d.real();p.min_seed_distance=d.real();p.cell_expansion=d.real();p.min_nucleus_area=d.real();
    if(d.integer(1)) p.max_nucleus_area=d.real();p.max_cells=std::size_t(d.integer(1000000));p.work_bytes=std::size_t(d.integer(512ULL*1024*1024));
    p.contours.clip_to_roi=d.integer(1)!=0;p.contours.max_components=std::size_t(d.integer(1000000));p.contours.max_edges=std::size_t(d.integer(10000000));
    p.contours.max_vertices=std::size_t(d.integer(10000000));p.contours.work_bytes=std::size_t(d.integer(512ULL*1024*1024));
    if(processing) p.processing=CellProcessingPolicy(d.integer(1));p.validate();return p;
}
}
