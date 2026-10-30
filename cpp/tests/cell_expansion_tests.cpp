#include "../cell_expansion.hpp"
#include "build_manifest.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <filesystem>
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
void probe(const char* source,const char* destination,bool small=true,bool timings=false) {
    std::ifstream in(source,std::ios::binary);char magic[4];require(bool(in.read(magic,4)) && !std::memcmp(magic,"QBE1",4));
    const auto width=read(in),height=read(in);require(width && height && std::uint64_t(width)*height<=1024*1024);
    const auto radius=read(in);const auto n=std::size_t(width)*height;
    std::vector<std::uint32_t> labels(n);std::vector<std::uint8_t> domain(n);
    for(auto& label:labels) label=read(in);
    require(bool(in.read(reinterpret_cast<char*>(domain.data()),static_cast<std::streamsize>(n))));require(in.peek()==std::char_traits<char>::eof());
    const auto began=std::chrono::steady_clock::now();
    const auto result=qpx::cell_detail::expand_cells_watershed(labels,domain,width,height,radius,128u*1024*1024,nullptr,small);
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
    std::ofstream out(destination,std::ios::binary|std::ios::trunc);out.write("QBR1",4);write(out,width);write(out,height);
    for(auto label:result.labels) write(out,label);out.flush();require(bool(out));
    if(timings) std::cout<<"{\"kernel_ms\":"<<std::setprecision(17)<<elapsed
        <<",\"queued_pixels\":"<<result.queued_pixels<<",\"peak_queue\":"<<result.peak_queue
        <<",\"queue\":"<<std::quoted(small && radius<=8?"buckets":"heap")<<"}\n";
}
void queue_equivalence() {
    using namespace qpx::cell_detail;
    SmallExpansionFrontier bins(64);
    float previous=-1;
    for(unsigned k=0;k<64;++k) {
        const float priority=std::sqrt(static_cast<float>(k));
        require(priority>previous && std::lround(priority*priority)==k);previous=priority;
    }
    for(unsigned k=64;k--;) bins.push(k,std::sqrt(static_cast<float>(k)));
    for(unsigned k=0;k<64;++k) require(bins.pop()==k);
    require(bins.empty() && bins.size()==0);
    bins.push(0,std::sqrt(63.f));require(bins.pop()==0);
    bins.push(1,std::sqrt(5.f));bins.push(2,std::sqrt(5.f));bins.push(3,1);
    require(bins.pop()==3 && bins.pop()==1 && bins.pop()==2 && bins.empty());
    const double diagonal=std::sqrt(2.f);
    const std::array<double,14> radii={.01,1.,1.00000001,diagonal,
        std::nextafter(diagonal,std::numeric_limits<double>::infinity()),2.,std::sqrt(5.f),3.7,5.,
        std::nextafter(8.,0.),8.,std::nextafter(8.,9.),12.,1000.};
    std::uint32_t random=0x51a9u;std::size_t comparisons=0;
    for(auto shape:{std::pair<std::size_t,std::size_t>{1,129},{129,1},{2,2},{5,7},{37,29},{64,64}})
    for(unsigned trial=0;trial<16;++trial) {
        const auto width=shape.first,height=shape.second,n=width*height;
        std::vector<std::uint32_t> labels(n);std::vector<std::uint8_t> domain(n);std::uint32_t label=0;
        for(std::size_t i=0;i<n;++i) {
            random=random*1664525u+1013904223u;domain[i]=random%11!=0;
            if(domain[i] && random%23==0) labels[i]=++label;
        }
        domain[0]=1;labels[0]=++label;
        for(double radius:radii) {
            const auto fast=expand_cells_watershed(labels,domain,width,height,radius);
            const auto heap=expand_cells_watershed(labels,domain,width,height,radius,128u*1024*1024,nullptr,false);
            require(fast.labels==heap.labels && fast.queued_pixels==heap.queued_pixels && fast.peak_queue==heap.peak_queue);
            ++comparisons;
        }
    }
    require(comparisons==1344);
    std::cout<<"Small frontier 64 exact priority keys and 1344 full heap-equivalence cases passed\n";
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
void boundary_reference() {
    using namespace qpx::cell_detail;
    auto load=[](const char* name) {
        std::ifstream input(std::filesystem::path(QPX_BODY_BOUNDARY_FIXTURE)/name,std::ios::binary);require(bool(input));
        std::vector<std::uint32_t> values(256*256);for(auto& value:values) value=read(input);
        require(input.peek()==std::char_traits<char>::eof());return values;
    };
    const auto nuclei=load("nucleus.u32le"),expected=load("body.u32le");
    const std::vector<std::uint8_t> domain(nuclei.size(),1);
    for(bool small:{false,true}) {
        const auto old=expand_cells_watershed(nuclei,domain,256,256,5,128*1024*1024,nullptr,small);
        std::size_t differences=0;for(std::size_t i=0;i<nuclei.size();++i) differences+=old.labels[i]!=expected[i];
        require(differences==8);
        const auto corrected=expand_cells_watershed(nuclei,domain,256,256,5,128*1024*1024,nullptr,small,true);
        require(corrected.labels==expected && corrected.queued_pixels<=nuclei.size());
        require(expand_cells_watershed(nuclei,domain,256,256,1,128*1024*1024,nullptr,small,true).labels==nuclei);
    }
    std::atomic<bool> cancelled{true};
    rejects<std::runtime_error>([&]{expand_cells_watershed(nuclei,domain,256,256,5,128*1024*1024,&cancelled,true,true);});
    rejects<std::length_error>([&]{expand_cells_watershed(nuclei,domain,256,256,5,body_expansion_work_bytes(256,256)-1,nullptr,true,true);});
    std::cout<<"506 public real-slide body masks exact with boundary frontier; both queues preserve eight V1 differences, unit radius, budget and cancellation passed\n";
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--environment") {
            std::cout<<"{\"source_tree_sha256\":"<<std::quoted(qpx::build_info::source_sha256)
                     <<",\"git_revision\":"<<std::quoted(qpx::build_info::git_revision)<<"}\n";return 0;
        }
        if(argc==3) {probe(argv[1],argv[2]);return 0;}
        if(argc==4 && std::string(argv[1])=="--heap") {probe(argv[2],argv[3],false);return 0;}
        if(argc==4 && std::string(argv[1])=="--timed") {probe(argv[2],argv[3],true,true);return 0;}
        if(argc==5 && std::string(argv[1])=="--timed" && std::string(argv[2])=="--heap") {probe(argv[3],argv[4],false,true);return 0;}
        require(argc==1);contracts();queue_equivalence();boundary_reference();std::cout<<"Strict-radius expansion, stable collision lines, domain barriers, work bounds and cancellation passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
