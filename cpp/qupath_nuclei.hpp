#pragma once
#include "log_watershed.hpp"
#include "qupath_response_basins.hpp"
#include "imagej_gaussian.hpp"

namespace qpx::cell_detail {
struct QuPathNucleusStages {
    std::vector<std::uint32_t> basins;
    std::vector<std::uint8_t> regions,binary,refinement;
    LogResult nuclei;
};
// Private, sampled-grid compatibility candidate. Input/LoG arrays are borrowed
// float32 planes; units of sigma and area parameters are processing samples.
// No shipped request or archived recipe selects this function yet.
inline QuPathNucleusStages qupath070_nuclei(const std::vector<float>& score,
    const std::vector<float>& response,const std::vector<std::uint8_t>& domain,
    std::size_t width,std::size_t height,double sigma,const LogParameters& p,
    const std::atomic<bool>* cancelled=nullptr) {
    checkpoint(cancelled);p.validate();
    if(!width || !height || width>(1ULL<<26)/height || score.size()!=width*height ||
       response.size()!=score.size() || domain.size()!=score.size() ||
       !std::isfinite(sigma) || sigma<0 || sigma>4096 || p.minimum_radius!=0 || p.minimum_spacing!=0)
        throw std::invalid_argument("invalid compatibility nucleus inputs or sampling parameters");
    const auto n=score.size();
    if(p.work_bytes<65536 || n>(p.work_bytes-65536)/128)
        throw std::length_error("compatibility nuclei exceed work budget");
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(!std::isfinite(score[i]) || domain[i]>1) throw std::invalid_argument("invalid compatibility score or domain");
    }
    QuPathNucleusStages output;
    output.basins=qupath070_response_basins(response,width,height,p.maximum_cells,p.work_bytes,cancelled);
    std::vector<std::uint8_t> positive(n);for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);positive[i]=response[i]>0;
    }
    output.regions=log_filled_basin_regions(score,output.basins,positive,width,height,p,cancelled);
    auto shaped=sampled_shape_watershed(output.regions,width,height,p.prominence,p.maximum_cells,p.work_bytes,cancelled);
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);shaped.mask[i]=shaped.mask[i] && domain[i];}
    output.binary=shaped.mask;
    if(sigma>1.5) {
        const auto blurred=detail::imagej154p_gaussian(score,width,height,1,1,p.work_bytes,cancelled);
        const auto response1=detail::imagej154p_laplacian(blurred,width,height,p.work_bytes,cancelled);
        output.refinement.resize(n);
        for(std::size_t y=0;y<height;++y) {
            checkpoint(cancelled);
            for(std::size_t x=0;x<width;++x) {
                const auto i=y*width+x;bool eroded=true;
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    const auto xx=std::clamp<std::int64_t>(std::int64_t(x)+dx,0,std::int64_t(width)-1);
                    const auto yy=std::clamp<std::int64_t>(std::int64_t(y)+dy,0,std::int64_t(height)-1);
                    eroded=eroded && output.binary[std::size_t(yy)*width+std::size_t(xx)];
                }
                output.refinement[i]=output.binary[i] && response1[i]>0;
                shaped.mask[i]=eroded || output.refinement[i];
            }
        }
    }
    output.nuclei=log_label_sampled_nuclei(score,domain,shaped,output.regions,width,height,p,cancelled,nullptr);
    checkpoint(cancelled);return output;
}
}
