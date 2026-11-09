#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/workflow.hpp"
#include <limits>
#ifdef QUPATHXX_HAS_OME_ZARR
#include "ome_zarr_internal.hpp"
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#endif
namespace qpx {
void OmeZarrOptions::validate()const {
    if(!metadata_bytes || metadata_bytes>16u*1024*1024 || !chunk_bytes || chunk_bytes>64u*1024*1024 || !encoded_bytes || encoded_bytes>128u*1024*1024 || !inventory_bytes || inventory_bytes>128u*1024*1024 || !max_files || max_files>1000000)throw std::invalid_argument("Invalid OME-Zarr metadata/chunk/inventory limits");
}
bool has_ome_zarr(){
#ifdef QUPATHXX_HAS_OME_ZARR
return true;
#else
return false;
#endif
}
#ifdef QUPATHXX_HAS_OME_ZARR
namespace {
using detail::zarr_require;using detail::ZarrArray;using J=detail::ZarrJson;
template<class T>T fill(const J& j){
    if(j.is_null())return T{};
    if constexpr(std::is_floating_point_v<T>){
        if(j.is_string()){const auto text=j.get<std::string>();if(text=="NaN")return std::numeric_limits<T>::quiet_NaN();if(text=="Infinity")return std::numeric_limits<T>::infinity();if(text=="-Infinity")return -std::numeric_limits<T>::infinity();}
        zarr_require(j.is_number(),"unsupported floating fill");const auto v=j.get<double>();const auto result=T(v);zarr_require(std::isfinite(v) && std::isfinite(result),"floating fill overflow");return result;
    }else{
        if(j.is_number_unsigned()){const auto v=j.get<std::uint64_t>();zarr_require(v<=std::uint64_t(std::numeric_limits<T>::max()),"integer fill overflow");return T(v);}
        if(j.is_number_integer()){const auto v=j.get<std::int64_t>();if constexpr(std::is_signed_v<T>)zarr_require(v>=std::int64_t(std::numeric_limits<T>::min()) && v<=std::int64_t(std::numeric_limits<T>::max()),"integer fill overflow");else zarr_require(v>=0 && std::uint64_t(v)<=std::uint64_t(std::numeric_limits<T>::max()),"integer fill overflow");return T(v);}
        zarr_require(j.is_number_float(),"unsupported integer fill");const auto v=j.get<double>();const auto upper=std::ldexp(1.,int(sizeof(T)*8)-(std::is_signed_v<T>?1:0));const auto lower=std::is_signed_v<T>?-upper:0.;zarr_require(std::isfinite(v) && v==std::floor(v) && v>=lower && v<upper,"integer fill is not exact/in range");return T(v);
    }
}
std::vector<std::uint8_t> decode(std::vector<std::uint8_t> bytes,const ZarrArray& a){
    if(a.codec.empty()){zarr_require(bytes.size()==a.decoded_bytes,"raw chunk size differs");return bytes;}
    std::vector<std::uint8_t> result(a.decoded_bytes);z_stream stream{};zarr_require(inflateInit2(&stream,a.codec=="gzip"?31:15)==Z_OK,"cannot initialize admitted codec");
    struct End{z_stream* stream;~End(){inflateEnd(stream);}}end{&stream};stream.next_in=bytes.data();stream.avail_in=uInt(bytes.size());stream.next_out=result.data();stream.avail_out=uInt(result.size());
    const auto rc=inflate(&stream,Z_FINISH);zarr_require(rc==Z_STREAM_END && stream.total_out==result.size() && stream.total_in==bytes.size(),"corrupt/truncated/trailing compressed chunk");return result;
}
std::size_t output_bytes(const RawRegionRequest& r,SampleType type){return std::size_t(r.region.width)*std::size_t(r.region.height)*(std::size_t(r.channels)*sample_bytes(type)+1);}
}
struct OmeZarrSource::Data {
    detail::ZarrStore store;detail::ZarrMetadata metadata;std::string root,id;std::shared_ptr<const ReaderSource> observed;
    Data(const std::string& path,OmeZarrOptions options,const std::atomic<bool>* c):store(path,options,c),metadata(detail::zarr_metadata(store,c)),root(store.root.u8string()),id(store.id){
        // Validate fill scalars even when all chunks happen to be populated.
        for(const auto& a:metadata.arrays){RawPixelBuffer scalar(1,1,1,a.type);std::visit([&](const auto& values){using T=typename std::decay_t<decltype(values)>::value_type;(void)fill<T>(a.fill);},scalar.storage());}
        store.require_current(c);
        observed=make_directory_reader_source("ome-zarr",root,store.metadata_version(),id);
    }
};
OmeZarrSource::OmeZarrSource(const std::string& path,OmeZarrOptions options,const std::atomic<bool>* c):data_(std::make_shared<Data>(path,options,c)){}
const RawImageMetadata& OmeZarrSource::metadata()const{return data_->metadata.raw;}
const std::string& OmeZarrSource::domain()const{static const std::string value="original-channel-samples";return value;}
const std::string& OmeZarrSource::sha256()const{return data_->id;}
const std::string& OmeZarrSource::directory()const{return data_->root;}
std::shared_ptr<const ReaderSource> OmeZarrSource::reader_source()const{return data_->observed;}
const std::vector<std::string>& OmeZarrSource::axes()const{return data_->metadata.axes;}
const std::vector<std::string>& OmeZarrSource::units()const{return data_->metadata.units;}
const std::vector<OmeZarrLevel>& OmeZarrSource::levels()const{return data_->metadata.levels;}
std::size_t OmeZarrSource::payload_bytes()const{return sizeof(Data)+data_->store.payload_bytes()+data_->metadata.payload+data_->root.capacity()+data_->observed->payload_bytes()+128;}
void OmeZarrSource::require_current(const std::atomic<bool>* c)const{data_->store.require_current(c);}
std::size_t OmeZarrSource::read_work_bytes(const RawRegionRequest& r)const{
    r.validate(metadata());const auto& d=*data_;const auto& a=d.metadata.arrays[std::size_t(r.region.level)];
    return payload_bytes()+output_bytes(r,a.type)+a.decoded_bytes+d.store.options.encoded_bytes+1024*1024;
}
RawPixelBuffer OmeZarrSource::read(const RawRegionRequest& r,std::size_t work,const std::atomic<bool>* cancelled)const {
    workflow_checkpoint(cancelled);zarr_require(work && work<=1024ull*1024*1024,"work limit must be1..1024MiB");if(read_work_bytes(r)>work)throw WorkflowCaptureLimit("OME-Zarr read exceeds accounted work limit");
    const auto& d=*data_;const auto& m=d.metadata;const auto& a=m.arrays[std::size_t(r.region.level)];const auto& l=a.level;const auto& base=m.arrays[0].level;const auto& store=d.store;
    for(const auto& key:{std::string(".zattrs"),std::string(".zgroup"),l.path+"/.zarray"})store.verify(key);
    const auto start=[&](int axis,std::int64_t p){
        const auto factor=std::int64_t(m.raw.levels[std::size_t(r.region.level)].downsample);
        // Cancel a shared physical origin before arithmetic. Multiplying p then
        // adding/subtracting a large origin loses even small pixel coordinates.
        const auto q=p/factor,rem=p%factor;
        const auto offset=(base.translation[axis]-l.translation[axis])/l.scale[axis];
        zarr_require(std::isfinite(offset) && std::abs(offset)<=4503599627370496. && (offset!=0 || base.translation[axis]==l.translation[axis]),"unsupported translation precision/range");
        const auto whole=std::int64_t(offset);
        const auto shift=whole+std::int64_t(std::floor(double(rem)/double(factor)+(offset-double(whole))));
        zarr_require((shift<=0 || q<=INT64_MAX-shift) && (shift>=0 || q>=INT64_MIN-shift),"mapped level coordinate overflow");return q+shift;
    };
    const auto sx=start(m.x,r.region.x),sy=start(m.y,r.region.y);zarr_require(sx<=INT64_MAX-r.region.width && sy<=INT64_MAX-r.region.height,"mapped region overflow");
    RawPixelBuffer output(r.region.width,r.region.height,r.channels,a.type);
    std::vector<std::uint64_t> first(l.shape.size()),last(l.shape.size()),at(l.shape.size());
    for(std::size_t axis=0;axis<l.shape.size();++axis){std::int64_t lo=0,hi=1;
        if(int(axis)==m.x){lo=std::max<std::int64_t>(0,sx);hi=std::min<std::int64_t>(l.shape[axis],sx+r.region.width);}
        else if(int(axis)==m.y){lo=std::max<std::int64_t>(0,sy);hi=std::min<std::int64_t>(l.shape[axis],sy+r.region.height);}
        else if(int(axis)==m.c){lo=r.first_channel;hi=lo+r.channels;}else if(int(axis)==m.z){lo=r.z;hi=lo+1;}else if(int(axis)==m.t){lo=r.t;hi=lo+1;}
        if(hi<=lo){for(const auto& key:{std::string(".zattrs"),std::string(".zgroup"),l.path+"/.zarray"})store.verify(key);workflow_checkpoint(cancelled);return output;}first[axis]=std::uint64_t(lo)/l.chunks[axis];last[axis]=std::uint64_t(hi-1)/l.chunks[axis];at[axis]=first[axis];
    }
    std::vector<std::size_t> strides(l.shape.size());std::size_t stride=sample_bytes(a.type);
    for(std::size_t i=0;i<strides.size();++i){const auto axis=a.fortran?i:strides.size()-i-1;strides[axis]=stride;stride*=std::size_t(l.chunks[axis]);}
    const std::uint16_t endian=1;const bool native_big=*reinterpret_cast<const std::uint8_t*>(&endian)==0;
    bool more=true;while(more){workflow_checkpoint(cancelled);std::string key=l.path+"/";for(std::size_t i=0;i<at.size();++i){if(i)key+=a.separator;key+=std::to_string(at[i]);}
        const bool exists=store.contains(key);std::vector<std::uint8_t> chunk;if(exists)chunk=decode(store.read(key,store.options.encoded_bytes),a);else{store.verify(key);zarr_require(!a.undefined_fill,"missing chunk has undefined null fill");}
        const auto x0=std::max<std::int64_t>({0,sx,std::int64_t(at[m.x]*l.chunks[m.x])}),y0=std::max<std::int64_t>({0,sy,std::int64_t(at[m.y]*l.chunks[m.y])});
        const auto x1=std::min<std::int64_t>(sx+r.region.width,std::int64_t(std::min(l.shape[m.x],(at[m.x]+1)*l.chunks[m.x]))),y1=std::min<std::int64_t>(sy+r.region.height,std::int64_t(std::min(l.shape[m.y],(at[m.y]+1)*l.chunks[m.y])));
        const auto c0=m.c<0?0:std::max<std::uint64_t>(r.first_channel,at[m.c]*l.chunks[m.c]),c1=m.c<0?1:std::min<std::uint64_t>(r.first_channel+r.channels,(at[m.c]+1)*l.chunks[m.c]);
        std::visit([&](auto& values){using T=typename std::decay_t<decltype(values)>::value_type;const T fill_value=fill<T>(a.fill);
            for(auto y=y0;y<y1;++y){workflow_checkpoint(cancelled);for(auto x=x0;x<x1;++x){const auto pixel=std::size_t(y-sy)*std::size_t(r.region.width)+std::size_t(x-sx);output.writable_valid()[pixel]=1;
                for(auto c=c0;c<c1;++c){T value=fill_value;if(exists){std::size_t offset=0;for(std::size_t axis=0;axis<at.size();++axis){const auto index=int(axis)==m.x?std::uint64_t(x):int(axis)==m.y?std::uint64_t(y):int(axis)==m.c?c:int(axis)==m.z?r.z:int(axis)==m.t?r.t:0;offset+=std::size_t(index%l.chunks[axis])*strides[axis];}std::uint8_t bytes[sizeof(T)];std::memcpy(bytes,chunk.data()+offset,sizeof(T));if(native_big!=a.big_endian)std::reverse(bytes,bytes+sizeof(T));std::memcpy(&value,bytes,sizeof(T));}values[pixel*r.channels+std::size_t(c-r.first_channel)]=value;}
            }}
        },output.writable_storage());store.verify(key);
        more=false;for(std::size_t i=at.size();i-->0;){if(at[i]<last[i]){++at[i];more=true;break;}at[i]=first[i];}
    }
    for(const auto& key:{std::string(".zattrs"),std::string(".zgroup"),l.path+"/.zarray"})store.verify(key);workflow_checkpoint(cancelled);return output;
}
#else
struct OmeZarrSource::Data {};
namespace {[[noreturn]]void unavailable(){throw std::runtime_error("OME-Zarr support was not enabled");}}
OmeZarrSource::OmeZarrSource(const std::string&,OmeZarrOptions,const std::atomic<bool>*){unavailable();}
const RawImageMetadata& OmeZarrSource::metadata()const{unavailable();}
const std::string& OmeZarrSource::domain()const{unavailable();}
const std::string& OmeZarrSource::sha256()const{unavailable();}
const std::string& OmeZarrSource::directory()const{unavailable();}
std::shared_ptr<const ReaderSource> OmeZarrSource::reader_source()const{unavailable();}
const std::vector<std::string>& OmeZarrSource::axes()const{unavailable();}
const std::vector<std::string>& OmeZarrSource::units()const{unavailable();}
const std::vector<OmeZarrLevel>& OmeZarrSource::levels()const{unavailable();}
std::size_t OmeZarrSource::payload_bytes()const{unavailable();}
std::size_t OmeZarrSource::read_work_bytes(const RawRegionRequest&)const{unavailable();}
void OmeZarrSource::require_current(const std::atomic<bool>*)const{unavailable();}
RawPixelBuffer OmeZarrSource::read(const RawRegionRequest&,std::size_t,const std::atomic<bool>*)const{unavailable();}
#endif
}
