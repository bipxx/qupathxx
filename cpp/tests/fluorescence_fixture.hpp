#pragma once
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace qpx_test {
// Independent, uncompressed scalar IFDs: three channels, two Z planes, four
// pixels per plane. Encoding never calls the reader or its TIFF dependency.
inline void fluorescence_fixture(const std::filesystem::path& path,bool floating) {
    const std::string xml="<OME xmlns=\"http://www.openmicroscopy.org/Schemas/OME/2016-06\"><Image ID=\"Image:0\"><Pixels ID=\"Pixels:0\" DimensionOrder=\"XYCZT\" Type=\""+
        std::string(floating?"float":"uint16")+"\" SizeX=\"2\" SizeY=\"2\" SizeC=\"3\" SizeZ=\"2\" SizeT=\"1\" PhysicalSizeX=\"0.5\" PhysicalSizeXUnit=\"um\" PhysicalSizeY=\"0.75\" PhysicalSizeYUnit=\"um\"><Channel ID=\"Channel:0\" Name=\"High\" SamplesPerPixel=\"1\"/><Channel ID=\"Channel:1\" Name=\"Zero\" SamplesPerPixel=\"1\"/><Channel ID=\"Channel:2\" Name=\"Marker\" SamplesPerPixel=\"1\"/><TiffData IFD=\"0\" PlaneCount=\"6\"/></Pixels></Image></OME>";
    const std::uint32_t description=8+6*150,sample_bytes=floating?4:2;
    const auto pixels=description+static_cast<std::uint32_t>(xml.size()+1)+(xml.size()%2==0?1:0);
    std::vector<std::uint8_t> bytes(pixels+6*4*sample_bytes,0);
    auto u16=[&](std::size_t at,std::uint16_t value){for(unsigned j=0;j<2;++j)bytes[at+j]=std::uint8_t(value>>(8*j));};
    auto u32=[&](std::size_t at,std::uint32_t value){for(unsigned j=0;j<4;++j)bytes[at+j]=std::uint8_t(value>>(8*j));};
    bytes[0]=bytes[1]='I';u16(2,42);u32(4,8);
    std::memcpy(bytes.data()+description,xml.c_str(),xml.size()+1);
    for(unsigned plane=0;plane<6;++plane) {
        const auto ifd=8+150*plane;unsigned tags=0;
        auto tag=[&](unsigned id,unsigned type,unsigned count,unsigned value){const auto at=ifd+2+12*tags++;u16(at,std::uint16_t(id));u16(at+2,std::uint16_t(type));u32(at+4,count);u32(at+8,value);};
        tag(256,4,1,2);tag(257,4,1,2);tag(258,3,1,8*sample_bytes);tag(259,3,1,1);tag(262,3,1,1);
        if(!plane)tag(270,2,static_cast<unsigned>(xml.size()+1),description);
        tag(273,4,1,pixels+plane*4*sample_bytes);tag(277,3,1,1);tag(278,4,1,2);tag(279,4,1,4*sample_bytes);tag(284,3,1,1);tag(339,3,1,floating?3:1);
        u16(ifd,std::uint16_t(tags));u32(ifd+2+12*tags,plane==5?0:ifd+150);
        for(unsigned i=0;i<4;++i) {
            const auto c=plane%3,z=plane/3,at=pixels+(plane*4+i)*sample_bytes;
            if(floating) {
                const float marker[]={1.25f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-2.75f};
                const float value=c==0?float(1000+100*z+10000*i):c==1?0:marker[i];
                std::uint32_t bits;std::memcpy(&bits,&value,4);u32(at,bits);
            } else u16(at,std::uint16_t(c==0?1000+100*z+10000*i:c==1?0:400+10*z+i));
        }
    }
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();
    if(!file)throw std::runtime_error("Could not write native fluorescence fixture");
}
}
