#include "../cell_contours.hpp"
#include "../cell_archive.hpp"
#include "../scientific_raster.hpp"
#include "../cell_sampling_grid.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
void checked(bool value,int line) {if(!value) throw std::runtime_error("integer cell geometry check failed at line "+std::to_string(line));}
#define verify(value) checked(bool(value),__LINE__)
template<class E,class F> void rejects(F action) {try {action();} catch(const E&) {return;}throw std::runtime_error("integer cell geometry rejection missing");}
}
// Keep these value-heavy fixtures out of the large nominal test's stack frame.
void check_integer_cell_geometry() {
    using namespace qpx;
    const StainTransform transform({{.65,.7,.29},{.07,.99,.11}});
    // A large nominal result uses independent contour workers. Compare its
    // exact geometry with serial tracing and exercise a joined worker failure.
    {
        std::vector<std::uint8_t> pixels(256*256*3);
        for(int y=0;y<256;++y) for(int x=0;x<256;++x) {
            const auto dx=x%8-4,dy=y%16-8;const bool dark=dx*dx+dy*dy<=9;
            const std::array<std::uint8_t,3> color=dark?std::array<std::uint8_t,3>{75,55,120}:std::array<std::uint8_t,3>{245,244,242};
            for(int c=0;c<3;++c) pixels[(y*256+x)*3+c]=color[c];
        }
        Document doc(Image(256,256,std::move(pixels)));const auto object=doc.add_rectangle({0,0,256,256});
        GridCellParameters p;p.processing=CellProcessingPolicy::QuPath070V2;p.threshold=.4;p.min_nucleus_area=16;p.max_nucleus_area=512;
        const auto request=CellRequest::for_nominal_roi(doc,object,transform,1,0,0,p,RgbNominalSampling::AffineInverse);
        const auto result=detect_cells(request);verify(result->features().size()==512);
        for(bool body:{false,true}) {
            const auto serial=trace_tissue(*(body?result->cells():result->nuclei()),p.contours);
            const auto concurrent=body?result->cell_contours():result->nucleus_contours();
            verify(serial->components().size()==concurrent->components().size());
            for(std::size_t i=0;i<serial->components().size();++i)
                verify(serial->components()[i].geometry.wkt()==concurrent->components()[i].geometry.wkt());
        }
        verify(verify_integer_cell_containment(*result->nuclei(),*result->nucleus_contours(),*result->cells(),*result->cell_contours()));
        const CellSamplingGrid lattice(256,256,NominalCellSampling(RgbNominalRequest(0,0,256,256,1,RgbNominalSampling::AffineInverse),0,0));
        const auto rgb=read_cell_rgb(*doc.server(),lattice,nullptr);
        AnalysisRequest source(doc,object,transform,AnalysisKind::OpticalDensityMeans);
        auto shifted=result->nuclei()->labels();std::fill(shifted.begin(),shifted.end(),0);
        std::copy(result->nuclei()->labels().begin(),result->nuclei()->labels().end()-256,shifted.begin()+256);
        ScientificRasterSpec spec;spec.ticket=std::make_shared<const char>();spec.raw_components=512;spec.threshold_pixels=result->nuclei()->foreground_pixels();
        const auto wrong=ScientificRasterKernel::make(source,rgb,std::move(shifted),spec);
        const auto wrong_geometry=trace_tissue(*wrong,p.contours);
        for(std::size_t i=0;i<512;++i) verify(wrong_geometry->components()[i].pixels==result->nucleus_contours()->components()[i].pixels);
        // Same labels/counts/areas, wrong positions: a metadata-only check fails.
        rejects<std::runtime_error>([&]{verify_integer_cell_containment(*result->nuclei(),*wrong_geometry,*result->cells(),*result->cell_contours());});
        // Each geometry matches its own raster, but the body loses nucleus pixels.
        rejects<std::runtime_error>([&]{verify_integer_cell_containment(*result->nuclei(),*result->nucleus_contours(),*wrong,*wrong_geometry);});
        std::atomic<bool> stop{true};rejects<std::runtime_error>([&]{verify_integer_cell_containment(*result->nuclei(),*result->nucleus_contours(),*result->cells(),*result->cell_contours(),&stop);});
        const auto packet=CellArchiveCodec::encode(*result);
        const auto restored=CellArchiveCodec::decode(packet.data(),packet.size(),request.roi(),doc.metadata());
        verify(CellArchiveCodec::encode(*restored)==packet);
        auto refused=p;refused.contours.max_edges=3;
        rejects<std::length_error>([&]{detect_cells(CellRequest::for_nominal_roi(doc,object,transform,1,0,0,refused,RgbNominalSampling::AffineInverse));});
    }
    // Exhaust all2x2 background/body/nucleus states with a nonempty nucleus.
    {
        Document small(Image(7,7,std::vector<std::uint8_t>(147,32)));const auto object=small.add_rectangle({0,0,7,7});
        AnalysisRequest source(small,object,transform,AnalysisKind::OpticalDensityMeans);
        const auto rgb=read_cell_rgb(*small.server(),CellSamplingGrid(7,7,NominalCellSampling(RgbNominalRequest(0,0,7,7),0,0)),nullptr);
        for(unsigned trial=0;trial<82;++trial) {
            unsigned value=trial;std::vector<std::uint32_t> n(49),b(49);
            for(int y=1;y<3;++y) for(int x=1;x<3;++x) {const auto state=value%3;value/=3;n[y*7+x]=state==2;b[y*7+x]=state!=0;}
            if(trial==81) {
                std::fill(n.begin(),n.end(),0);std::fill(b.begin(),b.end(),0);
                for(int y=0;y<7;++y) for(int x=0;x<7;++x) b[y*7+x]=x==0 || y==0 || x==6 || y==6;
                for(int x=0;x<7;++x) n[x]=1;
            }
            if(std::find(n.begin(),n.end(),1)==n.end() && std::find(b.begin(),b.end(),1)!=b.end()) continue;
            auto make=[&](std::vector<std::uint32_t> labels) {
                ScientificRasterSpec spec;spec.ticket=std::make_shared<const char>();spec.raw_components=std::find(labels.begin(),labels.end(),1)!=labels.end();
                spec.threshold_pixels=std::count(labels.begin(),labels.end(),1);
                return ScientificRasterKernel::make(source,rgb,std::move(labels),spec);
            };
            const auto nuclei=make(std::move(n)),bodies=make(std::move(b));
            const auto ng=trace_tissue(*nuclei),bg=trace_tissue(*bodies);
            verify(verify_integer_cell_containment(*nuclei,*ng,*bodies,*bg));
            if(!ng->components().empty()) verify(bg->components()[0].geometry.covers(ng->components()[0].geometry));
        }
    }
    std::cout<<"Integer cell boundary/area identity, inclusion, shifted-geometry refusal,65 exact patterns plus empty and hole cases, cancellation and canonical archive checks passed\n";
}
