#include "qupathxx/fluorescence_tiles.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
using namespace qpx;
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected tiled fluorescence refusal");}
constexpr std::int64_t width=80,height=52;
RawPixelBuffer pixels(RegionRequest region,bool cut=false) {
    RawPixelBuffer buffer(region.width,region.height,2,SampleType::UInt16);
    std::fill(buffer.writable_valid().begin(),buffer.writable_valid().end(),1);
    auto& values=std::get<std::vector<std::uint16_t>>(buffer.writable_storage());
    const std::vector<std::pair<int,int>> centers=cut?std::vector<std::pair<int,int>>{{20,20},{42,20}}:
        std::vector<std::pair<int,int>>{{20,20},{40,18},{56,27},{37,39}};
    for(std::int64_t y=0;y<region.height;++y)for(std::int64_t x=0;x<region.width;++x) {
        const auto gx=region.x+x,gy=region.y+y;bool positive=false;
        for(auto c:centers)positive=positive || (gx-c.first)*(gx-c.first)+(gy-c.second)*(gy-c.second)<=9;
        if(cut && gx>=20 && gx<=42 && gy==20)positive=true;
        const auto at=(std::size_t(y)*std::size_t(region.width)+std::size_t(x))*2;
        values[at]=positive?100:0;values[at+1]=std::uint16_t(gx+10*gy);
    }
    return buffer;
}
FluorescenceCellOptions options(){FluorescenceCellOptions p;p.threshold=45;p.sigma=.7;p.cell_expansion=2.2;p.max_cells=1000;return p;}
std::vector<std::size_t> decode(const std::vector<FluorescencePixelSpan>& spans) {
    std::vector<std::size_t> result;
    for(const auto& span:spans){check(span.width>0 && span.x>=0 && span.x+span.width<=width && span.y>=0 && span.y<height,"Global RLE span is in bounds");
        for(std::int64_t x=span.x;x<span.x+span.width;++x)result.push_back(std::size_t(span.y*width+x));}
    return result;
}
void compartment(const RawCellCompartment& actual,const RawCellCompartment& reference) {
    check(actual.pixels==reference.pixels && actual.valid_pixels==reference.valid_pixels && actual.channels.size()==reference.channels.size(),"Whole/tiled compartment geometry agrees");
    for(std::size_t c=0;c<actual.channels.size();++c) {
        const auto& a=actual.channels[c];const auto& b=reference.channels[c];
        check(a.finite_samples==b.finite_samples && a.nonfinite_samples==b.nonfinite_samples && a.sum==b.sum && a.mean==b.mean &&
            a.minimum==b.minimum && a.maximum==b.maximum && a.sample_standard_deviation==b.sample_standard_deviation,"Whole/tiled raw moments agree exactly");
    }
}
std::map<std::string,std::vector<std::size_t>> tiled(std::int64_t tw,std::int64_t th,const FluorescenceCellResult& full,bool reverse=false) {
    std::vector<RegionRequest> cores;
    for(std::int64_t y=0;y<height;y+=th)for(std::int64_t x=0;x<width;x+=tw)cores.push_back({x,y,0,std::min(tw,width-x),std::min(th,height-y)});
    if(reverse)std::reverse(cores.begin(),cores.end());
    std::map<std::string,std::vector<std::size_t>> result;std::set<std::uint32_t> labels;
    const auto p=options();
    for(const auto& core:cores) {
        const auto x=std::max<std::int64_t>(0,core.x-14),y=std::max<std::int64_t>(0,core.y-14);
        const auto right=std::min(width,core.x+core.width+14),bottom=std::min(height,core.y+core.height+14);
        RawRegionRequest request;request.region={x,y,0,right-x,bottom-y};request.first_channel=2;request.channels=2;request.z=1;request.t=2;
        auto raw=pixels(request.region);const auto tile=segment_fluorescence_tile(raw,std::vector<std::uint8_t>(std::size_t(raw.width()*raw.height()),1),request,core,{0,0,0,width,height},std::string(64,'a'),p);
        check(tile.complete() && tile.accounted_work_bytes<=p.work_bytes,"Bounded complete synthetic tile");
        for(const auto& cell:tile.cells) {
            check(cell.anchor_x>=core.x && cell.anchor_x<core.x+core.width && cell.anchor_y>=core.y && cell.anchor_y<core.y+core.height,"Half-open core owns global anchor");
            const auto at=std::size_t(cell.anchor_y*width+cell.anchor_x);const auto label=full.nuclei[at];
            check(label && labels.insert(label).second && !cell.measurements.label,"One global cell per whole-raster nucleus; no local label masquerades as identity");
            std::vector<std::size_t> nuclei,bodies;
            for(std::size_t i=0;i<full.nuclei.size();++i){if(full.nuclei[i]==label)nuclei.push_back(i);if(full.bodies[i]==label)bodies.push_back(i);}
            check(decode(cell.nucleus)==nuclei && decode(cell.cell)==bodies,"Every tiled nucleus/body pixel agrees with independent whole-ROI segmentation across seams");
            compartment(cell.measurements.nucleus,full.measurements[label-1].nucleus);compartment(cell.measurements.cell,full.measurements[label-1].cell);
            check(bool(cell.measurements.cytoplasm)==bool(full.measurements[label-1].cytoplasm),"Cytoplasm absence agrees");
            if(cell.measurements.cytoplasm)compartment(*cell.measurements.cytoplasm,*full.measurements[label-1].cytoplasm);
            check(result.emplace(cell.id,std::move(bodies)).second,"Global IDs are unique");
        }
    }
    check(labels.size()==full.measurements.size(),"All independent whole-ROI cells are accounted once");return result;
}
void boundary() {
    RawRegionRequest request;request.region={0,0,0,34,height};request.channels=2;
    auto raw=pixels(request.region,true);const std::vector<std::uint8_t> domain(std::size_t(raw.width()*raw.height()),1);
    auto p=options();p.sigma=0;p.cell_expansion=0;
    const auto cut=segment_fluorescence_tile(raw,domain,request,{10,10,0,20,30},{0,0,0,width,height},std::string(64,'a'),p);
    check(!cut.complete() && cut.unresolved_foreground_pixels_in_core>0 && !cut.unresolved_labels.empty() && cut.cells.empty(),"Interior watershed child inherits cut foreground parent instead of becoming a complete tile cell");
    p.min_nucleus_area=10000;
    const auto filtered=segment_fluorescence_tile(raw,domain,request,{10,10,0,20,30},{0,0,0,width,height},std::string(64,'a'),p);
    check(!filtered.complete() && filtered.cells.empty() && filtered.unresolved_labels.empty() && filtered.unresolved_foreground_pixels_in_core>0,"Area filtering cannot hide unresolved foreground");
    p=options();p.threshold_method=FluorescenceThreshold::Otsu256;
    rejects<std::invalid_argument>([&]{segment_fluorescence_tile(raw,domain,request,{10,10,0,20,30},{0,0,0,width,height},std::string(64,'a'),p);});
    p=options();p.work_bytes=4096;
    rejects<WorkflowCaptureLimit>([&]{segment_fluorescence_tile(raw,domain,request,{10,10,0,10,30},{0,0,0,width,height},std::string(64,'a'),p);});
    p=options();
    rejects<std::invalid_argument>([&]{segment_fluorescence_tile(raw,domain,request,{10,10,0,23,30},{0,0,0,width,height},std::string(64,'a'),p);});
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{segment_fluorescence_tile(raw,domain,request,{10,10,0,10,30},{0,0,0,width,height},std::string(64,'a'),p,{},&cancelled);});
}
int main(){try {
    auto raw=pixels({0,0,0,width,height});const std::vector<std::uint8_t> domain(std::size_t(width*height),1);
    const auto full=segment_fluorescence_cells(raw,domain,options());check(full.measurements.size()==4,"Analytical disconnected four-nucleus fixture");
    const auto first=tiled(40,height,full);check(first==tiled(20,26,full) && first==tiled(32,26,full,true),"Global cell IDs and complete geometry are independent of tile layout/order");
    boundary();std::cout<<"Native tiled fluorescence: exact whole-ROI seam geometry/moments, stable ownership/IDs, cut-parent and filtered-boundary detection, work/halo/cancel guards pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
