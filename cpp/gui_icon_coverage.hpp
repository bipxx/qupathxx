#pragma once
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace qpx::ui {
// ImGui1.93 strokes encode side-edge coverage in atlas UVs, not vertex alpha.
// Keep only the sampled coverage rows in an owned, bounded white-alpha texture.
// This avoids copying the dynamic font atlas or borrowing its GPU texture.
struct IconCoverage {
    int width=0,height=0;
    std::vector<unsigned char> pixels;
    std::vector<ImVec2> uv;
};
inline IconCoverage icon_coverage(const ImDrawList& draw,const ImTextureData& atlas) {
    if(draw.VtxBuffer.empty())return {};
    if(!atlas.Pixels || atlas.Width<1 || atlas.Height<1 || atlas.Width>4096 || atlas.Height>4096 ||
       (atlas.BytesPerPixel!=1 && atlas.BytesPerPixel!=4))throw std::runtime_error("Invalid icon coverage atlas");
    std::vector<float> rows;
    float left=1,right=0;
    for(const auto& v:draw.VtxBuffer) {
        if(!std::isfinite(v.uv.x) || !std::isfinite(v.uv.y) || v.uv.x<0 || v.uv.x>1 || v.uv.y<0 || v.uv.y>1)
            throw std::runtime_error("Invalid icon coverage coordinates");
        left=std::min(left,v.uv.x);right=std::max(right,v.uv.x);
        if(std::find(rows.begin(),rows.end(),v.uv.y)==rows.end())rows.push_back(v.uv.y);
    }
    if(rows.size()>32)throw std::runtime_error("Icon coverage exceeds its row budget");
    // Every stroke/fill triangle samples a single atlas row. Repacking those
    // rows must not interpolate between unrelated coverage profiles.
    for(int i=0;i+2<draw.IdxBuffer.Size;i+=3) {
        const auto y=draw.VtxBuffer[draw.IdxBuffer[i]].uv.y;
        if(draw.VtxBuffer[draw.IdxBuffer[i+1]].uv.y!=y || draw.VtxBuffer[draw.IdxBuffer[i+2]].uv.y!=y)
            throw std::runtime_error("Icon triangle mixes coverage profiles");
    }
    const int x0=std::max(0,static_cast<int>(std::floor(left*atlas.Width))-1);
    const int x1=std::min(atlas.Width,static_cast<int>(std::ceil(right*atlas.Width))+1);
    IconCoverage result;result.width=x1-x0;result.height=static_cast<int>(rows.size())*3;
    if(result.width<1)throw std::runtime_error("Empty icon coverage strip");
    result.pixels.resize(std::size_t(result.width)*result.height*4);
    const auto* source=atlas.Pixels;
    auto alpha=[&](int x,int y){return source[(std::size_t(y)*atlas.Width+x)*atlas.BytesPerPixel+atlas.BytesPerPixel-1];};
    for(std::size_t row=0;row<rows.size();++row) {
        const float y=std::clamp(rows[row]*atlas.Height-.5f,0.f,float(atlas.Height-1));
        const int y0=static_cast<int>(std::floor(y)),y1=std::min(y0+1,atlas.Height-1);
        for(int x=0;x<result.width;++x) {
            const auto a=static_cast<unsigned char>(std::lround(alpha(x+x0,y0)*(1-(y-y0))+alpha(x+x0,y1)*(y-y0)));
            // Duplicate rows prevent linear sampling from bleeding into a
            // neighboring profile. RGB=alpha is premultiplied white.
            for(int copy=0;copy<3;++copy)for(int channel=0;channel<4;++channel)
                result.pixels[((row*3+copy)*result.width+x)*4+channel]=a;
        }
    }
    result.uv.reserve(draw.VtxBuffer.Size);
    for(const auto& v:draw.VtxBuffer) {
        const auto row=std::find(rows.begin(),rows.end(),v.uv.y)-rows.begin();
        result.uv.push_back({(v.uv.x*atlas.Width-x0)/result.width,(float(row)*3+1.5f)/result.height});
    }
    return result;
}
}
