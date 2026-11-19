#include "reader_plugin_profile.hpp"
#include <qupathxx/reader_plugin.h>
#include <windows.h>
#include <zlib.h>
#include <bzlib.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace {
constexpr std::uint64_t work_limit=UINT64_C(256)*1024*1024;
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"NRRD Float32 requires IEEE-754 binary32");
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,"NRRD Float64 requires IEEE-754 binary64");
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
constexpr std::size_t line_scan_block=4096;
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
        if(position==size)return {reinterpret_cast<const char*>(bytes+start),position-start};
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
constexpr unsigned axis_limit=4;
std::array<std::string_view,axis_limit> tokens(std::string_view value,unsigned dimension) {
    std::array<std::string_view,axis_limit> result{};
    for(unsigned axis=0;axis<dimension;++axis) {
        value=trim(value);valid(!value.empty(),"NRRD per-axis field has too few values");
        const auto end=value.find_first_of(" \t");result[axis]=value.substr(0,end);
        value=end==std::string_view::npos?std::string_view{}:value.substr(end);
    }
    valid(trim(value).empty(),"NRRD per-axis field has too many values");return result;
}
std::array<std::string_view,axis_limit> units(std::string_view value,unsigned dimension) {
    std::array<std::string_view,axis_limit> result{};
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
std::array<double,3> space_vector(std::string_view& value,unsigned axes) {
    value=trim(value);
    const auto end=value.find(')');
    valid(!value.empty() && value.front()=='(' && end!=std::string_view::npos,"NRRD space vector requires parentheses");
    auto components=value.substr(1,end-1);
    value.remove_prefix(end+1);
    valid(value.empty() || horizontal(value.front()),"NRRD space vectors require a separator");
    std::array<double,3> result{};
    for(unsigned axis=0;axis<axes;++axis) {
        const auto comma=components.find(',');
        valid((comma==std::string_view::npos)==(axis+1==axes),"NRRD space vector has the wrong number of components");
        auto token=trim(components.substr(0,comma));
        if(!token.empty() && token.front()=='+')token.remove_prefix(1);
        valid(!token.empty(),"NRRD space vector component is empty");
        const auto parsed=std::from_chars(token.data(),token.data()+token.size(),result[axis],std::chars_format::general);
        valid(parsed.ec==std::errc{} && parsed.ptr==token.data()+token.size() &&
            std::isfinite(result[axis]),"NRRD space vector requires finite decimal components");
        if(comma!=std::string_view::npos)components.remove_prefix(comma+1);
    }
    return result;
}
double spatial_unit(std::string_view unit) {
    if(unit=="nm")return 0.001;
    if(unit=="um")return 1;
    if(unit=="mm")return 1000;
    if(unit=="cm")return 10000;
    if(unit=="m")return 1000000;
    fail(QPX_READER_UNSUPPORTED,"NRRD axis units are unsupported");
}
struct Raster {
    std::int64_t width=0,height=0;
    std::uint32_t dimension=0,spatial_axes=0,z_planes=1,timepoints=1,sample_type=0,sample_bytes=0,calibration_flags=0;
    static constexpr std::uint32_t channels=1;
    std::array<double,3> spacing_um{};
    double time_interval_seconds=0;
    Window window;std::size_t payload=0;
    std::unique_ptr<std::uint8_t[]> bytes;
};
void select_scalar_type(std::string_view type,Raster& raster) {
    struct Type {
        std::uint32_t id,bytes;
        std::array<std::string_view,7> names;
    };
    constexpr Type supported[]={
        {QPX_READER_UINT8,1,{"uchar","unsigned char","uint8","uint8_t"}},
        {QPX_READER_INT8,1,{"signed char","int8","int8_t"}},
        {QPX_READER_UINT16,2,{"ushort","unsigned short","unsigned short int","uint16","uint16_t"}},
        {QPX_READER_INT16,2,{"short","short int","signed short","signed short int","int16","int16_t"}},
        {QPX_READER_UINT32,4,{"uint","unsigned int","uint32","uint32_t"}},
        {QPX_READER_INT32,4,{"int","signed int","int32","int32_t"}},
        {QPX_READER_UINT64,8,{"ulonglong","unsigned long long","unsigned long long int","uint64","uint64_t"}},
        {QPX_READER_INT64,8,{"longlong","long long","long long int","signed long long","signed long long int","int64","int64_t"}},
        {QPX_READER_FLOAT32,4,{"float"}},
        {QPX_READER_FLOAT64,8,{"double"}},
    };
    for(const auto& entry:supported)for(const auto alias:entry.names) {
        if(alias.empty())break;
        if(words_equal(type,alias)) {
            raster.sample_type=entry.id;
            raster.sample_bytes=entry.bytes;
            return;
        }
    }
    fail(QPX_READER_UNSUPPORTED,"NRRD sample type is unsupported");
}
template<class F> decltype(auto) with_scalar_type(std::uint32_t id,F&& f) {
    switch(id) {
    case QPX_READER_UINT8:return f(std::uint8_t{});
    case QPX_READER_INT8:return f(std::int8_t{});
    case QPX_READER_UINT16:return f(std::uint16_t{});
    case QPX_READER_INT16:return f(std::int16_t{});
    case QPX_READER_UINT32:return f(std::uint32_t{});
    case QPX_READER_INT32:return f(std::int32_t{});
    case QPX_READER_UINT64:return f(std::uint64_t{});
    case QPX_READER_INT64:return f(std::int64_t{});
    case QPX_READER_FLOAT32:return f(float{});
    case QPX_READER_FLOAT64:return f(double{});
    default:fail(QPX_READER_INTERNAL_ERROR,"NRRD raster has an invalid scalar type");
    }
}
template<class T> struct FloatBits {
    using Word=std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>;
    static constexpr Word exponent=static_cast<Word>(sizeof(T)==4?0x7f800000ULL:0x7ff0000000000000ULL);
    static constexpr Word nan=static_cast<Word>(sizeof(T)==4?0x7fc00000ULL:0x7ff8000000000000ULL);
    static constexpr Word sign=Word(1)<<(8*sizeof(T)-1);
};
struct Handle {Raster image;};
struct FileBytes {
    std::unique_ptr<std::uint8_t[]> bytes;
    std::size_t size;
    std::uint64_t total;
};
FileBytes read_primary(const std::filesystem::path& path,Budget& budget,
    qpx_reader_cancel_v1 cancel,std::uint64_t limit=UINT64_MAX) {
    const auto total=std::filesystem::file_size(path);
    const auto size=std::min<std::uint64_t>(total,limit);
    require(size<=budget.remaining && 32<=budget.remaining-size,"NRRD input exceeds reader work budget");
    FileBytes result{budget.array(size),static_cast<std::size_t>(size),total};
    std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(path.c_str(),L"rb"),std::fclose);
    if(!file)fail(QPX_READER_IO_ERROR,"NRRD image cannot be opened");
    if(std::setvbuf(file.get(),nullptr,_IONBF,0))fail(QPX_READER_IO_ERROR,"NRRD unbuffered input could not be configured");
    for(std::size_t position=0;position<result.size;) {
        checkpoint(cancel);
        const auto count=std::min<std::size_t>(65536,result.size-position);
        valid(std::fread(result.bytes.get()+position,1,count,file.get())==count,"NRRD input is truncated or changed");
        position+=count;
    }
    if(size==total)valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"NRRD input changed during reading");
    return result;
}

