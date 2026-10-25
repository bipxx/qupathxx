#include "scientific_cells.hpp"
#include "scientific_raster.hpp"
#include "scientific_cell_recipe.hpp"
#include "qupathxx/scientific_channels.hpp"
#include "qupathxx/provenance.hpp"
#include "cell_contours.hpp"
#include <chrono>

namespace qpx {
using cell_detail::checkpoint;
ResolvedGridCellParameters resolve_grid_cell_parameters(const ScientificGrid& grid,const GridCellParameters& p,const AnalysisProvenance& source) {
    const auto sx=GridFraction(grid.x().extent(),grid.x().samples()).binary64();
    const auto sy=GridFraction(grid.y().extent(),grid.y().samples()).binary64();
    const auto metric=p.units==GridCellUnits::Micrometres?
        cell_detail::PhysicalGridMetric::capture(grid,source.effective_pixel_width_um,source.effective_pixel_height_um):
        cell_detail::PhysicalGridMetric(sx,sy);
    const auto sigmas=metric.sigma_pixels(p.sigma);const auto n=std::uint64_t(grid.width())*grid.height();
    // Match inclusive calibrated area admission: binary search the actual
    // multi-factor product, without first rounding one combined sample area.
    auto area=[&](std::uint64_t count) {
        if(p.units==GridCellUnits::Micrometres) return detail::calibrated_cell_product({double(count),sx,sy,
            *source.effective_pixel_width_um,*source.effective_pixel_height_um},true);
        return detail::calibrated_cell_product({double(count),sx,sy},true);
    };
    auto first=[&](auto predicate) {std::uint64_t lo=1,hi=n+1;while(lo<hi) {const auto mid=lo+(hi-lo)/2;if(predicate(area(mid))) hi=mid;else lo=mid+1;}return lo;};
    const auto minimum=first([&](double a){return a>=p.min_nucleus_area;});
    const auto maximum=p.max_nucleus_area?(*p.max_nucleus_area==0?0:first([&](double a){return a>*p.max_nucleus_area;})-1):n;
    return {metric.x_um(),metric.y_um(),sigmas.first,sigmas.second,minimum>maximum?n+1:minimum,minimum>maximum?0:maximum};
}
std::size_t grid_cell_work_bytes(const ScientificGrid& grid,const GridCellParameters& p,const ResolvedGridCellParameters& r,
    const Roi& roi,const AnalysisProvenance& source,const ImageMetadata& m,const std::atomic<bool>* cancelled) {
    if(m.width!=grid.image_width() || m.height!=grid.image_height() || m.levels.empty() ||
       m.levels[0].width!=m.width || m.levels[0].height!=m.height || m.levels[0].downsample!=1 || m.pixel_type!=PixelType::UInt8 || m.layout!=PixelLayout::RGBA)
        throw std::invalid_argument("grid cells require an open matching unit-scale RGBA8 level zero");
    validate_scientific_raster_geometry(grid,cancelled);
    const auto width=std::size_t(grid.width()),height=std::size_t(grid.height());const auto n=cell_detail::scientific_cell_pixels(width,height);
    const auto rx=cell_detail::scientific_gaussian_radius(r.sigma_x),ry=cell_detail::scientific_gaussian_radius(r.sigma_y);
    if(cell_detail::physical_gaussian_visits(width,height,rx,ry)>(1ULL<<28) || n>(1ULL<<28)/(8+2*cell_detail::physical_bit_width(height)))
        throw std::length_error("grid cell kernels exceed traversal limit");
    const auto wx=r.step_x*r.step_x,wy=r.step_y*r.step_y;
    if(!std::isfinite(wx) || !std::isfinite(wy) || wx<=0 || wy<=0 || !std::isfinite(1/wx) || !std::isfinite(1/wy) ||
       !std::isfinite(double(width)*double(width)*wx+double(height)*double(height)*wy)) throw std::overflow_error("grid cell metric is not representable");
    // Conservative simultaneous-live allowance. Includes sampled RGB + channel
    // float64 arrays, validity/domain, LoG scratch, EDT/shape heap/maps, two
    // retained masks, compensated feature accumulators and peak/bin metadata.
    // Charge the reader window too even though it is released before kernels.
    const auto b=grid.clipped_read_bounds();
    if(p.contours.work_bytes<65536ULL+2ULL*roi.payload_bytes())
        throw std::length_error("grid cell contours cannot fit their base geometry budget");
    const auto bytes=256*n+1024*std::min<std::uint64_t>(n,p.max_cells)+40ULL*std::max(width,height)+
        8ULL*(2*rx+2*ry+2)+4ULL*b.width*b.height+2ULL*p.contours.work_bytes+
        source.payload_bytes()+4ULL*roi.payload_bytes()+sizeof(CellResult)+65536;
    cell_detail::scientific_cell_budget(bytes,p.work_bytes);
    return std::size_t(bytes);
}
void GridCellParameters::validate() const {
    if(units!=GridCellUnits::LevelZeroPixels && units!=GridCellUnits::Micrometres) throw std::invalid_argument("unknown grid cell units");
    if(feature!=TissueFeature::OpticalDensitySum && feature!=TissueFeature::Stain1 && feature!=TissueFeature::Stain2 && feature!=TissueFeature::Stain3)
        throw std::invalid_argument("unknown grid cell feature");
    validate_sampling_policy(sampling_policy);
    if(!std::isfinite(threshold) || !std::isfinite(log_prominence) || log_prominence<=0 || log_prominence>1000000)
        throw std::invalid_argument("invalid grid cell threshold or prominence");
    detail::validate_cell_area_limits(min_nucleus_area,max_nucleus_area);
    for(auto value:{sigma,min_peak_radius,min_seed_distance,cell_expansion})
        if(!std::isfinite(value) || value<0 || value>1000000) throw std::invalid_argument("invalid grid cell distance parameter");
    if(!max_cells || max_cells>1000000 || !work_bytes || work_bytes>512ULL*1024*1024) throw std::invalid_argument("invalid grid cell limits");
    contours.validate();if(!contours.clip_to_roi) throw std::invalid_argument("grid cell contours must clip to their source ROI");
}
void GridCellSampling::validate() const {
    if(!samples_x || !samples_y || samples_x>1000000 || samples_y>1000000 || context_x>4096 || context_y>4096)
        throw std::invalid_argument("grid cell sampling requires 1..1000000 samples and 0..4096 context per axis");
}
CellRequest CellRequest::for_grid_roi(const Document& document,std::uint64_t id,const StainTransform& transform,GridCellSampling sampling,GridCellParameters p) {
    sampling.validate();p.validate();const auto& roi=document.annotation(id).geometry;const auto b=roi.bounds();const auto& metadata=document.metadata();
    constexpr auto limit=ScientificGridAxis::coordinate_limit;
    if(!roi.is_area() || b.x<0 || b.y<0 || b.x+b.width>metadata.width || b.y+b.height>metadata.height || metadata.width>limit || metadata.height>limit)
        throw std::invalid_argument("grid cell ROI must be an area inside a supported source");
    const auto x=std::int64_t(std::floor(b.x)),y=std::int64_t(std::floor(b.y));
    const auto right=std::int64_t(std::ceil(b.x+b.width)),bottom=std::int64_t(std::ceil(b.y+b.height));
    ScientificGrid grid(metadata.width,metadata.height,{x,right-x,sampling.samples_x,sampling.context_x},{y,bottom-y,sampling.samples_y,sampling.context_y},p.work_bytes);
    return CellRequest(document,id,transform,std::move(grid),std::move(p));
}
CellRequest::CellRequest(const Document& doc,std::uint64_t id,const StainTransform& transform,ScientificGrid grid,GridCellParameters p):
    reader_(doc.server()),transform_(transform),object_revision_(doc.annotation(id).revision),document_revision_(doc.revision()) {
    p.validate();
    // Empty *natural* center grids are permitted: the requested scientific grid
    // can still contain eligible centers. Capture retains normal owner checks.
    AnalysisRequest source(doc,id,transform,p.feature==TissueFeature::OpticalDensitySum?AnalysisKind::OpticalDensityMeans:AnalysisKind::StainMeans,
        false,256,true,p.sampling_policy,true);
    auto resolved=resolve_grid_cell_parameters(grid,p,*source.provenance());
    auto context=std::make_shared<ScientificCellContext>(ScientificCellContext{std::move(source),grid,p,resolved});
    if(reader_->closed()) throw std::invalid_argument("grid cells require an open source");
    work_=grid_cell_work_bytes(grid,p,resolved,context->source.roi(),*context->source.provenance(),reader_->metadata());
    scientific_=std::move(context);physical_calibration_revision_=doc.calibration_revision();
    // Internal common contour budget only; legacy sampled parameters are not
    // exposed as a false representation of the typed grid recipe.
    parameters_.algorithm=CellAlgorithm::GridLogWatershedBodies;parameters_.contours=p.contours;
    parameters_.max_cells=p.max_cells;parameters_.work_bytes=p.work_bytes;
}
bool CellRequest::matches(const Document& d,std::uint64_t id) const {
    return (scientific_?scientific_->source.matches(d,id):source_->matches(d,id)) &&
        (!physical_calibration_revision_ || *physical_calibration_revision_==d.calibration_revision());
}
const CellParameters& CellRequest::parameters() const {
    if(scientific_) throw std::logic_error("explicit-grid cells require grid_parameters");return parameters_;
}
const ScientificGrid* CellRequest::scientific_grid() const {return scientific_?&scientific_->grid:nullptr;}
const GridCellParameters* CellRequest::grid_parameters() const {return scientific_?&scientific_->parameters:nullptr;}
const ResolvedGridCellParameters* CellRequest::resolved_grid_parameters() const {return scientific_?&scientific_->resolved:nullptr;}
std::uint64_t CellRequest::object_id() const {return scientific_?scientific_->source.object_id():source_->object_id();}
std::int32_t CellRequest::level() const {return scientific_?0:source_->level();}
double CellRequest::downsample() const {if(scientific_) throw std::logic_error("explicit-grid cells have separate axes");return source_->downsample();}
Rect CellRequest::level_bounds() const {if(scientific_) throw std::logic_error("explicit-grid cells have no natural level bounds");return processing_grid_;}
const Roi& CellRequest::roi() const {return scientific_?scientific_->source.roi():source_->roi();}
Rect CellRequest::bounds() const {
    if(scientific_) {const auto& grid=scientific_->grid;const auto x=grid.x().edge(grid.x().first_index()).binary64(),y=grid.y().edge(grid.y().first_index()).binary64();
        return {x,y,grid.x().edge(grid.x().stop_index()).binary64()-x,grid.y().edge(grid.y().stop_index()).binary64()-y};}
    const auto b=level_bounds();return {b.x*downsample(),b.y*downsample(),b.width*downsample(),b.height*downsample()};
}
std::uint64_t CellRequest::total_tiles() const {return scientific_?1:((std::uint64_t(processing_grid_.width)+parameters_.tile_size-1)/parameters_.tile_size)*((std::uint64_t(processing_grid_.height)+parameters_.tile_size-1)/parameters_.tile_size);}

std::shared_ptr<const CellResult> ScientificCellKernel::run(const CellRequest& request,const std::atomic<bool>* cancelled,
    std::atomic<std::uint64_t>* completed,std::atomic<int>* phase) {
    checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();const auto& c=*request.scientific_;
    const auto& p=c.parameters;const auto& r=c.resolved;const auto& grid=c.grid;const auto width=std::size_t(grid.width()),height=std::size_t(grid.height()),n=width*height;
    if(phase) phase->store(1);
    auto rgb=read_scientific_rgb(*request.reader_,grid,cancelled);if(completed) completed->store(1);
    auto channels=p.feature==TissueFeature::OpticalDensitySum?scientific_optical_densities(rgb,request.transform_,p.work_bytes,cancelled):scientific_deconvolve(rgb,request.transform_,p.work_bytes,cancelled);
    std::vector<double> score(n);std::vector<std::uint8_t> domain(n);std::uint64_t samples=0;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        score[i]=p.feature==TissueFeature::OpticalDensitySum?channels.values()[3*i]+channels.values()[3*i+1]+channels.values()[3*i+2]:channels.values()[3*i+std::size_t(p.feature)-1];
        domain[i]=scientific_sample_eligible(c.source,rgb,i%width,i/width);samples+=domain[i];
    }
    if(!samples) throw std::invalid_argument("grid cell ROI contains no eligible opaque core centers");
    if(phase) phase->store(2);
    const cell_detail::PhysicalGridMetric metric(r.step_x,r.step_y);
    cell_detail::LogParameters log;log.sigma=0;log.threshold=p.threshold;log.prominence=p.log_prominence;log.minimum_radius=p.min_peak_radius;
    log.minimum_spacing=p.min_seed_distance;log.minimum_area=r.min_nucleus_samples;log.maximum_area=r.max_nucleus_samples;
    log.maximum_cells=p.max_cells;log.work_bytes=p.work_bytes;log.eight_connected_background=true;
    auto detected=cell_detail::grid_log_watershed(score,rgb.valid(),domain,width,height,metric,p.sigma,log,cancelled,phase);
    std::vector<double>().swap(score);
    // Apply the inclusive area limits again after core/ROI clipping.
    std::vector<std::uint64_t> counts(detected.count+1);for(auto label:detected.labels) ++counts[label];
    std::vector<std::uint32_t> dense(detected.count+1);std::uint32_t count=0;
    for(std::size_t i=1;i<counts.size();++i) if(counts[i]>=r.min_nucleus_samples && (!r.max_nucleus_samples || counts[i]<=r.max_nucleus_samples)) dense[i]=++count;
    for(auto& label:detected.labels) label=dense[label];
    auto result=std::shared_ptr<CellResult>(new CellResult());result->parameters_=request.parameters_;result->grid_parameters_=p;result->resolved_grid_parameters_=r;
    result->ticket_=request.ticket_;result->algorithm_="qpx.grid-log-watershed-bodies/1";
    for(const auto& peak:detected.peaks) {
        if(peak.radius>=p.min_peak_radius) ++result->raw_peaks_;if(!peak.selected) continue;
        const auto x=grid.x().center(std::int64_t(peak.index%width)+grid.x().first_index()).binary64();
        const auto y=grid.y().center(std::int64_t(peak.index/width)+grid.y().first_index()).binary64();
        result->markers_.push_back({std::uint32_t(result->markers_.size()+1),dense[peak.label],peak.index,{x,y},peak.radius});
    }
    if(phase) phase->store(5);
    auto bodies=cell_detail::expand_grid_cells(detected.labels,domain,width,height,metric,p.cell_expansion,p.work_bytes,cancelled).labels;
    struct Sums {ColorVector nr{},br{},nc{},bc{},cr{},cc{},nrerr{},brerr{},ncerr{},bcerr{},crerr{},ccerr{};};
    std::vector<Sums> sums(count);result->features_.resize(count);
    auto add=[](double value,double& sum,double& error){const auto adjusted=value-error,next=sum+adjusted;error=(next-sum)-adjusted;sum=next;};
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);const auto a=detected.labels[i],b=bodies[i];if(a && a!=b) throw std::logic_error("grid expansion lost a nucleus");
        if(a) ++result->features_[a-1].nucleus_pixels;if(b) ++result->features_[b-1].cell_pixels;
        for(std::size_t channel=0;channel<3;++channel) {
            const auto v=rgb.values()[3*i+channel],w=channels.values()[3*i+channel];
            if(a) {auto& s=sums[a-1];add(v,s.nr[channel],s.nrerr[channel]);add(w,s.nc[channel],s.ncerr[channel]);}
            if(b) {auto& s=sums[b-1];add(v,s.br[channel],s.brerr[channel]);add(w,s.bc[channel],s.bcerr[channel]);
                if(!a) {add(v,s.cr[channel],s.crerr[channel]);add(w,s.cc[channel],s.ccerr[channel]);}}
        }
    }
    for(std::size_t i=0;i<count;++i) {
        auto& f=result->features_[i];const auto& s=sums[i];f.label=std::uint32_t(i+1);const auto cytoplasm=f.cell_pixels-f.nucleus_pixels;
        if(cytoplasm) {f.cytoplasm_rgb=ColorVector{};f.cytoplasm_channels=ColorVector{};}
        for(std::size_t j=0;j<3;++j) {
            f.nucleus_rgb[j]=s.nr[j]/double(f.nucleus_pixels);f.nucleus_channels[j]=s.nc[j]/double(f.nucleus_pixels);
            f.cell_rgb[j]=s.br[j]/double(f.cell_pixels);f.cell_channels[j]=s.bc[j]/double(f.cell_pixels);
            if(cytoplasm) {(*f.cytoplasm_rgb)[j]=s.cr[j]/double(cytoplasm);(*f.cytoplasm_channels)[j]=s.cc[j]/double(cytoplasm);}
        }
    }
    result->domain_bits_.resize((n+7)/8);for(std::size_t i=0;i<n;++i) if(domain[i]) result->domain_bits_[i/8]|=std::uint8_t(1u<<(i%8));
    ScientificRasterSpec spec;spec.parameters.feature=p.feature;spec.parameters.threshold=p.threshold;spec.parameters.max_components=p.max_cells;
    spec.parameters.work_bytes=p.work_bytes;spec.parameters.sampling_policy=p.sampling_policy;spec.ticket=request.ticket_;
    spec.raw_components=count;spec.threshold_pixels=detected.candidate_pixels;
    result->nuclei_=ScientificRasterKernel::make(c.source,rgb,std::move(detected.labels),spec,cancelled);
    spec.body=true;spec.input_sha256=result->nuclei_->input_sha256();result->cells_=ScientificRasterKernel::make(c.source,rgb,std::move(bodies),spec,cancelled);
    if(phase) phase->store(6);restore_cell_contours(*result,cancelled);checkpoint(cancelled);
    result->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();if(phase) phase->store(7);
    return result;
}
}
