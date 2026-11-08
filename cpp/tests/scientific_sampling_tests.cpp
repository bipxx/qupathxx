#include "qupathxx/scientific_sampling.hpp"
#include "scientific_grid_reference.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value) {if(!value) throw std::runtime_error("scientific RGB sampler assertion failed");}
template<class Exception,class Function> void rejects(Function function) {
    try {function();} catch(const Exception&) {return;}
    throw std::runtime_error("scientific RGB sampler did not reject");
}
std::uint64_t bits(double value) {std::uint64_t result;std::memcpy(&result,&value,sizeof(value));return result;}
class Reader final:public qpx::ImageServer {
public:
    qpx::ImageMetadata meta;mutable int reads=0;int mode=0;bool is_closed=false;
    std::size_t case_index=0;std::atomic<bool>* cancelled=nullptr;
    mutable qpx::RegionRequest last{0,0,0,1,1};
    Reader(std::int64_t width,std::int64_t height) {meta.width=width;meta.height=height;meta.levels={{width,height,1}};}
    const qpx::ImageMetadata& metadata() const override {return meta;}
    void close() override {is_closed=true;}
    bool closed() const override {return is_closed;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest& request) const override {
        ++reads;last=request;
        if(mode==3) throw std::runtime_error("synthetic decoder failure");
        qpx::PixelBuffer buffer(request.width+(mode==1),request.height);
        auto& bytes=buffer.writable_bytes();
        for(std::int64_t row=0;row<request.height;++row) for(std::int64_t col=0;col<request.width;++col) {
            const auto x=request.x+col,y=request.y+row;const auto index=std::size_t((row*buffer.width()+col)*4);
            bytes[index]=std::uint8_t((17*x+3*y)%256);bytes[index+1]=std::uint8_t((5*x+19*y)%256);
            bytes[index+2]=std::uint8_t((11*x+7*y)%256);bytes[index+3]=255;
            for(const auto& a:grid_reference::alpha) if(a.case_index==case_index && a.x==x && a.y==y) bytes[index+3]=a.alpha;
        }
        if(mode==2) bytes.clear();
        if(cancelled) cancelled->store(true);
        return buffer;
    }
};
}
void check_scientific_rgb_sampling() {
    using namespace qpx;
    std::size_t case_index=0;
    for(const auto& v:grid_reference::plans) {
        Reader reader(v.iw,v.ih);reader.case_index=case_index;
        const ScientificGrid grid(v.iw,v.ih,{v.x,v.w,v.nx,v.cx},{v.y,v.h,v.ny,v.cy});
        auto result=read_scientific_rgb(reader,grid);
        require(result.width()==v.nx+2*v.cx && result.height()==v.ny+2*v.cy);
        const auto bounds=grid.clipped_read_bounds();
        require(reader.reads==int(bounds.width!=0 && bounds.height!=0));
        if(reader.reads) require(reader.last.level==0 && reader.last.x==bounds.x && reader.last.y==bounds.y &&
            reader.last.width==bounds.width && reader.last.height==bounds.height);
        for(const auto& pixel:grid_reference::pixels) if(pixel.case_index==case_index) {
            require(result.valid()[pixel.index]==std::uint8_t(pixel.valid));
            for(unsigned c=0;c<3;++c) require(bits(result.values()[pixel.index*3+c])==pixel.bits[c]);
        }
        ++case_index;
    }
    Reader reader(8,8);const ScientificGrid grid(8,8,{1,3,2},{1,2,2});
    std::atomic<bool> cancelled{true};
    rejects<std::runtime_error>([&]{read_scientific_rgb(reader,grid,&cancelled);});require(reader.reads==0);
    cancelled.store(false);reader.cancelled=&cancelled;
    rejects<std::runtime_error>([&]{read_scientific_rgb(reader,grid,&cancelled);});require(reader.reads==1);
    reader.cancelled=nullptr;
    for(int mode=1;mode<=3;++mode) {reader.mode=mode;rejects<std::runtime_error>([&]{read_scientific_rgb(reader,grid);});}
    reader.mode=0;const auto previous=reader.reads;
    reader.meta.levels[0].downsample=1.5;
    rejects<std::invalid_argument>([&]{read_scientific_rgb(reader,grid);});require(reader.reads==previous);
    reader.meta.levels[0].downsample=1.;reader.meta.levels[0].width=7;
    rejects<std::invalid_argument>([&]{read_scientific_rgb(reader,grid);});require(reader.reads==previous);
    reader.meta.levels[0].width=8;
    rejects<std::invalid_argument>([&]{read_scientific_rgb(reader,ScientificGrid(7,8,{0,1,1},{0,1,1}));});
    reader.close();rejects<std::runtime_error>([&]{read_scientific_rgb(reader,grid);});
    rejects<std::runtime_error>([&]{read_scientific_rgb(reader,ScientificGrid(8,8,{10,1,1},{10,1,1}));});
    std::cout<<"Native scientific RGB float64 exact means, support, reader and cancellation checks passed\n";
}
