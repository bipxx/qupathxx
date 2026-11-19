// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include <windows.h>
#include <zlib.h>
#include <charconv>
#include <fstream>
#include <map>
#include <string_view>

namespace {
using namespace qpx_image_plugin;
namespace fs = std::filesystem;
constexpr std::size_t header_limit=65536;
constexpr std::size_t companion_limit=63;
constexpr auto abi=QPX_READER_ABI_V1;

bool horizontal(char c) { return c==' ' || c=='\t'; }
std::string_view trim(std::string_view text) {
    while(!text.empty() && horizontal(text.front()))text.remove_prefix(1);
    while(!text.empty() && horizontal(text.back()))text.remove_suffix(1);
    return text;
}
template<class T> T number(std::string_view text) {
    valid(!text.empty(),"Missing MetaImage number");
    T value{};
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    valid(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size(),
          "Invalid MetaImage number");
    return value;
}
template<class T,std::size_t N>
std::array<T,N> numbers(std::string_view text,unsigned count) {
    std::array<T,N> values{};
    for(unsigned i=0;i<count;++i) {
        text=trim(text);
        const auto end=text.find_first_of(" \t");
        values[i]=number<T>(text.substr(0,end));
        text=end==std::string_view::npos?std::string_view{}:text.substr(end);
    }
    valid(trim(text).empty(),"MetaImage numeric field has extra values");
    return values;
}
bool boolean(std::string_view text) {
    valid(text=="True" || text=="False","MetaImage flags require True or False");
    return text=="True";
}
struct Sample {unsigned type=0,bytes=0;double low=0,high=0;};
Sample sample_type(std::string_view name) {
    if(name=="MET_CHAR")return {QPX_READER_INT8,1,-128,127};
    if(name=="MET_UCHAR")return {QPX_READER_UINT8,1,0,255};
    if(name=="MET_SHORT")return {QPX_READER_INT16,2,-32768,32767};
    if(name=="MET_USHORT")return {QPX_READER_UINT16,2,0,65535};
    if(name=="MET_INT")return {QPX_READER_INT32,4,INT32_MIN,INT32_MAX};
    if(name=="MET_UINT")return {QPX_READER_UINT32,4,0,UINT32_MAX};
    if(name=="MET_LONG_LONG")return {QPX_READER_INT64,8,static_cast<double>(INT64_MIN),static_cast<double>(INT64_MAX)};
    if(name=="MET_ULONG_LONG")return {QPX_READER_UINT64,8,0,static_cast<double>(UINT64_MAX)};
    if(name=="MET_FLOAT")return {QPX_READER_FLOAT32,4,-1,1};
    if(name=="MET_DOUBLE")return {QPX_READER_FLOAT64,8,-1,1};
    fail(QPX_READER_UNSUPPORTED,"MetaImage element type is outside the fixed-width profile");
}
fs::path primary_path(const qpx_reader_open_v1* request) {
    require(request && request->struct_size>=sizeof(*request) && !request->reserved &&
        request->path.data && request->path.size && request->path.size<=32768,"Invalid MetaImage open request");
    const std::string_view input(request->path.data,std::size_t(request->path.size));
    require(input.find('\0')==std::string_view::npos,"MetaImage path contains NUL");
    const auto path=fs::u8path(input.begin(),input.end());
    require(path.is_absolute(),"MetaImage requires an absolute input path");
    return path;
}
std::string companion_name(std::string_view name) {
    const auto acceptable=[](char c) {
        return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') ||
               c=='-' || c=='_' || c=='.' || c==' ';
    };
    if(name.empty() || name.size()>255 || name.front()=='.' || name.back()=='.' || trim(name)!=name || name=="LIST" ||
       !std::all_of(name.begin(),name.end(),acceptable))
        fail(QPX_READER_UNSUPPORTED,"MetaImage requires a plain same-directory payload name");
    std::string stem(trim(name.substr(0,name.find('.'))));
    for(auto& c:stem)if(c>='a' && c<='z')c=char(c-'a'+'A');
    if(stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" ||
       (stem.size()==4 && (stem.compare(0,3,"COM")==0 || stem.compare(0,3,"LPT")==0) &&
        stem.back()>='0' && stem.back()<='9'))
        fail(QPX_READER_UNSUPPORTED,"MetaImage payload name is a reserved device name");
    return std::string(name);
}
std::vector<std::string> numbered_slices(std::string_view locator, std::uint32_t count,
                                         qpx_reader_cancel_v1 cancel) {
    const auto last_separator=locator.find_last_of(" \t");
    valid(last_separator!=std::string_view::npos,"MetaImage numbered slices require a decimal pattern and range");
    const auto before_last=trim(locator.substr(0,last_separator));
    const auto previous_separator=before_last.find_last_of(" \t");
    valid(previous_separator!=std::string_view::npos,"MetaImage numbered slices require start and end indices");
    const auto before_end=trim(before_last.substr(0,previous_separator));
    const auto third_separator=before_end.find_last_of(" \t");
    const bool has_step=third_separator!=std::string_view::npos;
    const auto separator=has_step?third_separator:previous_separator;
    const auto pattern=trim(locator.substr(0,separator));
    const auto marker=pattern.find('%');
    const auto conversion=pattern.find('d',marker);
    valid(marker!=std::string_view::npos && conversion!=std::string_view::npos,
          "MetaImage numbered slices require a decimal pattern and range");
    const auto padding=pattern.substr(marker+1,conversion-marker-1);
    unsigned width=0;
    if(!padding.empty()) {
        valid(padding.size()>=2 && padding.size()<=3 && padding.front()=='0' && padding[1]!='0',
              "MetaImage numbered slices require %d or a zero-padded decimal pattern");
        width=number<unsigned>(padding.substr(1));
        valid(width>=1 && width<=11,"MetaImage numbered slice width exceeds 11");
    }
    const auto values=numbers<std::int32_t,3>(locator.substr(separator),has_step?3:2);
    const std::int64_t first=values[0],last=values[1],step=has_step?values[2]:1;
    valid(step>0 && first<=last,"MetaImage numbered slices require an ascending range and positive step");
    valid(1+(last-first)/step==count,"MetaImage numbered range does not match Z size");
    std::vector<std::string> names;
    names.reserve(count);
    for(std::uint32_t z=0;z<count;++z) {
        checkpoint(cancel);
        auto digits=std::to_string(first+step*z);
        if(digits.size()<width) digits.insert(digits.front()=='-'?1:0,width-digits.size(),'0');
        std::string name(pattern.substr(0,marker));
        name+=digits;
        name+=pattern.substr(conversion+1);
        names.push_back(companion_name(name));
    }
    return names;
}
fs::path companion_path(const fs::path& primary,const std::string& name) {
    const auto result=primary.parent_path()/fs::u8path(name);
    const auto attributes=GetFileAttributesW(result.c_str());
    valid(attributes!=INVALID_FILE_ATTRIBUTES,"MetaImage companion is missing");
    if((attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) ||
       fs::equivalent(primary,result))
        fail(QPX_READER_UNSUPPORTED,"MetaImage companion must be a distinct regular file");
    return result;
}
struct Header {
    std::vector<std::string> companions;
    std::array<std::uint32_t,3> shape{1,1,1};
    std::array<double,3> spacing{};
    unsigned rank=0,calibration=0,channels=1;
    Sample sample;
    std::uint64_t payload=0,bytes=0,encoded_bytes=0;
    std::int64_t skip=0;
    bool big=false,compressed=false,slices=false;
};

bool read_header_line(std::ifstream& stream,std::string& line,std::size_t& offset) {
    line.clear();
    while(offset<header_limit) {
        const auto ch=stream.get();
        if(ch==EOF)break;
        ++offset;
        if(ch=='\n') {
            if(!line.empty() && line.back()=='\r')line.pop_back();
            return true;
        }
        valid(ch=='\t' || ch=='\r' || (ch>=32 && ch<=126),"MetaImage header must be ASCII");
        line.push_back(char(ch));
    }
    if(!line.empty() && line.back()=='\r')line.pop_back();
    return false;
}

Header read_header(std::ifstream& stream,qpx_reader_cancel_v1 cancel) {
    valid(bool(stream),"Cannot open MetaImage header");
    constexpr std::string_view allowed[]={"ObjectType","NDims","BinaryData","BinaryDataByteOrderMSB",
        "ElementByteOrderMSB","CompressedData","TransformMatrix","Offset","Position","Origin",
        "CenterOfRotation","ElementSpacing","ElementSize","DimSize","ElementNumberOfChannels",
        "ElementType","ElementDataFile","HeaderSize","AnatomicalOrientation","CompressedDataSize"};
    std::map<std::string,std::string,std::less<>> fields;
    std::string line;
    Header header;
    bool terminated=false;
    std::size_t offset=0;
    while(offset<header_limit) {
        checkpoint(cancel);
        const bool newline=read_header_line(stream,line,offset);
        const auto text=trim(line);
        if(text.empty() || text.front()=='#') {
            if(!newline)break;
            continue;
        }
        const auto delimiter=text.find('=');
        valid(delimiter!=std::string_view::npos,"MetaImage header requires key = value fields");
        const auto key=trim(text.substr(0,delimiter)),value=trim(text.substr(delimiter+1));
        valid(!key.empty() && !value.empty(),"MetaImage header field is empty");
        if(std::find(std::begin(allowed),std::end(allowed),key)==std::end(allowed))
            fail(QPX_READER_UNSUPPORTED,"MetaImage header contains an unsupported field");
        valid(fields.emplace(key,value).second,"MetaImage header contains a duplicate field");
        if(key=="ElementDataFile") {
            if(value=="LOCAL") {
                valid(newline,"MetaImage LOCAL payload must follow its header line");
                header.payload=offset;
            } else if(value=="LIST" || value=="LIST 2D" || value=="LIST 2") {
                valid(newline,"MetaImage LIST requires slice names below its header");
                header.slices=true;
            } else {
                if(value.find('%')==std::string_view::npos) header.companions.push_back(companion_name(value));
                valid(stream.peek()==EOF,"MetaImage detached header has trailing entries");
            }
            terminated=true;
            break;
        }
        if(!newline)break;
    }
    valid(terminated,"MetaImage terminal data field is missing or past 64 KiB");
    const auto find=[&](const char* name)->std::string_view {
        const auto it=fields.find(name);
        return it==fields.end()?std::string_view{}:it->second;
    };
    if((!find("ObjectType").empty() && find("ObjectType")!="Image") ||
       (!find("BinaryData").empty() && !boolean(find("BinaryData"))))
        fail(QPX_READER_UNSUPPORTED,"MetaImage profile requires a binary image");
    if(const auto channels=find("ElementNumberOfChannels");!channels.empty())
        header.channels=number<unsigned>(channels);
    require(header.channels && header.channels<=65536,"Invalid MetaImage channel count");
    const auto compressed=find("CompressedData");
    header.compressed=!compressed.empty() && boolean(compressed);
    if(const auto size=find("CompressedDataSize"); !size.empty()) {
        header.encoded_bytes=number<std::uint64_t>(size);
        valid(header.compressed && header.encoded_bytes>0,"Invalid MetaImage CompressedDataSize");
    }
    header.rank=number<unsigned>(find("NDims"));
    if(header.rank!=2 && header.rank!=3)fail(QPX_READER_UNSUPPORTED,"MetaImage profile requires XY or XYZ");
    header.sample=sample_type(find("ElementType"));
    const auto dims=numbers<std::uint32_t,3>(find("DimSize"),header.rank);
    header.bytes=std::uint64_t(header.sample.bytes)*header.channels;
    for(unsigned i=0;i<header.rank;++i) {
        require(dims[i] && dims[i]<=work_limit/header.bytes,"MetaImage raster exceeds the reader bound");
        header.bytes*=dims[i];
        header.shape[i]=dims[i];
    }
    const auto order=find("BinaryDataByteOrderMSB"),alias=find("ElementByteOrderMSB");
    if(!order.empty() && !alias.empty())valid(boolean(order)==boolean(alias),"MetaImage byte-order fields conflict");
    const auto selected=order.empty()?alias:order;
    valid(header.sample.bytes==1 || !selected.empty(),"MetaImage requires explicit byte order for multibyte samples");
    header.big=!selected.empty() && boolean(selected);
    if(!find("HeaderSize").empty())header.skip=number<std::int64_t>(find("HeaderSize"));
    const auto locator=find("ElementDataFile");
    const bool numbered=locator.find('%')!=std::string_view::npos;
    if(header.skip<-1 || (!header.slices && !numbered && header.companions.empty() && header.skip))
        fail(QPX_READER_UNSUPPORTED,"MetaImage header skip is supported only for detached payloads");
    if(header.slices || numbered) {
        require(header.rank==3 && header.shape[2]<=companion_limit,
                numbered?"MetaImage numbered slices require XYZ with at most 63 slices":
                         "MetaImage LIST requires XYZ with at most 63 slices");
        valid(!header.compressed || header.skip>=0,"MetaImage compressed slices require a nonnegative header offset");
    }
    if(numbered) {
        header.companions=numbered_slices(locator,header.shape[2],cancel);
    } else if(header.slices) {
        for(std::uint32_t z=0;z<header.shape[2];++z) {
            checkpoint(cancel);
            const auto newline=read_header_line(stream,line,offset);
            const auto name=trim(line);
            valid(!name.empty(),"MetaImage LIST has fewer names than slices");
            valid(newline || (z+1==header.shape[2] && stream.peek()==EOF),
                  "MetaImage LIST exceeds header bounds or has fewer names than slices");
            auto item=companion_name(name);
            valid(std::none_of(header.companions.begin(),header.companions.end(),[&](const auto& existing) {
                return _stricmp(existing.c_str(),item.c_str())==0;
            }),"MetaImage LIST has a duplicate slice name");
            header.companions.push_back(std::move(item));
        }
        valid(stream.peek()==EOF && !stream.bad(),"MetaImage LIST has trailing entries");
    }
    valid(!header.compressed || header.skip!=-1 || header.encoded_bytes>0,
          "MetaImage compressed EOF-relative offset requires CompressedDataSize");
    const auto transform=find("TransformMatrix");
    if(!transform.empty()) {
        const auto matrix=numbers<double,9>(transform,header.rank*header.rank);
        for(unsigned i=0;i<header.rank;++i)for(unsigned j=0;j<header.rank;++j)
            if(matrix[i*header.rank+j]!=(i==j?1:0))
                fail(QPX_READER_UNSUPPORTED,"MetaImage transform must be identity");
    }
    // ITK derives the XYZ acronym from its direction matrix. Its XY writer emits unknown axes.
    const auto orientation=find("AnatomicalOrientation");
    if(!orientation.empty() && (transform.empty() || orientation!=(header.rank==2?"??":"RAI")))
        fail(QPX_READER_UNSUPPORTED,"MetaImage anatomical orientation requires the verified identity geometry");
    for(const auto* key:{"Offset","Position","Origin","CenterOfRotation"}) {
        if(find(key).empty())continue;
        const auto origin=numbers<double,3>(find(key),header.rank);
        if(std::any_of(origin.begin(),origin.end(),[](double value){return value!=0;}))
            fail(QPX_READER_UNSUPPORTED,"MetaImage origin and rotation center must be zero");
    }
    const auto spacing=find("ElementSpacing"),size=find("ElementSize");
    const auto length=spacing.empty()?size:spacing;
    if(!length.empty()) {
        const auto mm=numbers<double,3>(length,header.rank);
        if(!spacing.empty() && !size.empty() && numbers<double,3>(size,header.rank)!=mm)
            fail(QPX_READER_UNSUPPORTED,"MetaImage voxel size and spacing differ");
        for(unsigned i=0;i<header.rank;++i) {
            header.spacing[i]=mm[i]*1000;
            valid(std::isfinite(header.spacing[i]) && header.spacing[i]>0,"Invalid MetaImage spacing");
            header.calibration|=1u<<i;
        }
    }
    return header;
}
void inflate_raster(std::istream& input,std::uint64_t encoded,std::uint8_t* output,
                    std::size_t expected,Budget& budget,qpx_reader_cancel_v1 cancel) {
    constexpr std::size_t chunk_size=65536,codec_workspace=65536;
    auto chunk=budget.array(chunk_size);
    require(budget.remaining>=codec_workspace,"MetaImage codec exceeds the reader work budget");
    auto available=codec_workspace;
    z_stream decoder{};
    decoder.opaque=&available;
    decoder.zalloc=[](voidpf opaque,uInt items,uInt width) -> voidpf {
        auto& remaining=*static_cast<std::size_t*>(opaque);
        if(width && items>remaining/width)return nullptr;
        auto memory=std::calloc(items,width);
        if(memory)remaining-=std::size_t(items)*width;
        return memory;
    };
    decoder.zfree=[](voidpf,voidpf memory){std::free(memory);};
    if(inflateInit(&decoder)!=Z_OK)fail(QPX_READER_INTERNAL_ERROR,"Cannot initialize MetaImage zlib decoder");
    struct End { z_stream& decoder; ~End(){inflateEnd(&decoder);} } end{decoder};
    std::uint64_t supplied=0;
    std::uint8_t overflow=0;
    for(;;) {
        checkpoint(cancel);
        if(!decoder.avail_in && supplied<encoded) {
            const auto count=std::min<std::uint64_t>(chunk_size,encoded-supplied);
            valid(bool(input.read(reinterpret_cast<char*>(chunk.get()),std::streamsize(count))),
                  "MetaImage compressed payload was truncated");
            decoder.next_in=chunk.get();
            decoder.avail_in=static_cast<uInt>(count);
            supplied+=count;
        }
        if(!decoder.avail_out) {
            const auto written=std::size_t(decoder.total_out);
            decoder.next_out=written==expected?&overflow:output+written;
            decoder.avail_out=static_cast<uInt>(written==expected?1:std::min(chunk_size,expected-written));
        }
        const auto status=inflate(&decoder,Z_NO_FLUSH);
        valid(decoder.total_out<=expected,"MetaImage decoded raster exceeds the declared size");
        if(status==Z_STREAM_END)break;
        if(status==Z_MEM_ERROR)fail(QPX_READER_INTERNAL_ERROR,"MetaImage zlib workspace allocation failed");
        valid(status==Z_OK,"MetaImage zlib stream is corrupt or truncated");
    }
    valid(supplied==encoded && !decoder.avail_in && decoder.total_out==expected,
          "MetaImage compressed raster has the wrong decoded extent or trailing data");
}

void read_raster(const fs::path& path,std::istream& stream,const Header& header,
                 std::uint8_t* output,std::size_t expected,bool local,Budget budget,qpx_reader_cancel_v1 cancel) {
    const auto extent=fs::file_size(path);
    auto stored=header.compressed?header.encoded_bytes:expected;
    std::uint64_t payload=header.payload;
    if(!local && header.skip==-1) {
        valid(extent>=stored,"MetaImage raster has the wrong extent");
        payload=extent-stored;
    } else if(!local) payload=std::uint64_t(header.skip);
    valid(extent>=payload,"MetaImage raster has the wrong extent");
    const auto available=extent-payload;
    if(header.compressed && !stored)stored=available;
    valid(stored>0 && stored<=work_limit && available==stored,"MetaImage raster has the wrong extent");
    valid(payload<=std::uint64_t(std::numeric_limits<std::streamoff>::max()) &&
          bool(stream.seekg(std::streamoff(payload))),"Cannot seek to MetaImage raster");
    if(header.compressed)inflate_raster(stream,stored,output,expected,budget,cancel);
    constexpr std::size_t step=65536;
    for(std::size_t at=0;at<expected;at+=step) {
        checkpoint(cancel);
        const auto n=std::min(step,expected-at);
        if(!header.compressed)
            valid(bool(stream.read(reinterpret_cast<char*>(output+at),std::streamsize(n))),"MetaImage raster was truncated");
        if(header.big && header.sample.bytes>1)
            for(std::size_t i=at;i<at+n;i+=header.sample.bytes)
                std::reverse(output+i,output+i+header.sample.bytes);
    }
    valid(stream.peek()==EOF && !stream.bad(),"MetaImage raster changed while reading");
}
struct Image {Header header;std::unique_ptr<std::uint8_t[]> samples;};
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&] {
        require(out && out->struct_size>=sizeof(*out),"Invalid MetaImage descriptor buffer");
        out->abi_version=abi;out->capabilities=QPX_READER_CAP_RAW|QPX_READER_CAP_RGBA|QPX_READER_CAP_SOURCE_FILES;
        bool complete=text(out->provider_id,"org.qupathxx.metaimage");
        complete=text(out->display_name,"MetaImage") && complete;
        complete=text(out->version,QPX_METAIMAGE_VERSION) && complete;
        complete=text(out->contract,QPX_METAIMAGE_CONTRACT) && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"Short MetaImage descriptor buffer");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&] {
        require(out,"Null MetaImage handle output");*out=nullptr;
        const auto path=primary_path(request);
        require(request->work_bytes<=work_limit && request->work_bytes>=4*header_limit && !request->options.size,
                "MetaImage requires a bounded workspace and empty options");
        std::ifstream primary(path,std::ios::binary);
        auto header=read_header(primary,request->cancel);
        Budget budget{request->work_bytes-4*header_limit};
        auto data=budget.array(header.bytes);
        if(header.companions.empty()) {
            read_raster(path,primary,header,data.get(),std::size_t(header.bytes),true,budget,request->cancel);
        } else {
            const auto bytes=std::size_t(header.bytes)/header.companions.size();
            for(std::size_t i=0;i<header.companions.size();++i) {
                checkpoint(request->cancel);
                const auto source=companion_path(path,header.companions[i]);
                std::ifstream detached(source,std::ios::binary);
                valid(bool(detached),"Cannot open MetaImage companion");
                read_raster(source,detached,header,data.get()+i*bytes,bytes,false,budget,request->cancel);
            }
        }
        *out=new Image{std::move(header),std::move(data)};
        return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {delete static_cast<Image*>(value);}
const Image& image(void* value){require(value,"Null MetaImage handle");return *static_cast<Image*>(value);}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* out,
        qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&] {
        const auto& h=image(value).header;checkpoint(cancel);
        require(out && out->struct_size>=sizeof(*out),"Short MetaImage metadata buffer");
        out->level_count=out->has_raw=out->timepoints=out->series_count=1;
        out->channels=h.channels;
        out->width=h.shape[0];out->height=h.shape[1];out->z_planes=h.shape[2];out->series=0;
        out->sample_type=h.sample.type;out->calibration_flags=h.calibration;
        out->pixel_width_um=h.spacing[0];out->pixel_height_um=h.spacing[1];out->z_spacing_um=h.spacing[2];out->time_interval_seconds=0;
        const auto vendor=text(out->vendor,"MetaImage");
        const char* axes=h.channels==1?(h.rank==2?"XY":"XYZ"):(h.rank==2?"CXY":"CXYZ");
        return text(out->dimension_order,axes) && vendor?QPX_READER_OK:
            failure(error,QPX_READER_BUFFER_TOO_SMALL,"Short MetaImage metadata text buffer");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&] {
        const auto& h=image(value).header;
        require(out && out->struct_size>=sizeof(*out) && !index,"Invalid MetaImage level");
        *out={sizeof(*out),0,h.shape[0],h.shape[1],1};return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value,std::uint32_t index,qpx_reader_text_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&] {
        const auto count=image(value).header.channels;
        require(out && index<count,"Invalid MetaImage channel index");
        const auto name=count==1?"Scalar":"Component "+std::to_string(index+1);
        return text(*out,name.c_str())?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"Short MetaImage channel buffer");
    });
}
template<class T>T load(const std::uint8_t* sample) {T value{};std::memcpy(&value,sample,sizeof(value));return value;}
void display(const Sample& type,const std::uint8_t* sample,std::uint8_t* out) {
    double value=0;
    switch(type.type) {
    case QPX_READER_INT8:value=load<std::int8_t>(sample);break;
    case QPX_READER_UINT8:value=*sample;break;
    case QPX_READER_INT16:value=load<std::int16_t>(sample);break;
    case QPX_READER_UINT16:value=load<std::uint16_t>(sample);break;
    case QPX_READER_INT32:value=load<std::int32_t>(sample);break;
    case QPX_READER_UINT32:value=load<std::uint32_t>(sample);break;
    case QPX_READER_INT64:value=static_cast<double>(load<std::int64_t>(sample));break;
    case QPX_READER_UINT64:value=static_cast<double>(load<std::uint64_t>(sample));break;
    case QPX_READER_FLOAT32:
        if((load<std::uint32_t>(sample)&0x7f800000u)==0x7f800000u)return;
        value=load<float>(sample);break;
    case QPX_READER_FLOAT64:
        if((load<std::uint64_t>(sample)&0x7ff0000000000000ull)==0x7ff0000000000000ull)return;
        value=load<double>(sample);break;
    }
    const auto gray=std::uint8_t(std::floor(std::clamp((value-type.low)/(type.high-type.low),0.,1.)*255+.5));
    out[0]=out[1]=out[2]=gray;out[3]=255;
}
qpx_reader_status_v1 read_pixels(const Image& source,const qpx_reader_region_v1* req,
        qpx_reader_pixels_v1* out,bool raw,qpx_reader_cancel_v1 cancel) {
    require(out && out->struct_size>=sizeof(*out),"Short MetaImage pixel descriptor");
    out->written=out->validity_written=0;
    const auto& h=source.header;
    require(req && req->struct_size>=sizeof(*req) && !req->level && !req->t &&
        (raw ? req->z<h.shape[2] && req->first_channel<h.channels && req->channels &&
               req->channels<=h.channels-req->first_channel :
               !req->z && !req->first_channel && !req->channels),"Invalid MetaImage plane or channel");
    const auto w=req->width,d=req->height;
    const unsigned stride=raw?h.sample.bytes*req->channels:4, cost=stride+(raw?1:0);
    require(w>0 && d>0 && std::uint64_t(w)<=work_limit/cost && std::uint64_t(d)<=work_limit/cost/w &&
        req->x<=INT64_MAX-w && req->y<=INT64_MAX-d,"Invalid MetaImage region extent");
    const auto count=std::uint64_t(w)*d;
    require(out->data && out->capacity>=count*stride && !out->reserved &&
        out->width==w && out->height==d && out->sample_type==(raw?h.sample.type:QPX_READER_UINT8) &&
        out->channels==(raw?req->channels:4u) && out->pixel_stride==stride && out->row_stride==std::uint64_t(w)*stride &&
        out->channel_stride==(raw?h.sample.bytes:1u) &&
        (raw?out->validity && out->validity_capacity>=count:!out->validity && !out->validity_capacity),
        "Invalid MetaImage pixel buffer layout");
    checkpoint(cancel);std::memset(out->data,0,std::size_t(count*stride));
    if(raw)std::memset(out->validity,0,std::size_t(count));
    for(std::int64_t row=0;row<d;++row) {
        checkpoint(cancel);
        for(std::int64_t col=0;col<w;++col) {
            if((col&4095)==0)checkpoint(cancel);
            const auto x=req->x+col,y=req->y+row;
            if(x<0 || y<0 || x>=h.shape[0] || y>=h.shape[1])continue;
            const auto at=(std::uint64_t(req->z)*h.shape[1]+y)*h.shape[0]+x;
            const auto* sample=source.samples.get()+(at*h.channels+req->first_channel)*h.sample.bytes;
            const auto index=std::uint64_t(row)*w+col;
            auto* target=out->data+index*stride;
            if(raw){std::memcpy(target,sample,stride);out->validity[index]=1;}
            else display(h.sample,sample,target);
        }
    }
    checkpoint(cancel);out->written=count*stride;out->validity_written=raw?count:0;
    return QPX_READER_OK;
}
qpx_reader_status_v1 QPX_READER_CALL rgba(void* v,const qpx_reader_region_v1* r,qpx_reader_pixels_v1* o,qpx_reader_text_v1* e) noexcept {
    return boundary(e,[&]{return read_pixels(image(v),r,o,false,{});});
}
qpx_reader_status_v1 QPX_READER_CALL raw(void* v,const qpx_reader_region_v1* r,qpx_reader_pixels_v1* o,
        qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* e) noexcept {
    return boundary(e,[&]{return read_pixels(image(v),r,o,true,cancel);});
}
qpx_reader_status_v1 QPX_READER_CALL source_count(const qpx_reader_open_v1* request,std::uint32_t* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]{
        require(out,"Null MetaImage source count");const auto path=primary_path(request);
        std::ifstream stream(path,std::ios::binary);
        *out=static_cast<std::uint32_t>(read_header(stream,request->cancel).companions.size());return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request,std::uint32_t index,
        qpx_reader_text_v1* out,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]{
        require(out,"Invalid MetaImage companion output");const auto path=primary_path(request);
        std::ifstream stream(path,std::ios::binary);const auto header=read_header(stream,request->cancel);
        require(index<header.companions.size(),"Invalid MetaImage companion index");
        return text(*out,companion_path(path,header.companions[index]).u8string().c_str())?QPX_READER_OK:
            failure(error,QPX_READER_BUFFER_TOO_SMALL,"Short MetaImage source path buffer");
    });
}
}
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
        std::uint32_t requested,qpx_reader_api_v1* out,qpx_reader_text_v1* error) {
    return boundary(error,[&]{
        require(out && out->struct_size>=sizeof(*out) && requested==abi,"Invalid MetaImage ABI entry request");
        *out={sizeof(*out),abi,describe,open,destroy,metadata,level,channel_name,rgba,raw};return QPX_READER_OK;
    });
}
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(
        std::uint32_t requested,qpx_reader_files_api_v1* out,qpx_reader_text_v1* error) {
    return boundary(error,[&]{
        require(out && out->struct_size>=sizeof(*out) && requested==abi,"Invalid MetaImage source ABI entry request");
        *out={sizeof(*out),abi,source_count,source_path};return QPX_READER_OK;
    });
}
