#include "reader_plugin_profile.hpp"
#include <qupathxx/reader_plugin.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
constexpr std::uint64_t work_limit=UINT64_C(256)*1024*1024;
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"ICS Float32 requires IEEE-754 binary32");
static_assert(sizeof(std::int16_t)==2 && std::numeric_limits<std::int16_t>::min()==-32768,"ICS Int16 requires the declared signed range");
struct Failure {qpx_reader_status_v1 status;const char* message;};
[[noreturn]] void fail(qpx_reader_status_v1 status,const char* message) {throw Failure{status,message};}
void require(bool condition,const char* message) {if(!condition)fail(QPX_READER_INVALID_REQUEST,message);}
void valid(bool condition,const char* message) {if(!condition)fail(QPX_READER_IO_ERROR,message);}
void checkpoint(qpx_reader_cancel_v1 cancel) {
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))fail(QPX_READER_CANCELLED,"ICS operation cancelled");
}
bool text(qpx_reader_text_v1& output,const char* value) noexcept {
    const auto length=std::strlen(value);output.size=length;
    if(output.capacity<length || (length && !output.data))return false;
    if(length)std::memcpy(output.data,value,length);
    return true;
}
qpx_reader_status_v1 failure(qpx_reader_text_v1* error,qpx_reader_status_v1 status,const char* message) noexcept {
    if(error && !text(*error,message))return QPX_READER_BUFFER_TOO_SMALL;
    return status;
}
template<class Function>qpx_reader_status_v1 boundary(qpx_reader_text_v1* error,Function&& function) noexcept {
    if(error)error->size=0;
    try{return function();}
    catch(const Failure& e){return failure(error,e.status,e.message);}
    catch(const std::invalid_argument& e){return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::out_of_range& e){return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::bad_alloc&){return failure(error,QPX_READER_INTERNAL_ERROR,"ICS plugin allocation failed");}
    catch(const std::filesystem::filesystem_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::runtime_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::exception& e){return failure(error,QPX_READER_INTERNAL_ERROR,e.what());}
    catch(...){return failure(error,QPX_READER_INTERNAL_ERROR,"ICS plugin internal failure");}
}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {
    require(!a || b<=std::numeric_limits<std::uint64_t>::max()/a,"ICS decoded size overflow");
    return a*b;
}
struct Budget {
    std::uint64_t remaining;
    std::unique_ptr<std::uint8_t[]> array(std::uint64_t bytes) {
        // Payload sizes are exact new[] counts; reserve a fixed bookkeeping
        // allowance per owned array in addition to the wrapper/context budget.
        constexpr std::uint64_t bookkeeping=32;
        require(bytes<=remaining && bookkeeping<=remaining-bytes,"ICS input exceeds reader work budget");
        remaining-=bytes+bookkeeping;
        return std::unique_ptr<std::uint8_t[]>(new std::uint8_t[static_cast<std::size_t>(bytes)]);
    }
};
constexpr std::size_t header_limit=65536;
constexpr std::uint32_t channel_limit=256;
bool digit(char value) {return value>='0' && value<='9';}
struct Window {std::uint32_t channel=0;double lower=0,upper=255,span=255;};
double json_number(std::string_view value,std::size_t& position) {
    const auto start=position;
    if(position<value.size() && value[position]=='-')++position;
    require(position<value.size() && digit(value[position]),"ICS window requires JSON numbers");
    if(value[position]=='0') {++position;require(position==value.size() || !digit(value[position]),"ICS JSON number has a leading zero");}
    else while(position<value.size() && digit(value[position]))++position;
    if(position<value.size() && value[position]=='.') {
        ++position;const auto first=position;
        while(position<value.size() && digit(value[position]))++position;
        require(position>first,"ICS JSON fraction is incomplete");
    }
    if(position<value.size() && (value[position]=='e' || value[position]=='E')) {
        ++position;if(position<value.size() && (value[position]=='+' || value[position]=='-'))++position;
        const auto first=position;while(position<value.size() && digit(value[position]))++position;
        require(position>first,"ICS JSON exponent is incomplete");
    }
    double result=0;
    const auto parsed=std::from_chars(value.data()+start,value.data()+position,result,std::chars_format::general);
    require(parsed.ec==std::errc{} && parsed.ptr==value.data()+position && std::isfinite(result),"ICS window numbers must be finite and representable");
    return result;
}
Window parse_window(qpx_reader_string_v1 options) {
    require(options.size<=512 && (!options.size || options.data),"ICS options are invalid");
    if(!options.size)return {};
    const std::string_view value(options.data,static_cast<std::size_t>(options.size));
    constexpr std::string_view prefix="{\"channel\":";
    require(value.substr(0,prefix.size())==prefix,"ICS options require canonical channel and window fields");
    auto position=prefix.size();Window window;
    require(position<value.size() && digit(value[position]),"ICS display channel must be an unsigned JSON integer");
    const auto start=position;
    while(position<value.size() && digit(value[position])) {
        const auto next=static_cast<unsigned>(value[position++]-'0');
        require(window.channel<=UINT32_MAX/10 && (window.channel<UINT32_MAX/10 || next<=UINT32_MAX%10),"ICS display channel overflows");
        window.channel=window.channel*10+next;
    }
    require(position-start==1 || value[start]!='0',"ICS display channel has a leading zero");
    constexpr std::string_view middle=",\"window\":[";
    require(value.substr(position,middle.size())==middle,"ICS options require both channel and window");position+=middle.size();
    window.lower=json_number(value,position);
    require(position<value.size() && value[position++]==',',"ICS window requires two numbers");
    window.upper=json_number(value,position);
    require(value.substr(position)=="]}","ICS options have unexpected or trailing fields");
    window.span=window.upper-window.lower;
    require(window.lower<window.upper && std::isfinite(window.span) && window.span>0,"ICS window must have a finite positive span");
    return window;
}
struct ReadFile {
    std::unique_ptr<std::uint8_t[]> bytes;
    std::size_t size=0;
    std::uint64_t total_size=0;
};
ReadFile read_file(const std::filesystem::path& path,Budget& budget,qpx_reader_cancel_v1 cancel,
    std::size_t maximum=SIZE_MAX,std::uint64_t expected=UINT64_MAX) {
    checkpoint(cancel);ReadFile result;result.total_size=std::filesystem::file_size(path);
    if(expected!=UINT64_MAX)valid(result.total_size==expected,"ICS IDS payload is truncated or has trailing data");
    const auto count=std::min<std::uint64_t>(maximum,result.total_size);
    require(count<=budget.remaining && 32<=budget.remaining-count,"ICS input exceeds reader work budget");
    result.size=static_cast<std::size_t>(count);result.bytes=budget.array(count);
    std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(path.c_str(),L"rb"),std::fclose);
    if(!file)fail(QPX_READER_IO_ERROR,"ICS input cannot be opened");
    if(std::setvbuf(file.get(),nullptr,_IONBF,0))fail(QPX_READER_IO_ERROR,"ICS unbuffered input could not be configured");
    for(std::size_t position=0;position<result.size;) {
        checkpoint(cancel);const auto length=std::min<std::size_t>(65536,result.size-position);
        valid(std::fread(result.bytes.get()+position,1,length,file.get())==length,"ICS input is truncated or changed");position+=length;
    }
    if(count==result.total_size)valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"ICS input changed during reading");
    valid(std::filesystem::file_size(path)==result.total_size,"ICS input size changed during reading");
    return result;
}
struct HeaderCursor {
    const ReadFile& file;std::size_t position=0;qpx_reader_cancel_v1 cancel;
    std::string_view line() {
        const auto start=position;
        while(position<file.size && file.bytes[position]!='\n' && file.bytes[position]!='\r') {
            require(position<header_limit,"ICS header exceeds 64 KiB limit");
            if((position&4095)==0)checkpoint(cancel);
            valid(file.bytes[position]=='\t' || (file.bytes[position]>=32 && file.bytes[position]<=126),"ICS header contains invalid ASCII text");++position;
        }
        if(position==file.size && file.total_size>file.size)fail(QPX_READER_INVALID_REQUEST,"ICS header exceeds 64 KiB limit");
        valid(position<file.size,"ICS header line is truncated");
        require(position<header_limit,"ICS header exceeds 64 KiB limit");
        const auto end=position;
        if(file.bytes[position++]=='\r') {
            require(position<header_limit,"ICS header exceeds 64 KiB limit");
            valid(position<file.size && file.bytes[position]=='\n',"ICS header requires LF or CRLF lines");++position;
        }
        return {reinterpret_cast<const char*>(file.bytes.get()+start),end-start};
    }
};
struct Tokens {std::array<std::string_view,8> values{};unsigned count=0;};
Tokens split(std::string_view value) {
    Tokens result;
    for(;;) {
        valid(result.count<result.values.size(),"ICS field has too many TAB-separated values");
        const auto end=value.find('\t');result.values[result.count++]=value.substr(0,end);
        if(end==std::string_view::npos)break;
        value.remove_prefix(end+1);
    }
    return result;
}
std::uint64_t integer(std::string_view value,bool positive=true) {
    valid(!value.empty(),"ICS integer is empty");std::uint64_t result=0;
    for(char character:value) {
        valid(digit(character),"ICS requires unsigned decimal integers");
        const auto next=static_cast<unsigned>(character-'0');
        require(result<=UINT64_MAX/10 && (result<UINT64_MAX/10 || next<=UINT64_MAX%10),"ICS size integer overflows");result=result*10+next;
    }
    valid(!positive || result>0,"ICS dimensions and counts must be positive");return result;
}
double numeric(std::string_view value) {
    if(!value.empty() && value.front()=='+')value.remove_prefix(1);
    valid(!value.empty(),"ICS numeric field is empty");double result=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result,std::chars_format::general);
    valid(parsed.ec==std::errc{} && parsed.ptr==value.data()+value.size() && std::isfinite(result),"ICS header numbers must be finite and representable");return result;
}
struct Raster {
    unsigned version=0;
    std::int64_t width=0,height=0;
    std::uint32_t z_planes=1,channels=1,sample_type=0,sample_bytes=0,calibration_flags=0;
    bool has_channels=false,has_z=false;
    std::array<std::uint8_t,4> byte_order{};
    std::array<double,3> spacing_um{};
    Window window;std::size_t payload=0;std::uint64_t data_bytes=0;
    std::unique_ptr<std::uint8_t[]> primary,ids;
    const std::uint8_t* data() const {return ids?ids.get():primary.get()+payload;}
    std::uint8_t* data() {return ids?ids.get():primary.get()+payload;}
};
struct Handle {Raster image;};
Raster parse_header(const ReadFile& file,Window window,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    if(file.size<2 || file.bytes[0]!='\t' || (file.bytes[1]!='\n' && !(file.size>=3 && file.bytes[1]=='\r' && file.bytes[2]=='\n')))
        fail(QPX_READER_UNSUPPORTED,"ICS requires the TAB/newline separator declaration");
    HeaderCursor cursor{file,0,cancel};
    if(cursor.line()!="\t")fail(QPX_READER_UNSUPPORTED,"ICS separator declaration is unsupported");
    const auto version=split(cursor.line());
    if(version.count!=2 || version.values[0]!="ics_version" || (version.values[1]!="1.0" && version.values[1]!="2.0"))
        fail(QPX_READER_UNSUPPORTED,"ICS version signature is unsupported");
    const auto filename=split(cursor.line());
    valid(filename.count==2 && filename.values[0]=="filename" && !filename.values[1].empty(),"ICS descriptive filename field is malformed");
    Raster result;result.window=window;result.version=version.values[1]=="1.0"?1:2;
    enum Field {Parameters,Order,Sizes,Coordinates,Significant,Format,Sign,ByteOrder,Compression,Origin,Scale,Units,Labels,Count};
    std::array<std::string_view,Count> fields{};unsigned seen=0;bool ended=false;
    while(cursor.position<file.size) {
        const auto line=cursor.line();if(line.empty())continue;
        // The reference writer appends one field separator before newline.
        // Both forms end exactly at that line terminator; raster bytes follow.
        if(line=="end" || line=="end\t") {
            if(result.version!=2)fail(QPX_READER_UNSUPPORTED,"ICS version1 requires a separate IDS file and header EOF");
            ended=true;break;
        }
        const auto first=line.find('\t');const auto category=line.substr(0,first);
        if(category=="history")continue;
        if(category=="ics_version" || category=="filename")fail(QPX_READER_IO_ERROR,"ICS signature field is duplicated");
        if(category=="source" || category=="sensor")fail(QPX_READER_UNSUPPORTED,"ICS source or sensor interpretation is unsupported");
        if(category=="end")fail(QPX_READER_IO_ERROR,"ICS end marker contains extra values");
        if(category!="layout" && category!="representation" && category!="parameter")
            fail(QPX_READER_UNSUPPORTED,"ICS header category is unsupported");
        valid(first!=std::string_view::npos,"ICS header subcategory is missing");
        const auto rest=line.substr(first+1);
        const auto second=rest.find('\t');
        valid(second!=std::string_view::npos,"ICS header field value is missing");
        const auto subcategory=rest.substr(0,second),value=rest.substr(second+1);Field field=Count;
        if(category=="layout") {
            if(subcategory=="parameters")field=Parameters;
            else if(subcategory=="order")field=Order;
            else if(subcategory=="sizes")field=Sizes;
            else if(subcategory=="coordinates")field=Coordinates;
            else if(subcategory=="significant_bits")field=Significant;
        } else if(category=="representation") {
            if(subcategory=="format")field=Format;
            else if(subcategory=="sign")field=Sign;
            else if(subcategory=="byte_order")field=ByteOrder;
            else if(subcategory=="compression")field=Compression;
        } else {
            if(subcategory=="origin")field=Origin;
            else if(subcategory=="scale")field=Scale;
            else if(subcategory=="units")field=Units;
            else if(subcategory=="labels")field=Labels;
        }
        if(field==Count)fail(QPX_READER_UNSUPPORTED,"ICS header subcategory is unsupported");
        valid(!(seen&(1u<<field)),"ICS header field is duplicated");seen|=1u<<field;fields[field]=value;
    }
    if(file.total_size>file.size && !ended)fail(QPX_READER_INVALID_REQUEST,"ICS header exceeds 64 KiB limit");
    valid(result.version==1 || ended,"ICS version2 end marker is missing");
    constexpr unsigned mandatory=(1u<<Labels)-1;
    valid((seen&mandatory)==mandatory,"ICS required header fields are missing");
    const auto parameter_count=integer(fields[Parameters]);
    if(parameter_count<3 || parameter_count>5)fail(QPX_READER_UNSUPPORTED,"ICS axis layout is unsupported");
    const auto order=split(fields[Order]),sizes=split(fields[Sizes]);
    valid(order.count==parameter_count && sizes.count==parameter_count,"ICS layout count is inconsistent");
    if(order.values[0]!="bits")fail(QPX_READER_UNSUPPORTED,"ICS requires bits as the first layout parameter");
    unsigned x=1;
    if(order.values[1]=="ch") {result.has_channels=true;x=2;}
    const unsigned y=x+1,z=x+2;
    if(order.count!=y+1 && order.count!=z+1)fail(QPX_READER_UNSUPPORTED,"ICS supports only XY, XYZ, CXY and CXYZ order");
    if(order.values[x]!="x" || order.values[y]!="y" || (order.count==z+1 && order.values[z]!="z"))
        fail(QPX_READER_UNSUPPORTED,"ICS supports only explicit channel-first spatial axes");
    result.has_z=order.count==z+1;
    std::array<std::uint64_t,5> shape{};
    for(unsigned axis=0;axis<order.count;++axis)shape[axis]=integer(sizes.values[axis]);
    const auto bits=shape[0];
    valid(!fields[Format].empty() && !fields[Sign].empty(),"ICS numeric representation is missing");
    if(fields[Sign]!="signed" && fields[Sign]!="unsigned")fail(QPX_READER_UNSUPPORTED,"ICS numeric sign interpretation is unsupported");
    if(fields[Format]=="integer" && bits==8 && fields[Sign]=="unsigned") {result.sample_type=QPX_READER_UINT8;result.sample_bytes=1;}
    else if(fields[Format]=="integer" && bits==16) {result.sample_type=fields[Sign]=="signed"?QPX_READER_INT16:QPX_READER_UINT16;result.sample_bytes=2;}
    else if(fields[Format]=="real" && bits==32) {result.sample_type=QPX_READER_FLOAT32;result.sample_bytes=4;}
    else fail(QPX_READER_UNSUPPORTED,"ICS numeric depth or type is unsupported");
    if(integer(fields[Significant],false)!=bits)fail(QPX_READER_UNSUPPORTED,"ICS requires full-width significant bits");
    if(fields[Compression]!="uncompressed")fail(QPX_READER_UNSUPPORTED,"ICS compression is unsupported");
    if(fields[Coordinates]!="video")fail(QPX_READER_UNSUPPORTED,"ICS coordinates must explicitly be video");
    const auto channels=result.has_channels?shape[1]:1,z_planes=result.has_z?shape[z]:1;
    require(channels<=channel_limit && z_planes<=UINT32_MAX &&
        shape[x]<=static_cast<std::uint64_t>(INT64_MAX) && shape[y]<=static_cast<std::uint64_t>(INT64_MAX),"ICS dimensions exceed the declared native profile");
    result.channels=static_cast<std::uint32_t>(channels);result.z_planes=static_cast<std::uint32_t>(z_planes);
    result.width=static_cast<std::int64_t>(shape[x]);result.height=static_cast<std::int64_t>(shape[y]);
    require(window.channel<result.channels,"ICS display channel is outside the source");
    result.data_bytes=multiply(multiply(multiply(shape[x],shape[y]),z_planes),multiply(channels,result.sample_bytes));
    require(result.data_bytes<=work_limit,"ICS data exceeds reader work budget");
    const auto byte_order=split(fields[ByteOrder]);valid(byte_order.count==result.sample_bytes,"ICS byte-order count is invalid");
    unsigned byte_seen=0;
    for(unsigned byte=0;byte<result.sample_bytes;++byte) {
        const auto rank=integer(byte_order.values[byte]);
        valid(rank<=result.sample_bytes && !(byte_seen&(1u<<(rank-1))),"ICS byte-order permutation is invalid");
        byte_seen|=1u<<(rank-1);result.byte_order[byte]=static_cast<std::uint8_t>(rank-1);
    }
    const auto origin=split(fields[Origin]),scale=split(fields[Scale]),units=split(fields[Units]);
    valid(origin.count==order.count && scale.count==order.count && units.count==order.count,"ICS parameter vector count is inconsistent");
    const auto labels=split(fields[Labels]);
    if(seen&(1u<<Labels)) {
        valid(labels.count==order.count,"ICS axis-label count is inconsistent");
        for(unsigned axis=0;axis<order.count;++axis)
            if(labels.values[axis]!=(axis?order.values[axis]:"intensity"))fail(QPX_READER_UNSUPPORTED,"ICS custom axis labels are unsupported");
    }
    for(unsigned axis=0;axis<order.count;++axis) {
        if(numeric(origin.values[axis])!=0)fail(QPX_READER_UNSUPPORTED,"ICS nonzero spatial or scalar origin cannot be represented");
        const auto gap=numeric(scale.values[axis]);
        if(axis==0 || (result.has_channels && axis==1)) {
            if(gap!=1 || (axis==0?units.values[axis]!="relative":!units.values[axis].empty() && units.values[axis]!="undefined"))
                fail(QPX_READER_UNSUPPORTED,"ICS scalar or channel scaling/units cannot be represented");
            continue;
        }
        valid(gap!=0,"ICS spatial scale must not be zero");
        if(gap<0)fail(QPX_READER_UNSUPPORTED,"ICS negative spatial direction cannot be represented");
        const auto unit=units.values[axis];double factor=0;
        if(unit.empty() || unit=="undefined")continue;
        if(unit=="nm")factor=0.001;else if(unit=="um")factor=1;else if(unit=="mm")factor=1000;
        else if(unit=="cm")factor=10000;else if(unit=="m")factor=1000000;
        else fail(QPX_READER_UNSUPPORTED,"ICS spatial units are unsupported");
        const auto spatial=axis-x;result.spacing_um[spatial]=gap*factor;
        if(!std::isfinite(result.spacing_um[spatial]) || result.spacing_um[spatial]<=0)
            fail(QPX_READER_UNSUPPORTED,"ICS calibrated spacing exceeds representable bounds");
        result.calibration_flags|=1u<<spatial;
    }
    result.payload=cursor.position;
    if(result.version==2)valid(result.data_bytes==file.total_size-cursor.position,"ICS inline data is truncated or has trailing bytes");
    else valid(cursor.position==file.total_size,"ICS version1 header has trailing data");
    checkpoint(cancel);return result;
}
void normalize(Raster& image,qpx_reader_cancel_v1 cancel) {
    bool identity=true;for(unsigned byte=0;byte<image.sample_bytes;++byte)identity=identity && image.byte_order[byte]==byte;
    if(!identity)for(std::uint64_t sample=0;sample<image.data_bytes/image.sample_bytes;++sample) {
        if((sample&4095)==0)checkpoint(cancel);
        auto* value=image.data()+sample*image.sample_bytes;std::array<std::uint8_t,4> canonical{};
        for(unsigned byte=0;byte<image.sample_bytes;++byte)canonical[image.byte_order[byte]]=value[byte];
        std::memcpy(value,canonical.data(),image.sample_bytes);
    }
    checkpoint(cancel);
}
bool display_sample(const Raster& source,std::uint64_t index,double& result) {
    const auto* bytes=source.data()+index*source.sample_bytes;
    if(source.sample_type==QPX_READER_UINT8)result=*bytes;
    else if(source.sample_type==QPX_READER_UINT16) {std::uint16_t value=0;std::memcpy(&value,bytes,2);result=value;}
    else if(source.sample_type==QPX_READER_INT16) {std::int16_t value=0;std::memcpy(&value,bytes,2);result=value;}
    else {
        std::uint32_t bits=0;std::memcpy(&bits,bytes,4);
        if((bits&UINT32_C(0x7f800000))==UINT32_C(0x7f800000))return false;
        float value=0;std::memcpy(&value,&bits,4);result=value;
    }
    return true;
}
std::string path_string(qpx_reader_string_v1 value);
std::filesystem::path companion_path(const std::filesystem::path& primary) {
    auto value=primary.u8string();
    // prepare() has already checked the .ics extension. Change only its c/C.
    value[value.size()-2]=value[value.size()-2]=='C'?'D':'d';
    return std::filesystem::u8path(value).lexically_normal();
}
struct Prepared {Window window;Budget budget;std::string path;std::filesystem::path native;};
Prepared prepare(const qpx_reader_open_v1* request) {
    require(request && request->struct_size>=sizeof(*request),"ICS open structure is too small");
    require(request->reserved==0,"ICS reserved open field must be zero");
    const auto window=parse_window(request->options);
    require(request->work_bytes && request->work_bytes<=work_limit,"ICS work budget must be between 1 and 256 MiB");
    require(request->path.data && request->path.size && request->path.size<=32768,"ICS image path is invalid");
    const auto overhead=UINT64_C(8192)+16*request->path.size+request->options.size+sizeof(Handle);
    require(request->work_bytes>overhead,"ICS reader work budget is too small for opening overhead");
    checkpoint(request->cancel);auto path=path_string(request->path);auto native=std::filesystem::u8path(path).lexically_normal();
    const auto extension=native.extension().u8string();
    if(extension.size()!=4 || extension[0]!='.' || (extension[1]!='i' && extension[1]!='I') ||
        (extension[2]!='c' && extension[2]!='C') || (extension[3]!='s' && extension[3]!='S'))
        fail(QPX_READER_UNSUPPORTED,"ICS primary input must have an ICS extension");
    return {window,{request->work_bytes-overhead},std::move(path),std::move(native)};
}
qpx_reader_status_v1 QPX_READER_CALL source_count(const qpx_reader_open_v1* request,std::uint32_t* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"ICS source count output is null");*output=0;
        auto prepared=prepare(request);auto primary=read_file(prepared.native,prepared.budget,request->cancel,header_limit);
        const auto parsed=parse_header(primary,prepared.window,request->cancel);
        *output=parsed.version==1?1:0;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request,std::uint32_t index,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"ICS source path output is null");
        auto prepared=prepare(request);auto primary=read_file(prepared.native,prepared.budget,request->cancel,header_limit);
        const auto parsed=parse_header(primary,prepared.window,request->cancel);
        require(parsed.version==1 && index==0,"ICS companion index is out of range");
        const auto path=companion_path(prepared.native).u8string();
        return text(*output,path.c_str())?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"ICS companion path buffer is too small");
    });
}
const Raster& image(void* value) {require(value!=nullptr,"ICS reader handle is null");return static_cast<Handle*>(value)->image;}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"ICS image path is invalid");
    std::string path(value.data,static_cast<std::size_t>(value.size));
    require(path.find('\0')==std::string::npos,"ICS image path contains NUL");
    require(std::filesystem::u8path(path).is_absolute(),"ICS image path must be absolute");
    return path;
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output && output->struct_size>=sizeof(*output),"ICS reader info structure is too small");
        output->abi_version=QPX_READER_ABI_V1;output->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW|QPX_READER_CAP_SOURCE_FILES;
        bool complete=text(output->provider_id,"org.qupathxx.ics");
        complete=text(output->display_name,"ICS") && complete;
        complete=text(output->version,"1.0.0") && complete;
        complete=text(output->contract,"ics-raw-channel-window/1") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"ICS descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"ICS reader output handle is null");*output=nullptr;
        auto prepared=prepare(request);
        auto primary=read_file(prepared.native,prepared.budget,request->cancel);
        auto image=parse_header(primary,prepared.window,request->cancel);
        image.primary=std::move(primary.bytes);
        if(image.version==1) {
            const auto ids=companion_path(prepared.native);
            auto data=read_file(ids,prepared.budget,request->cancel,SIZE_MAX,image.data_bytes);
            image.ids=std::move(data.bytes);
        }
        normalize(image,request->cancel);
        auto result=std::make_unique<Handle>();result->image=std::move(image);
        checkpoint(request->cancel);*output=result.release();return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"ICS metadata structure is too small");checkpoint(cancel);
        output->level_count=1;output->width=source.width;output->height=source.height;
        output->has_raw=1;output->channels=source.channels;output->sample_type=source.sample_type;
        output->z_planes=source.z_planes;output->timepoints=1;output->series=0;output->series_count=1;
        output->calibration_flags=source.calibration_flags;output->pixel_width_um=source.spacing_um[0];output->pixel_height_um=source.spacing_um[1];output->z_spacing_um=source.spacing_um[2];output->time_interval_seconds=0;
        bool complete=text(output->vendor,"ICS");complete=text(output->dimension_order,source.has_channels?(source.has_z?"CXYZ":"CXY"):(source.has_z?"XYZ":"XY")) && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"ICS metadata text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"ICS level structure is too small");require(index==0,"ICS reader has only level zero");
        output->reserved=0;output->width=source.width;output->height=source.height;output->downsample=1;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value,std::uint32_t channel,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output!=nullptr,"ICS channel-name output is null");require(channel<source.channels,"ICS channel index is out of range");
        char name[16]{};std::snprintf(name,sizeof(name),"C%u",channel);
        return text(*output,name)?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"ICS channel-name buffer is too small");
    });
}
struct Region {std::int64_t width,height;std::uint64_t pixels;};
Region region(const qpx_reader_region_v1* request,std::uint64_t bytes_per_pixel) {
    require(request && request->struct_size>=sizeof(*request),"ICS region structure is too small");
    require(request->level==0 && !request->t,"ICS reader has only level zero and T=0");
    const auto width=request->width,height=request->height;
    require(width>0 && height>0 && static_cast<std::uint64_t>(width)<=work_limit/bytes_per_pixel &&
        static_cast<std::uint64_t>(height)<=work_limit/(bytes_per_pixel*static_cast<std::uint64_t>(width)),"ICS output must have positive dimensions and fit 256 MiB");
    const auto limit=std::numeric_limits<std::int64_t>::max();
    require(request->x<=limit-width && request->y<=limit-height,"ICS region coordinate overflow");
    return {width,height,static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)};
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"ICS pixel structure is too small");output->written=0;output->validity_written=0;
        const auto area=region(request,4);require(!request->first_channel && !request->channels && !request->z,"ICS display requires zero channel and Z selectors");
        const auto row_stride=static_cast<std::uint64_t>(area.width)*4,count=area.pixels*4;
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==4 && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==4 && output->channel_stride==1,"ICS RGBA output must use packed UInt8x4 layout");
        require(output->data && output->capacity>=count && !output->validity && !output->validity_capacity,"ICS RGBA output buffer is invalid");
        std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row)for(std::int64_t col=0;col<area.width;++col) {
            const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            const auto index=(static_cast<std::uint64_t>(y)*source.width+x)*source.channels+source.window.channel;
            auto* target=output->data+(static_cast<std::uint64_t>(row)*area.width+col)*4;
            double sample=0;
            if(!display_sample(source,index,sample))continue;
            const auto code=sample<=source.window.lower?0:sample>=source.window.upper?255:
                static_cast<int>(std::floor((sample-source.window.lower)/source.window.span*255+0.5));
            target[0]=target[1]=target[2]=static_cast<std::uint8_t>(code);target[3]=255;
        }
        output->written=count;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"ICS raw pixel structure is too small");output->written=0;output->validity_written=0;
        require(request && request->struct_size>=sizeof(*request),"ICS raw region structure is too small");
        require(request->channels>0 && request->first_channel<source.channels && request->channels<=source.channels-request->first_channel && request->z<source.z_planes,"ICS raw channel or Z range is invalid");
        const auto pixel_stride=static_cast<std::uint64_t>(request->channels)*source.sample_bytes;
        const auto area=region(request,pixel_stride);const auto count=area.pixels*pixel_stride,row_stride=static_cast<std::uint64_t>(area.width)*pixel_stride;
        require(area.pixels<=work_limit-count,"ICS raw output and validity exceed 256 MiB");
        const auto sample_type=source.sample_type;
        require(output->sample_type==sample_type && output->width==area.width && output->height==area.height && output->channels==request->channels && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==pixel_stride && output->channel_stride==source.sample_bytes,"ICS raw output must use packed native-endian HWC layout");
        require(output->data && output->capacity>=count && output->validity && output->validity_capacity>=area.pixels,"ICS raw output or validity buffer is invalid");
        checkpoint(cancel);std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));std::fill_n(output->validity,static_cast<std::size_t>(area.pixels),std::uint8_t(0));
        const auto sample_interval=4096/request->channels;
        for(std::int64_t row=0;row<area.height;++row) {
            checkpoint(cancel);
            for(std::int64_t col=0;col<area.width;++col) {
                if(static_cast<std::uint64_t>(col)%sample_interval==0)checkpoint(cancel);
                const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
                const auto to=static_cast<std::uint64_t>(row)*area.width+col;
                const auto from=((static_cast<std::uint64_t>(request->z)*source.height+y)*source.width+x)*source.channels+request->first_channel;
                std::memcpy(output->data+to*pixel_stride,source.data()+from*source.sample_bytes,static_cast<std::size_t>(pixel_stride));output->validity[to]=1;
            }
        }
        checkpoint(cancel);output->written=count;output->validity_written=area.pixels;return QPX_READER_OK;
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi,qpx_reader_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"ICS reader does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"ICS reader API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,describe,open,destroy,metadata,level,channel_name,read_rgba,read_raw};return QPX_READER_OK;
    });
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(
    std::uint32_t requested_abi,qpx_reader_files_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"ICS source-file API does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"ICS source-file API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,source_count,source_path};return QPX_READER_OK;
    });
}
