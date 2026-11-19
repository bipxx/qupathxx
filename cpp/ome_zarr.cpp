#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/workflow.hpp"
#include <limits>
#ifdef QUPATHXX_HAS_OME_ZARR
#include "ome_zarr_internal.hpp"
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#ifdef QUPATHXX_HAS_NGFF05
#include "shard_index.hpp"
#include "zstd_chunk.hpp"
#endif
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
        // The metadata authority validates decimal/exponent tokens against
        // their original spelling before normalizing exact values to integers.
        // A binary64 value alone cannot establish integral source semantics.
        zarr_require(false,"unsupported integer fill");return T{};
    }
}
template<class T> std::array<std::uint8_t,sizeof(T)> fill_sample_bytes(const ZarrArray& array){
    std::array<std::uint8_t,sizeof(T)> bytes{};
    if constexpr(std::is_floating_point_v<T>){
        if(array.floating_fill_bits){
            using Bits=std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>;
            const auto bits=static_cast<Bits>(*array.floating_fill_bits);
            std::memcpy(bytes.data(),&bits,sizeof(T));
            return bytes;
        }
    }
    const auto value=fill<T>(array.fill);
    std::memcpy(bytes.data(),&value,sizeof(T));
    return bytes;
}
std::size_t serialized_chunk_bytes(const ZarrArray& array){
    return array.decoded_bytes+(array.decoded_crc32c?sizeof(std::uint32_t):0);
}
std::vector<std::uint8_t> decode_compression(std::vector<std::uint8_t> bytes,const ZarrArray& a){
    const auto decoded=serialized_chunk_bytes(a);
    if(a.codec.empty()){zarr_require(bytes.size()==decoded,"raw chunk size differs");return bytes;}
#ifdef QUPATHXX_HAS_NGFF05
    if(a.codec=="zstd")return detail::decode_ngff05_zstd(bytes,decoded,a.checksum);
    if(a.codec=="blosc")return detail::decode_ngff05_blosc(bytes,decoded,a.blosc);
#endif
    std::vector<std::uint8_t> result(decoded);z_stream stream{};zarr_require(inflateInit2(&stream,a.codec=="gzip"?31:15)==Z_OK,"cannot initialize admitted codec");
    struct End{z_stream* stream;~End(){inflateEnd(stream);}}end{&stream};stream.next_in=bytes.data();stream.avail_in=uInt(bytes.size());stream.next_out=result.data();stream.avail_out=uInt(result.size());
    const auto rc=inflate(&stream,Z_FINISH);zarr_require(rc==Z_STREAM_END && stream.total_out==result.size() && stream.total_in==bytes.size(),"corrupt/truncated/trailing compressed chunk");return result;
}
#ifdef QUPATHXX_HAS_NGFF05
void check_chunk_crc32c(std::vector<std::uint8_t>& bytes,const char* mismatch){
    zarr_require(bytes.size()>=sizeof(std::uint32_t),"chunk CRC32C is truncated");
    const auto size=bytes.size()-sizeof(std::uint32_t);
    const auto expected=detail::shard_index::little_endian<std::uint32_t>(bytes.data()+size);
    zarr_require(detail::shard_index::crc32c(bytes.data(),size)==expected,mismatch);
    bytes.resize(size);
}
#endif
std::vector<std::uint8_t> decode(std::vector<std::uint8_t> bytes,const ZarrArray& a){
#ifdef QUPATHXX_HAS_NGFF05
    if(a.encoded_crc32c)check_chunk_crc32c(bytes,"encoded chunk CRC32C mismatch");
#endif
    auto result=decode_compression(std::move(bytes),a);
#ifdef QUPATHXX_HAS_NGFF05
    if(a.decoded_crc32c)check_chunk_crc32c(result,"decoded chunk CRC32C mismatch");
#endif
    return result;
}
std::size_t output_bytes(const RawRegionRequest& r,SampleType type){return std::size_t(r.region.width)*std::size_t(r.region.height)*(std::size_t(r.channels)*sample_bytes(type)+1);}
std::size_t encoded_chunk_limit(const ZarrArray& array,std::size_t configured){
    return array.codec.empty()?std::min(configured,serialized_chunk_bytes(array)):configured;
}

