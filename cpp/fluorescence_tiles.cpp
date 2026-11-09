#include "qupathxx/fluorescence_tiles.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_cell_internal.hpp"
#include "scientific_cell_numeric.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <limits>

namespace qpx {
namespace {
void rectangle(const RegionRequest& r) {
    if(r.level || r.x<0 || r.y<0 || r.width<=0 || r.height<=0 ||
        r.width>INT64_MAX-r.x || r.height>INT64_MAX-r.y)throw std::invalid_argument("Invalid fluorescence tile rectangle");
}
bool inside(std::int64_t x,std::int64_t y,const RegionRequest& r){return x>=r.x && y>=r.y && x-r.x<r.width && y-r.y<r.height;}
void charge(std::size_t& value,std::size_t n,std::size_t each,std::size_t work) {
    if(n>work/each)throw WorkflowCaptureLimit("Fluorescence tile buffers exceed work budget");
    workflow_capture_charge(value,n*each,work);
}
void text(Sha256& sha,const std::string& s){sha.integer(s.size());sha.update(s.data(),s.size());}
}
FluorescenceTileResult detail::segment_fluorescence_tile_reserved(const RawPixelBuffer& pixels,const std::vector<std::uint8_t>& domain,
    const RawRegionRequest& samples,RegionRequest core,RegionRequest bounds,const std::string& source,
    FluorescenceCellOptions p,std::optional<std::array<double,2>> calibration,std::size_t external,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);p.validate();rectangle(samples.region);rectangle(core);rectangle(bounds);
    if(p.threshold_method!=FluorescenceThreshold::Explicit)throw std::invalid_argument("Resolve a global fluorescence threshold before tiling");
    if(source.size()!=64 || source.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid tiled fluorescence source identity");
    if(pixels.width()!=samples.region.width || pixels.height()!=samples.region.height || pixels.channels()!=samples.channels ||
        samples.first_channel>UINT32_MAX-samples.channels ||
        !inside(core.x,core.y,bounds) || !inside(core.x+core.width-1,core.y+core.height-1,bounds) ||
        !inside(core.x,core.y,samples.region) || !inside(core.x+core.width-1,core.y+core.height-1,samples.region))
        throw std::invalid_argument("Fluorescence tile samples/core/analysis geometry differs");
    if(p.units==FluorescenceCellUnits::Micrometres && !calibration)throw std::invalid_argument("Physical fluorescence tiles need calibration");
    const auto spacing=p.units==FluorescenceCellUnits::Pixels?std::array<double,2>{1,1}:*calibration;
    const cell_detail::PhysicalGridMetric metric(spacing[0],spacing[1]);const auto sigma=metric.sigma_pixels(p.sigma);
    const auto rx=cell_detail::scientific_gaussian_radius(sigma.first),ry=cell_detail::scientific_gaussian_radius(sigma.second);
    const auto expansion=[&](double scale) {
        const auto value=std::ceil(p.cell_expansion/scale);
        if(!std::isfinite(value) || value>double(INT64_MAX/4))throw std::invalid_argument("Fluorescence tile expansion is not representable");
        return std::int64_t(value);
    };
    const auto ex=expansion(spacing[0]),ey=expansion(spacing[1]);
    const auto gx=ex+rx+1,gy=ey+ry+1,hx=2*ex+rx+1,hy=2*ey+ry+1;
    const auto& r=samples.region;
    if((r.x>bounds.x && core.x-r.x<hx) || (r.y>bounds.y && core.y-r.y<hy) ||
        (r.x+r.width<bounds.x+bounds.width && r.x+r.width-core.x-core.width<hx) ||
        (r.y+r.height<bounds.y+bounds.height && r.y+r.height-core.y-core.height<hy))throw std::invalid_argument("Fluorescence segmentation halo is insufficient");
    const auto n=std::size_t(pixels.width())*std::size_t(pixels.height());
    if(domain.size()!=n)throw std::invalid_argument("Fluorescence tile domain differs from sample lattice");
    // Reserve cut-component traversal, per-label state and worst-case output
    // runs while the raw raster/result still exist. No slide-sized allocation.
    std::size_t fixed=external;workflow_capture_charge(fixed,sizeof(FluorescenceTileResult)+4096,p.work_bytes);
    charge(fixed,n,9+6*sizeof(FluorescencePixelSpan),p.work_bytes);
    charge(fixed,std::min(n,p.max_cells),sizeof(FluorescenceOwnedCell)+160+3*std::size_t(pixels.channels())*sizeof(RawChannelIntensity),p.work_bytes);
    for(std::int64_t y=0;y<pixels.height();++y)for(std::int64_t x=0;x<pixels.width();++x) {
        const auto i=std::size_t(y)*std::size_t(pixels.width())+std::size_t(x);
        if(!(i&4095))workflow_checkpoint(cancelled);
        if(domain[i] && !inside(r.x+x,r.y+y,bounds))throw std::invalid_argument("Tile domain extends outside declared analysis bounds");
    }
    FluorescenceTileResult result;
    result.accounted_work_bytes=detail::fluorescence_cell_work_bytes(pixels.width(),pixels.height(),pixels.channels(),pixels.sample_type(),p,calibration,pixels.payload_bytes(),domain.capacity(),fixed);
    std::vector<std::uint8_t> cut;
    const auto raster=detail::segment_fluorescence_cells_reserved(pixels,domain,p,calibration,fixed,cancelled,&cut);
    std::visit([&](const auto& values){for(std::size_t i=0;i<n;++i) {
        if(!(i&4095))workflow_checkpoint(cancelled);
        const auto x=r.x+std::int64_t(i%std::size_t(pixels.width())),y=r.y+std::int64_t(i/std::size_t(pixels.width()));
        if(domain[i] && pixels.valid()[i] && inside(x,y,core) && std::isfinite(double(values[i*pixels.channels()+p.segmentation_channel])))++result.finite_segmentation_pixels_in_core;
    }},pixels.storage());
    result.input_sha256=raster.input_sha256;result.recipe_sha256=raster.recipe_sha256;
    const auto count=raster.measurements.size();std::vector<std::size_t> anchor(count,n);
    std::vector<std::uint8_t> dependent(count,0);
    for(std::size_t i=0;i<n;++i) {
        if(!(i&4095))workflow_checkpoint(cancelled);
        const auto x=std::int64_t(i%std::size_t(pixels.width())),y=std::int64_t(i/std::size_t(pixels.width()));
        if(cut[i] && inside(r.x+x,r.y+y,core))++result.unresolved_foreground_pixels_in_core;
        if(const auto label=raster.nuclei[i]){anchor[label-1]=std::min(anchor[label-1],i);dependent[label-1]|=cut[i];}
        if(const auto label=raster.bodies[i])if(x<gx || y<gy || pixels.width()-1-x<gx || pixels.height()-1-y<gy)dependent[label-1]=1;
    }
    result.cells.reserve(count);result.unresolved_labels.reserve(count);
    for(std::size_t i=0;i<count;++i) {
        workflow_checkpoint(cancelled);if(anchor[i]==n)throw std::runtime_error("Tiled cell has no nucleus anchor");
        const auto x=r.x+std::int64_t(anchor[i]%std::size_t(pixels.width())),y=r.y+std::int64_t(anchor[i]/std::size_t(pixels.width()));
        if(!inside(x,y,core))continue;
        if(dependent[i] || raster.boundaries[i].nucleus || raster.boundaries[i].cell){result.unresolved_labels.push_back(std::uint32_t(i+1));continue;}
        FluorescenceOwnedCell cell;cell.anchor_x=x;cell.anchor_y=y;cell.measurements=raster.measurements[i];
        // Local labels are deliberately omitted from persistent cell identity.
        cell.measurements.label=0;
        Sha256 id;text(id,"qpx.tiled-fluorescence-cell/1");text(id,source);text(id,raster.recipe_sha256);
        id.integer(samples.first_channel);id.integer(samples.channels);id.integer(samples.z);id.integer(samples.t);id.integer(std::uint64_t(pixels.sample_type()));id.integer(std::uint64_t(x));id.integer(std::uint64_t(y));cell.id=id.finish();
        result.cells.push_back(std::move(cell));
    }
    std::vector<std::size_t> selected(count,std::numeric_limits<std::size_t>::max());
    for(std::size_t i=0;i<result.cells.size();++i) {
        const auto& c=result.cells[i];const auto at=std::size_t(c.anchor_y-r.y)*std::size_t(pixels.width())+std::size_t(c.anchor_x-r.x);
        selected[raster.nuclei[at]-1]=i;
    }
    const auto runs=[&](const std::vector<std::uint32_t>& labels,bool nucleus) {
        for(std::int64_t y=0;y<pixels.height();++y)for(std::int64_t x=0;x<pixels.width();) {
            workflow_checkpoint(cancelled);const auto label=labels[std::size_t(y)*std::size_t(pixels.width())+std::size_t(x)];
            const auto start=x++;
            while(x<pixels.width() && labels[std::size_t(y)*std::size_t(pixels.width())+std::size_t(x)]==label)++x;
            if(!label || selected[label-1]==std::numeric_limits<std::size_t>::max())continue;
            auto& cell=result.cells[selected[label-1]];(nucleus?cell.nucleus:cell.cell).push_back({r.y+y,r.x+start,x-start});
        }
    };
    runs(raster.nuclei,true);runs(raster.bodies,false);return result;
}
FluorescenceTileResult segment_fluorescence_tile(const RawPixelBuffer& pixels,const std::vector<std::uint8_t>& domain,
    const RawRegionRequest& samples,RegionRequest core,RegionRequest bounds,const std::string& source,
    FluorescenceCellOptions p,std::optional<std::array<double,2>> calibration,const std::atomic<bool>* cancelled) {
    return detail::segment_fluorescence_tile_reserved(pixels,domain,samples,core,bounds,source,p,calibration,0,cancelled);
}
}
