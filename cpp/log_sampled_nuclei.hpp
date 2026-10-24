#pragma once
// Included inside qpx::cell_detail by log_watershed.hpp. Internal candidate;
// existing production recipes never enable sampled_shape.
inline LogResult log_sampled_nuclei(const std::vector<float>& score,const std::vector<std::uint8_t>& domain,
                                    const std::vector<std::uint8_t>& mask,std::size_t width,std::size_t height,
                                    const LogParameters& p,const std::atomic<bool>* cancelled,std::atomic<int>* phase) {
    if(p.minimum_radius!=0 || p.minimum_spacing!=0)
        throw std::invalid_argument("sampled shape candidate requires explicit zero radius and spacing filters");
    if(phase) phase->store(3);const auto shaped=sampled_shape_watershed(mask,width,height,p.prominence,p.maximum_cells,p.work_bytes,cancelled);
    if(phase) phase->store(4);const auto n=mask.size();LogResult result;result.labels.resize(n);
    std::vector<std::uint8_t> state(n);std::vector<std::uint32_t> queue;queue.reserve(n);std::vector<std::uint32_t> dense(1);
    std::uint64_t box_work=0;
    // Four-connected foreground components are traced after clipping. Their
    // holes use the dual, eight-connected background topology.
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);result.candidate_pixels+=mask[i] && domain[i];
        if(!shaped.mask[i] || !domain[i] || result.labels[i]) continue;
        const auto id=static_cast<std::uint32_t>(dense.size());dense.push_back(0);queue.clear();queue.push_back(static_cast<std::uint32_t>(i));result.labels[i]=id;
        std::size_t left=i%width,right=left,top=i/width,bottom=top;
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);const auto j=queue[at];left=std::min(left,j%width);right=std::max(right,j%width);top=std::min(top,j/width);bottom=std::max(bottom,j/width);
            log_neighbors(j,width,height,false,[&](std::size_t k){if(shaped.mask[k] && domain[k] && !result.labels[k]) {result.labels[k]=id;queue.push_back(static_cast<std::uint32_t>(k));}});
        }
        box_work+=(right-left+1)*(bottom-top+1);if(box_work>64ull*n) throw std::length_error("sampled nucleus filling exceeds traversal limit");
        queue.clear();
        for(auto y=top;y<=bottom;++y) {checkpoint(cancelled);for(auto x=left;x<=right;++x) state[y*width+x]=result.labels[y*width+x]==id?2:0;}
        auto add=[&](std::size_t j){if(!state[j]) {state[j]=1;queue.push_back(static_cast<std::uint32_t>(j));}};
        for(auto x=left;x<=right;++x) {add(top*width+x);add(bottom*width+x);}
        for(auto y=top;y<=bottom;++y) {add(y*width+left);add(y*width+right);}
        for(std::size_t at=0;at<queue.size();++at) {
            if((at&4095)==0) checkpoint(cancelled);
            log_neighbors(queue[at],width,height,true,[&](std::size_t j){if(j%width>=left && j%width<=right && j/width>=top && j/width<=bottom) add(j);});
        }
        std::uint64_t count=0;double sum=0;
        for(auto y=top;y<=bottom;++y) {checkpoint(cancelled);for(auto x=left;x<=right;++x) {const auto j=y*width+x;if(state[j]!=1 && domain[j]) {result.labels[j]=id;++count;sum+=score[j];}}}
        if(count>=p.minimum_area && (!p.maximum_area || count<=p.maximum_area) && sum/count>=p.threshold) {
            if(result.count>=p.maximum_cells) throw std::length_error("sampled nucleus output limit exceeded");dense[id]=static_cast<std::uint32_t>(++result.count);
        }
    }
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);result.labels[i]=dense[result.labels[i]];}
    for(std::size_t i=0;i<shaped.maxima.size();++i) {const auto index=shaped.maxima[i];result.peaks.push_back({index,result.labels[index],shaped.maxima_radii[i],true});}
    return result;
}
