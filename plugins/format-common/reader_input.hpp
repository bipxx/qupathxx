// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <qupathxx/reader_plugin.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace qpx_binary {
struct Failure { qpx_reader_status_v1 status; const char* message; };
[[noreturn]] inline void fail(const char* message, qpx_reader_status_v1 status=QPX_READER_IO_ERROR) { throw Failure{status,message}; }
inline void valid(bool condition,const char* message) { if(!condition) fail(message); }
inline void require(bool condition,const char* message) { if(!condition) fail(message,QPX_READER_INVALID_REQUEST); }
inline void poll(qpx_reader_cancel_v1 c) { if(c.is_cancelled && c.is_cancelled(c.context)) fail("Binary format operation cancelled",QPX_READER_CANCELLED); }
inline bool text(qpx_reader_text_v1& out,std::string_view value) noexcept {
 out.size=value.size(); if(out.capacity<value.size() || (!value.empty() && !out.data))return false;
 if(!value.empty())std::memcpy(out.data,value.data(),value.size()); return true;
}
template<class F> qpx_reader_status_v1 boundary(qpx_reader_text_v1* error,F&& action) noexcept {
 if(error)error->size=0;
 try { return action(); }
 catch(...) {
  auto status=QPX_READER_INTERNAL_ERROR; const char* message="Binary format internal failure";
  try { throw; } catch(const Failure& f){status=f.status;message=f.message;}
  catch(const std::bad_alloc&){status=QPX_READER_INVALID_REQUEST;message="Binary format work budget exhausted";}
  catch(const std::filesystem::filesystem_error&){status=QPX_READER_IO_ERROR;message="Binary source filesystem operation failed";} catch(...){}
  if(error && !text(*error,message))return QPX_READER_BUFFER_TOO_SMALL; return status;
 }
}
struct Bytes {
 const std::uint8_t* data=nullptr; std::size_t size=0;
 Bytes sub(std::uint64_t offset,std::uint64_t length) const {
  valid(offset<=size && length<=size-offset,"Binary field is outside source bounds");
  return {data+std::size_t(offset),std::size_t(length)};
 }
 std::uint64_t number(std::size_t offset,unsigned count,bool big=false) const {
  const auto p=sub(offset,count);std::uint64_t n=0;
  for(unsigned i=0;i<count;++i)n|=std::uint64_t(p.data[i])<<(8*(big?count-1-i:i));return n;
 }
 std::uint16_t be16(std::size_t n) const { return std::uint16_t(number(n,2,true)); }
 std::uint32_t be32(std::size_t n) const { return std::uint32_t(number(n,4,true)); }
 std::uint16_t le16(std::size_t n) const { return std::uint16_t(number(n,2)); }
 std::uint32_t le32(std::size_t n) const { return std::uint32_t(number(n,4)); }
 std::uint64_t le64(std::size_t n) const { return number(n,8); }
 float real(std::size_t n) const {
  const auto bits=be32(n);float f;std::memcpy(&f,&bits,4);
  valid(std::isfinite(f),"Binary coordinate or style is not finite");return f;
 }
 std::string_view view() const { return {reinterpret_cast<const char*>(data),size}; }
};
inline void utf8(std::string_view s,qpx_reader_cancel_v1 c,bool controls=false) {
 for(std::size_t i=0;i<s.size();) {
  if((i&1023)==0)poll(c);auto b=std::uint8_t(s[i++]);std::uint32_t code=b,min=0;unsigned n=0;
  if(b<128){valid(b && (controls || (b>=32 && b!=127)),"Binary text contains a forbidden control or NUL");continue;}
  if(b>=194 && b<=223){code=b&31;n=1;min=128;}
  else if(b>=224 && b<=239){code=b&15;n=2;min=2048;}
  else if(b>=240 && b<=244){code=b&7;n=3;min=65536;}else fail("Invalid UTF-8 source text");
  valid(n<=s.size()-i,"Truncated UTF-8 source text");
  while(n--){b=std::uint8_t(s[i++]);valid((b&192)==128,"Invalid UTF-8 continuation");code=(code<<6)|(b&63);}
  valid(code>=min && code<=0x10ffff && !(code>=0xd800 && code<=0xdfff) && (controls || code<128 || code>159),"Invalid Unicode source value");
 }poll(c);
}
inline void append_utf8(std::pmr::string& out,std::uint32_t c) {
 valid(c && c<=0x10ffff && !(c>=0xd800 && c<=0xdfff),"Invalid UTF-16 source value");
 if(c<128)out.push_back(char(c));else if(c<2048){out.push_back(char(192|(c>>6)));out.push_back(char(128|(c&63)));}
 else if(c<65536){out.push_back(char(224|(c>>12)));out.push_back(char(128|((c>>6)&63)));out.push_back(char(128|(c&63)));}
 else {out.push_back(char(240|(c>>18)));out.push_back(char(128|((c>>12)&63)));out.push_back(char(128|((c>>6)&63)));out.push_back(char(128|(c&63)));}
}
inline std::pmr::string utf16(Bytes b,std::pmr::memory_resource* r,qpx_reader_cancel_v1 c) {
 valid(!(b.size&1) && b.size<=512*1024,"UTF-16 field exceeds bounds");std::pmr::string out(r);out.reserve(b.size*2);
 for(std::size_t i=0;i<b.size;i+=2){if((i&1023)==0)poll(c);std::uint32_t v=b.be16(i);
  if(v>=0xd800 && v<=0xdbff){valid(b.size-i>=4,"Truncated UTF-16 surrogate");auto tail=b.be16(i+2);valid(tail>=0xdc00 && tail<=0xdfff,"Invalid UTF-16 surrogate");v=0x10000+((v-0xd800)<<10)+(tail-0xdc00);i+=2;}
  append_utf8(out,v);
 }return out;
}
struct Arena {
 std::unique_ptr<std::byte[]> storage;std::pmr::monotonic_buffer_resource memory;
 explicit Arena(std::size_t size):storage(new std::byte[size]),memory(storage.get(),size,std::pmr::null_memory_resource()){}
 Arena(const Arena&)=delete;
};
inline Bytes read(qpx_reader_string_v1 source,qpx_reader_cancel_v1 cancel,std::pmr::memory_resource* r,
                  std::uint64_t file_limit=64u*1024*1024) {
 require(source.data && source.size && source.size<=32768,"Invalid binary source path");
 const std::string_view path(source.data,std::size_t(source.size));utf8(path,cancel);
 const auto native=std::filesystem::u8path(path);require(native.is_absolute(),"Binary source path must be absolute");poll(cancel);
 const auto size=std::filesystem::file_size(native);valid(size && size<=64u*1024*1024,"Binary source must contain 1 byte to 64 MiB");
 require(size<=file_limit,"Binary source exceeds the requested file allowance");
 auto* data=static_cast<std::uint8_t*>(r->allocate(std::size_t(size),alignof(std::uint8_t)));
 std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(native.c_str(),L"rb"),std::fclose);
 valid(bool(file),"Cannot open binary source");valid(!std::setvbuf(file.get(),nullptr,_IONBF,0),"Cannot configure binary input");
 for(std::size_t at=0;at<size;){poll(cancel);auto n=std::min<std::size_t>(4096,std::size_t(size)-at);valid(std::fread(data+at,1,n,file.get())==n,"Binary source changed or truncated");at+=n;}
 valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"Binary source changed while reading");poll(cancel);return {data,std::size_t(size)};
}
inline Bytes read(const qpx_reader_open_v1& request,std::pmr::memory_resource* r) {
 return read(request.path,request.cancel,r);
}
} // namespace qpx_binary
