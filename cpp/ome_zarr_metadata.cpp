#include "ome_zarr_internal.hpp"
#include "qupathxx/workflow.hpp"
#include <cmath>
#include <cctype>
#include <set>
namespace qpx::detail {
namespace {
using J=ZarrJson;
J json(const ZarrStore& store,const std::string& name){const auto bytes=store.read(name,store.options.metadata_bytes);return J::parse(bytes.begin(),bytes.end(),[](int depth,J::parse_event_t,J&){zarr_require(depth<=32,"metadata nesting exceeds32");return true;});}
std::vector<std::uint64_t> dimensions(const J& j,std::size_t n){zarr_require(j.is_array() && j.size()==n,"array dimensions differ from axes");std::vector<std::uint64_t> result;for(const auto& v:j){zarr_require(v.is_number_integer() && v>0 && v<=INT64_MAX,"invalid array dimension");result.push_back(v.get<std::uint64_t>());}return result;}
void transforms(const J& j,std::vector<double>& scale,std::vector<double>& offset,bool mandatory){
    zarr_require(j.is_array() && j.size()<=2 && (!mandatory || !j.empty()),"unsupported coordinate transform list");bool scaled=false,translated=false;
    for(const auto& transform:j){const auto type=transform.at("type").get<std::string>();zarr_require(type=="scale" || type=="translation","unsupported coordinate transform");const auto& values=transform.at(type);zarr_require(values.is_array() && values.size()==scale.size(),"coordinate transform rank differs");
        if(type=="scale"){zarr_require(!scaled && !translated,"scale must precede translation");scaled=true;}else{zarr_require(!translated,"repeated translation");translated=true;}
        for(std::size_t a=0;a<scale.size();++a){zarr_require(values[a].is_number(),"nonnumeric transform");const auto v=values[a].get<double>();zarr_require(std::isfinite(v) && (type!="scale" || v>0),"invalid transform value");if(type=="scale"){scale[a]*=v;offset[a]*=v;}else offset[a]+=v;zarr_require(std::isfinite(scale[a]) && std::isfinite(offset[a]),"transform overflow");}
    }zarr_require(!mandatory || scaled,"dataset scale is required");
}
double unit(const std::string& value,bool time){
    if(time){if(value=="second")return 1;if(value=="millisecond")return .001;if(value=="microsecond")return .000001;if(value=="minute")return 60;if(value=="hour")return 3600;}
    else{if(value=="micrometer")return 1;if(value=="nanometer")return .001;if(value=="millimeter")return 1000;if(value=="centimeter")return 10000;if(value=="meter")return 1000000;}
    throw std::invalid_argument("OME-Zarr: unsupported declared axis unit: "+value);
}
SampleType dtype(const std::string& value){
    static const std::map<std::string,SampleType> types{{"u1",SampleType::UInt8},{"u2",SampleType::UInt16},{"u4",SampleType::UInt32},{"u8",SampleType::UInt64},{"i1",SampleType::Int8},{"i2",SampleType::Int16},{"i4",SampleType::Int32},{"i8",SampleType::Int64},{"f4",SampleType::Float32},{"f8",SampleType::Float64}};
    zarr_require(value.size()==3 && (value[0]=='<' || value[0]=='>' || value[0]=='|'),"unsupported scalar dtype");const auto found=types.find(value.substr(1));zarr_require(found!=types.end(),"unsupported scalar dtype");zarr_require(value[0]!='|' || sample_bytes(found->second)==1,"multibyte dtype requires byte order");return found->second;
}
}
ZarrMetadata zarr_metadata(const ZarrStore& store,const std::atomic<bool>* cancelled){
    ZarrMetadata result;const auto group=json(store,".zgroup"),attrs=json(store,".zattrs");zarr_require(group.at("zarr_format")==2,"requires Zarr2 group");
    const auto& multiscales=attrs.at("multiscales");zarr_require(multiscales.is_array() && multiscales.size()==1,"select a single NGFF image group");const auto& m=multiscales[0];zarr_require(m.at("version")=="0.4","requires NGFF0.4");
    const auto& axes=m.at("axes");zarr_require(axes.is_array() && axes.size()>=2 && axes.size()<=5,"requires2..5 axes");std::set<std::string> unique;
    for(std::size_t a=0;a<axes.size();++a){const auto& axis=axes[a];const auto name=axis.at("name").get<std::string>(),type=axis.at("type").get<std::string>(),u=axis.value("unit",std::string{});zarr_require(unique.insert(name).second,"duplicate axis");
        int* index=nullptr;if(name=="x")index=&result.x;else if(name=="y")index=&result.y;else if(name=="z")index=&result.z;else if(name=="c")index=&result.c;else if(name=="t")index=&result.t;zarr_require(index,"unsupported axis name");*index=int(a);
        zarr_require(type==(name=="t"?"time":name=="c"?"channel":"space"),"axis type/name differ");if(!u.empty())unit(u,name=="t");zarr_require(name!="c" || u.empty(),"channel units unsupported");result.axes.push_back(name);result.units.push_back(u);result.raw.dimension_order+=char(std::toupper(name[0]));
    }
    zarr_require(result.x==int(axes.size())-1 && result.y==result.x-1 && (result.z<0 || result.z==result.y-1) && (result.c<0 || result.c<result.y) && (result.t<0 || result.t==0) && (result.c<0 || result.z<0 || result.c<result.z),"unsupported NGFF axis order");
    const auto& datasets=m.at("datasets");zarr_require(datasets.is_array() && !datasets.empty() && datasets.size()<=32,"requires1..32 pyramid levels");std::set<std::string> paths;
    for(const auto& d:datasets){workflow_checkpoint(cancelled);ZarrArray a;auto& l=a.level;l.path=d.at("path").get<std::string>();zarr_require(!l.path.empty() && l.path.size()<=4096 && l.path.find('\\')==std::string::npos && paths.insert(l.path).second,"invalid or duplicate dataset path");
        const auto spec=json(store,l.path+"/.zarray");zarr_require(spec.at("zarr_format")==2,"requires Zarr2 array");l.shape=dimensions(spec.at("shape"),axes.size());l.chunks=dimensions(spec.at("chunks"),axes.size());
        a.type=dtype(spec.at("dtype").get<std::string>());a.big_endian=spec.at("dtype").get<std::string>()[0]=='>';const auto order=spec.at("order").get<std::string>();zarr_require(order=="C" || order=="F","unsupported byte order");a.fortran=order=="F";
        const auto& filters=spec.at("filters");zarr_require(filters.is_null() || (filters.is_array() && filters.empty()),"unsupported array filters");const auto& compressor=spec.at("compressor");if(!compressor.is_null()){a.codec=compressor.at("id").get<std::string>();zarr_require(a.codec=="zlib" || a.codec=="gzip","unsupported compressor: "+a.codec);}
        a.separator=spec.value("dimension_separator",std::string("."));zarr_require(a.separator=="." || a.separator=="/","invalid chunk separator");a.fill=spec.at("fill_value");a.undefined_fill=a.fill.is_null();
        std::size_t bytes=sample_bytes(a.type);for(auto n:l.chunks){zarr_require(n<=store.options.chunk_bytes/bytes,"decoded chunk exceeds limit");bytes*=std::size_t(n);}a.decoded_bytes=bytes;
        l.scale.assign(axes.size(),1);l.translation.assign(axes.size(),0);transforms(d.at("coordinateTransformations"),l.scale,l.translation,true);if(m.contains("coordinateTransformations"))transforms(m.at("coordinateTransformations"),l.scale,l.translation,false);
        if(result.c>=0)zarr_require(l.scale[result.c]==1 && l.translation[result.c]==0,"nonidentity channel transform unsupported");
        if(!result.arrays.empty()){
            const auto& base=result.arrays.front();zarr_require(a.type==base.type,"mixed pyramid dtypes unsupported");const auto down=l.scale[result.x]/base.level.scale[result.x];zarr_require(std::isfinite(down) && down>=1 && down==std::floor(down) && down<=INT32_MAX && l.scale[result.x]==base.level.scale[result.x]*down && l.scale[result.y]==base.level.scale[result.y]*down,"unsupported anisotropic/nonintegral pyramid geometry");
            zarr_require(down>result.raw.levels.back().downsample,"pyramid levels must increase");
            for(int i:{result.c,result.z,result.t})if(i>=0)zarr_require(l.shape[i]==base.level.shape[i] && l.scale[i]==base.level.scale[i] && l.translation[i]==base.level.translation[i],"nonspatial pyramid axes changed");
            result.raw.levels.push_back({std::int64_t(l.shape[result.x]),std::int64_t(l.shape[result.y]),down});
        }else{
            result.raw.width=std::int64_t(l.shape[result.x]);result.raw.height=std::int64_t(l.shape[result.y]);result.raw.sample_type=a.type;
            const auto count=[&](int i){const auto n=i<0?1:l.shape[i];zarr_require(n<=65536,"C/Z/T axis exceeds65536");return std::uint32_t(n);};result.raw.channels=count(result.c);result.raw.z_planes=count(result.z);result.raw.timepoints=count(result.t);
            result.raw.levels.push_back({result.raw.width,result.raw.height,1});
            const auto calibrated=[&](int i)->std::optional<double>{if(i<0 || result.units[i].empty())return {};const auto v=l.scale[i]*unit(result.units[i],i==result.t);zarr_require(std::isfinite(v) && v>0,"calibration overflow");return v;};
            result.raw.pixel_width_um=calibrated(result.x);result.raw.pixel_height_um=calibrated(result.y);result.raw.z_spacing_um=calibrated(result.z);result.raw.time_interval_seconds=calibrated(result.t);
        }
        result.levels.push_back(l);result.arrays.push_back(std::move(a));
    }
    if(attrs.contains("omero") && attrs["omero"].contains("channels")){const auto& channels=attrs["omero"]["channels"];zarr_require(channels.is_array() && channels.size()==result.raw.channels,"channel labels differ from axis");for(const auto& channel:channels){auto label=channel.value("label",std::string{});zarr_require(label.size()<=4096,"channel label exceeds limit");result.raw.channel_names.push_back(std::move(label));}}
    if(result.raw.channel_names.empty())for(std::uint32_t i=0;i<result.raw.channels;++i)result.raw.channel_names.push_back("Channel "+std::to_string(i));
    const auto level_bytes=[](const OmeZarrLevel& l){return l.path.capacity()+(l.shape.capacity()+l.chunks.capacity())*sizeof(std::uint64_t)+(l.scale.capacity()+l.translation.capacity())*sizeof(double);};
    result.payload=sizeof(result)+result.raw.payload_bytes()+result.arrays.capacity()*sizeof(ZarrArray)+result.levels.capacity()*sizeof(OmeZarrLevel)+(result.axes.capacity()+result.units.capacity())*sizeof(std::string);
    for(std::size_t i=0;i<result.axes.size();++i)result.payload+=result.axes[i].capacity()+result.units[i].capacity();
    for(const auto& a:result.arrays)result.payload+=level_bytes(a.level)+a.codec.capacity()+a.separator.capacity()+a.fill.dump().size()+64;
    for(const auto& l:result.levels)result.payload+=level_bytes(l);
    return result;
}
}
