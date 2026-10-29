#include "scientific_cell_recipe.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=5) return 2;
    const auto w=std::stoull(argv[2]),h=std::stoull(argv[3]);
    std::vector<double> score(w*h);std::ifstream in(argv[1],std::ios::binary);
    in.read(reinterpret_cast<char*>(score.data()),score.size()*sizeof(double));
    if(!in || in.peek()!=std::char_traits<char>::eof()) return 3;
    std::vector<std::uint8_t> valid(w*h,1);const qpx::cell_detail::PhysicalGridMetric metric(1,1);
    auto blur=score;qpx::cell_detail::physical_gaussian(blur,valid,w,h,metric,1,256*1024*1024);
    const auto positive=qpx::cell_detail::grid_log_positive(score,valid,w,h,metric,1,256*1024*1024,nullptr);
    std::vector<double> log(w*h);
    for(std::size_t y=0;y<h;++y) for(std::size_t x=0;x<w;++x) {
        const auto i=y*w+x;
        log[i]=((blur[i]-blur[y*w+(x?x-1:x)])+(blur[i]-blur[y*w+(x+1<w?x+1:x)]))+
            ((blur[i]-blur[(y?y-1:y)*w+x])+(blur[i]-blur[(y+1<h?y+1:y)*w+x]));
        if(positive[i]!=(log[i]>0)) return 4;
    }
    const std::string prefix=argv[4];
    auto write=[&](const std::string& suffix,const auto& values) {
        std::ofstream out(prefix+suffix,std::ios::binary);
        out.write(reinterpret_cast<const char*>(values.data()),values.size()*sizeof(values[0]));out.close();
        if(!out) throw std::runtime_error("probe output failed");
    };
    write("-blur.f64le",blur);write("-log.f64le",log);write("-positive.u8",positive);
}
