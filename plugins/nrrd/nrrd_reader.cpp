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
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"NRRD Float32 requires IEEE-754 binary32");
static_assert(sizeof(std::int16_t)==2 && std::numeric_limits<std::int16_t>::min()==-32768,"NRRD Int16 requires the declared signed range");
struct Failure {qpx_reader_status_v1 status;const char* message;};
[[noreturn]] void fail(qpx_reader_status_v1 status,const char* message) {throw Failure{status,message};}
void require(bool condition,const char* message) {if(!condition)fail(QPX_READER_INVALID_REQUEST,message);}
void valid(bool condition,const char* message) {if(!condition)fail(QPX_READER_IO_ERROR,message);}
void checkpoint(qpx_reader_cancel_v1 cancel) {
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))fail(QPX_READER_CANCELLED,"NRRD operation cancelled");
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
    catch(const std::bad_alloc&){return failure(error,QPX_READER_INTERNAL_ERROR,"NRRD plugin allocation failed");}
    catch(const std::filesystem::filesystem_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::runtime_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::exception& e){return failure(error,QPX_READER_INTERNAL_ERROR,e.what());}
    catch(...){return failure(error,QPX_READER_INTERNAL_ERROR,"NRRD plugin internal failure");}
}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {
    require(!a || b<=std::numeric_limits<std::uint64_t>::max()/a,"NRRD decoded size overflow");
    return a*b;
}
struct Budget {
    std::uint64_t remaining;
    std::unique_ptr<std::uint8_t[]> array(std::uint64_t bytes) {
        // Payload sizes are exact new[] counts; reserve a fixed bookkeeping
        // allowance per owned array in addition to the wrapper/context budget.
        constexpr std::uint64_t bookkeeping=32;
        require(bytes<=remaining && bookkeeping<=remaining-bytes,"NRRD decoded data exceeds reader work budget");
        remaining-=bytes+bookkeeping;
        return std::unique_ptr<std::uint8_t[]>(new std::uint8_t[static_cast<std::size_t>(bytes)]);
    }
};
constexpr std::size_t header_limit=65536;
bool horizontal(char value) {return value==' ' || value=='\t';}
bool digit(char value) {return value>='0' && value<='9';}
char lower(char value) {return value>='A' && value<='Z'?static_cast<char>(value+('a'-'A')):value;}
std::string_view trim(std::string_view value) {
    while(!value.empty() && horizontal(value.front()))value.remove_prefix(1);
    while(!value.empty() && horizontal(value.back()))value.remove_suffix(1);
    return value;
}
bool equal(std::string_view a,std::string_view b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(lower(a[i])!=lower(b[i]))return false;
    return true;
}
bool words_equal(std::string_view value,std::string_view expected) {
    while(!value.empty() && !expected.empty()) {
        const auto a=value.find_first_of(" \t"),b=expected.find_first_of(" \t");
        if(!equal(value.substr(0,a),expected.substr(0,b)))return false;
        value=a==std::string_view::npos?std::string_view{}:trim(value.substr(a));
        expected=b==std::string_view::npos?std::string_view{}:trim(expected.substr(b));
    }
    return value.empty() && expected.empty();
}
struct Window {double lower=0,upper=255,span=255;};
double json_number(std::string_view value,std::size_t& position) {
    const auto start=position;
    if(position<value.size() && value[position]=='-')++position;
    require(position<value.size() && digit(value[position]),"NRRD window requires JSON numbers");
    if(value[position]=='0') {++position;require(position==value.size() || !digit(value[position]),"NRRD JSON number has a leading zero");}
    else while(position<value.size() && digit(value[position]))++position;
    if(position<value.size() && value[position]=='.') {
        ++position;const auto first=position;
        while(position<value.size() && digit(value[position]))++position;
        require(position>first,"NRRD JSON fraction is incomplete");
    }
    if(position<value.size() && (value[position]=='e' || value[position]=='E')) {
        ++position;if(position<value.size() && (value[position]=='+' || value[position]=='-'))++position;
        const auto first=position;while(position<value.size() && digit(value[position]))++position;
        require(position>first,"NRRD JSON exponent is incomplete");
    }
    double result=0;
    const auto parsed=std::from_chars(value.data()+start,value.data()+position,result,std::chars_format::general);
    require(parsed.ec==std::errc{} && parsed.ptr==value.data()+position && std::isfinite(result),"NRRD window numbers must be finite and representable");
    return result;
}
Window parse_window(qpx_reader_string_v1 options) {
    require(options.size<=512 && (!options.size || options.data),"NRRD options are invalid");
    if(!options.size)return {};
    const std::string_view value(options.data,static_cast<std::size_t>(options.size));
    constexpr std::string_view prefix="{\"window\":[";
    require(value.substr(0,prefix.size())==prefix,"NRRD options require the canonical window object");
    auto position=prefix.size();Window window;
    window.lower=json_number(value,position);
    require(position<value.size() && value[position++]==',',"NRRD window requires two numbers");
    window.upper=json_number(value,position);
    require(value.substr(position)=="]}","NRRD options have unexpected or trailing fields");
    window.span=window.upper-window.lower;
    require(window.lower<window.upper && std::isfinite(window.span) && window.span>0,"NRRD window must have a finite positive span");
    return window;
}
struct HeaderCursor {
    const std::uint8_t* bytes;std::size_t size,position=0;
    qpx_reader_cancel_v1 cancel;
    std::string_view line() {
        const auto start=position;
        while(position<size && bytes[position]!='\n' && bytes[position]!='\r') {
            require(position<header_limit,"NRRD header exceeds 64 KiB limit");
            if((position&4095)==0)checkpoint(cancel);
            valid(bytes[position]=='\t' || (bytes[position]>=32 && bytes[position]<=126),"NRRD header contains invalid ASCII text");
            ++position;
        }
        valid(position<size,"NRRD header line is truncated");
        require(position<header_limit,"NRRD header exceeds 64 KiB limit");
        const auto end=position;
        if(bytes[position++]=='\r') {
            require(position<header_limit,"NRRD header exceeds 64 KiB limit");
            valid(position<size && bytes[position]=='\n',"NRRD header requires LF or CRLF lines");++position;
        }
        return {reinterpret_cast<const char*>(bytes+start),end-start};
    }
};
std::uint64_t integer(std::string_view value) {
    valid(!value.empty(),"NRRD integer is empty");std::uint64_t result=0;
    for(char character:value) {
        valid(digit(character),"NRRD requires unsigned decimal dimensions");
        const auto next=static_cast<unsigned>(character-'0');
        require(result<=std::numeric_limits<std::uint64_t>::max()/10 &&
            (result<std::numeric_limits<std::uint64_t>::max()/10 || next<=std::numeric_limits<std::uint64_t>::max()%10),"NRRD size integer overflows");
        result=result*10+next;
    }
    valid(result>0,"NRRD dimensions must be positive");return result;
}
std::array<std::string_view,3> tokens(std::string_view value,unsigned dimension) {
    std::array<std::string_view,3> result{};
    for(unsigned axis=0;axis<dimension;++axis) {
        value=trim(value);valid(!value.empty(),"NRRD per-axis field has too few values");
        const auto end=value.find_first_of(" \t");result[axis]=value.substr(0,end);
        value=end==std::string_view::npos?std::string_view{}:value.substr(end);
    }
    valid(trim(value).empty(),"NRRD per-axis field has too many values");return result;
}
std::array<std::string_view,3> units(std::string_view value,unsigned dimension) {
    std::array<std::string_view,3> result{};
    for(unsigned axis=0;axis<dimension;++axis) {
        value=trim(value);valid(!value.empty() && value.front()=='"',"NRRD units must use quoted per-axis strings");
        value.remove_prefix(1);const auto end=value.find('"');
        valid(end!=std::string_view::npos,"NRRD unit string is unterminated");
        result[axis]=value.substr(0,end);value.remove_prefix(end+1);
        valid(value.empty() || horizontal(value.front()),"NRRD unit strings require separators");
    }
    valid(trim(value).empty(),"NRRD units have too many values");return result;
}
double spacing(std::string_view value) {
    if(equal(value,"nan") || equal(value,"+nan") || equal(value,"-nan"))return std::numeric_limits<double>::quiet_NaN();
    if(!value.empty() && value.front()=='+')value.remove_prefix(1);
    valid(!value.empty(),"NRRD spacing is empty");
    double result=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result,std::chars_format::general);
    valid(parsed.ec==std::errc{} && parsed.ptr==value.data()+value.size(),"NRRD spacing is not a representable decimal number");
    valid(!std::isinf(result) && result!=0,"NRRD spacing must not be zero or infinite");
    if(result<0)fail(QPX_READER_UNSUPPORTED,"NRRD negative axis direction cannot be represented");
    return result;
}
struct Raster {
    std::int64_t width=0,height=0;
    std::uint32_t dimension=0,z_planes=1,sample_type=0,sample_bytes=0,calibration_flags=0;
    static constexpr std::uint32_t channels=1;
    std::array<double,3> spacing_um{};
    Window window;std::size_t payload=0;
    std::unique_ptr<std::uint8_t[]> bytes;
};
struct Handle {Raster image;};
Raster decode(std::unique_ptr<std::uint8_t[]> bytes,std::size_t size,Window window,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    if(size<8 || std::memcmp(bytes.get(),"NRRD0005",8))fail(QPX_READER_UNSUPPORTED,"NRRD reader requires NRRD0005");
    HeaderCursor cursor{bytes.get(),size,0,cancel};
    if(cursor.line()!="NRRD0005")fail(QPX_READER_UNSUPPORTED,"NRRD magic line is not supported");
    enum Field {Type,Dimension,Sizes,Encoding,Endian,Kinds,Spacings,Units,Content,Count};
    std::array<std::string_view,Count> fields{};unsigned seen=0;Raster result;result.window=window;
    for(;;) {
        const auto line=cursor.line();if(line.empty())break;
        if(line.front()=='#')continue;
        valid(!horizontal(line.front()),"NRRD field identifiers must not have leading whitespace");
        const auto colon=line.find(':');valid(colon!=std::string_view::npos && colon>0,"NRRD field delimiter is missing");
        if(colon+1<line.size() && line[colon+1]=='=')fail(QPX_READER_UNSUPPORTED,"NRRD custom key/value interpretation is unsupported");
        valid(colon+1<line.size() && line[colon+1]==' ' && !horizontal(line[colon-1]),"NRRD fields require a colon-space delimiter");
        const auto name=line.substr(0,colon),value=trim(line.substr(colon+2));
        Field field=Count;
        const char* names[]={"type","dimension","sizes","encoding","endian","kinds","spacings","units","content"};
        for(unsigned i=0;i<Count;++i)if(equal(name,names[i])){field=static_cast<Field>(i);break;}
        if(field==Count)fail(QPX_READER_UNSUPPORTED,"NRRD header interpretation is unsupported");
        valid(!(seen&(1u<<field)),"NRRD field is duplicated");
        if(field==Sizes || field==Kinds || field==Spacings || field==Units)
            valid((seen&(1u<<Dimension))!=0,"NRRD dimension must precede per-axis fields");
        seen|=1u<<field;fields[field]=value;
        if(field==Dimension) {
            const auto dimension=integer(value);
            if(dimension!=2 && dimension!=3)fail(QPX_READER_UNSUPPORTED,"NRRD reader supports only scalar XY and XYZ arrays");
            result.dimension=static_cast<std::uint32_t>(dimension);
        }
    }
    constexpr unsigned mandatory=(1u<<Type)|(1u<<Dimension)|(1u<<Sizes)|(1u<<Encoding);
    valid((seen&mandatory)==mandatory,"NRRD required fields are missing");
    valid(!fields[Type].empty() && !fields[Encoding].empty(),"NRRD type or encoding is empty");
    if(!equal(fields[Encoding],"raw"))fail(QPX_READER_UNSUPPORTED,"NRRD reader accepts only inline raw encoding");
    const auto type=fields[Type];
    if(words_equal(type,"uchar") || words_equal(type,"unsigned char") || words_equal(type,"uint8") || words_equal(type,"uint8_t")) {
        result.sample_type=QPX_READER_UINT8;result.sample_bytes=1;
    } else if(words_equal(type,"ushort") || words_equal(type,"unsigned short") || words_equal(type,"unsigned short int") || words_equal(type,"uint16") || words_equal(type,"uint16_t")) {
        result.sample_type=QPX_READER_UINT16;result.sample_bytes=2;
    } else if(words_equal(type,"short") || words_equal(type,"short int") || words_equal(type,"signed short") || words_equal(type,"signed short int") || words_equal(type,"int16") || words_equal(type,"int16_t")) {
        result.sample_type=QPX_READER_INT16;result.sample_bytes=2;
    } else if(equal(type,"float")) {result.sample_type=QPX_READER_FLOAT32;result.sample_bytes=4;}
    else fail(QPX_READER_UNSUPPORTED,"NRRD sample type is unsupported");
    if(!(seen&(1u<<Kinds)))fail(QPX_READER_UNSUPPORTED,"NRRD reader requires explicit domain or space axis kinds");
    const auto kind=tokens(fields[Kinds],result.dimension);
    for(unsigned axis=0;axis<result.dimension;++axis)
        if(!equal(kind[axis],"domain") && !equal(kind[axis],"space"))fail(QPX_READER_UNSUPPORTED,"NRRD axis kind is not a scalar spatial domain");
    const auto sizes=tokens(fields[Sizes],result.dimension);
    std::array<std::uint64_t,3> shape{1,1,1};
    for(unsigned axis=0;axis<result.dimension;++axis)shape[axis]=integer(sizes[axis]);
    require(shape[0]<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) &&
        shape[1]<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) && shape[2]<=UINT32_MAX,"NRRD dimensions exceed native coordinate ranges");
    result.width=static_cast<std::int64_t>(shape[0]);result.height=static_cast<std::int64_t>(shape[1]);result.z_planes=static_cast<std::uint32_t>(shape[2]);
    const auto samples=multiply(multiply(shape[0],shape[1]),shape[2]);
    const auto expected=multiply(samples,result.sample_bytes);
    require(expected<=work_limit,"NRRD decoded size exceeds reader work budget");
    valid(expected==size-cursor.position,"NRRD raw payload is truncated or has trailing data");
    bool big=false;
    if(result.sample_bytes>1)valid((seen&(1u<<Endian))!=0,"NRRD multibyte raw samples require endian");
    if(seen&(1u<<Endian)) {
        valid(equal(fields[Endian],"little") || equal(fields[Endian],"big"),"NRRD endian must be little or big");big=equal(fields[Endian],"big");
    }
    std::array<double,3> gaps{};std::array<bool,3> known{};
    if(seen&(1u<<Spacings)) {
        const auto values=tokens(fields[Spacings],result.dimension);
        for(unsigned axis=0;axis<result.dimension;++axis) {gaps[axis]=spacing(values[axis]);known[axis]=!std::isnan(gaps[axis]);}
    }
    if(seen&(1u<<Units)) {
        const auto names=units(fields[Units],result.dimension);
        for(unsigned axis=0;axis<result.dimension;++axis) {
            double factor=0;
            if(names[axis].empty())continue;
            if(names[axis]=="nm")factor=0.001;
            else if(names[axis]=="um")factor=1;
            else if(names[axis]=="mm")factor=1000;
            else if(names[axis]=="cm")factor=10000;
            else if(names[axis]=="m")factor=1000000;
            else fail(QPX_READER_UNSUPPORTED,"NRRD axis units are unsupported");
            if(known[axis]) {
                result.spacing_um[axis]=gaps[axis]*factor;
                if(!std::isfinite(result.spacing_um[axis]) || result.spacing_um[axis]<=0)
                    fail(QPX_READER_UNSUPPORTED,"NRRD calibrated spacing exceeds representable bounds");
                result.calibration_flags|=1u<<axis;
            }
        }
    }
    if(big && result.sample_bytes>1)for(std::uint64_t sample=0;sample<samples;++sample) {
        if((sample&4095)==0)checkpoint(cancel);
        auto* value=bytes.get()+cursor.position+sample*result.sample_bytes;
        std::reverse(value,value+result.sample_bytes);
    }
    checkpoint(cancel);result.payload=cursor.position;result.bytes=std::move(bytes);return result;
}
bool display_sample(const Raster& source,std::uint64_t index,double& result) {
    const auto* bytes=source.bytes.get()+source.payload+index*source.sample_bytes;
    if(source.sample_type==QPX_READER_UINT8)result=*bytes;
    else if(source.sample_type==QPX_READER_UINT16) {std::uint16_t value=0;std::memcpy(&value,bytes,2);result=value;}
    else if(source.sample_type==QPX_READER_INT16) {std::int16_t value=0;std::memcpy(&value,bytes,2);result=value;}
    else {
        std::uint32_t bits=0;std::memcpy(&bits,bytes,4);
        // Reject nonfinite display values before evaluating even a local sNaN.
        if((bits&UINT32_C(0x7f800000))==UINT32_C(0x7f800000))return false;
        float value=0;std::memcpy(&value,&bits,4);result=value;
    }
    return true;
}
const Raster& image(void* value) {require(value!=nullptr,"NRRD reader handle is null");return static_cast<Handle*>(value)->image;}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"NRRD image path is invalid");
    std::string path(value.data,static_cast<std::size_t>(value.size));
    require(path.find('\0')==std::string::npos,"NRRD image path contains NUL");
    require(std::filesystem::u8path(path).is_absolute(),"NRRD image path must be absolute");
    return path;
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output && output->struct_size>=sizeof(*output),"NRRD reader info structure is too small");
        output->abi_version=QPX_READER_ABI_V1;output->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW;
        bool complete=text(output->provider_id,"org.qupathxx.nrrd");
        complete=text(output->display_name,"NRRD") && complete;
        complete=text(output->version,"1.0.0") && complete;
        complete=text(output->contract,"nrrd5-raw-scalar-window/1") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"NRRD descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"NRRD reader output handle is null");*output=nullptr;
        require(request && request->struct_size>=sizeof(*request),"NRRD open structure is too small");
        require(request->reserved==0,"NRRD open reserved field must be zero");
        const auto window=parse_window(request->options);
        require(request->work_bytes && request->work_bytes<=work_limit,"NRRD work budget must be between 1 and 256 MiB");
        require(request->path.data && request->path.size && request->path.size<=32768,"NRRD image path is invalid");
        // Reserve the path/wrapper allowance before constructing owned paths.
        const auto overhead=UINT64_C(4096)+8*request->path.size+request->options.size+sizeof(Handle);
        require(request->work_bytes>overhead,"NRRD reader work budget is too small for opening overhead");
        checkpoint(request->cancel);
        const auto path=path_string(request->path);const auto native=std::filesystem::u8path(path);
        Budget budget{request->work_bytes-overhead};
        const auto encoded=std::filesystem::file_size(native);
        require(encoded<=budget.remaining && 32<=budget.remaining-encoded,"NRRD input exceeds reader work budget");
        // Bounded UTF-8/UTF-16 path copies, owner and unbuffered FILE state;
        // One source array plus bookkeeping is charged below. Raw payload
        // normalization is in-place; display reads own no intermediate array.
        auto bytes=budget.array(encoded);
        std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(native.c_str(),L"rb"),std::fclose);
        if(!file)fail(QPX_READER_IO_ERROR,"NRRD image cannot be opened");
        if(std::setvbuf(file.get(),nullptr,_IONBF,0))fail(QPX_READER_IO_ERROR,"NRRD unbuffered input could not be configured");
        for(std::uint64_t position=0;position<encoded;) {
            checkpoint(request->cancel);
            const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(65536,encoded-position));
            valid(std::fread(bytes.get()+position,1,count,file.get())==count,"NRRD input is truncated or changed");position+=count;
        }
        valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"NRRD input changed during reading");
        auto result=std::make_unique<Handle>();
        result->image=decode(std::move(bytes),static_cast<std::size_t>(encoded),window,request->cancel);
        *output=result.release();return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"NRRD metadata structure is too small");checkpoint(cancel);
        output->level_count=1;output->width=source.width;output->height=source.height;
        output->has_raw=1;output->channels=source.channels;output->sample_type=source.sample_type;
        output->z_planes=source.z_planes;output->timepoints=1;output->series=0;output->series_count=1;
        output->calibration_flags=source.calibration_flags;output->pixel_width_um=source.spacing_um[0];output->pixel_height_um=source.spacing_um[1];output->z_spacing_um=source.spacing_um[2];output->time_interval_seconds=0;
        bool complete=text(output->vendor,"NRRD");complete=text(output->dimension_order,source.dimension==2?"XY":"XYZ") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"NRRD metadata text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"NRRD level structure is too small");require(index==0,"NRRD reader has only level zero");
        output->reserved=0;output->width=source.width;output->height=source.height;output->downsample=1;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value,std::uint32_t channel,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output!=nullptr,"NRRD channel-name output is null");require(channel<source.channels,"NRRD channel index is out of range");
        const char* name="Scalar";
        return text(*output,name)?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"NRRD channel-name buffer is too small");
    });
}
struct Region {std::int64_t width,height;std::uint64_t pixels;};
Region region(const qpx_reader_region_v1* request,std::uint64_t bytes_per_pixel) {
    require(request && request->struct_size>=sizeof(*request),"NRRD region structure is too small");
    require(request->level==0 && !request->t,"NRRD reader has only level zero and T=0");
    const auto width=request->width,height=request->height;
    require(width>0 && height>0 && static_cast<std::uint64_t>(width)<=work_limit/bytes_per_pixel &&
        static_cast<std::uint64_t>(height)<=work_limit/(bytes_per_pixel*static_cast<std::uint64_t>(width)),"NRRD output must have positive dimensions and fit 256 MiB");
    const auto limit=std::numeric_limits<std::int64_t>::max();
    require(request->x<=limit-width && request->y<=limit-height,"NRRD region coordinate overflow");
    return {width,height,static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)};
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"NRRD pixel structure is too small");output->written=0;output->validity_written=0;
        const auto area=region(request,4);require(!request->first_channel && !request->channels && !request->z,"NRRD display requires zero channel and Z selectors");
        const auto row_stride=static_cast<std::uint64_t>(area.width)*4,count=area.pixels*4;
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==4 && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==4 && output->channel_stride==1,"NRRD RGBA output must use packed UInt8x4 layout");
        require(output->data && output->capacity>=count && !output->validity && !output->validity_capacity,"NRRD RGBA output buffer is invalid");
        std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row)for(std::int64_t col=0;col<area.width;++col) {
            const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            const auto index=static_cast<std::uint64_t>(y)*source.width+x;
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
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"NRRD raw pixel structure is too small");output->written=0;output->validity_written=0;
        require(request && request->struct_size>=sizeof(*request),"NRRD raw region structure is too small");
        require(request->channels>0 && request->first_channel<source.channels && request->channels<=source.channels-request->first_channel && request->z<source.z_planes,"NRRD raw channel or Z range is invalid");
        const auto pixel_stride=static_cast<std::uint64_t>(request->channels)*source.sample_bytes;
        const auto area=region(request,pixel_stride);const auto count=area.pixels*pixel_stride,row_stride=static_cast<std::uint64_t>(area.width)*pixel_stride;
        require(area.pixels<=work_limit-count,"NRRD raw output and validity exceed 256 MiB");
        const auto sample_type=source.sample_type;
        require(output->sample_type==sample_type && output->width==area.width && output->height==area.height && output->channels==request->channels && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==pixel_stride && output->channel_stride==source.sample_bytes,"NRRD raw output must use packed native-endian HWC layout");
        require(output->data && output->capacity>=count && output->validity && output->validity_capacity>=area.pixels,"NRRD raw output or validity buffer is invalid");
        checkpoint(cancel);std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));std::fill_n(output->validity,static_cast<std::size_t>(area.pixels),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row) {
            checkpoint(cancel);
            for(std::int64_t col=0;col<area.width;++col) {
                if((col&4095)==0)checkpoint(cancel);
                const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
                const auto to=static_cast<std::uint64_t>(row)*area.width+col;
                const auto from=(static_cast<std::uint64_t>(request->z)*source.height+y)*source.width+x;
                std::memcpy(output->data+to*pixel_stride,source.bytes.get()+source.payload+from*source.sample_bytes,static_cast<std::size_t>(pixel_stride));output->validity[to]=1;
            }
        }
        checkpoint(cancel);output->written=count;output->validity_written=area.pixels;return QPX_READER_OK;
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi,qpx_reader_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"NRRD reader does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"NRRD reader API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,describe,open,destroy,metadata,level,channel_name,read_rgba,read_raw};return QPX_READER_OK;
    });
}
