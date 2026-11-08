#include "../scientific_raster.hpp"
#include "../cell_sampling_grid.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>
#include "qupathxx/project.hpp"
#include <stdexcept>
using namespace qpx;
namespace {
void checked(bool value,int line){if(!value)throw std::runtime_error("cell raster pair check failed at line "+std::to_string(line));}
#define verify(...) checked((__VA_ARGS__),__LINE__)
template<class E=std::exception,class F>void rejects(F fn){bool hit=false;try{fn();}catch(const E&){hit=true;}verify(hit);}
void equal(const TissueResult& a,const TissueResult& b) {
    verify(a.input_sha256()==b.input_sha256() && a.labels()==b.labels() && a.samples()==b.samples());
    verify(a.foreground_pixels()==b.foreground_pixels() && a.estimated_work_bytes()==b.estimated_work_bytes());
    verify(a.components().size()==b.components().size() && a.algorithm()==b.algorithm());
    for(std::size_t i=0;i<a.components().size();++i) {
        const auto& x=a.components()[i];const auto& y=b.components()[i];
        verify(x.label==y.label && x.pixels==y.pixels && x.centroid.x==y.centroid.x && x.centroid.y==y.centroid.y);
        verify(x.bounds.x==y.bounds.x && x.bounds.y==y.bounds.y && x.bounds.width==y.bounds.width && x.bounds.height==y.bounds.height);
    }
}
}
void check_cell_raster_pair() {
    Document doc(Image(8,8,std::vector<std::uint8_t>(8*8*3,32)));doc.set_calibration(.5,.25);
    const StainTransform transform({{.65,.70,.29},{.07,.99,.11}});
    const auto object=doc.add_rectangle({1,1,6,6});
    AnalysisRequest source(doc,object,transform,AnalysisKind::OpticalDensityMeans);
    for(const auto& grid:{CellSamplingGrid(8,8,NominalCellSampling(RgbNominalRequest(0,0,8,8),0,0)),
                         CellSamplingGrid(8,8,NominalCellSampling(RgbNominalRequest(0,0,8,8,1.5),1,1)),
                         CellSamplingGrid(ScientificGrid(8,8,{0,8,8,0},{0,8,8,0}))}) {
        auto input=read_cell_rgb(*doc.server(),grid,nullptr);
        std::vector<std::uint32_t> nuclei(input.valid().size()),bodies(nuclei.size());
        for(std::size_t i=0;i<nuclei.size();++i) if(input.valid()[i] && grid.core_roi_contains(source.roi(),source.sampling_policy(),i%grid.width(),i/grid.width())) {
            bodies[i]=1;if(!std::any_of(nuclei.begin(),nuclei.end(),[](auto v){return v!=0;}))nuclei[i]=1;
        }
        ScientificRasterSpec ns;ns.ticket=std::make_shared<const char>();ns.raw_components=1;ns.threshold_pixels=1;
        auto bs=ns;bs.body=true;
        auto n=ScientificRasterKernel::make(source,input,nuclei,ns);auto b=ScientificRasterKernel::make(source,input,bodies,bs);
        auto pair=ScientificRasterKernel::make_pair(source,input,nuclei,bodies,ns,bs);equal(*n,*pair.first);equal(*b,*pair.second);
        auto exact_n=ns,exact_b=bs;exact_n.parameters.work_bytes=n->estimated_work_bytes();exact_b.parameters.work_bytes=b->estimated_work_bytes();
        pair=ScientificRasterKernel::make_pair(source,input,nuclei,bodies,exact_n,exact_b);equal(*n,*pair.first);equal(*b,*pair.second);
        --exact_b.parameters.work_bytes;rejects<std::length_error>([&]{ScientificRasterKernel::make_pair(source,input,nuclei,bodies,exact_n,exact_b);});
        auto bad=bs;bad.input_sha256=std::string(64,'0');rejects<std::invalid_argument>([&]{ScientificRasterKernel::make_pair(source,input,nuclei,bodies,ns,bad);});
        bad=ns;bad.input_sha256=std::string(64,'0');rejects<std::invalid_argument>([&]{ScientificRasterKernel::make_pair(source,input,nuclei,bodies,bad,bs);});
        auto invalid=bodies;invalid[0]=1;rejects([&]{ScientificRasterKernel::make_pair(source,input,nuclei,invalid,ns,bs);});
        invalid.assign(bodies.size(),0);for(std::size_t i=0;i<bodies.size();++i)if(bodies[i])invalid[i]=2;
        rejects([&]{ScientificRasterKernel::make_pair(source,input,nuclei,invalid,ns,bs);});
        invalid=bodies;invalid.pop_back();rejects([&]{ScientificRasterKernel::make_pair(source,input,nuclei,invalid,ns,bs);});
        auto changed=input;changed.support=input.support;
        if(changed.area)continue;
        changed.decoded_source_sha256=std::string(64,'a');bad=bs;bad.input_sha256=n->input_sha256();
        rejects<std::invalid_argument>([&]{ScientificRasterKernel::make_pair(source,changed,nuclei,bodies,ns,bad);});
        std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{ScientificRasterKernel::make_pair(source,input,nuclei,bodies,ns,bs,&cancelled);});
    }
    std::cout<<"Native raster pair: nominal/fractional/context/general equality, exact per-mask budgets, expected identity/source changes, invalid labels and cancellation passed\n";
}

