#include "qupathxx/raw_image.hpp"
#include "qupathxx/core.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("raw image check failed");}
template<class F> void rejects(F&& f) {try {f();}catch(const std::exception&) {return;}throw std::runtime_error("missing raw rejection");}
int main() {
    namespace fs=std::filesystem;const auto path=fs::current_path()/("raw-test-"+std::to_string(std::random_device{}())+".tif");
    try {
        RawPixelBuffer owned(2,2,1,SampleType::UInt16);check(owned.payload_bytes()==12 && std::get<std::vector<std::uint16_t>>(owned.storage()).size()==4);
        rejects([] {RawPixelBuffer invalid(0,2,1,SampleType::UInt8);});rejects([] {RawPixelBuffer invalid(100000,100000,3,SampleType::Float64);});
        TiffView view;view.channels={0,0,0};view.lower={0,0,0};view.upper={65535,65535,65535};check(tiff_view_decode(tiff_view_encode(view)).upper==view.upper);
        auto corrupt=tiff_view_encode(view);corrupt.back()^=1;rejects([&]{tiff_view_decode(corrupt);});
        if(has_tiff()) {
            // Independent minimal TIFF byte fixture: no libtiff encoder involved.
            std::vector<unsigned char> bytes={'I','I',42,0,8,0,0,0};
            auto u16=[&](std::uint16_t n){bytes.push_back(n&255);bytes.push_back(n>>8);};auto u32=[&](std::uint32_t n){for(int i=0;i<4;++i) bytes.push_back((n>>(8*i))&255);};
            u16(10);auto tag=[&](std::uint16_t id,std::uint16_t type,std::uint32_t n){u16(id);u16(type);u32(1);u32(n);};
            tag(256,4,2);tag(257,4,2);tag(258,3,16);tag(259,3,1);tag(262,3,1);tag(273,4,134);tag(277,3,1);tag(278,4,2);tag(279,4,8);tag(339,3,1);u32(0);
            for(auto value:{1,256,40000,65535}) u16(static_cast<std::uint16_t>(value));
            std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.close();
            auto server=open_tiff(path.u8string());const auto metadata=server->raw_metadata();check(metadata && metadata->sample_type==SampleType::UInt16 && metadata->channels==1);
            RawRegionRequest request;request.region=RegionRequest(0,0,0,2,2);const auto raw=server->read_raw(request);check(std::get<std::vector<std::uint16_t>>(raw.storage())==std::vector<std::uint16_t>({1,256,40000,65535}));
            const auto display=server->read_region(request.region);check(display.bytes()[0]==0 && display.bytes()[4]==1 && display.bytes()[8]==156 && display.bytes()[12]==255);
            const auto statistics=raw_region_statistics(*server,request);check(statistics.valid_pixels==4 && statistics.finite_samples[0]==4 && statistics.means[0]==26448.);
            request.region=RegionRequest(-1,-1,0,4,4);const auto padded=server->read_raw(request);check(std::count(padded.valid().begin(),padded.valid().end(),1)==4);
            check(server->tiff_read_stats()->decoded_blocks==1 && server->tiff_read_stats()->cache_hits>=3);
            auto alternate=*server->tiff_view();alternate.upper={40000,40000,40000};auto projected=server->with_tiff_view(alternate);
            check(projected->read_region(RegionRequest(0,0,0,2,2)).bytes()[8]==255);
            check(projected->tiff_read_stats()->decoded_blocks==1 && server->tiff_view()->upper[0]==65535);
            server->close();check(server->closed());
            check(std::get<std::vector<std::uint16_t>>(projected->read_raw(request).storage())[10]==65535);
            check(projected->tiff_read_stats()->decoded_blocks==1);projected->close();rejects([&]{server->read_raw(request);});check(std::get<std::vector<std::uint16_t>>(raw.storage())[2]==40000);
            check(path.parent_path()==fs::current_path());fs::remove(path);
        } else rejects([&]{open_tiff(path.u8string());});
        std::cout<<"raw quantitative buffers, TIFF decode/cache, preview and statistics passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
