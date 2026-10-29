#include "../imagej_gaussian.hpp"
#include "../sha256.hpp"
#include "imagej_gaussian_reference.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
void check(bool ok) {if(!ok) throw std::runtime_error("ImageJ Gaussian check failed");}
template<class E,class F> void refuses(F f) {try {f();}catch(const E&){return;}throw std::runtime_error("expected Gaussian rejection");}
std::vector<unsigned char> bytes(const std::vector<float>& values) {
    std::vector<unsigned char> out;out.reserve(values.size()*4);
    for(float value:values) {std::uint32_t bits;std::memcpy(&bits,&value,4);for(int shift=0;shift<32;shift+=8) out.push_back(static_cast<unsigned char>(bits>>shift));}
    return out;
}
std::vector<float> read(const std::filesystem::path& path,const char* hash,std::size_t count) {
    std::ifstream file(path,std::ios::binary);if(!file) throw std::runtime_error("missing Gaussian fixture");
    const std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});
    check(data.size()==count*4);qpx::Sha256 digest(true);digest.update(data.data(),data.size());check(digest.finish()==hash);
    std::vector<float> out(count);
    for(std::size_t i=0;i<count;++i) {std::uint32_t bits=0;for(int j=0;j<4;++j) bits|=std::uint32_t(data[4*i+j])<<(8*j);std::memcpy(&out[i],&bits,4);}
    return out;
}
void write(const std::filesystem::path& path,const std::vector<float>& values) {
    const auto data=bytes(values);std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());file.close();check(bool(file));
}
}
int main(int argc,char** argv) {
    try {
        using namespace qpx::detail;
        if(argc!=3) throw std::invalid_argument("fixture and artifact directory required");
        const auto fixture=std::filesystem::u8path(argv[1]),output=std::filesystem::u8path(argv[2]);
        std::filesystem::create_directories(output);
        constexpr std::size_t budget=256*1024*1024;
        std::size_t passed=0;
        for(const auto& c:imagej_gaussian_reference::cases) {
            const std::string name=c.name;
            const auto input=read(fixture/(name+"-input.f32le"),c.input_hash,c.width*c.height);
            const auto original=bytes(input);
            const auto expected_blur=read(fixture/(name+"-blur.f32le"),c.blur_hash,c.width*c.height);
            const auto expected_log=read(fixture/(name+"-log.f32le"),c.log_hash,c.width*c.height);
            const auto blur=imagej154p_gaussian(input,c.width,c.height,c.sigma,c.sigma,budget);
            const auto log=imagej154p_laplacian(blur,c.width,c.height,budget);
            write(output/(name+"-blur.f32le"),blur);write(output/(name+"-log.f32le"),log);
            if(bytes(blur)!=bytes(expected_blur) || bytes(log)!=bytes(expected_log)) throw std::runtime_error("Gaussian reference mismatch: "+name);
            check(bytes(input)==original);++passed;
        }
        const std::vector<float> input={1,2,3,4};const auto original=bytes(input);
        check(imagej154p_gaussian(input,2,2,0,0,budget)==input);
        check(imagej154p_laplacian(input,2,2,budget)==std::vector<float>({-3,-1,1,3}));
        std::atomic<bool> cancelled{true};
        refuses<std::runtime_error>([&]{imagej154p_gaussian(input,2,2,1,1,budget,&cancelled);});
        refuses<std::runtime_error>([&]{imagej154p_laplacian(input,2,2,budget,&cancelled);});
        for(double sigma:{-1.,4097.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            refuses<std::invalid_argument>([&]{imagej154p_gaussian(input,2,2,sigma,1,budget);});
            refuses<std::invalid_argument>([&]{imagej154p_gaussian(input,2,2,1,sigma,budget);});
        }
        refuses<std::invalid_argument>([&]{imagej154p_gaussian(input,0,2,1,1,budget);});
        refuses<std::invalid_argument>([&]{imagej154p_gaussian(input,3,2,1,1,budget);});
        refuses<std::invalid_argument>([&]{imagej154p_gaussian(input,std::numeric_limits<std::size_t>::max(),2,1,1,budget);});
        refuses<std::length_error>([&]{imagej154p_gaussian(input,2,2,1,1,0);});
        refuses<std::length_error>([&]{imagej154p_laplacian(input,2,2,15);});
        for(float value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            auto bad=input;bad[0]=value;
            refuses<std::invalid_argument>([&]{imagej154p_gaussian(bad,2,2,1,1,budget);});
            refuses<std::invalid_argument>([&]{imagej154p_laplacian(bad,2,2,budget);});
        }
        const std::vector<float> huge(9,std::numeric_limits<float>::max());
        refuses<std::overflow_error>([&]{imagej154p_gaussian(huge,3,3,1,1,budget);});
        check(bytes(input)==original);
        std::cout<<passed<<" ImageJ1.54p captured float32 Gaussian and LoG arrays exact; private kernel admission/cancellation/overflow/input ownership passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
