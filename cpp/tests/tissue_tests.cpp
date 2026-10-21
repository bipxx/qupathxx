#include "qupathxx/tissue.hpp"
#include <algorithm>
#include <chrono>
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
        std::cout<<"tissue passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
