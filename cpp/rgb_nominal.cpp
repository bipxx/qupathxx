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
Point point(std::int64_t origin,std::int64_t extent,std::uint64_t start,std::uint64_t stride,std::int64_t index,bool nearest) {
    // Split the signed lattice index into whole pixels and fractional bits.
    // A4096-sample context at a very large step can exceed int64 fixed-point
    // range while its whole-pixel coordinate remains representable.
    constexpr std::int64_t unit=std::int64_t(1)<<32;
    const auto fractional=std::int64_t(start&0xffffffffULL)+index*std::int64_t(stride&0xffffffffULL);
    const auto whole=fractional/unit-(fractional%unit<0);
    const auto fraction=std::uint32_t(fractional-whole*unit);
    const auto offset=std::int64_t(start>>32)+index*std::int64_t(stride>>32)+whole;
    if((offset>0 && origin>std::numeric_limits<std::int64_t>::max()-offset) ||
       (offset<0 && origin<std::numeric_limits<std::int64_t>::min()-offset)) return {};
    const auto integer=origin+offset;
    if(integer<0 || integer>=extent) return {};
    if(nearest) return {integer,integer,0,true};
    const auto first=integer-(fraction<(std::uint32_t(1)<<31));
    if(first<0) return {0,0,0,true};
    if(first>=extent-1) return {extent-1,extent-1,0,true};
    const auto weight=static_cast<unsigned>(std::uint32_t(fraction-(std::uint32_t(1)<<31))>>24);
    return {first,weight?first+1:first,weight,true};
}
std::pair<std::int64_t,std::int64_t> window_axis(std::int64_t origin,std::int64_t extent,
    std::int64_t sample,std::int64_t count,std::uint64_t start,std::uint64_t stride,bool nearest,const std::atomic<bool>* cancel) {
    std::int64_t first=extent,last=-1;
    for(std::int64_t i=0;i<count;++i) {
        if((i&4095)==0) checkpoint(cancel);
        const auto p=point(origin,extent,start,stride,sample+i,nearest);
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
void validate_rgb_nominal_request(std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,double step) {
    if(!std::isfinite(step) || step<=0)
        throw std::invalid_argument("RGB nominal downsample must be positive and finite");
    region_byte_count(width,height);
    if(x>std::numeric_limits<std::int64_t>::max()-width || y>std::numeric_limits<std::int64_t>::max()-height)
        throw std::invalid_argument("RGB nominal request coordinate overflow");
    const auto ow=output_size(width,step),oh=output_size(height,step);
    region_byte_count(ow,oh);
}
RgbNominalPlan plan_rgb_nominal(std::int64_t image_width,std::int64_t image_height,
    std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,double step,
    const std::atomic<bool>* cancel) {
    checkpoint(cancel);validate_rgb_nominal_request(x,y,width,height,step);
    return plan_rgb_nominal_window(image_width,image_height,x,y,width,height,step,
        0,0,output_size(width,step),output_size(height,step),cancel);
}
RgbNominalPlan plan_rgb_nominal_window(std::int64_t image_width,std::int64_t image_height,
    std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,double step,
    std::int64_t sample_x,std::int64_t sample_y,std::int64_t samples_width,std::int64_t samples_height,
    const std::atomic<bool>* cancel) {
    checkpoint(cancel);
    if(image_width<=0 || image_height<=0)
        throw std::invalid_argument("RGB nominal source dimensions must be positive");
    validate_rgb_nominal_request(x,y,width,height,step);
    const auto ow=output_size(width,step),oh=output_size(height,step);
    region_byte_count(samples_width,samples_height);
    if(sample_x < -4096 || sample_y < -4096 || sample_x>ow+4096-samples_width || sample_y>oh+4096-samples_height)
        throw std::invalid_argument("RGB nominal sample window exceeds output/context bounds");
    // Admission bounds each nominal extent by2^26 and step by twice that extent.
    // Split whole/fractional index products also admit signed context without
    // multiplying a large step by4096 in a single fixed-point integer.
    const auto start=static_cast<std::uint64_t>(std::floor(std::ldexp(step,31)));
    const auto stride=static_cast<std::uint64_t>(std::floor(std::ldexp(step,32)));
    if(!stride) throw std::invalid_argument("RGB nominal downsample is below fixed-point resolution");
    const auto wx=window_axis(x,image_width,sample_x,samples_width,start,stride,step<1,cancel);
    const auto wy=window_axis(y,image_height,sample_y,samples_height,start,stride,step<1,cancel);
    RegionRequest window{0,0,0,0,0};
    if(wx.second && wy.second) {
        region_byte_count(wx.second,wy.second);
        window={wx.first,wy.first,0,wx.second,wy.second};
    }
    checkpoint(cancel);
    return {image_width,image_height,x,y,samples_width,samples_height,start,stride,step<1,window,sample_x,sample_y};
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
    for(std::int64_t y=0;y<plan.output_height;++y) {
        checkpoint(cancel);
        const auto py=point(plan.y,plan.image_height,plan.start,plan.stride,plan.sample_y+y,plan.nearest);
        for(std::int64_t x=0;x<plan.output_width;++x) {
            const auto i=static_cast<std::size_t>((y*plan.output_width+x)*4);
            if((i&16383)==0) checkpoint(cancel);
            bytes[i+3]=255;
            const auto px=point(plan.x,plan.image_width,plan.start,plan.stride,plan.sample_x+x,plan.nearest);
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
