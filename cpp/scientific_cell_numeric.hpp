#pragma once
#include "cell_numeric.hpp"
#include "calibrated_cell_numeric.hpp"
#include "qupathxx/scientific_grid.hpp"
#include <utility>
#include <cstring>

namespace qpx::cell_detail {
// Internal preparation for the additive grid recipe. Existing recipes never
// call these kernels. Resolved binary64 spacings are captured per axis.
class PhysicalGridMetric {
    double x_,y_;
public:
    PhysicalGridMetric(double x_um,double y_um):x_(x_um),y_(y_um) {
        if(!std::isfinite(x_) || !std::isfinite(y_) || x_<=0 || y_<=0)
            throw std::invalid_argument("physical grid requires two positive finite spacings");
    }
    static PhysicalGridMetric capture(const ScientificGrid& grid,std::optional<double> x_um,std::optional<double> y_um) {
        if(!x_um || !y_um || !std::isfinite(*x_um) || !std::isfinite(*y_um) || *x_um<=0 || *y_um<=0)
            throw std::invalid_argument("physical grid requires both calibration axes");
        return {detail::calibrated_cell_product({GridFraction(grid.x().extent(),grid.x().samples()).binary64(),*x_um}),
                detail::calibrated_cell_product({GridFraction(grid.y().extent(),grid.y().samples()).binary64(),*y_um})};
    }
    double x_um() const {return x_;}
    double y_um() const {return y_;}
    double area_um2() const {return detail::calibrated_cell_product({x_,y_});}
    std::pair<double,double> sigma_pixels(double sigma_um) const {
        if(!std::isfinite(sigma_um) || sigma_um<0) throw std::invalid_argument("invalid physical Gaussian sigma");
        const auto x=sigma_um/x_,y=sigma_um/y_;
        if(!std::isfinite(x) || !std::isfinite(y) || (sigma_um>0 && (x==0 || y==0)))
            throw std::overflow_error("physical Gaussian sigma is not representable on both axes");
        return {x,y};
    }
};
inline std::uint64_t scientific_cell_pixels(std::size_t width,std::size_t height) {
    if(!width || !height || width>std::numeric_limits<std::uint32_t>::max()/height)
        throw std::invalid_argument("physical cell grid exceeds index range");
    return std::uint64_t(width)*height;
}
inline void scientific_cell_budget(std::uint64_t charge,std::size_t budget) {
    if(!budget || budget>1024ULL*1024*1024) throw std::invalid_argument("invalid physical cell work budget");
    if(charge>budget) throw std::length_error("physical cell stage exceeds work budget");
}
// Portable exact comparisons of binary64-weighted integer squared offsets.
// No long double or compiler-specific 128-bit integer is used.
struct PhysicalWide {std::uint64_t hi=0,lo=0;};
inline PhysicalWide physical_multiply(std::uint64_t a,std::uint64_t b) {
    constexpr std::uint64_t mask=0xffffffffULL;
    const auto a0=a&mask,a1=a>>32,b0=b&mask,b1=b>>32;
    const auto p00=a0*b0,p01=a0*b1,p10=a1*b0,p11=a1*b1;
    const auto middle=(p00>>32)+(p01&mask)+(p10&mask);
    return {p11+(p01>>32)+(p10>>32)+(middle>>32),(middle<<32)|(p00&mask)};
}
inline unsigned physical_bit_width(std::uint64_t value) {
    unsigned result=0;
    if(value>>32) {value>>=32;result+=32;}if(value>>16) {value>>=16;result+=16;}
    if(value>>8) {value>>=8;result+=8;}if(value>>4) {value>>=4;result+=4;}
    if(value>>2) {value>>=2;result+=2;}if(value>>1) {value>>=1;result+=1;}
    return result+unsigned(value!=0);
}
inline PhysicalWide physical_normalize(PhysicalWide value,unsigned width) {
    const auto shift=128-width;if(!shift) return value;
    if(shift>=64) return {value.lo<<(shift-64),0};
    return {(value.hi<<shift)|(value.lo>>(64-shift)),value.lo<<shift};
}
inline int physical_scaled_order(PhysicalWide a,int ae,PhysicalWide b,int be) {
    const auto aw=a.hi?64+physical_bit_width(a.hi):physical_bit_width(a.lo);
    const auto bw=b.hi?64+physical_bit_width(b.hi):physical_bit_width(b.lo);
    if(!aw || !bw) return int(aw!=0)-int(bw!=0);
    if(int(aw)+ae!=int(bw)+be) return int(aw)+ae<int(bw)+be?-1:1;
    a=physical_normalize(a,aw);b=physical_normalize(b,bw);
    if(a.hi!=b.hi) return a.hi<b.hi?-1:1;
    return a.lo==b.lo?0:(a.lo<b.lo?-1:1);
}
struct PhysicalWeight {
    std::uint64_t mantissa;int exponent;
    explicit PhysicalWeight(double value) {
        std::uint64_t bits;std::memcpy(&bits,&value,sizeof(value));
        const auto field=int((bits>>52)&2047);mantissa=bits&0xfffffffffffffULL;
        exponent=field?field-1023-52:-1074;if(field) mantissa|=1ULL<<52;
    }
};
struct PhysicalDifference {std::uint64_t magnitude;bool negative;};
inline PhysicalDifference physical_square_difference(std::size_t q,std::size_t a,std::size_t b) {
    auto square=[&](std::size_t v) {const auto delta=std::int64_t(q)-std::int64_t(v);const auto d=std::uint64_t(delta<0?-delta:delta);return d*d;};
    const auto aa=square(a),bb=square(b);return {aa>=bb?aa-bb:bb-aa,aa<bb};
}
struct PhysicalCostOrder {
    std::size_t width;PhysicalWeight x,y;
    PhysicalCostOrder(std::size_t w,double wx,double wy):width(w),x(wx),y(wy) {}
    int operator()(std::uint32_t a,std::uint32_t b,std::size_t qx,std::size_t qy) const {
        const auto dx=physical_square_difference(qx,a%width,b%width),dy=physical_square_difference(qy,a/width,b/width);
        if(!dx.magnitude) return dy.magnitude?(dy.negative?-1:1):0;
        if(!dy.magnitude || dx.negative==dy.negative) return dx.negative?-1:1;
        const auto order=physical_scaled_order(physical_multiply(dx.magnitude,x.mantissa),x.exponent,
                                               physical_multiply(dy.magnitude,y.mantissa),y.exponent);
        return dx.negative?-order:order;
    }
};
inline std::uint64_t physical_distance_work_bytes(std::size_t width,std::size_t height,bool /*nearest*/) {
    const auto n=scientific_cell_pixels(width,height);
    // In-place distances + nearest map (output or scratch), source-column map,
    // parabola sites and integer first-winning boundaries. Includes caller data.
    return 12*n+12ULL*height+4096;
}
inline void physical_distance(std::vector<double>& values,std::size_t width,std::size_t height,
    const PhysicalGridMetric& metric,bool outside,std::vector<std::uint32_t>* nearest,
    std::size_t work_bytes,const std::atomic<bool>* cancelled=nullptr) {
    checkpoint(cancelled);const auto n=scientific_cell_pixels(width,height);
    if(values.size()!=n || (outside && nearest)) throw std::invalid_argument("invalid physical distance buffer or outside nearest request");
    scientific_cell_budget(physical_distance_work_bytes(width,height,nearest!=nullptr),work_bytes);
    // Integer binary searches avoid rounding away the boundary between two
    // candidates at extreme aspect ratios. Admit their O(N log(height)) work.
    if(n>(1ULL<<28)/(8+2*physical_bit_width(height))) throw std::length_error("physical distance exceeds traversal limit");
    const auto wx=metric.x_um()*metric.x_um(),wy=metric.y_um()*metric.y_um();
    const auto span_x=double(outside?width:width-1),span_y=double(outside?height:height-1);
    const auto maximum=span_x*span_x*wx+span_y*span_y*wy;
    if(!std::isfinite(wx) || !std::isfinite(wy) || wx<=0 || wy<=0 || !std::isfinite(maximum))
        throw std::overflow_error("physical squared distances are not representable");
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(values[i]!=0 && values[i]!=std::numeric_limits<double>::infinity())
            throw std::invalid_argument("physical distance expects zero sites and positive infinity elsewhere");
    }
    constexpr auto absent=std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> scratch;auto& map=nearest?*nearest:scratch;map.assign(n,absent);
    for(std::size_t y=0;y<height;++y) {
        std::uint32_t left=absent,right=absent;
        for(std::size_t x=0;x<width;++x) {
            if((x&4095)==0) checkpoint(cancelled);const auto i=y*width+x;
            if(values[i]==0) left=static_cast<std::uint32_t>(i);map[i]=left;
        }
        for(std::size_t x=width;x-->0;) {
            if((x&4095)==0) checkpoint(cancelled);const auto i=y*width+x;
            if(values[i]==0) right=static_cast<std::uint32_t>(i);
            if(right!=absent && (map[i]==absent || right%width-x<x-map[i]%width)) map[i]=right;
        }
    }
    const PhysicalCostOrder order(width,wx,wy);
    std::vector<std::uint32_t> column(height),sites(height),starts(height);
    for(std::size_t x=0;x<width;++x) {
        checkpoint(cancelled);
        for(std::size_t y=0;y<height;++y) {if((y&4095)==0) checkpoint(cancelled);column[y]=map[y*width+x];}
        std::size_t used=0;
        for(std::size_t y=0;y<height;++y) {
            if((y&4095)==0) checkpoint(cancelled);if(column[y]==absent) continue;std::size_t start=0;
            while(used) {
                std::size_t lo=0,hi=height;
                while(lo<hi) {
                    checkpoint(cancelled);const auto mid=lo+(hi-lo)/2;
                    // Only strict wins admit the later row; ties keep the
                    // earlier original row, then its earlier nearest column.
                    if(order(column[y],column[sites[used-1]],x,mid)<0) hi=mid;else lo=mid+1;
                }
                start=lo;if(start>starts[used-1]) break;--used;
            }
            if(!used) start=0;if(start==height) continue;
            sites[used]=static_cast<std::uint32_t>(y);starts[used]=static_cast<std::uint32_t>(start);++used;
        }
        std::size_t at=0;
        for(std::size_t y=0;y<height;++y) {
            if((y&4095)==0) checkpoint(cancelled);while(at+1<used && starts[at+1]<=y) ++at;
            const auto site=used?column[sites[at]]:absent;double value=std::numeric_limits<double>::infinity();
            if(site!=absent) {const auto dx=double(x)-double(site%width),dy=double(y)-double(site/width);value=dx*dx*wx+dy*dy*wy;}
            if(outside) {
                const auto a=double(x+1),b=double(width-x),c=double(y+1),d=double(height-y);
                value=std::min(value,std::min(std::min(a*a,b*b)*wx,std::min(c*c,d*d)*wy));
            }
            values[y*width+x]=value;map[y*width+x]=site;
        }
    }
    checkpoint(cancelled);
}
inline int scientific_gaussian_radius(double sigma) {
    if(!std::isfinite(sigma) || sigma<0) throw std::invalid_argument("invalid scientific Gaussian sigma");
    if(sigma>4096./3.) throw std::length_error("scientific Gaussian support exceeds 4096 samples");
    return static_cast<int>(std::ceil(3*sigma));
}
inline std::uint64_t physical_gaussian_visits(std::size_t width,std::size_t height,int rx,int ry) {
    scientific_cell_pixels(width,height);
    if(rx<0 || ry<0 || rx>4096 || ry>4096) throw std::invalid_argument("invalid physical Gaussian support");
    auto axis=[](std::uint64_t length,int radius) {
        const auto r=std::min<std::uint64_t>(radius,length-1);
        return length*(2*r+1)-r*(r+1);
    };
    return (rx?axis(width,rx)*height:0)+(ry?axis(height,ry)*width:0);
}
inline std::uint64_t physical_gaussian_work_bytes(std::size_t width,std::size_t height,double sx,double sy) {
    const auto n=scientific_cell_pixels(width,height);const auto rx=scientific_gaussian_radius(sx),ry=scientific_gaussian_radius(sy);
    return (rx || ry?33:9)*n+8ULL*(2*rx+2*ry+2)+4096;
}
// Validity-normalized finite Gaussian: X then Y, float64 intermediates,
// clipped support at the array edge and canonical zero at invalid centers.
inline void physical_gaussian(std::vector<double>& values,const std::vector<std::uint8_t>& valid,
    std::size_t width,std::size_t height,const PhysicalGridMetric& metric,double sigma_um,
    std::size_t work_bytes,const std::atomic<bool>* cancelled=nullptr) {
    checkpoint(cancelled);const auto n=scientific_cell_pixels(width,height);const auto sigmas=metric.sigma_pixels(sigma_um);
    const auto rx=scientific_gaussian_radius(sigmas.first),ry=scientific_gaussian_radius(sigmas.second);
    if(values.size()!=n || valid.size()!=n) throw std::invalid_argument("invalid physical Gaussian buffers");
    scientific_cell_budget(physical_gaussian_work_bytes(width,height,sigmas.first,sigmas.second),work_bytes);
    if(physical_gaussian_visits(width,height,rx,ry)>(1ULL<<28)) throw std::length_error("physical Gaussian exceeds traversal limit");
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(valid[i]>1 || (valid[i] && !std::isfinite(values[i]))) throw std::invalid_argument("invalid physical Gaussian values or validity");
    }
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);if(!valid[i]) values[i]=0;}
    if(!rx && !ry) return;
    std::vector<double> weights(valid.begin(),valid.end()),temporary(n),temporary_weights(n);
    for(int axis=0;axis<2;++axis) {
        const auto sigma=axis?sigmas.second:sigmas.first;const auto radius=axis?ry:rx;if(!radius) continue;
        std::vector<double> kernel(2*radius+1);
        for(int d=-radius;d<=radius;++d) {if((d&4095)==0) checkpoint(cancelled);const auto scaled=double(d)/sigma;kernel[d+radius]=d==0?1:std::exp(-.5*scaled*scaled);}
        for(std::size_t y=0;y<height;++y) {
            checkpoint(cancelled);
            for(std::size_t x=0;x<width;++x) {
                if((x&4095)==0) checkpoint(cancelled);double sum=0,weight=0;
                const auto at=axis?y:x,length=axis?height:width;
                const auto first=-int(std::min<std::size_t>(radius,at)),last=int(std::min<std::size_t>(radius,length-1-at));
                for(int d=first;d<=last;++d) {
                    const auto row=axis?std::size_t(std::int64_t(y)+d):y,col=axis?x:std::size_t(std::int64_t(x)+d);
                    const auto index=row*width+col;sum+=values[index]*kernel[d+radius];weight+=weights[index]*kernel[d+radius];
                }
                if(!std::isfinite(sum) || !std::isfinite(weight)) throw std::overflow_error("physical Gaussian accumulation overflow");
                temporary[y*width+x]=sum;temporary_weights[y*width+x]=weight;
            }
        }
        values.swap(temporary);weights.swap(temporary_weights);
    }
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);values[i]=valid[i] && weights[i]>0?values[i]/weights[i]:0;}
    checkpoint(cancelled);
}
}
