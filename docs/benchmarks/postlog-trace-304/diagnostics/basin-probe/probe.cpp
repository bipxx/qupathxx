#include "log_watershed.hpp"
#include <fstream>
#include <iostream>
#include <string>
namespace qpx::cell_detail {
inline std::vector<std::uint32_t> trace_basins(const std::vector<float>& response,std::size_t width,std::size_t height,
                                                     std::size_t maximum_basins,const std::atomic<bool>* cancelled,bool exclude_edge,bool cap_priority) {
    checkpoint(cancelled);
    if(!width || !height || width>std::numeric_limits<std::uint32_t>::max()/height || width*height!=response.size() || !maximum_basins)
        throw std::invalid_argument("invalid LoG response basin raster");
    const auto n=response.size();std::vector<std::uint8_t> visited(n),maxima(n);std::vector<std::uint32_t> queue;queue.reserve(n);
    for(auto value:response) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite LoG response");
    // Test whole eight-connected plateaus, including their full perimeter.
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);if(visited[i] || response[i]<.001f) continue;
        queue.clear();queue.push_back(static_cast<std::uint32_t>(i));visited[i]=1;bool higher=false;
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            const auto j=queue[at];if(exclude_edge && (! (j%width) || j%width+1==width || j<width || j/width+1==height)) higher=true;
            log_neighbors(queue[at],width,height,true,[&](std::size_t j) {
                if(response[j]>response[i]) higher=true;
                if(!visited[j] && response[j]==response[i]) {visited[j]=1;queue.push_back(static_cast<std::uint32_t>(j));}
            });
        }
        if(!higher) for(auto j:queue) maxima[j]=1;
    }
    std::vector<std::uint32_t> seeds(n),labels(n);std::uint32_t count=0;
    // Seed regions and flooding use four-connectivity.
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);if(!maxima[i] || seeds[i]) continue;
        if(count>=maximum_basins) throw std::length_error("LoG response basin candidate limit exceeded");
        ++count;queue.clear();queue.push_back(static_cast<std::uint32_t>(i));seeds[i]=count;
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            log_neighbors(queue[at],width,height,false,[&](std::size_t j) {
                if(maxima[j] && !seeds[j]) {seeds[j]=count;queue.push_back(static_cast<std::uint32_t>(j));}
            });
        }
    }
    struct Entry {float floor;std::uint64_t age;std::uint32_t index,label;};
    auto lower=[](const Entry& a,const Entry& b) {return a.floor!=b.floor?a.floor<b.floor:a.age>b.age;};
    std::vector<Entry> frontier;frontier.reserve(n);std::vector<std::uint8_t> queued(n);std::uint64_t age=0;
    for(std::size_t i=0;i<n;++i) if(seeds[i]) {queued[i]=1;frontier.push_back({response[i],age++,static_cast<std::uint32_t>(i),seeds[i]});}
    std::make_heap(frontier.begin(),frontier.end(),lower);std::size_t processed=0;
    while(!frontier.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);
        std::pop_heap(frontier.begin(),frontier.end(),lower);const auto next=frontier.back();frontier.pop_back();bool collision=false;
        if(!seeds[next.index]) log_neighbors(next.index,width,height,false,[&](std::size_t j) {if(labels[j] && labels[j]!=next.label) collision=true;});
        if(collision) continue;labels[next.index]=next.label;
        log_neighbors(next.index,width,height,false,[&](std::size_t j) {
            if(queued[j] || response[j]<=0) return;queued[j]=1;
            frontier.push_back({(cap_priority?std::min(next.floor,response[j]):response[j]),age++,static_cast<std::uint32_t>(j),next.label});std::push_heap(frontier.begin(),frontier.end(),lower);
        });
    }
    return labels;
}

}
int main(int argc,char** argv) {
    try {
        if(argc!=7) throw std::invalid_argument("response output width height exclude_edge cap_priority required");
        const auto w=std::stoull(argv[3]),h=std::stoull(argv[4]);std::ifstream input(argv[1],std::ios::binary);
        std::vector<float> response(w*h);input.read(reinterpret_cast<char*>(response.data()),std::streamsize(response.size()*4));
        if(!input) throw std::runtime_error("input failed");
        const auto labels=qpx::cell_detail::trace_basins(response,w,h,10000,nullptr,std::stoi(argv[5])!=0,std::stoi(argv[6])!=0);
        std::ofstream output(argv[2],std::ios::binary);output.write(reinterpret_cast<const char*>(labels.data()),std::streamsize(labels.size()*4));
        if(!output) throw std::runtime_error("output failed");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
