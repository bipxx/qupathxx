#include "reader_plugin_profile.hpp"
#include "../image-common/gzip_member.hpp"
#include <qupathxx/reader_plugin.h>
#include <windows.h>
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
enum Axis : unsigned {axis_x,axis_y,axis_z,axis_time,axis_channel,axis_count};
constexpr std::array<std::string_view,axis_count> axis_names{"x","y","z","t","ch"};
constexpr std::array<std::string_view,axis_count> reference_labels{"x-position","y-position","z-position","time","ch"};
constexpr std::array<char,axis_count> axis_letters{'X','Y','Z','T','C'};
struct Raster {
    unsigned version=0;
    std::int64_t width=0,height=0;
    std::uint32_t z_planes=1,timepoints=1,channels=1,sample_type=0,sample_bytes=0,calibration_flags=0;
    bool compressed=false;
    std::array<std::uint8_t,4> byte_order{};
    std::array<std::uint64_t,axis_count> strides{};
    std::array<char,axis_count+1> dimension_order{};
    std::array<double,3> spacing_um{};
    double interval_seconds=0;
    Window window;std::size_t payload=0;std::uint64_t data_bytes=0;
    std::unique_ptr<std::uint8_t[]> primary,ids;
    const std::uint8_t* data() const {return ids?ids.get():primary.get()+payload;}
    std::uint8_t* data() {return ids?ids.get():primary.get()+payload;}
};
struct Handle {Raster image;};
struct Header {
    Raster raster;
    std::string_view external;
    std::uint64_t offset=0;
    bool has_companion() const {return raster.version==1 || !external.empty();}
};
Header parse_header(const ReadFile& file,Window window,qpx_reader_cancel_v1 cancel) {
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
    Header header;
    auto& result=header.raster;
    result.window=window;result.version=version.values[1]=="1.0"?1:2;
    enum Field {Parameters,Order,Sizes,Coordinates,Significant,Format,Sign,ByteOrder,Compression,Origin,Scale,Units,Labels,SourceFile,SourceOffset,Count};
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
        if(category=="sensor" || (category=="source" && result.version==1))
            fail(QPX_READER_UNSUPPORTED,"ICS source or sensor interpretation is unsupported");
        if(category=="end")fail(QPX_READER_IO_ERROR,"ICS end marker contains extra values");
        if(category!="layout" && category!="representation" && category!="parameter" && category!="source")
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
        } else if(category=="parameter") {
            if(subcategory=="origin")field=Origin;
            else if(subcategory=="scale")field=Scale;
            else if(subcategory=="units")field=Units;
            else if(subcategory=="labels")field=Labels;
        } else {
            if(subcategory=="file")field=SourceFile;
            else if(subcategory=="offset")field=SourceOffset;
        }
        if(field==Count)fail(QPX_READER_UNSUPPORTED,"ICS header subcategory is unsupported");
        valid(!(seen&(1u<<field)),"ICS header field is duplicated");seen|=1u<<field;fields[field]=value;
    }
    if(file.total_size>file.size && !ended)fail(QPX_READER_INVALID_REQUEST,"ICS header exceeds 64 KiB limit");
    constexpr unsigned external_fields=(1u<<SourceFile)|(1u<<SourceOffset);
    if(seen&external_fields) {
        valid((seen&external_fields)==external_fields && !fields[SourceFile].empty(),
            "ICS external source requires both a file and an offset");
        valid(fields[SourceFile].find('\t')==std::string_view::npos,"ICS external source file must occupy one TAB field");
        header.external=fields[SourceFile];
        header.offset=integer(fields[SourceOffset],false);
    }
    valid(header.has_companion() || ended,"ICS version2 end marker is missing");
    constexpr unsigned mandatory=(1u<<Labels)-1;
    valid((seen&mandatory)==mandatory,"ICS required header fields are missing");
    const auto parameter_count=integer(fields[Parameters]);
    if(parameter_count<3 || parameter_count>6)fail(QPX_READER_UNSUPPORTED,"ICS axis layout is unsupported");
    const auto order=split(fields[Order]),sizes=split(fields[Sizes]);
    valid(order.count==parameter_count && sizes.count==parameter_count,"ICS layout count is inconsistent");
    if(order.values[0]!="bits")fail(QPX_READER_UNSUPPORTED,"ICS requires bits as the first layout parameter");
    std::array<std::uint64_t,axis_count> shape{1,1,1,1,1};
    std::array<unsigned,axis_count+1> axes{};
    std::uint64_t samples=1;
    for(unsigned slot=1;slot<order.count;++slot) {
        const auto match=std::find(axis_names.begin(),axis_names.end(),order.values[slot]);
        if(match==axis_names.end())fail(QPX_READER_UNSUPPORTED,"ICS axis name is unsupported");
        const auto axis=static_cast<unsigned>(match-axis_names.begin());
        valid(!result.strides[axis],"ICS axis name is duplicated");
        axes[slot]=axis;
        shape[axis]=integer(sizes.values[slot]);
        result.strides[axis]=samples;
        result.dimension_order[slot-1]=axis_letters[axis];
        samples=multiply(samples,shape[axis]);
    }
    if(!result.strides[axis_x] || !result.strides[axis_y])
        fail(QPX_READER_UNSUPPORTED,"ICS requires explicit X and Y axes");
    const auto bits=integer(sizes.values[0]);
    valid(!fields[Format].empty() && !fields[Sign].empty(),"ICS numeric representation is missing");
    if(fields[Sign]!="signed" && fields[Sign]!="unsigned")fail(QPX_READER_UNSUPPORTED,"ICS numeric sign interpretation is unsupported");
    if(fields[Format]=="integer" && bits==8 && fields[Sign]=="unsigned") {result.sample_type=QPX_READER_UINT8;result.sample_bytes=1;}
    else if(fields[Format]=="integer" && bits==16) {result.sample_type=fields[Sign]=="signed"?QPX_READER_INT16:QPX_READER_UINT16;result.sample_bytes=2;}
    else if(fields[Format]=="real" && bits==32) {result.sample_type=QPX_READER_FLOAT32;result.sample_bytes=4;}
    else fail(QPX_READER_UNSUPPORTED,"ICS numeric depth or type is unsupported");
    if(integer(fields[Significant],false)!=bits)fail(QPX_READER_UNSUPPORTED,"ICS requires full-width significant bits");
    result.compressed=fields[Compression]=="gzip";
    if(!result.compressed && fields[Compression]!="uncompressed")
        fail(QPX_READER_UNSUPPORTED,"ICS compression is unsupported");
    if(fields[Coordinates]!="video")fail(QPX_READER_UNSUPPORTED,"ICS coordinates must explicitly be video");
    const auto channels=shape[axis_channel],z_planes=shape[axis_z],timepoints=shape[axis_time];
    require(channels<=channel_limit && z_planes<=UINT32_MAX && timepoints<=UINT32_MAX &&
        shape[axis_x]<=static_cast<std::uint64_t>(INT64_MAX) && shape[axis_y]<=static_cast<std::uint64_t>(INT64_MAX),"ICS dimensions exceed the declared native profile");
    result.channels=static_cast<std::uint32_t>(channels);result.z_planes=static_cast<std::uint32_t>(z_planes);
    result.timepoints=static_cast<std::uint32_t>(timepoints);
    result.width=static_cast<std::int64_t>(shape[axis_x]);result.height=static_cast<std::int64_t>(shape[axis_y]);
    require(window.channel<result.channels,"ICS display channel is outside the source");
    result.data_bytes=multiply(samples,result.sample_bytes);
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
        for(unsigned slot=0;slot<order.count;++slot) {
            const auto label=labels.values[slot];
            if(label==(slot?order.values[slot]:"intensity") ||
                (slot && label==reference_labels[axes[slot]])) continue;
            fail(QPX_READER_UNSUPPORTED,"ICS custom axis labels are unsupported");
        }
    }
    for(unsigned slot=0;slot<order.count;++slot) {
        const auto axis=axes[slot];
        if(numeric(origin.values[slot])!=0)fail(QPX_READER_UNSUPPORTED,"ICS nonzero origin cannot be represented");
        const auto gap=numeric(scale.values[slot]);
        const auto unit=units.values[slot];
        if(!slot || axis==axis_channel) {
            if(gap!=1 || (!slot?unit!="relative":!unit.empty() && unit!="undefined"))
                fail(QPX_READER_UNSUPPORTED,"ICS scalar or channel scaling/units cannot be represented");
            continue;
        }
        valid(gap!=0,"ICS axis scale must not be zero");
        if(gap<0)fail(QPX_READER_UNSUPPORTED,"ICS negative axis direction cannot be represented");
        double factor=0;
        if(unit.empty() || unit=="undefined")continue;
        if(axis==axis_time) {
            if(unit=="s") factor=1; else if(unit=="ms") factor=1000; else if(unit=="us") factor=1000000;
            else fail(QPX_READER_UNSUPPORTED,"ICS time units are unsupported");
            result.interval_seconds=gap/factor;
            if(!(result.interval_seconds>0))
                fail(QPX_READER_UNSUPPORTED,"ICS calibrated time step exceeds representable bounds");
            result.calibration_flags|=QPX_READER_CALIBRATION_TIME;
            continue;
        }
        if(unit=="nm")factor=0.001;else if(unit=="um")factor=1;else if(unit=="mm")factor=1000;
        else if(unit=="cm")factor=10000;else if(unit=="m")factor=1000000;
        else fail(QPX_READER_UNSUPPORTED,"ICS spatial units are unsupported");
        result.spacing_um[axis]=gap*factor;
        if(!std::isfinite(result.spacing_um[axis]) || result.spacing_um[axis]<=0)
            fail(QPX_READER_UNSUPPORTED,"ICS calibrated spacing exceeds representable bounds");
        result.calibration_flags|=1u<<axis;
    }
    result.payload=cursor.position;
    if(header.has_companion())valid(cursor.position==file.total_size,"ICS separate header has trailing data");
    else if(!result.compressed)valid(result.data_bytes==file.total_size-cursor.position,"ICS inline data is truncated or has trailing bytes");
    checkpoint(cancel);return header;
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
std::filesystem::path companion_path(const std::filesystem::path& primary,const Header& header) {
    if(!header.external.empty()) {
        const auto name=header.external;
        const auto letter=[](char c){return (c>='A' && c<='Z') || (c>='a' && c<='z');};
        if(name.size()>255 || name.front()=='.' || name.back()=='.' ||
            !std::all_of(name.begin(),name.end(),[&](char c){return letter(c) || digit(c) || c=='_' || c=='-' || c=='.';}))
            fail(QPX_READER_UNSUPPORTED,"ICS external source must be one plain relative basename");
        std::string stem(name.substr(0,name.find('.')));
        for(auto& c:stem)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
        if(stem=="con" || stem=="prn" || stem=="aux" || stem=="nul" ||
            (stem.size()==4 && (stem.compare(0,3,"com")==0 || stem.compare(0,3,"lpt")==0) && digit(stem.back())))
            fail(QPX_READER_UNSUPPORTED,"ICS external source cannot name a Windows device");
        const auto basename=std::filesystem::u8path(name.begin(),name.end());
        if(CompareStringOrdinal(primary.filename().c_str(),-1,basename.c_str(),-1,TRUE)==CSTR_EQUAL)
            fail(QPX_READER_UNSUPPORTED,"ICS external source cannot name its header");
        return primary.parent_path()/basename;
    }
    auto value=primary.u8string();
    // prepare() has already checked the .ics extension. Change only its c/C.
    value[value.size()-2]=value[value.size()-2]=='C'?'D':'d';
    auto derived=std::filesystem::u8path(value).lexically_normal();
    // The alternate spelling applies only to declared gzip and a missing canonical name.
    if(header.raster.compressed && GetFileAttributesW(derived.c_str())==INVALID_FILE_ATTRIBUTES
        && GetLastError()==ERROR_FILE_NOT_FOUND) {
        auto gzip=derived;
        gzip+=L".gz";
        if(GetFileAttributesW(gzip.c_str())!=INVALID_FILE_ATTRIBUTES || GetLastError()!=ERROR_FILE_NOT_FOUND)
            return gzip;
    }
    return derived;
}
class ExternalFile {
public:
    explicit ExternalFile(const std::filesystem::path& path) {
        handle_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(handle_==INVALID_HANDLE_VALUE)fail(QPX_READER_IO_ERROR,"ICS external source cannot be opened");
        BY_HANDLE_FILE_INFORMATION info{};
        if(GetFileType(handle_)!=FILE_TYPE_DISK || !GetFileInformationByHandle(handle_,&info) ||
            (info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) || info.nNumberOfLinks!=1) {
            CloseHandle(handle_);
            fail(QPX_READER_UNSUPPORTED,"ICS external source must be a regular file without aliases");
        }
        size_=(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    }
    ExternalFile(const ExternalFile&)=delete;
    ExternalFile& operator=(const ExternalFile&)=delete;
    ~ExternalFile(){CloseHandle(handle_);}

    ReadFile raster(const Header& header,Budget& budget,qpx_reader_cancel_v1 cancel) const {
        checkpoint(cancel);
        valid(header.offset<=size_,"ICS external source offset exceeds file size");
        const auto available=size_-header.offset;
        const auto count=header.raster.compressed?available:header.raster.data_bytes;
        valid(count<=available,"ICS external source raster is truncated");
        // Only the selected range is copied. The host observes the complete file.
        auto bytes=budget.array(count);
        require(header.offset<=static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max()),
            "ICS external source offset exceeds native seek range");
        LARGE_INTEGER offset;
        offset.QuadPart=static_cast<LONGLONG>(header.offset);
        valid(SetFilePointerEx(handle_,offset,nullptr,FILE_BEGIN)!=0,"ICS external source seek failed");
        for(std::uint64_t position=0;position<count;) {
            checkpoint(cancel);
            const auto chunk=static_cast<DWORD>(std::min<std::uint64_t>(65536,count-position));
            DWORD received=0;
            valid(::ReadFile(handle_,bytes.get()+static_cast<std::size_t>(position),chunk,&received,nullptr) && received==chunk,
                "ICS external source was truncated during reading");
            position+=chunk;
        }
        return {std::move(bytes),static_cast<std::size_t>(count),size_};
    }
private:
    HANDLE handle_=INVALID_HANDLE_VALUE;
    std::uint64_t size_=0;
};
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
        *output=parsed.has_companion()?1:0;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request,std::uint32_t index,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"ICS source path output is null");
        auto prepared=prepare(request);auto primary=read_file(prepared.native,prepared.budget,request->cancel,header_limit);
        const auto parsed=parse_header(primary,prepared.window,request->cancel);
        require(parsed.has_companion() && index==0,"ICS companion index is out of range");
        const auto native=companion_path(prepared.native,parsed);
        if(!parsed.external.empty()) {
            checkpoint(request->cancel);
            const ExternalFile observed(native);
        }
        const auto path=native.u8string();
        require(path.size()<=32768,"ICS companion path exceeds reader path limit");
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
        complete=text(output->version,"1.5.0") && complete;
        complete=text(output->contract,"ics-raw-gzip-channel-window/3") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"ICS descriptor text buffer is too small");
    });
}
void inflate_raster(Raster& image,std::size_t encoded,Budget& budget,qpx_reader_cancel_v1 cancel) {
    using namespace qpx::image_plugin;
    require(budget.remaining>=gzip_workspace_bytes,"ICS gzip workspace exceeds reader work budget");
    budget.remaining-=gzip_workspace_bytes;
    auto output=budget.array(image.data_bytes);
    const auto status=decode_gzip_member(image.data(),encoded,output.get(),static_cast<std::size_t>(image.data_bytes),
        [&]{checkpoint(cancel);});
    if(status==GzipResult::workspace_failure)fail(QPX_READER_INTERNAL_ERROR,"ICS gzip decoder workspace allocation failed");
    valid(status==GzipResult::ok,"ICS gzip member is malformed or differs from the declared raster size");
    image.ids=std::move(output);
    image.primary.reset();
    image.payload=0;
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"ICS reader output handle is null");*output=nullptr;
        auto prepared=prepare(request);
        auto primary=read_file(prepared.native,prepared.budget,request->cancel);
        auto header=parse_header(primary,prepared.window,request->cancel);
        auto& image=header.raster;
        auto encoded=primary.size-image.payload;
        image.primary=std::move(primary.bytes);
        if(header.has_companion()) {
            const auto path=companion_path(prepared.native,header);
            auto data=header.external.empty()
                ?read_file(path,prepared.budget,request->cancel,SIZE_MAX,image.compressed?UINT64_MAX:image.data_bytes)
                :ExternalFile(path).raster(header,prepared.budget,request->cancel);
            encoded=data.size;
            image.ids=std::move(data.bytes);
        }
        if(image.compressed)inflate_raster(image,encoded,prepared.budget,request->cancel);
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
        output->z_planes=source.z_planes;output->timepoints=source.timepoints;output->series=0;output->series_count=1;
        output->calibration_flags=source.calibration_flags;output->pixel_width_um=source.spacing_um[0];output->pixel_height_um=source.spacing_um[1];output->z_spacing_um=source.spacing_um[2];output->time_interval_seconds=source.interval_seconds;
        bool complete=text(output->vendor,"ICS");complete=text(output->dimension_order,source.dimension_order.data()) && complete;
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
    require(request->level==0,"ICS reader has only level zero");
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
        const auto area=region(request,4);require(!request->first_channel && !request->channels && !request->z && !request->t,"ICS display requires zero channel, Z and T selectors");
        const auto row_stride=static_cast<std::uint64_t>(area.width)*4,count=area.pixels*4;
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==4 && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==4 && output->channel_stride==1,"ICS RGBA output must use packed UInt8x4 layout");
        require(output->data && output->capacity>=count && !output->validity && !output->validity_capacity,"ICS RGBA output buffer is invalid");
        std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row)for(std::int64_t col=0;col<area.width;++col) {
            const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            const auto index=static_cast<std::uint64_t>(y)*source.strides[axis_y]+
                static_cast<std::uint64_t>(x)*source.strides[axis_x]+source.window.channel*source.strides[axis_channel];
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
        require(request->channels>0 && request->first_channel<source.channels && request->channels<=source.channels-request->first_channel && request->z<source.z_planes && request->t<source.timepoints,"ICS raw channel, Z or T range is invalid");
        const auto pixel_stride=static_cast<std::uint64_t>(request->channels)*source.sample_bytes;
        const auto area=region(request,pixel_stride);const auto count=area.pixels*pixel_stride,row_stride=static_cast<std::uint64_t>(area.width)*pixel_stride;
        require(area.pixels<=work_limit-count,"ICS raw output and validity exceed 256 MiB");
        const auto sample_type=source.sample_type;
        require(output->sample_type==sample_type && output->width==area.width && output->height==area.height && output->channels==request->channels && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==pixel_stride && output->channel_stride==source.sample_bytes,"ICS raw output must use packed native-endian HWC layout");
        require(output->data && output->capacity>=count && output->validity && output->validity_capacity>=area.pixels,"ICS raw output or validity buffer is invalid");
        checkpoint(cancel);std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));std::fill_n(output->validity,static_cast<std::size_t>(area.pixels),std::uint8_t(0));
        const auto sample_interval=4096/request->channels;
        const auto base=request->t*source.strides[axis_time]+request->z*source.strides[axis_z]+
            request->first_channel*source.strides[axis_channel];
        const auto channel_stride=source.strides[axis_channel]*source.sample_bytes;
        for(std::int64_t row=0;row<area.height;++row) {
            checkpoint(cancel);
            for(std::int64_t col=0;col<area.width;++col) {
                if(static_cast<std::uint64_t>(col)%sample_interval==0)checkpoint(cancel);
                const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
                const auto to=static_cast<std::uint64_t>(row)*area.width+col;
                const auto from=base+static_cast<std::uint64_t>(y)*source.strides[axis_y]+
                    static_cast<std::uint64_t>(x)*source.strides[axis_x];
                auto* dst=output->data+to*pixel_stride;
                const auto* src=source.data()+from*source.sample_bytes;
                if(request->channels==1 || source.strides[axis_channel]==1) {
                    std::memcpy(dst,src,static_cast<std::size_t>(pixel_stride));
                } else {
                    for(unsigned channel=0;channel<request->channels;++channel)
                        std::memcpy(dst+channel*source.sample_bytes,src+channel*channel_stride,source.sample_bytes);
                }
                output->validity[to]=1;
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