void validate_payload_name(std::string_view name) {
    valid(!name.empty(),"NRRD detached payload name is empty");
    const auto letter=[](char c) {return (c>='a' && c<='z') || (c>='A' && c<='Z');};
    if(name.size()>255 || name.front()=='.' || name.back()=='.' || trim(name)!=name ||
        !std::all_of(name.begin(),name.end(),[&](char c){return letter(c) || digit(c) || c=='_' || c=='-' || c=='.' || c==' ';}))
        fail(QPX_READER_UNSUPPORTED,"NRRD detached payload must be one plain relative basename");
    const auto stem=trim(name.substr(0,name.find('.')));
    if(equal(stem,"con") || equal(stem,"prn") || equal(stem,"aux") || equal(stem,"nul") ||
        (stem.size()==4 && (equal(stem.substr(0,3),"com") || equal(stem.substr(0,3),"lpt")) && digit(stem[3])))
        fail(QPX_READER_UNSUPPORTED,"NRRD detached payload cannot name a Windows device");
}
std::string_view local_payload_name(std::string_view name) {
    if(name.substr(0,2)=="./") name.remove_prefix(2);
    validate_payload_name(name);
    return name;
}
std::filesystem::path payload_path(const std::filesystem::path& primary,std::string_view name) {
    const auto basename=std::filesystem::u8path(name.begin(),name.end());
    if(CompareStringOrdinal(primary.filename().c_str(),-1,basename.c_str(),-1,TRUE)==CSTR_EQUAL)
        fail(QPX_READER_UNSUPPORTED,"NRRD detached payload cannot name its header");
    return primary.parent_path()/basename;
}

std::size_t scan_line_block(const std::uint8_t* bytes,std::size_t size,std::uint32_t& lines) {
    std::size_t position=0;
    while(position<size && lines)if(bytes[position++]=='\n')--lines;
    return position;
}

std::size_t skip_inline_lines(const FileBytes& file,std::size_t start,std::uint32_t lines,qpx_reader_cancel_v1 cancel) {
    while(lines && start<file.size) {
        checkpoint(cancel);
        const auto size=std::min(line_scan_block,file.size-start);
        start+=scan_line_block(file.bytes.get()+start,size,lines);
    }
    valid(!lines,"NRRD line skip exceeds the complete payload lines");
    return start;
}

