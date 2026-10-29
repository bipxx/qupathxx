#include "imagej_gaussian.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

// Independent C++ implementation of the captured ImageJ1.54p numeric behavior.
// Mathematical reference: imagej/ImageJ v1.54p GaussianBlur (Michael Schmid,
// Stephan Saalfeld). Keep float intermediates and disable FP contraction for
// this translation unit; quantitative/default Gaussian policies are separate.
namespace qpx::detail {
namespace {
void checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("Gaussian processing cancelled");
}
void admit(const std::vector<float>& values,std::size_t w,std::size_t h,std::size_t scratch,std::size_t budget,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);
    if(!w || !h || w>std::size_t(std::numeric_limits<int>::max()) || h>std::size_t(std::numeric_limits<int>::max()) ||
       w>(1ULL<<26)/h || values.size()!=w*h) throw std::invalid_argument("invalid Gaussian array dimensions");
    if(scratch>budget || values.size()>(budget-scratch)/sizeof(float)) throw std::length_error("Gaussian work budget exceeded");
    for(std::size_t i=0;i<values.size();++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(!std::isfinite(values[i])) throw std::invalid_argument("Gaussian input must be finite");
    }
}
float finite(float value) {
    if(!std::isfinite(value)) throw std::overflow_error("Gaussian float32 arithmetic overflow");return value;
}
struct Weights {std::vector<float> center,tail;};
Weights weights(double sigma,int length) {
    const int cap=std::max(50,length);
    const int count=std::min(cap,int(std::ceil(sigma*std::sqrt(-2*std::log(.0002))))+1);
    Weights out{std::vector<float>(count),std::vector<float>(count)};
    for(int d=0;d<count;++d) out.center[d]=float(std::exp(-.5*d*d/sigma/sigma));
    if(count<cap && count>3) {
        double slope=std::numeric_limits<double>::max();int d=count;
        while(d>count/2) {
            --d;const double candidate=std::sqrt(double(out.center[d]))/(count-d);
            if(candidate>=slope) break;slope=candidate;
        }
        for(int j=d+2;j<count;++j) out.center[j]=float((count-j)*(count-j)*slope*slope);
    }
    double mass=sigma*std::sqrt(2*3.14159265358979323846);
    if(count<cap) {
        mass=out.center[0];
        for(int d=1;d<count;++d) mass+=2*out.center[d];
    }
    double remainder=.5+.5*out.center[0]/mass;
    for(int d=0;d<count;++d) {
        const double normalized=out.center[d]/mass;
        out.center[d]=float(normalized);remainder-=normalized;out.tail[d]=float(remainder);
    }
    return out;
}
void convolve(const std::vector<float>& source,std::vector<float>& target,const Weights& k,int begin,int end,const std::atomic<bool>* cancelled) {
    const int length=int(source.size()),radius=int(k.center.size());
    for(int at=begin;at<end;++at) {
        if((at&4095)==0) checkpoint(cancelled);
        float sum=source[at]*k.center[0];
        // Edge contributions precede the symmetric pairs. Their float32
        // addition order is observable even on a constant input.
        if(at<radius) sum+=k.tail[at]*source.front();
        if(at+radius>length || (at>=std::min(radius,length) && at+radius==length))
            sum+=k.tail[length-at-1]*source.back();
        for(int d=1;d<radius;++d) {
            float pair=0;
            if(at>=d) pair+=source[at-d];
            if(at+d<length) pair+=source[at+d];
            sum+=k.center[d]*pair;
        }
        target[at]=finite(sum);
    }
}
std::vector<float> reduction_weights(int factor) {
    std::vector<float> out(3*factor);const int center=3*factor/2;
    for(int d=0;d<=(3*factor-1)/2;++d) {
        const double x=double(d)/factor;
        const float value=float(d<=factor/2?(.75-x*x)/factor:(.125+.5*(x-1)*(x-2))/factor);
        if(center-d>=0) out[center-d]=value;
        if(center+d<int(out.size())) out[center+d]=value;
    }
    return out;
}
std::vector<float> enlargement_weights(int factor) {
    std::vector<float> out(4*factor);const int center=2*factor;
    for(int d=0;d<2*factor;++d) {
        const double x=double(d)/factor;
        const float value=float(d<factor?2./3.-x*x*(1-.5*x):(2-x)*(2-x)*(2-x)/6.);
        out[center-d]=value;out[center+d]=value;
    }
    return out;
}
void axis(std::vector<float>& values,int width,int height,double sigma,bool vertical,const std::atomic<bool>* cancelled) {
    if(sigma==0) return;
    const int length=vertical?height:width,lines=vertical?width:height;
    const bool reduced=sigma>8.5;
    const int factor=reduced?std::min(length,int(std::floor(sigma/4))):1;
    const int count=reduced?(length+factor-1)/factor+6:length;
    const double blur_sigma=reduced?std::sqrt(sigma*sigma/(factor*factor)-1./3.-1./4.):sigma;
    const auto k=weights(blur_sigma,count);
    const auto down=reduced?reduction_weights(factor):std::vector<float>{};
    const auto up=reduced?enlargement_weights(factor):std::vector<float>{};
    std::vector<float> original(length),cache(count),smoothed(count);
    for(int line=0;line<lines;++line) {
        checkpoint(cancelled);
        auto index=[&](int at){return vertical?std::size_t(at)*width+line:std::size_t(line)*width+at;};
        for(int at=0;at<length;++at) original[at]=values[index(at)];
        if(!reduced) {
            convolve(original,smoothed,k,0,length,cancelled);
            for(int at=0;at<length;++at) values[index(at)]=smoothed[at];
            continue;
        }
        std::fill(cache.begin(),cache.end(),0.f);
        int position=-3*factor-3*factor/2;
        for(int block=-1;block<=count;++block) {
            if((block&4095)==0) checkpoint(cancelled);
            float left=0,middle=0,right=0;
            for(int j=0;j<factor;++j,++position) {
                const auto value=original[std::clamp(position,0,length-1)];
                left+=value*down[j+2*factor];middle+=value*down[j+factor];right+=value*down[j];
            }
            if(block>0) cache[block-1]+=left;
            if(block>=0 && block<count) cache[block]+=middle;
            if(block+1<count) cache[block+1]=right;
        }
        // Last cache element is padding and is not needed by reconstruction.
        convolve(cache,smoothed,k,1,count-1,cancelled);
        for(int at=0;at<length;++at) {
            const int shifted=at+3*factor+factor-1;
            const int next=shifted/factor,phase=factor-1-shifted%factor;
            float value=smoothed[next-2]*up[phase];
            value+=smoothed[next-1]*up[phase+factor];
            value+=smoothed[next]*up[phase+2*factor];
            value+=smoothed[next+1]*up[phase+3*factor];
            values[index(at)]=finite(value);
        }
    }
}
}
std::vector<float> imagej154p_gaussian(const std::vector<float>& input,std::size_t width,std::size_t height,
    double sx,double sy,std::size_t budget,const std::atomic<bool>* cancelled) {
    if(!std::isfinite(sx) || !std::isfinite(sy) || sx<0 || sy<0 || sx>4096 || sy>4096)
        throw std::invalid_argument("Gaussian sigma must be finite and between 0 and 4096");
    const auto longest=std::max(width,height);
    if(longest>std::size_t(std::numeric_limits<int>::max())) throw std::invalid_argument("Gaussian axis exceeds integer limits");
    const auto scratch=12*(longest+6)+28*std::min<std::size_t>(longest,1024)+1024;
    admit(input,width,height,scratch,budget,cancelled);
    auto result=input;
    axis(result,int(width),int(height),sx,false,cancelled);
    axis(result,int(width),int(height),sy,true,cancelled);
    checkpoint(cancelled);return result;
}
std::vector<float> imagej154p_laplacian(const std::vector<float>& input,std::size_t width,std::size_t height,
    std::size_t budget,const std::atomic<bool>* cancelled) {
    admit(input,width,height,0,budget,cancelled);std::vector<float> result(input.size());
    for(std::size_t y=0;y<height;++y) {
        checkpoint(cancelled);
        for(std::size_t x=0;x<width;++x) {
            if((x&4095)==0) checkpoint(cancelled);
            const auto at=y*width+x;const double center=input[at];
            const double value=((center-input[y*width+(x?x-1:x)])+(center-input[y*width+(x+1<width?x+1:x)]))+
                ((center-input[(y?y-1:y)*width+x])+(center-input[(y+1<height?y+1:y)*width+x]));
            result[at]=finite(float(value));
        }
    }
    checkpoint(cancelled);return result;
}
}
