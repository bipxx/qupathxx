#include "../cell_expansion.hpp"
#include "build_manifest.hpp"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace {
void require(bool value) {if(!value) throw std::runtime_error("cell expansion contract failed");}
template<class Exception,class Function> void rejects(Function f) {
    try {f();} catch(const Exception&) {return;}throw std::runtime_error("missing cell expansion rejection");
}
std::uint32_t read(std::istream& in) {
    unsigned char b[4];require(bool(in.read(reinterpret_cast<char*>(b),4)));
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
void write(std::ostream& out,std::uint32_t n) {for(unsigned i=0;i<4;++i) out.put(static_cast<char>((n>>(8*i))&255));}
void probe(const char* source,const char* destination) {
    std::ifstream in(source,std::ios::binary);char magic[4];require(bool(in.read(magic,4)) && !std::memcmp(magic,"QBE1",4));
    const auto width=read(in),height=read(in);require(width && height && std::uint64_t(width)*height<=1024*1024);
    const auto radius=read(in);const auto n=std::size_t(width)*height;
    std::vector<std::uint32_t> labels(n);std::vector<std::uint8_t> domain(n);
    for(auto& label:labels) label=read(in);
    require(bool(in.read(reinterpret_cast<char*>(domain.data()),static_cast<std::streamsize>(n))));require(in.peek()==std::char_traits<char>::eof());
    const auto result=qpx::cell_detail::expand_cells_watershed(labels,domain,width,height,radius);
    std::ofstream out(destination,std::ios::binary|std::ios::trunc);out.write("QBR1",4);write(out,width);write(out,height);
    for(auto label:result.labels) write(out,label);out.flush();require(bool(out));
}
void contracts() {
    using namespace qpx::cell_detail;
    std::vector<std::uint32_t> labels(121);labels[5*11+5]=7;std::vector<std::uint8_t> domain(121,1);
    const auto result=expand_cells_watershed(labels,domain,11,11,5);
    for(int y=0;y<11;++y) for(int x=0;x<11;++x) {
        const auto squared=(x-5)*(x-5)+(y-5)*(y-5);
        require(result.labels[std::size_t(y)*11+x]==(squared<25?7u:0u));
    }
    require(result.queued_pixels<=labels.size() && result.peak_queue<=result.queued_pixels);
    require(expand_cells_watershed(labels,domain,11,11,0).labels==labels);
    labels.assign(35,0);domain.assign(35,1);labels[2*7+1]=1;labels[2*7+5]=2;
    const auto pair=expand_cells_watershed(labels,domain,7,5,20);
    for(std::size_t y=0;y<5;++y) {require(pair.labels[y*7+3]==0);require(pair.labels[y*7]==1 && pair.labels[y*7+6]==2);}
    // A domain barrier prevents propagation even within Euclidean radius.
    for(std::size_t y=0;y<5;++y) domain[y*7+2]=0;
    labels[2*7+5]=0;const auto clipped=expand_cells_watershed(labels,domain,7,5,20);
    for(std::size_t y=0;y<5;++y) for(std::size_t x=2;x<7;++x) require(!clipped.labels[y*7+x]);
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{expand_cells_watershed(labels,domain,7,5,20,1024*1024,&cancelled);});
    rejects<std::length_error>([&]{expand_cells_watershed(labels,domain,7,5,20,1);});
    rejects<std::invalid_argument>([&]{expand_cells_watershed(labels,domain,7,5,-1);});
    rejects<std::invalid_argument>([&]{expand_cells_watershed(labels,domain,7,5,std::numeric_limits<double>::quiet_NaN());});
    domain[2*7+1]=0;rejects<std::invalid_argument>([&]{expand_cells_watershed(labels,domain,7,5,20);});
    rejects<std::length_error>([]{body_expansion_work_bytes(std::numeric_limits<std::uint32_t>::max(),2);});
    labels.assign(2048,0);domain.assign(2048,1);for(std::size_t i=0;i<labels.size();i+=17) labels[i]=static_cast<std::uint32_t>(i+1);
    const auto dense=expand_cells_watershed(labels,domain,64,32,10);
    require(dense.queued_pixels<=labels.size() && dense.peak_queue<=dense.queued_pixels);
    for(std::size_t i=0;i<labels.size();++i) if(labels[i]) require(dense.labels[i]==labels[i]);
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--environment") {
            std::cout<<"{\"source_tree_sha256\":"<<std::quoted(qpx::build_info::source_sha256)
                     <<",\"git_revision\":"<<std::quoted(qpx::build_info::git_revision)<<"}\n";return 0;
        }
        if(argc==3) {probe(argv[1],argv[2]);return 0;}
        require(argc==1);contracts();std::cout<<"Strict-radius expansion, stable collision lines, domain barriers, work bounds and cancellation passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
