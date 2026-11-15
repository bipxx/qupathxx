// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <qupathxx/format_plugin.h>
#include <algorithm>
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
#include <type_traits>

namespace qpx_tsv {
constexpr std::uint64_t work_limit=UINT64_C(256)*1024*1024;
constexpr std::uint64_t token_limit=4096,header_limit=65536;
struct Failure {qpx_reader_status_v1 status;const char* message;};
[[noreturn]] inline void fail(qpx_reader_status_v1 status,const char* message) {throw Failure{status,message};}
inline void require(bool condition,const char* message) {if(!condition)fail(QPX_READER_INVALID_REQUEST,message);}
inline void valid(bool condition,const char* message) {if(!condition)fail(QPX_READER_IO_ERROR,message);}
inline void checkpoint(qpx_reader_cancel_v1 cancel) {
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))fail(QPX_READER_CANCELLED,"TSV operation cancelled");
}
inline bool text(qpx_reader_text_v1& output,std::string_view value) noexcept {
    output.size=value.size();
    if(output.capacity<value.size() || (!value.empty() && !output.data))return false;
    if(!value.empty())std::memcpy(output.data,value.data(),value.size());
    return true;
}
inline qpx_reader_status_v1 failure(qpx_reader_text_v1* error,qpx_reader_status_v1 status,const char* message) noexcept {
    if(error && !text(*error,message))return QPX_READER_BUFFER_TOO_SMALL;
    return status;
}
template<class Function>qpx_reader_status_v1 boundary(qpx_reader_text_v1* error,Function&& function) noexcept {
    if(error)error->size=0;
    try{return function();}
    catch(const Failure& e){return failure(error,e.status,e.message);}
    catch(const std::bad_alloc&){return failure(error,QPX_READER_INTERNAL_ERROR,"TSV plugin allocation failed");}
    catch(const std::filesystem::filesystem_error&){return failure(error,QPX_READER_IO_ERROR,"TSV source filesystem operation failed");}
    catch(const std::exception&){return failure(error,QPX_READER_INTERNAL_ERROR,"TSV plugin internal failure");}
    catch(...){return failure(error,QPX_READER_INTERNAL_ERROR,"TSV plugin internal failure");}
}
struct Budget {
    std::uint64_t remaining;
    template<class T>std::unique_ptr<T[]> array(std::uint64_t count) {
        static_assert(std::is_trivially_destructible<T>::value,"TSV owned arrays must be plain storage");
        if(!count)return {};
        constexpr std::uint64_t bookkeeping=32;
        require(count<=std::numeric_limits<std::uint64_t>::max()/sizeof(T),"TSV allocation size overflows");
        const auto bytes=count*sizeof(T);
        require(bytes<=remaining && bookkeeping<=remaining-bytes,"TSV owned data exceeds work budget");
        remaining-=bytes+bookkeeping;
        // Exact new[] counts; no vector capacity, hidden catalog string copies,
        // or per-object allocations. Plain elements are filled with checkpoints.
        return std::unique_ptr<T[]>(new T[static_cast<std::size_t>(count)]);
    }
};
inline void utf8(std::string_view value,qpx_reader_cancel_v1 cancel,qpx_reader_status_v1 status=QPX_READER_IO_ERROR) {
    auto bad=[&](){fail(status,"TSV text must be valid UTF-8 without NUL or control bytes");};
    std::size_t since_check=4096;
    for(std::size_t i=0;i<value.size();) {
        if(since_check>=4093){checkpoint(cancel);since_check=0;}
        const auto first=static_cast<unsigned char>(value[i]);
        if(first<0x80) {if(first<0x20 || first==0x7f)bad();++i;++since_check;continue;}
        unsigned count=0;std::uint32_t code=0,minimum=0;
        if(first>=0xc2 && first<=0xdf){count=2;code=first&0x1f;minimum=0x80;}
        else if(first>=0xe0 && first<=0xef){count=3;code=first&0x0f;minimum=0x800;}
        else if(first>=0xf0 && first<=0xf4){count=4;code=first&7;minimum=0x10000;}
        else bad();
        if(count>value.size()-i)bad();
        for(unsigned j=1;j<count;++j) {
            const auto next=static_cast<unsigned char>(value[i+j]);
            if((next&0xc0)!=0x80)bad();code=(code<<6)|(next&0x3f);
        }
        if(code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff) || (code>=0x80 && code<=0x9f))bad();
        i+=count;since_check+=count;
    }
    checkpoint(cancel);
}
struct Span {std::uint64_t offset,size;};
inline std::string_view view(const char* bytes,Span span) {return {bytes+span.offset,static_cast<std::size_t>(span.size)};}
enum class End {tab,line,eof};
struct Token {Span span;End end;};
struct Cursor {
    const char* bytes;std::uint64_t size,position;qpx_reader_cancel_v1 cancel;
    bool has() const noexcept {return position<size;}
    Token next() {
        checkpoint(cancel);
        valid(has() || (position==size && size && bytes[size-1]=='\t'),"TSV row is truncated");const auto start=position;
        while(position<size) {
            if((position&4095)==0)checkpoint(cancel);
            const auto character=static_cast<unsigned char>(bytes[position]);
            if(character=='\t' || character=='\n' || character=='\r')break;
            valid(character>=0x20 && character!=0x7f,"TSV contains NUL or control bytes");
            valid(position-start<token_limit,"TSV token exceeds 4096 bytes");++position;
        }
        const Span span{start,position-start};
        checkpoint(cancel);
        if(position==size)return {span,End::eof};
        const auto separator=bytes[position++];
        if(separator=='\t')return {span,End::tab};
        if(separator=='\r') {valid(position<size && bytes[position]=='\n',"TSV requires LF or CRLF line endings");++position;}
        return {span,End::line};
    }
};
inline void label(const char* bytes,Span span,qpx_reader_cancel_v1 cancel,bool empty=false) {
    valid((empty || span.size) && span.size<=token_limit,"TSV identifier or label is empty or exceeds 4096 bytes");
    utf8(view(bytes,span),cancel);
}
inline std::uint64_t integer(std::string_view value,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    valid(!value.empty(),"TSV count is empty");std::uint64_t result=0;
    for(char character:value) {
        valid(character>='0' && character<='9',"TSV counts require unsigned decimal integers");
        const auto digit=static_cast<unsigned>(character-'0');
        valid(result<=UINT64_MAX/10 && (result<UINT64_MAX/10 || digit<=UINT64_MAX%10),"TSV count exceeds UInt64");
        result=result*10+digit;
    }
    checkpoint(cancel);return result;
}
inline std::int32_t plane(std::string_view value,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    if(value=="-1")return -1;
    const auto result=integer(value,cancel);valid(result<=INT32_MAX,"TSV plane exceeds Int32");return static_cast<std::int32_t>(result);
}
inline double coordinate(std::string_view value,qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    valid(!value.empty(),"TSV coordinate is empty");double result=0;
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result,std::chars_format::general);
    valid(parsed.ec==std::errc{} && parsed.ptr==value.data()+value.size() && std::isfinite(result) && result>=0,
        "TSV coordinates require finite nonnegative decimal image-pixel values");
    checkpoint(cancel);return result;
}
struct Source {std::unique_ptr<char[]> bytes;std::uint64_t size;Budget budget;};
inline Source source(const qpx_reader_open_v1* request,std::size_t handle_bytes) {
    require(request && request->struct_size>=sizeof(*request),"TSV open structure is too small");
    require(!request->reserved,"TSV open reserved field must be zero");
    require(!request->options.size,"TSV selected profile accepts empty options only");
    require(request->work_bytes && request->work_bytes<=work_limit,"TSV work budget must be between 1 and 256 MiB");
    require(request->path.data && request->path.size && request->path.size<=32768,"TSV source path is invalid");
    const auto overhead=UINT64_C(4096)+8*request->path.size+handle_bytes;
    require(request->work_bytes>overhead,"TSV work budget is too small for opening overhead");
    checkpoint(request->cancel);utf8({request->path.data,static_cast<std::size_t>(request->path.size)},request->cancel,QPX_READER_INVALID_REQUEST);
    const std::string path(request->path.data,static_cast<std::size_t>(request->path.size));
    const auto native=std::filesystem::u8path(path);
    require(native.is_absolute(),"TSV source path must be absolute UTF-8");
    Source result{{},std::filesystem::file_size(native),{request->work_bytes-overhead}};
    result.bytes=result.budget.array<char>(result.size);
    std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(native.c_str(),L"rb"),std::fclose);
    if(!file)fail(QPX_READER_IO_ERROR,"TSV source cannot be opened");
    if(std::setvbuf(file.get(),nullptr,_IONBF,0))fail(QPX_READER_IO_ERROR,"TSV unbuffered input could not be configured");
    for(std::uint64_t position=0;position<result.size;) {
        checkpoint(request->cancel);const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(4096,result.size-position));
        valid(std::fread(result.bytes.get()+position,1,count,file.get())==count,"TSV source is truncated or changed");position+=count;
    }
    valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"TSV source changed during reading");
    checkpoint(request->cancel);return result;
}
template<class GetSpan>void unique_ids(const char* bytes,std::uint64_t count,std::uint64_t* index,GetSpan get,qpx_reader_cancel_v1 cancel) {
    for(std::uint64_t i=0;i<count;++i){if((i&511)==0)checkpoint(cancel);index[i]=i;}
    auto less=[&](std::uint64_t a,std::uint64_t b) {checkpoint(cancel);return view(bytes,get(a))<view(bytes,get(b));};
    // In-place iterative heap sort has no allocator or uncharged recursion.
    auto sift=[&](std::uint64_t root,std::uint64_t length) {
        for(;;) {
            checkpoint(cancel);if(root>=length/2)return;
            auto child=root*2+1;if(child+1<length && less(index[child],index[child+1]))++child;
            if(!less(index[root],index[child]))return;
            std::swap(index[root],index[child]);root=child;
        }
    };
    for(auto i=count/2;i>0;--i)sift(i-1,count);
    for(auto length=count;length>1;--length){checkpoint(cancel);std::swap(index[0],index[length-1]);sift(0,length-1);}
    for(std::uint64_t i=1;i<count;++i) {checkpoint(cancel);valid(view(bytes,get(index[i-1]))!=view(bytes,get(index[i])),"TSV source identifiers are duplicated");}
}
inline std::uint64_t page(std::uint64_t first,std::uint64_t count,std::uint64_t total) {
    require(count && first<=total,"TSV page range is invalid");return std::min(count,total-first);
}
inline void descriptor(qpx_format_info_v1* output,std::uint32_t domain,const char* id,const char* format,const char* name,const char* contract,const char* profile,bool& complete) {
    require(output && output->struct_size>=sizeof(*output),"TSV descriptor structure is too small");
    output->abi_version=QPX_FORMAT_ABI_V1;output->domain=domain;output->reserved=0;
    complete=text(output->provider_id,id);complete=text(output->format_id,format)&&complete;
    complete=text(output->display_name,name)&&complete;complete=text(output->version,"1.0.0")&&complete;
    complete=text(output->contract,contract)&&complete;complete=text(output->profile,profile)&&complete;
}
} // namespace qpx_tsv
