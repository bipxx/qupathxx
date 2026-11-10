#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/workflow.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <chrono>
using namespace qpx;
namespace fs=std::filesystem;
using J=nlohmann::json;
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
template<class F>void rejects(F&& fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed,"Expected OME-Zarr refusal");}
J json(const fs::path& path){std::ifstream in(path);J j;in>>j;return j;}
void write(const fs::path& path,const J& j){std::ofstream out(path);out<<j.dump(2);}
fs::path copy_fixture(const fs::path& root,const std::string& name,const std::string& fixture="plain"){const auto target=root/name;fs::copy(fs::path(QPX_ZARR_FIXTURES)/fixture,target,fs::copy_options::recursive);return target;}
void rich(const fs::path& path){
    OmeZarrSource source(path.u8string());const auto& m=source.metadata();
    check(m.width==7 && m.height==5 && m.channels==3 && m.z_planes==2 && m.timepoints==2 && m.levels.size()==2 && m.levels[1].downsample==2,"NGFF axes/pyramid changed");
    check(m.pixel_width_um==1.5 && m.pixel_height_um==1 && m.z_spacing_um==4 && m.time_interval_seconds==.5,"NGFF physical units/scales changed");
    check(source.axes()==std::vector<std::string>{"t","c","z","y","x"} && source.levels()[0].translation==std::vector<double>{0,0,4,23,44},"Ordered group/dataset transforms lost origin");
    check(source.domain()=="original-channel-samples" && source.sha256().size()==64,"Native source lacks domain/version identity");
    for(int level=0;level<2;++level)for(std::uint32_t t=0;t<2;++t)for(std::uint32_t z=0;z<2;++z){
        RawRegionRequest r;r.region={-2,-2,level,8,7};r.first_channel=1;r.channels=2;r.z=z;r.t=t;
        const auto buffer=source.read(r);const auto& values=std::get<std::vector<std::uint16_t>>(buffer.storage());const auto factor=level?2:1;
        const auto& shape=source.levels()[level].shape;
        for(int row=0;row<7;++row)for(int col=0;col<8;++col){const auto x=-2/factor+col,y=-2/factor+row;const auto pixel=std::size_t(row*8+col);const bool valid=x>=0 && y>=0 && x<int(shape[4]) && y<int(shape[3]);check(bool(buffer.valid()[pixel])==valid,"Outside padding confused valid fill");
            for(int c=1;c<3;++c){const bool hole=!level && t==1 && z==0 && c==2 && y>=3 && x>=4;const auto expected=valid?(hole?7:t*10000+c*1000+z*100+y*factor*10+x*factor):0;check(values[pixel*2+c-1]==expected,"C/Z/T/chunk seam/order/endian/edge/fill changed samples");}}
        rejects([&]{source.read(r,source.read_work_bytes(r)-1);});
    }
    RawRegionRequest full;full.region={0,0,0,7,5};full.channels=3;full.z=1;full.t=1;const auto expected=source.read(full);
    const auto& all=std::get<std::vector<std::uint16_t>>(expected.storage());
    for(int y=0;y<5;y+=2)for(int x=0;x<7;x+=3){auto tile=full;tile.region={x,y,0,std::min(3,7-x),std::min(2,5-y)};const auto b=source.read(tile);const auto& v=std::get<std::vector<std::uint16_t>>(b.storage());for(int row=0;row<b.height();++row)for(int col=0;col<b.width();++col)for(int c=0;c<3;++c)check(v[(row*b.width()+col)*3+c]==all[((y+row)*7+x+col)*3+c],"Tile assembly differs from whole source");}
    std::atomic<bool> cancel{true};rejects([&]{source.read(full,256u*1024*1024,&cancel);});rejects([&]{source.require_current(&cancel);});
}
void scalar(const std::string& fixture,SampleType type){
    OmeZarrSource source((fs::path(QPX_ZARR_FIXTURES)/fixture).u8string());RawRegionRequest request;request.region={0,0,0,3,2};const auto data=source.read(request);check(data.sample_type()==type && data.valid()==std::vector<std::uint8_t>(6,1),"Scalar type/validity differs");
    std::visit([&](const auto& v){using T=typename std::decay_t<decltype(v)>::value_type;
        if constexpr(std::is_floating_point_v<T>)check(v[0]==T(-1.25) && v[2]==T(3.5) && std::isnan(v[3]) && std::isinf(v[4]) && v[4]>0 && std::isinf(v[5]) && v[5]<0,"Float bits/nonfinite values changed");
        else{check(v.front()==(std::is_signed_v<T>?std::numeric_limits<T>::min():T(0)) && v.back()==std::numeric_limits<T>::max(),"Integer extremes changed");if constexpr(sizeof(T)==8){if constexpr(std::is_unsigned_v<T>)check(v[2]==9007199254740993ULL,"uint64 lost precision beyond2^53");else check(v[1]==-9007199254740993LL,"int64 lost precision beyond2^53");}}
    },data.storage());
}
int main(){try{
    const auto root=fs::current_path()/("ome-zarr-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);
    for(const std::string name:{"plain","little-c-zlib","big-f-gzip"})rich(fs::path(QPX_ZARR_FIXTURES)/name);
    for(const auto& item:std::vector<std::pair<std::string,SampleType>>{{"u1",SampleType::UInt8},{"u2",SampleType::UInt16},{"i1",SampleType::Int8},{"i2",SampleType::Int16},{"u4",SampleType::UInt32},{"i4",SampleType::Int32},{"u8",SampleType::UInt64},{"i8",SampleType::Int64},{"f4",SampleType::Float32},{"f8",SampleType::Float64}})scalar(item.first,item.second);
    for(int mode=0;mode<13;++mode){const auto path=copy_fixture(root,"invalid-"+std::to_string(mode));auto attrs=json(path/".zattrs"),array=json(path/"0/.zarray");
        if(mode==0)attrs["multiscales"][0]["version"]="0.5";if(mode==1)attrs["multiscales"][0]["axes"][4]["name"]="y";if(mode==2)attrs["multiscales"][0]["datasets"][0]["path"]="../outside";
        if(mode==3)array["dtype"]="<c8";if(mode==4)array["compressor"]={{"id","blosc"}};if(mode==5)array["filters"]=J::array({{{"id","delta"}}});if(mode==6)array["chunks"][0]=100000000;
        if(mode==7)attrs["multiscales"][0]["datasets"][1]["coordinateTransformations"][0]["scale"][3]=1.5;
        if(mode==8)attrs["multiscales"][0]["axes"][4]["unit"]="unsupported-unit";
        if(mode==9)array["fill_value"]=-1;if(mode==10)array["fill_value"]=1.5;
        if(mode==11)array["order"]="A";if(mode==12)array["shape"][0]=0;
        write(path/".zattrs",attrs);write(path/"0/.zarray",array);rejects([&]{OmeZarrSource source(path.u8string());});
    }
    const auto changed=copy_fixture(root,"changed");OmeZarrSource old(changed.u8string());{std::ofstream out(changed/"0/0.0.0.0.0",std::ios::binary|std::ios::app);out.put('x');}rejects([&]{old.require_current();});rejects([&]{RawRegionRequest r;r.region={0,0,0,1,1};old.read(r);});
    const auto corrupt=copy_fixture(root,"corrupt","big-f-gzip");{std::ofstream out(corrupt/"0/0.0.0.0.0",std::ios::binary);out<<"bad";}OmeZarrSource bad(corrupt.u8string());rejects([&]{RawRegionRequest r;r.region={0,0,0,1,1};bad.read(r);});
    const auto missing=copy_fixture(root,"missing");OmeZarrSource missing_source(missing.u8string());fs::remove(missing/"0/0.0.0.0.0");rejects([&]{RawRegionRequest r;r.region={0,0,0,1,1};missing_source.read(r);});
    const auto null=copy_fixture(root,"null-fill");auto spec=json(null/"0/.zarray");spec["fill_value"]=nullptr;write(null/"0/.zarray",spec);OmeZarrSource undefined(null.u8string());rejects([&]{RawRegionRequest r;r.region={4,3,0,1,1};r.first_channel=2;r.z=0;r.t=1;undefined.read(r);});
    const auto added=copy_fixture(root,"added");OmeZarrSource observed(added.u8string());{std::ofstream out(added/"new-file");out<<"new";}rejects([&]{observed.require_current();});
    OmeZarrOptions limited;limited.max_files=1;rejects([&]{OmeZarrSource too_many((fs::path(QPX_ZARR_FIXTURES)/"plain").u8string(),limited);});limited={};limited.inventory_bytes=1;rejects([&]{OmeZarrSource too_large((fs::path(QPX_ZARR_FIXTURES)/"plain").u8string(),limited);});
    const auto translated=copy_fixture(root,"translated");auto attrs=json(translated/".zattrs");attrs["multiscales"][0]["datasets"][1]["coordinateTransformations"][1]["translation"][4]=20.75;write(translated/".zattrs",attrs);OmeZarrSource shifted(translated.u8string());RawRegionRequest r;r.region={0,0,1,3,1};const auto data=shifted.read(r);check(data.valid()==std::vector<std::uint8_t>{0,1,1} && std::get<std::vector<std::uint16_t>>(data.storage())==std::vector<std::uint16_t>{0,0,2},"Pyramid translation was ignored or guessed");
    const auto large=copy_fixture(root,"large-origin");auto origin=json(large/".zattrs");for(auto& dataset:origin["multiscales"][0]["datasets"])dataset["coordinateTransformations"][1]["translation"][4]=1e20;write(large/".zattrs",origin);OmeZarrSource huge_origin(large.u8string());r.region={1,1,0,2,1};check(std::get<std::vector<std::uint16_t>>(huge_origin.read(r).storage())==std::vector<std::uint16_t>{11,12},"Large common physical origin erased pixel coordinates");
    std::cout<<"Native NGFF0.4/Zarr2 scalar/CZT/scale/origin/endian/C-F/chunks/fill/codec/guard/bounds checks pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
