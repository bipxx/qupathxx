#include "qupath_response_basins.hpp"
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    try {
        if(argc!=5) return 2;const auto w=std::stoull(argv[3]),h=std::stoull(argv[4]);
        std::vector<float> input(w*h);std::ifstream source(argv[1],std::ios::binary);
        source.read(reinterpret_cast<char*>(input.data()),std::streamsize(input.size()*4));if(!source) return 3;
        const auto labels=qpx::cell_detail::qupath070_response_basins(input,w,h,10000,256*1024*1024);
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<const char*>(labels.data()),std::streamsize(labels.size()*4));if(!dest) return 4;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
