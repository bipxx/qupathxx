#include "../log_watershed.hpp"
#include "build_manifest.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>

namespace {
void require(bool condition) {if(!condition) throw std::runtime_error("LoG watershed contract failed");}
void reconstruction_oracle() {
    // Independent simultaneous fixed-point dilation, without a priority queue
    // or the production neighbor traversal. Quarter-valued inputs are exact.
    for(auto shape:{std::pair<std::size_t,std::size_t>{1,9},{9,1},{5,7},{13,11}})
    for(double prominence:{.125,.5,2.}) {
        const auto width=shape.first,height=shape.second,n=width*height;
        std::vector<double> d(n),previous(n);std::vector<std::uint8_t> mask(n),expected(n);
        for(std::size_t i=0;i<n;++i) {
            mask[i]=i%7!=3;d[i]=mask[i]?double((i*37)%13+1)/4:0;
            previous[i]=mask[i]?std::max(0.,d[i]-prominence):0;
        }
        bool settled=false;
        for(std::size_t step=0;step<=n;++step) {
            auto next=previous;
            for(std::size_t y=0;y<height;++y) for(std::size_t x=0;x<width;++x) {
                const auto i=y*width+x;if(!mask[i]) continue;double value=previous[i];
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    const auto xx=static_cast<std::int64_t>(x)+dx,yy=static_cast<std::int64_t>(y)+dy;
                    if(xx>=0 && yy>=0 && xx<static_cast<std::int64_t>(width) && yy<static_cast<std::int64_t>(height)) {
                        const auto j=static_cast<std::size_t>(yy)*width+static_cast<std::size_t>(xx);
                        if(mask[j]) value=std::max(value,previous[j]);
                    }
                }
                next[i]=std::min(d[i],value);
            }
            if(next==previous) {settled=true;break;}previous=std::move(next);
        }
        require(settled);
        for(std::size_t i=0;i<n;++i) expected[i]=mask[i] && d[i]-previous[i]>=prominence;
        require(qpx::cell_detail::log_h_maxima(d,mask,width,height,prominence,nullptr)==expected);
    }
}
std::uint32_t read_integer(std::istream& stream) {
    unsigned char bytes[4];if(!stream.read(reinterpret_cast<char*>(bytes),4)) throw std::runtime_error("truncated LoG input");
    return std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
}
void write_integer(std::ostream& stream,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) stream.put(static_cast<char>((value>>(i*8))&255));
}
void probe(const char* input,const char* output) {
    std::ifstream stream(input,std::ios::binary);char magic[4];stream.read(magic,4);
    require(stream.good() && std::memcmp(magic,"QLP1",4)==0);
    const auto width=read_integer(stream),height=read_integer(stream);
    require(width && height && std::uint64_t(width)*height<=1024*1024);
    const auto n=std::size_t(width)*height;std::vector<float> score(n);std::vector<std::uint8_t> domain(n);
    for(auto& value:score) {const auto bits=read_integer(stream);std::memcpy(&value,&bits,4);}
    require(bool(stream.read(reinterpret_cast<char*>(domain.data()),static_cast<std::streamsize>(n))));
    require(stream.peek()==std::char_traits<char>::eof());
    const auto result=qpx::cell_detail::log_watershed(score,domain,width,height);
    std::ofstream target(output,std::ios::binary|std::ios::trunc);require(bool(target));
    target.write("QLR1",4);write_integer(target,width);write_integer(target,height);write_integer(target,static_cast<std::uint32_t>(result.count));
    for(auto value:result.labels) write_integer(target,value);target.flush();require(bool(target));
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--environment") {
            std::cout<<"{\"source_tree_sha256\":"<<std::quoted(qpx::build_info::source_sha256)
                     <<",\"git_revision\":"<<std::quoted(qpx::build_info::git_revision)
                     <<",\"compiler\":"<<std::quoted(qpx::build_info::compiler)<<"}\n";return 0;
        }
        if(argc==3) {probe(argv[1],argv[2]);return 0;}
        require(argc==1);
        using namespace qpx::cell_detail;
        reconstruction_oracle();
        const std::vector<double> distance={0,1,2,1.8,2.1,1,0};
        const std::vector<std::uint8_t> mask={0,1,1,1,1,1,0};
        require(log_h_maxima(distance,mask,7,1,.5,nullptr)==std::vector<std::uint8_t>({0,0,0,0,1,0,0}));
        require(log_h_maxima(distance,mask,7,1,.1,nullptr)==std::vector<std::uint8_t>({0,0,1,0,1,0,0}));
        require(log_h_maxima({0,1,2,2,1,0},{0,1,1,1,1,0},6,1,.5,nullptr)==std::vector<std::uint8_t>({0,0,1,1,0,0}));
        std::vector<float> score(65*65);std::vector<std::uint8_t> domain(score.size(),1);
        require(log_watershed(score,domain,65,65).count==0);
        for(std::size_t y=0;y<65;++y) for(std::size_t x=0;x<65;++x) {
            const auto dx=double(x)-32,dy=double(y)-32;score[y*65+x]=dx*dx+dy*dy<=81?1.f:0.f;
        }
        const auto disk=log_watershed(score,domain,65,65);require(disk.count==1);
        require(disk.labels[32*65+32]==1 && disk.labels.front()==0);
        std::fill(domain.begin(),domain.end(),0);require(log_watershed(score,domain,65,65).count==0);
        std::atomic<bool> cancelled{true};bool stopped=false;
        try {log_watershed(score,domain,65,65,{},&cancelled);} catch(const std::runtime_error&) {stopped=true;}
        require(stopped);
        LogParameters tiny;tiny.work_bytes=1;bool bounded=false;
        try {log_watershed(score,domain,65,65,tiny);} catch(const std::length_error&) {bounded=true;}
        require(bounded);
        std::cout<<"LoG regions, prominence reconstruction, native labels, clipping, cancellation and work guard passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
