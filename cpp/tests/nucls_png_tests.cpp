#include "../nucls_png.hpp"
#include "qupathxx/project.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <cmath>
using namespace qpx;
using namespace qpx::detail;
namespace fs=std::filesystem;
void checked(bool v,int line){if(!v)throw std::runtime_error("NuCLS PNG failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E,class F>void rejects_at(F action,int line){bool hit=false;try{action();}catch(const E&){hit=true;}checked(hit,line);}
#define rejects(E,...) rejects_at<E>(__VA_ARGS__,__LINE__)
void u32(std::string& out,std::uint32_t value){for(int n=24;n>=0;n-=8)out+=char(value>>n);}
std::uint32_t crc(const std::string& data){std::uint32_t value=0xffffffff;for(unsigned char c:data){value^=c;for(int i=0;i<8;++i)value=(value>>1)^((value&1)?0xedb88320u:0);}return value^0xffffffff;}
std::string chunk(const std::string& type,const std::string& data){std::string out;u32(out,static_cast<std::uint32_t>(data.size()));out+=type;out+=data;u32(out,crc(type+data));return out;}
std::string stored(const std::string& raw){check(raw.size()<65536);std::string out="\x78\x01";out+=char(1);const auto n=unsigned(raw.size());out+=char(n);out+=char(n>>8);out+=char(~n);out+=char((~n)>>8);out+=raw;unsigned a=1,b=0;for(unsigned char c:raw){a=(a+c)%65521;b=(b+a)%65521;}u32(out,(b<<16)|a);return out;}
std::string png(unsigned width,unsigned height,const std::string& raw,unsigned color=2,unsigned depth=8,bool interlace=false,bool split=false) {
    std::string out("\x89PNG\r\n\x1a\n",8),header;u32(header,width);u32(header,height);header+=char(depth);header+=char(color);header+=char(0);header+=char(0);header+=char(interlace);
    out+=chunk("IHDR",header);const auto compressed=stored(raw);
    if(split){out+=chunk("IDAT",compressed.substr(0,5));out+=chunk("IDAT",compressed.substr(5));}else out+=chunk("IDAT",compressed);
    return out+chunk("IEND","");
}
void write(const fs::path& path,const std::string& data){std::ofstream file(path,std::ios::binary);file.write(data.data(),data.size());if(!file)throw std::runtime_error("Fixture write failed");}
std::string filtered(const std::vector<std::uint8_t>& pixels,unsigned width,unsigned height,unsigned filter) {
    std::string out;const auto stride=3*width;
    for(unsigned y=0;y<height;++y){out+=char(filter);for(unsigned x=0;x<stride;++x){
        const int value=pixels[y*stride+x],left=x>=3?pixels[y*stride+x-3]:0,above=y?pixels[(y-1)*stride+x]:0,diagonal=y && x>=3?pixels[(y-1)*stride+x-3]:0;
        int predictor=0;if(filter==1)predictor=left;if(filter==2)predictor=above;if(filter==3)predictor=(left+above)/2;
        if(filter==4){const auto p=left+above-diagonal,pa=std::abs(p-left),pb=std::abs(p-above),pc=std::abs(p-diagonal);predictor=pa<=pb && pa<=pc?left:pb<=pc?above:diagonal;}
        out+=char(value-predictor);
    }}return out;
}
std::string adam7(const std::vector<std::uint8_t>& pixels,unsigned width,unsigned height) {
    std::string out;constexpr std::array<std::array<unsigned,4>,7> passes={{{0,0,8,8},{4,0,8,8},{0,4,4,8},{2,0,4,4},{0,2,2,4},{1,0,2,2},{0,1,1,2}}};
    for(const auto& p:passes)if(p[0]<width)for(unsigned y=p[1];y<height;y+=p[3]){out+=char(0);for(unsigned x=p[0];x<width;x+=p[2])for(unsigned c=0;c<3;++c)out+=char(pixels[3*(y*width+x)+c]);}
    return out;
}
int main(){try{
    Project id;const auto root=fs::current_path()/(id.identity()+"-nucls-png");fs::create_directory(root);
    const auto path=root/fs::u8path(u8"\u6848\u4f8b.png");
    const std::vector<std::uint8_t> pixels={0,1,255,128,64,32,7,99,201,250,240,230,200,150,100,13,17,19};
    std::string original;
    for(unsigned filter=0;filter<5;++filter) {
        original=png(3,2,filtered(pixels,3,2,filter),2,8,false,filter==4);write(path,original);
        WorkflowInput source(path.u8string(),32u*1024*1024);check(source.bytes()==original.size() && read_workflow_input(source)==original);
        const auto decoded=decode_nucls_rgb_png(source);check(decoded.width==3 && decoded.height==2 && decoded.pixels==pixels);source.verify(true);
    }
    original=png(3,2,adam7(pixels,3,2),2,8,true);write(path,original);
    {WorkflowInput source(path.u8string(),32u*1024*1024);check(decode_nucls_rgb_png(source).pixels==pixels);}
    original=png(3,2,filtered(pixels,3,2,0));const auto raw=filtered(pixels,3,2,0);
    const auto check_bad=[&](const std::string& bytes){write(path,bytes);WorkflowInput source(path.u8string(),32u*1024*1024);rejects(std::invalid_argument,[&]{decode_nucls_rgb_png(source);});};
    for(unsigned color:{0u,3u,4u,6u})check_bad(png(3,2,raw,color));
    check_bad(png(3,2,raw,2,16));check_bad(std::string(100,'x'));check_bad(original.substr(0,20));check_bad(original.substr(0,original.size()-12));
    auto bad_crc=original;bad_crc[bad_crc.size()-1]^=1;check_bad(bad_crc);
    auto metadata=original;auto text=chunk("tEXt",std::string("key\0value",9));text.back()^=1;metadata.insert(metadata.size()-12,text);check_bad(metadata);
    auto unknown=original;unknown.insert(unknown.size()-12,chunk("ABCD","critical"));check_bad(unknown);
    auto too_long=original;for(unsigned i=33;i<37;++i)too_long[i]=char(255);check_bad(too_long);
    check_bad(png(3,2,std::string(100,'x')));
    write(path,original);WorkflowInput source(path.u8string(),32u*1024*1024);
    rejects(NuclsPixelLimit,[&]{decode_nucls_rgb_png(source,5);});
    std::atomic<bool> cancelled{true};rejects(std::runtime_error,[&]{decode_nucls_rgb_png(source,16000000,&cancelled);});
    rejects(std::invalid_argument,[&]{WorkflowInput too_small(path.u8string(),1);});
    write(path,original+"trailing source bytes");rejects(WorkflowInputChanged,[&]{decode_nucls_rgb_png(source);});
    {WorkflowInput trailing(path.u8string(),32u*1024*1024);check(decode_nucls_rgb_png(trailing).pixels==pixels);}
    write(path,png(5000,5000,raw));{WorkflowInput huge(path.u8string(),32u*1024*1024);rejects(NuclsPixelLimit,[&]{decode_nucls_rgb_png(huge);});}
    {std::ofstream large(root/"oversized.png",std::ios::binary);large.seekp(32u*1024*1024);large.put(' ');}
    WorkflowInput oversized((root/"oversized.png").u8string(),64u*1024*1024);rejects(std::length_error,[&]{decode_nucls_rgb_png(oversized);});
    std::cout<<"NuCLS PNG: exact RGB8/filter/Adam7 pixels, no color conversion, CRC/chunk/deflate validation, bounded allocation, cancellation and source guards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
