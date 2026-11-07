#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include <stdexcept>
namespace nucls_png_fixture {
inline void u32(std::string& out,std::uint32_t value){for(int n=24;n>=0;n-=8)out+=char(value>>n);}
inline std::uint32_t crc(const std::string& data){std::uint32_t value=0xffffffff;for(unsigned char c:data){value^=c;for(int i=0;i<8;++i)value=(value>>1)^((value&1)?0xedb88320u:0);}return value^0xffffffff;}
inline std::string chunk(const std::string& type,const std::string& data){std::string out;u32(out,static_cast<std::uint32_t>(data.size()));out+=type;out+=data;u32(out,crc(type+data));return out;}
inline std::string stored(const std::string& raw){if(raw.size()>=65536)throw std::runtime_error("Fixture deflate block exceeds65535 bytes");std::string out="\x78\x01";out+=char(1);const auto n=unsigned(raw.size());out+=char(n);out+=char(n>>8);out+=char(~n);out+=char((~n)>>8);out+=raw;unsigned a=1,b=0;for(unsigned char c:raw){a=(a+c)%65521;b=(b+a)%65521;}u32(out,(b<<16)|a);return out;}
inline std::string png(unsigned width,unsigned height,const std::string& raw,unsigned color=2,unsigned depth=8,bool interlace=false,bool split=false) {
    std::string out("\x89PNG\r\n\x1a\n",8),header;u32(header,width);u32(header,height);header+=char(depth);header+=char(color);header+=char(0);header+=char(0);header+=char(interlace);
    out+=chunk("IHDR",header);const auto compressed=stored(raw);
    if(split){out+=chunk("IDAT",compressed.substr(0,5));out+=chunk("IDAT",compressed.substr(5));}else out+=chunk("IDAT",compressed);
    return out+chunk("IEND","");
}
inline std::string filtered(const std::vector<std::uint8_t>& pixels,unsigned width,unsigned height,unsigned filter) {
    std::string out;const auto stride=3*width;
    for(unsigned y=0;y<height;++y){out+=char(filter);for(unsigned x=0;x<stride;++x){
        const int value=pixels[y*stride+x],left=x>=3?pixels[y*stride+x-3]:0,above=y?pixels[(y-1)*stride+x]:0,diagonal=y && x>=3?pixels[(y-1)*stride+x-3]:0;
        int predictor=0;if(filter==1)predictor=left;if(filter==2)predictor=above;if(filter==3)predictor=(left+above)/2;
        if(filter==4){const auto p=left+above-diagonal,pa=std::abs(p-left),pb=std::abs(p-above),pc=std::abs(p-diagonal);predictor=pa<=pb && pa<=pc?left:pb<=pc?above:diagonal;}
        out+=char(value-predictor);
    }}return out;
}
inline std::string adam7(const std::vector<std::uint8_t>& pixels,unsigned width,unsigned height) {
    std::string out;constexpr std::array<std::array<unsigned,4>,7> passes={{{0,0,8,8},{4,0,8,8},{0,4,4,8},{2,0,4,4},{0,2,2,4},{1,0,2,2},{0,1,1,2}}};
    for(const auto& p:passes)if(p[0]<width)for(unsigned y=p[1];y<height;y+=p[3]){out+=char(0);for(unsigned x=p[0];x<width;x+=p[2])for(unsigned c=0;c<3;++c)out+=char(pixels[3*(y*width+x)+c]);}
    return out;
}
}
