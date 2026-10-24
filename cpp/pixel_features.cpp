#include "qupathxx/pixel_features.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "masked_gaussian.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace qpx {
namespace {
void stopped(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("pixel feature extraction cancelled");}
std::string number(double v) {std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<v;return s.str();}
void text(Sha256& h,const std::string& s) {h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double v) {std::uint64_t b;std::memcpy(&b,&v,8);h.integer(b);}
std::string white_hash(const StainTransform& t) {Sha256 h;const std::string tag="qpx.pixel-white/1";h.update(tag.data(),tag.size());for(auto v:t.white()) real(h,v);return h.finish();}
struct Plan {PixelFeatureWindow window;std::int64_t x,y,w,h;std::size_t work;};
Plan plan(std::int64_t iw,std::int64_t ih,const PixelFeatureOptions& options,std::optional<PixelFeatureWindow> region) {
    options.validate();if(iw<=0 || ih<=0) throw std::invalid_argument("pixel feature input dimensions must be positive");
    const auto r=region.value_or(PixelFeatureWindow{iw,ih,0,0,iw,ih});
    if(r.image_width<=0 || r.image_height<=0 || r.input_x<0 || r.input_y<0 || iw>r.image_width || ih>r.image_height || r.input_x>r.image_width-iw || r.input_y>r.image_height-ih || r.x<r.input_x || r.y<r.input_y || r.width<=0 || r.height<=0 || r.width>iw || r.height>ih || r.x-r.input_x>iw-r.width || r.y-r.input_y>ih-r.height) throw std::invalid_argument("pixel feature output/input window exceeds image bounds");
    const auto halo=options.halo();const auto x=std::max<std::int64_t>(0,r.x-halo),y=std::max<std::int64_t>(0,r.y-halo);
    const auto right=r.x+r.width+std::min(halo,r.image_width-r.x-r.width),bottom=r.y+r.height+std::min(halo,r.image_height-r.y-r.height);
    if(x<r.input_x || y<r.input_y || right>r.input_x+iw || bottom>r.input_y+ih) throw std::invalid_argument("pixel feature input does not contain the required image context halo");
    const auto width=static_cast<std::uint64_t>(right-x),height=static_cast<std::uint64_t>(bottom-y);
    if(width>options.work_bytes/128/height) throw std::length_error("pixel feature working context exceeds memory budget");
    std::size_t bytes=static_cast<std::size_t>(width*height*128);
    const auto outw=static_cast<std::uint64_t>(r.width),outh=static_cast<std::uint64_t>(r.height);
    const auto stride=4*options.feature_count()+1;
    if(outw>(options.work_bytes-bytes)/stride/outh) throw std::length_error("pixel feature output exceeds memory budget");
    bytes+=static_cast<std::size_t>(outw*outh*stride);if(bytes>options.work_bytes || 65536>options.work_bytes-bytes) throw std::length_error("pixel feature scratch exceeds memory budget");bytes+=65536;
    return {r,x,y,right-x,bottom-y,bytes};
}
void validate_pixels(const PixelBuffer& input) {
    if(input.width()<=0 || input.height()<=0 || std::uint64_t(input.width())>std::numeric_limits<std::size_t>::max()/4/std::uint64_t(input.height()) || input.bytes().size()!=std::size_t(input.width())*std::size_t(input.height())*4) throw std::invalid_argument("invalid pixel feature RGBA storage");
}
std::string input_hash(const PixelBuffer& input,const Plan& p,const std::vector<ClassifierFeature>& features,std::vector<std::uint8_t>* copy,const std::atomic<bool>* cancelled) {
    Sha256 hash;const std::string tag="qpx.pixel-feature-input/1";hash.update(tag.data(),tag.size());
    for(auto v:{p.window.image_width,p.window.image_height,p.window.x,p.window.y,p.window.width,p.window.height,p.x,p.y,p.w,p.h}) hash.integer(v);
    hash.integer(features.size());for(const auto& f:features) {text(hash,f.name);text(hash,f.unit);}
    const auto width=static_cast<std::size_t>(p.w);for(std::int64_t y=0;y<p.h;++y) {
        stopped(cancelled);const auto offset=static_cast<std::size_t>(p.y-p.window.input_y+y)*static_cast<std::size_t>(input.width())+static_cast<std::size_t>(p.x-p.window.input_x);
        const auto* row=input.bytes().data()+offset*4;if(copy) {auto* target=copy->data()+static_cast<std::size_t>(y)*width*4;std::copy_n(row,width*4,target);row=target;}hash.update(row,width*4);
    }
    stopped(cancelled);return hash.finish();
}
}
void PixelFeatureOptions::validate() const {
    if(!rgb && !optical_density && !stains) throw std::invalid_argument("pixel features require at least one channel group");
    if(sigmas.empty() || sigmas.size()>8) throw std::invalid_argument("pixel feature scales require 1..8 values");
    double previous=-1;for(auto sigma:sigmas) {if(!std::isfinite(sigma) || (sigma==0 && std::signbit(sigma)) || sigma<0 || sigma>16 || sigma<=previous) throw std::invalid_argument("pixel feature sigmas must be finite, increasing and within 0..16");previous=sigma;}
    if(!feature_count() || feature_count()>128) throw std::invalid_argument("pixel feature schema exceeds 128 columns");
    if(work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("pixel feature work budget must be 1..1024 MiB");
}
std::size_t PixelFeatureOptions::feature_count() const {return 3*(std::size_t(rgb)+std::size_t(optical_density)+std::size_t(stains))*sigmas.size()*(1+std::size_t(gradient_magnitude)+std::size_t(laplacian));}
std::int64_t PixelFeatureOptions::halo() const {validate();return static_cast<std::int64_t>(std::ceil(3*sigmas.back()))+(gradient_magnitude || laplacian?1:0);}
std::vector<ClassifierFeature> pixel_feature_schema(const PixelFeatureOptions& options,const StainTransform& transform) {
    options.validate();std::vector<ClassifierFeature> out;out.reserve(options.feature_count());
    for(int group=0;group<3;++group) {
        if(!(group==0?options.rgb:group==1?options.optical_density:options.stains)) continue;
        const auto prefix=group==0?std::string("pixel-v1.rgb."):group==1?"pixel-v1.od."+white_hash(transform)+".":"pixel-v1.stain."+stain_model_sha256(transform)+".";
        const std::string unit=group==0?"device-rgb8":"OD";
        for(int channel=0;channel<3;++channel) for(auto sigma:options.sigmas) {
            const auto name=prefix+(group==2?std::to_string(channel+1):std::vector<std::string>{"red","green","blue"}[channel])+".sigma="+number(sigma);
            out.emplace_back(name+".gaussian",unit);
            if(options.gradient_magnitude) out.emplace_back(name+".gradient-magnitude",unit+"/grid-px");
            if(options.laplacian) out.emplace_back(name+".laplacian",unit+"/grid-px2");
        }
    }
    return out;
}
std::size_t pixel_feature_work_bytes(std::int64_t width,std::int64_t height,const PixelFeatureOptions& options,std::optional<PixelFeatureWindow> window) {return plan(width,height,options,window).work;}
std::string pixel_feature_input_sha256(const PixelBuffer& input,const StainTransform& transform,PixelFeatureOptions options,std::optional<PixelFeatureWindow> window,const std::atomic<bool>* cancelled) {
    stopped(cancelled);validate_pixels(input);const auto p=plan(input.width(),input.height(),options,window);return input_hash(input,p,pixel_feature_schema(options,transform),nullptr,cancelled);
}
std::size_t PixelFeatureImage::retained_bytes() const {std::size_t n=sizeof(PixelFeatureImage)+values_.capacity()*sizeof(float)+valid_.capacity()+features_.capacity()*sizeof(ClassifierFeature)+options_.sigmas.capacity()*sizeof(double)+sha256_.capacity()+input_sha256_.capacity();for(const auto& f:features_) n+=f.name.capacity()+f.unit.capacity();return n;}
std::shared_ptr<const PixelFeatureImage> extract_pixel_features(const PixelBuffer& input,const StainTransform& transform,PixelFeatureOptions options,std::optional<PixelFeatureWindow> window,const std::atomic<bool>* cancelled) {
    const auto started=std::chrono::steady_clock::now();stopped(cancelled);validate_pixels(input);const auto p=plan(input.width(),input.height(),options,window);
    auto out=std::shared_ptr<PixelFeatureImage>(new PixelFeatureImage());out->window_=p.window;out->options_=std::move(options);out->features_=pixel_feature_schema(out->options_,transform);out->white_=transform.white();out->stains_=transform.stains();out->work_bytes_=p.work;out->cx_=p.x;out->cy_=p.y;out->cw_=p.w;out->ch_=p.h;
    const auto width=static_cast<std::size_t>(p.w),height=static_cast<std::size_t>(p.h),count=width*height,rows=static_cast<std::size_t>(p.window.width)*static_cast<std::size_t>(p.window.height),columns=out->features_.size();
    PixelBuffer pixels(p.w,p.h);auto& bytes=pixels.writable_bytes();
    out->input_sha256_=input_hash(input,p,out->features_,&bytes,cancelled);std::vector<std::uint8_t> domain(count);for(std::size_t i=0;i<count;++i) {if((i&4095)==0) stopped(cancelled);domain[i]=bytes[i*4+3]==255;}
    out->values_.assign(rows*columns,std::numeric_limits<float>::quiet_NaN());out->valid_.resize(rows);
    const auto ox=static_cast<std::size_t>(p.window.x-p.x),oy=static_cast<std::size_t>(p.window.y-p.y),ow=static_cast<std::size_t>(p.window.width),oh=static_cast<std::size_t>(p.window.height);
    for(std::size_t y=0;y<oh;++y) {stopped(cancelled);for(std::size_t x=0;x<ow;++x) {const auto valid=domain[(oy+y)*width+ox+x];out->valid_[y*ow+x]=valid;out->valid_pixels_+=valid;}}
    std::size_t column=0;
    for(int group=0;group<3;++group) {
        if(!(group==0?out->options_.rgb:group==1?out->options_.optical_density:out->options_.stains)) continue;
        std::optional<FloatRgbImage> converted;if(group==1) converted=transform.optical_densities(pixels,cancelled);else if(group==2) converted=transform.deconvolve(pixels,cancelled);
        for(std::size_t channel=0;channel<3;++channel) {
            std::vector<double> base(count);for(std::size_t i=0;i<count;++i) {if((i&4095)==0) stopped(cancelled);if(domain[i]) base[i]=group==0?bytes[4*i+channel]:converted->values()[3*i+channel];}
            for(auto sigma:out->options_.sigmas) {
                stopped(cancelled);auto values=base;detail::masked_gaussian(values,domain,width,height,sigma,[&]{stopped(cancelled);});
                auto append=[&](std::size_t row,std::size_t feature,double value) {if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max()/4) throw std::invalid_argument("pixel feature exceeds finite float32 range");out->values_[row*columns+feature]=static_cast<float>(value);};
                for(std::size_t y=0;y<oh;++y) {stopped(cancelled);for(std::size_t x=0;x<ow;++x) {
                    const auto row=y*ow+x;if(!out->valid_[row]) continue;const auto gx=ox+x,gy=oy+y,index=gy*width+gx;const auto center=values[index];append(row,column,center);std::size_t feature=column+1;
                    if(out->options_.gradient_magnitude || out->options_.laplacian) {
                        const auto left=gx && domain[index-1]?values[index-1]:center,right=gx+1<width && domain[index+1]?values[index+1]:center;
                        const auto top=gy && domain[index-width]?values[index-width]:center,bottom=gy+1<height && domain[index+width]?values[index+width]:center;
                        if(out->options_.gradient_magnitude) append(row,feature++,std::hypot((right-left)*.5,(bottom-top)*.5));
                        if(out->options_.laplacian) append(row,feature,left+right+top+bottom-4*center);
                    }
                }}
                column+=1+std::size_t(out->options_.gradient_magnitude)+std::size_t(out->options_.laplacian);
            }
        }
    }
    Sha256 hash;const std::string tag="qpx.pixel-features/1";hash.update(tag.data(),tag.size());
    for(auto v:{p.window.image_width,p.window.image_height,p.window.x,p.window.y,p.window.width,p.window.height}) hash.integer(v);
    hash.integer(out->features_.size());for(const auto& f:out->features_) {text(hash,f.name);text(hash,f.unit);}
    hash.update(out->valid_.data(),out->valid_.size());
    for(std::size_t i=0;i<out->values_.size();++i) {if((i&4095)==0) stopped(cancelled);std::uint32_t b=0x7fc00000;if(std::isfinite(out->values_[i])) std::memcpy(&b,&out->values_[i],4);hash.integer(b);}
    out->sha256_=hash.finish();out->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();stopped(cancelled);return out;
}
}
