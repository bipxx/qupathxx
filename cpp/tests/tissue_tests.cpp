#include "qupathxx/tissue.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
void check(bool v) {if(!v) throw std::runtime_error("tissue check failed");}
class Gate final:public qpx::ImageServer {
    qpx::ImageMetadata metadata_;
public:
    mutable std::atomic<bool> entered{false},release{false};
    std::atomic<bool> shut{false};bool transparent=false;
    Gate() {metadata_.width=40;metadata_.height=30;metadata_.levels={{40,30,1}};}
    const qpx::ImageMetadata& metadata() const override {return metadata_;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest& r) const override {
        entered.store(true);while(!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        qpx::PixelBuffer buffer(r.width,r.height);if(!transparent) for(std::size_t i=3;i<buffer.bytes().size();i+=4) buffer.writable_bytes()[i]=255;return buffer;
    }
    void close() override {shut.store(true);}
    bool closed() const override {return shut.load();}
};
class Pyramid final:public qpx::ImageServer {
public:
    qpx::ImageMetadata info;
    mutable unsigned reads=0;
    Pyramid() {info.width=300;info.height=240;info.levels={{300,240,1},{200,160,1.5},{75,60,4}};}
    const qpx::ImageMetadata& metadata() const override {return info;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest& r) const override {
        qpx::validate_region(info,r);++reads;qpx::PixelBuffer output(r.width,r.height);
        const auto scale=info.levels[r.level].downsample;
        const auto gx=static_cast<std::int64_t>(std::llround(r.x/scale)),gy=static_cast<std::int64_t>(std::llround(r.y/scale));
        for(std::int64_t row=0;row<r.height;++row) for(std::int64_t col=0;col<r.width;++col) {
            const auto i=static_cast<std::size_t>((row*r.width+col)*4);
            const bool dark=(gx+col)%19<6 || (gy+row)%17<5;
            for(int c=0;c<3;++c) output.writable_bytes()[i+c]=dark?0:255;
            output.writable_bytes()[i+3]=255;
        }
        return output;
    }
    void close() override {}
    bool closed() const override {return false;}
};
int main() {
    try {
        std::vector<std::uint8_t> rgb;
        for(int y=0;y<3;++y) for(int x=0;x<3;++x) for(int c=0;c<3;++c) rgb.push_back((x+y)%2?255:0);
        qpx::Document doc(qpx::Image(3,3,rgb));const auto id=doc.add_rectangle({0,0,3,3});
        qpx::StainTransform transform({{1,0,0},{0,1,0}});qpx::TissueParameters p;p.tile_size=16;p.connectivity=4;
        const auto four=qpx::detect_tissue(qpx::TissueRequest(doc,id,transform,p));
        check(four->samples()==9 && four->components().size()==5 && four->foreground_pixels()==5);
        p.connectivity=8;const auto eight=qpx::detect_tissue(qpx::TissueRequest(doc,id,transform,p));
        check(eight->components().size()==1 && eight->components()[0].pixels==5 && eight->components()[0].centroid.x==1.5);
        p.min_pixels=6;check(qpx::detect_tissue(qpx::TissueRequest(doc,id,transform,p))->foreground_pixels()==0);
        p.min_pixels=1;p.work_bytes=1;
        try {qpx::TissueRequest invalid(doc,id,transform,p);throw std::runtime_error("accepted budget");} catch(const std::length_error&) {}
        auto server=std::make_shared<Gate>();qpx::Document blocked(server);const auto object=blocked.add_rectangle({0,0,40,30});
        qpx::TissueJob job;
        struct Release {std::shared_ptr<Gate> server;~Release(){server->release.store(true);}} guard{server};
        check(job.request(qpx::TissueRequest(blocked,object,transform)));
        check(!job.request(qpx::TissueRequest(blocked,object,transform)));
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!server->entered.load() && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
        check(server->entered.load());const auto start=std::chrono::steady_clock::now();
        check(job.poll(blocked,object).state=="running");check(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100));
        blocked.rename_annotation(object,"Changed");check(job.poll(blocked,object).state=="cancelling");server->release.store(true);
        check(job.wait(blocked,object).state=="stale" && !job.result());
        check(job.request(qpx::TissueRequest(blocked,object,transform)));job.cancel();check(job.wait(blocked,object).state=="cancelled");
        check(job.request(qpx::TissueRequest(blocked,object,transform)));check(job.wait(blocked,object).state=="ready" && job.result()->foreground_pixels()==1200);
        server->close();check(job.poll(blocked,object).state=="stale" && !job.result());
        auto empty=std::make_shared<Gate>();empty->release.store(true);empty->transparent=true;qpx::Document no_pixels(empty);
        const auto no_id=no_pixels.add_rectangle({0,0,40,30});check(job.request(qpx::TissueRequest(no_pixels,no_id,transform)));
        check(job.wait(no_pixels,no_id).state=="failed" && !job.result());
        auto pyramid=std::make_shared<Pyramid>();qpx::Document multi(pyramid);
        const auto region=multi.add_rectangle({2.2,3.1,190,150});p=qpx::TissueParameters{};p.level=1;
        std::vector<std::uint32_t> baseline;
        for(auto tile:{16u,31u,256u}) {
            p.tile_size=tile;qpx::TissueRequest request(multi,region,transform,p);const auto result=qpx::detect_tissue(request);
            check(request.level()==1 && result->level()==1 && result->downsample()==1.5);
            check(result->width()==127 && result->height()==100 && result->bounds().x==1.5 && result->bounds().y==3);
            for(std::int64_t y=0;y<100;++y) for(std::int64_t x=0;x<127;++x)
                check((result->labels()[static_cast<std::size_t>(y*127+x)]!=0)==((x+1)%19<6 || (y+2)%17<5));
            if(baseline.empty()) baseline=result->labels();else check(baseline==result->labels());
            check(result->components().size()==1 && result->components()[0].bounds.width==190.5);
            check(result->algorithm()=="qpx.threshold-components/2");
        }
        const auto tiny=multi.add_rectangle({.7,.7,.1,.1});
        check(qpx::detect_tissue(qpx::TissueRequest(multi,tiny,transform,p))->samples()==1);
        p.level=0;try {qpx::TissueRequest invalid(multi,tiny,transform,p);throw std::runtime_error("accepted empty grid");} catch(const std::invalid_argument&) {}
        p.level=9;try {qpx::TissueRequest invalid(multi,region,transform,p);throw std::runtime_error("accepted missing level");} catch(const std::out_of_range&) {}
        auto large=std::make_shared<Pyramid>();large->info.width=100352;large->info.height=2048;
        large->info.levels={{100352,2048,1},{50176,1024,2},{25088,512,4},{12544,256,8}};
        qpx::Document whole(large);const auto all=whole.add_rectangle({0,0,100352,2048});p=qpx::TissueParameters{};
        qpx::TissueRequest automatic(whole,all,transform,p);check(automatic.level()==3 && automatic.estimated_work_bytes()<=p.work_bytes && large->reads==0);
        p.level=0;try {qpx::TissueRequest invalid(whole,all,transform,p);throw std::runtime_error("accepted large level");} catch(const std::length_error&) {}
        check(large->reads==0);
        std::cout<<"tissue passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
