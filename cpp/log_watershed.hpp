#pragma once
#include "cell_numeric.hpp"
#include <map>
#include <numeric>
#include <tuple>

namespace qpx::cell_detail {
// Independently implemented discrete LoG regions and shape watershed. This is
// native analysis code; it neither calls Python nor embeds an upstream plugin.
struct LogParameters {
    double sigma=1, threshold=.4, prominence=.5, minimum_radius=1, minimum_spacing=4;
    std::uint64_t minimum_area=16, maximum_area=512;
    int peak_connectivity=8;
    std::size_t maximum_cells=10000, work_bytes=256*1024*1024;
    void validate() const {
        if(!std::isfinite(sigma) || sigma<0 || sigma>16 || !std::isfinite(threshold) ||
           !std::isfinite(prominence) || prominence<=0 || prominence>1000000 ||
           !std::isfinite(minimum_radius) || minimum_radius<0 || minimum_radius>1000000 ||
           !std::isfinite(minimum_spacing) || minimum_spacing<0 || minimum_spacing>1000000 ||
           !minimum_area || (maximum_area && maximum_area<minimum_area) || !maximum_cells ||
           (peak_connectivity!=4 && peak_connectivity!=8))
            throw std::invalid_argument("invalid LoG watershed parameters");
    }
};
struct LogPeak {std::uint32_t index=0,label=0;double radius=0;};
struct LogResult {std::vector<std::uint32_t> labels;std::vector<LogPeak> peaks;std::size_t count=0;};

template<class F> inline void log_neighbors(std::size_t i,std::size_t width,std::size_t height,bool diagonal,F f) {
    const auto x=i%width,y=i/width;
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        if((!dx && !dy) || (!diagonal && dx && dy)) continue;
        if((dx<0 && !x) || (dy<0 && !y) || (dx>0 && x+1==width) || (dy>0 && y+1==height)) continue;
        f(static_cast<std::size_t>(static_cast<std::int64_t>(i)+dy*static_cast<std::int64_t>(width)+dx));
    }
}

inline std::vector<std::uint8_t> log_positive(const std::vector<float>& score,std::size_t width,std::size_t height,
                                             double sigma,const std::atomic<bool>* cancelled) {
    auto blurred=score;
    if(sigma>0) {
        const int radius=static_cast<int>(std::ceil(3*sigma));
        std::vector<double> weights(2*radius+1);double total=0;
        for(int d=-radius;d<=radius;++d) {const double scaled=d/sigma;total+=weights[d+radius]=std::exp(-.5*scaled*scaled);}
        for(auto& w:weights) w/=total;
        std::vector<float> intermediate(score.size());
        // Explicit nearest-edge extension, with float storage between passes.
        for(std::size_t y=0;y<height;++y) {
            checkpoint(cancelled);
            for(std::size_t x=0;x<width;++x) {
                double sum=0;
                for(int d=-radius;d<=radius;++d) {
                    const auto row=std::clamp<std::int64_t>(static_cast<std::int64_t>(y)+d,0,static_cast<std::int64_t>(height)-1);
                    sum+=weights[d+radius]*score[static_cast<std::size_t>(row)*width+x];
                }
                intermediate[y*width+x]=static_cast<float>(sum);
            }
        }
        for(std::size_t y=0;y<height;++y) {
            checkpoint(cancelled);
            for(std::size_t x=0;x<width;++x) {
                double sum=0;
                for(int d=-radius;d<=radius;++d) {
                    const auto column=std::clamp<std::int64_t>(static_cast<std::int64_t>(x)+d,0,static_cast<std::int64_t>(width)-1);
                    sum+=weights[d+radius]*intermediate[y*width+static_cast<std::size_t>(column)];
                }
                blurred[y*width+x]=static_cast<float>(sum);
            }
        }
    }
    std::vector<std::uint8_t> positive(score.size());
    for(std::size_t y=0;y<height;++y) {
        checkpoint(cancelled);
        for(std::size_t x=0;x<width;++x) {
            const auto i=y*width+x;
            const double laplacian=4.*blurred[i]-blurred[(y?y-1:y)*width+x]-blurred[(y+1<height?y+1:y)*width+x]
                -blurred[y*width+(x?x-1:x)]-blurred[y*width+(x+1<width?x+1:x)];
            positive[i]=laplacian>0;
        }
    }
    return positive;
}

