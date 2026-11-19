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
    qpx_reader_files_api_v1 files{};
    explicit Module(const fs::path& path){
        library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(library!=nullptr,"Cannot load module: "+std::to_string(GetLastError()));
        entry=reinterpret_cast<qpx_reader_entry_fn_v1>(GetProcAddress(library,"qpx_reader_entry_v1"));
        check(entry!=nullptr,"Module omitted C ABI entry");api.struct_size=sizeof(api);Error error;
        status(entry(QPX_READER_ABI_V1,&api,&error.text),QPX_READER_OK,error,"entry");
        check(api.struct_size==sizeof(api)&&api.abi_version==1&&api.describe&&api.open&&api.destroy&&api.metadata&&api.level&&api.channel_name&&api.read_rgba&&api.read_raw,"Incomplete native table");
        const auto file_entry=reinterpret_cast<qpx_reader_files_entry_fn_v1>(GetProcAddress(library,"qpx_reader_files_entry_v1"));
        check(file_entry!=nullptr,"Module omitted file-set ABI entry");files.struct_size=sizeof(files);
        status(file_entry(1,&files,&error.text),QPX_READER_OK,error,"file-set entry");
        check(files.abi_version==1&&files.count&&files.path,"Incomplete file-set table");
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
struct Fixture {std::string name,vendor,order,options;std::int64_t width=0,height=0;unsigned planes=1,times=1,type=0,sample=0,flags=0,channels=1;double px=0,py=0,pz=0,seconds=0;};
std::vector<Fixture> fixtures(const fs::path& root){
    std::ifstream in(root/"fixtures.tsv");check(bool(in),"Missing authored NIfTI-1 fixture list");std::vector<Fixture> result;Fixture f;
    while(in>>f.name>>f.width>>f.height>>f.planes>>f.times>>f.type>>f.sample>>f.vendor>>f.order>>f.flags>>f.px>>f.py>>f.pz>>f.seconds>>f.options)result.push_back(f);
    check(in.eof()&&result.size()==46,"Incomplete NIfTI fixture list");return result;
}
std::vector<std::string> names(const Fixture& f){check(f.channels==1,"Scalar NIfTI-1 must expose one channel");return {"Scalar"};}
bool same_input(const qpx_reader_pixels_v1& before,qpx_reader_pixels_v1 after){after.written=before.written;after.validity_written=before.validity_written;return std::memcmp(&before,&after,sizeof(before))==0;}
void exact_region(Source& source,const Fixture& fixture,const std::vector<std::uint8_t>& oracle,bool raw,std::int64_t x,std::int64_t y,std::int64_t width,std::int64_t height,unsigned first=0,unsigned count=0,unsigned z=0,unsigned t=0){
    const unsigned sample=raw?fixture.sample:1,channels=raw?(count?count:fixture.channels):4;
    qpx_reader_region_v1 request{};request.struct_size=sizeof(request);request.x=x;request.y=y;request.width=width;request.height=height;request.first_channel=first;request.channels=raw?channels:0;request.z=raw?z:0;request.t=raw?t:0;
    const auto output_size=static_cast<std::size_t>(width*height*channels*sample);
    std::vector<std::uint8_t> output(output_size+16,0xcd),validity(static_cast<std::size_t>(width*height)+16,0xcd);
    qpx_reader_pixels_v1 pixels{};pixels.struct_size=sizeof(pixels);pixels.sample_type=raw?fixture.type:QPX_READER_UINT8;pixels.width=width;pixels.height=height;pixels.channels=channels;pixels.row_stride=width*channels*sample;pixels.pixel_stride=channels*sample;pixels.channel_stride=sample;pixels.data=output.data();pixels.capacity=output_size;
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
        if(inside){const auto stride=raw?fixture.channels:4u;const auto plane=raw?std::int64_t(t)*fixture.planes+z:0;const auto from=static_cast<std::size_t>(((plane*fixture.height+sy)*fixture.width+sx)*stride*sample+first*sample);const auto to=static_cast<std::size_t>((row*width+col)*channels*sample);std::copy_n(oracle.data()+from,channels*sample,expected.data()+to);}
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
    check(value(info.provider_id)=="org.qupathxx.nifti"&&value(info.version)=="1.5.0"&&value(info.contract)=="nifti-scalar-window/2","Provider identity/contract differs");
    check(value(info.display_name)=="NIfTI"&&info.abi_version==1&&info.capabilities==(QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW|QPX_READER_CAP_SOURCE_FILES),"Provider capabilities differ");
    std::array<char,8> guard{};guard.fill('X');info.provider_id={guard.data(),1,0};
    status(module.api.describe(&info,&error.text),QPX_READER_BUFFER_TOO_SMALL,error,"short descriptor text");
    check(info.provider_id.size==std::strlen("org.qupathxx.nifti")&&std::all_of(guard.begin()+1,guard.end(),[](auto v){return v=='X';}),"Descriptor short-buffer overrun/size");
    info.struct_size=4;status(module.api.describe(&info,&error.text),QPX_READER_INVALID_REQUEST,error,"short descriptor");
}
void metadata(Source& source,const Fixture& f){
    Error error;qpx_reader_metadata_v1 meta{};meta.struct_size=sizeof(meta);std::array<char,256> vendor{},order{};meta.vendor={vendor.data(),vendor.size(),0};meta.dimension_order={order.data(),order.size(),0};
    status(source.module.api.metadata(source.handle,&meta,{},&error.text),QPX_READER_OK,error,"metadata");
    check(meta.width==f.width&&meta.height==f.height&&meta.level_count==1&&meta.has_raw==1&&meta.channels==f.channels&&meta.sample_type==f.type&&meta.z_planes==f.planes&&meta.timepoints==f.times&&meta.series==0&&meta.series_count==1,"Metadata dimensions/axes/raw type differs");
    const auto calibration_matches=[](double actual,double expected){
        return std::abs(actual-expected)<=std::abs(expected)*2e-15;
    };
    check(meta.calibration_flags==f.flags && calibration_matches(meta.pixel_width_um,f.px) &&
          calibration_matches(meta.pixel_height_um,f.py) && calibration_matches(meta.z_spacing_um,f.pz) &&
          calibration_matches(meta.time_interval_seconds,f.seconds),"Physical calibration differs or was guessed");
    qpx_reader_level_v1 level{};level.struct_size=sizeof(level);status(source.module.api.level(source.handle,0,&level,&error.text),QPX_READER_OK,error,"level");check(level.width==f.width&&level.height==f.height&&level.downsample==1&&level.reserved==0,"Level-zero lattice differs");
    status(source.module.api.level(source.handle,1,&level,&error.text),QPX_READER_INVALID_REQUEST,error,"invalid level");level.struct_size=4;status(source.module.api.level(source.handle,0,&level,&error.text),QPX_READER_INVALID_REQUEST,error,"short level structure");
    check(std::string(vendor.data(),static_cast<std::size_t>(meta.vendor.size))==f.vendor&&std::string(order.data(),static_cast<std::size_t>(meta.dimension_order.size))==f.order,"Semantic vendor/dimension order differs");
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
    region.channels=f.channels;out.sample_type=f.type;out.channels=f.channels;out.channel_stride=f.sample;out.pixel_stride=f.channels*f.sample;out.row_stride=out.pixel_stride;out.capacity=out.pixel_stride;out.validity=valid.data();out.validity_capacity=1;
    auto before=out;Counter c;status(source.module.api.read_raw(source.handle,&region,&out,{cancelled,&c},&error.text),QPX_READER_CANCELLED,error,"cancel raw");check(same_input(before,out)&&out.written==0&&out.validity_written==0,"Cancelled raw read changed inputs or reports data");
    auto invalid_region=region;invalid_region.first_channel=f.channels;status(source.module.api.read_raw(source.handle,&invalid_region,&out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"raw channel range");
    invalid_region=region;invalid_region.z=f.planes;status(source.module.api.read_raw(source.handle,&invalid_region,&out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"raw Z range");invalid_region=region;invalid_region.t=f.times;status(source.module.api.read_raw(source.handle,&invalid_region,&out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"raw time range");
    auto invalid_out=out;invalid_out.validity_capacity=0;status(source.module.api.read_raw(source.handle,&region,&invalid_out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"raw validity capacity");
    invalid_out=out;invalid_out.sample_type=f.type==QPX_READER_UINT8?QPX_READER_UINT16:QPX_READER_UINT8;status(source.module.api.read_raw(source.handle,&region,&invalid_out,{},&error.text),QPX_READER_INVALID_REQUEST,error,"wrong raw sample type");
}
void invalid_open(Module& module,const fs::path& root){
    const auto source=fs::absolute(root/"i16-main.nii").u8string();qpx_reader_open_v1 request{};request.struct_size=sizeof(request);request.path={source.data(),source.size()};request.work_bytes=256ull*1024*1024;Error error;void* handle=nullptr;
    auto refuse=[&](qpx_reader_open_v1* r,qpx_reader_status_v1 expected,const char* label){handle=nullptr;status(module.api.open(r,&handle,&error.text),expected,error,label);check(handle==nullptr,"Failed open exposed handle");};
    status(module.api.open(&request,nullptr,&error.text),QPX_READER_INVALID_REQUEST,error,"null output handle");
    refuse(nullptr,QPX_READER_INVALID_REQUEST,"null open");auto bad=request;bad.struct_size=4;refuse(&bad,QPX_READER_INVALID_REQUEST,"short open structure");bad=request;bad.reserved=1;refuse(&bad,QPX_READER_INVALID_REQUEST,"reserved open field");bad=request;bad.work_bytes=0;refuse(&bad,QPX_READER_INVALID_REQUEST,"zero work budget");bad=request;bad.work_bytes++;refuse(&bad,QPX_READER_INVALID_REQUEST,"excess work budget");bad=request;bad.work_bytes=64;refuse(&bad,QPX_READER_INVALID_REQUEST,"bounded opening overhead");bad=request;const std::string relative="relative.nii";bad.path={relative.data(),relative.size()};refuse(&bad,QPX_READER_INVALID_REQUEST,"relative source");
    for(const std::string option:{"{}","null","{\"window\":[]}","{\"window\":[0,1,2]}","{\"window\":[1,1]}","{\"window\":[1,0]}","{\"window\":[0,NaN]}","{\"window\":[0,1e309]}","{\"window\":[-1e308,1e308]}","{ \"window\":[0,255]}","{\"window\":[+0,255]}","{\"window\":[01,255]}","{\"window\":[.5,255]}","{\"window\":[0,1.]}","{\"window\":[0,255],\"extra\":0}"}){bad=request;bad.options={option.data(),option.size()};refuse(&bad,QPX_READER_INVALID_REQUEST,"invalid window options");}
    const std::string oversized(513,'x');bad=request;bad.options={oversized.data(),oversized.size()};refuse(&bad,QPX_READER_INVALID_REQUEST,"window option byte bound");
    Counter cancelled_now;bad=request;bad.cancel={cancelled,&cancelled_now};refuse(&bad,QPX_READER_CANCELLED,"cancel before opening");
    const auto input=fs::absolute(root/"budget-input.nii.gz").u8string();
    Counter trace;trace.after=std::numeric_limits<int>::max();bad=request;bad.path={input.data(),input.size()};bad.cancel={cancelled,&trace};handle=nullptr;status(module.api.open(&bad,&handle,&error.text),QPX_READER_OK,error,"noncancelling opening checkpoint control");check(handle!=nullptr,"Controlled open omitted its handle");module.api.destroy(handle);handle=nullptr;check(trace.calls>1,"Open did not expose synchronous checkpoints");Counter during;during.after=trace.calls/2+1;bad=request;bad.path={input.data(),input.size()};bad.cancel={cancelled,&during};refuse(&bad,QPX_READER_CANCELLED,"controlled during-open cancellation");check(during.calls>=during.after,"Open did not reach controlled cancellation threshold");
    bad=request;bad.path={input.data(),input.size()};bad.work_bytes=96*1024;check(fs::file_size(fs::u8path(input))<bad.work_bytes,"Decoded-budget fixture must fit its compressed quota");refuse(&bad,QPX_READER_INVALID_REQUEST,"bounded owned raw input");
    std::ifstream in(root/"negative.tsv");check(bool(in),"Missing negative fixtures");std::string name;unsigned expected;unsigned count=0;
    while(in>>name>>expected){const auto path=(root/fs::u8path(name)).u8string();auto negative=request;negative.path={path.data(),path.size()};refuse(&negative,expected,("negative "+name).c_str());++count;std::cout<<"Refused "<<name<<" with status "<<expected<<'\n';}check(in.eof()&&count==84,"Incomplete negative fixture list");
}

void file_set(Module& module,const fs::path& root){
    Error error;qpx_reader_open_v1 request{};request.struct_size=sizeof(request);request.work_bytes=256ull*1024*1024;
    for(const auto& names:{std::pair{"i16-main.nii",""},std::pair{"i16-pair.hdr","i16-pair.img"},
                          std::pair{"f32-pair.hdr.gz","f32-pair.img.gz"},
                          std::pair{"v2-u64.nii",""},std::pair{"v2-i16.hdr","v2-i16.img"},
                          std::pair{"v2-f32.hdr.gz","v2-f32.img.gz"},
                          std::pair{"i16-mixed.hdr","i16-mixed.img.gz"},
                          std::pair{"f32-mixed.hdr.gz","f32-mixed.img"},
                          std::pair{"v2-i16-mixed.hdr","v2-i16-mixed.img.gz"},
                          std::pair{"v2-f32-mixed.hdr.gz","v2-f32-mixed.img"}}){
        const auto primary=(root/names.first).u8string();request.path={primary.data(),primary.size()};
        const bool paired=*names.second;std::uint32_t count=99;
        status(module.files.count(&request,&count,&error.text),QPX_READER_OK,error,"source count");
        check(count==static_cast<std::uint32_t>(paired),"Wrong companion count");
        std::array<char,32768> buffer{};qpx_reader_text_v1 path{buffer.data(),buffer.size(),0};
        status(module.files.path(&request,count,&path,&error.text),QPX_READER_INVALID_REQUEST,error,"invalid source index");
        if(paired){
            const auto expected=(root/names.second).u8string();path.capacity=1;
            status(module.files.path(&request,0,&path,&error.text),QPX_READER_BUFFER_TOO_SMALL,error,"short companion path");
            check(path.size==expected.size(),"File-set short path length differs");path.capacity=buffer.size();
            status(module.files.path(&request,0,&path,&error.text),QPX_READER_OK,error,"companion path");
            check(std::string(buffer.data(),static_cast<std::size_t>(path.size))==expected,"Companion path differs");
        }
    }
}
}
int run(int argc,char** argv){try{
    check(argc==3,"Usage: nifti_plugin_tests MODULE_DLL FIXTURES_DIR");Module module(fs::absolute(fs::u8path(argv[1])));const auto root=fs::absolute(fs::u8path(argv[2]));descriptor(module);file_set(module,root);
    const auto all=fixtures(root);std::size_t count=0;for(const auto& f:all){
        Source source(module,root/fs::u8path(f.name),f.options=="-"?"":f.options);metadata(source,f);const auto rgba=bytes(root/fs::u8path(f.name+".rgba")),raw=bytes(root/fs::u8path(f.name+".raw"));check(raw.size()==static_cast<std::size_t>(f.width*f.height*f.planes*f.times*f.sample)&&rgba.size()==static_cast<std::size_t>(f.width*f.height*4),"Invalid authored oracle byte lengths");
        for(unsigned t=0;t<f.times;++t)for(unsigned z=0;z<f.planes;++z){exact_region(source,f,raw,true,0,0,f.width,f.height,0,1,z,t);exact_region(source,f,raw,true,-2,-1,f.width+3,f.height+2,0,1,z,t);exact_region(source,f,raw,true,2,1,4,3,0,1,z,t);}
        exact_region(source,f,rgba,false,0,0,f.width,f.height);exact_region(source,f,rgba,false,-2,-1,f.width+3,f.height+2);exact_region(source,f,rgba,false,2,1,4,3);
        for(unsigned t=0;t<f.times;++t)for(unsigned z=0;z<f.planes;++z)exact_region(source,f,raw,true,0,0,f.width,f.height,0,1,z,t);invalid_reads(source,f);
        ++count;std::cout<<f.name<<": exact scalar type/all Z/raw bits before+after display/window/calibration/crop/padding and ABI failures passed\n";
    }
    const auto main=std::find_if(all.begin(),all.end(),[](const auto& f){return f.name=="i16-main.nii";});check(main!=all.end(),"Missing Unicode control fixture");auto unicode=*main;unicode.name=u8"\u03bc-\u56fe.nii.gz";Source unicode_source(module,root/fs::u8path(unicode.name));metadata(unicode_source,unicode);exact_region(unicode_source,unicode,bytes(root/"i16-main.nii.rgba"),false,0,0,unicode.width,unicode.height);for(unsigned z=0;z<unicode.planes;++z)exact_region(unicode_source,unicode,bytes(root/"i16-main.nii.raw"),true,0,0,unicode.width,unicode.height,0,1,z);
    check(bytes(root/"i16-main.nii.raw")==bytes(root/"i16-window.nii.raw"),"Window choice changed the authored raw source codes");invalid_open(module,root);std::cout<<"NIfTI-1 direct ABI passed "<<count<<" authored profiles, Unicode and exact refusal/cancellation/input-budget/buffer checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

int wmain(int argc,wchar_t** wide_argv){try{
    std::vector<std::string> owned;owned.reserve(static_cast<std::size_t>(argc));
    for(int i=0;i<argc;++i){
        const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);check(count>0,"Cannot encode Windows argument as UTF8");
        std::string value(static_cast<std::size_t>(count),'\0');check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,value.data(),count,nullptr,nullptr)==count,"Windows argument UTF8 conversion changed length");value.pop_back();owned.push_back(std::move(value));
    }
    std::vector<char*> argv;argv.reserve(owned.size()+1);for(auto& value:owned)argv.push_back(value.data());argv.push_back(nullptr);return run(argc,argv.data());
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
