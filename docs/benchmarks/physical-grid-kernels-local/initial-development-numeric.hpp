#pragma once
#include "cell_numeric.hpp"
#include "calibrated_cell_numeric.hpp"
#include "qupathxx/scientific_grid.hpp"
#include <utility>

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
inline std::uint64_t physical_distance_work_bytes(std::size_t width,std::size_t height,bool nearest) {
    const auto n=scientific_cell_pixels(width,height);
    // Includes caller's in-place distance array, optional retained nearest map,
    // shared line workspace and optional nearest-column propagation workspace.
    return (nearest?12:8)*n+32ULL*std::max(width,height)+(nearest?4ULL*height:0)+4096;
}
inline void physical_line(LineTransform& line,std::size_t n,double weight,const std::atomic<bool>* cancelled) {
    const auto inf=std::numeric_limits<double>::infinity();std::size_t used=0;
    for(std::size_t q=0;q<n;++q) {
        if((q&4095)==0) checkpoint(cancelled);if(!std::isfinite(line.input[q])) continue;
        double crossing=-inf;
        while(used) {
            const auto v=line.sites[used-1];
            // Avoid subtracting large squared coordinates or overflowing 2*w.
            crossing=((line.input[q]-line.input[v])/weight)/(2*(double(q)-double(v)))+.5*(double(q)+double(v));
            if(crossing>line.breaks[used-1]) break;--used;
        }
        line.sites[used]=static_cast<std::uint32_t>(q);line.breaks[used]=used?crossing:-inf;
        ++used;line.breaks[used]=inf;
    }
    std::size_t k=0;
    for(std::size_t q=0;q<n;++q) {
        if((q&4095)==0) checkpoint(cancelled);
        if(!used) {line.output[q]=inf;line.nearest[q]=std::numeric_limits<std::uint32_t>::max();continue;}
        while(k+1<used && line.breaks[k+1]<double(q)) ++k;
        auto best=line.sites[k];const auto d=double(q)-double(best);auto value=d*d*weight+line.input[best];
        // At a rounded intersection, compare adjacent envelope candidates
        // directly and retain the lower site for a binary64 cost tie.
        for(auto j=(k?k-1:k);j<std::min(used,k+2);++j) {
            const auto site=line.sites[j];const auto delta=double(q)-double(site),cost=delta*delta*weight+line.input[site];
            if(cost<value || (cost==value && site<best)) {best=site;value=cost;}
        }
        line.nearest[q]=best;line.output[q]=value;
    }
}
inline void physical_distance(std::vector<double>& values,std::size_t width,std::size_t height,
    const PhysicalGridMetric& metric,bool outside,std::vector<std::uint32_t>* nearest,
    std::size_t work_bytes,const std::atomic<bool>* cancelled=nullptr) {
    checkpoint(cancelled);const auto n=scientific_cell_pixels(width,height);
    if(values.size()!=n || (outside && nearest)) throw std::invalid_argument("invalid physical distance buffer or outside nearest request");
    scientific_cell_budget(physical_distance_work_bytes(width,height,nearest!=nullptr),work_bytes);
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
    LineTransform line(std::max(width,height));if(nearest) nearest->resize(n);
    for(std::size_t y=0;y<height;++y) {
        checkpoint(cancelled);for(std::size_t x=0;x<width;++x) line.input[x]=values[y*width+x];
        physical_line(line,width,wx,cancelled);
        for(std::size_t x=0;x<width;++x) {
            const auto a=double(x+1),b=double(width-x);
            values[y*width+x]=outside?std::min(std::min(a*a,b*b)*wx,line.output[x]):line.output[x];
            if(nearest) (*nearest)[y*width+x]=line.nearest[x]==std::numeric_limits<std::uint32_t>::max()?line.nearest[x]:static_cast<std::uint32_t>(y*width+line.nearest[x]);
        }
    }
    std::vector<std::uint32_t> column(nearest?height:0);
    for(std::size_t x=0;x<width;++x) {
        checkpoint(cancelled);
        for(std::size_t y=0;y<height;++y) {line.input[y]=values[y*width+x];if(nearest) column[y]=(*nearest)[y*width+x];}
        physical_line(line,height,wy,cancelled);
        for(std::size_t y=0;y<height;++y) {
            const auto a=double(y+1),b=double(height-y);
            values[y*width+x]=outside?std::min(std::min(a*a,b*b)*wy,line.output[y]):line.output[y];
            if(nearest) (*nearest)[y*width+x]=line.nearest[y]==std::numeric_limits<std::uint32_t>::max()?line.nearest[y]:column[line.nearest[y]];
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
