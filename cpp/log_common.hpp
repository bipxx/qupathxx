#pragma once
#include "cell_numeric.hpp"

namespace qpx::cell_detail {
struct LogParameters {
    double sigma=1, threshold=.4, prominence=.5, minimum_radius=1, minimum_spacing=4;
    std::uint64_t minimum_area=16, maximum_area=512;
    int peak_connectivity=8;
    bool response_basins=false;
    bool eight_connected_background=false;
    bool sampled_shape=false;
    std::size_t maximum_cells=10000, work_bytes=256*1024*1024;
    void validate() const {
        if(!std::isfinite(sigma) || sigma<0 || sigma>16 || !std::isfinite(threshold) ||
           !std::isfinite(prominence) || prominence<=0 || prominence>1000000 ||
           !std::isfinite(minimum_radius) || minimum_radius<0 || minimum_radius>1000000 ||
           !std::isfinite(minimum_spacing) || minimum_spacing<0 || minimum_spacing>1000000 ||
           !minimum_area || (maximum_area && maximum_area<minimum_area) || !maximum_cells ||
           (peak_connectivity!=4 && peak_connectivity!=8))
            throw std::invalid_argument("invalid LoG watershed parameters");
    }
};
struct LogPeak {std::uint32_t index=0,label=0;double radius=0;bool selected=false;};
struct LogResult {std::vector<std::uint32_t> labels;std::vector<LogPeak> peaks;std::size_t count=0;std::uint64_t candidate_pixels=0;};

template<class F> inline void log_neighbors(std::size_t i,std::size_t width,std::size_t height,bool diagonal,F f) {
    const auto x=i%width,y=i/width;
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        if((!dx && !dy) || (!diagonal && dx && dy)) continue;
        if((dx<0 && !x) || (dy<0 && !y) || (dx>0 && x+1==width) || (dy>0 && y+1==height)) continue;
        f(static_cast<std::size_t>(static_cast<std::int64_t>(i)+dy*static_cast<std::int64_t>(width)+dx));
    }
}

}
