#include "qupath_nuclei.hpp"
#include "cell_expansion.hpp"
#include <fstream>
#include <iostream>
#include <string>
template<class T> std::vector<T> read(const char* name,std::size_t n) {std::vector<T> a(n);std::ifstream f(name,std::ios::binary);f.read(reinterpret_cast<char*>(a.data()),std::streamsize(n*sizeof(T)));if(!f) throw std::runtime_error("input failed");return a;}
template<class T> void write(const std::string& name,const std::vector<T>& a) {std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(a.data()),std::streamsize(a.size()*sizeof(T)));if(!f) throw std::runtime_error("output failed");}
int main(int argc,char** argv) {
    try {
        if(argc!=12) return 2;const auto w=std::stoull(argv[5]),h=std::stoull(argv[6]);
        const auto score=read<float>(argv[1],w*h),response=read<float>(argv[2],w*h);const auto domain=read<std::uint8_t>(argv[3],w*h);
        qpx::cell_detail::LogParameters p;p.sigma=0;p.minimum_radius=0;p.minimum_spacing=0;p.threshold=std::stod(argv[11]);
        p.minimum_area=std::stoull(argv[8]);p.maximum_area=std::stoull(argv[9]);p.eight_connected_background=true;
        const auto result=qpx::cell_detail::qupath070_nuclei(score,response,domain,w,h,std::stod(argv[7]),p);
        const auto bodies=qpx::cell_detail::expand_cells_watershed(result.nuclei.labels,domain,w,h,std::stod(argv[10]));
        const std::string prefix=argv[4];write(prefix+"-candidate-basins.u32le",result.basins);write(prefix+"-candidate-regions.u8",result.regions);
        write(prefix+"-candidate-binary.u8",result.binary);write(prefix+"-candidate-refinement.u8",result.refinement);
        write(prefix+"-candidate-nuclei.u32le",result.nuclei.labels);write(prefix+"-candidate-bodies.u32le",bodies.labels);
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
