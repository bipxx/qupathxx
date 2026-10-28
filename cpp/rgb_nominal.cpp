#include "rgb_nominal.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx::detail {
namespace {
void checkpoint(const std::atomic<bool>* flag) {
    if(flag && flag->load()) throw std::runtime_error("RGB nominal sampling cancelled");
}
struct Point {std::int64_t first=0,second=0;unsigned fraction=0;bool valid=false;};
Point point(std::int64_t origin,std::int64_t extent,std::uint64_t center,bool nearest) {
    const auto offset=static_cast<std::int64_t>(center>>32);
    if(origin>std::numeric_limits<std::int64_t>::max()-offset) return {};
    const auto integer=origin+offset;
    if(integer<0 || integer>=extent) return {};
    if(nearest) return {integer,integer,0,true};
    const auto shifted=center-(std::uint64_t(1)<<31);
    const auto first=origin+static_cast<std::int64_t>(shifted>>32);
    if(first<0) return {0,0,0,true};
    if(first>=extent-1) return {extent-1,extent-1,0,true};
    const auto fraction=static_cast<unsigned>((shifted>>24)&255);
    return {first,fraction?first+1:first,fraction,true};
}
std::pair<std::int64_t,std::int64_t> window_axis(std::int64_t origin,std::int64_t extent,
    std::int64_t count,std::uint64_t start,std::uint64_t stride,bool nearest,const std::atomic<bool>* cancel) {
    std::int64_t first=extent,last=-1;
    auto center=start;
    for(std::int64_t i=0;i<count;++i,center+=stride) {
        if((i&4095)==0) checkpoint(cancel);
        const auto p=point(origin,extent,center,nearest);
        if(p.valid) {first=std::min(first,p.first);last=std::max(last,p.second);}
    }
    return last<first?std::make_pair(std::int64_t(0),std::int64_t(0)):std::make_pair(first,last-first+1);
}
std::int64_t output_size(std::int64_t extent,double step) {
    const auto rounded=std::floor(static_cast<double>(extent)/step+.5);
    if(!std::isfinite(rounded) || rounded<1 || rounded>67108864)
        throw std::invalid_argument("RGB nominal output dimensions exceed buffer limits");
    return static_cast<std::int64_t>(rounded);
}
}
RgbNominalPlan plan_rgb_nominal(std::int64_t image_width,std::int64_t image_height,
    std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,double step,
    const std::atomic<bool>* cancel) {
    checkpoint(cancel);
    if(image_width<=0 || image_height<=0 || !std::isfinite(step) || step<=0)
        throw std::invalid_argument("RGB nominal source dimensions and downsample must be positive and finite");
    region_byte_count(width,height);
    if(x>std::numeric_limits<std::int64_t>::max()-width || y>std::numeric_limits<std::int64_t>::max()-height)
        throw std::invalid_argument("RGB nominal request coordinate overflow");
    const auto ow=output_size(width,step),oh=output_size(height,step);
    region_byte_count(ow,oh);
    // Admission bounds each nominal extent by2^26 and step by twice that extent.
    // Thus fixed coordinates and products remain below2^60, including one extra
    // loop increment. Integer origins stay separate to support large images.
    const auto start=static_cast<std::uint64_t>(std::floor(std::ldexp(step,31)));
    const auto stride=static_cast<std::uint64_t>(std::floor(std::ldexp(step,32)));
    if(!stride) throw std::invalid_argument("RGB nominal downsample is below fixed-point resolution");
    const auto wx=window_axis(x,image_width,ow,start,stride,step<1,cancel);
    const auto wy=window_axis(y,image_height,oh,start,stride,step<1,cancel);
    RegionRequest window{0,0,0,0,0};
    if(wx.second && wy.second) {
        region_byte_count(wx.second,wy.second);
        window={wx.first,wy.first,0,wx.second,wy.second};
    }
    checkpoint(cancel);
    return {image_width,image_height,x,y,ow,oh,start,stride,step<1,window};
}
PixelBuffer sample_rgb_nominal(const RgbNominalPlan& plan,const PixelBuffer* source,const std::atomic<bool>* cancel) {
    checkpoint(cancel);
    if(plan.empty()) {
        if(source) throw std::invalid_argument("Empty RGB nominal plan must not supply source pixels");
    }else {
        if(!source || source->width()!=plan.window.width || source->height()!=plan.window.height ||
           source->bytes().size()!=region_byte_count(plan.window.width,plan.window.height))
            throw std::runtime_error("RGB nominal reader returned unexpected window dimensions or byte count");
        const auto& bytes=source->bytes();
        for(std::size_t i=3;i<bytes.size();i+=4) {
            if((i&16383)==3) checkpoint(cancel);
            if(bytes[i]!=255) throw std::invalid_argument("RGB nominal sampling requires opaque source pixels");
        }
    }
    PixelBuffer output(plan.output_width,plan.output_height);
    auto& bytes=output.writable_bytes();
    auto cy=plan.start;
    for(std::int64_t y=0;y<plan.output_height;++y,cy+=plan.stride) {
        checkpoint(cancel);
        const auto py=point(plan.y,plan.image_height,cy,plan.nearest);
        auto cx=plan.start;
        for(std::int64_t x=0;x<plan.output_width;++x,cx+=plan.stride) {
            const auto i=static_cast<std::size_t>((y*plan.output_width+x)*4);
            if((i&16383)==0) checkpoint(cancel);
            bytes[i+3]=255;
            const auto px=point(plan.x,plan.image_width,cx,plan.nearest);
            if(!px.valid || !py.valid) continue;
            const auto offset=[&](std::int64_t sx,std::int64_t sy) {
                return static_cast<std::size_t>(((sy-plan.window.y)*plan.window.width+sx-plan.window.x)*4);
            };
            const auto a=offset(px.first,py.first),b=offset(px.second,py.first);
            const auto c=offset(px.first,py.second),d=offset(px.second,py.second);
            const auto fx=px.fraction,fy=py.fraction;
            const auto& input=source->bytes();
            for(unsigned channel=0;channel<3;++channel) {
                const std::uint32_t total=input[a+channel]*(256-fx)*(256-fy)+input[b+channel]*fx*(256-fy)
                    +input[c+channel]*(256-fx)*fy+input[d+channel]*fx*fy+32768;
                bytes[i+channel]=static_cast<std::uint8_t>(total>>16);
            }
        }
    }
    checkpoint(cancel);return output;
}
}