#ifdef QUPATHXX_HAS_NGFF05
constexpr std::size_t shard_scan_slice=1024*1024;
constexpr std::uint64_t shard_scan_limit=128ull*1024*1024;
constexpr std::uint64_t shard_scan_region_limit=512ull*1024*1024;
constexpr std::size_t shard_scan_cache_size=32;
constexpr std::size_t shard_scan_workspace=shard_scan_slice+shard_scan_cache_size*8192;

class ZarrShardReader {
    std::string cached_key_;
    std::vector<detail::ShardChunkRange> index_;
    std::array<std::string,shard_scan_cache_size> verified_;
    std::size_t next_verified_=0;
    std::uint64_t scan_remaining_=shard_scan_region_limit;

    std::uint64_t verified_length(const detail::ZarrStore& store,const std::string& key,
        std::uint64_t length,std::size_t index_bytes,const std::atomic<bool>* cancelled){
        constexpr auto trailer_bytes=sizeof(std::uint32_t);
        zarr_require(length>=index_bytes+trailer_bytes,"checksummed shard is shorter than its index and CRC32C");
        const auto body=length-trailer_bytes;
        if(std::find(verified_.begin(),verified_.end(),key)!=verified_.end())return body;
        zarr_require(length<=shard_scan_limit && length<=scan_remaining_,"whole-shard CRC32C exceeds scan limit");
        scan_remaining_-=length;
        const auto trailer=store.read_range(key,body,trailer_bytes,trailer_bytes);
        const auto expected=detail::shard_index::little_endian<std::uint32_t>(trailer.data());
        auto crc=std::numeric_limits<std::uint32_t>::max();
        for(std::uint64_t offset=0;offset<body;){
            workflow_checkpoint(cancelled);
            const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(body-offset,shard_scan_slice));
            const auto slice=store.read_range(key,offset,count,shard_scan_slice);
            crc=detail::shard_index::update_crc32c(crc,slice.data(),slice.size());
            offset+=count;
        }
        zarr_require(~crc==expected,"whole-shard CRC32C mismatch");
        verified_[next_verified_]=key;
        next_verified_=(next_verified_+1)%verified_.size();
        return body;
    }

