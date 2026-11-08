#include "scientific_cell_recipe.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
int main(int argc,char** argv) {
 if(argc!=2) return 2;
 constexpr std::size_t w=192,h=128;
 std::vector<double> input(w*h);std::ifstream file(argv[1],std::ios::binary);
 file.read(reinterpret_cast<char*>(input.data()),std::streamsize(input.size()*sizeof(double)));
 if(!file || file.peek()!=std::char_traits<char>::eof()) return 3;
 std::vector<std::uint8_t> valid(w*h,1);const qpx::cell_detail::PhysicalGridMetric metric(1,1);
 auto smooth=input;qpx::cell_detail::physical_gaussian(smooth,valid,w,h,metric,1,256*1024*1024);
 const auto positive=qpx::cell_detail::grid_log_positive(input,valid,w,h,metric,1,256*1024*1024,nullptr);
 std::cout<<std::setprecision(17)<<"[";bool first=true;
 for(const auto y:{36,49}) for(const auto x:{0,1}) {
  const auto i=y*w+x;
  const auto value=((smooth[i]-smooth[y*w+(x?x-1:x)])+(smooth[i]-smooth[y*w+x+1]))+
                   ((smooth[i]-smooth[(y-1)*w+x])+(smooth[i]-smooth[(y+1)*w+x]));
  if(!first)std::cout<<",";first=false;
  std::cout<<"{\"x\":"<<x+32<<",\"y\":"<<y+32<<",\"response\":"<<value<<",\"positive\":"<<int(positive[i])<<"}";
 }
 std::cout<<"]\n";
}