inline void log_fill_holes(std::vector<std::uint8_t>& mask,std::size_t width,std::size_t height,const std::atomic<bool>* cancelled) {
    std::vector<std::uint8_t> exterior(mask.size());std::vector<std::uint32_t> queue;queue.reserve(mask.size());
    auto add=[&](std::size_t i) {if(!mask[i] && !exterior[i]) {exterior[i]=1;queue.push_back(static_cast<std::uint32_t>(i));}};
    for(std::size_t x=0;x<width;++x) {add(x);add((height-1)*width+x);}
    for(std::size_t y=0;y<height;++y) {add(y*width);add(y*width+width-1);}
    for(std::size_t at=0;at<queue.size();++at) {
        if((at&4095)==0) checkpoint(cancelled);
        log_neighbors(queue[at],width,height,false,add);
    }
    for(std::size_t i=0;i<mask.size();++i) {if((i&4095)==0) checkpoint(cancelled);mask[i]=mask[i] || !exterior[i];}
}

inline std::vector<std::uint8_t> log_regions(const std::vector<float>& score,std::size_t width,std::size_t height,
                                            const LogParameters& p,const std::atomic<bool>* cancelled) {
    const auto n=score.size();const auto positive=log_positive(score,width,height,p.sigma,cancelled);
    std::vector<std::uint8_t> mask(n),state(n);std::vector<std::uint32_t> components(n),queue;queue.reserve(n);
    std::uint32_t id=0;std::uint64_t box_work=0;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(!positive[i] || components[i]) continue;
        ++id;queue.clear();queue.push_back(static_cast<std::uint32_t>(i));components[i]=id;
        std::size_t left=i%width,right=left,top=i/width,bottom=top;double sum=0;
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            const auto j=queue[at];sum+=score[j];const auto x=j%width,y=j/width;
            left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
            log_neighbors(j,width,height,false,[&](std::size_t k) {if(positive[k] && !components[k]) {components[k]=id;queue.push_back(static_cast<std::uint32_t>(k));}});
        }
        // Apply intensity to the unfilled component, rather than diluting it
        // with its enclosed pixels before deciding whether it is a nucleus.
        if(sum/queue.size()<=p.threshold) continue;
        box_work+=(right-left+1)*(bottom-top+1);
        if(box_work>64ull*n) throw std::length_error("LoG component filling exceeds traversal limit");
        queue.clear();
        for(auto y=top;y<=bottom;++y) for(auto x=left;x<=right;++x) state[y*width+x]=components[y*width+x]==id?2:0;
        auto add=[&](std::size_t j) {if(!state[j]) {state[j]=1;queue.push_back(static_cast<std::uint32_t>(j));}};
        for(auto x=left;x<=right;++x) {add(top*width+x);add(bottom*width+x);}
        for(auto y=top;y<=bottom;++y) {add(y*width+left);add(y*width+right);}
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            log_neighbors(queue[at],width,height,false,[&](std::size_t j) {
                if(j%width>=left && j%width<=right && j/width>=top && j/width<=bottom) add(j);
            });
        }
        for(auto y=top;y<=bottom;++y) {
            checkpoint(cancelled);
            for(auto x=left;x<=right;++x) if(state[y*width+x]!=1) mask[y*width+x]=1;
        }
    }
    std::vector<std::uint8_t> expanded(n);
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(!positive[i]) continue;
        bool near=mask[i]!=0;log_neighbors(i,width,height,true,[&](std::size_t j) {near=near || mask[j];});expanded[i]=near;
    }
    log_fill_holes(expanded,width,height,cancelled);return expanded;
}

