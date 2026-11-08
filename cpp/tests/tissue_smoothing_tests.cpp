#include "qupathxx/tissue.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "../tissue_archive.hpp"
#include "../sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("tissue smoothing check failed");}
template<class F> void rejects(F action) {bool rejected=false;try {action();}catch(const std::exception&) {rejected=true;}check(rejected);}
class Pattern final:public ImageServer {
public:
    ImageMetadata info;
    Pattern() {info.width=54;info.height=36;info.levels={{54,36,1},{36,24,1.5}};}
    static std::uint8_t gray(int x,int y,int level) {return ((x+level*3)%17<5 || (y+level*2)%11<3)?20:225;}
    static std::uint8_t alpha(int x,int y) {return (x+3*y)%31==0?0:(x+2*y)%37==0?128:255;}
    const ImageMetadata& metadata() const override {return info;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        validate_region(info,r);PixelBuffer out(r.width,r.height);const auto scale=info.levels[r.level].downsample;
        const auto ox=int(std::llround(r.x/scale)),oy=int(std::llround(r.y/scale));
        for(int y=0;y<r.height;++y) for(int x=0;x<r.width;++x) {
            const auto at=(y*r.width+x)*4;for(int c=0;c<3;++c) out.writable_bytes()[at+c]=gray(ox+x,oy+y,r.level);
            out.writable_bytes()[at+3]=alpha(ox+x,oy+y);
        }
        return out;
    }
    void close() override {}
    bool closed() const override {return false;}
};
// Independent direct 2D weighted convolution, rather than the separable kernel.
std::vector<std::uint8_t> expected(const TissueRequest& request) {
    const auto b=request.level_bounds();const int w=int(b.width),h=int(b.height);const auto& p=request.parameters();
    std::vector<double> value(w*h);std::vector<std::uint8_t> domain(w*h),mask(w*h);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const auto gx=int(b.x)+x,gy=int(b.y)+y;const Point center{(gx+.5)*request.downsample(),(gy+.5)*request.downsample()};
        if(Pattern::alpha(gx,gy)!=255 || !request.roi().contains_sample(center,p.sampling_policy)) continue;
        domain[y*w+x]=1;const auto od=float(-std::log10(double(Pattern::gray(gx,gy,request.level()))/255.));
        value[y*w+x]=p.feature==TissueFeature::OpticalDensitySum?3*double(od):double(od);
    }
    const auto radius=int(std::ceil(3*p.smoothing_sigma));std::vector<double> kernel(2*radius+1);
    for(int d=-radius;d<=radius;++d) kernel[d+radius]=p.smoothing_sigma?std::exp(-.5*std::pow(d/p.smoothing_sigma,2)):1.;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        if(!domain[y*w+x]) continue;double sum=0,weight=0;
        for(int yy=std::max(0,y-radius);yy<=std::min(h-1,y+radius);++yy)
            for(int xx=std::max(0,x-radius);xx<=std::min(w-1,x+radius);++xx) {
                if(!domain[yy*w+xx]) continue;const auto coefficient=kernel[yy-y+radius]*kernel[xx-x+radius];
                sum+=value[yy*w+xx]*coefficient;weight+=coefficient;
            }
        mask[y*w+x]=sum/weight>=p.threshold;
    }
    return mask;
}
void resign(std::vector<std::uint8_t>& bytes,const Roi& roi) {
    Sha256 hash;const auto geometry=roi.wkt();hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());
    hash.update(bytes.data(),bytes.size()-64);const auto digest=hash.finish();std::copy(digest.begin(),digest.end(),bytes.end()-64);
}
int main() {
 try {
    auto server=std::make_shared<Pattern>();Document doc(server);StainTransform transform({{1,0,0},{0,1,0}});
    std::vector<Roi> rois{Roi::rectangle({1.2,2.1,48,30}),Roi::ellipse({1.2,2.1,48,30})};
    if(has_geos()) rois.push_back(Roi::polygon({{1.2,2.1},{49.2,2.1},{49.2,32.1},{1.2,32.1}},{{{15,8},{21,8},{21,15},{15,15}}}));
    std::size_t changed=0;
    for(const auto& roi:rois) {
        const auto id=doc.add_roi(roi);
        for(int level:{0,1}) for(auto feature:{TissueFeature::OpticalDensitySum,TissueFeature::Stain1}) for(double sigma:{.6,1.7,2.5}) {
            TissueParameters p;p.level=level;p.feature=feature;p.smoothing_sigma=sigma;p.threshold=feature==TissueFeature::Stain1?.22:.66;
            TissueRequest grid(doc,id,transform,p);const auto oracle=expected(grid);std::vector<std::uint32_t> labels;
            for(std::size_t tile:{16,31,256}) {
                p.tile_size=tile;TissueRequest request(doc,id,transform,p);const auto result=detect_tissue(request);
                check(result->algorithm()=="qpx.threshold-components/4" && result->labels().size()==oracle.size());
                for(std::size_t i=0;i<oracle.size();++i) check((result->labels()[i]!=0)==bool(oracle[i]));
                if(labels.empty()) labels=result->labels();else check(result->labels()==labels);
                const auto encoded=TissueArchiveCodec::encode(*result);check(encoded[3]==3);
                const auto decoded=TissueArchiveCodec::decode(encoded.data(),encoded.size(),roi,doc.metadata());
                check(decoded->parameters().smoothing_sigma==sigma && decoded->labels()==result->labels());
                check(TissueArchiveCodec::encode(*decoded)==encoded);
            }
            p.smoothing_sigma=0;auto raw=detect_tissue(TissueRequest(doc,id,transform,p));
            for(std::size_t i=0;i<oracle.size();++i) changed+=(raw->labels()[i]!=0)!=bool(oracle[i]);
        }
    }
    check(changed>0);
    const auto id=doc.add_rectangle({0,0,54,36});TissueParameters p;p.level=0;p.smoothing_sigma=1.7;
    TissueRequest request(doc,id,transform,p);auto result=detect_tissue(request);const auto record=doc.keep_tissue(request,result);
    check(detect_tissue(TissueRequest::replay(doc,record))->labels()==result->labels());
    p.work_bytes=request.estimated_work_bytes()-1;rejects([&]{TissueRequest too_small(doc,id,transform,p);});p.work_bytes=64*1024*1024;
    const auto good=TissueArchiveCodec::encode(*result);
    for(int kind:{0,1,2}) {
        auto bad=good;
        if(kind==0) std::fill(bad.begin()+12,bad.begin()+20,0); // QTM3 must actually smooth.
        if(kind==1) bad[4]=2; // Unsupported sampling policy.
        if(kind==2) {const double sigma=17;std::uint64_t bits;std::memcpy(&bits,&sigma,8);for(int i=0;i<8;++i) bad[12+i]=std::uint8_t(bits>>(i*8));}
        resign(bad,result->roi());rejects([&]{TissueArchiveCodec::decode(bad.data(),bad.size(),result->roi(),doc.metadata());});
    }
    const auto table=measurements_csv(MeasurementTableRequest(doc,MeasurementTableKind::Tissue));
    check(table.find("qpx.tissue-table/2")!=std::string::npos && table.find("smoothing_sigma_sample_px")!=std::string::npos);
    p.smoothing_sigma=0;const auto unfiltered=detect_tissue(TissueRequest(doc,id,transform,p));
    check(TissueArchiveCodec::encode(*unfiltered)[3]==1);
    for(double invalid:{-1.,17.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {p.smoothing_sigma=invalid;rejects([&]{p.validate();});}
    std::cout<<"Independent masked 2D Gaussian, ROI/alpha/crop/level/tile seams, QTM3 replay/guards and budget checks passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
