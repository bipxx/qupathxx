#include "scientific_cell_recipe.hpp"
#include <fstream>
#include <iostream>
#include <string>
template<class T> std::vector<T> read(const char* name,std::size_t n) {
    std::ifstream file(name,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()!=std::streamoff(n*sizeof(T))) throw std::runtime_error("trace input size differs");
    file.seekg(0);std::vector<T> result(n);file.read(reinterpret_cast<char*>(result.data()),std::streamsize(n*sizeof(T)));
    if(!file) throw std::runtime_error("trace input read failed");return result;
}
template<class T> void write(const std::string& name,const std::vector<T>& values) {
    std::ofstream file(name,std::ios::binary);file.write(reinterpret_cast<const char*>(values.data()),std::streamsize(values.size()*sizeof(T)));
    if(!file) throw std::runtime_error("trace output failed");
}
int main(int argc,char** argv) {
    try {
        if(argc!=11) throw std::invalid_argument("score response domain prefix width height step min max threshold required");
        const auto w=std::stoull(argv[5]),h=std::stoull(argv[6]);
        if(!w || !h || w>(1ULL<<26)/h) throw std::invalid_argument("invalid dimensions");
        const auto n=w*h;const auto input=read<float>(argv[1],n),response=read<float>(argv[2],n);const auto domain=read<std::uint8_t>(argv[3],n);
        std::vector<double> score(input.begin(),input.end());std::vector<std::uint8_t> positive(n),valid(n,1);
        for(std::size_t i=0;i<n;++i) positive[i]=response[i]>0;
        qpx::cell_detail::LogParameters p;p.sigma=0;p.threshold=std::stod(argv[10]);p.minimum_radius=0;p.minimum_spacing=0;
        p.minimum_area=std::stoull(argv[8]);p.maximum_area=std::stoull(argv[9]);p.eight_connected_background=true;
        const auto mask=qpx::cell_detail::log_regions_from_positive(score,positive,w,h,p,nullptr);
        const auto basins=qpx::cell_detail::log_response_basins(response,w,h,p.maximum_cells,nullptr);
        const auto step=std::stod(argv[7]);const qpx::cell_detail::PhysicalGridMetric metric(step,step);
        const auto current=qpx::cell_detail::grid_log_watershed(score,valid,domain,w,h,metric,0,p,nullptr,nullptr,&positive);
        const auto shape=qpx::cell_detail::sampled_shape_watershed(mask,w,h,p.prominence,p.maximum_cells,p.work_bytes,nullptr);
        const auto sampled=qpx::cell_detail::log_sampled_nuclei(input,domain,mask,w,h,p,nullptr,nullptr);
        const std::string prefix=argv[4];write(prefix+"-positive.u8",positive);write(prefix+"-regions.u8",mask);
        write(prefix+"-basins.u32le",basins);write(prefix+"-grid.u32le",current.labels);
        write(prefix+"-sampled.u8",shape.mask);write(prefix+"-sampled-nuclei.u32le",sampled.labels);
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