class PayloadFile {
public:
    explicit PayloadFile(const std::filesystem::path& path) {
        handle_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(handle_==INVALID_HANDLE_VALUE)fail(QPX_READER_IO_ERROR,"NRRD detached payload cannot be opened");
        BY_HANDLE_FILE_INFORMATION info{};
        if(GetFileType(handle_)!=FILE_TYPE_DISK || !GetFileInformationByHandle(handle_,&info) ||
            (info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) || info.nNumberOfLinks!=1) {
            CloseHandle(handle_);
            fail(QPX_READER_UNSUPPORTED,"NRRD detached payload must be a regular file without aliases");
        }
        size_=(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    }
    PayloadFile(const PayloadFile&)=delete;
    PayloadFile& operator=(const PayloadFile&)=delete;
    ~PayloadFile(){CloseHandle(handle_);}

    std::uint64_t size() const { return size_; }

    std::uint64_t skip_lines(std::uint32_t lines,std::uint64_t& scan_remaining,qpx_reader_cancel_v1 cancel) const {
        if(!lines)return 0;
        // Fixed scratch is covered by the opening overhead. Bound total scanning separately.
        std::array<std::uint8_t,line_scan_block> block;
        seek(0);
        const auto end=std::min(size_,scan_remaining);
        std::uint64_t offset=0;
        while(lines && offset<end) {
            checkpoint(cancel);
            const auto count=static_cast<DWORD>(std::min<std::uint64_t>(block.size(),end-offset));
            read_exact(block.data(),count);
            offset+=scan_line_block(block.data(),count,lines);
        }
        if(lines && end<size_)fail(QPX_READER_INVALID_REQUEST,"NRRD line preamble exceeds the scan limit");
        valid(!lines,"NRRD line skip exceeds the complete payload lines");
        scan_remaining-=offset;
        return offset;
    }

    FileBytes read_range(std::uint64_t offset,std::uint64_t length,Budget& budget,qpx_reader_cancel_v1 cancel) const {
        valid(offset<=size_ && length<=size_-offset,"NRRD detached payload range is truncated");
        require(length<=budget.remaining && 32<=budget.remaining-length,"NRRD detached payload exceeds reader work budget");
        checkpoint(cancel);
        seek(offset);
        FileBytes result{budget.array(length),static_cast<std::size_t>(length),size_};
        for(std::size_t position=0;position<result.size;) {
            checkpoint(cancel);
            const auto count=static_cast<DWORD>(std::min<std::size_t>(65536,result.size-position));
            read_exact(result.bytes.get()+position,count);
            position+=count;
        }
        return result;
    }
private:
    void seek(std::uint64_t offset) const {
        valid(offset<=INT64_MAX,"NRRD detached payload offset exceeds the file coordinate range");
        LARGE_INTEGER start{};
        start.QuadPart=static_cast<LONGLONG>(offset);
        valid(SetFilePointerEx(handle_,start,nullptr,FILE_BEGIN),"NRRD detached payload seek failed");
    }
    void read_exact(std::uint8_t* bytes,DWORD count) const {
        DWORD received=0;
        valid(ReadFile(handle_,bytes,count,&received,nullptr) && received==count,
              "NRRD detached payload is truncated or changed");
    }
    HANDLE handle_=INVALID_HANDLE_VALUE;
    std::uint64_t size_=0;
};

std::unique_ptr<std::uint8_t[]> inflate_gzip(std::uint8_t* input,std::size_t size,
    std::size_t expected,Budget& budget,qpx_reader_cancel_v1 cancel) {
    constexpr std::size_t step=65536;
    constexpr std::size_t codec_workspace=65536;
    require(budget.remaining>=codec_workspace,"NRRD gzip workspace exceeds reader work budget");
    budget.remaining-=codec_workspace;
    auto output=budget.array(expected);
    std::size_t codec_remaining=codec_workspace;
    z_stream decoder{};
    decoder.opaque=&codec_remaining;
    decoder.zalloc=[](voidpf opaque,uInt items,uInt width) -> voidpf {
        auto& remaining=*static_cast<std::size_t*>(opaque);
        if(width && items>remaining/width)return nullptr;
        const auto bytes=static_cast<std::size_t>(items)*width;
        const auto memory=std::calloc(items,width);
        if(memory)remaining-=bytes;
        return memory;
    };
    decoder.zfree=[](voidpf,voidpf memory) {std::free(memory);};
    if(inflateInit2(&decoder,15+16)!=Z_OK)fail(QPX_READER_INTERNAL_ERROR,"Cannot initialize NRRD gzip decoder");
    struct End {z_stream& decoder;~End(){inflateEnd(&decoder);}} end{decoder};
    std::size_t supplied=0;
    std::uint8_t overflow=0;
    for(;;) {
        checkpoint(cancel);
        if(!decoder.avail_in && supplied<size) {
            const auto count=std::min(step,size-supplied);
            decoder.next_in=input+supplied;decoder.avail_in=static_cast<uInt>(count);supplied+=count;
        }
        if(!decoder.avail_out) {
            const auto written=static_cast<std::size_t>(decoder.total_out);
            decoder.next_out=written==expected?&overflow:output.get()+written;
            decoder.avail_out=static_cast<uInt>(written==expected?1:std::min(step,expected-written));
        }
        const auto status=inflate(&decoder,Z_NO_FLUSH);
        valid(decoder.total_out<=expected,"NRRD gzip decoded data exceeds declared size");
        if(status==Z_STREAM_END)break;
        if(status==Z_MEM_ERROR)fail(QPX_READER_INTERNAL_ERROR,"NRRD gzip decoder workspace allocation failed");
        valid(status==Z_OK,"NRRD gzip payload is corrupt or truncated");
    }
    valid(decoder.total_in==size && decoder.total_out==expected,"NRRD gzip payload size differs or has trailing data");
    return output;
}
std::unique_ptr<std::uint8_t[]> expand_bzip2(std::uint8_t* input,std::size_t size,
    std::size_t expected,Budget& budget,qpx_reader_cancel_v1 cancel) {
    constexpr std::size_t step=65536;
    constexpr std::size_t workspace=3*1024*1024;
    require(budget.remaining>=workspace,"NRRD bzip2 workspace exceeds reader work budget");
    budget.remaining-=workspace;
    auto output=budget.array(expected);
    std::size_t remaining=workspace;
    bz_stream decoder{};
    decoder.opaque=&remaining;
    decoder.bzalloc=[](void* opaque,int count,int width) -> void* {
        auto& available=*static_cast<std::size_t*>(opaque);
        if(count<=0 || width<=0 || static_cast<std::size_t>(count)>available/static_cast<std::size_t>(width))
            return nullptr;
        const auto bytes=static_cast<std::size_t>(count)*static_cast<std::size_t>(width);
        auto* memory=std::malloc(bytes);
        if(memory)available-=bytes;
        return memory;
    };
    decoder.bzfree=[](void*,void* memory) {std::free(memory);};
    // Use libbzip2's small decoder to bound its block workspace.
    if(BZ2_bzDecompressInit(&decoder,0,1)!=BZ_OK)
        fail(QPX_READER_INTERNAL_ERROR,"Cannot initialize NRRD bzip2 decoder");
    struct End {bz_stream& decoder;~End(){BZ2_bzDecompressEnd(&decoder);}} end{decoder};
    std::size_t supplied=0;
    char overflow=0;
    for(;;) {
        checkpoint(cancel);
        if(!decoder.avail_in && supplied<size) {
            const auto count=std::min(step,size-supplied);
            decoder.next_in=reinterpret_cast<char*>(input+supplied);
            decoder.avail_in=static_cast<unsigned>(count);
            supplied+=count;
        }
        if(!decoder.avail_out) {
            const auto written=static_cast<std::size_t>(decoder.total_out_lo32);
            decoder.next_out=written==expected?&overflow:reinterpret_cast<char*>(output.get()+written);
            decoder.avail_out=static_cast<unsigned>(written==expected?1:std::min(step,expected-written));
        }
        const auto before_in=decoder.total_in_lo32,before_out=decoder.total_out_lo32;
        const auto status=BZ2_bzDecompress(&decoder);
        valid(!decoder.total_out_hi32 && decoder.total_out_lo32<=expected,"NRRD bzip2 decoded data exceeds declared size");
        if(status==BZ_STREAM_END)break;
        if(status==BZ_MEM_ERROR)fail(QPX_READER_INTERNAL_ERROR,"NRRD bzip2 decoder workspace allocation failed");
        valid(status==BZ_OK && (decoder.total_in_lo32!=before_in || decoder.total_out_lo32!=before_out),
              "NRRD bzip2 payload is corrupt or truncated");
    }
    valid(!decoder.total_in_hi32 && decoder.total_in_lo32==size && decoder.total_out_lo32==expected,
          "NRRD bzip2 payload size differs or has trailing data");
    return output;
}
struct FilenamePattern {
    std::string_view prefix,suffix;
    std::int64_t first,step;
    std::size_t width;

