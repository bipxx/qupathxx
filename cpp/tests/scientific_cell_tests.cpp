#include "qupathxx/cells.hpp"
#include "qupathxx/input_digest.hpp"
#include "../scientific_cell_recipe.hpp"
#include "../cell_archive.hpp"
#include "grid_cell_reference.hpp"
#include <iostream>

namespace {
using namespace qpx;
void require(bool value) {if(!value) throw std::runtime_error("explicit-grid cell workflow failed");}
bool near(double a,double b) {return std::abs(a-b)<=2e-12+2e-12*std::abs(b);}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::runtime_error("grid cell admission did not reject");}
class CountingReader final:public ImageServer {
    ImageMetadata m;
public:
    mutable unsigned reads=0;bool stopped=false;std::atomic<bool>* cancel_on_read=nullptr;unsigned red_offset=0;
    CountingReader(std::int64_t w=6,std::int64_t h=9) {m.width=w;m.height=h;m.levels={{w,h,1}};}
    const ImageMetadata& metadata() const override {return m;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        ++reads;PixelBuffer buffer(r.width,r.height);
        for(std::int64_t y=0;y<r.height;++y) for(std::int64_t x=0;x<r.width;++x) {
            const auto at=std::size_t((y*r.width+x)*4);const bool dark=r.x+x>=2 && r.x+x<4 && r.y+y>=3 && r.y+y<6;
            for(unsigned c=0;c<3;++c) buffer.writable_bytes()[at+c]=dark?32:255;buffer.writable_bytes()[at+3]=255;
            if(r.x+x==2 && r.y+y==3) buffer.writable_bytes()[at]+=std::uint8_t(red_offset);
        }
        if(cancel_on_read) cancel_on_read->store(true);return buffer;
    }
    void close() override {stopped=true;}
    bool closed() const override {return stopped;}
};
}
void check_scientific_cells() {
    using namespace qpx;using namespace qpx::cell_detail;
    constexpr auto budget=256ULL*1024*1024;
    const StainTransform transform({{1,0,0},{0,1,0}});
    for(const auto& c:grid_cell_reference::curvature)
        require(grid_log_positive(c.score,c.valid,c.w,c.h,{c.sx,c.sy},0,budget,nullptr)==c.positive);
    for(const auto& c:grid_cell_reference::expansion)
        require(expand_grid_cells(c.seeds,c.domain,c.w,c.h,{c.sx,c.sy},c.radius,budget,nullptr).labels==c.labels);
    {
        // Hand-calculated negative Laplacian: 2/1^2 - 4/2^2 = +1;
        // isotropic spacing instead gives 2-4=-2.
        std::vector<double> score{4,7,4,4,5,4,4,7,4};std::vector<std::uint8_t> valid(9,1);
        require(grid_log_positive(score,valid,3,3,{1,2},0,budget,nullptr)[4]);
        require(!grid_log_positive(score,valid,3,3,{1,1},0,budget,nullptr)[4]);
        score.assign(9,1);score[4]+=std::ldexp(1.,-30);
        require(grid_log_positive(score,valid,3,3,{1,1},0,budget,nullptr)[4]);
        valid[4]=0;require(!grid_log_positive(score,valid,3,3,{1,1},0,budget,nullptr)[4]);
        for(auto scale:{1.,1e-20,1e4}) {
            require(log_h_maxima({scale,2*scale,scale},{1,1,1},3,1,.5*scale,nullptr,scale)==std::vector<std::uint8_t>({0,1,0}));
        }
        std::vector<std::uint32_t> seed(9);seed[4]=1;valid.assign(9,1);
        require(expand_grid_cells(seed,valid,3,3,{2,3},3.1,budget,nullptr).labels==std::vector<std::uint32_t>({0,1,0,1,1,1,0,1,0}));
        require(expand_grid_cells(seed,valid,3,3,{1,.75},.9,budget,nullptr).labels==std::vector<std::uint32_t>({0,1,0,0,1,0,0,1,0}));
        require(expand_grid_cells(seed,valid,3,3,{.25,.5},.25,budget,nullptr).labels==seed); // strict radius
        require(expand_grid_cells(seed,valid,3,3,{.25,.5},.3,budget,nullptr).labels==std::vector<std::uint32_t>({0,0,0,1,1,1,0,0,0}));
        std::vector<std::uint32_t> two{1,0,0,0,2};
        require(expand_grid_cells(two,std::vector<std::uint8_t>(5,1),5,1,{.5,2},3,budget,nullptr).labels==std::vector<std::uint32_t>({1,1,0,2,2}));
        require(expand_grid_cells({1,0,0,0,0},{1,1,0,1,1},5,1,{1,1},10,budget,nullptr).labels==std::vector<std::uint32_t>({1,1,0,0,0}));
    }
    const auto reader=std::make_shared<CountingReader>();Document doc(reader);doc.set_calibration(.5,.25);
    const auto id=doc.add_rectangle({0,0,6,9});const ScientificGrid grid(6,9,{0,6,3},{0,9,3});
    GridCellParameters p;p.sigma=0;p.min_nucleus_area=1;p.cell_expansion=3.1;p.contours.work_bytes=1024*1024;
    const CellRequest request(doc,id,transform,grid,p);require(reader->reads==0 && request.matches(doc,id));
    require(request.scientific_grid() && request.grid_parameters() && request.total_tiles()==1 && request.level()==0);
    const auto& resolved=*request.resolved_grid_parameters();require(resolved.step_x==2 && resolved.step_y==3 && resolved.min_nucleus_samples==1);
    rejects<std::logic_error>([&]{request.downsample();});rejects<std::logic_error>([&]{request.level_bounds();});rejects<std::logic_error>([&]{request.parameters();});
    const auto result=detect_cells(request);require(reader->reads==1 && result->grid_parameters() && result->features().size()==1);
    require(std::string(result->algorithm())=="qpx.grid-log-watershed-bodies/1");
    require(result->nuclei()->labels()==std::vector<std::uint32_t>({0,0,0,0,1,0,0,0,0}));
    require(result->cells()->labels()==std::vector<std::uint32_t>({0,1,0,1,1,1,0,1,0}));
    require(result->markers().size()==1 && result->markers()[0].radius==2 && result->markers()[0].center.x==3 && result->markers()[0].center.y==4.5);
    const auto& feature=result->features()[0];require(feature.nucleus_pixels==1 && feature.cell_pixels==5);
    const auto od=3*optical_density(32);
    for(unsigned c=0;c<3;++c) {
        require(feature.nucleus_rgb[c]==32 && near(feature.cell_rgb[c],210.4) && (*feature.cytoplasm_rgb)[c]==255);
        require(near(feature.nucleus_channels[c],od/3) && near(feature.cell_channels[c],od/15) && (*feature.cytoplasm_channels)[c]==0);
    }
    const auto calibrated=result->calibrated_feature(1);
    require(calibrated.nucleus_sample_area_um2==.75 && calibrated.cell_sample_area_um2==3.75 && calibrated.cytoplasm_sample_area_um2==3.);
    require(calibrated.nucleus_centroid_um->x==1.5 && calibrated.nucleus_centroid_um->y==1.125);
    require(result->nuclei()->input_sha256()==result->cells()->input_sha256());
    const auto hash=input_sha256(request);require(hash==result->nuclei()->input_sha256() && reader->reads==2);
    rejects<std::logic_error>([&]{CellArchiveCodec::encode(*result);});
    const auto revision=doc.revision();rejects<std::invalid_argument>([&]{doc.keep_cells(request,result);});require(doc.revision()==revision);
    // The complete ledger is checked at capture, before any image read.
    auto limited=p;limited.work_bytes=request.estimated_work_bytes();CellRequest exact(doc,id,transform,grid,limited);
    --limited.work_bytes;rejects<std::length_error>([&]{CellRequest denied(doc,id,transform,grid,limited);});require(reader->reads==2);
    limited=p;limited.contours.work_bytes=65536;rejects<std::length_error>([&]{CellRequest denied(doc,id,transform,grid,limited);});require(reader->reads==2);
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{detect_cells(request,&cancelled);});require(reader->reads==2);
    cancelled.store(false);reader->cancel_on_read=&cancelled;rejects<std::runtime_error>([&]{detect_cells(request,&cancelled);});reader->cancel_on_read=nullptr;require(reader->reads==3);
    auto physical=p;physical.units=GridCellUnits::Micrometres;physical.min_nucleus_area=.75;physical.max_nucleus_area=.75;
    physical.log_prominence=.2;physical.cell_expansion=.9;
    const CellRequest physical_request(doc,id,transform,grid,physical);const auto physical_result=detect_cells(physical_request);
    require(physical_request.resolved_grid_parameters()->step_x==1 && physical_request.resolved_grid_parameters()->step_y==.75);
    require(physical_result->features().size()==1 && physical_result->features()[0].cell_pixels==3);
    require(physical_result->markers()[0].radius==.75 && physical_result->calibrated_feature(1).cell_sample_area_um2==2.25);
    auto absent=physical;absent.min_nucleus_area=0;absent.max_nucleus_area=std::nextafter(.75,0.);require(detect_cells(CellRequest(doc,id,transform,grid,absent))->features().empty());
    auto invalid=physical;invalid.max_nucleus_area=.5;rejects<std::invalid_argument>([&]{invalid.validate();});
    auto signed_threshold=p;signed_threshold.threshold=-1;signed_threshold.validate();
    const ScientificGrid context_grid(6,9,{0,6,3,1},{0,9,3,1});const auto contextual=detect_cells(CellRequest(doc,id,transform,context_grid,p));
    require(contextual->nuclei()->samples()==9 && contextual->features().size()==1 && contextual->features()[0].cell_pixels==5);
    for(std::size_t i=0;i<25;++i) if(i%5==0 || i%5==4 || i/5==0 || i/5==4) require(!contextual->cells()->labels()[i]);
    const auto narrow=doc.add_rectangle({1.5,0,3,9});const auto clipped=detect_cells(CellRequest(doc,narrow,transform,grid,p));
    require(clipped->nuclei()->samples()==3 && clipped->features().size()==1 && clipped->features()[0].cell_pixels==3);
    auto stain=p;stain.feature=TissueFeature::Stain1;stain.threshold=.3;
    require(detect_cells(CellRequest(doc,id,transform,grid,stain))->features().size()==1);
    auto smoothed=p;smoothed.sigma=1;const auto smooth_result=detect_cells(CellRequest(doc,id,transform,grid,smoothed));
    require(smooth_result->features().size()==1 && smooth_result->features()[0].nucleus_rgb[0]==32);
    reader->red_offset=1;const auto fractional=detect_cells(CellRequest(doc,id,transform,grid,p));reader->red_offset=0;
    require(fractional->features().size()==1 && fractional->features()[0].nucleus_rgb[0]==32+1./6);
    require(near(fractional->features()[0].nucleus_channels[0],optical_density(32+1./6)));
    require(fractional->nuclei()->input_sha256()!=result->nuclei()->input_sha256());
    {
        std::vector<std::uint8_t> pixels(10*9*3,255);
        for(int y=3;y<6;++y) for(int x:{2,3,6,7}) for(int channel=0;channel<3;++channel) pixels[(y*10+x)*3+channel]=32;
        Document pair(Image(10,9,pixels));const auto object=pair.add_rectangle({0,0,10,9});const ScientificGrid pair_grid(10,9,{0,10,5},{0,9,3});
        auto spaced=p;spaced.cell_expansion=0;spaced.min_seed_distance=3.5;spaced.min_peak_radius=1.5;spaced.log_prominence=1.5;
        // The peaks are four level-zero pixels apart, not two sampled pixels;
        // the physical EDT radius is two, not one sampled pixel.
        require(detect_cells(CellRequest(pair,object,transform,pair_grid,spaced))->features().size()==2);
        spaced.min_seed_distance=std::nextafter(4.,5.);require(detect_cells(CellRequest(pair,object,transform,pair_grid,spaced))->features().size()==1);
        spaced.min_peak_radius=std::nextafter(2.,3.);require(detect_cells(CellRequest(pair,object,transform,pair_grid,spaced))->features().empty());
    }
    doc.set_calibration(1,1);require(!request.matches(doc,id) && !physical_request.matches(doc,id));require(result->calibrated_feature(1).nucleus_sample_area_um2==.75);
    if(has_geos()) {
        require(result->nucleus_contours()->components().size()==1 && near(result->nucleus_contours()->components()[0].geometry.area(),6));
        require(result->cell_contours()->components().size()==1 && near(result->cell_contours()->components()[0].geometry.area(),30));
    }
    // Scientific centers can exist where the old natural-center capture is empty.
    Document tiny(Image(2,2,std::vector<std::uint8_t>(12,32)));const auto tiny_id=tiny.add_rectangle({.05,.05,.1,.1});
    rejects<std::invalid_argument>([&]{AnalysisRequest old(tiny,tiny_id,transform);});
    const ScientificGrid tiny_grid(2,2,{0,1,5},{0,1,5});const auto tiny_result=detect_cells(CellRequest(tiny,tiny_id,transform,tiny_grid,p));
    require(tiny_result->nuclei()->samples()==1 && tiny_result->features().empty());
    rejects<std::invalid_argument>([&]{CellRequest missing_cal(tiny,tiny_id,transform,tiny_grid,physical);});
    // A large native ROI does not force a huge natural-grid preflight/allocation.
    const auto large_reader=std::make_shared<CountingReader>(1000000000000LL,1000000000000LL);Document large(large_reader);const auto large_id=large.add_rectangle({0,0,1000000000000.,1000000000000.});
    const ScientificGrid small(1000000000000LL,1000000000000LL,{999999999990LL,6,3},{2,6,2});
    const CellRequest small_request(large,large_id,transform,small,p);require(large_reader->reads==0);
    const auto empty=detect_cells(small_request);require(large_reader->reads==1 && empty->nuclei()->samples()==6 && empty->features().empty());
    const CellRequest live(doc,id,transform,grid,p);CellJob job;require(job.request(live));require(job.wait(doc,id).state=="ready" && job.result()->features().size()==1);
    doc.set_calibration(2,1);require(job.poll(doc,id).state=="stale" && !job.result());
    const CellRequest open(doc,id,transform,grid,p);reader->close();require(!open.matches(doc,id));rejects<std::runtime_error>([&]{detect_cells(open);});
    CellParameters legacy(CellAlgorithm::GridLogWatershedBodies);rejects<std::invalid_argument>([&]{legacy.validate();});
    std::cout<<"Native grid cell request, float64 LoG, metric shape/body, calibrated features and one-read detection passed\n";
    std::cout<<"32 exact metric LoG signs and 128 rational-distance body expansion references passed\n";
}
