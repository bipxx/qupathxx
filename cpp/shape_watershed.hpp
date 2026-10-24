#pragma once
#include "cell_numeric.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

namespace qpx::cell_detail {
// Native adaptation of the EDM-specific stages of ImageJ 1.54p MaximumFinder
// (Michael Schmid and Wayne Rasband). ImageJ is public-domain software; see
// third_party/imagej-shape-NOTICE.txt. No Java runtime is used by this engine.
// The distance transform is our exact squared-Euclidean implementation.
struct ShapeSplit {std::vector<std::uint8_t> mask;std::vector<std::uint32_t> maxima;std::vector<float> maxima_radii;};

class SampledShapeWatershed {
    static constexpr std::uint8_t maximum=1,listed=2,processed=4,area=8,equal=16,point=32,eliminated=64;
    static constexpr float root2=1.4142135624f;
    inline static constexpr std::array<int,8> dx={0,1,1,1,0,-1,-1,-1},dy={-1,-1,0,1,1,1,0,-1};
    std::size_t width_,height_,size_,limit_;
    std::uint64_t steps_=0;
    const std::atomic<bool>* cancelled_;
    std::vector<float> edm_,ridge_;
    std::vector<std::uint8_t> types_,levels_;
    std::vector<std::uint32_t> queue_;
    struct Peak {std::int32_t height;std::uint32_t index;};
    std::vector<Peak> peaks_;

