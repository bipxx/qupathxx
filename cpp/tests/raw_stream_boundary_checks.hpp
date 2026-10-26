#pragma once
#include "../raw_stream_boundaries.hpp"
#include "raw_contour_seam_reference.hpp"
#include <algorithm>
#include <iostream>
#include <tuple>

namespace raw_stream_boundary_checks {
using Edge=std::tuple<std::uint32_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t>;
inline void require(bool value,const char* message) {if(!value) throw std::runtime_error(std::string("Raw stream boundaries: ")+message);}
template<class E,class F> void rejects(F&& f) {try {f();}catch(const E&) {return;}throw std::runtime_error("Raw stream boundary refusal missing");}
inline qpx::RawPixelStreamGeometry geometry(const raw_contour_seam_reference::Case& c,std::int64_t w,std::int64_t h) {
    qpx::RawPixelStreamOptions options;options.tile_width=w;options.tile_height=h;
    return {qpx::ScientificRawLattice(c.image_size[0],c.image_size[1],
        {c.origin[0],c.extent[0],c.samples[0]},{c.origin[1],c.extent[1],c.samples[1]}),
        {c.first[0],c.first[1],c.width,c.height},options};
}
inline std::vector<std::uint32_t> block(const raw_contour_seam_reference::Case& c,const qpx::ScientificRawWindow& w) {
    std::vector<std::uint32_t> values;
    for(std::int64_t y=0;y<w.core_height();++y) for(std::int64_t x=0;x<w.core_width();++x)
        values.push_back(c.labels[std::size_t((w.y().first_core_index()-c.first[1]+y)*c.width+w.x().first_core_index()-c.first[0]+x)]);
    return values;
}
inline std::vector<Edge> expand(const qpx::detail::RawStreamBoundaries& result) {
    std::vector<Edge> edges;
    for(const auto& r:result.runs) {
        require(r.first<r.last && r.before!=r.after,"empty or equal-sided run retained");
        for(auto p=r.first;p<r.last;++p) {
            if(r.vertical) {
                if(r.before) edges.emplace_back(r.before,r.fixed,p,r.fixed,p+1);
                if(r.after) edges.emplace_back(r.after,r.fixed,p+1,r.fixed,p);
            } else {
                if(r.before) edges.emplace_back(r.before,p+1,r.fixed,p,r.fixed);
                if(r.after) edges.emplace_back(r.after,p,r.fixed,p+1,r.fixed);
            }
        }
    }
    std::sort(edges.begin(),edges.end());return edges;
}
inline void run() {
    using namespace qpx;using namespace qpx::detail;
    const auto cases=raw_contour_seam_reference::cases();
    for(const auto& c:cases) {
        std::vector<Edge> expected;for(const auto& e:c.edges) expected.emplace_back(e.label,e.x0,e.y0,e.x1,e.y1);
        std::sort(expected.begin(),expected.end());std::vector<RawBoundaryRun> canonical;
        for(const auto shape:std::vector<std::pair<std::int64_t,std::int64_t>>{{1,1},{2,3},{3,2},{4,4}}) {
            const auto g=geometry(c,shape.first,shape.second);RawStreamBoundaryBuilder builder(g,3);
            for(std::uint64_t i=0;i<g.windows;++i) {const auto w=g.window(i);builder.append(w,block(c,w));}
            const auto result=builder.finish();require(expand(result)==expected,"independent class edge oracle differs");
            require(result.class_counts==std::vector<std::uint64_t>(c.counts.begin(),c.counts.end()) && result.completed_samples==g.samples && result.completed_windows==g.windows,"counts or coverage differ");
            if(!canonical.empty()) require(canonical==result.runs,"compressed network depends on tile shape");
            canonical=result.runs;
            require(result.retained_bytes<=result.estimated_work_bytes && result.estimated_work_bytes<=64u*1024*1024,"retained/work admission differs");
            rejects<std::logic_error>([&]{builder.finish();});rejects<std::logic_error>([&]{builder.append(g.window(0),block(c,g.window(0)));});
        }
    }
    const auto& c=cases[2];const auto g=geometry(c,2,2);const auto first=g.window(0);const auto values=block(c,first);
    {RawStreamBoundaryBuilder b(g,3);rejects<std::logic_error>([&]{b.finish();});rejects<std::logic_error>([&]{b.append(first,values);});}
    {RawStreamBoundaryBuilder b(g,3);b.append(first,values);rejects<std::invalid_argument>([&]{b.append(first,values);});rejects<std::logic_error>([&]{b.finish();});}
    {RawStreamBoundaryBuilder b(g,3);auto short_values=values;short_values.pop_back();rejects<std::invalid_argument>([&]{b.append(first,short_values);});}
    {RawStreamBoundaryBuilder b(g,3);auto wrong=values;wrong[0]=4;const auto saved=wrong;rejects<std::invalid_argument>([&]{b.append(first,wrong);});require(wrong==saved,"borrowed input modified");}
    {RawStreamBoundaryBuilder b(g,3);const ScientificRawWindow shifted(4,4,{1,4,4},{0,4,4},0,0,2,2);rejects<std::invalid_argument>([&]{b.append(shifted,values);});}
    {RawStreamBoundaryBuilder b(g,3);const ScientificRawWindow halo(4,4,{0,4,4,1},{0,4,4},0,0,2,2);rejects<std::invalid_argument>([&]{b.append(halo,values);});}
    {RawStreamBoundaryBuilder b(g,3);std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{b.append(first,values,&cancelled);});rejects<std::logic_error>([&]{b.finish();});}
    {const auto whole=geometry(c,4,4);RawStreamBoundaryBuilder b(whole,3);b.append(whole.window(0),c.labels);std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{b.finish(&cancelled);});}
    rejects<std::invalid_argument>([&]{RawStreamBoundaryBuilder b(g,0);});
    rejects<std::length_error>([&]{RawStreamBoundaryBuilder b(g,3,{100,1});});
    {RawStreamBoundaryBuilder b(g,3,{1,64u*1024*1024});rejects<std::length_error>([&]{b.append(first,values);});rejects<std::logic_error>([&]{b.finish();});}
    {RawStreamBoundaryBuilder b(g,3,{100,65536});rejects<std::length_error>([&]{b.append(first,values);});}
    {RawStreamBoundaryBuilder b(g,3);const std::vector<std::uint32_t> zeros(4);for(std::uint64_t i=0;i<g.windows;++i) b.append(g.window(i),zeros);const auto empty=b.finish();require(empty.runs.empty() && empty.class_counts==std::vector<std::uint64_t>(3,0),"unclassified cells produced geometry");}
    // A million-column frontier stays compressed: no full-row raster or visited map.
    RawPixelStreamOptions options;options.tile_width=4096;options.tile_height=1;
    const RawPixelStreamGeometry wide(ScientificRawLattice(1000000,2,{0,1000000,1000000},{0,2,2}),{0,0,1000000,2},options);
    RawStreamBoundaryBuilder uniform(wide,1,{16,128*1024});std::vector<std::uint32_t> labels;
    for(std::uint64_t i=0;i<wide.windows;++i) {const auto w=wide.window(i);labels.assign(std::size_t(w.core_width()),1);uniform.append(w,labels);}
    const auto result=uniform.finish();require(result.runs.size()==4 && result.class_counts==std::vector<std::uint64_t>{2000000} && result.completed_samples==2000000 && result.peak_runs<=16 && result.estimated_work_bytes<=128*1024,"uniform wide-frontier storage grew with image width");
    std::cout<<"Raw stream boundaries: independent class seams/holes/corners/global phase, strict block/cancel/quota refusal and 2000000 samples with "<<result.peak_runs<<" peak runs passed\n";
}
}
