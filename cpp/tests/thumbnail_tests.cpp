#include "qupathxx/thumbnail.hpp"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("thumbnail check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::runtime_error("missing thumbnail rejection");}
struct Reader:ImageServer {
    ImageMetadata m;std::vector<std::uint8_t> data;
    mutable unsigned reads=0;mutable RegionRequest last{0,0,0,1,1};bool shut=false,broken=false;
    mutable std::mutex mutex;mutable std::condition_variable cv;mutable bool entered=false,released=true;
    Reader(int w,int h):data(static_cast<std::size_t>(w)*h*4) {m.width=w;m.height=h;m.levels.push_back({w,h,1});}
    const ImageMetadata& metadata() const override {return m;}
    PixelBuffer read_region(const RegionRequest& r) const override {
        std::unique_lock<std::mutex> lock(mutex);++reads;last=r;entered=true;cv.notify_all();cv.wait(lock,[&]{return released;});
        if(shut) throw std::runtime_error("reader closed");
        PixelBuffer p(r.width,r.height);
        if(r.level==0) p.writable_bytes()=data;
        else std::fill(p.writable_bytes().begin(),p.writable_bytes().end(),255);
        if(broken) p.writable_bytes().clear();return p;
    }
    void close() override {std::lock_guard<std::mutex> lock(mutex);shut=true;}
    bool closed() const override {return shut;}
};
// Independent floating-area oracle, visiting every source pixel rather than
// using the production integer interval traversal. Inputs are small/exact.
void compare_oracle(const Reader& reader,const ThumbnailResult& result) {
    const auto sw=reader.m.width,sh=reader.m.height,w=result.plan.width,h=result.plan.height;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        double a=0,rgb[3]={};const double area=double(sw)*sh/w/h;
        for(int sy=0;sy<sh;++sy) for(int sx=0;sx<sw;++sx) {
            auto dx=std::max(0.,std::min(double(x+1)*sw/w,double(sx+1))-std::max(double(x)*sw/w,double(sx)));
            auto dy=std::max(0.,std::min(double(y+1)*sh/h,double(sy+1))-std::max(double(y)*sh/h,double(sy)));
            const auto i=(sy*sw+sx)*4;auto weight=dx*dy*reader.data[i+3];a+=weight;
            for(int c=0;c<3;++c) rgb[c]+=weight*reader.data[i+c];
        }
        const auto i=(y*w+x)*4;const auto alpha=static_cast<int>(std::floor(a/area+.5+1e-9));
        check(result.pixels.bytes()[i+3]==alpha);
        for(int c=0;c<3;++c) check(result.pixels.bytes()[i+c]==(alpha?static_cast<int>(std::floor(rgb[c]/a+.5+1e-9)):0));
    }
}
int main() {try {
    std::mt19937 rng(42);
    for(int h=1;h<=9;++h) for(int w=1;w<=11;++w) {
        Reader r(w,h);for(auto& c:r.data)c=static_cast<std::uint8_t>(rng());
        for(int bound=1;bound<=12;++bound) {
            ThumbnailOptions o;o.max_width=bound;o.max_height=std::max(1,bound-2);
            auto result=read_thumbnail(r,o);check(result->plan.width<=w && result->plan.height<=h);
            check(result->plan.width<=o.max_width && result->plan.height<=o.max_height);compare_oracle(r,*result);
        }
    }
    Reader alpha(2,1);alpha.data={255,0,0,255,0,0,255,0};ThumbnailOptions one;one.max_width=one.max_height=1;
    check(read_thumbnail(alpha,one)->pixels.bytes()==std::vector<std::uint8_t>({255,0,0,128}));
    alpha.data={255,0,0,0,0,255,0,0};check(read_thumbnail(alpha,one)->pixels.bytes()==std::vector<std::uint8_t>({0,0,0,0}));
    Reader pyramid(1001,701);pyramid.m.levels={{1001,701,1},{334,234,3},{143,101,7}};
    ThumbnailOptions o;o.max_width=250;o.max_height=150;
    auto p=plan_thumbnail(pyramid.m,o);check(p.level==1 && p.width==214 && p.height==150 && p.source_pixels==334*234);
    read_thumbnail(pyramid,o);check(pyramid.reads==1 && pyramid.last.x==0 && pyramid.last.y==0 && pyramid.last.level==1 && pyramid.last.width==334 && pyramid.last.height==234);
    o.source_pixel_limit=100;rejects<std::length_error>([&]{read_thumbnail(pyramid,o);});check(pyramid.reads==1);
    o={};o.output_pixel_limit=1;rejects<std::length_error>([&]{read_thumbnail(pyramid,o);});check(pyramid.reads==1);
    o={};o.max_width=0;rejects<std::invalid_argument>([&]{plan_thumbnail(pyramid.m,o);});
    auto huge=pyramid.m;huge.width=std::numeric_limits<std::int64_t>::max()-1;huge.height=huge.width/2;
    huge.levels={{huge.width,huge.height,1},{512,256,1}};o={};o.max_width=o.max_height=255;
    p=plan_thumbnail(huge,o);check(p.width==255 && p.height==127 && p.level==1);
    Reader malformed(2,1);malformed.broken=true;rejects<std::runtime_error>([&]{read_thumbnail(malformed);});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{read_thumbnail(pyramid,{},&cancelled);});check(pyramid.reads==1);
    auto server=std::make_shared<Reader>(2,1);server->data={1,2,3,255,4,5,6,255};server->released=false;
    ThumbnailJob job;check(job.request(server));
    {std::unique_lock<std::mutex> lock(server->mutex);server->cv.wait(lock,[&]{return server->entered;});}
    check(!job.request(server));job.cancel();check(job.poll().state=="cancelling");
    {std::lock_guard<std::mutex> lock(server->mutex);server->released=true;server->cv.notify_all();}
    check(job.wait().state=="cancelled" && !job.result() && job.poll().completed==0);
    check(job.request(server));check(job.wait().state=="succeeded" && job.poll().completed==1);
    auto retained=job.result();check(retained->pixels.bytes()==server->data);
    one.max_width=0;rejects<std::invalid_argument>([&]{job.request(server,one);});check(job.result()==retained);
    server->close();check(job.request(server));check(job.wait().state=="failed" && !job.result());
    server.reset();check(retained->pixels.bytes()[7]==255);
    std::cout<<"Thumbnail planning, area/alpha oracle, budgets, cancellation and ownership passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
