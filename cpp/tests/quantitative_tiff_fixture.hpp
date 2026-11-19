#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>
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

// Independently authored 8x8 little-endian BigTIFF with one calibrated OME plane.
inline void ome_bigtiff_fixture(const std::filesystem::path& path) {
    std::string xml=R"(<OME xmlns="http://www.openmicroscopy.org/Schemas/OME/2016-06"><Image ID="Image:0"><Pixels ID="Pixels:0" SizeX="8" SizeY="8" SizeZ="1" SizeC="1" SizeT="1" DimensionOrder="XYZCT" Type="uint16" PhysicalSizeX="0.5" PhysicalSizeY="2"><Channel ID="Channel:0:0"/><TiffData/></Pixels></Image></OME>)";
    xml.push_back('\0');
    constexpr std::uint64_t tag_count=12,xml_offset=16+8+20*tag_count+8;
    const auto pixel_offset=(xml_offset+xml.size()+7)&~std::uint64_t(7);
    std::vector<std::uint8_t> bytes(pixel_offset+128);
    const auto number=[&](std::size_t at,std::uint64_t value,unsigned width) {
        for(unsigned i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(8*i));
    };
    bytes[0]=bytes[1]='I';number(2,43,2);number(4,8,2);number(8,16,8);number(16,tag_count,8);
    const std::uint64_t tags[tag_count][4]={{256,4,1,8},{257,4,1,8},{258,3,1,16},{259,3,1,1},
        {262,3,1,1},{270,2,xml.size(),xml_offset},{273,16,1,pixel_offset},{277,3,1,1},
        {278,4,1,8},{279,16,1,128},{284,3,1,1},{339,3,1,1}};
    for(std::size_t i=0;i<tag_count;++i){
        const auto at=24+20*i;number(at,tags[i][0],2);number(at+2,tags[i][1],2);
        number(at+4,tags[i][2],8);number(at+12,tags[i][3],8);
    }
    for(std::size_t i=0;i<xml.size();++i)bytes[xml_offset+i]=std::uint8_t(xml[i]);
    for(std::size_t i=0;i<64;++i)number(pixel_offset+2*i,1000*i,2);
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    out.close();if(!out)throw std::runtime_error("cannot write OME BigTIFF fixture");
}
}
