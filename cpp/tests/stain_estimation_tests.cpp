#include "qupathxx/stain_estimation.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace qpx;
void check(bool b) {if(!b) throw std::runtime_error("stain estimation check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing stain estimation rejection");}
double dot(const ColorVector& a,const ColorVector& b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
struct BrokenReader:ImageServer {
    ImageMetadata m;
    BrokenReader() {m.width=m.height=32;m.levels.push_back({32,32,1});}
    const ImageMetadata& metadata() const override {return m;}
    PixelBuffer read_region(const RegionRequest& r) const override {PixelBuffer p(r.width,r.height);p.writable_bytes().clear();return p;}
    void close() override {}bool closed() const override {return false;}
};
int main() {
 try {
    const ColorVector white={255,250,245};StainTransform truth({{.65,.70,.29},{.07,.99,.11}},white);std::vector<std::uint8_t> rgb;
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
        const auto a=x%8==0?0.:.1+.6*x/63,b=x%8==1?0.:.1+.7*y/63;
        for(int c=0;c<3;++c) rgb.push_back(static_cast<std::uint8_t>(std::round(white[c]*std::pow(10.,-a*truth.stains()[0][c]-b*truth.stains()[1][c]))));
    }
    Document doc(Image(64,64,rgb));const auto object=doc.add_rectangle({0,0,64,64});StainEstimateParameters p;p.tile_size=16;
    StainEstimateRequest request(doc,object,p,white);const auto result=estimate_stains(request);check(result.tiles==16 && result.samples==4096 && result.sampled==result.eligible && result.level==0);
    check(dot(result.stains[0],truth.stains()[0])>.9999 && dot(result.stains[1],truth.stains()[1])>.9999 && result.angle_degrees>20 && result.residual_fraction<.001);
    check(stain_model_sha256(result.transform())==result.model_sha256 && result.input_sha256==analyze(AnalysisRequest(doc,object,truth,AnalysisKind::OpticalDensityMeans,false,16)).input_sha256);
    const auto original=stain_estimate_json(result);check(original.find("qpx.stain-estimate/1")!=std::string::npos);
    p.sample_limit=64;p.seed=42;const auto sampled=estimate_stains(StainEstimateRequest(doc,object,p,white));check(sampled.sampled==64 && sampled.eligible==result.eligible && sampled.model_sha256==estimate_stains(StainEstimateRequest(doc,object,p,white)).model_sha256);
    p.work_bytes=1;rejects<std::length_error>([&]{StainEstimateRequest bad(doc,object,p);});p={};p.trim_quantile=.5;rejects<std::invalid_argument>([&]{p.validate();});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{estimate_stains(request,&cancelled);});
    StainEstimateJob job;check(job.request(request));check(job.wait(doc,object).state=="ready" && job.result()->model_sha256==result.model_sha256);
    doc.rename_annotation(object,"changed");check(job.poll(doc,object).state=="stale" && !job.result());
    Document gray(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));const auto g=gray.add_rectangle({0,0,32,32});rejects<std::invalid_argument>([&]{estimate_stains(StainEstimateRequest(gray,g));});
    Document broken(std::make_shared<BrokenReader>());const auto bad=broken.add_rectangle({0,0,32,32});rejects<std::runtime_error>([&]{estimate_stains(StainEstimateRequest(broken,bad));});
    const auto path=std::filesystem::current_path()/("stain-model-"+std::to_string(std::random_device{}())+".json");save_stain_estimate(result,path.u8string());std::ifstream input(path,std::ios::binary);const std::string saved{std::istreambuf_iterator<char>(input),{}};check(saved==original);input.close();std::filesystem::remove(path);
    doc.remove_annotation(object);check(stain_estimate_json(result)==original);
    std::cout<<"Native stain estimation, models, budgets, cancellation and stale jobs passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