    void step() {
        if((++steps_&4095)==0) checkpoint(cancelled_);
        if(steps_>1024ull*size_) throw std::length_error("shape watershed traversal limit exceeded");
    }
    bool neighbor(std::size_t i,int d,std::size_t& j) const {
        const auto x=i%width_,y=i/width_;
        if((dx[d]<0 && !x) || (dx[d]>0 && x+1==width_) || (dy[d]<0 && !y) || (dy[d]>0 && y+1==height_)) return false;
        j=static_cast<std::size_t>(static_cast<std::int64_t>(i)+dx[d]+static_cast<std::int64_t>(dy[d])*static_cast<std::int64_t>(width_));return true;
    }
    float ridge_height(std::size_t i) const {
        const auto x=i%width_,y=i/width_;const auto v=edm_[i];
        if(!x || !y || x+1==width_ || y+1==height_ || v==0) return v;
        float result=v+.5f*root2;bool ridge=false;
        for(int d=0;d<4;++d) {
            std::size_t a=0,b=0;neighbor(i,d,a);neighbor(i,d+4,b);
            float h;
            if(v>=edm_[a] && v>=edm_[b]) {ridge=true;h=(edm_[a]+edm_[b])/2.f;}
            else h=std::min(edm_[a],edm_[b]);
            h+=d%2?root2:1.f;result=std::min(result,h);
        }
        return ridge?result:v;
    }
    void find_candidates(float lo,float hi) {
        const float factor=static_cast<float>(2e9/(hi-lo));
        for(std::size_t i=0;i<size_;++i) {
            step();if(edm_[i]==lo) continue;bool is_max=true;
            for(int d=0;d<8;++d) {std::size_t j=0;if(neighbor(i,d,j) && edm_[j]>edm_[i] && ridge_[j]>ridge_[i]) {is_max=false;break;}}
            if(!is_max) continue;types_[i]=maximum;
            // Java float-to-int conversion saturates rather than overflowing.
            const double value=static_cast<float>((ridge_[i]-lo)*factor);
            const auto scaled=value>=std::numeric_limits<std::int32_t>::max()?std::numeric_limits<std::int32_t>::max():static_cast<std::int32_t>(value);
            peaks_.push_back({scaled,static_cast<std::uint32_t>(i)});
        }
        std::sort(peaks_.begin(),peaks_.end(),[](const Peak& a,const Peak& b){return a.height!=b.height?a.height<b.height:a.index<b.index;});checkpoint(cancelled_);
    }
    void mark_maxima(double prominence) {
        const float sort_error=1.1f*(root2/2.f);std::size_t accepted=0;
        for(auto it=peaks_.rbegin();it!=peaks_.rend();++it) {
            step();std::size_t start=it->index;if(types_[start]&processed) continue;float height=ridge_[start];bool retry;
            do {
                queue_.clear();queue_.push_back(static_cast<std::uint32_t>(start));types_[start]|=equal|listed;
                bool possible=true;retry=false;double sx=double(start%width_),sy=double(start/width_);std::size_t nequal=1;
                for(std::size_t at=0;at<queue_.size();++at) {
                    const auto i=queue_[at];
                    for(int d=0;d<8;++d) {
                        step();std::size_t j=0;if(!neighbor(i,d,j) || (types_[j]&listed) || edm_[j]<=0) continue;
                        if(types_[j]&processed) {possible=false;break;}
                        const auto other=ridge_[j];
                        if(other>height+sort_error) {possible=false;break;}
                        if(other>=height-static_cast<float>(prominence)) {
                            if(other>height) {retry=true;start=j;height=other;}
                            queue_.push_back(static_cast<std::uint32_t>(j));types_[j]|=listed;
                            if(other==height) {types_[j]|=equal;sx+=double(j%width_);sy+=double(j/width_);++nequal;}
                        }
                    }
                }
                if(retry) {for(auto i:queue_) types_[i]=0;continue;}
                const auto reset=static_cast<std::uint8_t>(~(possible?listed:(listed|equal)));sx/=nequal;sy/=nequal;
                double nearest=std::numeric_limits<double>::infinity();std::size_t center=start;
                for(auto i:queue_) {
                    step();types_[i]&=reset;types_[i]|=processed;
                    if(possible) {
                        types_[i]|=area;
                        if(types_[i]&equal) {const double x=sx-double(i%width_),y=sy-double(i/width_);const auto distance=x*x+y*y;if(distance<nearest) {nearest=distance;center=i;}}
                    }
                }
                if(possible) {if(++accepted>limit_) throw std::length_error("shape watershed maximum candidate limit exceeded");types_[center]|=point;}
            } while(retry);
        }
        if(peaks_.empty()) std::fill(types_.begin(),types_.end(),processed|area);
    }
    void quantize(float high) {
        const double offset=1.-(double(high)-1.)*(1./253/2-1e-6);
        const double factor=high==1?1.:std::min(1.,253/(double(high)-1.));
        for(std::size_t i=0;i<size_;++i) {
            step();if(edm_[i]<.5f) levels_[i]=0;
            else if(types_[i]&area) levels_[i]=255;
            else {const auto value=1+static_cast<std::int64_t>(std::floor((edm_[i]-offset)*factor+.5));levels_[i]=static_cast<std::uint8_t>(std::clamp<std::int64_t>(value,1,254));}
        }
    }
    void flatten_suppressed() {
        for(auto it=peaks_.rbegin();it!=peaks_.rend();++it) {
            step();const auto start=it->index;if(types_[start]&(area|eliminated)) continue;
            int low=int(levels_[start])+1;queue_.clear();queue_.push_back(start);types_[start]|=listed;std::size_t last=1;bool saddle=false;
            while(!saddle && low>0) {
                --low;last=queue_.size();
                for(std::size_t at=0;at<queue_.size() && !saddle;++at) {
                    const auto i=queue_[at];
                    for(int d=0;d<8;++d) {
                        step();std::size_t j=0;if(!neighbor(i,d,j) || (types_[j]&listed)) continue;
                        if((types_[j]&area) || ((types_[j]&eliminated) && levels_[j]>=low)) {saddle=true;break;}
                        if(levels_[j]>=low && !(types_[j]&eliminated)) {queue_.push_back(static_cast<std::uint32_t>(j));types_[j]|=listed;}
                    }
                }
            }
            for(auto i:queue_) types_[i]&=static_cast<std::uint8_t>(~listed);
            for(std::size_t at=0;at<last;++at) {const auto i=queue_[at];levels_[i]=static_cast<std::uint8_t>(low);types_[i]|=eliminated;}
        }
    }
    static std::array<std::uint8_t,256> fate_table() {
        std::array<std::uint8_t,256> table{};
        for(unsigned bits=0;bits<256;++bits) {
            std::array<bool,8> present{};
            for(unsigned d=0;d<8;++d) {present[d]=(bits&(1u<<d))!=0;if(bits&(1u<<((d+4)%8))) table[bits]|=static_cast<std::uint8_t>(1u<<d);}
            for(unsigned d=0;d<8;d+=2) if(present[d]) {present[(d+1)%8]=true;present[(d+7)%8]=true;}
            unsigned transitions=0;for(unsigned d=0;d<8;++d) transitions+=present[d]!=present[(d+1)%8];
            if(transitions>=4) table[bits]=0;
        }
        return table;
    }
    void grow_levels() {
        std::array<std::size_t,256> histogram{},start{},used{};for(auto v:levels_) ++histogram[v];
        int highest=0;std::size_t count=0;
        for(int level=1;level<255;++level) {start[level]=count;count+=histogram[level];if(histogram[level]) highest=level;}
        queue_.resize(size_);for(std::size_t i=0;i<size_;++i) {const auto v=levels_[i];if(v && v<255) queue_[start[v]+used[v]++]=static_cast<std::uint32_t>(i);}
        const auto fate=fate_table();const std::array<int,8> sequence={7,3,1,5,0,4,2,6};std::vector<std::uint32_t> changed;changed.reserve(size_);
        for(int level=highest;level>=1;--level) {
            auto remaining=histogram[level];int idle=0;
            while(remaining && idle<8) {
                std::size_t direction=0;
                do {
                    changed.clear();std::size_t untouched=0;const auto pass=sequence[direction%8];
                    for(std::size_t at=0;at<remaining;++at) {
                        step();const auto i=queue_[start[level]+at];unsigned bits=0;
                        for(int d=0;d<8;++d) {std::size_t j=0;if(neighbor(i,d,j) && levels_[j]==255) bits|=1u<<d;}
                        if(fate[bits]&(1u<<pass)) changed.push_back(i);else queue_[start[level]+untouched++]=i;
                    }
                    for(auto i:changed) levels_[i]=255;remaining-=changed.size();if(!changed.empty()) idle=0;++direction;
                } while(remaining && idle++<8);
            }
            if(remaining && level>1) {
                auto next=level;do {--next;} while(next>1 && !histogram[next]);auto end=start[next]+histogram[next];
                for(std::size_t at=0;at<remaining;++at) {
                    step();const auto i=queue_[start[level]+at];const auto x=i%width_,y=i/width_;bool border=!x || !y || x+1==width_ || y+1==height_;
                    for(int d=0;d<8 && !border;++d) {std::size_t j=0;if(neighbor(i,d,j) && !levels_[j]) border=true;}
                    if(border) {if(end>=queue_.size()) throw std::logic_error("shape watershed level capacity exceeded");queue_[end++]=i;}
                }
                histogram[next]=end-start[next];
            }
        }
    }
public:
    SampledShapeWatershed(std::size_t width,std::size_t height,std::size_t maximum_cells,const std::atomic<bool>* cancelled):width_(width),height_(height),size_(width*height),limit_(maximum_cells),cancelled_(cancelled) {}
    ShapeSplit run(const std::vector<std::uint8_t>& mask,double prominence,std::size_t work_bytes) {
        checkpoint(cancelled_);steps_=0;
        if(!width_ || !height_ || width_>std::numeric_limits<std::uint32_t>::max()/height_ || size_!=mask.size() || !limit_ ||
                !std::isfinite(prominence) || prominence<=0 || prominence>1000000) throw std::invalid_argument("invalid shape watershed input");
        // Includes output, float EDM/ridges, double EDT, types, quantized levels,
        // candidate ordering, work/change lists and the largest EDT line workspace.
        const auto line=std::max(width_,height_);if(line>work_bytes/64 || size_>(work_bytes-line*64)/64) throw std::length_error("shape watershed work budget exceeded");
        ShapeSplit output;output.mask.resize(size_);
        if(std::none_of(mask.begin(),mask.end(),[](auto v){return v==0;})) {std::fill(output.mask.begin(),output.mask.end(),1);return output;}
        if(std::none_of(mask.begin(),mask.end(),[](auto v){return v!=0;})) return output;
        std::vector<double> squared(size_);for(std::size_t i=0;i<size_;++i) squared[i]=mask[i]?infinity:0;
        cell_detail::distance(squared,width_,height_,false,nullptr,cancelled_);edm_.resize(size_);
        for(std::size_t i=0;i<size_;++i) edm_[i]=static_cast<float>(std::sqrt(squared[i]));std::vector<double>().swap(squared);
        ridge_.resize(size_);types_.assign(size_,0);levels_.resize(size_);queue_.clear();queue_.reserve(size_);peaks_.clear();
        for(std::size_t i=0;i<size_;++i) {step();ridge_[i]=ridge_height(i);}
        const auto extrema=std::minmax_element(edm_.begin(),edm_.end());find_candidates(*extrema.first,*extrema.second);
        mark_maxima(prominence);quantize(*extrema.second);flatten_suppressed();grow_levels();
        for(std::size_t i=0;i<size_;++i) {step();output.mask[i]=mask[i] && levels_[i]==255;if(types_[i]&point) {output.maxima.push_back(static_cast<std::uint32_t>(i));output.maxima_radii.push_back(edm_[i]);}}
        checkpoint(cancelled_);return output;
    }
};

inline ShapeSplit sampled_shape_watershed(const std::vector<std::uint8_t>& mask,std::size_t width,std::size_t height,double prominence=.5,
                                         std::size_t maximum_cells=10000,std::size_t work_bytes=256*1024*1024,const std::atomic<bool>* cancelled=nullptr) {
    return SampledShapeWatershed(width,height,maximum_cells,cancelled).run(mask,prominence,work_bytes);
}
}