public:
    std::vector<std::uint8_t> read(const detail::ZarrStore& store,const ZarrArray& array,
        const std::string& key,std::size_t inner_index,const std::atomic<bool>* cancelled){
        if(key!=cached_key_){
            // Drop the previous index before accounting the next validation.
            std::vector<detail::ShardChunkRange>().swap(index_);
            const auto count=detail::shard_index_encoded_bytes(array.shard.entries,array.shard.index_encoding);
            auto length=store.member_bytes(key);
            if(array.shard.crc32c)length=verified_length(store,key,length,count,cancelled);
            zarr_require(length>=count,"shard is shorter than its index");
            const auto offset=array.shard.index_at_start?0:length-count;
            const auto encoded=store.read_range(key,offset,count,count);
            index_=detail::decode_shard_index(encoded,length,array.shard.index_at_start,
                encoded_chunk_limit(array,store.options.encoded_bytes),array.shard.index_encoding);
            cached_key_=key;
        }
        const auto& range=index_.at(inner_index);
        if(!range.length)return {};
        return decode(store.read_range(key,range.offset,static_cast<std::size_t>(range.length),
            encoded_chunk_limit(array,store.options.encoded_bytes)),array);
    }
};
#endif
}
struct OmeZarrSource::Data {
    detail::ZarrStore store;detail::ZarrMetadata metadata;std::string root,id;std::shared_ptr<const ReaderSource> observed;
#ifdef QUPATHXX_HAS_NGFF05
    Data(const std::string& path,OmeZarrOptions options,const std::atomic<bool>* c,detail::NgffLayout profile=detail::NgffLayout::v04):store(path,options,c,profile),metadata(detail::zarr_metadata(store,c)),root(store.root.u8string()),id(store.id){
#else
    Data(const std::string& path,OmeZarrOptions options,const std::atomic<bool>* c):store(path,options,c),metadata(detail::zarr_metadata(store,c)),root(store.root.u8string()),id(store.id){
#endif
        // Validate fill scalars even when all chunks happen to be populated.
        for(const auto& a:metadata.arrays){RawPixelBuffer scalar(1,1,1,a.type);std::visit([&](const auto& values){using T=typename std::decay_t<decltype(values)>::value_type;(void)fill_sample_bytes<T>(a);},scalar.storage());}
        store.require_current(c);
        observed=make_directory_reader_source(store.layout()==detail::NgffLayout::v05?"org.qupathxx.ngff05":"ome-zarr",root,store.metadata_version(),id);
    }
};
OmeZarrSource::OmeZarrSource(const std::string& path,OmeZarrOptions options,const std::atomic<bool>* c):data_(std::make_shared<Data>(path,options,c)){}
#ifdef QUPATHXX_HAS_NGFF05
std::shared_ptr<const OmeZarrSource> detail::Ngff05SourceFactory::open(const std::string& path,OmeZarrOptions options,const std::atomic<bool>* c){
    return std::shared_ptr<const OmeZarrSource>(new OmeZarrSource(std::make_shared<OmeZarrSource::Data>(path,options,c,NgffLayout::v05)));
}
const detail::Ngff05Display* detail::Ngff05SourceFactory::display(const OmeZarrSource& source){
    const auto& display=source.data_->metadata.display;
    return display?&*display:nullptr;
}
#endif
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
    const auto base=payload_bytes()+output_bytes(r,a.type)+serialized_chunk_bytes(a)+
        encoded_chunk_limit(a,d.store.options.encoded_bytes)+1024*1024;
#ifdef QUPATHXX_HAS_NGFF05
    if(d.store.layout()==detail::NgffLayout::v05){
        const auto index_work=a.shard.entries?detail::shard_index_workspace_bytes(a.shard.entries,a.shard.index_encoding):0;
        auto codec_work=a.codec=="zstd"?detail::ngff05_zstd_workspace_bytes:0;
        if(a.codec=="blosc") {
            codec_work=detail::ngff05_blosc_workspace(serialized_chunk_bytes(a));
            if(a.blosc.codec==detail::Ngff05Blosc::Codec::Zstandard)
                codec_work+=detail::ngff05_zstd_workspace_bytes;
        }
        return base+codec_work+index_work+(a.shard.crc32c?shard_scan_workspace:0);
    }
#endif
    return base;
}
std::array<std::int64_t,2> OmeZarrSource::region_level_origin(const RawRegionRequest& r)const {
    r.validate(metadata());
    const auto& m=data_->metadata;
    const auto& l=m.arrays[std::size_t(r.region.level)].level;
    const auto& base=m.arrays.front().level;
    const auto modern=data_->store.layout()==detail::NgffLayout::v05;
    const auto factor=std::int64_t(m.raw.levels[std::size_t(r.region.level)].downsample);
    const auto start=[&](int axis,std::int64_t p){
        // Cancel a shared physical origin before arithmetic. Multiplying p then
        // adding/subtracting a large origin loses even small pixel coordinates.
        const auto q=p/factor,rem=p%factor;
        // v0.5 admits this lattice only after the actual pyramid transforms
        // have been checked against the pixel-center convention.
        const auto offset=modern?0:(base.translation[axis]-l.translation[axis])/l.scale[axis];
        zarr_require(std::isfinite(offset) && std::abs(offset)<=4503599627370496. && (modern || offset!=0 || base.translation[axis]==l.translation[axis]),"unsupported translation precision/range");
        const auto whole=std::int64_t(offset);
        const auto shift=whole+std::int64_t(std::floor(double(rem)/double(factor)+(offset-double(whole))));
        zarr_require((shift<=0 || q<=INT64_MAX-shift) && (shift>=0 || q>=INT64_MIN-shift),"mapped level coordinate overflow");return q+shift;
    };
    const auto sx=start(m.x,r.region.x),sy=start(m.y,r.region.y);
    zarr_require(sx<=INT64_MAX-r.region.width && sy<=INT64_MAX-r.region.height,"mapped region overflow");
    return {sx,sy};
}
RawPixelBuffer OmeZarrSource::read(const RawRegionRequest& r,std::size_t work,const std::atomic<bool>* cancelled)const {
    return read_region(r,work,cancelled,false);
}
RawPixelBuffer OmeZarrSource::read_level(const RawRegionRequest& r,std::size_t work,const std::atomic<bool>* cancelled)const {
    return read_region(r,work,cancelled,true);
}
RawPixelBuffer OmeZarrSource::read_region(const RawRegionRequest& r,std::size_t work,const std::atomic<bool>* cancelled,bool level_coordinates)const {
    workflow_checkpoint(cancelled);
    zarr_require(work && work<=1024ull*1024*1024,"work limit must be1..1024MiB");
    auto work_request=r;
    // Work is independent of origin. Validate selected-level indices separately from level-zero bounds.
    if(level_coordinates)work_request.region.x=work_request.region.y=0;
    if(read_work_bytes(work_request)>work)throw WorkflowCaptureLimit("OME-Zarr read exceeds accounted work limit");
    const auto& d=*data_;const auto& m=d.metadata;const auto& a=m.arrays[std::size_t(r.region.level)];const auto& l=a.level;const auto& store=d.store;
    const auto modern=store.layout()==detail::NgffLayout::v05;
    const auto verify_metadata=[&]{
        store.verify(store.primary_key());
        if(!modern)store.verify(".zgroup");
        store.verify(l.path+(modern?"/zarr.json":"/.zarray"));
    };
    verify_metadata();
    const auto [sx,sy]=level_coordinates?std::array<std::int64_t,2>{r.region.x,r.region.y}:region_level_origin(r);
    if(level_coordinates)
        zarr_require(sx<=INT64_MAX-r.region.width && sy<=INT64_MAX-r.region.height,"level-array coordinate overflow");
    RawPixelBuffer output(r.region.width,r.region.height,r.channels,a.type);
    std::vector<std::uint64_t> first(l.shape.size()),last(l.shape.size()),at(l.shape.size());
    for(std::size_t axis=0;axis<l.shape.size();++axis){std::int64_t lo=0,hi=1;
        if(int(axis)==m.x){lo=std::max<std::int64_t>(0,sx);hi=std::min<std::int64_t>(l.shape[axis],sx+r.region.width);}
        else if(int(axis)==m.y){lo=std::max<std::int64_t>(0,sy);hi=std::min<std::int64_t>(l.shape[axis],sy+r.region.height);}
        else if(int(axis)==m.c){lo=r.first_channel;hi=lo+r.channels;}else if(int(axis)==m.z){lo=r.z;hi=lo+1;}else if(int(axis)==m.t){lo=r.t;hi=lo+1;}
        if(hi<=lo){verify_metadata();workflow_checkpoint(cancelled);return output;}first[axis]=std::uint64_t(lo)/l.chunks[axis];last[axis]=std::uint64_t(hi-1)/l.chunks[axis];at[axis]=first[axis];
    }
    std::vector<std::size_t> strides(l.shape.size());std::size_t stride=sample_bytes(a.type);
    for(std::size_t i=0;i<strides.size();++i){
        const auto axis=a.fortran?i:a.sample_axes[strides.size()-i-1];
        strides[axis]=stride;
        stride*=std::size_t(l.chunks[axis]);
    }
    const std::uint16_t endian=1;const bool native_big=*reinterpret_cast<const std::uint8_t*>(&endian)==0;
#ifdef QUPATHXX_HAS_NGFF05
    ZarrShardReader shard_reader;
#endif
    bool more=true;while(more){
        workflow_checkpoint(cancelled);
        std::string key=l.path+(modern?"/c/":"/");
#ifdef QUPATHXX_HAS_NGFF05
        std::size_t inner_index=0;
#endif
        for(std::size_t i=0;i<at.size();++i){
            if(i)key+=a.separator;
            auto coordinate=at[i];
#ifdef QUPATHXX_HAS_NGFF05
            if(a.shard.entries){
                const auto ratio=a.shard.chunks_per_axis[i];
                inner_index+=static_cast<std::size_t>(coordinate%ratio)*a.shard.index_strides[i];
                coordinate/=ratio;
            }
#endif
            key+=std::to_string(coordinate);
        }
        std::vector<std::uint8_t> chunk;
        const auto member_present=store.contains(key);
        if(member_present){
#ifdef QUPATHXX_HAS_NGFF05
            if(a.shard.entries)chunk=shard_reader.read(store,a,key,inner_index,cancelled);
            else
#endif
                chunk=decode(store.read(key,encoded_chunk_limit(a,store.options.encoded_bytes)),a);
        }else{
            store.verify(key);
            zarr_require(!a.undefined_fill,"missing chunk has undefined null fill");
        }
        const bool exists=!chunk.empty();
        const auto x0=std::max<std::int64_t>({0,sx,std::int64_t(at[m.x]*l.chunks[m.x])}),y0=std::max<std::int64_t>({0,sy,std::int64_t(at[m.y]*l.chunks[m.y])});
        const auto x1=std::min<std::int64_t>(sx+r.region.width,std::int64_t(std::min(l.shape[m.x],(at[m.x]+1)*l.chunks[m.x]))),y1=std::min<std::int64_t>(sy+r.region.height,std::int64_t(std::min(l.shape[m.y],(at[m.y]+1)*l.chunks[m.y])));
        const auto c0=m.c<0?0:std::max<std::uint64_t>(r.first_channel,at[m.c]*l.chunks[m.c]),c1=m.c<0?1:std::min<std::uint64_t>(r.first_channel+r.channels,(at[m.c]+1)*l.chunks[m.c]);
        std::visit([&](auto& values){
            using T=typename std::decay_t<decltype(values)>::value_type;
            const auto fill_bytes=fill_sample_bytes<T>(a);
            for(auto y=y0;y<y1;++y){
                workflow_checkpoint(cancelled);
                for(auto x=x0;x<x1;++x){
                    const auto pixel=std::size_t(y-sy)*std::size_t(r.region.width)+std::size_t(x-sx);
                    output.writable_valid()[pixel]=1;
                    for(auto c=c0;c<c1;++c){
                        auto& value=values[pixel*r.channels+std::size_t(c-r.first_channel)];
                        if(!exists){
                            std::memcpy(&value,fill_bytes.data(),sizeof(T));
                            continue;
                        }
                        std::size_t offset=0;
                        for(std::size_t axis=0;axis<at.size();++axis){
                            const auto index=int(axis)==m.x?std::uint64_t(x):int(axis)==m.y?std::uint64_t(y):
                                int(axis)==m.c?c:int(axis)==m.z?r.z:int(axis)==m.t?r.t:0;
                            offset+=std::size_t(index%l.chunks[axis])*strides[axis];
                        }
                        auto* bytes=reinterpret_cast<std::uint8_t*>(&value);
                        std::memcpy(bytes,chunk.data()+offset,sizeof(T));
                        if(native_big!=a.big_endian)std::reverse(bytes,bytes+sizeof(T));
                    }
                }
            }
        },output.writable_storage());
        store.verify(key);
        more=false;for(std::size_t i=at.size();i-->0;){if(at[i]<last[i]){++at[i];more=true;break;}at[i]=first[i];}
    }
    verify_metadata();workflow_checkpoint(cancelled);return output;
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
std::array<std::int64_t,2> OmeZarrSource::region_level_origin(const RawRegionRequest&)const{unavailable();}
std::size_t OmeZarrSource::payload_bytes()const{unavailable();}
std::size_t OmeZarrSource::read_work_bytes(const RawRegionRequest&)const{unavailable();}
void OmeZarrSource::require_current(const std::atomic<bool>*)const{unavailable();}
RawPixelBuffer OmeZarrSource::read(const RawRegionRequest&,std::size_t,const std::atomic<bool>*)const{unavailable();}
RawPixelBuffer OmeZarrSource::read_level(const RawRegionRequest&,std::size_t,const std::atomic<bool>*)const{unavailable();}
#endif
}
