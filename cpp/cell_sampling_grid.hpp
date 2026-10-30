#pragma once
#include "qupathxx/cells.hpp"
#include "qupathxx/scientific_sampling.hpp"
#include "cell_policy_names.hpp"
#include <cstring>

namespace qpx {
// Private geometry adapter for the two explicit cell input contracts. It never
// changes ScientificGrid's rational area geometry or its public serialization.
struct CellSamplingGrid {
    std::optional<ScientificGrid> area;
    std::shared_ptr<const NominalCellSampling> nominal;
    std::int64_t image_width_,image_height_;
    CellSamplingGrid(ScientificGrid grid):area(std::move(grid)),image_width_(area->image_width()),image_height_(area->image_height()) {}
    CellSamplingGrid(std::int64_t w,std::int64_t h,NominalCellSampling sampling):
        nominal(std::make_shared<const NominalCellSampling>(std::move(sampling))),image_width_(w),image_height_(h) {
        if(w<=0 || h<=0) throw std::invalid_argument("nominal cell source dimensions must be positive");
    }
    std::int64_t width() const {return nominal?nominal->width():area->width();}
    std::int64_t height() const {return nominal?nominal->height():area->height();}
    std::int64_t image_width() const {return image_width_;}
    std::int64_t image_height() const {return image_height_;}
    std::uint64_t core_samples() const {return nominal?std::uint64_t(nominal->core_width())*nominal->core_height():std::uint64_t(area->x().samples())*area->y().samples();}
    double step_x() const {return nominal?nominal->source_request().downsample:GridFraction(area->x().extent(),area->x().samples()).binary64();}
    double step_y() const {return nominal?step_x():GridFraction(area->y().extent(),area->y().samples()).binary64();}
    double edge_x(std::int64_t i) const {return nominal?nominal->edge_x(i):area->x().edge(i+area->x().first_index()).binary64();}
    double edge_y(std::int64_t i) const {return nominal?nominal->edge_y(i):area->y().edge(i+area->y().first_index()).binary64();}
    double center_x(std::int64_t i) const {return nominal?nominal->center_x(i):area->x().center(i+area->x().first_index()).binary64();}
    double center_y(std::int64_t i) const {return nominal?nominal->center_y(i):area->y().center(i+area->y().first_index()).binary64();}
    bool source_support(std::size_t i) const;
    bool core_roi_contains(const Roi&,RoiSamplingPolicy,std::size_t x,std::size_t y) const;
    void validate_geometry(const std::atomic<bool>* cancelled=nullptr) const;
    GridReadBounds read_bounds(const std::atomic<bool>* cancelled=nullptr) const;
    std::vector<TissueComponent> components(const std::vector<std::uint32_t>&,std::uint32_t,const std::atomic<bool>*) const;
    bool affine() const {return nominal && nominal->source_request().sampling==RgbNominalSampling::AffineInverse;}
    const char* input_format() const {return affine()?"qpx.nominal-affine-cell-rgb/1":nominal?"qpx.nominal-cell-rgb/1":"qpx.grid-opaque-rgb-area/1";}
    const char* algorithm(CellProcessingPolicy processing=CellProcessingPolicy::Quantitative) const {
        return nominal?cell_detail::nominal_algorithm(nominal->source_request().sampling,processing):"qpx.grid-log-watershed-bodies/1";
    }
    const char* mask_algorithm(bool body,CellProcessingPolicy processing=CellProcessingPolicy::Quantitative) const {
        if(body && processing==CellProcessingPolicy::QuPath070V2) return affine()?"qpx.nominal-affine-qupath070-cell-bodies/2":"qpx.nominal-qupath070-cell-bodies/2";
        if(cell_detail::qupath_processing(processing)) return affine()?
            (body?"qpx.nominal-affine-qupath070-cell-bodies/1":"qpx.nominal-affine-qupath070-cell-nuclei/1"):
            (body?"qpx.nominal-qupath070-cell-bodies/1":"qpx.nominal-qupath070-cell-nuclei/1");
        if(affine()) return processing==CellProcessingPolicy::ImageJ154p?
            (body?"qpx.nominal-affine-imagej154p-cell-bodies/1":"qpx.nominal-affine-imagej154p-cell-nuclei/1"):
            (body?"qpx.nominal-affine-cell-bodies/1":"qpx.nominal-affine-cell-nuclei/1");
        if(processing==CellProcessingPolicy::ImageJ154p) return body?"qpx.nominal-imagej154p-cell-bodies/1":"qpx.nominal-imagej154p-cell-nuclei/1";
        return nominal?(body?"qpx.nominal-cell-bodies/1":"qpx.nominal-cell-nuclei/1"):(body?"qpx.grid-cell-bodies/1":"qpx.grid-cell-nuclei/1");
    }
};
struct CellRgbInput {
    CellSamplingGrid grid;
    std::optional<ScientificRgbGrid> area;
    std::vector<double> rgb;
    std::vector<std::uint8_t> support;
    std::string decoded_source_sha256;
    const std::vector<double>& values() const {return area?area->values():rgb;}
    const std::vector<std::uint8_t>& valid() const {return area?area->valid():support;}
};
CellRgbInput read_cell_rgb(const ImageServer&,const CellSamplingGrid&,const std::atomic<bool>*);
std::string cell_raster_input_sha256(const AnalysisRequest&,const CellRgbInput&,const std::atomic<bool>*);
std::uint64_t cell_raster_work_bytes(const CellSamplingGrid&,std::uint32_t,const Roi&,const AnalysisProvenance&);
template<class Writer> void cell_grid_write(Writer& w,const CellSamplingGrid& grid,bool sampling=false) {
    if(grid.affine() && !sampling) throw std::invalid_argument("affine sampling requires a versioned grid descriptor");
    if(grid.nominal) {
        const auto& n=*grid.nominal;const auto& r=n.source_request();
        for(auto v:{grid.image_width(),grid.image_height(),r.x_px,r.y_px,r.width_px,r.height_px}) w.integer(std::uint64_t(v));
        w.real(r.downsample);w.integer(n.context_x());w.integer(n.context_y());
        if(sampling) w.integer(std::uint64_t(r.sampling));
    } else {
        const auto& g=*grid.area;
        for(auto v:{g.image_width(),g.image_height(),g.x().origin(),g.y().origin(),g.x().extent(),g.y().extent(),g.x().samples(),g.y().samples(),g.x().context(),g.y().context()}) w.integer(std::uint64_t(v));
    }
}
template<class Reader> CellSamplingGrid cell_grid_read(Reader& r,bool nominal,std::size_t work_bytes,bool sampling=false) {
    auto integer=[&] {const auto bits=r.integer();std::int64_t v;std::memcpy(&v,&bits,8);return v;};
    if(nominal) {
        const auto w=integer(),h=integer(),x=integer(),y=integer(),rw=integer(),rh=integer();const auto step=r.real();
        const auto cx=std::uint32_t(r.integer(4096)),cy=std::uint32_t(r.integer(4096));
        const auto policy=sampling?RgbNominalSampling(r.integer(1)):RgbNominalSampling::IntegerOrigin;
        return CellSamplingGrid(w,h,NominalCellSampling(RgbNominalRequest(x,y,rw,rh,step,policy),cx,cy));
    }
    std::array<std::int64_t,10> v;for(auto& n:v) n=integer();
    return ScientificGrid(v[0],v[1],{v[2],v[4],v[6],v[8]},{v[3],v[5],v[7],v[9]},work_bytes);
}
}