// Opt-in developer measurement; ordinary correctness tests make no timing claim.
void benchmark_cell_raster_pair(const std::string& path) {
    auto project=Project::open_without_journal_recovery(path);auto doc=project->document(1);
    const auto result=doc->cell_record(3)->result;
    const auto& n=*result->nuclei();const auto& b=*result->cells();
    if(!n.nominal_sampling())throw std::invalid_argument("Probe requires the retained nominal cell fixture");
    const CellSamplingGrid grid(doc->metadata().width,doc->metadata().height,*n.nominal_sampling());
    const auto input=read_cell_rgb(*doc->server(),grid,nullptr);
    StainTransform transform(std::vector<ColorVector>(n.stains().begin(),n.stains().end()),n.white());
    AnalysisRequest source(*doc,n.object_id(),transform,AnalysisKind::OpticalDensityMeans);
    ScientificRasterSpec ns;ns.parameters=n.parameters();ns.ticket=std::make_shared<const char>();
    ns.threshold_pixels=n.threshold_pixels();ns.raw_components=n.raw_components();ns.algorithm_override=n.algorithm().c_str();
    auto bs=ns;bs.body=true;bs.parameters=b.parameters();bs.threshold_pixels=b.threshold_pixels();bs.raw_components=b.raw_components();bs.algorithm_override=b.algorithm().c_str();
    using Pair=std::pair<std::shared_ptr<const TissueResult>,std::shared_ptr<const TissueResult>>;
    for(int iteration=0;iteration<16;++iteration) {
        Pair separate,paired;double separate_ms=0,paired_ms=0;
        auto single=[&]{const auto started=std::chrono::steady_clock::now();
            auto x=ScientificRasterKernel::make(source,input,n.labels(),ns);auto expected=bs;expected.input_sha256=x->input_sha256();
            auto y=ScientificRasterKernel::make(source,input,b.labels(),expected);separate_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
            separate={std::move(x),std::move(y)};};
        auto pair=[&]{const auto started=std::chrono::steady_clock::now();
            paired=ScientificRasterKernel::make_pair(source,input,n.labels(),b.labels(),ns,bs);
            paired_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();};
        if(iteration%2){pair();single();}else{single();pair();}
        equal(*separate.first,*paired.first);equal(*separate.second,*paired.second);
        if(iteration>=4)std::cout<<"{\"iteration\":"<<iteration<<",\"separate_ms\":"<<separate_ms<<",\"paired_ms\":"<<paired_ms<<"}\n";
    }
    doc->server()->close();
}
