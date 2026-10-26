#include "raw_pixel_features.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace qpx {
void RawPixelFeatureOptions::validate(std::uint32_t channels) const {
    if(!channels || channels>128 || offsets.size()!=channels || scales.size()!=channels) throw std::invalid_argument("raw features require one explicit offset and scale per channel");
    for(std::size_t c=0;c<channels;++c) if(!std::isfinite(offsets[c]) || (offsets[c]==0 && std::signbit(offsets[c])) || !std::isfinite(scales[c]) || scales[c]<=0) throw std::invalid_argument("invalid raw feature normalization");
    if(sigmas.empty() || sigmas.size()>8 || channels*sigmas.size()*(1+std::size_t(gradient_magnitude)+std::size_t(laplacian))>128) throw std::invalid_argument("raw feature schema exceeds scale/column limits");
    double previous_x=-1,previous_y=-1;
    for(const auto s:sigmas) {
        if(!std::isfinite(s.x) || !std::isfinite(s.y) || s.x<0 || s.y<0 || std::signbit(s.x) || std::signbit(s.y) || s.x>16 || s.y>16 || s.x<previous_x || s.y<previous_y || (s.x==previous_x && s.y==previous_y)) throw std::invalid_argument("raw feature scale pairs must increase componentwise within 0..16");
        previous_x=s.x;previous_y=s.y;
    }
    if(work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("raw feature work budget must be 1..1024 MiB");
}
}

