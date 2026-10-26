#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>
namespace qpx_test {
// Independently authored little-endian two-channel uint16 TIFF.
inline void raw_pixel_tiff_fixture(const std::filesystem::path& path,std::uint32_t width=8,std::uint32_t height=6) {
    std::vector<std::uint8_t> b(160+std::size_t(width)*height*4);
    auto u16=[&](std::size_t at,std::uint16_t v) {b[at]=v&255;b[at+1]=v>>8;};
    auto u32=[&](std::size_t at,std::uint32_t v) {for(int i=0;i<4;++i) b[at+i]=std::uint8_t(v>>(8*i));};
    b[0]=b[1]='I';u16(2,42);u32(4,8);u16(8,12);
    const std::uint32_t tags[][4]={{256,4,1,width},{257,4,1,height},{258,3,2,0x00100010},{259,3,1,1},{262,3,1,1},{273,4,1,160},{277,3,1,2},{278,4,1,height},{279,4,1,width*height*4},{284,3,1,1},{338,3,1,0},{339,3,2,0x00010001}};
    for(std::size_t i=0;i<12;++i) {auto at=10+12*i;u16(at,std::uint16_t(tags[i][0]));u16(at+2,std::uint16_t(tags[i][1]));u32(at+4,tags[i][2]);u32(at+8,tags[i][3]);}
    for(std::size_t i=0;i<std::size_t(width)*height;++i) {u16(160+4*i,std::uint16_t(i%60000));u16(162+4*i,std::uint16_t((i+100)%60000));}
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());out.close();if(!out) throw std::runtime_error("cannot write quantitative TIFF fixture");
}
}