inline std::vector<std::uint8_t> log_h_maxima(const std::vector<double>& distance,const std::vector<std::uint8_t>& mask,
                                            std::size_t width,std::size_t height,double prominence,const std::atomic<bool>* cancelled) {
    // Max-min reconstruction with one indexed heap entry per foreground pixel.
    // Raising a tentative value sifts it up; finalized values cannot be raised
    // by any later (lower) maximum. Heap storage is O(n), without stale entries.
    const auto none=std::numeric_limits<std::uint32_t>::max();const auto n=mask.size();
    std::vector<double> reconstructed(n);std::vector<std::uint32_t> heap,position(n,none);heap.reserve(n);
    for(std::size_t i=0;i<n;++i) if(mask[i]) {
        reconstructed[i]=std::max(0.,distance[i]-prominence);position[i]=static_cast<std::uint32_t>(heap.size());heap.push_back(static_cast<std::uint32_t>(i));
    }
    auto higher=[&](std::uint32_t a,std::uint32_t b) {return reconstructed[a]>reconstructed[b] || (reconstructed[a]==reconstructed[b] && a<b);};
    auto exchange=[&](std::size_t a,std::size_t b) {std::swap(heap[a],heap[b]);position[heap[a]]=static_cast<std::uint32_t>(a);position[heap[b]]=static_cast<std::uint32_t>(b);};
    auto down=[&](std::size_t at) {
        while(at*2+1<heap.size()) {auto next=at*2+1;if(next+1<heap.size() && higher(heap[next+1],heap[next])) ++next;
            if(!higher(heap[next],heap[at])) break;exchange(at,next);at=next;}
    };
    for(auto i=heap.size()/2;i>0;--i) down(i-1);
    std::size_t processed=0;
    while(!heap.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);
        const auto i=heap.front();exchange(0,heap.size()-1);heap.pop_back();position[i]=none;if(!heap.empty()) down(0);
        log_neighbors(i,width,height,true,[&](std::size_t j) {
            if(position[j]==none) return;const auto proposed=std::min(reconstructed[i],distance[j]);if(proposed<=reconstructed[j]) return;
            reconstructed[j]=proposed;auto at=static_cast<std::size_t>(position[j]);
            while(at && higher(heap[at],heap[(at-1)/2])) {exchange(at,(at-1)/2);at=(at-1)/2;}
        });
    }
    std::vector<std::uint8_t> maxima(n);
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);
        const auto tolerance=8*std::numeric_limits<double>::epsilon()*std::max(1.,distance[i]);
        maxima[i]=mask[i] && distance[i]-reconstructed[i]>=prominence-tolerance;
    }
    return maxima;
}

