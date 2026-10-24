#include "../log_watershed.hpp"
#include "../shape_watershed.hpp"
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
void shape_probe(const char* input,const char* output) {
    std::ifstream stream(input,std::ios::binary);char magic[4];stream.read(magic,4);require(stream.good() && std::memcmp(magic,"QLR1",4)==0);
    const auto width=read_integer(stream),height=read_integer(stream);read_integer(stream);
    require(width && height && std::uint64_t(width)*height<=1024*1024);std::vector<std::uint8_t> mask(std::size_t(width)*height);
    for(auto& value:mask) value=read_integer(stream)!=0;require(stream.peek()==std::char_traits<char>::eof());
    const auto result=qpx::cell_detail::sampled_shape_watershed(mask,width,height);
    std::ofstream target(output,std::ios::binary|std::ios::trunc);require(bool(target));target.write("QLR1",4);
    write_integer(target,width);write_integer(target,height);write_integer(target,1);for(auto v:result.mask) write_integer(target,v);target.flush();require(bool(target));
}
void shape_contract() {
    using qpx::cell_detail::sampled_shape_watershed;
    const std::vector<std::uint8_t> blank(25),full(25,1),single={0,0,0,0,1,0,0,0,0};
    require(sampled_shape_watershed(blank,5,5).mask==blank);require(sampled_shape_watershed(full,5,5).mask==full);
    require(sampled_shape_watershed(single,3,3).mask==single);
    bool invalid=false;try {sampled_shape_watershed(single,2,3);} catch(const std::invalid_argument&) {invalid=true;}require(invalid);
    bool bounded=false;try {sampled_shape_watershed(single,3,3,.5,100,1);} catch(const std::length_error&) {bounded=true;}require(bounded);
    std::atomic<bool> cancelled{true};bool stopped=false;try {sampled_shape_watershed(single,3,3,.5,100,100000,&cancelled);} catch(const std::runtime_error&) {stopped=true;}require(stopped);
    std::vector<std::uint8_t> particles(81);for(std::size_t y=1;y<9;y+=3) for(std::size_t x=1;x<9;x+=3) particles[y*9+x]=1;
    bool limited=false;try {sampled_shape_watershed(particles,9,9,.5,1);} catch(const std::length_error&) {limited=true;}require(limited);
}
void probe(const char* input,const char* output,bool response_basins=false,bool regions_only=false,bool dual_background=false) {
    std::ifstream stream(input,std::ios::binary);char magic[4];stream.read(magic,4);
    require(stream.good() && std::memcmp(magic,"QLP1",4)==0);
    const auto width=read_integer(stream),height=read_integer(stream);
    require(width && height && std::uint64_t(width)*height<=1024*1024);
    const auto n=std::size_t(width)*height;std::vector<float> score(n);std::vector<std::uint8_t> domain(n);
    for(auto& value:score) {const auto bits=read_integer(stream);std::memcpy(&value,&bits,4);}
    require(bool(stream.read(reinterpret_cast<char*>(domain.data()),static_cast<std::streamsize>(n))));
    require(stream.peek()==std::char_traits<char>::eof());
    qpx::cell_detail::LogParameters parameters;parameters.response_basins=response_basins;
    parameters.eight_connected_background=dual_background;
    qpx::cell_detail::LogResult result;
    if(regions_only) {
        const auto mask=response_basins?qpx::cell_detail::log_basin_regions(score,width,height,parameters,nullptr):qpx::cell_detail::log_regions(score,width,height,parameters,nullptr);
        result.labels.assign(mask.begin(),mask.end());result.count=std::any_of(mask.begin(),mask.end(),[](auto value){return value!=0;})?1:0;
    } else result=qpx::cell_detail::log_watershed(score,domain,width,height,parameters);
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
        if(argc==4 && std::string(argv[1])=="--response-basins") {probe(argv[2],argv[3],true);return 0;}
        if(argc==4 && std::string(argv[1])=="--regions") {probe(argv[2],argv[3],false,true);return 0;}
        if(argc==4 && std::string(argv[1])=="--response-regions") {probe(argv[2],argv[3],true,true);return 0;}
        if(argc==4 && std::string(argv[1])=="--dual-background") {probe(argv[2],argv[3],false,false,true);return 0;}
        if(argc==4 && std::string(argv[1])=="--dual-regions") {probe(argv[2],argv[3],false,true,true);return 0;}
        if(argc==4 && std::string(argv[1])=="--shape-split") {shape_probe(argv[2],argv[3]);return 0;}
        require(argc==1);
        using namespace qpx::cell_detail;
        reconstruction_oracle();
        shape_contract();
        std::vector<std::uint8_t> leak4={0,1,1,1,0,1,1,1,1},leak8=leak4;
        log_fill_holes(leak4,3,3,nullptr);log_fill_holes(leak8,3,3,nullptr,true);
        require(leak4[4]==1 && leak8[4]==0);
        require(log_response_basins({0,3,2,1,2,3,0},7,1,20,nullptr)==std::vector<std::uint32_t>({0,1,1,0,2,2,0}));
        require(log_response_basins({0,3,3,2,1,0},6,1,20,nullptr)==std::vector<std::uint32_t>({0,1,1,1,1,0}));
        require(log_response_basins({0,.0005f,0},3,1,20,nullptr)==std::vector<std::uint32_t>({0,0,0}));
        bool basin_limit=false;try {log_response_basins({0,3,1,3,0},5,1,1,nullptr);} catch(const std::length_error&) {basin_limit=true;}require(basin_limit);
        require(log_response_basins({0,2,2,3,0},5,1,20,nullptr)==std::vector<std::uint32_t>({0,1,1,1,0}));
        require(log_response_basins({0,0,0,0},2,2,20,nullptr)==std::vector<std::uint32_t>({0,0,0,0}));
        bool invalid_basins=false;try {log_response_basins({0},2,2,20,nullptr);} catch(const std::invalid_argument&) {invalid_basins=true;}require(invalid_basins);
        std::atomic<bool> basin_cancel{true};bool basin_stopped=false;try {log_response_basins({0,1,0},3,1,20,&basin_cancel);} catch(const std::runtime_error&) {basin_stopped=true;}require(basin_stopped);
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
        LogParameters basin_parameters;basin_parameters.response_basins=true;
        require(log_watershed(score,domain,65,65,basin_parameters).labels==disk.labels);
        require(disk.labels[32*65+32]==1 && disk.labels.front()==0);
        std::fill(domain.begin(),domain.end(),0);require(log_watershed(score,domain,65,65).count==0);
        std::atomic<bool> cancelled{true};bool stopped=false;
        try {log_watershed(score,domain,65,65,{},&cancelled);} catch(const std::runtime_error&) {stopped=true;}
        require(stopped);
        LogParameters tiny;tiny.work_bytes=1;bool bounded=false;
        try {log_watershed(score,domain,65,65,tiny);} catch(const std::length_error&) {bounded=true;}
        require(bounded);
        tiny.response_basins=true;bounded=false;try {log_watershed(score,domain,65,65,tiny);} catch(const std::length_error&) {bounded=true;}require(bounded);
        std::cout<<"LoG regions, prominence reconstruction, native labels, clipping, cancellation and work guard passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