    std::string name(std::size_t index) const {
        const auto number=first+static_cast<std::int64_t>(index)*step;
        const auto magnitude=std::to_string(number<0?-number:number);
        const auto used=magnitude.size()+static_cast<std::size_t>(number<0);
        std::string result(prefix);
        if(number<0)result+='-';
        if(width>used)result.append(width-used,'0');
        result+=magnitude;
        result+=suffix;
        return result;
    }
};
struct Header {
    Raster raster;
    std::array<std::string_view,63> companions{};
    std::optional<FilenamePattern> pattern;
    std::size_t companion_count=0;
    std::uint64_t part_samples=0,part_bytes=0,total_bytes=0;
    enum class Encoding { raw, gzip, bzip2, ascii, hex };
    Encoding encoding = Encoding::raw;
    std::optional<std::int32_t> byte_skip;
    std::uint32_t line_skip=0;
    bool big = false;
    std::string companion_name(std::size_t index) const {
        return pattern?pattern->name(index):std::string(companions[index]);
    }
};
std::int32_t bounded_int32(std::string_view token,std::int32_t minimum,const char* diagnostic) {
    valid(!token.empty(),diagnostic);
    if(token.front()=='+') {
        token.remove_prefix(1);
        valid(!token.empty() && digit(token.front()),diagnostic);
    }
    std::int32_t count=0;
    const auto* end=token.data()+token.size();
    const auto parsed=std::from_chars(token.data(),end,count);
    valid(parsed.ec==std::errc{} && parsed.ptr==end && count>=minimum,diagnostic);
    return count;
}
std::uint32_t parse_companion_pattern(Header& header,std::string_view value,qpx_reader_cancel_v1 cancel) {
    std::array<std::string_view,5> fields{};
    std::size_t field_count=0;
    while(!value.empty()) {
        valid(field_count<fields.size(),"NRRD numbered data file has too many fields");
        const auto end=value.find_first_of(" \t");
        fields[field_count++]=value.substr(0,end);
        value=end==std::string_view::npos?std::string_view{}:trim(value.substr(end));
    }
    valid(field_count>=4,"NRRD numbered data file requires first, last and step");
    const auto format=fields[0];
    const auto marker=format.find('%');
    valid(marker!=std::string_view::npos,"NRRD numbered data file requires a conversion");
    const auto conversion=format.find_first_not_of("0123456789",marker+1);
    if(conversion==std::string_view::npos || (format[conversion]!='d' && format[conversion]!='i') ||
       format.find('%',conversion+1)!=std::string_view::npos || format.size()>257)
        fail(QPX_READER_UNSUPPORTED,"NRRD numbered data file requires one decimal filename conversion");
    const auto padding=format.substr(marker+1,conversion-marker-1);
    std::size_t width=0;
    if(!padding.empty()) {
        if(padding.front()!='0' || padding.size()<2)
            fail(QPX_READER_UNSUPPORTED,"NRRD numbered filename width must use zero padding");
        width=static_cast<std::size_t>(bounded_int32(padding,1,"NRRD numbered filename width is invalid"));
        require(width<=255,"NRRD numbered filename width exceeds 255 characters");
    }
    const auto coordinate=[](std::string_view token){
        return std::int64_t(bounded_int32(token,INT32_MIN,"NRRD numbered file range requires signed 32-bit integers"));
    };
    const auto first=coordinate(fields[1]),last=coordinate(fields[2]),step=coordinate(fields[3]);
    valid((step>0 && first<=last) || (step<0 && first>=last),"NRRD numbered file range has a zero or reversed step");
    const auto count=step>0?(last-first)/step+1:(first-last)/-step+1;
    require(count<=static_cast<std::int64_t>(header.companions.size()),"NRRD numbered file range exceeds 63 companions");
    header.pattern=FilenamePattern{format.substr(0,marker),format.substr(conversion+1),first,step,width};
    header.companion_count=static_cast<std::size_t>(count);
    for(std::size_t i=0;i<header.companion_count;++i) {
        checkpoint(cancel);
        validate_payload_name(header.companion_name(i));
    }
    return field_count==5?static_cast<std::uint32_t>(bounded_int32(fields[4],1,"NRRD file subdimension must be positive")):
        header.raster.dimension-1;
}
void parse_companions(Header& header,std::string_view value,HeaderCursor& cursor,std::uint64_t file_size,
                      const std::array<std::uint64_t,axis_limit>& shape,std::uint64_t samples,unsigned version) {
    const auto separator=value.find_first_of(" \t");
    const auto form=value.substr(0,separator);
    const bool listed=equal(form,"list");
    const bool numbered=separator!=std::string_view::npos && form.find('%')!=std::string_view::npos;
    if(version==3 && (listed || numbered || value.substr(0,2)!="./"))
        fail(QPX_READER_UNSUPPORTED,"NRRD0003 detached input requires one header-relative ./basename");
    if(!listed && !numbered) {
        valid(cursor.position==file_size,"NRRD detached header has unexpected trailing data");
        header.companions[header.companion_count++]=local_payload_name(value);
        return;
    }
    const auto rank=header.raster.dimension;
    std::uint32_t subdim=rank-1;
    if(numbered) {
        valid(cursor.position==file_size,"NRRD detached header has unexpected trailing data");
        subdim=parse_companion_pattern(header,value,cursor.cancel);
    } else {
        require(file_size<=header_limit,"NRRD LIST exceeds the 64 KiB header limit");
        const auto tail=separator==std::string_view::npos?std::string_view{}:trim(value.substr(separator));
        if(!tail.empty())subdim=static_cast<std::uint32_t>(bounded_int32(tail,1,"NRRD file subdimension must be positive"));
        while(cursor.position<cursor.size) {
            checkpoint(cursor.cancel);
            const auto name=local_payload_name(cursor.line());
            require(header.companion_count<header.companions.size(),"NRRD LIST exceeds 63 companions");
            for(std::size_t i=0;i<header.companion_count;++i)
                valid(!equal(name,header.companions[i]),"NRRD LIST contains a duplicate companion");
            header.companions[header.companion_count++]=name;
        }
        valid(header.companion_count!=0,"NRRD LIST is empty");
    }
    valid(subdim<=rank,"NRRD file subdimension exceeds the data rank");
    if(subdim==rank) {
        valid(shape[rank-1]%header.companion_count==0,"NRRD file slabs must divide the slow axis evenly");
    } else {
        std::uint64_t count=1;
        for(auto axis=subdim;axis<rank;++axis)count=multiply(count,shape[axis]);
        valid(count==header.companion_count,"NRRD file count differs from the declared axes");
    }
    header.part_samples=samples/header.companion_count;
}
Header parse_header(const FileBytes& file,Window window,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    if(file.size<8 || std::memcmp(file.bytes.get(),"NRRD000",7) ||
        file.bytes[7]<'3' || file.bytes[7]>'5')
        fail(QPX_READER_UNSUPPORTED,"NRRD reader requires NRRD0003, NRRD0004 or NRRD0005");
    HeaderCursor cursor{file.bytes.get(),file.size,0,cancel};
    const auto magic=cursor.line();
    if(magic.size()!=8)fail(QPX_READER_UNSUPPORTED,"NRRD magic line is not supported");
    const auto version=static_cast<unsigned>(magic.back()-'0');
    enum Field {Type,Dimension,Sizes,Encoding,Endian,Kinds,Spacings,Units,Content,DataFile,ByteSkip,LineSkip,
        SpaceDimension,SpaceDirections,SpaceUnits,SpaceOrigin,Count};
    std::array<std::string_view,Count> fields{};
    unsigned seen=0;
    Header header;
    auto& result=header.raster;
    result.window=window;
    bool separated=false;
    while(cursor.position<file.size) {
        const auto line=cursor.line();
        if(line.empty()){separated=true;break;}
        if(line.front()=='#')continue;
        valid(!horizontal(line.front()),"NRRD field identifiers must not have leading whitespace");
        const auto colon=line.find(':');valid(colon!=std::string_view::npos && colon>0,"NRRD field delimiter is missing");
        if(colon+1<line.size() && line[colon+1]=='=')fail(QPX_READER_UNSUPPORTED,"NRRD custom key/value interpretation is unsupported");
        valid(colon+1<line.size() && line[colon+1]==' ' && !horizontal(line[colon-1]),"NRRD fields require a colon-space delimiter");
        const auto name=line.substr(0,colon),value=trim(line.substr(colon+2));
        Field field=Count;
        const char* names[]={"type","dimension","sizes","encoding","endian","kinds","spacings","units","content","data file","byte skip","line skip",
            "space dimension","space directions","space units","space origin"};
        for(unsigned i=0;i<Count;++i)if(equal(name,names[i])){field=static_cast<Field>(i);break;}
        if(equal(name,"datafile"))field=DataFile;
        if(equal(name,"byteskip"))field=ByteSkip;
        if(equal(name,"lineskip"))field=LineSkip;
        if(field==Count)fail(QPX_READER_UNSUPPORTED,"NRRD header interpretation is unsupported");
        valid(!(seen&(1u<<field)),"NRRD field is duplicated");
        if(field==Sizes || field==Kinds || field==Spacings || field==Units || field==SpaceDirections)
            valid((seen&(1u<<Dimension))!=0,"NRRD dimension must precede per-axis fields");
        if(version==3 && (field==SpaceDimension || field==SpaceDirections || field==SpaceUnits || field==SpaceOrigin))
            fail(QPX_READER_UNSUPPORTED,"NRRD space fields require NRRD0004 or later");
        if(field==SpaceDirections || field==SpaceUnits || field==SpaceOrigin)
            if(!(seen&(1u<<SpaceDimension)))fail(QPX_READER_UNSUPPORTED,"NRRD reader requires space dimension before space vectors and units");
        seen|=1u<<field;fields[field]=value;
        if(field==DataFile && equal(value.substr(0,value.find_first_of(" \t")),"list"))break;
        if(field==Dimension) {
            const auto dimension=integer(value);
            if(dimension<2 || dimension>axis_limit)fail(QPX_READER_UNSUPPORTED,"NRRD reader supports scalar XY, XYZ, XYT and XYZT arrays");
            result.dimension=static_cast<std::uint32_t>(dimension);
        }
    }
    if(!(seen&(1u<<DataFile))) {
        valid(separated,"NRRD attached header has no blank separator");
    }
    constexpr unsigned mandatory=(1u<<Type)|(1u<<Dimension)|(1u<<Sizes)|(1u<<Encoding);
    valid((seen&mandatory)==mandatory,"NRRD required fields are missing");
    valid(!fields[Type].empty() && !fields[Encoding].empty(),"NRRD type or encoding is empty");
    if(equal(fields[Encoding],"gzip") || equal(fields[Encoding],"gz"))
        header.encoding=Header::Encoding::gzip;
    else if(equal(fields[Encoding],"bzip2") || equal(fields[Encoding],"bz2"))
        header.encoding=Header::Encoding::bzip2;
    else if(equal(fields[Encoding],"ascii") || equal(fields[Encoding],"text") || equal(fields[Encoding],"txt"))
        header.encoding=Header::Encoding::ascii;
    else if(equal(fields[Encoding],"hex"))
        header.encoding=Header::Encoding::hex;
    else if(!equal(fields[Encoding],"raw"))
        fail(QPX_READER_UNSUPPORTED,"NRRD reader accepts only raw, gzip, bzip2, ASCII and hexadecimal encodings");
    if(seen&(1u<<ByteSkip)) {
        const auto skip=bounded_int32(fields[ByteSkip],-1,"NRRD byte skip must be -1 or a nonnegative 32-bit integer");
        if(skip==-1 && header.encoding!=Header::Encoding::raw)
            fail(QPX_READER_UNSUPPORTED,"NRRD EOF-relative byte skip requires raw encoding");
        header.byte_skip=skip;
    }
    if(seen&(1u<<LineSkip))
        header.line_skip=static_cast<std::uint32_t>(bounded_int32(fields[LineSkip],0,"NRRD line skip must be a nonnegative 32-bit integer"));
    select_scalar_type(fields[Type],result);
    if(!(seen&(1u<<Kinds)))fail(QPX_READER_UNSUPPORTED,"NRRD reader requires explicit spatial and temporal axis kinds");
    const auto kind=tokens(fields[Kinds],result.dimension);
    const bool has_time=equal(kind[result.dimension-1],"time");
    result.spatial_axes=result.dimension-unsigned(has_time);
    if(result.spatial_axes<2 || result.spatial_axes>3)
        fail(QPX_READER_UNSUPPORTED,"NRRD reader requires two or three spatial axes before an optional time axis");
    for(unsigned axis=0;axis<result.spatial_axes;++axis)
        if(!equal(kind[axis],"domain") && !equal(kind[axis],"space"))fail(QPX_READER_UNSUPPORTED,"NRRD axis kind is not a scalar spatial domain");
    const auto sizes=tokens(fields[Sizes],result.dimension);
    std::array<std::uint64_t,axis_limit> shape{1,1,1,1};
    for(unsigned axis=0;axis<result.dimension;++axis)shape[axis]=integer(sizes[axis]);
    const auto depth=result.spatial_axes==3?shape[2]:1;
    const auto frames=has_time?shape[result.dimension-1]:1;
    require(shape[0]<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) &&
        shape[1]<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) &&
        depth<=UINT32_MAX && frames<=UINT32_MAX,"NRRD dimensions exceed native coordinate ranges");
    result.width=static_cast<std::int64_t>(shape[0]);result.height=static_cast<std::int64_t>(shape[1]);
    result.z_planes=static_cast<std::uint32_t>(depth);result.timepoints=static_cast<std::uint32_t>(frames);
    const auto samples=multiply(multiply(multiply(shape[0],shape[1]),depth),frames);
    header.total_bytes=multiply(samples,result.sample_bytes);
    require(header.total_bytes<=work_limit,"NRRD decoded size exceeds reader work budget");
    header.part_samples=samples;
    if(seen&(1u<<DataFile))parse_companions(header,fields[DataFile],cursor,file.total,shape,samples,version);
    header.part_bytes=multiply(header.part_samples,result.sample_bytes);
    const bool binary=header.encoding!=Header::Encoding::ascii;
    if(binary && result.sample_bytes>1)valid((seen&(1u<<Endian))!=0,"NRRD multibyte raw samples require endian");
    if(seen&(1u<<Endian)) {
        valid(equal(fields[Endian],"little") || equal(fields[Endian],"big"),"NRRD endian must be little or big");
        header.big=binary && equal(fields[Endian],"big");
    }
    std::array<double,axis_limit> gaps{};std::array<bool,axis_limit> known{};
    if(seen&(1u<<Spacings)) {
        const auto values=tokens(fields[Spacings],result.dimension);
        for(unsigned axis=0;axis<result.dimension;++axis) {gaps[axis]=spacing(values[axis]);known[axis]=!std::isnan(gaps[axis]);}
    }
    std::array<std::string_view,axis_limit> names{};
    if(seen&(1u<<Units))names=units(fields[Units],result.dimension);
    auto spatial_names=names;
    if(seen&(1u<<SpaceDimension)) {
        const auto axes=result.spatial_axes;
        if(integer(fields[SpaceDimension])!=axes || !(seen&(1u<<SpaceDirections)) || !(seen&(1u<<SpaceUnits)))
            fail(QPX_READER_UNSUPPORTED,"NRRD spatial basis requires matching space dimension, directions and units");
        spatial_names=units(fields[SpaceUnits],axes);
        auto directions=fields[SpaceDirections];
        for(unsigned axis=0;axis<axes;++axis) {
            if(known[axis] || !names[axis].empty())
                fail(QPX_READER_UNSUPPORTED,"NRRD spatial directions conflict with per-axis spacing or units");
            const auto direction=space_vector(directions,axes);
            for(unsigned component=0;component<axes;++component)
                if(component!=axis && direction[component]!=0)
                    fail(QPX_READER_UNSUPPORTED,"NRRD space directions must align with the local axes");
            if(direction[axis]<=0 || spatial_names[axis].empty())
                fail(QPX_READER_UNSUPPORTED,"NRRD space directions require positive spacing and explicit units");
            gaps[axis]=direction[axis];
            known[axis]=true;
        }
        directions=trim(directions);
        if(has_time?!equal(directions,"none"):!directions.empty())
            fail(QPX_READER_UNSUPPORTED,"NRRD space directions require one spatial vector per spatial axis and none for time");
        if(seen&(1u<<SpaceOrigin)) {
            auto origin=fields[SpaceOrigin];
            const auto vector=space_vector(origin,axes);
            valid(trim(origin).empty(),"NRRD space origin has trailing data");
            if(std::any_of(vector.begin(),vector.end(),[](double v){return v!=0;}))
                fail(QPX_READER_UNSUPPORTED,"NRRD nonzero space origin cannot be represented");
        }
    }
    for(unsigned axis=0;axis<result.spatial_axes;++axis) {
        if(spatial_names[axis].empty())continue;
        const auto factor=spatial_unit(spatial_names[axis]);
        if(known[axis]) {
            result.spacing_um[axis]=gaps[axis]*factor;
            if(!std::isfinite(result.spacing_um[axis]) || result.spacing_um[axis]<=0)
                fail(QPX_READER_UNSUPPORTED,"NRRD calibrated spacing exceeds representable bounds");
            result.calibration_flags|=1u<<axis;
        }
    }
    if(seen&(1u<<Units)) {
        if(has_time) {
            const auto axis=result.dimension-1;
            const auto unit=names[axis];
            double factor=0;
            if(unit=="s")factor=1;
            else if(unit=="ms")factor=0.001;
            else if(unit=="us")factor=0.000001;
            else if(unit=="ns")factor=0.000000001;
            else if(!unit.empty())fail(QPX_READER_UNSUPPORTED,"NRRD temporal units are unsupported");
            if(factor && known[axis]) {
                result.time_interval_seconds=gaps[axis]*factor;
                if(!std::isfinite(result.time_interval_seconds) || result.time_interval_seconds<=0)
                    fail(QPX_READER_UNSUPPORTED,"NRRD calibrated time interval exceeds representable bounds");
                result.calibration_flags|=QPX_READER_CALIBRATION_TIME;
            }
        }
    }
    result.payload=cursor.position;
    return header;
}
bool data_space(char c) {
    return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\v' || c=='\f';
}

