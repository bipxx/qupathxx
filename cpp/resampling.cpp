#include "qupathxx/resampling.hpp"
#include "rgb_nominal.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace qpx {
RgbNominalRequest::RgbNominalRequest(std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,double step,RgbNominalSampling policy)
    :x_px(x),y_px(y),width_px(width),height_px(height),downsample(step),sampling(policy) {
    detail::validate_rgb_nominal_request(x,y,width,height,step,sampling);
}
namespace {
void checkpoint(const std::atomic<bool>* cancel) {
    if(cancel && cancel->load()) throw std::runtime_error("region resampling cancelled");
}
void validate(std::int64_t sw,std::int64_t sh,std::int64_t w,std::int64_t h) {
    region_byte_count(sw,sh);region_byte_count(w,h);
    // Number of nonempty intersections between two regular 1D partitions.
    // Their Cartesian product is the exact inner-loop count, even when one
    // axis shrinks and the other grows. Reject before decoding/allocation.
    const auto nx=static_cast<std::uint64_t>(sw+w-std::gcd(sw,w));
    const auto ny=static_cast<std::uint64_t>(sh+h-std::gcd(sh,h));
    constexpr std::uint64_t limit=1ULL<<28;
    if(nx>limit/ny) throw std::length_error("region resampling exceeds overlap work limit");
}
}
PixelBuffer resample_area(const PixelBuffer& source,std::int64_t w,std::int64_t h,const std::atomic<bool>* cancel) {
    checkpoint(cancel);const auto sw=source.width(),sh=source.height();validate(sw,sh,w,h);
    if(source.bytes().size()!=region_byte_count(sw,sh)) throw std::runtime_error("resampling reader returned unexpected byte count");
    if(sw==w && sh==h) {
        // Identity has unit weights: retain bytes and canonicalize transparent
        // RGB without per-pixel overlap traversal or integer division.
        PixelBuffer output(w,h,source.bytes());auto& pixels=output.writable_bytes();
        for(std::size_t i=0;i<pixels.size();i+=4) {
            if((i&16383)==0) checkpoint(cancel);
            if(!pixels[i+3]) pixels[i]=pixels[i+1]=pixels[i+2]=0;
        }
        checkpoint(cancel);return output;
    }
    PixelBuffer output(w,h);const auto& input=source.bytes();auto& pixels=output.writable_bytes();
    // Overlap units have denominators w,h. Per-output weight is sw*sh.
    // 255^2*sw*sh and all coordinate products fit uint64/int64 under admission.
    const auto total_weight=static_cast<std::uint64_t>(sw)*sh;std::uint64_t visited=0;
    for(std::int64_t y=0;y<h;++y) {
        checkpoint(cancel);const auto top=y*sh,bottom=(y+1)*sh;
        const auto first_y=top/h,last_y=(bottom+h-1)/h;
        for(std::int64_t x=0;x<w;++x) {
            const auto left=x*sw,right=(x+1)*sw;
            const auto first_x=left/w,last_x=(right+w-1)/w;
            std::uint64_t alpha=0,channels[3]={0,0,0};
            for(auto sy=first_y;sy<last_y;++sy) {
                const auto wy=std::min(bottom,(sy+1)*h)-std::max(top,sy*h);
                for(auto sx=first_x;sx<last_x;++sx) {
                    if((++visited&4095)==0) checkpoint(cancel);
                    const auto wx=std::min(right,(sx+1)*w)-std::max(left,sx*w);
                    const auto weight=static_cast<std::uint64_t>(wx)*wy;
                    const auto index=static_cast<std::size_t>((sy*sw+sx)*4);
                    const auto weighted_alpha=weight*input[index+3];alpha+=weighted_alpha;
                    for(unsigned c=0;c<3;++c) channels[c]+=weighted_alpha*input[index+c];
                }
            }
            const auto index=static_cast<std::size_t>((y*w+x)*4);
            pixels[index+3]=static_cast<std::uint8_t>((alpha+total_weight/2)/total_weight);
            for(unsigned c=0;c<3;++c) pixels[index+c]=pixels[index+3]?static_cast<std::uint8_t>((channels[c]+alpha/2)/alpha):0;
        }
    }
    checkpoint(cancel);return output;
}
PixelBuffer read_resampled_region(const ImageServer& server,const RegionRequest& request,
    std::int64_t w,std::int64_t h,const std::atomic<bool>* cancel) {
    checkpoint(cancel);validate(request.width,request.height,w,h);validate_region(server.metadata(),request);
    auto source=server.read_region(request);checkpoint(cancel);
    if(source.width()!=request.width || source.height()!=request.height || source.bytes().size()!=region_byte_count(request.width,request.height))
        throw std::runtime_error("resampling reader returned unexpected dimensions or byte count");
    return resample_area(source,w,h,cancel);
}
PixelBuffer read_rgb_nominal_region(const ImageServer& server,const RgbNominalRequest& request,
    const std::atomic<bool>* cancel) {
    checkpoint(cancel);
    if(server.closed()) throw std::runtime_error("image server is closed");
    const auto& metadata=server.metadata();
    if(metadata.levels.empty() || metadata.levels.front().downsample!=1 ||
       metadata.levels.front().width!=metadata.width || metadata.levels.front().height!=metadata.height)
        throw std::invalid_argument("RGB nominal reads require a unit level-zero source");
    const auto plan=detail::plan_rgb_nominal(metadata.width,metadata.height,request.x_px,request.y_px,
        request.width_px,request.height_px,request.downsample,cancel,request.sampling);
    std::optional<PixelBuffer> window;
    if(!plan.empty()) {
        validate_region(metadata,plan.window);
        window.emplace(server.read_region(plan.window));
    }
    checkpoint(cancel);
    return detail::sample_rgb_nominal(plan,window?&*window:nullptr,cancel);
}
}
