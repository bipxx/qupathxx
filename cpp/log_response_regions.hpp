#pragma once
// Internal implementation included inside qpx::cell_detail by log_watershed.hpp.
// Independent regional maxima, descending flood and per-basin intensity checks.
inline std::vector<std::uint32_t> log_response_basins(const std::vector<float>& response,std::size_t width,std::size_t height,
                                                     std::size_t maximum_basins,const std::atomic<bool>* cancelled) {
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
            frontier.push_back({std::min(next.floor,response[j]),age++,static_cast<std::uint32_t>(j),next.label});std::push_heap(frontier.begin(),frontier.end(),lower);
        });
    }
    return labels;
}

inline std::vector<std::uint8_t> log_filled_basin_regions(const std::vector<float>& score,
    const std::vector<std::uint32_t>& components,const std::vector<std::uint8_t>& positive,
    std::size_t width,std::size_t height,const LogParameters& p,const std::atomic<bool>* cancelled) {
    const auto n=score.size();
    const auto last=*std::max_element(components.begin(),components.end());
    std::vector<std::uint8_t> mask(n),state(n),seen(std::size_t(last)+1);std::vector<std::uint32_t> queue;queue.reserve(n);
    std::uint64_t box_work=0;
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);const auto id=components[i];if(!id || seen[id]) continue;seen[id]=1;
        queue.clear();queue.push_back(static_cast<std::uint32_t>(i));state[i]=3;
        std::size_t left=i%width,right=left,top=i/width,bottom=top;
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);const auto j=queue[at];left=std::min(left,j%width);right=std::max(right,j%width);top=std::min(top,j/width);bottom=std::max(bottom,j/width);
            log_neighbors(j,width,height,false,[&](std::size_t k) {if(components[k]==id && state[k]!=3) {state[k]=3;queue.push_back(static_cast<std::uint32_t>(k));}});
        }
        box_work+=(right-left+1)*(bottom-top+1);if(box_work>64ull*n) throw std::length_error("LoG basin filling exceeds traversal limit");
        queue.clear();
        for(auto y=top;y<=bottom;++y) for(auto x=left;x<=right;++x) state[y*width+x]=components[y*width+x]==id?2:0;
        auto add=[&](std::size_t j) {if(!state[j]) {state[j]=1;queue.push_back(static_cast<std::uint32_t>(j));}};
        for(auto x=left;x<=right;++x) {add(top*width+x);add(bottom*width+x);}
        for(auto y=top;y<=bottom;++y) {add(y*width+left);add(y*width+right);}
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            log_neighbors(queue[at],width,height,p.eight_connected_background,[&](std::size_t j) {if(j%width>=left && j%width<=right && j/width>=top && j/width<=bottom) add(j);});
        }
        double sum=0;std::size_t count=0;
        for(auto y=top;y<=bottom;++y) {checkpoint(cancelled);for(auto x=left;x<=right;++x) if(state[y*width+x]!=1) {sum+=score[y*width+x];++count;}}
        if(count && sum/count>p.threshold) for(auto y=top;y<=bottom;++y) {checkpoint(cancelled);for(auto x=left;x<=right;++x) if(state[y*width+x]!=1) mask[y*width+x]=1;}
    }
    std::vector<std::uint8_t> expanded(n);
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);if(!positive[i]) continue;
        bool near=mask[i]!=0;log_neighbors(i,width,height,true,[&](std::size_t j) {near=near || mask[j];});expanded[i]=near;
    }
    log_fill_holes(expanded,width,height,cancelled,p.eight_connected_background);return expanded;
}
inline std::vector<std::uint8_t> log_basin_regions(const std::vector<float>& score,std::size_t width,std::size_t height,
                                                 const LogParameters& p,const std::atomic<bool>* cancelled) {
    std::vector<float> response;const auto positive=log_positive(score,width,height,p.sigma,cancelled,&response);
    const auto components=log_response_basins(response,width,height,p.maximum_cells,cancelled);
    return log_filled_basin_regions(score,components,positive,width,height,p,cancelled);
}