void ascii_sample(std::string_view token, std::uint32_t type, std::uint8_t* out) {
    if(token.front()=='+') {
        token.remove_prefix(1);
        valid(!token.empty() && token.front()!='-' && token.front()!='+',
              "NRRD ASCII sample has an invalid sign");
    }
    const auto* end=token.data()+token.size();
    with_scalar_type(type,[&](auto value) {
        using T=decltype(value);
        if constexpr(std::is_floating_point_v<T>) {
            using Bits=FloatBits<T>;
            const bool negative=token.front()=='-';
            const auto magnitude=negative?token.substr(1):token;
            if(equal(magnitude,"nan") || equal(magnitude,"inf") || equal(magnitude,"infinity")) {
                const typename Bits::Word bits=equal(magnitude,"nan")?Bits::nan:
                    Bits::exponent|(negative?Bits::sign:0);
                std::memcpy(out,&bits,sizeof(bits));
                return;
            }
            const auto parsed=std::from_chars(token.data(),end,value,std::chars_format::general);
            valid(parsed.ec==std::errc{} && parsed.ptr==end && std::isfinite(value),
                  "NRRD ASCII floating sample is invalid or out of range");
        } else {
            // Preserve the numeric zero spelling accepted for unsigned input.
            const bool negative_unsigned=std::is_unsigned_v<T> && token.front()=='-';
            const auto* begin=token.data()+(negative_unsigned?1:0);
            const auto parsed=std::from_chars(begin,end,value);
            valid(parsed.ec==std::errc{} && parsed.ptr==end && (!negative_unsigned || value==0),
                  "NRRD ASCII integer is invalid or out of range");
        }
        std::memcpy(out,&value,sizeof(value));
    });
}

