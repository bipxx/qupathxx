// Direct C ABI plus independent native trust-boundary checks. The real parsers
// run in their DLLs; literal expected arrays are authored by the fixture control.
#include "qupathxx/format_plugin.h"
#include "qupathxx/format_plugins.hpp"
#include "qupathxx/workflow.hpp"
#include "../format_plugin_runtime.hpp"
#include "../format_family_catalog.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
void check(bool x,const std::string& m){if(!x)throw std::runtime_error(m);}
template<class E=std::exception,class F>std::string refuses(F&& f){try{f();}catch(const E& e){return e.what();}throw std::runtime_error("Missing native boundary refusal");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"Read authored fixture");return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& p,const std::string& text){std::ofstream out(p,std::ios::binary);out<<text;out.close();check(bool(out),"Write owned boundary control");}
struct Error {std::array<char,8192> data{};qpx_reader_text_v1 text{data.data(),data.size(),0};std::string message()const{return {data.data(),static_cast<std::size_t>(std::min<std::uint64_t>(text.size,data.size()))};}};
void status(qpx_reader_status_v1 actual,qpx_reader_status_v1 expected,const Error& e,const std::string& label){check(actual==expected,label+": status "+std::to_string(actual)+" expected "+std::to_string(expected)+"; "+e.message());}
int32_t QPX_READER_CALL yes(void*){return 1;}
const qpx_reader_cancel_v1 cancelled{yes,nullptr};
struct Module {
#ifdef _WIN32
    HMODULE value=nullptr;
    explicit Module(const fs::path& path):value(LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)){check(value!=nullptr,"Load exact test DLL");}
    ~Module(){if(value)FreeLibrary(value);}
    void* symbol(const char* name){auto p=GetProcAddress(value,name);check(p!=nullptr,std::string("Missing DLL entry ")+name);return reinterpret_cast<void*>(p);}
#else
    void* value=nullptr;
    explicit Module(const fs::path& path):value(dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL)){check(value!=nullptr,"Load exact test module");}
    ~Module(){if(value)dlclose(value);}
    void* symbol(const char* name){auto p=dlsym(value,name);check(p!=nullptr,std::string("Missing entry ")+name);return p;}
