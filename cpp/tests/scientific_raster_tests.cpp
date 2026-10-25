#include "../scientific_raster.hpp"
#include "../tissue_archive.hpp"
#include "qupathxx/scientific_channels.hpp"
#include "qupathxx/provenance.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value) {if(!value) throw std::runtime_error("scientific raster geometry contract failed");}
template<class Exception,class Function> void rejects(Function function) {
    try {function();} catch(const Exception&) {return;}
    throw std::runtime_error("scientific raster admission did not reject");
}
bool close(double a,double b) {return std::abs(a-b)<1e-12;}
qpx::ScientificRasterSpec spec(std::uint64_t threshold,std::uint64_t raw) {
    qpx::ScientificRasterSpec result;result.ticket=std::make_shared<const char>();
    result.threshold_pixels=threshold;result.raw_components=raw;return result;
}
class LargeReader final:public qpx::ImageServer {
    qpx::ImageMetadata meta_;
public:
    mutable unsigned reads=0;
    LargeReader() {meta_.width=1000000000000LL;meta_.height=16;meta_.levels={{meta_.width,meta_.height,1}};}
    const qpx::ImageMetadata& metadata() const override {return meta_;}
    void close() override {}bool closed() const override {return false;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest& r) const override {
        ++reads;
        qpx::PixelBuffer result(r.width,r.height);auto& bytes=result.writable_bytes();
        for(std::size_t i=0;i<bytes.size();i+=4) {bytes[i]=bytes[i+1]=bytes[i+2]=32;bytes[i+3]=255;}
        return result;
    }
};
}
void check_scientific_raster() {
    using namespace qpx;
    StainTransform transform({{1,0,0},{0,1,0}});
    Document doc(Image(10,10,std::vector<std::uint8_t>(300,32)));doc.set_calibration(.5,.25);
    const auto object=doc.add_rectangle({2,1,4,6});AnalysisRequest source(doc,object,transform,AnalysisKind::OpticalDensityMeans);
    const ScientificGrid grid(10,10,{2,4,2,1},{1,6,2,1});validate_scientific_raster_geometry(grid);
    auto rgb=read_scientific_rgb(*doc.server(),grid);auto od=scientific_optical_densities(rgb,transform);
    std::vector<std::uint32_t> labels(std::size_t(grid.width()*grid.height()));
    for(std::size_t i=0;i<labels.size();++i) if(scientific_sample_eligible(source,rgb,i%4,i/4) && od.values()[i*3]>.5) labels[i]=1;
    labels[2*4+2]=2;auto options=spec(4,2);
    const auto mask=ScientificRasterKernel::make(source,rgb,labels,options);
    // Independent hashlib/struct vectors pin the complete little-endian
    // snapshot and canonical input encoding, not just equality between calls.
    require(rgb.decoded_source_sha256()=="df9be96d64e700183ef552de4fb0fb10d6b4bc8f290f9906d3399b6e159fab99");
    require(mask->input_sha256()=="f06532a66cd89373a219d1dc103c99d0f51918969ab920a6327e94304f835df1");
    require(mask->input_sha256()==scientific_raster_input_sha256(source,rgb));
    auto body_spec=options;body_spec.body=true;body_spec.threshold_pixels=0;
    const auto body=ScientificRasterKernel::make(source,rgb,labels,body_spec);
    require(body->algorithm()=="qpx.grid-cell-bodies/1" && body->input_sha256()==mask->input_sha256());
    require(body->threshold_pixels()==4 && body->downsample_x()==2 && body->downsample_y()==3);
    require(read_scientific_rgb(*doc.server(),grid).decoded_source_sha256()==rgb.decoded_source_sha256());
    AnalysisRequest tile_source(doc,object,transform,AnalysisKind::OpticalDensityMeans,false,16);
    require(scientific_raster_input_sha256(tile_source,rgb)==mask->input_sha256());
    const ScientificGrid different_grid(10,10,{2,4,4,2},{1,6,2,1});const auto different_rgb=read_scientific_rgb(*doc.server(),different_grid);
    require(different_rgb.decoded_source_sha256()==rgb.decoded_source_sha256());
    require(scientific_raster_input_sha256(source,different_rgb)!=mask->input_sha256());
    StainTransform other_transform({{1,0,0},{0,1,0}},{200,240,180});
    AnalysisRequest other_model(doc,object,other_transform,AnalysisKind::OpticalDensityMeans);
    require(scientific_raster_input_sha256(other_model,rgb)!=mask->input_sha256());
    AnalysisRequest wrong_kind(doc,object,transform,AnalysisKind::StainMeans);
    rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(wrong_kind,rgb,labels,options);});
    require(mask->scientific_grid() && mask->width()==4 && mask->height()==4 && mask->level()==0);
    require(mask->downsample_x()==2 && mask->downsample_y()==3);
    require(mask->samples()==4 && mask->foreground_pixels()==4 && mask->components().size()==2);
    require(mask->bounds().x==0 && mask->bounds().y==-2 && mask->bounds().width==8 && mask->bounds().height==12);
    const auto& first=mask->components()[0];const auto& second=mask->components()[1];
    require(first.pixels==3 && first.bounds.x==2 && first.bounds.y==1 && first.bounds.width==4 && first.bounds.height==6);
    require(close(first.centroid.x,11./3.) && close(first.centroid.y,3.5));
    require(second.pixels==1 && second.centroid.x==5 && second.centroid.y==5.5);
    require(mask->provenance()->effective_pixel_width_um==.5 && mask->provenance()->effective_pixel_height_um==.25);
    require(mask->white()==transform.white() && mask->stains()==transform.stains());
    rejects<std::logic_error>([&]{mask->downsample();});rejects<std::logic_error>([&]{mask->level_bounds();});
    rejects<std::out_of_range>([&]{mask->sample_edge_x(5);});rejects<std::out_of_range>([&]{mask->sample_edge_y(-1);});
    rejects<std::invalid_argument>([&]{TissueArchiveCodec::encode(*mask);});
    TissueRequest legacy(doc,object,transform);rejects<std::logic_error>([&]{doc.keep_tissue(legacy,mask);});
    const auto legacy_mask=detect_tissue(legacy);
    require(!legacy_mask->scientific_grid() && legacy_mask->downsample_x()==legacy_mask->downsample() && legacy_mask->downsample_y()==legacy_mask->downsample());
    require(legacy_mask->sample_edge_x(1)==legacy_mask->bounds().x+legacy_mask->downsample());
    auto exact=options;exact.parameters.work_bytes=mask->estimated_work_bytes();
    require(ScientificRasterKernel::make(source,rgb,labels,exact)->labels()==labels);
    --exact.parameters.work_bytes;rejects<std::length_error>([&]{ScientificRasterKernel::make(source,rgb,labels,exact);});
    auto bad=labels;bad[0]=1;rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,bad,options);});
    bad=labels;bad[5]=std::uint32_t(options.parameters.max_components+1);rejects<std::length_error>([&]{ScientificRasterKernel::make(source,rgb,bad,options);});
    bad.assign(labels.size(),0);bad[5]=2;rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,bad,options);});
    auto wrong=options;wrong.ticket.reset();rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,labels,wrong);});
    wrong=options;wrong.input_sha256=std::string(64,'G');rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,labels,wrong);});
    wrong=options;wrong.input_sha256=std::string(64,'b');rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,labels,wrong);});
    wrong=options;wrong.parameters.level=1;rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,labels,wrong);});
    wrong=options;wrong.parameters.sampling_policy=RoiSamplingPolicy::PolygonWinding;rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,labels,wrong);});
    wrong=options;wrong.threshold_pixels=3;rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(source,rgb,labels,wrong);});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{ScientificRasterKernel::make(source,rgb,labels,options,&cancelled);});
    const auto empty=ScientificRasterKernel::make(source,rgb,std::vector<std::uint32_t>(labels.size()),spec(0,0));
    require(empty->components().empty() && empty->samples()==4);
    {
        // Decoded pixels can change without changing a sampled mean. Input
        // identity must still bind the actual snapshot, without rereading it.
        std::vector<std::uint8_t> changed(300,32);changed[(1*10+2)*3]=33;changed[(1*10+3)*3]=31;
        Document altered(Image(10,10,changed));altered.set_calibration(.5,.25);
        const auto id=altered.add_rectangle({2,1,4,6});AnalysisRequest altered_source(altered,id,transform,AnalysisKind::OpticalDensityMeans);
        const auto altered_rgb=read_scientific_rgb(*altered.server(),grid);
        require(altered_rgb.values()==rgb.values() && altered_rgb.valid()==rgb.valid());
        require(altered_rgb.decoded_source_sha256()!=rgb.decoded_source_sha256());
        require(scientific_raster_input_sha256(altered_source,altered_rgb)!=mask->input_sha256());
        const auto prior=scientific_raster_input_sha256(source,rgb);doc.set_calibration(1.,1.);
        require(scientific_raster_input_sha256(source,rgb)==prior); // capture is immutable
        AnalysisRequest recalibrated(doc,object,transform,AnalysisKind::OpticalDensityMeans);
        require(scientific_raster_input_sha256(recalibrated,rgb)!=prior);
        doc.set_calibration(.5,.25);
    }
    const auto boundary=doc.add_rectangle({1,1,2,2});AnalysisRequest boundary_source(doc,boundary,transform,AnalysisKind::OpticalDensityMeans);
    const ScientificGrid boundary_grid(10,10,{0,6,3},{0,4,2});auto boundary_input=read_scientific_rgb(*doc.server(),boundary_grid);
    require(scientific_sample_eligible(boundary_source,boundary_input,0,0));
    require(!scientific_sample_eligible(boundary_source,boundary_input,1,0) && !scientific_sample_eligible(boundary_source,boundary_input,0,1));
    const auto boundary_mask=ScientificRasterKernel::make(boundary_source,boundary_input,{1,0,0,0,0,0},spec(1,1));
    require(boundary_mask->samples()==1);
    rejects<std::invalid_argument>([&]{ScientificRasterKernel::make(boundary_source,boundary_input,{0,1,0,0,0,0},spec(1,1));});
    // Exact edge conversion also applies at large origins. A grid that cannot
    // represent interior centers is rejected before IO by the planner helper.
    const auto large_reader=std::make_shared<LargeReader>();Document large(large_reader);const auto large_object=large.add_rectangle({999999999989.,2,7,5});
    AnalysisRequest large_source(large,large_object,transform,AnalysisKind::OpticalDensityMeans);
    const ScientificGrid large_grid(1000000000000LL,16,{999999999989LL,7,3,1},{2,5,2});
    auto large_input=read_scientific_rgb(*large.server(),large_grid);std::vector<std::uint32_t> large_labels(10);large_labels[1]=1;
    const auto large_mask=ScientificRasterKernel::make(large_source,large_input,large_labels,spec(1,1));
    require(large_reader->reads==1);
    require(large_mask->components()[0].bounds.x==large_grid.x().edge(0).binary64());
    require(large_mask->sample_edge_x(2)==large_grid.x().edge(1).binary64());
    rejects<std::invalid_argument>([]{validate_scientific_raster_geometry(ScientificGrid(1000000000000LL,1,{999999999998LL,1,1000000},{0,1,1}));});
    std::cout<<"Native explicit-grid raster eligibility, per-axis metadata, precision and legacy isolation passed\n";
    if(has_geos()) {
        const auto contours=trace_tissue(*mask);require(contours->components().size()==2);
        require(close(contours->components()[0].geometry.area(),18) && close(contours->components()[1].geometry.area(),6));
        const auto clipped=trace_tissue(*boundary_mask);require(clipped->components().size()==1 && close(clipped->components()[0].geometry.area(),1));
        const auto large_contours=trace_tissue(*large_mask);require(large_contours->components().size()==1);
        const auto box=large_mask->components()[0].bounds;require(std::abs(large_contours->components()[0].geometry.area()-box.area())<1e-8);
        Document holes(Image(8,8,std::vector<std::uint8_t>(8*8*3,32)));
        const auto hole_object=holes.add_roi(Roi::polygon({{0,0},{8,0},{8,8},{0,8}},{{{2,2},{6,2},{6,6},{2,6}}}));
        AnalysisRequest hole_source(holes,hole_object,transform,AnalysisKind::OpticalDensityMeans);const ScientificGrid hole_grid(8,8,{0,8,4},{0,8,4});auto hole_input=read_scientific_rgb(*holes.server(),hole_grid);
        std::vector<std::uint32_t> hole_labels(16);for(std::size_t i=0;i<16;++i) hole_labels[i]=scientific_sample_eligible(hole_source,hole_input,i%4,i/4)?1:0;
        const auto hole_mask=ScientificRasterKernel::make(hole_source,hole_input,hole_labels,spec(12,1));
        const auto hole_contours=trace_tissue(*hole_mask);require(hole_mask->samples()==12 && hole_contours->components().size()==1);
        require(close(hole_contours->components()[0].geometry.area(),48));
        require(!hole_contours->components()[0].geometry.contains({4,4}));
        std::cout<<"GEOS explicit-grid edges, ROI holes and anisotropic areas passed\n";
    }
}
