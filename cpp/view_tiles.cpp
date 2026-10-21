#include "qupathxx/view_tiles.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
struct Grid {
    std::int64_t x0, y0, x1, y1; // Inclusive tile indices.
    long double count() const { return static_cast<long double>(x1-x0+1)*(y1-y0+1); }
};
Grid visible_grid(const ImageLevel& level, double x0, double y0, double x1, double y1, int size) {
    const double step = level.downsample * size;
    auto last_x = (level.width-1)/size, last_y = (level.height-1)/size;
    auto index = [&](double v, std::int64_t last) {
        return static_cast<std::int64_t>(std::clamp(std::floor(v/step),0.,static_cast<double>(last)));
    };
    return {index(x0,last_x),index(y0,last_y),index(std::nextafter(x1,0.),last_x),index(std::nextafter(y1,0.),last_y)};
}
void append(TilePlan& plan, const ImageMetadata& metadata, std::int32_t level_index,
    Grid grid, std::size_t cap, int size, bool fallback) {
    if (!cap) return;
    const auto& level = metadata.levels[level_index];
    const double center_x = (static_cast<double>(grid.x0)+grid.x1)/2;
    const double center_y = (static_cast<double>(grid.y0)+grid.y1)/2;
    if (grid.count() > cap) {
        plan.truncated = true;
        auto cols = std::min<std::int64_t>(grid.x1-grid.x0+1,std::max<std::int64_t>(1,static_cast<std::int64_t>(std::sqrt(static_cast<double>(cap)))));
        auto rows = std::min<std::int64_t>(grid.y1-grid.y0+1,static_cast<std::int64_t>(cap)/cols);
        cols = std::min<std::int64_t>(grid.x1-grid.x0+1,static_cast<std::int64_t>(cap)/rows);
        auto left = std::clamp(static_cast<std::int64_t>(std::floor(center_x-cols/2.)),grid.x0,grid.x1-cols+1);
        auto top = std::clamp(static_cast<std::int64_t>(std::floor(center_y-rows/2.)),grid.y0,grid.y1-rows+1);
        grid = {left,top,left+cols-1,top+rows-1};
    }
    auto edge = [](std::int64_t coordinate, double scale, std::int64_t limit) {
        // Keep integer origins representable even on platforms where long double
        // has the same precision as double. Last edges clamp before conversion.
        auto value = std::round(static_cast<long double>(coordinate)*scale);
        if (value >= static_cast<long double>(limit)) return limit;
        return static_cast<std::int64_t>(value);
    };
    for (auto row=grid.y0; row<=grid.y1; ++row) for (auto col=grid.x0; col<=grid.x1; ++col) {
        auto lx=col*size, ly=row*size;
        auto w=std::min<std::int64_t>(size,level.width-lx), h=std::min<std::int64_t>(size,level.height-ly);
        auto x=edge(lx,level.downsample,metadata.width), y=edge(ly,level.downsample,metadata.height);
        auto right=edge(lx+w,level.downsample,metadata.width), bottom=edge(ly+h,level.downsample,metadata.height);
        if (right<=x || bottom<=y) continue;
        auto distance=std::min(9000.,std::hypot(col-center_x,row-center_y)*10);
        plan.tiles.push_back({{x,y,level_index,w,h},{static_cast<double>(x),static_cast<double>(y),static_cast<double>(right-x),static_cast<double>(bottom-y)},
            (fallback ? 20000 : 10000)-static_cast<int>(distance),fallback});
    }
}
}
TilePlan plan_view_tiles(const ImageMetadata& m, const Viewport& v, double cw, double ch, double dpi, int size, std::size_t cap) {
    if (m.width<=0 || m.height<=0 || m.levels.empty() || !std::isfinite(v.zoom) || v.zoom<=0 ||
        !std::isfinite(cw) || !std::isfinite(ch) || cw<=0 || ch<=0 || !std::isfinite(dpi) || dpi<=0 ||
        !std::isfinite(v.origin.x) || !std::isfinite(v.origin.y) || size<=0 || size>4096 || !cap || cap>4096)
        throw std::invalid_argument("invalid tile planning inputs");
    for (const auto& l : m.levels) if (l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<=0)
        throw std::invalid_argument("invalid image level");
    auto a=v.screen_to_image({0,0}), b=v.screen_to_image({cw,ch});
    double x0=std::max(0.,a.x), y0=std::max(0.,a.y), x1=std::min(static_cast<double>(m.width),b.x), y1=std::min(static_cast<double>(m.height),b.y);
    TilePlan plan;
    if (x1<=x0 || y1<=y0) return plan;
    const double target=1/v.zoom/dpi;
    std::int32_t detail=0, coarsest=0;
    for (std::size_t i=0;i<m.levels.size();++i) {
        if (m.levels[i].downsample>m.levels[coarsest].downsample) coarsest=static_cast<std::int32_t>(i);
        if (m.levels[i].downsample<=target && m.levels[i].downsample>=m.levels[detail].downsample) detail=static_cast<std::int32_t>(i);
    }
    auto grid=visible_grid(m.levels[detail],x0,y0,x1,y1,size);
    // Prefer a coarser complete view to an unbounded request list.
    while (grid.count()>std::max<std::size_t>(1,cap/2) && detail!=coarsest) {
        auto next=coarsest;
        for (std::size_t i=0;i<m.levels.size();++i)
            if (m.levels[i].downsample>m.levels[detail].downsample && m.levels[i].downsample<m.levels[next].downsample) next=static_cast<std::int32_t>(i);
        detail=next; grid=visible_grid(m.levels[detail],x0,y0,x1,y1,size);
    }
    plan.detail_level=detail;
    auto coarse=visible_grid(m.levels[coarsest],x0,y0,x1,y1,size);
    if (detail!=coarsest && coarse.count()<=cap/3) append(plan,m,coarsest,coarse,cap/3,size,true);
    append(plan,m,detail,grid,cap-plan.tiles.size(),size,false);
    return plan;
}
}