std::unique_ptr<std::uint8_t[]> decode_ascii(std::string_view input, const Header& header,
                                            Budget& budget, qpx_reader_cancel_v1 cancel) {
    valid(header.part_samples<=input.size(),"NRRD ASCII payload cannot contain the declared sample count");
    auto result=budget.array(header.part_bytes);
    std::size_t cursor=0;
    const auto skip_space=[&] {
        while(cursor<input.size() && data_space(input[cursor])) {
            if((cursor&4095)==0)checkpoint(cancel);
            ++cursor;
        }
    };
    for(std::uint64_t index=0;index<header.part_samples;++index) {
        if((index&1023)==0)checkpoint(cancel);
        skip_space();
        const auto start=cursor;
        while(cursor<input.size() && !data_space(input[cursor])) {
            valid(cursor-start<128,"NRRD ASCII sample token exceeds 128 bytes");
            ++cursor;
        }
        valid(cursor!=start,"NRRD ASCII payload has too few samples");
        ascii_sample(input.substr(start,cursor-start),header.raster.sample_type,
                     result.get()+index*header.raster.sample_bytes);
    }
    skip_space();
    valid(cursor==input.size(),"NRRD ASCII payload has trailing samples or bytes");
    return result;
}
void decode_hex(std::uint8_t* bytes,std::size_t size,std::uint64_t expected,qpx_reader_cancel_v1 cancel) {
    valid(expected<=size/2,"NRRD hex payload cannot contain the declared byte count");
    std::size_t written=0;
    int high=-1;
    for(std::size_t i=0;i<size;++i) {
        if((i&4095)==0)checkpoint(cancel);
        const auto c=lower(static_cast<char>(bytes[i]));
        if(data_space(c))continue;
        const int nibble=digit(c)?c-'0':c>='a' && c<='f'?c-'a'+10:-1;
        valid(nibble>=0,"NRRD hex payload contains an invalid digit");
        valid(written<expected,"NRRD hex payload exceeds the declared byte count");
        if(high<0) {
            high=nibble;
        } else {
            // Output trails the input cursor, so the existing payload buffer can be reused.
            bytes[written++]=static_cast<std::uint8_t>((high<<4)|nibble);
            high=-1;
        }
    }
    valid(written==expected && high<0,"NRRD hex payload is truncated or ends with a half-byte");
}

