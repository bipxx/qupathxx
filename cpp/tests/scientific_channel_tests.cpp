#include "qupathxx/scientific_channels.hpp"
#include "scientific_channel_reference.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool value) {if(!value) throw std::runtime_error("scientific float64 channel contract failed");}
template<class Exception,class Function> void rejects(Function function) {
    try {function();} catch(const Exception&) {return;}
    throw std::runtime_error("scientific channel admission did not reject");
}
double number(std::uint64_t bits) {double value;std::memcpy(&value,&bits,sizeof(value));return value;}
std::uint32_t float_bits(float value) {std::uint32_t bits;std::memcpy(&bits,&value,sizeof(value));return bits;}
bool close(double a,double b) {return std::abs(a-b)<=2e-12+2e-12*std::abs(b);}
class Reader final:public qpx::ImageServer {
    qpx::ImageMetadata metadata_;
    const channel_reference::Case& test_;
public:
    explicit Reader(const channel_reference::Case& test):test_(test) {metadata_.width=test.iw;metadata_.height=test.ih;metadata_.levels={{test.iw,test.ih,1}};}
    const qpx::ImageMetadata& metadata() const override {return metadata_;}
    void close() override {}bool closed() const override {return false;}
    qpx::PixelBuffer read_region(const qpx::RegionRequest& request) const override {
        qpx::PixelBuffer result(request.width,request.height);auto& bytes=result.writable_bytes();
        for(std::int64_t row=0;row<request.height;++row) for(std::int64_t col=0;col<request.width;++col) {
            const auto x=request.x+col,y=request.y+row,i=(row*request.width+col)*4;
            bytes[i]=std::uint8_t((17*x+3*y)%256);bytes[i+1]=std::uint8_t((5*x+19*y)%256);bytes[i+2]=std::uint8_t((11*x+7*y)%256);bytes[i+3]=255;
            for(const auto& value:test_.overrides) if(value.x==x && value.y==y) for(unsigned c=0;c<4;++c) bytes[i+c]=value.rgba[c];
        }
        return result;
    }
};
// Frozen pre-refactor arithmetic from 4bdfa98: distinct test-side cofactor
// expressions and LUT construction preserve byte/float32 operation order.
void legacy_control(const qpx::StainTransform& transform) {
    using namespace qpx;
    auto cross=[](const ColorVector& a,const ColorVector& b)->ColorVector {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    const auto& basis=transform.stains();const std::array<ColorVector,3> rows={cross(basis[1],basis[2]),cross(basis[2],basis[0]),cross(basis[0],basis[1])};
    const auto det=basis[0][0]*rows[0][0]+basis[0][1]*rows[0][1]+basis[0][2]*rows[0][2];
    std::array<std::array<double,256>,3> od;
    std::array<std::array<ColorVector,256>,3> contribution;
    for(unsigned c=0;c<3;++c) for(unsigned v=0;v<256;++v) {
        od[c][v]=std::max(0.,-std::log10(std::max(1.,double(v))/transform.white()[c]));
        for(unsigned s=0;s<3;++s) contribution[c][v][s]=od[c][v]*rows[s][c]/det;
    }
    PixelBuffer pixels(256,256);auto& bytes=pixels.writable_bytes();
    for(unsigned i=0;i<65536;++i) {bytes[i*4]=std::uint8_t(i);bytes[i*4+1]=std::uint8_t(i>>8);bytes[i*4+2]=std::uint8_t((17*(i&255)+37*(i>>8))%256);bytes[i*4+3]=i%97?255:0;}
    const auto actual_od=transform.optical_densities(pixels),actual_stains=transform.deconvolve(pixels);
    for(unsigned i=0;i<65536;++i) for(unsigned s=0;s<3;++s) {
        const auto r=bytes[i*4],g=bytes[i*4+1],b=bytes[i*4+2];
        const auto expected_od=bytes[i*4+3]?float(od[s][bytes[i*4+s]]):std::numeric_limits<float>::quiet_NaN();
        const auto expected_stain=bytes[i*4+3]?float((contribution[0][r][s]+contribution[1][g][s])+contribution[2][b][s]):std::numeric_limits<float>::quiet_NaN();
        require(float_bits(actual_od.values()[i*3+s])==float_bits(expected_od));
        require(float_bits(actual_stains.values()[i*3+s])==float_bits(expected_stain));
    }
}
}
void check_scientific_channels() {
    using namespace qpx;
    std::size_t index=0;
    for(const auto& test:channel_reference::cases) {
        Reader reader(test);const ScientificGrid grid(test.iw,test.ih,{test.x,test.w,test.nx,test.cx},{test.y,test.h,test.ny,test.cy});
        auto input=read_scientific_rgb(reader,grid);const StainTransform transform(test.raw,test.white);
        const auto charge=scientific_channel_work_bytes(input);
        auto od=scientific_optical_densities(input,transform,charge);auto stains=scientific_deconvolve(input,transform,charge);
        require(od.mode()==ScientificChannelMode::OpticalDensity && stains.mode()==ScientificChannelMode::Stains);
        require(od.valid()==test.valid && stains.valid()==test.valid);
        require(od.white()==test.white && stains.stains()==transform.stains());
        require(od.width()==grid.width() && od.height()==grid.height() && od.grid().x().origin()==test.x);
        require(od.estimated_work_bytes()==charge && charge==50*input.valid().size()+sizeof(StainTransform)+4096);
        for(std::size_t i=0;i<test.od.size();++i) {
            if(!close(od.values()[i],number(test.od[i])) || !close(stains.values()[i],number(test.stains[i]))) {
                std::cerr<<"scientific channel mismatch in case "<<index<<" value "<<i<<'\n';require(false);
            }
        }
        rejects<std::length_error>([&]{scientific_deconvolve(input,transform,charge-1);});
        rejects<std::invalid_argument>([&]{scientific_optical_densities(input,transform,0);});
        rejects<std::invalid_argument>([&]{scientific_optical_densities(input,transform,(1ULL<<30)+1);});
        std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{scientific_deconvolve(input,transform,charge,&cancelled);});
        if(index<4) legacy_control(transform);
        ++index;
    }
    std::cout<<"Native scientific float64 OD/stains, validity, captured basis and admission passed\n";
    std::cout<<"Legacy byte/float32 stain transforms preserve frozen arithmetic across 262144 RGB probes\n";
}
