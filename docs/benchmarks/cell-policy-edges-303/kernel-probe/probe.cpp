#include "imagej_gaussian.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    try {
        if(argc!=7) throw std::invalid_argument("input output width height sigma_x sigma_y required");
        const auto width=std::stoull(argv[3]),height=std::stoull(argv[4]);
        if(!width || !height || width>(1ULL<<26)/height) throw std::invalid_argument("invalid probe dimensions");
        std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
        if(!input || input.tellg()!=std::streamoff(width*height*4)) throw std::invalid_argument("probe input size differs");
        input.seekg(0);std::vector<float> values(width*height);
        input.read(reinterpret_cast<char*>(values.data()),std::streamsize(values.size()*4));
        if(!input) throw std::runtime_error("probe input failed");
        const auto blurred=qpx::detail::imagej154p_gaussian(values,width,height,std::stod(argv[5]),std::stod(argv[6]),256*1024*1024);
        const auto log=qpx::detail::imagej154p_laplacian(blurred,width,height,256*1024*1024);
        std::ofstream output(argv[2],std::ios::binary);output.write(reinterpret_cast<const char*>(log.data()),std::streamsize(log.size()*4));
        if(!output) throw std::runtime_error("probe output failed");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