std::uint64_t raw_window_start(std::uint64_t size,std::uint64_t start,std::uint64_t expected,std::int32_t skip) {
    const auto remaining=size-start;
    valid(expected<=remaining,"NRRD raw byte-skip window is truncated");
    if(skip==-1)return size-expected;
    const auto offset=static_cast<std::uint64_t>(skip);
    valid(offset<=remaining-expected,"NRRD raw byte-skip window is truncated");
    return start+offset;
}

struct DecodedPart {std::unique_ptr<std::uint8_t[]> bytes;std::size_t offset;};
DecodedPart decode_part(FileBytes file,const Header& header,const std::filesystem::path& path,
                        std::string_view name,Budget& budget,std::uint64_t& scan_remaining,qpx_reader_cancel_v1 cancel) {
    const auto sample_bytes=header.raster.sample_bytes;
    std::size_t offset=header.raster.payload;
    const bool detached=!name.empty();
    const auto skip=header.byte_skip.value_or(0);
    const auto lines=skip==-1?0u:header.line_skip;
    const bool raw_window=header.encoding==Header::Encoding::raw && (header.byte_skip || lines);
    if(detached) {
        const auto companion=payload_path(path,name);
        checkpoint(cancel);
        const PayloadFile payload(companion);
        const auto start=payload.skip_lines(lines,scan_remaining,cancel);
        const auto start_byte=raw_window?raw_window_start(payload.size(),start,header.part_bytes,skip):start;
        const auto length=raw_window?header.part_bytes:payload.size()-start_byte;
        file=payload.read_range(start_byte,length,budget,cancel);
        offset=0;
    } else {
        offset=skip_inline_lines(file,offset,lines,cancel);
    }
    const auto encoded=file.size-offset;
    if(header.encoding==Header::Encoding::ascii || header.encoding==Header::Encoding::hex) {
        const auto byte_offset=static_cast<std::size_t>(skip);
        valid(byte_offset<=encoded,"NRRD text payload byte skip exceeds the payload");
        offset+=byte_offset;
        auto* input=file.bytes.get()+offset;
        if(header.encoding==Header::Encoding::hex) {
            decode_hex(input,encoded-byte_offset,header.part_bytes,cancel);
        } else {
            file.bytes=decode_ascii({reinterpret_cast<const char*>(input),encoded-byte_offset},header,budget,cancel);
            offset=0;
        }
    } else if(header.encoding==Header::Encoding::gzip || header.encoding==Header::Encoding::bzip2) {
        const auto byte_offset=static_cast<std::size_t>(skip);
        const auto decoded=static_cast<std::size_t>(header.part_bytes)+byte_offset;
        const auto expand=header.encoding==Header::Encoding::gzip?inflate_gzip:expand_bzip2;
        file.bytes=expand(file.bytes.get()+offset,encoded,decoded,budget,cancel);
        offset=byte_offset;
    } else if(raw_window && !detached) {
        offset=static_cast<std::size_t>(raw_window_start(file.size,offset,header.part_bytes,skip));
    } else {
        valid(header.part_bytes==encoded,"NRRD raw payload is truncated or has trailing data");
    }
    if(header.big && sample_bytes>1)for(std::uint64_t sample=0;sample<header.part_samples;++sample) {
        if((sample&4095)==0)checkpoint(cancel);
        auto* value=file.bytes.get()+offset+sample*sample_bytes;
        std::reverse(value,value+sample_bytes);
    }
    checkpoint(cancel);
    return {std::move(file.bytes),offset};
}
Raster decode(FileBytes file,Header&& header,const std::filesystem::path& path,Budget& budget,qpx_reader_cancel_v1 cancel) {
    auto& result=header.raster;
    auto scan_remaining=work_limit;
    if(header.companion_count<=1) {
        const auto name=header.companion_name(0);
        auto part=decode_part(std::move(file),header,path,name,budget,scan_remaining,cancel);
        result.bytes=std::move(part.bytes);
        result.payload=part.offset;
    } else {
        result.bytes=budget.array(header.total_bytes);
        result.payload=0;
        const auto part_size=static_cast<std::size_t>(header.part_bytes);
        for(std::size_t i=0;i<header.companion_count;++i) {
            // The final raster stays live; each encoded/decoded part is temporary.
            auto part_budget=budget;
            const auto part=decode_part({},header,path,header.companion_name(i),part_budget,scan_remaining,cancel);
            for(std::size_t pos=0;pos<part_size;) {
                checkpoint(cancel);
                const auto count=std::min<std::size_t>(65536,part_size-pos);
                std::memcpy(result.bytes.get()+i*part_size+pos,part.bytes.get()+part.offset+pos,count);
                pos+=count;
            }
        }
    }
    return std::move(result);
}
bool display_sample(const Raster& source,std::uint64_t index,double& result) {
    const auto* bytes=source.bytes.get()+source.payload+index*source.sample_bytes;
    return with_scalar_type(source.sample_type,[&](auto value) {
        using T=decltype(value);
        if constexpr(std::is_floating_point_v<T>) {
            using Bits=FloatBits<T>;
            typename Bits::Word bits=0;
            std::memcpy(&bits,bytes,sizeof(bits));
            // Inspect the stored exponent before evaluating any signaling NaN.
            if((bits&Bits::exponent)==Bits::exponent)return false;
        }
        std::memcpy(&value,bytes,sizeof(value));
        result=static_cast<double>(value);
        return true;
    });
}
const Raster& image(void* value) {require(value!=nullptr,"NRRD reader handle is null");return static_cast<Handle*>(value)->image;}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"NRRD image path is invalid");
    std::string path(value.data,static_cast<std::size_t>(value.size));
    require(path.find('\0')==std::string::npos,"NRRD image path contains NUL");
    require(std::filesystem::u8path(path).is_absolute(),"NRRD image path must be absolute");
    return path;
}
struct Prepared {
    Window window;
    Budget budget;
    std::filesystem::path path;
};
Prepared prepare(const qpx_reader_open_v1* request) {
    require(request && request->struct_size>=sizeof(*request),"NRRD open structure is too small");
    require(request->reserved==0,"NRRD reserved open field must be zero");
    const auto window=parse_window(request->options);
    require(request->work_bytes && request->work_bytes<=work_limit,"NRRD work budget must be between 1 and 256 MiB");
    require(request->path.data && request->path.size && request->path.size<=32768,"NRRD image path is invalid");
    // Reserve path copies for the primary and its optional sibling before opening.
    const auto overhead=UINT64_C(4096)+16*request->path.size+request->options.size+sizeof(Handle)+sizeof(Header);
    require(request->work_bytes>overhead,"NRRD reader work budget is too small for opening overhead");
    checkpoint(request->cancel);
    return {window,{request->work_bytes-overhead},std::filesystem::u8path(path_string(request->path)).lexically_normal()};
}
qpx_reader_status_v1 QPX_READER_CALL source_count(const qpx_reader_open_v1* request,std::uint32_t* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"NRRD source count output is null");
        *output=0;
        auto prepared=prepare(request);
        const auto file=read_primary(prepared.path,prepared.budget,request->cancel,header_limit);
        const auto header=parse_header(file,prepared.window,request->cancel);
        *output=static_cast<std::uint32_t>(header.companion_count);
        return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request,std::uint32_t index,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"NRRD source path output is null");
        auto prepared=prepare(request);
        const auto file=read_primary(prepared.path,prepared.budget,request->cancel,header_limit);
        const auto header=parse_header(file,prepared.window,request->cancel);
        require(index<header.companion_count,"NRRD companion index is out of range");
        const auto companion=payload_path(prepared.path,header.companion_name(index));
        checkpoint(request->cancel);
        const PayloadFile observed(companion);
        const auto name=companion.u8string();
        require(name.size()<=32768,"NRRD detached payload path exceeds reader path limit");
        return text(*output,name.c_str())?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"NRRD source path buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output && output->struct_size>=sizeof(*output),"NRRD reader info structure is too small");
        output->abi_version=QPX_READER_ABI_V1;output->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW|QPX_READER_CAP_SOURCE_FILES;
        bool complete=text(output->provider_id,"org.qupathxx.nrrd");
        complete=text(output->display_name,"NRRD") && complete;
        complete=text(output->version,QPX_NRRD_VERSION) && complete;
        complete=text(output->contract,QPX_NRRD_CONTRACT) && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"NRRD descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"NRRD reader output handle is null");*output=nullptr;
        auto prepared=prepare(request);
        auto file=read_primary(prepared.path,prepared.budget,request->cancel);
        auto header=parse_header(file,prepared.window,request->cancel);
        auto result=std::make_unique<Handle>();
        result->image=decode(std::move(file),std::move(header),prepared.path,prepared.budget,request->cancel);
        *output=result.release();return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"NRRD metadata structure is too small");checkpoint(cancel);
        output->level_count=1;output->width=source.width;output->height=source.height;
        output->has_raw=1;output->channels=source.channels;output->sample_type=source.sample_type;
        output->z_planes=source.z_planes;output->timepoints=source.timepoints;output->series=0;output->series_count=1;
        output->calibration_flags=source.calibration_flags;output->pixel_width_um=source.spacing_um[0];output->pixel_height_um=source.spacing_um[1];output->z_spacing_um=source.spacing_um[2];output->time_interval_seconds=source.time_interval_seconds;
        const bool has_time=source.dimension!=source.spatial_axes;
        const char* order=source.spatial_axes==2?(has_time?"XYT":"XY"):(has_time?"XYZT":"XYZ");
        bool complete=text(output->vendor,"NRRD");complete=text(output->dimension_order,order) && complete;
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
    require(request->level==0,"NRRD reader has only level zero");
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
        const auto area=region(request,4);require(!request->first_channel && !request->channels && !request->z && !request->t,"NRRD display requires zero channel, Z and time selectors");
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
        require(request->channels>0 && request->first_channel<source.channels &&
            request->channels<=source.channels-request->first_channel && request->z<source.z_planes &&
            request->t<source.timepoints,"NRRD raw channel, Z or time range is invalid");
        const auto plane=static_cast<std::uint64_t>(request->t)*source.z_planes+request->z;
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
                const auto from=(plane*source.height+y)*source.width+x;
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

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(
    std::uint32_t requested_abi,qpx_reader_files_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"NRRD source-file API does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"NRRD source-file API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,source_count,source_path};
        return QPX_READER_OK;
    });
}
