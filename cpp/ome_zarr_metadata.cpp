#include "ome_zarr_internal.hpp"
#include "qupathxx/workflow.hpp"
#include "utf8_text.hpp"
#include <cmath>
#include <charconv>
#include <cctype>
#include <cstring>
#include <set>
#include <string_view>
#ifdef QUPATHXX_HAS_NGFF05
#include "shard_index.hpp"
#endif
namespace qpx::detail {
namespace {
using J=ZarrJson;
J json(const std::vector<std::uint8_t>& bytes){return J::parse(bytes.begin(),bytes.end(),[](int depth,J::parse_event_t,J&){zarr_require(depth<=32,"metadata nesting exceeds32");return true;});}
J json(const ZarrStore& store,const std::string& name){return json(store.read(name,store.options.metadata_bytes));}

// Decode a JSON decimal as an integer without passing its significant digits
// through binary64. The JSON parser has already checked the token grammar.
// All positions/exponents are bounded by the input length; no big integer or
// expanded decimal string is allocated for long zero runs or exponents.
J exact_integer_fill(std::string_view token,double parsed){
    const bool negative=token.front()=='-';std::size_t at=negative?1:0;
    const auto digits_begin=at;
    while(at<token.size() && token[at]>='0' && token[at]<='9')++at;
    const auto whole=at-digits_begin;
    std::size_t fraction=0;
    if(at<token.size() && token[at]=='.'){
        const auto begin=++at;while(at<token.size() && token[at]>='0' && token[at]<='9')++at;
        fraction=at-begin;
    }
    const auto digits_end=at;
    std::int64_t exponent=0;
    if(at<token.size()){
        ++at;bool minus=false;if(token[at]=='+' || token[at]=='-'){minus=token[at]=='-';++at;}
        const auto bound=std::int64_t(token.size())+64;
        for(;at<token.size();++at){const auto digit=token[at]-'0';exponent=exponent>(bound-digit)/10?bound:exponent*10+digit;}
        if(minus)exponent=-exponent;
    }
    const auto point=std::int64_t(whole)+exponent;
    std::size_t ordinal=0,first=whole+fraction,last=0;
    for(at=digits_begin;at<digits_end;++at){if(token[at]=='.')continue;if(token[at]!='0'){first=std::min(first,ordinal);last=ordinal;}++ordinal;}
    if(first==whole+fraction)return J(std::uint64_t(0));
    zarr_require(point>std::int64_t(last),"integer fill has a fractional source value");
    zarr_require(point-std::int64_t(first)<=20,"integer fill overflow");
    std::uint64_t magnitude=0;ordinal=0;
    const auto append=[&](unsigned digit){zarr_require(magnitude<=(UINT64_MAX-digit)/10,"integer fill overflow");magnitude=magnitude*10+digit;};
    for(at=digits_begin;at<digits_end;++at){if(token[at]=='.')continue;if(ordinal>=first && std::int64_t(ordinal)<point)append(unsigned(token[at]-'0'));++ordinal;}
    for(auto position=std::int64_t(whole+fraction);position<point;++position)append(0);
    if(negative){
        constexpr auto limit=std::uint64_t(1)<<63;
        zarr_require(magnitude<=limit,"integer fill overflow");
        const auto value=magnitude==limit?INT64_MIN:-std::int64_t(magnitude);
        zarr_require(std::isfinite(parsed) && parsed>=-std::ldexp(1.,63) && parsed<0 && std::int64_t(parsed)==value,"integer fill loses source precision");
        return J(value);
    }
    zarr_require(std::isfinite(parsed) && parsed>=0 && parsed<std::ldexp(1.,64) && std::uint64_t(parsed)==magnitude,"integer fill loses source precision");
    return J(magnitude);
}

// Public SAX callbacks expose the ORIGINAL number_float token (including an
// overflowing integer literal). DOM callbacks only expose its rounded double.
// Restrict capture to the decoded root fill_value key, never a nested alias.
template <J (*Convert)(std::string_view,double)>
struct FillNumberToken final:nlohmann::json_sax<J>{
    std::size_t depth=0;bool pending=false,seen=false,found=false;J value;
    bool scalar(){if(depth==1)pending=false;return true;}
    bool null()override{return scalar();}
    bool boolean(bool)override{return scalar();}
    bool number_integer(number_integer_t)override{return scalar();}
    bool number_unsigned(number_unsigned_t)override{return scalar();}
    bool number_float(number_float_t number,const string_t& token)override{
        if(depth==1 && pending){value=Convert(token,number);found=true;}
        return scalar();
    }
    bool string(string_t&)override{return scalar();}
    bool binary(binary_t&)override{return scalar();}
    bool start_object(std::size_t)override{scalar();++depth;return true;}
    bool key(string_t& key)override{
        if(depth==1){pending=key=="fill_value";if(pending){zarr_require(!seen,"repeated fill_value key");seen=true;}}
        return true;
    }
    bool end_object()override{--depth;return true;}
    bool start_array(std::size_t)override{scalar();++depth;return true;}
    bool end_array()override{--depth;return true;}
    bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception&)override{return false;}
};

J checked_fill(const J& fill,SampleType type,const std::vector<std::uint8_t>& metadata){
    if(type==SampleType::Float32 || type==SampleType::Float64 || !fill.is_number_float())return fill;
    FillNumberToken<exact_integer_fill> token;
    zarr_require(J::sax_parse(metadata.begin(),metadata.end(),&token) && token.found,"cannot verify integer fill source token");
    return std::move(token.value);
}

#ifdef QUPATHXX_HAS_NGFF05
template<class T> using FloatBits=std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>;

template<class T> FloatBits<T> float_bits(T value){
    static_assert(sizeof(T)==sizeof(FloatBits<T>) && std::numeric_limits<T>::is_iec559);
    FloatBits<T> bits;
    std::memcpy(&bits,&value,sizeof(bits));
    return bits;
}

J float32_number_fill(std::string_view token,double parsed){
    float value=0;
    const auto end=token.data()+token.size();
    const auto result=std::from_chars(token.data(),end,value,std::chars_format::general);
    if(result.ec==std::errc::result_out_of_range && result.ptr==end &&
       std::abs(parsed)<std::numeric_limits<float>::denorm_min())
        return token.front()=='-'?0x80000000u:0u;
    zarr_require(result.ec==std::errc{} && result.ptr==end && std::isfinite(value),"invalid Zarr3 float32 numeric fill");
    return float_bits(value);
}

template<class T> FloatBits<T> ngff05_float_fill(const J& fill,const std::vector<std::uint8_t>& metadata){
    if(fill.is_number_float()){
        if constexpr(std::is_same_v<T,float>){
            FillNumberToken<float32_number_fill> token;
            zarr_require(J::sax_parse(metadata.begin(),metadata.end(),&token) && token.found,"cannot verify float32 fill source token");
            return token.value.get<std::uint32_t>();
        }else{
            return float_bits(fill.get<double>());
        }
    }
    if(fill.is_number_unsigned())return float_bits(static_cast<T>(fill.get<std::uint64_t>()));
    if(fill.is_number_integer()){
        const auto value=fill.get<std::int64_t>();
        // The JSON parser uses its signed integer callback for the token -0.
        return float_bits(value==0?T(-0.0):static_cast<T>(value));
    }
    zarr_require(fill.is_string(),"invalid Zarr3 floating fill");
    const auto& value=fill.get_ref<const std::string&>();
    if(value=="NaN")return float_bits(std::numeric_limits<T>::quiet_NaN());
    if(value=="Infinity")return float_bits(std::numeric_limits<T>::infinity());
    if(value=="-Infinity")return float_bits(-std::numeric_limits<T>::infinity());
    zarr_require(value.size()==2+2*sizeof(T) && value.compare(0,2,"0x")==0,"invalid Zarr3 floating fill bits");
    FloatBits<T> bits;
    const auto end=value.data()+value.size();
    const auto result=std::from_chars(value.data()+2,end,bits,16);
    zarr_require(result.ec==std::errc{} && result.ptr==end,"invalid Zarr3 floating fill bits");
    return bits;
}
#endif
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

J ngff05_json(const std::vector<std::uint8_t>& bytes){
    using Event=J::parse_event_t;
    std::vector<std::set<std::string>> keys;
    return J::parse(bytes.begin(),bytes.end(),[&](int depth,Event event,J& node){
        zarr_require(depth<=32,"metadata nesting exceeds32");
        if(event==Event::object_start)keys.emplace_back();
        if(event==Event::object_end)keys.pop_back();
        if(event==Event::key)
            zarr_require(keys.back().insert(node.get<std::string>()).second,"duplicate metadata key");
        return true;
    });
}

bool ngff05_path(const std::string& path){
    if(path.empty() || path.size()>4096 || path.find_first_of(":\\")!=std::string::npos ||
       path.find('\0')!=std::string::npos)return false;
    const std::string_view key(path);
    std::size_t start=0;
    for(;;){
        const auto slash=key.find('/',start);
        const auto part=key.substr(start,slash-start);
        if(part.empty() || part=="." || part==".." || part.back()=='.' || part.back()==' ')return false;
        if(slash==std::string_view::npos)return true;
        start=slash+1;
    }
}

#ifdef QUPATHXX_HAS_NGFF05
const J& ngff05_shard_codecs(const J& configuration,ZarrArray& array){
    const auto inner=dimensions(configuration.at("chunk_shape"),array.level.chunks.size());
    auto& shard=array.shard;
    shard.entries=1;
    for(std::size_t encoded=inner.size();encoded-->0;){
        const auto axis=array.sample_axes[encoded];
        const auto outer=array.level.chunks[axis];
        zarr_require(outer%inner[encoded]==0,"inner chunks do not divide the shard shape");
        const auto ratio=outer/inner[encoded];
        zarr_require(ratio<=shard_index_max_entries/shard.entries,"shard index exceeds entry limit");
        shard.chunks_per_axis[axis]=ratio;
        shard.index_strides[axis]=shard.entries;
        shard.entries*=static_cast<std::size_t>(ratio);
        array.level.chunks[axis]=inner[encoded];
    }
    const auto& index=configuration.at("index_codecs");
    zarr_require(index.is_array() && (index.size()==1 || index.size()==2) && index[0].at("name")=="bytes",
        "shard index requires bytes with optional crc32c");
    const auto& bytes=index[0].at("configuration");
    const auto& endian=bytes.at("endian");
    zarr_require(bytes.is_object() && bytes.size()==1 && (endian=="little" || endian=="big"),
        "invalid shard index byte order or configuration");
    shard.index_encoding.big_endian=endian=="big";
    shard.index_encoding.crc32c=index.size()==2;
    if(shard.index_encoding.crc32c){
        const auto settings=index[1].value("configuration",J::object());
        zarr_require(index[1].at("name")=="crc32c" && settings.is_object() && settings.empty(),
            "shard index requires bytes with optional crc32c");
    }
    const auto location=configuration.value("index_location",std::string("end"));
    zarr_require(location=="start" || location=="end","unsupported shard index location");
    shard.index_at_start=location=="start";
    return configuration.at("codecs");
}
#endif

std::size_t ngff05_leading_transposes(const J& codecs,std::size_t rank,std::array<std::size_t,5>& axes){
    zarr_require(codecs.is_array() && !codecs.empty(),"Zarr3 codec pipeline is empty or invalid");
    std::size_t index=0;
    while(index<codecs.size() && codecs[index].at("name")=="transpose"){
        const auto& order=codecs[index].at("configuration").at("order");
        zarr_require(order.is_array() && order.size()==rank,"Zarr3 transpose order must match the chunk rank");
        std::array<bool,5> seen{};
        auto composed=axes;
        for(std::size_t encoded=0;encoded<rank;++encoded){
            const auto& position=order[encoded];
            zarr_require(position.is_number_integer(),"Zarr3 transpose axes must be integers");
            const auto axis=position.get<std::uint64_t>();
            zarr_require(axis<rank && !seen[axis],"Zarr3 transpose order must be a permutation");
            seen[axis]=true;
            composed[encoded]=axes[axis];
        }
        axes=composed;
        ++index;
    }
    return index;
}

void ngff05_array(const std::vector<std::uint8_t>& metadata,const std::vector<std::string>& axes,ZarrArray& a){
    const auto spec=ngff05_json(metadata);
    zarr_require(spec.at("zarr_format").is_number_integer() && spec.at("zarr_format")==3 &&
        spec.at("node_type")=="array","requires Zarr3 array");
    const auto& type=spec.at("data_type");
    if(type=="uint8")a.type=SampleType::UInt8;
    else if(type=="int8")a.type=SampleType::Int8;
    else if(type=="uint16")a.type=SampleType::UInt16;
    else if(type=="int16")a.type=SampleType::Int16;
    else if(type=="uint32")a.type=SampleType::UInt32;
    else if(type=="int32")a.type=SampleType::Int32;
    else if(type=="uint64")a.type=SampleType::UInt64;
    else if(type=="int64")a.type=SampleType::Int64;
    else if(type=="float64")a.type=SampleType::Float64;
    else{
        zarr_require(type=="float32","unsupported NGFF0.5 scalar data type");
        a.type=SampleType::Float32;
    }
    zarr_require(spec.at("dimension_names")==J(axes),"Zarr3 dimension_names differ from NGFF axes");
    const auto& grid=spec.at("chunk_grid");
    zarr_require(grid.at("name")=="regular","unsupported Zarr3 chunk grid");
    a.level.shape=dimensions(spec.at("shape"),axes.size());
    a.level.chunks=dimensions(grid.at("configuration").at("chunk_shape"),axes.size());
    const auto& key=spec.at("chunk_key_encoding");
    zarr_require(key.at("name")=="default" && key.at("configuration").at("separator")=="/",
        "unsupported Zarr3 chunk key encoding");
    if(spec.contains("storage_transformers")){
        const auto& storage=spec.at("storage_transformers");
        zarr_require(storage.is_array() && storage.empty(),"unsupported Zarr3 storage transformers");
    }
    const auto& outer_codecs=spec.at("codecs");
    const J* data_codecs=&outer_codecs;
    auto serializer_index=ngff05_leading_transposes(outer_codecs,axes.size(),a.sample_axes);
    const auto validate_crc32c=[](const J& codec){
        const auto configuration=codec.value("configuration",J::object());
        zarr_require(configuration.is_object() && configuration.empty(),"CRC32C does not take parameters");
    };
#ifdef QUPATHXX_HAS_NGFF05
    if(serializer_index<outer_codecs.size() && outer_codecs[serializer_index].at("name")=="sharding_indexed"){
        const auto suffix=serializer_index+1;
        if(suffix<outer_codecs.size()){
            zarr_require(suffix+1==outer_codecs.size() && outer_codecs[suffix].at("name")=="crc32c",
                "unsupported codecs after indexed sharding");
            validate_crc32c(outer_codecs[suffix]);
            a.shard.crc32c=true;
        }
        data_codecs=&ngff05_shard_codecs(outer_codecs[serializer_index].at("configuration"),a);
        serializer_index=ngff05_leading_transposes(*data_codecs,axes.size(),a.sample_axes);
    }
#endif
    const auto& codecs=*data_codecs;
    zarr_require(serializer_index<codecs.size() && codecs[serializer_index].at("name")=="bytes",
        "NGFF0.5 profile requires a bytes serializer");
    const auto serializer=codecs[serializer_index].value("configuration",J::object());
    zarr_require(serializer.is_object(),"invalid bytes configuration");
    const bool multibyte=sample_bytes(a.type)>1;
    if(serializer.contains("endian")){
        const auto& endian=serializer.at("endian");
        zarr_require(endian=="little" || endian=="big","unsupported bytes endian");
        a.big_endian=multibyte && endian=="big";
    }else{
        zarr_require(!multibyte,"multibyte Zarr3 samples require bytes endian");
    }
    auto next=serializer_index+1;
    const auto consume_crc32c=[&]{
        if(next==codecs.size() || codecs[next].at("name")!="crc32c")return false;
        validate_crc32c(codecs[next++]);
        return true;
    };
    a.decoded_crc32c=consume_crc32c();
    if(next<codecs.size()){
        const auto& compressor=codecs[next++];
        a.codec=compressor.at("name").get<std::string>();
        const auto& codec=compressor.at("configuration");
#ifdef QUPATHXX_HAS_NGFF05
        if(a.codec=="blosc"){
            const auto name=codec.at("cname").get<std::string>();
            const auto shuffle=codec.at("shuffle").get<std::string>();
            const auto& level=codec.at("clevel");
            const auto& block=codec.at("blocksize");
            if(name=="lz4" || name=="lz4hc")a.blosc.codec=Ngff05Blosc::Codec::Lz4;
            else if(name=="blosclz")a.blosc.codec=Ngff05Blosc::Codec::BloscLz;
            else if(name=="zstd")a.blosc.codec=Ngff05Blosc::Codec::Zstandard;
            else if(name=="zlib")a.blosc.codec=Ngff05Blosc::Codec::Zlib;
            else if(name=="snappy")a.blosc.codec=Ngff05Blosc::Codec::Snappy;
            else zarr_require(false,"unsupported Zarr3 Blosc compressor name");
            zarr_require(shuffle=="noshuffle" || shuffle=="shuffle" || shuffle=="bitshuffle",
                "invalid Zarr3 Blosc shuffle mode");
            zarr_require(level.is_number_integer() && level>=0 && level<=9 &&
                block.is_number_integer() && block>=0 && block<=ngff05_blosc_max_block_bytes,
                "invalid Zarr3 Blosc compression level or block size");
            zarr_require(codec.size()==4+std::size_t(codec.contains("typesize")),"unexpected Zarr3 Blosc fields");
            if(shuffle!="noshuffle"){
                const auto& size=codec.at("typesize");
                zarr_require(size.is_number_integer() && size>=1 && size<=BLOSC_MAX_TYPESIZE,
                    "invalid Zarr3 Blosc typesize");
                a.blosc.type_size=size.get<std::size_t>();
                a.blosc.shuffle=shuffle=="shuffle"?BLOSC_DOSHUFFLE:BLOSC_DOBITSHUFFLE;
            }
        }else
#endif
        if(a.codec=="gzip"){
            const auto& level=codec.at("level");
            zarr_require(level.is_number_integer() && level>=0 && level<=9,"invalid Zarr3 gzip level");
        }else{
            zarr_require(a.codec=="zstd","unsupported Zarr3 compressor");
            const auto& level=codec.at("level");
            zarr_require(level.is_number_integer() && level>=-131072 && level<=22 &&
                codec.at("checksum").is_boolean(),"invalid Zarr3 zstd configuration");
#ifdef QUPATHXX_HAS_NGFF05
            a.checksum=codec.at("checksum").get<bool>();
#endif
        }
        a.encoded_crc32c=consume_crc32c();
    }
    zarr_require(next==codecs.size(),"unsupported Zarr3 codec sequence after compression");
    a.separator="/";
    a.fill=spec.at("fill_value");
#ifdef QUPATHXX_HAS_NGFF05
    if(a.type==SampleType::Float32){
        a.floating_fill_bits=ngff05_float_fill<float>(a.fill,metadata);
        return;
    }
    if(a.type==SampleType::Float64){
        a.floating_fill_bits=ngff05_float_fill<double>(a.fill,metadata);
        return;
    }
#endif
    zarr_require(a.fill.is_number_integer(),"Zarr3 integer fill requires an integer value");
    // The source validates this integer with the same typed conversion used
    // for missing chunks, including the full unsigned 64-bit range.
}

void ngff05_pyramid(const ZarrMetadata& metadata,const OmeZarrLevel& level,double down){
    const auto& base=metadata.arrays.front().level;
    const auto factor=static_cast<std::uint64_t>(down);
    for(const auto axis:{metadata.x,metadata.y}){
        const auto shift=(down-1)*0.5*base.scale[axis];
        zarr_require(std::isfinite(shift) && level.translation[axis]==shift,
            "Zarr3 pyramid translation does not match the pixel-center lattice");
        const auto width=base.shape[axis]/factor;
        const auto remainder=base.shape[axis]%factor;
        zarr_require(level.shape[axis]>=width && level.shape[axis]<=width+(remainder!=0),
            "Zarr3 pyramid shape does not match the pixel-center lattice");
    }
}

#ifdef QUPATHXX_HAS_NGFF05
std::optional<Ngff05Display> ngff05_display(const J& omero,const J& channels){
    const auto declares_display=[](const J& channel){
        return channel.contains("color") || channel.contains("window") || channel.contains("active") ||
            channel.contains("family") || channel.contains("inverted") || channel.contains("coefficient");
    };
    if(!omero.contains("rdefs") && std::none_of(channels.begin(),channels.end(),declares_display))
        return {};
    Ngff05Display result;
    try{
        if(omero.contains("rdefs")){
            const auto& rdefs=omero.at("rdefs");
            zarr_require(rdefs.is_object() && rdefs.value("model",std::string("color"))=="color",
                "unsupported OMERO display model");
            // Plane selection belongs to the host. Its current store ABI does
            // not expose a producer default time or Z plane to analysis.
        }
        for(std::uint32_t i=0;i<channels.size();++i){
            const auto& record=channels[i];
            if(!record.value("active",true))continue;
            zarr_require(record.value("family",std::string("linear"))=="linear" &&
                (!record.contains("coefficient") || record.at("coefficient")==1),
                "unsupported OMERO transfer function");
            Ngff05DisplayChannel channel;
            channel.channel=i;
            channel.inverted=record.value("inverted",false);
            const auto& color=record.at("color").get_ref<const std::string&>();
            zarr_require(color.size()==6 && color.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,
                "invalid OMERO RGB color");
            for(std::size_t component=0;component<channel.rgb.size();++component){
                unsigned value=0;
                const auto start=color.data()+component*2;
                const auto parsed=std::from_chars(start,start+2,value,16);
                zarr_require(parsed.ec==std::errc{} && parsed.ptr==start+2,"invalid OMERO RGB color");
                channel.rgb[component]=static_cast<std::uint8_t>(value);
            }
            const auto& window=record.at("window");
            const auto number=[&](const char* key){
                const auto& field=window.at(key);
                zarr_require(field.is_number(),"invalid OMERO contrast window");
                const auto value=field.get<double>();
                zarr_require(std::isfinite(value),"invalid OMERO contrast window");
                return value;
            };
            const auto minimum=number("min"),maximum=number("max");
            channel.start=number("start");channel.end=number("end");
            zarr_require(minimum<=maximum && channel.start<channel.end && std::isfinite(channel.end-channel.start),
                "invalid OMERO contrast window");
            result.channels.push_back(channel);
        }
    }catch(const J::exception&){
        result.error="invalid OMERO display metadata";
    }catch(const std::invalid_argument& error){
        result.error=error.what();
    }
    return result;
}
#endif
}
ZarrMetadata zarr_metadata(const ZarrStore& store,const std::atomic<bool>* cancelled){
    ZarrMetadata result;
    const auto modern=store.layout()==NgffLayout::v05;
    const auto group=modern?ngff05_json(store.read("zarr.json",store.options.metadata_bytes)):json(store,".zgroup");
    zarr_require(group.at("zarr_format")== (modern?3:2),modern?"requires Zarr3 group":"requires Zarr2 group");
    if(modern)zarr_require(group.at("zarr_format").is_number_integer() && group.at("node_type")=="group","requires Zarr3 group");
    const auto attrs=modern?group.at("attributes").at("ome"):json(store,".zattrs");
    if(modern)zarr_require(attrs.at("version")=="0.5","requires NGFF0.5");
    const auto& multiscales=attrs.at("multiscales");zarr_require(multiscales.is_array() && multiscales.size()==1,"select a single NGFF image group");const auto& m=multiscales[0];
    if(!modern)zarr_require(m.at("version")=="0.4","requires NGFF0.4");
    const auto& axes=m.at("axes");zarr_require(axes.is_array() && axes.size()>=2 && axes.size()<=5,"requires2..5 axes");std::set<std::string> unique;
    for(std::size_t a=0;a<axes.size();++a){const auto& axis=axes[a];const auto name=axis.at("name").get<std::string>(),type=axis.at("type").get<std::string>(),u=axis.value("unit",std::string{});zarr_require(unique.insert(name).second,"duplicate axis");
        int* index=nullptr;if(name=="x")index=&result.x;else if(name=="y")index=&result.y;else if(name=="z")index=&result.z;else if(name=="c")index=&result.c;else if(name=="t")index=&result.t;zarr_require(index,"unsupported axis name");*index=int(a);
        zarr_require(type==(name=="t"?"time":name=="c"?"channel":"space"),"axis type/name differ");if(!u.empty())unit(u,name=="t");zarr_require(name!="c" || u.empty(),"channel units unsupported");result.axes.push_back(name);result.units.push_back(u);result.raw.dimension_order+=char(std::toupper(name[0]));
    }
    zarr_require(result.x==int(axes.size())-1 && result.y==result.x-1 && (result.z<0 || result.z==result.y-1) && (result.c<0 || result.c<result.y) && (result.t<0 || result.t==0) && (result.c<0 || result.z<0 || result.c<result.z),"unsupported NGFF axis order");
    const auto& datasets=m.at("datasets");zarr_require(datasets.is_array() && !datasets.empty() && datasets.size()<=32,"requires1..32 pyramid levels");std::set<std::string> paths;
    for(const auto& d:datasets){workflow_checkpoint(cancelled);ZarrArray a;auto& l=a.level;l.path=d.at("path").get<std::string>();zarr_require(!l.path.empty() && l.path.size()<=4096 && l.path.find('\\')==std::string::npos && paths.insert(l.path).second,"invalid or duplicate dataset path");
        if(modern){
            zarr_require(ngff05_path(l.path),"invalid Zarr3 dataset path");
            ngff05_array(store.read(l.path+"/zarr.json",store.options.metadata_bytes),result.axes,a);
        }else{
        const auto spec_bytes=store.read(l.path+"/.zarray",store.options.metadata_bytes);const auto spec=json(spec_bytes);zarr_require(spec.at("zarr_format")==2,"requires Zarr2 array");l.shape=dimensions(spec.at("shape"),axes.size());l.chunks=dimensions(spec.at("chunks"),axes.size());
        a.type=dtype(spec.at("dtype").get<std::string>());a.big_endian=spec.at("dtype").get<std::string>()[0]=='>';const auto order=spec.at("order").get<std::string>();zarr_require(order=="C" || order=="F","unsupported byte order");a.fortran=order=="F";
        const auto& filters=spec.at("filters");zarr_require(filters.is_null() || (filters.is_array() && filters.empty()),"unsupported array filters");const auto& compressor=spec.at("compressor");if(!compressor.is_null()){a.codec=compressor.at("id").get<std::string>();zarr_require(a.codec=="zlib" || a.codec=="gzip","unsupported compressor: "+a.codec);}
        a.separator=spec.value("dimension_separator",std::string("."));zarr_require(a.separator=="." || a.separator=="/","invalid chunk separator");a.fill=checked_fill(spec.at("fill_value"),a.type,spec_bytes);a.undefined_fill=a.fill.is_null();
        }
        std::size_t bytes=sample_bytes(a.type);for(auto n:l.chunks){zarr_require(n<=store.options.chunk_bytes/bytes,"decoded chunk exceeds limit");bytes*=std::size_t(n);}a.decoded_bytes=bytes;
        if(a.decoded_crc32c)zarr_require(store.options.chunk_bytes-bytes>=sizeof(std::uint32_t),
            "decoded chunk and CRC32C exceed limit");
        l.scale.assign(axes.size(),1);l.translation.assign(axes.size(),0);transforms(d.at("coordinateTransformations"),l.scale,l.translation,true);if(m.contains("coordinateTransformations"))transforms(m.at("coordinateTransformations"),l.scale,l.translation,false);
        if(result.c>=0)zarr_require(l.scale[result.c]==1 && l.translation[result.c]==0,"nonidentity channel transform unsupported");
        if(!result.arrays.empty()){
            const auto& base=result.arrays.front();zarr_require(a.type==base.type,"mixed pyramid dtypes unsupported");const auto down=l.scale[result.x]/base.level.scale[result.x];zarr_require(std::isfinite(down) && down>=1 && down==std::floor(down) && down<=INT32_MAX && l.scale[result.x]==base.level.scale[result.x]*down && l.scale[result.y]==base.level.scale[result.y]*down,"unsupported anisotropic/nonintegral pyramid geometry");
            zarr_require(down>result.raw.levels.back().downsample,"pyramid levels must increase");
            for(int i:{result.c,result.z,result.t})if(i>=0)zarr_require(l.shape[i]==base.level.shape[i] && l.scale[i]==base.level.scale[i] && l.translation[i]==base.level.translation[i],"nonspatial pyramid axes changed");
            if(modern)ngff05_pyramid(result,l,down);
            result.raw.levels.push_back({std::int64_t(l.shape[result.x]),std::int64_t(l.shape[result.y]),down});
        }else{
            if(modern)for(const auto shift:l.translation)zarr_require(shift==0,"Zarr3 base origin is not representable by the reader ABI");
            result.raw.width=std::int64_t(l.shape[result.x]);result.raw.height=std::int64_t(l.shape[result.y]);result.raw.sample_type=a.type;
            const auto count=[&](int i){const auto n=i<0?1:l.shape[i];zarr_require(n<=65536,"C/Z/T axis exceeds65536");return std::uint32_t(n);};result.raw.channels=count(result.c);result.raw.z_planes=count(result.z);result.raw.timepoints=count(result.t);
            result.raw.levels.push_back({result.raw.width,result.raw.height,1});
            const auto calibrated=[&](int i)->std::optional<double>{if(i<0 || result.units[i].empty())return {};const auto v=l.scale[i]*unit(result.units[i],i==result.t);zarr_require(std::isfinite(v) && v>0,"calibration overflow");return v;};
            result.raw.pixel_width_um=calibrated(result.x);result.raw.pixel_height_um=calibrated(result.y);result.raw.z_spacing_um=calibrated(result.z);result.raw.time_interval_seconds=calibrated(result.t);
        }
        result.levels.push_back(l);result.arrays.push_back(std::move(a));
    }
    if(attrs.contains("omero") && attrs["omero"].contains("channels")){
        const auto& omero=attrs["omero"];
        const auto& channels=omero["channels"];
        zarr_require(channels.is_array() && channels.size()==result.raw.channels,"channel labels differ from axis");
        for(const auto& channel:channels){
            auto label=channel.value("label",std::string{});
            if(modern)bounded_utf8(label,4096);
            else zarr_require(label.size()<=4096,"channel label exceeds limit");
            result.raw.channel_names.push_back(std::move(label));
        }
#ifdef QUPATHXX_HAS_NGFF05
        if(modern)result.display=ngff05_display(omero,channels);
#endif
    }
    if(result.raw.channel_names.empty())for(std::uint32_t i=0;i<result.raw.channels;++i)result.raw.channel_names.push_back("Channel "+std::to_string(i));
    const auto level_bytes=[](const OmeZarrLevel& l){return l.path.capacity()+(l.shape.capacity()+l.chunks.capacity())*sizeof(std::uint64_t)+(l.scale.capacity()+l.translation.capacity())*sizeof(double);};
    result.payload=sizeof(result)+result.raw.payload_bytes()+result.arrays.capacity()*sizeof(ZarrArray)+result.levels.capacity()*sizeof(OmeZarrLevel)+(result.axes.capacity()+result.units.capacity())*sizeof(std::string);
    for(std::size_t i=0;i<result.axes.size();++i)result.payload+=result.axes[i].capacity()+result.units[i].capacity();
    for(const auto& a:result.arrays)result.payload+=level_bytes(a.level)+a.codec.capacity()+a.separator.capacity()+a.fill.dump().size()+64;
    for(const auto& l:result.levels)result.payload+=level_bytes(l);
    if(result.display)result.payload+=result.display->channels.capacity()*sizeof(Ngff05DisplayChannel)+
        result.display->error.capacity();
    return result;
}
}
