#pragma once
#include "../sampling_summary.hpp"
#include <iostream>
namespace sampling_checks {
using namespace qpx;
inline void require(bool v) {if(!v)throw std::runtime_error("sampling summary check failed");}
class CountingServer final:public ImageServer {
    ImageMetadata info_;
public:
    mutable unsigned reads=0;
    CountingServer() {info_.width=32;info_.height=24;info_.levels={{32,24,1},{16,12,2}};}
    const ImageMetadata& metadata() const override {return info_;}
    PixelBuffer read_region(const RegionRequest& r) const override {++reads;PixelBuffer out(r.width,r.height);std::fill(out.writable_bytes().begin(),out.writable_bytes().end(),255);return out;}
    void close() override {}bool closed() const override {return false;}
};
inline void run() {
    const auto server=std::make_shared<CountingServer>();Document doc(server);const auto id=doc.add_rectangle({2,2,24,16});
    const StainTransform transform({{1,0,0},{0,1,0}});CellParameters cp;cp.sigma=0;
    auto cell=CellRequest(doc,id,transform,cp);auto cs=detail::sampling_summary(cell,{},{});require(!cs.step_x_um && cs.level==cell.level() && cs.step_x_px==cell.downsample());
    TissueParameters tp;tp.smoothing_sigma=0;auto tissue=TissueRequest(doc,id,transform,tp);auto ts=detail::sampling_summary(tissue);require(ts.step_x_px==tissue.downsample_x() && !ts.step_x_um);
    cp.level=1;auto manual=CellRequest(doc,id,transform,cp);require(detail::sampling_summary(manual,{},{}).step_x_px==2);
    doc.set_calibration(.5,.25);
    GridCellParameters gp;gp.sigma=0;GridCellSampling gs{12,4,0,0};
    auto grid=CellRequest::for_grid_roi(doc,id,transform,gs,gp);auto g=detail::sampling_summary(grid,doc.pixel_width_um(),doc.pixel_height_um());
    require(g.step_x_px==2 && g.step_y_px==4 && g.step_x_um==1 && g.step_y_um==1);
    auto nominal=CellRequest::for_nominal_roi(doc,id,transform,1.5,0,0,gp);auto n=detail::sampling_summary(nominal,doc.pixel_width_um(),doc.pixel_height_um());
    require(n.mode==detail::SamplingSummary::Mode::NominalGrid && n.step_x_px==1.5 && n.step_x_um==.75 && n.step_y_um==.375);
    auto tg=TissueRequest::for_grid_roi(doc,id,transform,TissueGridSampling{12,4,0,0},tp);auto tgs=detail::sampling_summary(tg);
    require(tgs.step_x_px==2 && tgs.step_y_px==4 && tgs.step_x_um==1 && tgs.step_y_um==1);
    require(server->reads==0);
    // Facts match the same request executed by Run, not a parallel resolver.
    const auto tissue_result=detect_tissue(tg);require(tissue_result->downsample_x()==tgs.step_x_px && tissue_result->downsample_y()==tgs.step_y_px);
    const auto cell_result=detect_cells(grid);require(cell_result->nuclei()->downsample_x()==g.step_x_px && cell_result->nuclei()->downsample_y()==g.step_y_px);
    detail::SamplingInputKey key;key.parameters(tp);detail::SamplingSummaryCache cache;unsigned builds=0;
    auto build=[&]{++builds;return ts;};cache.update(key,build);cache.update(key,build);require(builds==1);
    const auto original=tp;tp.smoothing_sigma=2;detail::SamplingInputKey changed;changed.parameters(tp);cache.update(changed,build);require(builds==2);
    tp=original;detail::SamplingInputKey restored;restored.parameters(tp);cache.update(restored,build);require(builds==3);
    restored.add(true);cache.update(restored,[]{throw std::invalid_argument("native validation message");return detail::SamplingSummary{};});
    require(!cache.summary && cache.error=="native validation message");
    std::cout<<"Sampling facts: Auto/manual/grid/nominal/calibration, zero reads, result agreement and value-cache restores passed\n";
}
}
