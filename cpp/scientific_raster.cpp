#include "scientific_raster.hpp"
#include "qupathxx/provenance.hpp"
#include "sha256.hpp"
#include "geometry_archive.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("scientific raster construction cancelled");}
double centroid(const ScientificGridAxis& axis,std::uint64_t sum,std::uint64_t count) {
    const auto mean=double(sum)/double(count),whole=std::floor(mean);
    const auto anchor=axis.center(axis.first_index()+static_cast<std::int64_t>(whole)).binary64();
    return anchor+(mean-whole)*GridFraction(axis.extent(),axis.samples()).binary64();
}
struct Stats {
    std::uint64_t count=0,x=0,y=0;
    std::int64_t left=std::numeric_limits<std::int64_t>::max(),top=std::numeric_limits<std::int64_t>::max(),right=0,bottom=0;
};
}
void validate_scientific_raster_geometry(const ScientificGrid& grid,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);
    for(const auto* axis:{&grid.x(),&grid.y()}) {
        auto previous=axis->edge(axis->first_index()).binary64();
        for(auto i=axis->first_index();i<axis->stop_index();++i) {
            if((i&4095)==0) checkpoint(cancelled);
            const auto next=axis->edge(i+1).binary64(),center=axis->center(i).binary64();
            if(!std::isfinite(previous) || !std::isfinite(next) || !(previous<center && center<next))
                throw std::invalid_argument("scientific raster grid loses world coordinate precision");
            previous=next;
        }
    }
}
bool scientific_core_roi_contains(const Roi& roi,const ScientificGrid& grid,RoiSamplingPolicy policy,std::size_t x,std::size_t y) {
    const auto& ax=grid.x();const auto& ay=grid.y();
    if(x>=std::uint64_t(grid.width()) || y>=std::uint64_t(grid.height())) throw std::out_of_range("scientific sample outside grid");
    const auto i=std::int64_t(x)+ax.first_index(),j=std::int64_t(y)+ay.first_index();
    if(i<0 || j<0 || i>=ax.samples() || j>=ay.samples()) return false;
    const Point center{ax.center(i).binary64(),ay.center(j).binary64()};const auto b=roi.bounds();
    if(center.x<b.x || center.y<b.y || center.x>=b.x+b.width || center.y>=b.y+b.height) return false;
    return policy==RoiSamplingPolicy::GeometryCoverage?roi.contains(center):roi.contains_sample(center,policy);
}
bool scientific_sample_eligible(const AnalysisRequest& source,const ScientificRgbGrid& input,std::size_t x,std::size_t y) {
    if(x>=std::uint64_t(input.width()) || y>=std::uint64_t(input.height())) throw std::out_of_range("scientific sample outside grid");
    return input.valid()[y*std::size_t(input.width())+x] && scientific_core_roi_contains(source.roi(),input.grid(),source.sampling_policy(),x,y);
}
std::uint64_t scientific_raster_work_bytes(const ScientificGrid& grid,std::uint32_t count,const Roi& roi,const AnalysisProvenance& source) {
    return 29ULL*grid.width()*grid.height()+std::uint64_t(count)*(sizeof(Stats)+sizeof(TissueComponent))+
        source.payload_bytes()+2ULL*roi.payload_bytes()+sizeof(TissueResult)+65536;
}
std::vector<TissueComponent> scientific_raster_components(const ScientificGrid& grid,const std::vector<std::uint32_t>& labels,
    std::uint32_t count,const std::atomic<bool>* cancelled) {
    std::vector<Stats> stats(count);
    for(std::size_t i=0;i<labels.size();++i) {
        if((i&4095)==0) checkpoint(cancelled);if(!labels[i]) continue;
        if(labels[i]>count) throw std::invalid_argument("scientific raster label exceeds component count");
        auto& s=stats[labels[i]-1];const auto x=std::int64_t(i%std::size_t(grid.width())),y=std::int64_t(i/std::size_t(grid.width()));
        ++s.count;s.x+=x;s.y+=y;s.left=std::min(s.left,x);s.top=std::min(s.top,y);s.right=std::max(s.right,x);s.bottom=std::max(s.bottom,y);
    }
    auto edge_x=[&](std::int64_t x){return grid.x().edge(x+grid.x().first_index()).binary64();};
    auto edge_y=[&](std::int64_t y){return grid.y().edge(y+grid.y().first_index()).binary64();};
    std::vector<TissueComponent> components;components.reserve(count);
    for(std::uint32_t i=0;i<count;++i) {
        checkpoint(cancelled);const auto& s=stats[i];if(!s.count) throw std::invalid_argument("scientific raster labels are not dense");
        const auto x=edge_x(s.left),y=edge_y(s.top);
        components.push_back({i+1,s.count,{x,y,edge_x(s.right+1)-x,edge_y(s.bottom+1)-y},{centroid(grid.x(),s.x,s.count),centroid(grid.y(),s.y,s.count)}});
    }
    return components;
}
std::string scientific_raster_input_sha256(const AnalysisRequest& source,const ScientificRgbGrid& input,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);const auto& grid=input.grid();
    if(!source.provenance() || source.provenance()->source.width!=grid.image_width() || source.provenance()->source.height!=grid.image_height() || input.decoded_source_sha256().size()!=64)
        throw std::invalid_argument("scientific input source dimensions or decoded snapshot differ");
    Sha256 hash;constexpr char format[]="qpx.grid-opaque-rgb-area/1";hash.update(format,sizeof(format)-1);
    auto text=[&](const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());};
    auto real=[&](double value) {std::uint64_t bits;std::memcpy(&bits,&value,sizeof(value));hash.integer(bits);};
    text(input.decoded_source_sha256());text(source.provenance()->reader);hash.integer(0); // explicit source level zero
    for(auto value:{grid.image_width(),grid.image_height(),grid.x().origin(),grid.y().origin(),grid.x().extent(),grid.y().extent(),grid.x().samples(),grid.y().samples(),grid.x().context(),grid.y().context()}) hash.integer(value);
    hash.integer(static_cast<std::uint64_t>(source.kind()));hash.integer(static_cast<std::uint64_t>(source.sampling_policy()));
    const auto geometry=GeometryArchiveCodec::encode(source.roi());hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());
    for(auto value:source.transform().white()) real(value);
    for(const auto& vector:source.transform().stains()) for(auto value:vector) real(value);
    for(const auto& value:{source.provenance()->effective_pixel_width_um,source.provenance()->effective_pixel_height_um}) {
        hash.integer(value.has_value());if(value) real(*value);
    }
    // Actual eligibility captures geometry-library boundary decisions as well
    // as source support. Pack row-major bits, least-significant bit first.
    std::array<std::uint8_t,4096> buffer{};std::size_t used=0;std::uint8_t byte=0;unsigned bit=0;
    for(std::size_t i=0;i<input.valid().size();++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(scientific_sample_eligible(source,input,i%std::size_t(grid.width()),i/std::size_t(grid.width()))) byte|=std::uint8_t(1u<<bit);
        if(++bit==8) {buffer[used++]=byte;byte=0;bit=0;if(used==buffer.size()) {hash.update(buffer.data(),used);used=0;}}
    }
    if(bit) buffer[used++]=byte;if(used) hash.update(buffer.data(),used);
    checkpoint(cancelled);return hash.finish();
}
std::shared_ptr<const TissueResult> ScientificRasterKernel::make(const AnalysisRequest& source,const ScientificRgbGrid& input,
    std::vector<std::uint32_t> labels,const ScientificRasterSpec& spec,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);const auto& p=spec.parameters;const auto& grid=input.grid();p.validate();
    if(!spec.ticket || p.level!=0 || p.sampling_policy!=source.sampling_policy() ||
       source.kind()!=(p.feature==TissueFeature::OpticalDensitySum?AnalysisKind::OpticalDensityMeans:AnalysisKind::StainMeans) || !source.provenance() ||
       source.provenance()->source.width!=grid.image_width() || source.provenance()->source.height!=grid.image_height())
        throw std::invalid_argument("scientific raster source, ticket, level or sampling policy differs");
    if(!spec.input_sha256.empty() && (spec.input_sha256.size()!=64 || !std::all_of(spec.input_sha256.begin(),spec.input_sha256.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');})))
        throw std::invalid_argument("invalid scientific raster input digest");
    if(labels.size()!=input.valid().size()) throw std::invalid_argument("scientific raster dimensions differ");
    validate_scientific_raster_geometry(grid,cancelled);
    std::uint32_t count=0;std::uint64_t samples=0,foreground=0;
    for(std::size_t i=0;i<labels.size();++i) {
        if((i&4095)==0) checkpoint(cancelled);
        const auto eligible=scientific_sample_eligible(source,input,i%std::size_t(grid.width()),i/std::size_t(grid.width()));
        samples+=eligible;
        if(labels[i]) {
            if(!eligible) throw std::invalid_argument("scientific raster label lies outside eligible opaque core ROI samples");
            if(labels[i]>p.max_components) throw std::length_error("scientific raster component limit exceeded");
            count=std::max(count,labels[i]);++foreground;
        }
    }
    if(!samples || spec.threshold_pixels>samples || spec.raw_components<count || spec.raw_components>p.max_components || (!spec.body && foreground>spec.threshold_pixels))
        throw std::invalid_argument("scientific raster counters differ from eligible samples");
    const auto charge=scientific_raster_work_bytes(grid,count,source.roi(),*source.provenance());
    if(charge>p.work_bytes) throw std::length_error("scientific raster input, mask and component buffers exceed work budget");
    const auto input_sha=scientific_raster_input_sha256(source,input,cancelled);
    if(!spec.input_sha256.empty() && spec.input_sha256!=input_sha) throw std::invalid_argument("scientific raster input digest differs");
    auto components=scientific_raster_components(grid,labels,count,cancelled);
    auto result=std::shared_ptr<TissueResult>(new TissueResult());result->scientific_grid_=grid;result->ticket_=spec.ticket;
    result->roi_=source.roi();result->provenance_=source.provenance();result->image_bounds_={0,0,double(grid.image_width()),double(grid.image_height())};
    result->parameters_=p;result->white_=source.transform().white();result->stains_=source.transform().stains();
    result->object_id_=source.object_id();result->object_revision_=source.object_revision();result->document_revision_=source.document_revision();result->nucleus_=source.nucleus();
    result->algorithm_=spec.body?"qpx.grid-cell-bodies/1":"qpx.grid-cell-nuclei/1";result->input_format_override_="qpx.grid-opaque-rgb-area/1";result->input_sha256_=input_sha;
    result->labels_=std::move(labels);result->samples_=samples;result->foreground_pixels_=foreground;result->raw_components_=spec.raw_components;
    result->threshold_pixels_=result->cleaned_pixels_=spec.body?foreground:spec.threshold_pixels;result->estimated_work_bytes_=static_cast<std::size_t>(charge);
    const auto left=result->sample_edge_x(0),top=result->sample_edge_y(0),right=result->sample_edge_x(result->width()),bottom=result->sample_edge_y(result->height());
    result->bounds_={left,top,right-left,bottom-top};result->components_=std::move(components);
    checkpoint(cancelled);return result;
}
}
