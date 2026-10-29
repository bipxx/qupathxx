#include "scientific_cells.hpp"
#include "roi_scientific_grid.hpp"
#include "scientific_raster.hpp"
#include "scientific_cell_recipe.hpp"
#include "scientific_channel_values.hpp"
#include "qupathxx/provenance.hpp"
#include "cell_contours.hpp"
#include "imagej_gaussian.hpp"
#include "qupath_nuclei.hpp"
#include <chrono>

namespace qpx {
using cell_detail::checkpoint;
ResolvedGridCellParameters resolve_grid_cell_parameters(const CellSamplingGrid& grid,const GridCellParameters& p,const AnalysisProvenance& source) {
    const auto sx=grid.step_x(),sy=grid.step_y();
    const auto metric=[&] {
        if(p.units!=GridCellUnits::Micrometres) return cell_detail::PhysicalGridMetric(sx,sy);
        if(grid.area) return cell_detail::PhysicalGridMetric::capture(*grid.area,source.effective_pixel_width_um,source.effective_pixel_height_um);
        if(!source.effective_pixel_width_um || !source.effective_pixel_height_um) throw std::invalid_argument("physical nominal cells require both calibration axes");
        return cell_detail::PhysicalGridMetric(detail::calibrated_cell_product({sx,*source.effective_pixel_width_um}),
            detail::calibrated_cell_product({sy,*source.effective_pixel_height_um}));
    }();
    const auto sigmas=metric.sigma_pixels(p.sigma);const auto n=std::uint64_t(grid.width())*grid.height();
    if(p.processing==CellProcessingPolicy::QuPath070 &&
       (!grid.nominal || metric.x_um()!=metric.y_um() || sigmas.first>4096 || p.cell_expansion/metric.x_um()>1000000 || p.min_peak_radius!=0 || p.min_seed_distance!=0))
        throw std::invalid_argument("QuPath070 processing requires nominal isotropic sampling, sigma <=4096 samples and zero radius/spacing filters");
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
std::size_t grid_cell_work_bytes(const CellSamplingGrid& grid,const GridCellParameters& p,const ResolvedGridCellParameters& r,
    const Roi& roi,const AnalysisProvenance& source,const ImageMetadata& m,const std::atomic<bool>* cancelled) {
    if(p.processing!=CellProcessingPolicy::Quantitative && !grid.nominal)
        throw std::invalid_argument("ImageJ processing requires nominal RGB cell sampling");
    if(m.width!=grid.image_width() || m.height!=grid.image_height() || m.levels.empty() ||
       m.levels[0].width!=m.width || m.levels[0].height!=m.height || m.levels[0].downsample!=1 || m.pixel_type!=PixelType::UInt8 || m.layout!=PixelLayout::RGBA)
        throw std::invalid_argument("grid cells require an open matching unit-scale RGBA8 level zero");
    grid.validate_geometry(cancelled);
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
    const auto b=grid.read_bounds(cancelled);
    if(p.contours.work_bytes<65536ULL+2ULL*roi.payload_bytes())
        throw std::length_error("grid cell contours cannot fit their base geometry budget");
    const auto bytes=(grid.nominal?260ULL:256ULL)*n+1024*std::min<std::uint64_t>(n,p.max_cells)+40ULL*std::max(width,height)+
        8ULL*(2*rx+2*ry+2)+4ULL*b.width*b.height+2ULL*p.contours.work_bytes+
        source.payload_bytes()+4ULL*roi.payload_bytes()+sizeof(CellResult)+65536;
    cell_detail::scientific_cell_budget(bytes,p.work_bytes);
    return std::size_t(bytes);
}
void GridCellParameters::validate() const {
    if(processing!=CellProcessingPolicy::Quantitative && processing!=CellProcessingPolicy::ImageJ154p && processing!=CellProcessingPolicy::QuPath070)
        throw std::invalid_argument("unknown cell processing policy");
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
    sampling.validate();p.validate();const auto& roi=document.annotation(id).geometry;
    auto grid=detail::roi_scientific_grid(document.metadata(),roi,sampling.samples_x,sampling.samples_y,sampling.context_x,sampling.context_y,p.work_bytes);
    return CellRequest(document,id,transform,std::move(grid),std::move(p));
}
CellRequest CellRequest::for_nominal_roi(const Document& document,std::uint64_t id,const StainTransform& transform,double downsample,
    std::uint32_t context_x,std::uint32_t context_y,GridCellParameters p,RgbNominalSampling sampling) {
    p.validate();const auto b=detail::roi_source_bounds(document.metadata(),document.annotation(id).geometry);
    return CellRequest(document,id,transform,NominalCellSampling(RgbNominalRequest(b.x,b.y,b.width,b.height,downsample,sampling),context_x,context_y),std::move(p));
}
CellRequest::CellRequest(const Document& doc,std::uint64_t id,const StainTransform& transform,ScientificGrid grid,GridCellParameters p):
    CellRequest(doc,id,transform,CellSamplingGrid(std::move(grid)),std::move(p)) {}
CellRequest::CellRequest(const Document& doc,std::uint64_t id,const StainTransform& transform,NominalCellSampling sampling,GridCellParameters p):
    CellRequest(doc,id,transform,CellSamplingGrid(doc.metadata().width,doc.metadata().height,std::move(sampling)),std::move(p)) {}
CellRequest::CellRequest(const Document& doc,std::uint64_t id,const StainTransform& transform,CellSamplingGrid grid,GridCellParameters p):
    reader_(doc.server()),transform_(transform),object_revision_(doc.annotation(id).revision),document_revision_(doc.revision()) {
    p.validate();
    if(p.processing!=CellProcessingPolicy::Quantitative && !grid.nominal)
        throw std::invalid_argument("ImageJ processing requires nominal RGB cell sampling");
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
const ScientificGrid* CellRequest::scientific_grid() const {return scientific_ && scientific_->grid.area?&*scientific_->grid.area:nullptr;}
const NominalCellSampling* CellRequest::nominal_sampling() const {return scientific_?scientific_->grid.nominal.get():nullptr;}
const GridCellParameters* CellRequest::grid_parameters() const {return scientific_?&scientific_->parameters:nullptr;}
const ResolvedGridCellParameters* CellRequest::resolved_grid_parameters() const {return scientific_?&scientific_->resolved:nullptr;}
std::uint64_t CellRequest::object_id() const {return scientific_?scientific_->source.object_id():source_->object_id();}
std::int32_t CellRequest::level() const {return scientific_?0:source_->level();}
double CellRequest::downsample() const {if(scientific_) throw std::logic_error("explicit-grid cells have separate axes");return source_->downsample();}
Rect CellRequest::level_bounds() const {if(scientific_) throw std::logic_error("explicit-grid cells have no natural level bounds");return processing_grid_;}
const Roi& CellRequest::roi() const {return scientific_?scientific_->source.roi():source_->roi();}
Rect CellRequest::bounds() const {
    if(scientific_) {const auto& grid=scientific_->grid;const auto x=grid.edge_x(0),y=grid.edge_y(0);
        return {x,y,grid.edge_x(grid.width())-x,grid.edge_y(grid.height())-y};}
    const auto b=level_bounds();return {b.x*downsample(),b.y*downsample(),b.width*downsample(),b.height*downsample()};
}
std::uint64_t CellRequest::total_tiles() const {return scientific_?1:((std::uint64_t(processing_grid_.width)+parameters_.tile_size-1)/parameters_.tile_size)*((std::uint64_t(processing_grid_.height)+parameters_.tile_size-1)/parameters_.tile_size);}

std::shared_ptr<const CellResult> ScientificCellKernel::run(const CellRequest& request,const std::atomic<bool>* cancelled,
    std::atomic<std::uint64_t>* completed,std::atomic<int>* phase) {
    checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();const auto& c=*request.scientific_;
    const auto& p=c.parameters;const auto& r=c.resolved;const auto& grid=c.grid;const auto width=std::size_t(grid.width()),height=std::size_t(grid.height()),n=width*height;
    if(phase) phase->store(1);
    auto rgb=read_cell_rgb(*request.reader_,grid,cancelled);if(completed) completed->store(1);
    std::vector<double> channels(rgb.values().size());
    if(p.processing==CellProcessingPolicy::Quantitative)
        detail::scientific_channel_values(rgb.values(),rgb.valid(),request.transform_,p.feature!=TissueFeature::OpticalDensitySum,channels,cancelled);
    std::vector<double> score(n);std::vector<std::uint8_t> domain(n);std::uint64_t samples=0;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        score[i]=p.feature==TissueFeature::OpticalDensitySum?channels[3*i]+channels[3*i+1]+channels[3*i+2]:channels[3*i+std::size_t(p.feature)-1];
        domain[i]=(rgb.valid()[i] && grid.core_roi_contains(c.source.roi(),c.source.sampling_policy(),i%width,i/width));samples+=domain[i];
    }
    if(!samples) throw std::invalid_argument("grid cell ROI contains no eligible opaque core centers");
    cell_detail::LogParameters log;log.sigma=0;log.threshold=p.threshold;log.prominence=p.log_prominence;log.minimum_radius=p.min_peak_radius;
    log.minimum_spacing=p.min_seed_distance;log.minimum_area=r.min_nucleus_samples;log.maximum_area=r.max_nucleus_samples;
    log.maximum_cells=p.max_cells;log.work_bytes=p.work_bytes;log.eight_connected_background=true;
    cell_detail::LogResult detected;
    std::vector<std::uint8_t> imagej_positive;
    if(p.processing!=CellProcessingPolicy::Quantitative) {
        std::size_t left=width,top=height,right=0,bottom=0;
        for(std::size_t i=0;i<n;++i) {
            if((i&4095)==0) checkpoint(cancelled);
            if(rgb.valid()[i]) {const auto x=i%width,y=i/width;left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1);}
        }
        const auto cw=right-left,ch=bottom-top;
        PixelBuffer pixels{std::int64_t(cw),std::int64_t(ch)};auto& bytes=pixels.writable_bytes();
        for(std::size_t y=0;y<ch;++y) {
            checkpoint(cancelled);
            for(std::size_t x=0;x<cw;++x) {
                const auto i=(top+y)*width+left+x,j=y*cw+x;
                if(!rgb.valid()[i]) throw std::invalid_argument("ImageJ processing requires rectangular opaque source support");
                for(unsigned c=0;c<3;++c) bytes[4*j+c]=std::uint8_t(rgb.values()[3*i+c]);bytes[4*j+3]=255;
            }
        }
        const auto transformed=p.feature==TissueFeature::OpticalDensitySum?request.transform_.optical_densities(pixels,cancelled):request.transform_.deconvolve(pixels,cancelled);
        const auto& values=transformed.values();std::vector<float> input(cw*ch);
        for(std::size_t y=0;y<ch;++y) {
            checkpoint(cancelled);
            for(std::size_t x=0;x<cw;++x) {
                const auto i=(top+y)*width+left+x,j=y*cw+x;
                for(unsigned c=0;c<3;++c) channels[3*i+c]=values[3*j+c];
                float value;
                if(p.feature==TissueFeature::OpticalDensitySum) {value=values[3*j]+values[3*j+1];value+=values[3*j+2];}
                else value=values[3*j+std::size_t(p.feature)-1];
                input[j]=value;score[i]=value;
            }
        }
        const auto blur=detail::imagej154p_gaussian(input,cw,ch,r.sigma_x,r.sigma_y,p.work_bytes,cancelled);
        const auto response=detail::imagej154p_laplacian(blur,cw,ch,p.work_bytes,cancelled);
        if(p.processing==CellProcessingPolicy::QuPath070) {
            std::vector<std::uint8_t> cropped_domain(cw*ch);
            for(std::size_t y=0;y<ch;++y) {checkpoint(cancelled);for(std::size_t x=0;x<cw;++x) cropped_domain[y*cw+x]=domain[(top+y)*width+left+x];}
            auto stages=cell_detail::qupath070_nuclei(input,response,cropped_domain,cw,ch,r.sigma_x,log,cancelled,false);
            detected=std::move(stages.nuclei);
            std::vector<std::uint32_t> labels(n);
            for(std::size_t y=0;y<ch;++y) {checkpoint(cancelled);for(std::size_t x=0;x<cw;++x) labels[(top+y)*width+left+x]=detected.labels[y*cw+x];}
            detected.labels=std::move(labels);
            for(auto& peak:detected.peaks) {peak.index=std::uint32_t((top+peak.index/cw)*width+left+peak.index%cw);peak.radius*=r.step_x;}
        } else {
            imagej_positive.resize(n);
            for(std::size_t y=0;y<ch;++y) {checkpoint(cancelled);for(std::size_t x=0;x<cw;++x) imagej_positive[(top+y)*width+left+x]=response[y*cw+x]>0;}
        }
    }
    if(phase) phase->store(2);
    const cell_detail::PhysicalGridMetric metric(r.step_x,r.step_y);
    if(p.processing!=CellProcessingPolicy::QuPath070) detected=cell_detail::grid_log_watershed(score,rgb.valid(),domain,width,height,metric,p.sigma,log,cancelled,phase,
        p.processing==CellProcessingPolicy::ImageJ154p?&imagej_positive:nullptr);
    std::vector<std::uint8_t>().swap(imagej_positive);
    std::vector<double>().swap(score);
    // Apply the inclusive area limits again after core/ROI clipping.
    std::vector<std::uint64_t> counts(detected.count+1);for(auto label:detected.labels) ++counts[label];
    std::vector<std::uint32_t> dense(detected.count+1);std::uint32_t count=0;
    for(std::size_t i=1;i<counts.size();++i) if(counts[i]>=r.min_nucleus_samples && (!r.max_nucleus_samples || counts[i]<=r.max_nucleus_samples)) dense[i]=++count;
    for(auto& label:detected.labels) label=dense[label];
    auto result=std::shared_ptr<CellResult>(new CellResult());result->parameters_=request.parameters_;result->grid_parameters_=p;result->resolved_grid_parameters_=r;
    result->ticket_=request.ticket_;result->algorithm_=grid.algorithm(p.processing);
    for(const auto& peak:detected.peaks) {
        if(peak.radius>=p.min_peak_radius) ++result->raw_peaks_;if(!peak.selected) continue;
        const auto x=grid.center_x(std::int64_t(peak.index%width));
        const auto y=grid.center_y(std::int64_t(peak.index/width));
        result->markers_.push_back({std::uint32_t(result->markers_.size()+1),dense[peak.label],peak.index,{x,y},peak.radius});
    }
    if(phase) phase->store(5);
    auto bodies=p.processing==CellProcessingPolicy::QuPath070?
        cell_detail::expand_cells_watershed(detected.labels,domain,width,height,p.cell_expansion/r.step_x,p.work_bytes,cancelled).labels:
        cell_detail::expand_grid_cells(detected.labels,domain,width,height,metric,p.cell_expansion,p.work_bytes,cancelled).labels;
    struct Sums {ColorVector nr{},br{},nc{},bc{},cr{},cc{},nrerr{},brerr{},ncerr{},bcerr{},crerr{},ccerr{};};
    std::vector<Sums> sums(count);result->features_.resize(count);
    auto add=[](double value,double& sum,double& error){const auto adjusted=value-error,next=sum+adjusted;error=(next-sum)-adjusted;sum=next;};
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);const auto a=detected.labels[i],b=bodies[i];if(a && a!=b) throw std::logic_error("grid expansion lost a nucleus");
        if(a) ++result->features_[a-1].nucleus_pixels;if(b) ++result->features_[b-1].cell_pixels;
        for(std::size_t channel=0;channel<3;++channel) {
            const auto v=rgb.values()[3*i+channel],w=channels[3*i+channel];
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
    spec.algorithm_override=grid.mask_algorithm(false,p.processing);
    result->nuclei_=ScientificRasterKernel::make(c.source,rgb,std::move(detected.labels),spec,cancelled);
    spec.body=true;spec.algorithm_override=grid.mask_algorithm(true,p.processing);spec.input_sha256=result->nuclei_->input_sha256();result->cells_=ScientificRasterKernel::make(c.source,rgb,std::move(bodies),spec,cancelled);
    if(phase) phase->store(6);restore_cell_contours(*result,cancelled);checkpoint(cancelled);
    result->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();if(phase) phase->store(7);
    return result;
}
}
