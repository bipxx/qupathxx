// Repository-authored development checks. This executable is not shipped.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "qupathxx/reader_plugin.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs=std::filesystem;
namespace {
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Cannot read oracle: "+path.u8string());return {std::istreambuf_iterator<char>(in),{}};}
struct Error {
    std::array<char,1024> buffer{};
    qpx_reader_text_v1 text{buffer.data(),buffer.size(),0};
    std::string message() const {return {buffer.data(),static_cast<std::size_t>(std::min<std::uint64_t>(text.size,buffer.size()))};}
};
void status(qpx_reader_status_v1 actual,qpx_reader_status_v1 expected,const Error& error,const std::string& context){
    check(actual==expected,context+": status "+std::to_string(actual)+" expected "+std::to_string(expected)+" ("+error.message()+")");
    if(actual!=QPX_READER_OK)check(error.text.size>0,context+": missing error message");
}
struct Module {
    HMODULE library=nullptr;
    qpx_reader_entry_fn_v1 entry=nullptr;
    qpx_reader_api_v1 api{};
    explicit Module(const fs::path& path){
        library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(library!=nullptr,"Cannot load module: "+std::to_string(GetLastError()));
        entry=reinterpret_cast<qpx_reader_entry_fn_v1>(GetProcAddress(library,"qpx_reader_entry_v1"));
        check(entry!=nullptr,"Module omitted C ABI entry");api.struct_size=sizeof(api);Error error;
        status(entry(QPX_READER_ABI_V1,&api,&error.text),QPX_READER_OK,error,"entry");
        check(api.struct_size==sizeof(api)&&api.abi_version==1&&api.describe&&api.open&&api.destroy&&api.metadata&&api.level&&api.channel_name&&api.read_rgba&&api.read_raw,"Incomplete native table");
    }
    ~Module(){if(library)FreeLibrary(library);}
};
struct Source {
    Module& module;void* handle=nullptr;
    Source(Module& m,const fs::path& path,const std::string& options=""):module(m){
        const auto s=fs::absolute(path).u8string();qpx_reader_open_v1 request{};request.struct_size=sizeof(request);request.path={s.data(),s.size()};request.options={options.data(),options.size()};request.work_bytes=256ull*1024*1024;Error error;
        status(module.api.open(&request,&handle,&error.text),QPX_READER_OK,error,"open "+path.filename().u8string());check(handle!=nullptr,"Successful open returned null");
    }
    ~Source(){if(handle)module.api.destroy(handle);}
};
struct Fixture {std::string name,semantic;std::int64_t width=0,height=0;unsigned channels=0,flags=0;double px=0,py=0;};
std::vector<Fixture> fixtures(const fs::path& root){
    std::ifstream in(root/"fixtures.tsv");check(bool(in),"Missing authored BMP fixture list");std::vector<Fixture> result;Fixture f;
    while(in>>f.name>>f.width>>f.height>>f.channels>>f.semantic>>f.flags>>f.px>>f.py)result.push_back(f);
    check(in.eof()&&result.size()==62,"Incomplete BMP fixture list");return result;
}
std::vector<std::string> names(const Fixture& f){
    if(f.semantic=="index")return {"Palette index"};if(f.semantic=="rgb")return {"Red","Green","Blue"};throw std::runtime_error("Unknown authored BMP channel semantics");
}
bool same_input(const qpx_reader_pixels_v1& before,qpx_reader_pixels_v1 after){after.written=before.written;after.validity_written=before.validity_written;return std::memcmp(&before,&after,sizeof(before))==0;}
void exact_region(Source& source,const Fixture& fixture,const std::vector<std::uint8_t>& oracle,bool raw,std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,unsigned first=0,unsigned count=0){
    const unsigned sample=1,channels=raw?(count?count:fixture.channels):4;
    qpx_reader_region_v1 request{};request.struct_size=sizeof(request);request.x=x;request.y=y;request.width=width;request.height=height;request.first_channel=first;request.channels=raw?channels:0;
    const auto output_size=static_cast<std::size_t>(width*height*channels*sample);
    std::vector<std::uint8_t> output(output_size+16,0xcd),validity(static_cast<std::size_t>(width*height)+16,0xcd);
    qpx_reader_pixels_v1 pixels{};pixels.struct_size=sizeof(pixels);pixels.sample_type=QPX_READER_UINT8;pixels.width=width;pixels.height=height;pixels.channels=channels;pixels.row_stride=width*channels*sample;pixels.pixel_stride=channels*sample;pixels.channel_stride=sample;pixels.data=output.data();pixels.capacity=output_size;
    if(raw){pixels.validity=validity.data();pixels.validity_capacity=width*height;}
    const auto before=pixels;Error error;
    const auto result=raw?source.module.api.read_raw(source.handle,&request,&pixels,{},&error.text):source.module.api.read_rgba(source.handle,&request,&pixels,&error.text);
    status(result,QPX_READER_OK,error,fixture.name+(raw?" raw":" display"));
    check(same_input(before,pixels),"Plugin changed host pixel layout/pointers");
    check(pixels.written==output_size&&pixels.validity_written==(raw?static_cast<std::uint64_t>(width*height):0),"Exact ABI fill lengths differ");
    std::vector<std::uint8_t> expected(output_size,0);
    for(std::int64_t row=0;row<height;++row)for(std::int64_t col=0;col<width;++col){
        const auto sx=x+col,sy=y+row;const bool inside=sx>=0&&sy>=0&&sx<fixture.width&&sy<fixture.height;
        if(raw)check(validity[static_cast<std::size_t>(row*width+col)]==std::uint8_t(inside),"Raw validity lost in-bounds/OOB distinction");
        if(inside){const auto stride=raw?fixture.channels:4u;const auto from=static_cast<std::size_t>((sy*fixture.width+sx)*stride*sample+first*sample);const auto to=static_cast<std::size_t>((row*width+col)*channels*sample);std::copy_n(oracle.data()+from,channels*sample,expected.data()+to);}
    }
    check(std::equal(expected.begin(),expected.end(),output.begin()),fixture.name+": exact authored "+(raw?"source samples":"RGBA pixels")+" differ");
    check(std::all_of(output.begin()+output_size,output.end(),[](auto v){return v==0xcd;}),"Pixel output exceeded capacity");
    if(raw)check(std::all_of(validity.begin()+width*height,validity.end(),[](auto v){return v==0xcd;}),"Validity output exceeded capacity");
}
struct Counter {int calls=0,after=1;};
std::int32_t QPX_READER_CALL cancelled(void* value){auto& c=*static_cast<Counter*>(value);return ++c.calls>=c.after;}
void descriptor(Module& module){
    Error error;qpx_reader_api_v1 short_api{};short_api.struct_size=8;
    status(module.entry(1,&short_api,&error.text),QPX_READER_INVALID_REQUEST,error,"short API table");
    status(module.entry(99,&short_api,&error.text),QPX_READER_INVALID_REQUEST,error,"unknown ABI");
    status(module.entry(1,nullptr,&error.text),QPX_READER_INVALID_REQUEST,error,"null API table");
    qpx_reader_info_v1 info{};info.struct_size=sizeof(info);std::array<std::array<char,128>,4> buffers{};
    info.provider_id={buffers[0].data(),buffers[0].size(),0};info.display_name={buffers[1].data(),buffers[1].size(),0};info.version={buffers[2].data(),buffers[2].size(),0};info.contract={buffers[3].data(),buffers[3].size(),0};
    status(module.api.describe(&info,&error.text),QPX_READER_OK,error,"descriptor");
    auto value=[](const auto& t){return std::string(t.data,static_cast<std::size_t>(t.size));};
    check(value(info.provider_id)=="org.qupathxx.bmp"&&value(info.version)=="1.0.0"&&value(info.contract)=="bmp-infoheader-uint8-samples/1","Provider identity/contract differs");
    check(value(info.display_name)=="BMP"&&info.abi_version==1&&info.capabilities==(QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW),"Provider capabilities differ");
    std::array<char,8> guard{};guard.fill('X');info.provider_id={guard.data(),1,0};
    status(module.api.describe(&info,&error.text),QPX_READER_BUFFER_TOO_SMALL,error,"short descriptor text");
    check(info.provider_id.size==std::strlen("org.qupathxx.bmp")&&std::all_of(guard.begin()+1,guard.end(),[](auto v){return v=='X';}),"Descriptor short-buffer overrun/size");
    info.struct_size=4;status(module.api.describe(&info,&error.text),QPX_READER_INVALID_REQUEST,error,"short descriptor");
}
void metadata(Source& source,const Fixture& f){
    Error error;qpx_reader_metadata_v1 meta{};meta.struct_size=sizeof(meta);std::array<char,256> vendor{},order{};meta.vendor={vendor.data(),vendor.size(),0};meta.dimension_order={order.data(),order.size(),0};
    status(source.module.api.metadata(source.handle,&meta,{},&error.text),QPX_READER_OK,error,"metadata");
    check(meta.width==f.width&&meta.height==f.height&&meta.level_count==1&&meta.has_raw==1&&meta.channels==f.channels&&meta.sample_type==QPX_READER_UINT8&&meta.z_planes==1&&meta.timepoints==1&&meta.series==0&&meta.series_count==1,"Metadata dimensions/axes/raw type differs");
    check(meta.calibration_flags==f.flags&&meta.pixel_width_um==f.px&&meta.pixel_height_um==f.py&&meta.z_spacing_um==0&&meta.time_interval_seconds==0,"Physical calibration differs or was guessed");
    qpx_reader_level_v1 level{};level.struct_size=sizeof(level);status(source.module.api.level(source.handle,0,&level,&error.text),QPX_READER_OK,error,"level");check(level.width==f.width&&level.height==f.height&&level.downsample==1&&level.reserved==0,"Level-zero lattice differs");
    status(source.module.api.level(source.handle,1,&level,&error.text),QPX_READER_INVALID_REQUEST,error,"invalid level");level.struct_size=4;status(source.module.api.level(source.handle,0,&level,&error.text),QPX_READER_INVALID_REQUEST,error,"short level structure");
    check(std::string(vendor.data(),static_cast<std::size_t>(meta.vendor.size))=="BMP"&&std::string(order.data(),static_cast<std::size_t>(meta.dimension_order.size))=="XYCZT","Semantic vendor/dimension order differs");
    const auto expected=names(f);for(unsigned c=0;c<f.channels;++c){std::array<char,64> name{};qpx_reader_text_v1 text{name.data(),name.size(),0};status(source.module.api.channel_name(source.handle,c,&text,&error.text),QPX_READER_OK,error,"channel name");check(std::string(text.data,static_cast<std::size_t>(text.size))==expected[c],"Semantic channel name differs");}
    qpx_reader_text_v1 invalid{};status(source.module.api.channel_name(source.handle,f.channels,&invalid,&error.text),QPX_READER_INVALID_REQUEST,error,"channel range");
    std::array<char,8> guard{};guard.fill('X');qpx_reader_text_v1 short_name{guard.data(),1,0};status(source.module.api.channel_name(source.handle,0,&short_name,&error.text),QPX_READER_BUFFER_TOO_SMALL,error,"short channel text");check(short_name.size==expected[0].size()&&std::all_of(guard.begin()+1,guard.end(),[](auto value){return value=='X';}),"Short channel name overrun/size");
    Counter c;status(source.module.api.metadata(source.handle,&meta,{cancelled,&c},&error.text),QPX_READER_CANCELLED,error,"cancel metadata");
    status(source.module.api.metadata(nullptr,&meta,{},&error.text),QPX_READER_INVALID_REQUEST,error,"null metadata owner");
    const auto size=meta.struct_size;meta.struct_size=4;status(source.module.api.metadata(source.handle,&meta,{},&error.text),QPX_READER_INVALID_REQUEST,error,"short metadata structure");meta.struct_size=size;
}
void invalid_reads(Source& source,const Fixture& f){
    Error error;qpx_reader_region_v1 region{};region.struct_size=sizeof(region);region.width=1;region.height=1;std::array<std::uint8_t,64> storage{};std::array<std::uint8_t,16> valid{};
    qpx_reader_pixels_v1 out{};out.struct_size=sizeof(out);out.sample_type=QPX_READER_UINT8;out.width=1;out.height=1;out.channels=4;out.row_stride=4;out.pixel_stride=4;out.channel_stride=1;out.data=storage.data();out.capacity=4;
    auto expect_rgba=[&](const qpx_reader_region_v1* r,qpx_reader_pixels_v1* p,const char* context){auto before=p?*p:qpx_reader_pixels_v1{};status(source.module.api.read_rgba(source.handle,r,p,&error.text),QPX_READER_INVALID_REQUEST,error,context);if(p){check(same_input(before,*p),"Invalid display read changed host input");check(p->written==0&&p->validity_written==0,"Failed display read reported data");}};
    expect_rgba(nullptr,&out,"null request");expect_rgba(&region,nullptr,"null pixel output");
    auto short_region=region;short_region.struct_size=4;expect_rgba(&short_region,&out,"short region structure");auto short_out=out;short_out.struct_size=4;expect_rgba(&region,&short_out,"short pixel structure");
    auto bad=region;bad.level=1;expect_rgba(&bad,&out,"wrong region level");bad=region;bad.z=1;expect_rgba(&bad,&out,"wrong Z plane");bad=region;bad.t=1;expect_rgba(&bad,&out,"wrong time plane");bad=region;bad.first_channel=1;expect_rgba(&bad,&out,"quantitative RGBA channel");bad=region;bad.width=0;expect_rgba(&bad,&out,"zero width");bad=region;bad.x=std::numeric_limits<std::int64_t>::max();expect_rgba(&bad,&out,"coordinate overflow");
    auto p=out;p.capacity=3;expect_rgba(&region,&p,"short pixel buffer");p=out;p.row_stride=5;expect_rgba(&region,&p,"nonpacked row");p=out;p.validity=valid.data();p.validity_capacity=1;expect_rgba(&region,&p,"RGBA validity array");p=out;p.reserved=1;expect_rgba(&region,&p,"reserved pixel field");
    region.channels=f.channels;out.sample_type=QPX_READER_UINT8;out.channels=f.channels;out.channel_stride=1;out.pixel_stride=f.channels;out.row_stride=out.pixel_stride;out.capacity=out.pixel_stride;out.validity=valid.data();out.validity_capacity=1;
    auto before=out;Counter c;status(source.module.api.read_raw(source.handle,&region,&out,{cancelled,&c},&error.text),QPX_READER_CANCELLED,error,"cancel raw");check(same_input(before,out)&&out.written==0&&out.validity_written==0,"Cancelled raw read changed inputs or reports data");
    auto invalid_region=region;invalid_region.first_channel=f.channels;status(source.module.api.read_raw(source.handle,&invalid_region,&out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"raw channel range");
    auto invalid_out=out;invalid_out.validity_capacity=0;status(source.module.api.read_raw(source.handle,&region,&invalid_out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"raw validity capacity");
    invalid_out=out;invalid_out.sample_type=QPX_READER_UINT16;status(source.module.api.read_raw(source.handle,&region,&invalid_out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"wrong raw sample type");
}
void invalid_open(Module& module,const fs::path& root){
    const auto source=fs::absolute(root/"rgb-main.bmp").u8string();qpx_reader_open_v1 request{};request.struct_size=sizeof(request);request.path={source.data(),source.size()};request.work_bytes=256ull*1024*1024;Error error;void* handle=nullptr;
    auto refuse=[&](qpx_reader_open_v1* r,qpx_reader_status_v1 expected,const char* label){handle=nullptr;status(module.api.open(r,&handle,&error.text),expected,error,label);check(handle==nullptr,"Failed open exposed handle");};
    status(module.api.open(&request,nullptr,&error.text),QPX_READER_INVALID_REQUEST,error,"null output handle");
    refuse(nullptr,QPX_READER_INVALID_REQUEST,"null open");auto bad=request;bad.struct_size=4;refuse(&bad,QPX_READER_INVALID_REQUEST,"short open structure");bad=request;bad.reserved=1;refuse(&bad,QPX_READER_INVALID_REQUEST,"reserved open field");bad=request;bad.work_bytes=0;refuse(&bad,QPX_READER_INVALID_REQUEST,"zero work budget");bad=request;bad.work_bytes++;refuse(&bad,QPX_READER_INVALID_REQUEST,"excess work budget");bad=request;bad.work_bytes=64;refuse(&bad,QPX_READER_INVALID_REQUEST,"bounded allocation");bad=request;const std::string relative="relative.bmp";bad.path={relative.data(),relative.size()};refuse(&bad,QPX_READER_INVALID_REQUEST,"relative source");
    for(const std::string option:{"{}","{\"unknown\":1}","null"}){bad=request;bad.options={option.data(),option.size()};refuse(&bad,QPX_READER_INVALID_REQUEST,"unsupported nonempty options");}
    Counter cancelled_now;bad=request;bad.cancel={cancelled,&cancelled_now};refuse(&bad,QPX_READER_CANCELLED,"cancel before decode");
    const auto raster=fs::absolute(root/"budget-raster.bmp").u8string();
    {Fixture control{"budget-raster","index",256,256,1,0,0,0};Source accepted(module,fs::u8path(raster));metadata(accepted,control);exact_region(accepted,control,std::vector<std::uint8_t>(256*256,0),true,0,0,1,1);std::vector<std::uint8_t> rgba(256*256*4,0);for(std::size_t i=3;i<rgba.size();i+=4)rgba[i]=255;exact_region(accepted,control,rgba,false,0,0,1,1);}
    Counter during;during.after=3;bad=request;bad.path={raster.data(),raster.size()};bad.cancel={cancelled,&during};refuse(&bad,QPX_READER_CANCELLED,"cancel during work");check(during.calls>=3,"Open did not inspect cancellation during work");
    bad=request;bad.path={raster.data(),raster.size()};bad.work_bytes=32768;
    check(fs::file_size(fs::u8path(raster))+8192+8*raster.size()<bad.work_bytes&&256ull*256>bad.work_bytes,"Budget fixture does not leave a conservative encoded/wrapper margin before decoded expansion");
    refuse(&bad,QPX_READER_INVALID_REQUEST,"bounded decoded allocation after encoded input");check(error.message()=="BMP decoded data exceeds reader work budget","Budget refusal did not reach owned decoded allocation: "+error.message());
    std::ifstream in(root/"negative.tsv");check(bool(in),"Missing negative fixtures");std::string name;unsigned expected;unsigned count=0;
    while(in>>name>>expected){const auto path=fs::absolute(root/(name+".bmp")).u8string();auto negative=request;negative.path={path.data(),path.size()};refuse(&negative,expected,("negative "+name).c_str());++count;std::cout<<"Refused "<<name<<" with status "<<expected<<'\n';}check(in.eof()&&count==40,"Incomplete negative fixture list");
}

}
int run(int argc,char** argv){try{
    check(argc==3,"Usage: bmp_plugin_tests MODULE_DLL FIXTURES_DIR");Module module(fs::absolute(fs::u8path(argv[1])));const auto root=fs::absolute(fs::u8path(argv[2]));descriptor(module);
    const auto all=fixtures(root);std::size_t count=0;for(const auto& f:all){
        Source source(module,root/fs::u8path(f.name+".bmp"));metadata(source,f);const auto rgba=bytes(root/fs::u8path(f.name+".rgba")),raw=bytes(root/fs::u8path(f.name+".raw"));check(raw.size()==static_cast<std::size_t>(f.width*f.height*f.channels)&&rgba.size()==static_cast<std::size_t>(f.width*f.height*4),"Invalid authored oracle byte lengths");
        exact_region(source,f,rgba,false,0,0,f.width,f.height);exact_region(source,f,rgba,false,-2,-1,f.width+3,f.height+2);exact_region(source,f,rgba,false,2,1,4,3);
        exact_region(source,f,raw,true,0,0,f.width,f.height);exact_region(source,f,raw,true,-2,-1,f.width+3,f.height+2);exact_region(source,f,raw,true,2,1,4,3,f.channels>1?1:0,f.channels>1?f.channels-1:1);invalid_reads(source,f);
        ++count;std::cout<<f.name<<": exact metadata/raw/RGBA/crop/padding/channel slice and ABI failures passed\n";
    }
    const auto opaque=std::find_if(all.begin(),all.end(),[](const auto& f){return f.name=="rgb-main";});check(opaque!=all.end(),"Missing Unicode control fixture");
    auto unicode=*opaque;unicode.name=u8"BMP-\u03bc-\u56fe";Source unicode_source(module,root/fs::u8path(unicode.name+".bmp"));metadata(unicode_source,unicode);exact_region(unicode_source,unicode,bytes(root/"rgb-main.rgba"),false,0,0,unicode.width,unicode.height);exact_region(unicode_source,unicode,bytes(root/"rgb-main.raw"),true,0,0,unicode.width,unicode.height);
    invalid_open(module,root);std::cout<<"BMP direct ABI passed "<<count<<" authored profiles, Unicode and exact refusal/cancellation/work/buffer checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

int wmain(int argc,wchar_t** wide_argv){try{
    std::vector<std::string> owned;owned.reserve(static_cast<std::size_t>(argc));
    for(int i=0;i<argc;++i){
        const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);check(count>0,"Cannot encode Windows argument as UTF8");
        std::string value(static_cast<std::size_t>(count),'\0');check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,value.data(),count,nullptr,nullptr)==count,"Windows argument UTF8 conversion changed length");value.pop_back();owned.push_back(std::move(value));
    }
    std::vector<char*> argv;argv.reserve(owned.size()+1);for(auto& value:owned)argv.push_back(value.data());argv.push_back(nullptr);return run(argc,argv.data());
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
