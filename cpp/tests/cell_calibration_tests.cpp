#include "qupathxx/cells.hpp"
#include "../cell_archive.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("calibrated cell check failed");}
int main() {
    try {
        std::vector<std::uint8_t> rgb(32*24*3,255);
        for(int y=6;y<16;++y) for(int x=8;x<16;++x) for(int c=0;c<3;++c) rgb[(y*32+x)*3+c]=16;
        Document doc(Image(32,24,rgb));const auto target=doc.add_rectangle({0,0,32,24});
        CellParameters p;p.sigma=0;p.min_nucleus_pixels=1;p.cell_expansion=2;
        StainTransform transform({{1,0,0},{0,1,0}});
        auto unknown=detect_cells(CellRequest(doc,target,transform,p));check(unknown->features().size()==1);
        check(!unknown->calibrated_feature(1).nucleus_sample_area_um2);
        bool missing=false;
        try {CellRequest::with_nucleus_area_limits(doc,target,transform,1.,{},p);}
        catch(const std::invalid_argument&) {missing=true;}
        check(missing);
        doc.set_calibration(.5,.25);
        auto result=detect_cells(CellRequest(doc,target,transform,p));const auto f=result->calibrated_feature(1);
        check(f.pixel_width_um==.5 && f.pixel_height_um==.25);
        check(*f.nucleus_sample_area_um2==80*.125);
        check(*f.cell_sample_area_um2==result->features()[0].cell_pixels*.125);
        check(*f.cytoplasm_sample_area_um2==*f.cell_sample_area_um2-*f.nucleus_sample_area_um2);
        check(f.nucleus_centroid_um->x==12*.5 && f.nucleus_centroid_um->y==11*.25);
        const auto exact=CellRequest::with_nucleus_area_limits(doc,target,transform,10.,10.,p);
        check(exact.parameters().min_nucleus_pixels==80 && exact.parameters().max_nucleus_pixels==80);
        check(p.min_nucleus_pixels==1 && p.max_nucleus_pixels==0);
        const auto filtered=detect_cells(exact);check(filtered->features().size()==1);
        auto restored=CellArchiveCodec::encode(*filtered);
        auto frozen=CellArchiveCodec::decode(restored.data(),restored.size(),filtered->nuclei()->roi(),doc.metadata());
        check(frozen->parameters().min_nucleus_pixels==80 && frozen->parameters().max_nucleus_pixels==80);
        check(detect_cells(CellRequest::with_nucleus_area_limits(doc,target,transform,std::nextafter(10.,20.),{},p))->features().empty());
        check(detect_cells(CellRequest::with_nucleus_area_limits(doc,target,transform,{},std::nextafter(10.,0.),p))->features().empty());
        const auto empty=CellRequest::with_nucleus_area_limits(doc,target,transform,{},0.,p);
        check(empty.parameters().min_nucleus_pixels==32*24+1 && empty.parameters().max_nucleus_pixels==0);
        check(detect_cells(empty)->features().empty());
        doc.set_calibration(2,3);doc.server()->close();
        check(result->calibrated_feature(1).nucleus_sample_area_um2==f.nucleus_sample_area_um2);
        const auto bytes=CellArchiveCodec::encode(*result);
        const auto legacy=CellArchiveCodec::decode(bytes.data(),bytes.size(),result->nuclei()->roi(),doc.metadata());
        check(!legacy->calibrated_feature(1).pixel_width_um && !legacy->calibrated_feature(1).cell_centroid_um);
        for(auto label:{0u,2u}) {bool rejected=false;try {result->calibrated_feature(label);}catch(const std::out_of_range&) {rejected=true;}check(rejected);}
        // The final area is representable despite an overflowing intermediate
        // count*x product. The x centroid also remains within finite range.
        Document extreme(Image(2,2,{0,0,0,255,255,255,0,0,0,255,255,255}));
        auto id=extreme.add_rectangle({0,0,2,2});extreme.set_calibration(1e308,1e-308);p.cell_expansion=0;
        auto tiny=detect_cells(CellRequest(extreme,id,transform,p));auto physical=tiny->calibrated_feature(1);
        check(std::abs(*physical.nucleus_sample_area_um2-2)<1e-12 && physical.cytoplasm_sample_area_um2==0);
        extreme.set_calibration(1e-300,1e-300);tiny=detect_cells(CellRequest(extreme,id,transform,p));
        bool underflow=false;try {tiny->calibrated_feature(1);}catch(const std::overflow_error&) {underflow=true;}check(underflow);
        std::cout<<"Captured anisotropic areas/centroids, legacy unknown, detached lifetime and numerical ranges passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
