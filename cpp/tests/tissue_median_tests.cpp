#include "qupathxx/tissue.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "../tissue_cleanup.hpp"
#include "../tissue_archive.hpp"
#include "../sha256.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("tissue median check failed");}
template<class F> void rejects(F f) {bool rejected=false;try {f();}catch(const std::exception&) {rejected=true;}check(rejected);}
std::vector<std::uint32_t> reference(const std::vector<std::uint32_t>& input,const std::vector<std::uint8_t>& domain,int w,int h) {
    std::vector<std::uint32_t> output(input.size());
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        std::array<int,9> neighborhood{};int i=0;
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            const auto at=std::clamp(y+dy,0,h-1)*w+std::clamp(x+dx,0,w-1);
            neighborhood[i++]=domain[at] && input[at]!=0;
        }
        std::sort(neighborhood.begin(),neighborhood.end());
        output[y*w+x]=domain[y*w+x] && neighborhood[4]?tissue_detail::foreground:0;
    }
    return output;
}
class Binary final:public ImageServer {
public:
    ImageMetadata info;
    Binary() {info.width=54;info.height=36;info.levels={{54,36,1},{36,24,1.5}};}
    static bool mask(int x,int y,int level) {return (x*7+y*11+level*3)%13<6;}
    static bool opaque(int x,int y) {return (x+3*y)%31!=0;}
    const ImageMetadata& metadata() const override {return info;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        validate_region(info,r);PixelBuffer output(r.width,r.height);const auto scale=info.levels[r.level].downsample;
        const auto ox=int(std::llround(r.x/scale)),oy=int(std::llround(r.y/scale));
        for(int y=0;y<r.height;++y) for(int x=0;x<r.width;++x) {
            const auto i=(y*r.width+x)*4;for(int c=0;c<3;++c) output.writable_bytes()[i+c]=mask(ox+x,oy+y,r.level)?0:255;
            output.writable_bytes()[i+3]=opaque(ox+x,oy+y)?255:128;
        }
        return output;
    }
    void close() override {}
    bool closed() const override {return false;}
};
void resign(std::vector<std::uint8_t>& bytes,const Roi& roi) {
    Sha256 hash;const auto geometry=roi.wkt();hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());hash.update(bytes.data(),bytes.size()-64);
    const auto digest=hash.finish();std::copy(digest.begin(),digest.end(),bytes.end()-64);
}
int main() {
 try {
    for(unsigned bits=0;bits<512;++bits) for(unsigned eligible:{511u,0x1bbu,0x1efu,0u}) {
        std::vector<std::uint32_t> input(9);std::vector<std::uint8_t> domain(9);
        for(int i=0;i<9;++i) {input[i]=bits&(1u<<i)?tissue_detail::foreground:0;domain[i]=(eligible>>i)&1;}
        auto expected=reference(input,domain,3,3),actual=input;std::vector<std::uint32_t> scratch;scratch.reserve(9);const auto capacity=scratch.capacity();
        tissue_detail::median(actual,scratch,domain,3,3,nullptr);check(actual==expected && scratch.empty() && scratch.capacity()==capacity);
    }
    for(int w:{1,2,17}) for(int h:{1,2,19}) {
        std::vector<std::uint32_t> input(w*h);std::vector<std::uint8_t> domain(w*h,1);std::vector<std::uint32_t> scratch;
        for(int i=0;i<w*h;++i) input[i]=(i*13)%17<8?tissue_detail::foreground:0;
        const auto expected=reference(input,domain,w,h);tissue_detail::median(input,scratch,domain,w,h,nullptr);check(input==expected);
    }
    auto server=std::make_shared<Binary>();Document doc(server);StainTransform transform({{1,0,0},{0,1,0}});
    std::vector<Roi> rois{Roi::rectangle({1.2,2.1,48,30}),Roi::ellipse({1.2,2.1,48,30})};
    if(has_geos()) rois.push_back(Roi::polygon({{1.2,2.1},{49.2,2.1},{49.2,32.1},{1.2,32.1}},{{{15,8},{21,8},{21,15},{15,15}}}));
    for(const auto& roi:rois) {
        const auto id=doc.add_roi(roi);
        for(int level:{0,1}) for(double sigma:{0.,1.7}) for(auto policy:{RoiSamplingPolicy::GeometryCoverage,RoiSamplingPolicy::PolygonWinding}) {
            TissueParameters p;p.level=level;p.smoothing_sigma=sigma;p.sampling_policy=policy;
            const TissueRequest base(doc,id,transform,p);const auto raw=detect_tissue(base);const auto b=base.level_bounds();const int w=int(b.width),h=int(b.height);
            std::vector<std::uint8_t> domain(w*h);
            for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
                const auto gx=int(b.x)+x,gy=int(b.y)+y;domain[y*w+x]=Binary::opaque(gx,gy) && roi.contains_sample({(gx+.5)*base.downsample(),(gy+.5)*base.downsample()},policy);
            }
            auto expected=reference(raw->labels(),domain,w,h);std::vector<std::uint32_t> prior;
            for(std::size_t tile:{16,31,256}) {
                p.median_cleanup=true;p.tile_size=tile;const auto result=detect_tissue(TissueRequest(doc,id,transform,p));
                check(result->algorithm()==(policy==RoiSamplingPolicy::GeometryCoverage?"qpx.threshold-components/5":"qpx.threshold-components/winding-5"));
                for(std::size_t i=0;i<expected.size();++i) check((result->labels()[i]!=0)==(expected[i]!=0));
                if(prior.empty()) prior=result->labels();else check(prior==result->labels());
                check(result->samples()==raw->samples() && result->threshold_pixels()==raw->threshold_pixels());
                const auto bytes=TissueArchiveCodec::encode(*result);check(bytes[3]==4);
                const auto restored=TissueArchiveCodec::decode(bytes.data(),bytes.size(),roi,doc.metadata());
                check(restored->parameters().median_cleanup && restored->parameters().smoothing_sigma==sigma && restored->labels()==result->labels());
                check(TissueArchiveCodec::encode(*restored)==bytes);
            }
        }
    }
    const auto id=doc.add_rectangle({0,0,54,36});TissueParameters p;p.median_cleanup=true;
    const TissueRequest request(doc,id,transform,p);auto result=detect_tissue(request);const auto record=doc.keep_tissue(request,result);
    check(detect_tissue(TissueRequest::replay(doc,record))->labels()==result->labels());
    const auto bytes=TissueArchiveCodec::encode(*result);
    for(int kind:{0,1,2}) {
        auto bad=bytes;if(kind==0) bad[20]=0;if(kind==1) bad[20]=2;if(kind==2) bad[4]=2;resign(bad,result->roi());
        rejects([&]{TissueArchiveCodec::decode(bad.data(),bad.size(),result->roi(),doc.metadata());});
    }
    const auto csv=measurements_csv(MeasurementTableRequest(doc,MeasurementTableKind::Tissue));
    check(csv.find("qpx.tissue-table/3")!=std::string::npos && csv.find("smoothing_sigma_sample_px,median_cleanup")!=std::string::npos);
    p.level=0;p.work_bytes=request.estimated_work_bytes()-1;rejects([&]{TissueRequest small(doc,id,transform,p);});
    TissueParameters legacy;check(detail::inactive_tissue_parameter_bytes(legacy)==detail::tissue_parameter_extension_bytes);
    legacy.smoothing_sigma=1.;check(detail::inactive_tissue_parameter_bytes(legacy)+sizeof(double)==detail::tissue_parameter_extension_bytes);
    legacy.median_cleanup=true;check(detail::inactive_tissue_parameter_bytes(legacy)==0);
    std::vector<std::uint32_t> labels(9,1),scratch;std::vector<std::uint8_t> domain(9,1);std::atomic<bool> stop{true};
    rejects([&]{tissue_detail::median(labels,scratch,domain,3,3,&stop);});
    std::cout<<"Exhaustive median rank/domain/border, native/Gaussian/level/tile integration, QTM4/CSV3 guards, replay and budgets passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