#endif
};
template<class Api>struct Handle {const Api& api;void* value=nullptr;explicit Handle(const Api& a):api(a){}~Handle(){if(value)api.destroy(value);}};
qpx_reader_open_v1 request(const std::string& path,const std::string& options={},std::uint64_t budget=64u*1024*1024){return {sizeof(qpx_reader_open_v1),0,{path.data(),path.size()},{options.data(),options.size()},budget,{}};}
template<class Api>void open_good(const Api& api,Handle<Api>& h,const fs::path& path){const auto text=fs::absolute(path).u8string();auto r=request(text);const auto before=r;Error e;status(api.open(&r,&h.value,&e.text),0,e,"Open "+path.filename().u8string());check(h.value&&std::memcmp(&r,&before,sizeof(r))==0,"Open mutated input or returned no handle");}
template<class Api>void descriptor(const Api& api,bool matrix){
    Error e;qpx_format_info_v1 out{};out.struct_size=sizeof(out);
    std::array<std::array<char,512>,6> text;for(auto& t:text)t.fill('!');
    qpx_reader_text_v1* fields[]={&out.provider_id,&out.format_id,&out.display_name,&out.version,&out.contract,&out.profile};
    for(unsigned i=0;i<6;++i)*fields[i]={text[i].data()+1,0,0};
    status(api.describe(&out,&e.text),6,e,"Descriptor capacity probe");
    for(unsigned i=0;i<6;++i){check(fields[i]->size>0&&fields[i]->size<510&&std::all_of(text[i].begin(),text[i].end(),[](char c){return c=='!';}),"Descriptor probe wrote payload or invalid length");fields[i]->capacity=510;}
    status(api.describe(&out,&e.text),0,e,"Descriptor full");
    const std::array<std::string,6> expected={matrix?"org.qupathxx.counts-tsv":"org.qupathxx.point-tsv",matrix?"counts-tsv":"point-tsv",matrix?"Dense counts TSV":"Pixel point TSV","1.0.0",matrix?"dense-observation-feature-uint64-tsv/1":"pixel-point-annotation-tsv/1","windows-x64-msvc1944-release-md"};
    check(out.struct_size==sizeof(out)&&out.abi_version==1&&out.domain==(matrix?1u:2u)&&!out.reserved,"Descriptor shape changed");
    for(unsigned i=0;i<6;++i)check(fields[i]->data==text[i].data()+1&&fields[i]->capacity==510&&std::string(fields[i]->data,static_cast<std::size_t>(fields[i]->size))==expected[i]&&text[i].front()=='!'&&text[i].back()=='!',"Exact descriptor identity/canary differs");
    status(api.describe(nullptr,&e.text),3,e,"Null descriptor");out.struct_size=4;status(api.describe(&out,&e.text),3,e,"Short descriptor");
}
template<class Api>Api table(Module& m,const char* name){
    Error e;auto entry=reinterpret_cast<qpx_reader_status_v1(QPX_FORMAT_CALL*)(uint32_t,Api*,qpx_reader_text_v1*)>(m.symbol(name));
    Api api{};api.struct_size=sizeof(api);status(entry(2,&api,&e.text),3,e,"Unknown ABI");status(entry(1,nullptr,&e.text),3,e,"Null ABI output");
    api.struct_size=4;status(entry(1,&api,&e.text),3,e,"Short API table");api.struct_size=sizeof(api);status(entry(1,&api,&e.text),0,e,"Full API table");
    check(api.struct_size==sizeof(api)&&api.abi_version==1&&api.describe&&api.open&&api.destroy&&api.metadata,"API table invalid");return api;
}
std::string span(const qpx_reader_text_v1& text,std::uint64_t offset,std::uint64_t length){check(text.size<=text.capacity&&offset<=text.size&&length<=text.size-offset,"Returned text span exceeds bytes");return {text.data+offset,static_cast<std::size_t>(length)};}
void matrix(const qpx_matrix_format_api_v1& api,const fs::path& path,const Json& expected){
    Handle<qpx_matrix_format_api_v1> h(api);open_good(api,h,path);Error e;
    const auto features=expected.at("features").get<std::vector<std::string>>(),observations=expected.at("observations").get<std::vector<std::string>>();
    const auto values=expected.at("values").get<std::vector<std::uint64_t>>();
    qpx_matrix_metadata_v1 meta{};meta.struct_size=sizeof(meta);status(api.metadata(h.value,&meta,{},&e.text),0,e,"Matrix metadata");
    check(meta.struct_size==sizeof(meta)&&meta.value_type==1&&meta.features==features.size()&&meta.observations==observations.size()&&!meta.reserved[0]&&!meta.reserved[1],"Exact matrix shape/type differs");
    status(api.metadata(h.value,&meta,cancelled,&e.text),1,e,"Matrix metadata cancellation");status(api.metadata(nullptr,&meta,{},&e.text),3,e,"Null matrix handle");status(api.metadata(h.value,nullptr,{},&e.text),3,e,"Null matrix metadata");
    for(unsigned axis=1;axis<=2;++axis){const auto& ids=axis==1?features:observations;
        for(std::uint64_t first=0;first<=ids.size();++first){
            const auto n=std::min<std::uint64_t>(2,ids.size()-first);qpx_matrix_axis_request_v1 r{sizeof(r),axis,first,2};const auto before=r;
            std::array<qpx_matrix_axis_row_v1,4> rows;std::memset(rows.data(),0xa5,sizeof(rows));const auto rows_before=rows;
            std::vector<char> text(32770,'!');qpx_matrix_axis_buffer_v1 out{sizeof(out),0,rows.data()+1,0,0,{text.data()+1,0,0}};
            const auto probe=api.axis(h.value,&r,&out,{},&e.text);status(probe,n?6:0,e,"Axis capacity probe");
            check(out.written==n&&std::memcmp(rows.data(),rows_before.data(),sizeof(rows))==0&&std::all_of(text.begin(),text.end(),[](char c){return c=='!';}),"Axis capacity probe wrote payload or wrong count");
            if(n){const auto required=out.text.size;out.capacity=2;out.text.capacity=required-1;status(api.axis(h.value,&r,&out,{},&e.text),6,e,"Axis short text only");check(out.written==n&&out.text.size==required&&std::memcmp(rows.data(),rows_before.data(),sizeof(rows))==0&&std::all_of(text.begin(),text.end(),[](char c){return c=='!';}),"Short axis text wrote payload or wrong required lengths");out.capacity=n-1;out.text.capacity=text.size()-2;status(api.axis(h.value,&r,&out,{},&e.text),6,e,"Axis short rows only");check(std::memcmp(rows.data(),rows_before.data(),sizeof(rows))==0&&std::all_of(text.begin(),text.end(),[](char c){return c=='!';}),"Short axis rows wrote payload");}
            out.capacity=2;out.text.capacity=text.size()-2;status(api.axis(h.value,&r,&out,{},&e.text),0,e,"Axis page");
            check(std::memcmp(&r,&before,sizeof(r))==0&&out.struct_size==sizeof(out)&&!out.reserved&&out.rows==rows.data()+1&&out.capacity==2&&out.text.data==text.data()+1&&out.text.capacity==text.size()-2&&out.text.size<=out.text.capacity&&out.written==n,"Axis input/output layout mutated");
            for(std::uint64_t i=0;i<n;++i){const auto& row=rows[static_cast<std::size_t>(i)+1];check(row.index==first+i&&span(out.text,row.id_offset,row.id_size)==ids[static_cast<std::size_t>(first+i)],"Exact axis ID/index differs");check(span(out.text,row.name_offset,row.name_size).empty(),"Counts TSV unexpectedly inferred display names");}
            check(std::memcmp(&rows.front(),&rows_before.front(),sizeof(rows.front()))==0&&std::memcmp(&rows[static_cast<std::size_t>(n)+1],&rows_before[static_cast<std::size_t>(n)+1],sizeof(rows.front()))==0&&text.front()=='!'&&text[static_cast<std::size_t>(out.text.size)+1]=='!'&&text.back()=='!',"Axis canary overwritten");
        }
    }
    for(std::uint64_t first=0;first<=values.size();++first){
        const auto n=std::min<std::uint64_t>(4,values.size()-first);qpx_matrix_values_request_v1 r{sizeof(r),0,first,4};const auto before=r;
        std::array<std::uint64_t,6> data;data.fill(UINT64_C(0xabadc0de12345678));const auto canary=data;
        qpx_matrix_values_buffer_v1 out{sizeof(out),0,data.data()+1,0,0};status(api.values(h.value,&r,&out,{},&e.text),n?6:0,e,"Value capacity probe");check(data==canary&&out.written==n,"Value capacity probe writes or length");
        out.capacity=4;status(api.values(h.value,&r,&out,{},&e.text),0,e,"Dense exact UInt64 page");check(std::memcmp(&r,&before,sizeof(r))==0&&out.struct_size==sizeof(out)&&!out.reserved&&out.values==data.data()+1&&out.capacity==4&&out.written==n,"Value input fields mutated");
        for(std::uint64_t i=0;i<n;++i)check(data[static_cast<std::size_t>(i)+1]==values[static_cast<std::size_t>(first+i)],"UInt64 changed, lost precision or wrong dense order");check(data.front()==canary.front()&&data[static_cast<std::size_t>(n)+1]==canary[static_cast<std::size_t>(n)+1]&&data.back()==canary.back(),"Values canary overwritten");
    }
    qpx_matrix_values_request_v1 r{sizeof(r),0,0,1};std::uint64_t v=99;qpx_matrix_values_buffer_v1 out{sizeof(out),0,&v,1,0};
    status(api.values(h.value,&r,&out,cancelled,&e.text),1,e,"Values cancellation");check(v==99,"Cancelled values wrote payload");
    status(api.values(h.value,nullptr,&out,{},&e.text),3,e,"Null values request");status(api.values(h.value,&r,nullptr,{},&e.text),3,e,"Null values output");
    r.reserved=1;status(api.values(h.value,&r,&out,{},&e.text),3,e,"Reserved values request");r.reserved=0;r.count=0;status(api.values(h.value,&r,&out,{},&e.text),3,e,"Zero values count");r.count=1;r.first=values.size()+1;status(api.values(h.value,&r,&out,{},&e.text),3,e,"Values beyond end");
    qpx_matrix_axis_request_v1 ar{sizeof(ar),99,0,1};qpx_matrix_axis_buffer_v1 ao{};ao.struct_size=sizeof(ao);status(api.axis(h.value,&ar,&ao,{},&e.text),3,e,"Unknown axis");ar.axis=1;status(api.axis(h.value,&ar,&ao,cancelled,&e.text),1,e,"Axis cancellation");
}
void annotation(const qpx_annotation_format_api_v1& api,const fs::path& path,const Json& expected){
    Handle<qpx_annotation_format_api_v1> h(api);open_good(api,h,path);Error e;const auto& points=expected.at("points");
    qpx_annotation_metadata_v1 meta{};meta.struct_size=sizeof(meta);status(api.metadata(h.value,&meta,{},&e.text),0,e,"Point metadata");check(meta.struct_size==sizeof(meta)&&meta.coordinate_frame==1&&meta.points==points.size()&&!meta.reserved[0]&&!meta.reserved[1],"Exact point shape/frame differs");
    status(api.metadata(h.value,&meta,cancelled,&e.text),1,e,"Point metadata cancellation");status(api.metadata(h.value,nullptr,{},&e.text),3,e,"Null point metadata");status(api.metadata(nullptr,&meta,{},&e.text),3,e,"Null point handle");
    for(std::uint64_t first=0;first<=points.size();++first){
        const auto n=std::min<std::uint64_t>(2,points.size()-first);qpx_annotation_points_request_v1 r{sizeof(r),0,first,2};const auto before=r;
        std::array<qpx_annotation_point_v1,4> data;std::memset(data.data(),0xa5,sizeof(data));const auto canary=data;std::vector<char> text(32770,'!');
        qpx_annotation_points_buffer_v1 out{sizeof(out),0,data.data()+1,0,0,{text.data()+1,0,0}};status(api.points(h.value,&r,&out,{},&e.text),n?6:0,e,"Point capacity probe");check(out.written==n&&std::memcmp(data.data(),canary.data(),sizeof(data))==0&&std::all_of(text.begin(),text.end(),[](char c){return c=='!';}),"Point capacity probe wrote payload");
        if(n){const auto required=out.text.size;out.capacity=2;out.text.capacity=required-1;status(api.points(h.value,&r,&out,{},&e.text),6,e,"Point short text only");check(out.written==n&&out.text.size==required&&std::memcmp(data.data(),canary.data(),sizeof(data))==0&&std::all_of(text.begin(),text.end(),[](char c){return c=='!';}),"Short point text wrote payload or wrong required lengths");out.capacity=n-1;out.text.capacity=text.size()-2;status(api.points(h.value,&r,&out,{},&e.text),6,e,"Point short rows only");check(std::memcmp(data.data(),canary.data(),sizeof(data))==0&&std::all_of(text.begin(),text.end(),[](char c){return c=='!';}),"Short point rows wrote payload");}
        out.capacity=2;out.text.capacity=text.size()-2;status(api.points(h.value,&r,&out,{},&e.text),0,e,"Point page");check(std::memcmp(&r,&before,sizeof(r))==0&&out.struct_size==sizeof(out)&&!out.reserved&&out.points==data.data()+1&&out.capacity==2&&out.text.data==text.data()+1&&out.text.capacity==text.size()-2&&out.text.size<=out.text.capacity&&out.written==n,"Point pointer/capacity/request mutated");
        for(std::uint64_t i=0;i<n;++i){const auto& a=data[static_cast<std::size_t>(i)+1];const auto& b=points.at(static_cast<std::size_t>(first+i));check(a.x==b.at("x").get<double>()&&a.y==b.at("y").get<double>()&&a.c==b.at("c").get<int>()&&a.z==b.at("z").get<int>()&&a.t==b.at("t").get<int>()&&!a.reserved&&span(out.text,a.source_id_offset,a.source_id_size)==b.at("id").get<std::string>()&&span(out.text,a.class_offset,a.class_size)==b.at("class").get<std::string>(),"Exact point identity/coordinates/plane/class differs");}
        check(std::memcmp(&data.front(),&canary.front(),sizeof(data.front()))==0&&std::memcmp(&data[static_cast<std::size_t>(n)+1],&canary[static_cast<std::size_t>(n)+1],sizeof(data.front()))==0&&text.front()=='!'&&text[static_cast<std::size_t>(out.text.size)+1]=='!'&&text.back()=='!',"Point canary overwritten");
    }
    qpx_annotation_points_request_v1 r{sizeof(r),0,0,1};qpx_annotation_point_v1 value{};value.x=123;qpx_annotation_points_buffer_v1 out{sizeof(out),0,&value,1,0,{}};
    status(api.points(h.value,&r,&out,cancelled,&e.text),1,e,"Point cancellation");check(value.x==123,"Cancelled points wrote payload");status(api.points(h.value,nullptr,&out,{},&e.text),3,e,"Null point request");status(api.points(h.value,&r,nullptr,{},&e.text),3,e,"Null point buffer");r.count=0;status(api.points(h.value,&r,&out,{},&e.text),3,e,"Zero point count");r.count=1;r.first=points.size()+1;status(api.points(h.value,&r,&out,{},&e.text),3,e,"Point range");r.first=0;r.reserved=1;status(api.points(h.value,&r,&out,{},&e.text),3,e,"Point reserved request");
}
template<class Api>void open_checks(const Api& api,const fs::path& source,const fs::path& resource){
    Error e;const auto text=fs::absolute(source).u8string();auto r=request(text);void* h=nullptr;
    status(api.open(nullptr,&h,&e.text),3,e,"Null open request");status(api.open(&r,nullptr,&e.text),3,e,"Null handle output");r.reserved=1;status(api.open(&r,&h,&e.text),3,e,"Open reserved");r.reserved=0;r.cancel=cancelled;status(api.open(&r,&h,&e.text),1,e,"Before-open cancellation");check(!h,"Cancelled open leaked handle");r.cancel={};
    const std::string options="{\"unexpected\":1}";r.options={options.data(),options.size()};status(api.open(&r,&h,&e.text),3,e,"Unsupported options");check(!h,"Invalid options returned handle");std::array<char,3> tiny{'!','!','!'};qpx_reader_text_v1 short_error{tiny.data()+1,1,0};check(api.open(&r,&h,&short_error)==6&&short_error.data==tiny.data()+1&&short_error.capacity==1&&short_error.size>1&&tiny==std::array<char,3>{'!','!','!'}&&!h,"Short error text must report required bytes without payload writes");r.options={};r.work_bytes=0;status(api.open(&r,&h,&e.text),3,e,"Zero work");r.work_bytes=256ull*1024*1024+1;status(api.open(&r,&h,&e.text),3,e,"Oversized work");
    Handle<Api> control(api);open_good(api,control,resource);const auto p=fs::absolute(resource).u8string();r=request(p,{},16384);status(api.open(&r,&h,&e.text),3,e,"Actual source/typed allocation work bound");check(!h,"Bound-refused open returned handle");
}
void abi(const fs::path& matrix_dll,const fs::path& point_dll,const fs::path& fixtures){
    Module mm(matrix_dll),pm(point_dll);const auto m=table<qpx_matrix_format_api_v1>(mm,"qpx_matrix_format_entry_v1");const auto p=table<qpx_annotation_format_api_v1>(pm,"qpx_annotation_format_entry_v1");check(m.axis&&m.values&&p.points,"Missing page callbacks");descriptor(m,true);descriptor(p,false);
    const auto manifest=Json::parse(bytes(fixtures/"fixtures.json"));std::size_t positives=0,negatives=0;
    for(const auto& item:manifest.at("positive")){const auto path=fixtures/fs::u8path(item.at("file").get<std::string>());if(item.at("domain")=="matrix")matrix(m,path,item);else annotation(p,path,item);++positives;}
    for(const auto& item:manifest.at("negative")){const auto path=fs::absolute(fixtures/fs::u8path(item.at("file").get<std::string>())).u8string();auto r=request(path);Error e;void* handle=nullptr;const auto code=item.at("domain")=="matrix"?m.open(&r,&handle,&e.text):p.open(&r,&handle,&e.text);status(code,item.at("status").get<unsigned>(),e,item.at("file").get<std::string>());check(!handle,"Refused source returned handle");++negatives;}
    open_checks(m,fixtures/"counts-main.tsv",fixtures/"budget-counts.tsv");open_checks(p,fixtures/"points-main.tsv",fixtures/"budget-points.tsv");
    std::cout<<positives<<" authored profiles and "<<negatives<<" exact parser refusals; direct ABI pages, UInt64, UTF8, buffers, cancellation and work checks pass\n";
}
void host(const std::string& which,const fs::path& manifest,const fs::path& fixtures){
    using namespace qpx;using namespace qpx::detail;
    if(which=="catalog-default"){const auto catalog=Json::parse(format_family_catalog_json());check(catalog.at("schema")=="qpx.format-catalog/1"&&catalog.at("matrix").size()==3&&catalog.at("annotations").size()==2&&format_plugin_infos().empty()&&format_plugin_diagnostics().empty(),"Default typed family catalog changed");const std::array<std::string,3> ids={"qmx","matrix-market","h5ad"},names={"Indexed expression sidecar (QMX)","Matrix Market with explicit QPX/10x catalogues","AnnData H5AD with explicit domain/frame"};for(unsigned i=0;i<3;++i)check(catalog.at("matrix").at(i).at("provider_id")==ids[i]&&catalog.at("matrix").at(i).at("display_name")==names[i],"Default matrix ID/display label changed");check(catalog.at("annotations").at(0).at("provider_id")=="geojson"&&catalog.at("annotations").at(0).at("display_name")=="GeoJSON"&&catalog.at("annotations").at(1).at("provider_id")=="measurements-csv"&&catalog.at("annotations").at(1).at("display_name")=="Object measurements CSV","Default annotation ID/display label changed");std::cout<<"Native default family catalog labels passed\n";return;}
    configure_format_plugins(fs::absolute(manifest).u8string());const auto infos=format_plugin_infos();check(infos.size()==2&&format_plugin_diagnostics().empty(),"Host catalog admission failed");refuses<std::logic_error>([&]{configure_format_plugins(fs::absolute(manifest).u8string());});
    if(which=="catalog"){const auto catalog=Json::parse(format_family_catalog_json());check(catalog.at("schema")=="qpx.format-catalog/1"&&catalog.at("matrix").size()==4&&catalog.at("annotations").size()==3,"Configured family membership differs");const std::array<std::string,4> mid={"qmx","matrix-market","h5ad","org.qupathxx.counts-tsv"},mnames={"Indexed expression sidecar (QMX)","Matrix Market with explicit QPX/10x catalogues","AnnData H5AD with explicit domain/frame","Dense counts TSV"};const std::array<std::string,3> aid={"geojson","measurements-csv","org.qupathxx.point-tsv"},anames={"GeoJSON","Object measurements CSV","Pixel point TSV"};for(unsigned i=0;i<4;++i)check(catalog.at("matrix").at(i).at("provider_id")==mid[i]&&catalog.at("matrix").at(i).at("display_name")==mnames[i],"Configured matrix catalog ID/label changed");for(unsigned i=0;i<3;++i)check(catalog.at("annotations").at(i).at("provider_id")==aid[i]&&catalog.at("annotations").at(i).at("display_name")==anames[i],"Configured annotation catalog ID/label changed");}
    else if(which=="matrix-control"||which=="receipt"||which=="source-change"||which=="cancel"||which=="ranges"){
        auto path=fixtures/"counts-main.tsv";if(which=="source-change"){path=fixtures/"source-change.control.tsv";write(path,bytes(fixtures/"counts-main.tsv"));}
        auto s=MatrixPluginSession::open("org.qupathxx.counts-tsv",path.u8string(),std::nullopt,8u*1024*1024);check(s->metadata().features==3&&s->metadata().observations==3,"Session shape");const auto values=s->values(0,9);check(values==std::vector<std::uint64_t>{UINT64_MAX,UINT64_C(9007199254740993),0,0,7,0,11,0,0},"Host exact UInt64 values");check(s->features(0,3).at(0).id=="0002"&&s->observations(0,3).at(0).id=="0001","Host IDs changed");s->verify_source();
        if(which=="receipt"){const auto serialized=format_plugin_receipt_json(s->receipt());const auto copy=parse_format_plugin_receipt(serialized);check(format_plugin_receipt_identity(copy)==format_plugin_receipt_identity(s->receipt())&&copy.options.empty()&&copy.provider.module_sha256.size()==64&&copy.provider.implementation_sha256.size()==64&&copy.source_sha256.size()==64,"Exact receipt roundtrip");auto modified=Json::parse(serialized);modified["source_sha256"]=std::string(64,'a');check(format_plugin_receipt_identity(parse_format_plugin_receipt(modified.dump()))!=format_plugin_receipt_identity(copy),"Source receipt aliases original");modified["implementation_sha256"]=std::string(64,'b');refuses([&]{parse_format_plugin_receipt(modified.dump());});}
        if(which=="source-change"){write(path,bytes(path)+"\n");refuses<WorkflowInputChanged>([&]{s->verify_source();});}
        if(which=="cancel"){std::atomic<bool> flag{true};refuses<WorkflowCancelled>([&]{s->values(0,1,&flag);});refuses<WorkflowCancelled>([&]{MatrixPluginSession::open("org.qupathxx.counts-tsv",path.u8string(),std::nullopt,8u*1024*1024,&flag);});}
        if(which=="ranges"){refuses([&]{s->values(10,1);});refuses([&]{s->features(0,0);});refuses([&]{s->observations(4,1);});}
    }else if(which=="annotation-control"){
        auto s=AnnotationPluginSession::open("org.qupathxx.point-tsv",(fixtures/"points-main.tsv").u8string(),std::nullopt,8u*1024*1024);auto rows=s->points(0,3);check(rows.size()==3&&rows[0].source_id=="0001"&&rows[1].source_id=="1"&&rows[0].c==-1&&rows[0].z==0&&rows[0].t==0&&rows[0].x==1.25&&rows[0].y==2.5&&rows[1].class_label.empty(),"Host point interpretation differs");s->verify_source();
    }else{
        const auto path=fixtures/fs::u8path("host-"+which+".control");write(path,which+"\n");const bool is_matrix=which.rfind("matrix-",0)==0||which.rfind("axis-",0)==0||which.rfind("values-",0)==0;
        if(is_matrix)refuses([&]{auto s=MatrixPluginSession::open("org.qupathxx.test.bad-matrix",path.u8string(),std::nullopt,8u*1024*1024);if(which.rfind("axis-",0)==0)s->features(0,3);else if(which.rfind("values-",0)==0)s->values(0,9);});
        else refuses([&]{auto s=AnnotationPluginSession::open("org.qupathxx.test.bad-annotation",path.u8string(),std::nullopt,8u*1024*1024);if(which.rfind("points-",0)==0)s->points(0,3);});
        check(bytes(fs::u8path(path.u8string()+".events"))=="open\ndestroy\n","Refused handle was not destroyed exactly once");
    }
    std::cout<<"Native host case "<<which<<" passed\n";
}
int dispatch(int argc,char** argv){try{if(argc==5&&std::string(argv[1])=="--abi")abi(fs::u8path(argv[2]),fs::u8path(argv[3]),fs::u8path(argv[4]));else if(argc==5&&std::string(argv[1])=="--host-case")host(argv[2],fs::u8path(argv[3]),fs::u8path(argv[4]));else throw std::invalid_argument("Usage: --abi MATRIX_DLL POINT_DLL FIXTURES | --host-case CASE MANIFEST FIXTURES");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> owned;std::vector<char*> utf8;for(int i=0;i<argc;++i){const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 2;std::string s(static_cast<std::size_t>(n),'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,s.data(),n,nullptr,nullptr))return 2;s.pop_back();owned.push_back(std::move(s));}for(auto& s:owned)utf8.push_back(s.data());return dispatch(argc,utf8.data());}
#else
int main(int argc,char** argv){return dispatch(argc,argv);}
#endif
