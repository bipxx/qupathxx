#pragma once
#include "../raw_stream_boundaries.hpp"
#include "raw_contour_seam_reference.hpp"
#include <algorithm>
#include <cmath>
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
inline Edge undirected(std::uint32_t label,std::int64_t x,std::int64_t y,std::int64_t xx,std::int64_t yy) {
    if(std::tie(xx,yy)<std::tie(x,y)) {std::swap(x,xx);std::swap(y,yy);}return {label,x,y,xx,yy};
}
inline void geometry_matches(const raw_contour_seam_reference::Case& c,const qpx::detail::RawStreamBoundaries& network) {
    using namespace qpx;using namespace qpx::detail;
    if(!has_geos()) {rejects<std::runtime_error>([&]{trace_raw_boundary_geometry(network);});return;}
    const auto traced=trace_raw_boundary_geometry(network);std::vector<Edge> actual,expected;
    require(traced->input_sha256().empty() && traced->object_id()==0,"geometry fabricated source/apply identity");
    require(traced->retained_bytes()<=traced->estimated_work_bytes(),"geometry retained admission differs");
    for(const auto& e:c.edges) expected.push_back(undirected(e.label,e.x0,e.y0,e.x1,e.y1));
    const auto dx=GridFraction(c.extent[0],c.samples[0]).binary64(),dy=GridFraction(c.extent[1],c.samples[1]).binary64();
    std::size_t seen=0;
    for(const auto& component:traced->components()) {
        ++seen;const auto label=component.label;require(label>=1 && label<=3 && component.pixels==c.counts[label-1],"geometry class sample count differs");
        double perimeter=0;
        for(const auto& e:c.edges) if(e.label==label) perimeter+=(e.x0==e.x1?dy:dx);
        require(component.geometry.area()==double(component.pixels)*dx*dy && component.geometry.perimeter()==perimeter,"exact independent world area/perimeter differs");
        const auto parts=component.geometry.polygon_parts();std::size_t holes=0;for(const auto& part:parts) holes+=part.holes.size();
        const bool nested=std::string(c.name)=="nested_holes",crop=std::string(c.name)=="cropped_anisotropic",diagonal=std::string(c.name)=="diagonal_contacts";
        require(parts.size()==(diagonal?4u:1u) && holes==((nested || (crop && label==1))?1u:0u),"independent component/hole topology differs");
        for(const auto& path:component.geometry.paths()) {
            auto index=[&](Point point) {
                const auto x=std::int64_t(std::llround((point.x-c.origin[0])/dx)),y=std::int64_t(std::llround((point.y-c.origin[1])/dy));
                require(network.geometry.lattice.x().edge(x).binary64()==point.x && network.geometry.lattice.y().edge(y).binary64()==point.y,"world vertex lost original lattice phase");
                return std::pair<std::int64_t,std::int64_t>{x,y};
            };
            for(std::size_t i=0;i<path.size();++i) {
                auto a=index(path[i]),b=index(path[(i+1)%path.size()]);require(a.first==b.first || a.second==b.second,"non-raster geometry edge");
                while(a!=b) {
                    const auto next=std::pair<std::int64_t,std::int64_t>{a.first+(b.first>a.first)-(b.first<a.first),a.second+(b.second>a.second)-(b.second<a.second)};
                    actual.push_back(undirected(label,a.first,a.second,next.first,next.second));a=next;
                }
            }
        }
    }
    require(seen==std::size_t(std::count_if(c.counts.begin(),c.counts.end(),[](auto n){return n!=0;})),"missing output class");
    std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());require(actual==expected,"independent geometry cell edges differ");
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
            geometry_matches(c,result);
            std::vector<RawBoundaryCell> cells;
            for(std::int64_t y=0;y<c.height;++y) for(std::int64_t x=0;x<c.width;++x)
                cells.push_back({c.first[0]+x,c.first[1]+y});
            const auto located=label_raw_boundary_cells(result,cells,1024*1024);
            require(located.labels==c.labels,"sweep lookup differs from independent authored cells");
            std::reverse(cells.begin(),cells.end());auto reverse=c.labels;std::reverse(reverse.begin(),reverse.end());
            require(label_raw_boundary_cells(result,cells,1024*1024).labels==reverse,"query order changes face classes");
            rejects<std::length_error>([&]{label_raw_boundary_cells(result,cells,1);});
            std::atomic<bool> stopped{true};rejects<std::runtime_error>([&]{label_raw_boundary_cells(result,cells,1024*1024,&stopped);});
            auto outside=cells;outside[0].x=c.first[0]-1;
            rejects<std::invalid_argument>([&]{label_raw_boundary_cells(result,outside,1024*1024);});
            auto partial=result;--partial.completed_samples;
            rejects<std::invalid_argument>([&]{label_raw_boundary_cells(partial,cells,1024*1024);});
            auto malformed=result;malformed.runs[0].after=99;
            rejects<std::invalid_argument>([&]{label_raw_boundary_cells(malformed,cells,1024*1024);});
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
    const std::vector<RawBoundaryCell> sparse_queries{{999999,1},{0,0},{500000,1},{0,1},{999999,0},{0,0}};
    const auto located=label_raw_boundary_cells(result,sparse_queries,128*1024);
    require(located.labels==std::vector<std::uint32_t>(sparse_queries.size(),1) && located.peak_active==2 && located.estimated_work_bytes<=128*1024,"sparse face lookup grew with raster area");
    auto overlapping=result;overlapping.runs.push_back(*std::find_if(result.runs.begin(),result.runs.end(),[](const auto& r){return r.vertical;}));
    rejects<std::invalid_argument>([&]{label_raw_boundary_cells(overlapping,sparse_queries,128*1024);});
    if(has_geos()) {
        TissueContourOptions options;options.work_bytes=192*1024;
        const auto geometry=trace_raw_boundary_geometry(result,options);
        require(geometry->components().size()==1 && geometry->components()[0].pixels==2000000 && geometry->components()[0].geometry.area()==2000000 && geometry->components()[0].geometry.perimeter()==2000004 && geometry->vertex_count()==4 && geometry->estimated_work_bytes()<=options.work_bytes,"uniform geometry scaled with raster area");
        const auto roi=Roi::rectangle({.5,0,4,2});
        const auto clipped=trace_raw_boundary_geometry(result,options,&roi);
        require(clipped->components()[0].geometry.area()==8 && clipped->components()[0].pixels==2000000,"ROI clipping/count semantics differ");
        options.clip_to_roi=false;require(trace_raw_boundary_geometry(result,options,&roi)->components()[0].geometry.area()==2000000,"ROI clipping toggle ignored");
        auto small=options;small.max_edges=1;rejects<std::length_error>([&]{trace_raw_boundary_geometry(result,small);});
        small=options;small.max_vertices=3;rejects<std::length_error>([&]{trace_raw_boundary_geometry(result,small);});
        small=options;small.work_bytes=1;rejects<std::length_error>([&]{trace_raw_boundary_geometry(result,small);});
        std::atomic<bool> stopped{true};rejects<std::runtime_error>([&]{trace_raw_boundary_geometry(result,options,nullptr,&stopped);});
        auto wrong_counts=result;wrong_counts.class_counts[0]-=1;rejects<std::runtime_error>([&]{trace_raw_boundary_geometry(wrong_counts);});
        RawPixelStreamOptions tile;tile.tile_width=2;tile.tile_height=2;
        const RawPixelStreamGeometry outside(ScientificRawLattice(1,1,{-1,2,2},{-1,2,2}),{0,0,2,2},tile);
        RawStreamBoundaryBuilder clipped_builder(outside,1);clipped_builder.append(outside.window(0),std::vector<std::uint32_t>(4,1));
        const auto image_clipped=trace_raw_boundary_geometry(clipped_builder.finish());
        require(image_clipped->components()[0].geometry.area()==1 && image_clipped->components()[0].pixels==4,"source image clipping differs");
        const RawPixelStreamGeometry imprecise(ScientificRawLattice(1000000000000LL,1,{999999999999LL,1,1000000},{0,1,1}),{0,0,1,1},tile);
        RawStreamBoundaryBuilder precision(imprecise,1);precision.append(imprecise.window(0),{1});const auto collapsed=precision.finish();
        rejects<std::invalid_argument>([&]{trace_raw_boundary_geometry(collapsed);});
        std::cout<<"Raw streamed geometry: 16 independent edge/area/perimeter/part/hole layouts, both-axis crop phase, ROI/image clipping, strict limits/cancel/precision/count refusal and 2000000 samples under 192 KiB passed\n";
    } else std::cout<<"Raw streamed geometry: GEOS-unavailable refusal passed\n";
    std::cout<<"Raw boundary face lookup: independent cells across 16 layouts, query order/duplicate/half-open/global crop, incomplete/invalid/cancel/work refusal and sparse 2000000-cell network passed\n";
    std::cout<<"Raw stream boundaries: independent class seams/holes/corners/global phase, strict block/cancel/quota refusal and 2000000 samples with "<<result.peak_runs<<" peak runs passed\n";
}
}
