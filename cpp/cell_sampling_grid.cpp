#include "cell_sampling_grid.hpp"
#include "scientific_raster.hpp"
#include "rgb_nominal.hpp"
#include "sha256.hpp"
#include "geometry_archive.hpp"
#include "qupathxx/provenance.hpp"
#include <cstring>

namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("cell sampling cancelled");}
}
bool CellSamplingGrid::source_support(std::size_t i) const {
    const auto x=std::int64_t(i%std::size_t(width())),y=std::int64_t(i/std::size_t(width()));
    if(nominal) return center_x(x)>=0 && center_y(y)>=0 && center_x(x)<image_width_ && center_y(y)<image_height_;
    const auto ix=x+area->x().first_index(),iy=y+area->y().first_index();
    return area->x().edge(ix).floor()>=0 && area->y().edge(iy).floor()>=0 &&
        area->x().edge(ix+1).ceil()<=image_width_ && area->y().edge(iy+1).ceil()<=image_height_;
}
bool CellSamplingGrid::core_roi_contains(const Roi& roi,RoiSamplingPolicy policy,std::size_t x,std::size_t y) const {
    if(!nominal) return scientific_core_roi_contains(roi,*area,policy,x,y);
    if(x>=std::uint64_t(width()) || y>=std::uint64_t(height())) throw std::out_of_range("nominal cell sample outside lattice");
    if(x<nominal->context_x() || y<nominal->context_y() || x>=std::uint64_t(nominal->context_x()+nominal->core_width()) ||
       y>=std::uint64_t(nominal->context_y()+nominal->core_height())) return false;
    const Point center{center_x(std::int64_t(x)),center_y(std::int64_t(y))};const auto b=roi.bounds();
    if(center.x<b.x || center.y<b.y || center.x>=b.x+b.width || center.y>=b.y+b.height) return false;
    return policy==RoiSamplingPolicy::GeometryCoverage?roi.contains(center):roi.contains_sample(center,policy);
}
void CellSamplingGrid::validate_geometry(const std::atomic<bool>* c) const {
    if(!nominal) {validate_scientific_raster_geometry(*area,c);return;}
    for(unsigned axis=0;axis<2;++axis) {
        auto edge=[&](std::int64_t i){return axis?edge_y(i):edge_x(i);};
        auto center=[&](std::int64_t i){return axis?center_y(i):center_x(i);};
        const auto count=axis?height():width();auto previous=edge(0);
        for(std::int64_t i=0;i<count;++i) {
            if((i&4095)==0) checkpoint(c);const auto next=edge(i+1),middle=center(i);
            if(!std::isfinite(previous) || !std::isfinite(next) || !(previous<middle && middle<next))
                throw std::invalid_argument("nominal cell lattice loses world coordinate precision");
            previous=next;
        }
    }
}
GridReadBounds CellSamplingGrid::read_bounds(const std::atomic<bool>* c) const {
    if(!nominal) return area->clipped_read_bounds();
    const auto& r=nominal->source_request();
    const auto plan=detail::plan_rgb_nominal_window(image_width_,image_height_,r.x_px,r.y_px,r.width_px,r.height_px,r.downsample,
        -std::int64_t(nominal->context_x()),-std::int64_t(nominal->context_y()),width(),height(),c,r.sampling);
    return {plan.window.x,plan.window.y,plan.window.width,plan.window.height};
}
std::vector<TissueComponent> CellSamplingGrid::components(const std::vector<std::uint32_t>& labels,std::uint32_t count,const std::atomic<bool>* c) const {
    if(!nominal) return scientific_raster_components(*area,labels,count,c);
    struct Stats {std::uint64_t n=0,x=0,y=0;std::int64_t l=std::numeric_limits<std::int64_t>::max(),t=l,r=0,b=0;};
    std::vector<Stats> stats(count);
    for(std::size_t i=0;i<labels.size();++i) {
        if((i&4095)==0) checkpoint(c);if(!labels[i]) continue;
        if(labels[i]>count) throw std::invalid_argument("nominal cell label exceeds component count");
        auto& s=stats[labels[i]-1];const auto x=std::int64_t(i%std::size_t(width())),y=std::int64_t(i/std::size_t(width()));
        ++s.n;s.x+=x;s.y+=y;s.l=std::min(s.l,x);s.t=std::min(s.t,y);s.r=std::max(s.r,x);s.b=std::max(s.b,y);
    }
    std::vector<TissueComponent> result;result.reserve(count);
    for(std::uint32_t i=0;i<count;++i) {
        checkpoint(c);const auto& s=stats[i];if(!s.n) throw std::invalid_argument("nominal cell labels are not dense");
        const auto x=edge_x(s.l),y=edge_y(s.t),mx=double(s.x)/double(s.n),my=double(s.y)/double(s.n);
        result.push_back({i+1,s.n,{x,y,edge_x(s.r+1)-x,edge_y(s.b+1)-y},
            {center_x(std::int64_t(std::floor(mx)))+(mx-std::floor(mx))*step_x(),center_y(std::int64_t(std::floor(my)))+(my-std::floor(my))*step_y()}});
    }
    return result;
}
CellRgbInput read_cell_rgb(const ImageServer& server,const CellSamplingGrid& grid,const std::atomic<bool>* c) {
    checkpoint(c);CellRgbInput result{grid};
    if(grid.area) {result.area.emplace(read_scientific_rgb(server,*grid.area,c));return result;}
    if(server.closed()) throw std::runtime_error("nominal cell source is closed");
    const auto& m=server.metadata();
    if(m.width!=grid.image_width() || m.height!=grid.image_height() || m.levels.empty() || m.levels[0].width!=m.width ||
       m.levels[0].height!=m.height || m.levels[0].downsample!=1 || m.pixel_type!=PixelType::UInt8 || m.layout!=PixelLayout::RGBA)
        throw std::invalid_argument("nominal cell reader metadata changed");
    const auto& n=*grid.nominal;const auto& r=n.source_request();
    const auto plan=detail::plan_rgb_nominal_window(m.width,m.height,r.x_px,r.y_px,r.width_px,r.height_px,r.downsample,
        -std::int64_t(n.context_x()),-std::int64_t(n.context_y()),grid.width(),grid.height(),c,r.sampling);
    std::optional<PixelBuffer> window;if(!plan.empty()) window.emplace(server.read_region(plan.window));
    checkpoint(c);const auto pixels=detail::sample_rgb_nominal(plan,window?&*window:nullptr,c);
    Sha256 decoded;for(auto v:{plan.window.x,plan.window.y,plan.window.width,plan.window.height}) decoded.integer(v);
    if(window) for(std::size_t at=0;at<window->bytes().size();) {
        checkpoint(c);const auto bytes=std::min<std::size_t>(65536,window->bytes().size()-at);decoded.update(window->bytes().data()+at,bytes);at+=bytes;
    }
    result.decoded_source_sha256=decoded.finish();
    const auto count=std::size_t(grid.width()*grid.height());result.rgb.resize(3*count);result.support.resize(count);
    for(std::size_t i=0;i<count;++i) {
        if((i&4095)==0) checkpoint(c);result.support[i]=grid.source_support(i);
        if(result.support[i]) for(unsigned channel=0;channel<3;++channel) result.rgb[3*i+channel]=pixels.bytes()[4*i+channel];
    }
    return result;
}
std::string cell_raster_input_sha256(const AnalysisRequest& source,const CellRgbInput& input,const std::atomic<bool>* c) {
    if(input.area) return scientific_raster_input_sha256(source,*input.area,c);
    checkpoint(c);const auto& grid=input.grid;const auto& n=*grid.nominal;const auto& r=n.source_request();
    Sha256 hash;const std::string format=grid.input_format();hash.update(format.data(),format.size());
    auto text=[&](const std::string& v){hash.integer(v.size());hash.update(v.data(),v.size());};
    auto real=[&](double v){std::uint64_t bits;std::memcpy(&bits,&v,8);hash.integer(bits);};
    text(input.decoded_source_sha256);text(source.provenance()->reader);text(r.algorithm());
    for(auto v:{grid.image_width(),grid.image_height(),r.x_px,r.y_px,r.width_px,r.height_px,std::int64_t(n.context_x()),std::int64_t(n.context_y())}) hash.integer(v);
    real(r.downsample);hash.integer(std::uint64_t(source.kind()));hash.integer(std::uint64_t(source.sampling_policy()));
    const auto geometry=GeometryArchiveCodec::encode(source.roi());hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());
    for(auto v:source.transform().white()) real(v);for(const auto& row:source.transform().stains()) for(auto v:row) real(v);
    for(const auto& v:{source.provenance()->effective_pixel_width_um,source.provenance()->effective_pixel_height_um}) {hash.integer(v.has_value());if(v) real(*v);}
    for(std::size_t i=0;i<input.valid().size();++i) {
        if((i&4095)==0) checkpoint(c);
        hash.integer(input.valid()[i] && grid.core_roi_contains(source.roi(),source.sampling_policy(),i%std::size_t(grid.width()),i/std::size_t(grid.width())));
    }
    return hash.finish();
}
std::uint64_t cell_raster_work_bytes(const CellSamplingGrid& grid,std::uint32_t count,const Roi& roi,const AnalysisProvenance& source) {
    if(grid.area) return scientific_raster_work_bytes(*grid.area,count,roi,source);
    return 29ULL*grid.width()*grid.height()+std::uint64_t(count)*(7*sizeof(std::uint64_t)+sizeof(TissueComponent))+
        source.payload_bytes()+2ULL*roi.payload_bytes()+sizeof(TissueResult)+sizeof(NominalCellSampling)+65536;
}
}
