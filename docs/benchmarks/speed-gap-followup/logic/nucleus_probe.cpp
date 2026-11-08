
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include "qupath_nuclei.hpp"
using namespace qpx;using namespace qpx::cell_detail;
int main(int argc,char** argv) {
 if(argc!=3)return 2;constexpr size_t w=512,h=512,n=w*h,budget=512u*1024u*1024u;
 std::vector<unsigned char> rgb(3*n);std::ifstream f(argv[1],std::ios::binary);f.read((char*)rgb.data(),rgb.size());if(f.gcount()!=rgb.size())return 3;
 std::vector<uint32_t> expected(n);std::ifstream g(argv[2],std::ios::binary);g.read((char*)expected.data(),4*n);if(g.gcount()!=4*n)return 4;
 float lut[256];for(int i=0;i<256;++i)lut[i]=(float)std::max(0.,-std::log10(std::max(1.,double(i))/255.));
 std::vector<float> score(n);for(size_t i=0;i<n;++i){float v=lut[rgb[3*i]]+lut[rgb[3*i+1]];score[i]=v+lut[rgb[3*i+2]];}
 auto blurred=detail::imagej154p_gaussian(score,w,h,1,1,budget);auto response=detail::imagej154p_laplacian(blurred,w,h,budget);
 LogParameters p;p.sigma=0;p.threshold=.4;p.prominence=.5;p.minimum_radius=0;p.minimum_spacing=0;p.minimum_area=16;p.maximum_area=512;p.maximum_cells=50000;p.work_bytes=budget;p.eight_connected_background=true;
 std::vector<uint8_t> domain(n,1);std::cout<<std::unitbuf;
 for(int run=0;run<12;++run){
 std::printf("{\"iteration\":%d,",run);auto start=std::chrono::steady_clock::now();auto tick=[&](const char* key){auto now=std::chrono::steady_clock::now();std::printf("\"%s\":%.6f,",key,std::chrono::duration<double,std::milli>(now-start).count());start=std::chrono::steady_clock::now();};
 auto basins=qupath070_response_basins(response,w,h,p.maximum_cells,budget);tick("basins_ms");
 std::vector<uint8_t> positive(n);for(size_t i=0;i<n;++i)positive[i]=response[i]>0;
 auto regions=log_filled_basin_regions(score,basins,positive,w,h,p,nullptr);tick("regions_ms");
 auto shape=sampled_shape_watershed(regions,w,h,p.prominence,p.maximum_cells,budget);tick("shape_ms");
 auto result=log_label_sampled_nuclei(score,domain,shape,regions,w,h,p,nullptr,nullptr);tick("label_ms");
 bool exact=result.labels==expected;std::printf("\"cells\":%zu,\"exact_labels\":%s}\n",result.count,exact?"true":"false");if(!exact || result.count!=2237)return 5;
 }
}
