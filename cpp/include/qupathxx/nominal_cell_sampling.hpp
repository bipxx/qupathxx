#pragma once
#include "resampling.hpp"
#include <cmath>

namespace qpx {
// ROI-based controller options, copied at capture. Each image/ROI resolves its
// own integer source bounds; this is not a fixed-source sampling descriptor.
struct NominalCellRoiSampling {
    double downsample=1.5;
    std::uint32_t context_x=32,context_y=32;
    RgbNominalSampling sampling=RgbNominalSampling::IntegerOrigin;
    void validate() const {
        if(sampling!=RgbNominalSampling::IntegerOrigin && sampling!=RgbNominalSampling::AffineInverse)
            throw std::invalid_argument("unknown nominal RGB sampling policy");
        if(!std::isfinite(downsample) || downsample<=0 || context_x>4096 || context_y>4096)
            throw std::invalid_argument("nominal cell ROI sampling requires a finite positive downsample and0..4096 context samples");
    }
    const char* algorithm() const {return sampling==RgbNominalSampling::AffineInverse?"qpx.rgb-nominal-affine/1":"qpx.rgb-nominal/1";}
};
// Immutable original-source rectangle, nominal lattice and detector context.
// Context is in output samples; the reader's interpolation halo is separate.
class NominalCellSampling {
    RgbNominalRequest request_;
    std::uint32_t context_x_,context_y_;
    std::int64_t core_width_,core_height_;
public:
    explicit NominalCellSampling(RgbNominalRequest request,std::uint32_t context_x=32,std::uint32_t context_y=32):
        request_(request),context_x_(context_x),context_y_(context_y),
        core_width_(std::int64_t(std::floor(double(request.width_px)/request.downsample+.5))),
        core_height_(std::int64_t(std::floor(double(request.height_px)/request.downsample+.5))) {
        if(context_x>4096 || context_y>4096 || request.x_px < -9007199254740992LL || request.x_px>9007199254740992LL ||
           request.y_px < -9007199254740992LL || request.y_px>9007199254740992LL)
            throw std::invalid_argument("nominal cell context or integer world origin exceeds limits");
        region_byte_count(width(),height());
    }
    const RgbNominalRequest& source_request() const {return request_;}
    std::uint32_t context_x() const {return context_x_;}
    std::uint32_t context_y() const {return context_y_;}
    std::int64_t core_width() const {return core_width_;}
    std::int64_t core_height() const {return core_height_;}
    std::int64_t width() const {return core_width_+2*context_x_;}
    std::int64_t height() const {return core_height_+2*context_y_;}
    double edge_x(std::int64_t index) const {
        if(index<0 || index>width()) throw std::out_of_range("nominal cell X edge outside lattice");
        return std::fma(double(index-std::int64_t(context_x_)),request_.downsample,double(request_.x_px));
    }
    double edge_y(std::int64_t index) const {
        if(index<0 || index>height()) throw std::out_of_range("nominal cell Y edge outside lattice");
        return std::fma(double(index-std::int64_t(context_y_)),request_.downsample,double(request_.y_px));
    }
    double center_x(std::int64_t index) const {
        if(index<0 || index>=width()) throw std::out_of_range("nominal cell X center outside lattice");
        return std::fma(double(index-std::int64_t(context_x_))+.5,request_.downsample,double(request_.x_px));
    }
    double center_y(std::int64_t index) const {
        if(index<0 || index>=height()) throw std::out_of_range("nominal cell Y center outside lattice");
        return std::fma(double(index-std::int64_t(context_y_))+.5,request_.downsample,double(request_.y_px));
    }
};
}
