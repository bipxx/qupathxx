#include "../nucls_png.hpp"
#include "nucls_png_fixture.hpp"
using namespace nucls_png_fixture;
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
void write(const fs::path& path,const std::string& data){std::ofstream file(path,std::ios::binary);file.write(data.data(),data.size());if(!file)throw std::runtime_error("Fixture write failed");}
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
