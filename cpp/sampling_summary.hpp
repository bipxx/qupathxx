#pragma once
#include "qupathxx/cells.hpp"
#include "qupathxx/provenance.hpp"
#include "calibrated_cell_numeric.hpp"
#include <cstring>
#include <type_traits>
namespace qpx::detail {
struct SamplingSummary {
    enum class Mode {Level,ScientificGrid,NominalGrid} mode=Mode::Level;
    std::int32_t level=0;double step_x_px=1,step_y_px=1;
    std::optional<double> step_x_um,step_y_um;
    std::int64_t samples_x=0,samples_y=0;std::size_t work_bytes=0;
};
inline void sampling_physical_steps(SamplingSummary& out,std::optional<double> x,std::optional<double> y) {
    if(x && y) {out.step_x_um=calibrated_cell_product({out.step_x_px,*x});out.step_y_um=calibrated_cell_product({out.step_y_px,*y});}
}
inline SamplingSummary sampling_summary(const TissueRequest& request) {
    SamplingSummary out;out.level=request.level();out.work_bytes=request.estimated_work_bytes();
    out.step_x_px=request.downsample_x();out.step_y_px=request.downsample_y();
    if(const auto* grid=request.scientific_grid()) {out.mode=SamplingSummary::Mode::ScientificGrid;out.samples_x=grid->width();out.samples_y=grid->height();}
    else {const auto b=request.level_bounds();out.samples_x=static_cast<std::int64_t>(b.width);out.samples_y=static_cast<std::int64_t>(b.height);}
    const auto& p=request.provenance();sampling_physical_steps(out,p->effective_pixel_width_um,p->effective_pixel_height_um);return out;
}
inline SamplingSummary sampling_summary(const CellRequest& request,std::optional<double> x,std::optional<double> y) {
    SamplingSummary out;out.level=request.level();out.work_bytes=request.estimated_work_bytes();
    if(const auto* grid=request.scientific_grid()) {
        out.mode=SamplingSummary::Mode::ScientificGrid;out.samples_x=grid->width();out.samples_y=grid->height();
        out.step_x_px=GridFraction(grid->x().extent(),grid->x().samples()).binary64();out.step_y_px=GridFraction(grid->y().extent(),grid->y().samples()).binary64();
    } else if(const auto* nominal=request.nominal_sampling()) {
        out.mode=SamplingSummary::Mode::NominalGrid;out.samples_x=nominal->width();out.samples_y=nominal->height();
        out.step_x_px=out.step_y_px=nominal->source_request().downsample;
    } else {
        out.step_x_px=out.step_y_px=request.downsample();const auto b=request.level_bounds();
        out.samples_x=static_cast<std::int64_t>(b.width);out.samples_y=static_cast<std::int64_t>(b.height);
    }
    sampling_physical_steps(out,x,y);return out;
}
// Explicit value fields avoid padding, pointer-to-options identity and missed
// programmatic restores. Floating inputs retain their exact binary values.
struct SamplingInputKey {
    std::vector<std::uint64_t> values;
    void add(double value) {std::uint64_t bits;static_assert(sizeof(bits)==sizeof(value));std::memcpy(&bits,&value,sizeof(bits));values.push_back(bits);}
    template<class T,std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>,int> =0> void add(T value) {values.push_back(static_cast<std::uint64_t>(value));}
    template<class T> void add(const std::optional<T>& value) {add(bool(value));if(value)add(*value);}
    template<class... T> void append(const T&... value) {(add(value),...);}
    void contours(const TissueContourOptions& p) {append(p.clip_to_roi,p.max_components,p.max_edges,p.max_vertices,p.work_bytes);}
    void parameters(const TissueParameters& p) {append(p.feature,p.threshold,p.connectivity,p.level,p.open_radius,p.close_radius,p.fill_holes,p.max_hole_pixels,p.min_pixels,p.max_pixels,p.tile_size,p.max_components,p.work_bytes,p.sampling_policy,p.smoothing_sigma,p.median_cleanup,p.canonical_pyramid);}
    void parameters(const CellParameters& p) {append(p.algorithm,p.log_prominence,p.context_padding,p.feature,p.threshold,p.sigma,p.min_peak_radius,p.min_seed_distance,p.cell_expansion,p.min_nucleus_pixels,p.max_nucleus_pixels,p.connectivity,p.level,p.tile_size,p.max_cells,p.work_bytes);contours(p.contours);}
    void parameters(const GridCellParameters& p) {append(p.processing,p.units,p.feature,p.threshold,p.sigma,p.log_prominence,p.min_peak_radius,p.min_seed_distance,p.cell_expansion,p.min_nucleus_area,p.max_nucleus_area,p.sampling_policy,p.max_cells,p.work_bytes);contours(p.contours);}
};
struct SamplingSummaryCache {
    std::optional<SamplingInputKey> key;std::optional<SamplingSummary> summary;std::string error;
    template<class Builder> void update(SamplingInputKey next,Builder&& build) {
        if(key && key->values==next.values)return;
        key=std::move(next);summary.reset();error.clear();
        try {summary=build();}catch(const std::exception& e){error=e.what();}
    }
};
}
