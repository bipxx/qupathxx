#include "qupathxx/stains.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
void validate_white(double white) {
    if(!std::isfinite(white) || white<1 || white>255)
        throw std::invalid_argument("8-bit white values must be finite and in [1,255]");
}
ColorVector normalized(ColorVector v) {
    double scale=0;
    for(double x:v) {
        if(!std::isfinite(x)) throw std::invalid_argument("stain vectors must be finite");
        scale=std::max(scale,std::abs(x));
    }
    if(scale==0) throw std::invalid_argument("stain vectors must be nonzero");
    for(auto& x:v) x/=scale;
    const double norm=std::hypot(v[0],v[1],v[2]);
    for(auto& x:v) x/=norm;
    return v;
}
ColorVector cross(const ColorVector& a,const ColorVector& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
double dot(const ColorVector& a,const ColorVector& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
void check_cancelled(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load(std::memory_order_relaxed))
        throw std::runtime_error("stain transform cancelled");
}
}
double optical_density(double intensity,double white) {
    validate_white(white);
    if(!std::isfinite(intensity) || intensity<0 || intensity>255)
        throw std::invalid_argument("8-bit intensity must be finite and in [0,255]");
    return std::max(0.,-std::log10(std::max(1.,intensity)/white));
}
FloatRgbImage::FloatRgbImage(std::int64_t width,std::int64_t height):width_(width),height_(height) {
    // Validate arithmetic before allocation, with a separate 256 MiB result cap.
    const auto pixels=region_byte_count(width,height)/4;
    if(pixels>(256u*1024u*1024u)/(3*sizeof(float)))
        throw std::invalid_argument("float RGB result exceeds the 256 MiB output limit");
    values_.resize(pixels*3);
}
StainTransform::StainTransform(const std::vector<ColorVector>& stains,ColorVector white):StainTransform(stains,white,false) {}
StainTransform StainTransform::from_normalized(const std::array<ColorVector,3>& stains,ColorVector white) {
    return StainTransform({stains[0],stains[1],stains[2]},white,true);
}
void StainTransform::validate_normalized(const std::array<ColorVector,3>& stains,ColorVector white) {
    for(auto value:white) validate_white(value);
    for(const auto& vector:stains) {
        for(auto value:vector) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite persisted stain vector");
        if(std::abs(std::hypot(vector[0],vector[1],vector[2])-1)>1e-12) throw std::invalid_argument("persisted stain vectors must be normalized");
    }
    if(std::abs(dot(stains[0],cross(stains[1],stains[2])))<1e-8) throw std::invalid_argument("persisted stain basis is singular");
}
StainTransform::StainTransform(const std::vector<ColorVector>& stains,ColorVector white,bool normalized_input):white_(white) {
    if(stains.size()!=2 && stains.size()!=3) throw std::invalid_argument("provide two or three stain vectors");
    for(auto w:white_) validate_white(w);
    stains_[0]=normalized(stains[0]); stains_[1]=normalized(stains[1]);
    const auto residual=cross(stains_[0],stains_[1]);
    if(stains.size()==2 && std::hypot(residual[0],residual[1],residual[2])<1e-8)
        throw std::invalid_argument("stain basis is singular or nearly singular");
    stains_[2]=normalized(stains.size()==3 ? stains[2] : residual);
    if(normalized_input) {
        for(std::size_t i=0;i<3;++i) {
            if(std::abs(std::hypot(stains[i][0],stains[i][1],stains[i][2])-1)>1e-12)
                throw std::invalid_argument("persisted stain vectors must be normalized");
            stains_[i]=stains[i];
        }
    }
    const auto r0=cross(stains_[1],stains_[2]);
    const auto r1=cross(stains_[2],stains_[0]);
    const auto r2=cross(stains_[0],stains_[1]);
    const double determinant=dot(stains_[0],r0);
    if(std::abs(determinant)<1e-8) throw std::invalid_argument("stain basis is singular or nearly singular");
    const std::array<ColorVector,3> rows={r0,r1,r2};
    for(std::size_t channel=0;channel<3;++channel) for(std::size_t v=0;v<256;++v) {
        od_[channel][v]=optical_density(static_cast<double>(v),white_[channel]);
        for(std::size_t stain=0;stain<3;++stain)
            contributions_[channel][v][stain]=od_[channel][v]*rows[stain][channel]/determinant;
    }
}
FloatRgbImage StainTransform::transform(const PixelBuffer& input,bool separate,const std::atomic<bool>* cancelled) const {
    check_cancelled(cancelled);
    if(input.bytes().size()!=region_byte_count(input.width(),input.height()))
        throw std::invalid_argument("RGBA buffer size mismatch");
    FloatRgbImage output(input.width(),input.height());
    const auto& bytes=input.bytes();
    for(std::size_t i=0,j=0;i<bytes.size();i+=4,j+=3) {
        if((j%12288)==0) check_cancelled(cancelled);
        if(bytes[i+3]!=255) {
            output.values_[j]=output.values_[j+1]=output.values_[j+2]=std::numeric_limits<float>::quiet_NaN();
        } else if(separate) {
            const auto& r=contributions_[0][bytes[i]];
            const auto& g=contributions_[1][bytes[i+1]];
            const auto& b=contributions_[2][bytes[i+2]];
            for(std::size_t c=0;c<3;++c) output.values_[j+c]=static_cast<float>((r[c]+g[c])+b[c]);
        } else {
            for(std::size_t c=0;c<3;++c) output.values_[j+c]=static_cast<float>(od_[c][bytes[i+c]]);
        }
    }
    check_cancelled(cancelled);
    return output;
}
FloatRgbImage StainTransform::optical_densities(const PixelBuffer& input,const std::atomic<bool>* cancelled) const {
    return transform(input,false,cancelled);
}
FloatRgbImage StainTransform::deconvolve(const PixelBuffer& input,const std::atomic<bool>* cancelled) const {
    return transform(input,true,cancelled);
}
}
