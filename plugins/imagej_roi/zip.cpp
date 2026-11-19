// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "roi.hpp"
#include <windows.h>
#include <zlib.h>
#include <cstddef>

namespace qpx_imagej {
namespace {
struct Entry {
 Bytes name,extra,comment,header;
 std::uint64_t compressed=0,size=0,offset=0;
 std::uint32_t crc=0,attributes=0;
 std::uint16_t flags=0,method=0,needed=0;
 bool wide=false;
};
void zip64(Bytes extra,std::uint64_t& size,std::uint64_t& compressed,std::uint64_t& offset,std::uint32_t& disk) {
 bool found=false;
 for(std::size_t at=0;at<extra.size;){valid(extra.size-at>=4,"Truncated ZIP extra field");const auto type=extra.le16(at),n=extra.le16(at+2);at+=4;const auto field=extra.sub(at,n);at+=n;
  if(type!=1)continue;valid(!found,"Duplicate ZIP64 extra field");found=true;std::size_t i=0;
  auto value=[&](std::uint64_t& v){if(v==0xffffffffu){v=field.le64(i);i+=8;}};value(size);value(compressed);value(offset);
  if(disk==0xffff){disk=field.le32(i);i+=4;}valid(i==field.size,"Unexpected ZIP64 extra values");
 }
 valid(size!=0xffffffffu && compressed!=0xffffffffu && offset!=0xffffffffu && disk!=0xffff,"ZIP64 fields are missing");
}
std::pmr::string entry_name(Bytes b,bool unicode,std::pmr::memory_resource* r,qpx_reader_cancel_v1 c) {
 valid(b.size && b.size<=1024,"ZIP entry name exceeds profile");std::pmr::string name(r);
 if(unicode){utf8(b.view(),c);name=b.view();}
 else {wchar_t wide[1024];const auto count=MultiByteToWideChar(437,0,b.view().data(),int(b.size),wide,1024);valid(count>0,"Invalid ZIP CP437 name");
  for(int i=0;i<count;++i)append_utf8(name,std::uint32_t(wide[i]));utf8(name,c);
 }
 valid(name.size()<=240 && name.front()!='/' && name.find('\\')==name.npos && name.find(':')==name.npos,"Unsafe or overlong ZIP entry name");
 for(std::size_t at=0;at<name.size();){const auto end=name.find('/',at);const auto part=std::string_view(name).substr(at,end==name.npos?end:end-at);valid(!part.empty() && part!="." && part!="..","ZIP path has an unsafe component");if(end==name.npos)break;at=end+1;}
 return name;
}
struct InflateArena {
 std::byte* data;std::size_t size,at=0;
 static voidpf allocate(voidpf opaque,uInt count,uInt bytes) noexcept {
  auto& a=*static_cast<InflateArena*>(opaque);const auto alignment=alignof(std::max_align_t);const auto start=(a.at+alignment-1)&~(alignment-1);
  if(bytes && count>SIZE_MAX/bytes)return nullptr;const auto n=std::size_t(count)*bytes;
  if(start>a.size || n>a.size-start)return nullptr;a.at=start+n;std::memset(a.data+start,0,n);return a.data+start;
 }
 static void release(voidpf,voidpf) noexcept {}
};
Bytes inflate(Bytes source,std::size_t size,std::pmr::memory_resource* r,InflateArena& arena,qpx_reader_cancel_v1 cancel) {
 auto* out=static_cast<std::uint8_t*>(r->allocate(size+1,alignof(std::uint8_t)));arena.at=0;
 z_stream stream{};stream.zalloc=InflateArena::allocate;stream.zfree=InflateArena::release;stream.opaque=&arena;
 valid(inflateInit2(&stream,-MAX_WBITS)==Z_OK,"ZIP inflater initialization failed");
 struct End {z_stream* stream;~End(){inflateEnd(stream);}} end{&stream};
 std::size_t input=0,output=0;int status=Z_OK;
 while(status!=Z_STREAM_END){poll(cancel);
  if(!stream.avail_in && input<source.size){const auto n=std::min<std::size_t>(4096,source.size-input);stream.next_in=const_cast<Bytef*>(source.data+input);stream.avail_in=uInt(n);input+=n;}
  if(!stream.avail_out){valid(output<size+1,"ZIP deflate exceeds declared size");const auto n=std::min<std::size_t>(4096,size+1-output);stream.next_out=out+output;stream.avail_out=uInt(n);output+=n;}
  const auto before_in=stream.total_in,before_out=stream.total_out;status=::inflate(&stream,Z_NO_FLUSH);
  valid(status==Z_OK || status==Z_STREAM_END,"Invalid or truncated ZIP deflate stream");
  valid(stream.total_out<=size && (stream.total_in!=before_in || stream.total_out!=before_out || status==Z_STREAM_END),"ZIP deflate size/progress differs");
 }
 valid(stream.total_in==source.size && stream.total_out==size,"ZIP deflate contains trailing or missing bytes");poll(cancel);return {out,size};
}
}
void zip(Handle& h,Bytes b,qpx_reader_cancel_v1 cancel) {
 poll(cancel);valid(b.size>=22,"Truncated ZIP archive");auto* r=&h.arena.memory;
 std::size_t eocd=b.size;
 for(std::size_t distance=22;distance<=std::min<std::size_t>(b.size,65557);++distance){const auto at=b.size-distance;if(b.le32(at)==0x06054b50 && b.le16(at+20)==distance-22){eocd=at;break;}}
 valid(eocd<b.size && !b.le16(eocd+4) && !b.le16(eocd+6),"ZIP end record missing or multi-disk archive");
 std::uint64_t count=b.le16(eocd+10),directory_size=b.le32(eocd+12),directory=b.le32(eocd+16),directory_end=eocd;
 valid(b.le16(eocd+8)==count,"ZIP disk entry counts differ");
 if(count==65535 || directory_size==0xffffffffu || directory==0xffffffffu){
  valid(eocd>=20 && b.le32(eocd-20)==0x07064b50 && !b.le32(eocd-16) && b.le32(eocd-4)==1,"Missing or multi-disk ZIP64 locator");
  const auto at=b.le64(eocd-12);valid(at<=eocd-20 && eocd-20-at>=56,"ZIP64 end record overlaps its locator");const auto record=b.sub(at,56);valid(record.le32(0)==0x06064b50,"Missing ZIP64 end record");
  const auto length=record.le64(4);valid(length>=44 && at<=eocd-20 && length==eocd-20-at-12,"ZIP64 end record extent differs");
  valid(!record.le32(16) && !record.le32(20) && record.le64(24)==record.le64(32),"Multi-disk ZIP64 archive");
  const auto actual_count=record.le64(32),actual_size=record.le64(40),actual_directory=record.le64(48);
  valid((count==65535 || count==actual_count) && (directory_size==0xffffffffu || directory_size==actual_size) && (directory==0xffffffffu || directory==actual_directory),"ZIP64 and classic end records disagree");
  count=actual_count;directory_size=actual_size;directory=actual_directory;directory_end=at;
 }
 valid(count && count<=100000 && directory<=directory_end && directory_size==directory_end-directory,"ZIP central directory extent/count differs");
 const auto cd=b.sub(directory,directory_size);std::pmr::vector<Entry> entries(r);entries.reserve(std::size_t(count));std::size_t pos=0;
 for(std::uint64_t i=0;i<count;++i){poll(cancel);const auto v=cd.sub(pos,46);valid(v.le32(0)==0x02014b50,"Invalid ZIP central directory signature");Entry e;
  e.header=v;e.needed=v.le16(6);e.flags=v.le16(8);e.method=v.le16(10);e.crc=v.le32(16);e.compressed=v.le32(20);e.size=v.le32(24);e.offset=v.le32(42);e.attributes=v.le32(38);
  valid(e.needed<=45 && !(e.flags&~0x080e) && (e.method==0 || e.method==8) && (e.method==8 || !(e.flags&6)),"ZIP encryption, codec or feature outside stored/deflate profile");
  const auto name=v.le16(28),extra=v.le16(30),comment=v.le16(32);e.name=cd.sub(pos+46,name);e.extra=cd.sub(pos+46+name,extra);e.comment=cd.sub(pos+46+name+extra,comment);pos+=46+name+extra+comment;
  e.wide=e.size==0xffffffffu || e.compressed==0xffffffffu;std::uint32_t disk=v.le16(34);zip64(e.extra,e.size,e.compressed,e.offset,disk);valid(!disk,"ZIP entry references another disk");
  valid(e.size<=8u*1024*1024 && e.compressed<=64u*1024*1024 && e.offset<directory,"ZIP entry exceeds selected resource bounds");
  const auto unix_type=(e.attributes>>16)&0170000;valid(!unix_type || unix_type==0100000 || unix_type==0040000,"ZIP special files or links are not admitted");entries.push_back(e);
 }
 valid(pos==cd.size,"Trailing ZIP central directory bytes");
 std::sort(entries.begin(),entries.end(),[&](const auto& a,const auto& v){poll(cancel);return a.offset<v.offset;});
 auto* scratch=static_cast<std::byte*>(r->allocate(128*1024,alignof(std::max_align_t)));InflateArena arena{scratch,128*1024};
 std::pmr::vector<std::pmr::string> names(r);names.reserve(entries.size());std::uint64_t previous_end=0;
 for(std::size_t entry_index=0;entry_index<entries.size();++entry_index){const auto& e=entries[entry_index];poll(cancel);valid(e.offset==previous_end,"ZIP local entries overlap, have a prefix or unexplained gaps");const auto local=b.sub(e.offset,30);
  valid(local.le32(0)==0x04034b50 && local.le16(4)==e.needed && local.le16(6)==e.flags && local.le16(8)==e.method,"ZIP local and central headers disagree");
  const auto name_size=local.le16(26),extra_size=local.le16(28);const auto local_name=b.sub(e.offset+30,name_size),local_extra=b.sub(e.offset+30+name_size,extra_size);
  valid(local_name.view()==e.name.view(),"ZIP local/central names differ");auto name=entry_name(e.name,bool(e.flags&2048),r,cancel);names.push_back(name);
  std::uint64_t local_size=local.le32(22),local_compressed=local.le32(18),unused_offset=0;std::uint32_t unused_disk=0;
  const bool local_wide=local_size==0xffffffffu || local_compressed==0xffffffffu;zip64(local_extra,local_size,local_compressed,unused_offset,unused_disk);
  if(e.flags&8)valid((!local.le32(14) || local.le32(14)==e.crc) && (!local_size || local_size==e.size) && (!local_compressed || local_compressed==e.compressed),"ZIP descriptor local sizes/CRC disagree");
  else valid(local.le32(14)==e.crc && local_size==e.size && local_compressed==e.compressed,"ZIP local sizes/CRC disagree");
  const auto data_at=e.offset+30+name_size+extra_size;valid(data_at<=directory && e.compressed<=directory-data_at,"ZIP payload overlaps central directory");const auto compressed=b.sub(data_at,e.compressed);auto end=data_at+e.compressed;
  if(e.flags&8){const auto bytes=(e.wide || local_wide)?8u:4u;const auto next=entry_index+1<entries.size()?entries[entry_index+1].offset:directory;
   // The optional signature can equal an unsigned descriptor's actual CRC.
   // Resolve by complete values AND the next central-indexed record boundary.
   auto matches=[&](bool signature){const auto at=end+(signature?4:0);const auto length=4+2*bytes;
    return at<=next && length==next-at && (!signature || b.le32(std::size_t(end))==0x08074b50) && b.le32(std::size_t(at))==e.crc && b.number(std::size_t(at+4),bytes)==e.compressed && b.number(std::size_t(at+4+bytes),bytes)==e.size;};
   const bool plain=matches(false),signed_record=matches(true);valid(plain!=signed_record,"ZIP data descriptor values or record extent differ");end=next;}
  valid(end<=directory,"ZIP descriptor overlaps central directory");previous_end=end;
  const bool is_directory=name.back()=='/';valid(is_directory?e.size==0:(name.size()>=4 && name.substr(name.size()-4)==".roi"),"ZIP profile accepts ROI members and empty directories only");
  Bytes decoded=compressed;if(e.method==8)decoded=inflate(compressed,std::size_t(e.size),r,arena,cancel);else valid(e.compressed==e.size,"Stored ZIP sizes differ");
  auto crc=crc32(0,Z_NULL,0);for(std::size_t at=0;at<decoded.size;){poll(cancel);auto n=std::min<std::size_t>(4096,decoded.size-at);crc=crc32(crc,decoded.data+at,uInt(n));at+=n;}valid(crc==e.crc,"ZIP entry CRC mismatch");if(is_directory)continue;
  Facts facts(r);hex_facts(facts,"zip.central-header",e.header);hex_facts(facts,"zip.central-extra",e.extra);hex_facts(facts,"zip.local-header",local);hex_facts(facts,"zip.local-extra",local_extra);hex_facts(facts,"zip.comment",e.comment);hex_facts(facts,"zip.archive-comment",b.sub(eocd+22,b.le16(eocd+20)));hex_facts(facts,"zip.raw-name",e.name);
  fact(facts,"zip.entry",name);number(facts,"zip.entries",count);number(facts,"zip.method",e.method);number(facts,"zip.crc32",e.crc);number(facts,"zip.uncompressed-bytes",e.size);
  const std::pmr::string id("zip:"+std::string(name),r);roi(h,decoded,id,std::string_view(name).substr(0,name.size()-4),facts,cancel);
 }
 valid(previous_end==directory && !h.rows.empty(),"ZIP has no ROI objects or inconsistent local directory extent");
 std::sort(names.begin(),names.end(),[&](const auto& a,const auto& v){poll(cancel);return a<v;});for(std::size_t i=1;i<names.size();++i)valid(names[i-1]!=names[i],"ZIP entry identities are duplicated");
}
} // namespace qpx_imagej
