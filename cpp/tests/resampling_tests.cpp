#include "qupathxx/resampling.hpp"
#include "qupathxx/core.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace qpx;
void check_scientific_grid_geometry();
void check_scientific_rgb_sampling();
void raw_scientific_numeric_tests();
void check_scientific_raw_sampling();
void check(bool ok) {if(!ok) throw std::runtime_error("resampling assertion failed");}
template<class E,class F> void rejects(F f) {try {f();}catch(const E&){return;}throw std::runtime_error("missing resampling rejection");}
// Independent all-source floating overlap oracle; production walks intersecting
// intervals with integer weights. Small coordinates are exact binary/rational.
void oracle(const PixelBuffer& input,const PixelBuffer& output) {
    const auto sw=input.width(),sh=input.height(),w=output.width(),h=output.height();
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        double a=0,channels[3]={};const double area=double(sw)*sh/w/h;
        for(int sy=0;sy<sh;++sy) for(int sx=0;sx<sw;++sx) {
            auto dx=std::max(0.,std::min(double(x+1)*sw/w,double(sx+1))-std::max(double(x)*sw/w,double(sx)));
            auto dy=std::max(0.,std::min(double(y+1)*sh/h,double(sy+1))-std::max(double(y)*sh/h,double(sy)));
            const auto i=(sy*sw+sx)*4;auto weight=dx*dy*input.bytes()[i+3];a+=weight;
            for(int c=0;c<3;++c) channels[c]+=weight*input.bytes()[i+c];
        }
        const auto i=(y*w+x)*4;const auto alpha=static_cast<int>(std::floor(a/area+.5+1e-9));
        check(output.bytes()[i+3]==alpha);
        for(int c=0;c<3;++c) check(output.bytes()[i+c]==(alpha?static_cast<int>(std::floor(channels[c]/a+.5+1e-9)):0));
    }
}
struct InvalidReader:ImageServer {
    ImageMetadata m;mutable int reads=0;int mode=0;
    InvalidReader(){m.width=m.height=1;m.levels={{1,1,1}};}
    const ImageMetadata& metadata() const override{return m;}
    PixelBuffer read_region(const RegionRequest&) const override {++reads;PixelBuffer p(mode==1?2:1,1);if(mode==2)p.writable_bytes().clear();return p;}
    void close() override{}bool closed() const override{return false;}
};
int main(){try {
    check_scientific_grid_geometry();
    check_scientific_rgb_sampling();
    raw_scientific_numeric_tests();
    check_scientific_raw_sampling();
    std::mt19937 rng(154);
    for(int sh=1;sh<=5;++sh) for(int sw=1;sw<=7;++sw) {
        PixelBuffer source(sw,sh);for(auto& v:source.writable_bytes())v=static_cast<std::uint8_t>(rng());
        for(int h=1;h<=7;++h) for(int w=1;w<=9;++w) oracle(source,resample_area(source,w,h));
    }
    PixelBuffer alpha(2,1,{255,0,0,255,0,0,255,0});
    check(resample_area(alpha,1,1).bytes()==std::vector<std::uint8_t>({255,0,0,128}));
    check(resample_area(alpha,4,1).bytes()==std::vector<std::uint8_t>({255,0,0,255,255,0,0,255,0,0,0,0,0,0,0,0}));
    InvalidReader reader;
    rejects<std::invalid_argument>([&]{read_resampled_region(reader,{0,0,0,1,1},0,1);});
    rejects<std::length_error>([&]{read_resampled_region(reader,{0,0,0,67108864,1},1,67108864);});
    rejects<std::out_of_range>([&]{read_resampled_region(reader,{0,0,9,1,1},1,1);});
    std::atomic<bool> cancel{true};rejects<std::runtime_error>([&]{read_resampled_region(reader,{0,0,0,1,1},1,1,&cancel);});check(reader.reads==0);
    reader.mode=1;rejects<std::runtime_error>([&]{read_resampled_region(reader,{0,0,0,1,1},1,1);});
    reader.mode=2;rejects<std::runtime_error>([&]{read_resampled_region(reader,{0,0,0,1,1},1,1);});
    auto server=dense_image_server(Image(2,1,{10,20,30,110,120,130}));
    auto padded=read_resampled_region(*server,{-1,0,0,4,1},2,1);
    check(padded.bytes()==std::vector<std::uint8_t>({10,20,30,128,110,120,130,128}));
    server->close();rejects<std::runtime_error>([&]{read_resampled_region(*server,{0,0,0,2,1},1,1);});
    std::cout<<"2205 independent oracle shapes and admission/alpha/reader checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
