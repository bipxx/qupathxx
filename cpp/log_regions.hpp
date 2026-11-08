#pragma once
#include "log_common.hpp"

namespace qpx::cell_detail {
inline std::vector<std::uint8_t> log_positive(const std::vector<float>& score,std::size_t width,std::size_t height,
                                             double sigma,const std::atomic<bool>* cancelled,std::vector<float>* response=nullptr) {
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
    std::vector<std::uint8_t> positive(score.size());if(response) response->resize(score.size());
    for(std::size_t y=0;y<height;++y) {
        checkpoint(cancelled);
        for(std::size_t x=0;x<width;++x) {
            const auto i=y*width+x;
            const double laplacian=4.*blurred[i]-blurred[(y?y-1:y)*width+x]-blurred[(y+1<height?y+1:y)*width+x]
                -blurred[y*width+(x?x-1:x)]-blurred[y*width+(x+1<width?x+1:x)];
            positive[i]=laplacian>0;
            if(response) (*response)[i]=static_cast<float>(laplacian);
        }
    }
    return positive;
}

inline void log_fill_holes(std::vector<std::uint8_t>& mask,std::size_t width,std::size_t height,const std::atomic<bool>* cancelled,bool diagonal=false) {
    std::vector<std::uint8_t> exterior(mask.size());std::vector<std::uint32_t> queue;queue.reserve(mask.size());
    auto add=[&](std::size_t i) {if(!mask[i] && !exterior[i]) {exterior[i]=1;queue.push_back(static_cast<std::uint32_t>(i));}};
    for(std::size_t x=0;x<width;++x) {add(x);add((height-1)*width+x);}
    for(std::size_t y=0;y<height;++y) {add(y*width);add(y*width+width-1);}
    for(std::size_t at=0;at<queue.size();++at) {
        if((at&4095)==0) checkpoint(cancelled);
        log_neighbors(queue[at],width,height,diagonal,add);
    }
    for(std::size_t i=0;i<mask.size();++i) {if((i&4095)==0) checkpoint(cancelled);mask[i]=mask[i] || !exterior[i];}
}

template<class Score>
inline std::vector<std::uint8_t> log_regions_from_positive(const std::vector<Score>& score,const std::vector<std::uint8_t>& positive,
    std::size_t width,std::size_t height,const LogParameters& p,const std::atomic<bool>* cancelled) {
    const auto n=score.size();
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
            log_neighbors(queue[at],width,height,p.eight_connected_background,[&](std::size_t j) {
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
    log_fill_holes(expanded,width,height,cancelled,p.eight_connected_background);return expanded;
}
inline std::vector<std::uint8_t> log_regions(const std::vector<float>& score,std::size_t width,std::size_t height,
    const LogParameters& p,const std::atomic<bool>* cancelled) {
    return log_regions_from_positive(score,log_positive(score,width,height,p.sigma,cancelled),width,height,p,cancelled);
}

}
