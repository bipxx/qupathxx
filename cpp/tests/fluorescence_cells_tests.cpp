#include "qupathxx/fluorescence_cells.hpp"
#include "qupathxx/workflow.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
using namespace qpx;
void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
void near(double a,double b,double tolerance=1e-10){check(std::isfinite(a) && std::abs(a-b)<=tolerance,"Raw-cell result differs from independent expected value");}
template<class E=std::exception,class F> void rejects(F action){try{action();}catch(const E&){return;}throw std::runtime_error("Expected fluorescence cell refusal");}
RawPixelBuffer fixture() {
    RawPixelBuffer image(15,9,4,SampleType::Float64);auto& values=std::get<std::vector<double>>(image.writable_storage());
    std::fill(image.writable_valid().begin(),image.writable_valid().end(),1);
    for(unsigned y=0;y<9;++y)for(unsigned x=0;x<15;++x) {
        const auto i=y*15+x;values[4*i]=(y>=3 && y<=5 && ((x>=3 && x<=5) || (x>=9 && x<=11)))?100:0;
        values[4*i+1]=x+10*y;values[4*i+2]=-double(x+10*y);values[4*i+3]=0;
    }
    return image;
}
void measurements() {
    RawPixelBuffer pixels(4,1,4,SampleType::Float64);auto& values=std::get<std::vector<double>>(pixels.writable_storage());
    values={10,0,NAN,-2,20,0,INFINITY,-4,30,0,6,-6,999,999,999,999};pixels.writable_valid()={1,1,1,0};
    const auto result=measure_raw_cell_compartments(pixels,{1,1,0,0},{1,1,1,1},1);
    check(result.size()==1 && result[0].nucleus.pixels==2 && result[0].cell.pixels==4 && result[0].cell.valid_pixels==3,"Independent compartment sizes and invalid padding");
    const auto& cell=result[0];check(cell.cytoplasm && cell.cytoplasm->pixels==2 && cell.cytoplasm->valid_pixels==1,"Cytoplasm excludes all nuclei, retains invalid geometric support");
    near(*cell.nucleus.channels[0].sum,30);near(*cell.nucleus.channels[0].mean,15);near(*cell.nucleus.channels[0].sample_standard_deviation,std::sqrt(50.));
    near(*cell.cell.channels[0].sum,60);near(*cell.cell.channels[0].mean,20);near(*cell.cell.channels[0].sample_standard_deviation,10);
    near(*cell.cytoplasm->channels[0].mean,30);check(!cell.cytoplasm->channels[0].sample_standard_deviation,"One finite sample has no sample SD");
    check(cell.nucleus.channels[2].finite_samples==0 && cell.nucleus.channels[2].nonfinite_samples==2 && !cell.nucleus.channels[2].mean && !cell.nucleus.channels[2].sum,"All nonfinite channel is missing, not zero");
    near(*cell.cell.channels[2].mean,6);near(*cell.nucleus.channels[1].mean,0);near(*cell.cytoplasm->channels[3].mean,-6);
    const auto empty=measure_raw_cell_compartments(pixels,{1,1,0,0},{1,1,0,0},1);check(!empty[0].cytoplasm,"Empty cytoplasm absent");
    pixels.writable_valid()={0,0,0,0};const auto missing=measure_raw_cell_compartments(pixels,{1,1,0,0},{1,1,0,0},1);
    check(missing[0].nucleus.pixels==2 && !missing[0].nucleus.valid_pixels && !missing[0].nucleus.channels[0].mean,"Invalid geometry has no invented intensity");
    rejects<std::invalid_argument>([&]{measure_raw_cell_compartments(pixels,{1,1,2,0},{1,1,1,0},2);});
    rejects<std::invalid_argument>([&]{measure_raw_cell_compartments(pixels,{1,1,0,0},{1,1,0,0},2);});
    check(measure_raw_cell_compartments(pixels,{0,0,0,0},{0,0,0,0},0).empty(),"No cells is a valid empty measurement result");
    RawPixelBuffer integers(1,1,1,SampleType::UInt64);integers.writable_valid()[0]=1;
    auto& counts=std::get<std::vector<std::uint64_t>>(integers.writable_storage());counts[0]=1ULL<<53;
    near(*measure_raw_cell_compartments(integers,{1},{1},1)[0].cell.channels[0].mean,double(1ULL<<53));
    ++counts[0];rejects<std::invalid_argument>([&]{measure_raw_cell_compartments(integers,{1},{1},1);});
}
void segmentation() {
    auto image=fixture();const auto original=std::get<std::vector<double>>(image.storage());std::vector<std::uint8_t> domain(15*9,1);
    FluorescenceCellOptions options;options.threshold=50;options.cell_expansion=1.5;
    const auto result=segment_fluorescence_cells(image,domain,options);
    check(result.measurements.size()==2 && result.nuclei.size()==135 && result.bodies.size()==135,"Two independently defined nuclei detected");
    check(result.segmentation_finite_pixels==135 && !result.segmentation_nonfinite_pixels && !result.invalid_pixels,"Segmentation support counters");
    for(std::size_t i=0;i<135;++i) {
        const auto x=i%15,y=i/15;const auto a=y>=3 && y<=5?(x>=3 && x<=5?1u:x>=9 && x<=11?2u:0u):0u;
        const auto b=y>=2 && y<=6?(x>=2 && x<=6?1u:x>=8 && x<=12?2u:0u):0u;
        check(result.nuclei[i]==a && result.bodies[i]==b,"Exact independently constructed threshold masks and radius expansion");
    }
    for(std::size_t c=0;c<2;++c) {
        const auto& m=result.measurements[c];check(m.nucleus.pixels==9 && m.cell.pixels==25 && m.cytoplasm && m.cytoplasm->pixels==16,"Analytical body/nucleus/cytoplasm counts");
        near(*m.nucleus.channels[1].mean,44+c*6);near(*m.cell.channels[1].mean,44+c*6);near(*m.cytoplasm->channels[1].mean,44+c*6);
        near(*m.cell.channels[3].mean,0);check(!result.boundaries[c].cell && !result.boundaries[c].nucleus,"Complete objects unflagged");
    }
    check(std::get<std::vector<double>>(image.storage())==original,"Segmentation/measurement never change raw samples");
    const auto repeated=segment_fluorescence_cells(image,domain,options);check(result.input_sha256==repeated.input_sha256 && result.recipe_sha256==repeated.recipe_sha256 && result.nuclei==repeated.nuclei,"Deterministic raw identity/recipe/masks");
    options.threshold_method=FluorescenceThreshold::Otsu256;const auto automatic=segment_fluorescence_cells(image,domain,options);
    check(automatic.nuclei==result.nuclei && automatic.recipe_sha256!=result.recipe_sha256 && automatic.input_sha256==result.input_sha256,"Otsu recovers independent binary foreground without changing raw identity");near(automatic.resolved_threshold,100./256);
    options.threshold_method=FluorescenceThreshold::Explicit;options.threshold=100;
    check(segment_fluorescence_cells(image,domain,options).measurements.empty(),"Threshold equality is background");
    options.threshold=50;options.cell_expansion=0;
    const auto nuclei_only=segment_fluorescence_cells(image,domain,options);check(nuclei_only.nuclei==nuclei_only.bodies && !nuclei_only.measurements[0].cytoplasm,"Radius zero preserves nuclei and absent cytoplasm");
    options.units=FluorescenceCellUnits::Micrometres;options.cell_expansion=.75;options.peak_prominence=.25;options.min_nucleus_area=4.5;options.max_nucleus_area=4.5;
    const auto physical=segment_fluorescence_cells(image,domain,options,std::array<double,2>{.5,1});
    check(physical.measurements.size()==2 && physical.measurements[0].nucleus.pixels==9 && physical.measurements[0].cell.pixels==15,"Anisotropic calibrated areas and expansion");
    options.min_nucleus_area=4.6;options.max_nucleus_area.reset();check(segment_fluorescence_cells(image,domain,options,std::array<double,2>{.5,1}).measurements.empty(),"Physical minimum area is inclusive, applied in squared declared units");
    options={};options.threshold=50;options.cell_expansion=1.5;
    for(unsigned y=0;y<9;++y)for(unsigned x=0;x<3;++x)domain[y*15+x]=0;
    const auto boundary=segment_fluorescence_cells(image,domain,options);check(boundary.boundaries[0].nucleus && boundary.boundaries[0].cell,"ROI boundary truncation is retained and explicit");
    domain.assign(135,1);auto& data=std::get<std::vector<double>>(image.writable_storage());data[0]=NAN;image.writable_valid()[1]=0;
    const auto masked=segment_fluorescence_cells(image,domain,options);check(masked.segmentation_nonfinite_pixels==1 && masked.invalid_pixels==1 && masked.segmentation_finite_pixels==133,"Nonfinite/invalid segmentation samples excluded explicitly");
}
void failures() {
    auto image=fixture();std::vector<std::uint8_t> domain(135,1);FluorescenceCellOptions p;p.threshold=50;
    p.units=FluorescenceCellUnits::Micrometres;rejects<std::invalid_argument>([&]{segment_fluorescence_cells(image,domain,p);});
    rejects<std::invalid_argument>([&]{segment_fluorescence_cells(image,domain,p,std::array<double,2>{0,1});});p.units=FluorescenceCellUnits::Pixels;
    p.max_cells=1;rejects<std::length_error>([&]{segment_fluorescence_cells(image,domain,p);});p.max_cells=100;
    p.segmentation_channel=4;rejects<std::invalid_argument>([&]{segment_fluorescence_cells(image,domain,p);});p.segmentation_channel=0;
    p.work_bytes=32;rejects<WorkflowCaptureLimit>([&]{segment_fluorescence_cells(image,domain,p);});p.work_bytes=256*1024*1024;
    domain[0]=2;rejects<std::invalid_argument>([&]{segment_fluorescence_cells(image,domain,p);});domain[0]=1;
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{segment_fluorescence_cells(image,domain,p,{},&cancelled);});
    rejects<WorkflowCancelled>([&]{measure_raw_cell_compartments(image,std::vector<std::uint32_t>(135),std::vector<std::uint32_t>(135),0,65536,&cancelled);});
    image.writable_valid().resize(1);rejects<std::invalid_argument>([&]{segment_fluorescence_cells(image,domain,p);});
    image=fixture();image.writable_storage()=std::vector<std::uint16_t>(540);rejects<std::invalid_argument>([&]{segment_fluorescence_cells(image,domain,p);});
}
void smoothing_and_constant() {
    RawPixelBuffer impulse(9,5,1,SampleType::Float64);std::fill(impulse.writable_valid().begin(),impulse.writable_valid().end(),1);
    std::get<std::vector<double>>(impulse.writable_storage())[2*9+4]=100;
    FluorescenceCellOptions p;p.threshold=5;p.sigma=.5;
    const auto smoothed=segment_fluorescence_cells(impulse,std::vector<std::uint8_t>(45,1),p);
    check(smoothed.measurements.size()==1 && smoothed.measurements[0].nucleus.pixels==5,"Gaussian impulse threshold has analytical five-pixel cross");
    near(*smoothed.measurements[0].nucleus.channels[0].sum,100);near(*smoothed.measurements[0].nucleus.channels[0].mean,20);
    for(std::size_t i=0;i<45;++i) {const auto x=i%9,y=i/9;const auto manhattan=std::abs(int(x)-4)+std::abs(int(y)-2);check(bool(smoothed.nuclei[i])==(manhattan<=1),"Smoothed detection mask, original raw intensity measurement");}
    RawPixelBuffer constant(3,3,1,SampleType::UInt16);std::fill(constant.writable_valid().begin(),constant.writable_valid().end(),1);
    std::fill(std::get<std::vector<std::uint16_t>>(constant.writable_storage()).begin(),std::get<std::vector<std::uint16_t>>(constant.writable_storage()).end(),7);
    p={};p.threshold_method=FluorescenceThreshold::Otsu256;
    const auto result=segment_fluorescence_cells(constant,std::vector<std::uint8_t>(9,1),p);near(result.resolved_threshold,7);check(result.measurements.empty(),"Constant Otsu input has no invented foreground");
    RawPixelBuffer adjacent(3,3,1,SampleType::Float64);std::fill(adjacent.writable_valid().begin(),adjacent.writable_valid().end(),1);
    std::fill(std::get<std::vector<double>>(adjacent.writable_storage()).begin(),std::get<std::vector<double>>(adjacent.writable_storage()).end(),std::nextafter(128.,INFINITY));
    p={};const auto close=segment_fluorescence_cells(adjacent,std::vector<std::uint8_t>(9,1),p);
    check(close.measurements.size()==1 && close.measurements[0].nucleus.pixels==9,"Strict foreground threshold is authoritative; no rounded regional mean threshold");
    constant.writable_valid().reserve(1000000);rejects<WorkflowCaptureLimit>([&]{measure_raw_cell_compartments(constant,std::vector<std::uint32_t>(9),std::vector<std::uint32_t>(9),0,65536);});
}
int main(int argc,char** argv){try {
    measurements();segmentation();failures();smoothing_and_constant();
    if(argc>1) {
        const auto source=open_tiff(argv[1]);const auto metadata=source->raw_metadata();RawRegionRequest region;region.region={0,0,0,metadata->width,metadata->height};region.channels=metadata->channels;
        const auto raw=source->read_raw(region);FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.min_nucleus_area=5;p.cell_expansion=3;
        const auto result=segment_fluorescence_cells(raw,std::vector<std::uint8_t>(std::size_t(raw.width())*raw.height(),1),p);
        check(!result.measurements.empty(),"Real admitted raw source produces bounded native cells");
        std::cout<<"Real raw source: "<<result.measurements.size()<<" candidates; geometry smoke only, no annotation/biological acceptance claim\n";
    }
    std::cout<<"Fluorescence cells: independent masks/compartments, raw finite/missing/zero semantics, Otsu, anisotropic metric, boundaries and refusal checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