namespace qpx::detail {
namespace {
void stopped(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("raw pixel feature extraction cancelled");}
std::string number(double v) {std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<v;return out.str();}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
std::size_t columns(std::uint32_t channels,const RawFeatureOptions& o) {return channels*o.sigmas.size()*(1+std::size_t(o.gradient_magnitude)+std::size_t(o.laplacian));}
// Unnormalized numerator/support are convolved along both axes, then divided
// once. A direct 2-D independently weighted reference is used by the tests.
void smooth(std::vector<double>& values,const std::vector<std::uint8_t>& valid,std::size_t width,std::size_t height,RawFeatureScale sigma,const std::atomic<bool>* cancelled) {
    if(sigma.x==0 && sigma.y==0) return;
    std::vector<double> weights(valid.begin(),valid.end()),temporary(values.size()),temporary_weights(values.size());
    for(int axis=0;axis<2;++axis) {
        const auto scale=axis?sigma.y:sigma.x;
        if(scale==0) continue;
        const auto radius=static_cast<int>(std::ceil(3*scale));std::vector<double> kernel(2*radius+1);
        for(int d=-radius;d<=radius;++d) {const auto ratio=double(d)/scale;kernel[d+radius]=d==0?1:std::exp(-.5*ratio*ratio);}
        for(std::size_t y=0;y<height;++y) {
            stopped(cancelled);
            for(std::size_t x=0;x<width;++x) {
                if((x&4095)==0) stopped(cancelled);
                double sum=0,weight=0;
                for(int d=-radius;d<=radius;++d) {
                    const auto xx=std::int64_t(x)+(axis==0?d:0),yy=std::int64_t(y)+(axis==1?d:0);
                    if(xx<0 || yy<0 || xx>=std::int64_t(width) || yy>=std::int64_t(height)) continue;
                    const auto i=std::size_t(yy)*width+std::size_t(xx);sum+=values[i]*kernel[d+radius];weight+=weights[i]*kernel[d+radius];
                }
                temporary[y*width+x]=sum;temporary_weights[y*width+x]=weight;
            }
        }
        values.swap(temporary);weights.swap(temporary_weights);
    }
    for(std::size_t i=0;i<values.size();++i) {if((i&4095)==0) stopped(cancelled);values[i]=weights[i]>0?values[i]/weights[i]:0;}
}
}
std::size_t raw_feature_work_bytes(const RawImageMetadata& metadata,const ScientificGrid& grid,const ScientificRawOptions& raw,const RawFeatureOptions& options) {
    options.validate(raw.channels);const auto channels=raw.channels;
    const auto width=std::uint64_t(grid.width()),height=std::uint64_t(grid.height());
    const auto rows=std::uint64_t(grid.x().samples())*std::uint64_t(grid.y().samples());
    // Includes captured HWC binary64/support/reasons, six simultaneous double
    // planes (base, values, weights, both temporaries and headroom), and masks.
    // Schema strings/recipe/metadata have a separately conservative fixed charge.
    const auto context_stride=std::uint64_t(channels)*10+50;
    const auto output_stride=std::uint64_t(columns(channels,options))*5+1;
    const auto reader_work=scientific_raw_work_bytes(metadata,grid,raw);
    // Raw capture includes metadata/string storage and the peak source read.
    // Charge it as well as the conservative feature workspace before pixel IO.
    if(reader_work>options.work_bytes || 262144>options.work_bytes-reader_work) throw std::length_error("raw feature input capture exceeds memory budget");
    const auto fixed=reader_work+262144;
    if(width>options.work_bytes/context_stride/height) throw std::length_error("raw feature context exceeds memory budget");
    auto bytes=std::size_t(width*height*context_stride);
    if(rows>(options.work_bytes-bytes)/output_stride) throw std::length_error("raw feature output exceeds memory budget");
    bytes+=std::size_t(rows*output_stride);
    if(fixed>options.work_bytes-bytes) throw std::length_error("raw feature scratch exceeds memory budget");
    // Context must be supplied even outside the source, where raw support is
    // invalid. This avoids silently inventing a block edge as an image edge.
    const auto derivative=options.gradient_magnitude || options.laplacian?1:0;
    const auto last=options.sigmas.back();
    if(grid.x().context()<std::ceil(3*last.x)+derivative || grid.y().context()<std::ceil(3*last.y)+derivative) throw std::invalid_argument("raw feature grid lacks required per-axis halo");
    return bytes+fixed;
}
std::vector<ClassifierFeature> raw_feature_schema(const ScientificRawGrid& input,const RawFeatureOptions& options) {return raw_feature_schema(input.metadata(),input.options(),options);}
std::vector<ClassifierFeature> raw_feature_schema(const RawImageMetadata& metadata,const ScientificRawOptions& raw,const RawFeatureOptions& options) {
    options.validate(raw.channels);std::vector<ClassifierFeature> result;result.reserve(columns(raw.channels,options));
    for(std::uint32_t c=0;c<raw.channels;++c) for(const auto s:options.sigmas) {
        const auto prefix="raw-pixel-v1."+sample_name(metadata.sample_type)+".c="+std::to_string(raw.first_channel+c)+".offset="+number(options.offsets[c])+".scale="+number(options.scales[c])+".sigma-x="+number(s.x)+".sigma-y="+number(s.y);
        result.emplace_back(prefix+".gaussian","normalized-raw");
        if(options.gradient_magnitude) result.emplace_back(prefix+".gradient-magnitude","normalized-raw/grid-px");
        if(options.laplacian) result.emplace_back(prefix+".laplacian","normalized-raw/grid-px2");
    }
    return result;
}
std::shared_ptr<const RawFeatureImage> extract_raw_pixel_features(const ScientificRawGrid& input,const RawFeatureOptions& options,const std::atomic<bool>* cancelled) {
    stopped(cancelled);const auto work=raw_feature_work_bytes(input.metadata(),input.grid(),input.options(),options);
    const auto width=std::size_t(input.width()),height=std::size_t(input.height()),count=width*height;
    const auto ow=std::size_t(input.grid().x().samples()),oh=std::size_t(input.grid().y().samples());
    const auto ox=std::size_t(input.grid().x().context()),oy=std::size_t(input.grid().y().context());
    auto output=std::make_shared<RawFeatureImage>();output->width=ow;output->height=oh;output->features=raw_feature_schema(input,options);output->estimated_work_bytes=work;
    const auto n=output->features.size();output->values.assign(ow*oh*n,std::numeric_limits<float>::quiet_NaN());output->valid.assign(ow*oh*n,0);output->rows_valid.assign(ow*oh,1);
    Sha256 source;text(source,"qpx.raw-pixel-feature-input/1");text(source,input.input_sha256());
    for(const auto& f:output->features) {text(source,f.name);text(source,f.unit);}output->input_sha256=source.finish();
    std::size_t column=0;
    for(std::size_t c=0;c<input.channels();++c) {
        std::vector<double> base(count);std::vector<std::uint8_t> valid(count);
        for(std::size_t i=0;i<count;++i) {
            if((i&4095)==0) stopped(cancelled);const auto index=i*input.channels()+c;valid[i]=input.valid()[index];
            if(valid[i]) {
                // Separate operations define the recipe: no implicit FMA,
                // clipping, adaptive scaling or float projection before filtering.
                const auto shifted=input.values()[index]-options.offsets[c];
                const auto value=shifted*options.scales[c];
                // Bound intermediate sums and derivatives, not just the output.
                if(!std::isfinite(shifted) || !std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max()/32) throw std::invalid_argument("raw feature normalization exceeds finite working range");
                base[i]=value;
            }
        }
        for(const auto sigma:options.sigmas) {
            auto values=base;smooth(values,valid,width,height,sigma,cancelled);
            for(std::size_t y=0;y<oh;++y) {
                stopped(cancelled);
                for(std::size_t x=0;x<ow;++x) {
                    const auto row=y*ow+x,gx=x+ox,gy=y+oy,index=gy*width+gx;
                    const auto stride=1+std::size_t(options.gradient_magnitude)+std::size_t(options.laplacian);
                    if(!valid[index]) {output->rows_valid[row]=0;continue;}
                    auto append=[&](std::size_t col,double value) {
                        if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max()/4) throw std::invalid_argument("raw feature exceeds finite forest range");
                        output->values[row*n+col]=static_cast<float>(value);output->valid[row*n+col]=1;
                    };
                    const auto center=values[index];append(column,center);
                    if(stride>1) {
                        const auto left=gx && valid[index-1]?values[index-1]:center,right=gx+1<width && valid[index+1]?values[index+1]:center;
                        const auto top=gy && valid[index-width]?values[index-width]:center,bottom=gy+1<height && valid[index+width]?values[index+width]:center;
                        auto col=column+1;
                        if(options.gradient_magnitude) append(col++,std::hypot((right-left)*.5,(bottom-top)*.5));
                        if(options.laplacian) append(col,left+right+top+bottom-4*center);
                    }
                }
            }
            column+=1+std::size_t(options.gradient_magnitude)+std::size_t(options.laplacian);
        }
    }
    Sha256 hash;text(hash,"qpx.raw-pixel-features/1");text(hash,output->input_sha256);hash.integer(output->width);hash.integer(output->height);hash.update(output->valid.data(),output->valid.size());
    for(std::size_t i=0;i<output->values.size();++i) {if((i&4095)==0) stopped(cancelled);std::uint32_t bits=0x7fc00000;if(output->valid[i]) std::memcpy(&bits,&output->values[i],4);hash.integer(bits);}
    output->sha256=hash.finish();stopped(cancelled);return output;
}
}
