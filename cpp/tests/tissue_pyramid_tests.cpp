#include "qupathxx/tissue.hpp"
#include "qupathxx/input_digest.hpp"
#include "../tissue_archive.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace qpx;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F function) {bool rejected=false;try {function();}catch(const std::exception&) {rejected=true;}check(rejected,"expected rejection");}
class PhaseReader final:public ImageServer {
public:
    ImageMetadata info;
    mutable unsigned reads=0;
    PhaseReader() {info.width=512;info.height=384;info.levels={{512,384,1},{205,154,2.5}};}
    const ImageMetadata& metadata() const override {return info;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        validate_region(info,r);++reads;PixelBuffer pixels(r.width,r.height);
        const auto scale=info.levels[r.level].downsample;
        const auto ox=std::llround(r.x/scale),oy=std::llround(r.y/scale);
        for(std::int64_t y=0;y<r.height;++y) for(std::int64_t x=0;x<r.width;++x) {
            const auto gx=ox+x,gy=oy+y;bool dark=gx%17<5 || gy%13<3;
            // Model a phase-sensitive decoder: caller-dependent tile starts
            // cannot establish the same input samples as one origin-zero read.
            if((r.x || r.y) && (gx+gy)%7==0) dark=!dark;
            const auto at=static_cast<std::size_t>((y*r.width+x)*4);
            for(unsigned c=0;c<3;++c) pixels.writable_bytes()[at+c]=dark?0:255;
            pixels.writable_bytes()[at+3]=(gx==4 && gy==5)?0:255;
        }
        return pixels;
    }
    void close() override {}
    bool closed() const override {return false;}
};
}
int main() {
    try {
        auto source=std::make_shared<PhaseReader>();Document doc(source);
        const auto obj=doc.add_rectangle({0,0,512,384});StainTransform transform({{.65,.70,.29},{.07,.99,.11}});
        TissueParameters p;p.level=1;p.canonical_pyramid=true;p.tile_size=31;
        TissueRequest request(doc,obj,transform,p);check(source->reads==0,"planner decoded input");
        const auto budget=request.estimated_work_bytes();
        auto first=detect_tissue(request);check(source->reads==1,"snapshot requires exactly one source read");
        check(first->samples()==205u*154u-1,"canonical alpha support");
        std::uint64_t expected=0;
        for(std::int64_t y=0;y<154;++y) for(std::int64_t x=0;x<205;++x) {
            const bool dark=(x%17<5 || y%13<3) && !(x==4 && y==5);expected+=dark;
            check((first->labels()[static_cast<std::size_t>(y*205+x)]!=0)==dark,"independent snapshot mask");
        }
        check(first->foreground_pixels()==expected && first->components().size()==1,"independent component support");
        check(first->input_sha256()==input_sha256(request),"snapshot digest API disagrees");
        p.tile_size=47;auto second=detect_tissue(TissueRequest(doc,obj,transform,p));
        check(first->labels()==second->labels(),"processing partition changes labels");
        check(first->input_sha256()==second->input_sha256(),"processing partition changes canonical input");
        check(first->algorithm()=="qpx.pyramid-threshold-components/1","canonical algorithm identity");
        auto encoded=TissueArchiveCodec::encode(*first);
        check(encoded[3]==6,"canonical mask must use QTM6");
        auto restored=TissueArchiveCodec::decode(encoded.data(),encoded.size(),request.roi(),source->metadata());
        check(restored->parameters().canonical_pyramid && restored->labels()==first->labels(),"QTM6 replay state");
        check(restored->input_sha256()==first->input_sha256(),"QTM6 input identity");
        const auto before=source->reads;p.tile_size=31;p.work_bytes=budget-1;
        rejects([&]{TissueRequest invalid(doc,obj,transform,p);});check(source->reads==before,"budget failure performed IO");
        p.work_bytes=64u*1024u*1024u;p.level=0;rejects([&]{p.validate();});p.level=-1;rejects([&]{p.validate();});
        p.level=1;p.smoothing_sigma=.8;p.median_cleanup=true;
        auto filtered=detect_tissue(TissueRequest(doc,obj,transform,p));p.tile_size=47;
        check(filtered->labels()==detect_tissue(TissueRequest(doc,obj,transform,p))->labels(),"filtered canonical partition");
        p.smoothing_sigma=0;p.median_cleanup=false;p.canonical_pyramid=false;
        auto legacy=detect_tissue(TissueRequest(doc,obj,transform,p));
        check(TissueArchiveCodec::encode(*legacy)[3]==1,"legacy format changed");
        check(legacy->input_sha256()!=first->input_sha256(),"legacy sampling relabeled canonical");
        p.canonical_pyramid=true;TissueJob job;check(job.request(TissueRequest(doc,obj,transform,p)),"job start");job.cancel();
        check(job.wait(doc,obj).state=="cancelled" && !job.result(),"cancelled snapshot published result");
        std::cout<<"canonical pyramid: independent pixels, partition/digest, QTM6, budget and cancellation pass\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