inline LogResult log_watershed(const std::vector<float>& score,const std::vector<std::uint8_t>& domain,
                               std::size_t width,std::size_t height,const LogParameters& p={},const std::atomic<bool>* cancelled=nullptr) {
    p.validate();checkpoint(cancelled);
    if(!width || !height || width>std::numeric_limits<std::uint32_t>::max()/height || width*height!=score.size() || domain.size()!=score.size())
        throw std::invalid_argument("invalid LoG raster dimensions");
    const auto n=score.size();if(n>p.work_bytes/128) throw std::length_error("LoG raster exceeds work budget");
    for(auto v:score) if(!std::isfinite(v)) throw std::invalid_argument("nonfinite LoG feature");
    auto mask=log_regions(score,width,height,p,cancelled);
    std::vector<double> distance(n);for(std::size_t i=0;i<n;++i) distance[i]=mask[i]?infinity:0;
    cell_detail::distance(distance,width,height,true,nullptr,cancelled);for(auto& value:distance) value=std::sqrt(value);
    auto maxima=log_h_maxima(distance,mask,width,height,p.prominence,cancelled);
    std::vector<std::uint32_t> markers(n),queue;queue.reserve(n);LogResult result;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);if(!maxima[i] || markers[i]) continue;
        if(result.peaks.size()>=p.maximum_cells) throw std::length_error("LoG maximum candidate limit exceeded");
        const auto id=static_cast<std::uint32_t>(result.peaks.size()+1);queue.clear();queue.push_back(static_cast<std::uint32_t>(i));markers[i]=id;double radius=0;
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);const auto j=queue[at];radius=std::max(radius,distance[j]);
            log_neighbors(j,width,height,p.peak_connectivity==8,[&](std::size_t k) {if(maxima[k] && !markers[k]) {markers[k]=id;queue.push_back(static_cast<std::uint32_t>(k));}});
        }
        result.peaks.push_back({static_cast<std::uint32_t>(i),id,radius});
    }
    std::vector<std::uint32_t> order(result.peaks.size());std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](auto a,auto b) {return result.peaks[a].radius!=result.peaks[b].radius?result.peaks[a].radius>result.peaks[b].radius:result.peaks[a].index<result.peaks[b].index;});
    std::vector<std::uint8_t> retained(result.peaks.size()+1);
    {
    std::map<std::pair<std::int64_t,std::int64_t>,std::vector<std::uint32_t>> bins;
    const auto spacing=std::max(1.,p.minimum_spacing);
    for(auto index:order) {
        checkpoint(cancelled);const auto& peak=result.peaks[index];if(peak.radius<p.minimum_radius) continue;bool close=false;
        const auto x=double(peak.index%width),y=double(peak.index/width);
        const auto gx=static_cast<std::int64_t>(std::floor(x/spacing)),gy=static_cast<std::int64_t>(std::floor(y/spacing));
        if(p.minimum_spacing>0) for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            const auto found=bins.find({gx+dx,gy+dy});if(found==bins.end()) continue;
            for(auto old:found->second) {
                const auto j=result.peaks[old].index;const auto xx=x-double(j%width),yy=y-double(j/width);
                if(xx*xx+yy*yy<p.minimum_spacing*p.minimum_spacing) {close=true;break;}
            }
        }
        if(!close) {retained[peak.label]=1;if(p.minimum_spacing>0) bins[{gx,gy}].push_back(index);}
    }
    }
    struct Node {double cost;std::uint64_t age;std::uint32_t index,label;};
    auto later=[](const Node& a,const Node& b) {return std::tie(a.cost,a.age)>std::tie(b.cost,b.age);};
    result.labels.resize(n);std::vector<std::uint8_t> state(n);std::vector<Node> heap;heap.reserve(n);std::uint64_t age=0;
    for(std::size_t i=0;i<n;++i) if(markers[i] && retained[markers[i]]) {state[i]=1;heap.push_back({-distance[i],age++,static_cast<std::uint32_t>(i),markers[i]});}
    std::make_heap(heap.begin(),heap.end(),later);std::size_t processed=0;
    while(!heap.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);std::pop_heap(heap.begin(),heap.end(),later);const auto next=heap.back();heap.pop_back();state[next.index]=2;
        bool conflict=false;
        if(!markers[next.index] || !retained[markers[next.index]]) log_neighbors(next.index,width,height,false,[&](std::size_t j) {if(result.labels[j] && result.labels[j]!=next.label) conflict=true;});
        if(conflict) continue;result.labels[next.index]=next.label;
        log_neighbors(next.index,width,height,false,[&](std::size_t j) {
            if(!mask[j] || state[j]) return;state[j]=1;heap.push_back({std::max(next.cost,-distance[j]),age++,static_cast<std::uint32_t>(j),next.label});std::push_heap(heap.begin(),heap.end(),later);
        });
    }
    std::vector<std::uint64_t> count(result.peaks.size()+1);std::vector<double> sum(result.peaks.size()+1);
    for(std::size_t i=0;i<n;++i) if(result.labels[i]) {++count[result.labels[i]];sum[result.labels[i]]+=score[i];}
    for(auto& peak:result.peaks) retained[peak.label]=count[peak.label]>=p.minimum_area && (!p.maximum_area || count[peak.label]<=p.maximum_area) && sum[peak.label]/std::max<std::uint64_t>(1,count[peak.label])>=p.threshold;
    std::vector<std::uint32_t> dense(result.peaks.size()+1);
    for(std::size_t i=0;i<n;++i) if(domain[i] && result.labels[i] && retained[result.labels[i]]) dense[result.labels[i]]=1;
    for(auto& id:dense) if(id) id=static_cast<std::uint32_t>(++result.count);
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);result.labels[i]=domain[i]?dense[result.labels[i]]:0;}
    for(auto& peak:result.peaks) peak.label=dense[peak.label];return result;
}
}
