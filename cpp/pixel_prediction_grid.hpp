#pragma once
#include "qupathxx/geometry.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
namespace qpx::detail {
inline Rect pixel_prediction_window(Rect roi,const std::optional<Rect>& window) {
    if(!window) return roi;
    const auto& w=*window;
    if(!std::isfinite(w.x) || !std::isfinite(w.y) || !std::isfinite(w.width) || !std::isfinite(w.height) || w.width<=0 || w.height<=0 || !std::isfinite(w.x+w.width) || !std::isfinite(w.y+w.height) || w.x+w.width<=w.x || w.y+w.height<=w.y)
        throw std::invalid_argument("pixel prediction window must have finite, positive, representable extents");
    const auto x=std::max(roi.x,w.x),y=std::max(roi.y,w.y),right=std::min(roi.x+roi.width,w.x+w.width),bottom=std::min(roi.y+roi.height,w.y+w.height);
    if(right<=x || bottom<=y) throw std::invalid_argument("pixel prediction window does not overlap ROI bounds");
    return {x,y,right-x,bottom-y};
}
}
