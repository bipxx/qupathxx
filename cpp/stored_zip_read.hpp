// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "stored_zip_write.hpp"
#include <algorithm>
#include <fstream>

namespace qpx::detail::stored_zip {
inline std::uint32_t read_little(const char* data, unsigned size) {
    std::uint32_t result = 0;
    for (unsigned i = 0; i < size; ++i)
        result |= std::uint32_t(static_cast<unsigned char>(data[i])) << (8 * i);
    return result;
}

struct ExchangeMember {
    Member member;
    std::uint32_t encoded_size,method,data_offset;
};

class Input {
    std::ifstream file_;
    std::uint64_t size_, offset_ = 0;
    const std::atomic<bool>* cancelled_;
    const char* profile_;

    [[noreturn]] void invalid(const char* reason) const {
        throw std::invalid_argument(std::string("Invalid ") + profile_ + ": " + reason);
    }
    void expect(std::string_view expected) {
        std::array<char, 64> data{};
        if (expected.size() > data.size())
            throw std::logic_error("Unexpected ZIP record size");
        read(data.data(), expected.size());
        if (std::string_view(data.data(), expected.size()) != expected)
            invalid("unexpected ZIP member or directory");
    }
    void skip_exchange_extras(std::uint32_t bytes) {
        if(bytes>1024)invalid("ZIP extra fields exceed exchange limit");
        bool timestamp=false,owner=false;
        while(bytes){
            if(bytes<4)invalid("truncated ZIP extra field");
            const auto field=record<4>();
            const auto kind=read_little(field.data(),2),length=read_little(field.data()+2,2);
            bytes-=4;
            if(length>bytes)invalid("truncated ZIP extra field payload");
            if(kind==0x5455 && !timestamp)timestamp=true;
            else if(kind==0x7875 && !owner)owner=true;
            else invalid("unsupported or repeated ZIP extra field");
            seek(offset_+length);
            bytes-=length;
        }
    }

public:
    Input(const std::filesystem::path& path, std::uint64_t size, const std::atomic<bool>* cancelled,
          const char* profile)
        : file_(path, std::ios::binary), size_(size), cancelled_(cancelled), profile_(profile) {}

    void seek(std::uint64_t offset) {
        export_checkpoint(cancelled_);
        if (offset > size_ || !file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg))
            invalid("invalid archive offset");
        offset_ = offset;
    }
    std::uint64_t offset() const { return offset_; }

    void read(char* data, std::size_t count) {
        export_checkpoint(cancelled_);
        if (count > size_ - offset_ || !file_.read(data, static_cast<std::streamsize>(count)))
            invalid("truncated archive");
        offset_ += count;
    }
    template <std::size_t N> std::array<char, N> record() {
        std::array<char, N> bytes{};
        read(bytes.data(), bytes.size());
        return bytes;
    }
    Member member(std::string_view name, std::size_t maximum) {
        const auto start = zip_size(static_cast<std::size_t>(offset_));
        const auto header = record<30>();
        const Member result{name, start, read_little(header.data() + 22, 4),
                            read_little(header.data() + 14, 4)};
        if (header != local_header(result) || result.size > maximum)
            invalid("unsupported ZIP member header or size");
        expect(name);
        if (result.size > size_ - offset_)
            invalid("truncated ZIP member");
        return result;
    }
    // Fixed-name ZIP32 exchange members. Other callers keep the canonical
    // stored format above, including the writer's exact framing.
    template <std::size_t N> std::array<ExchangeMember,N> exchange_members(
            const std::array<std::string_view,N>& names) {
        static_assert(N>0 && N<65535);
        if(size_<22)invalid("truncated ZIP directory");
        const auto end_offset=size_-22;
        seek(end_offset);
        const auto end=record<22>();
        const auto directory=read_little(end.data()+16,4),directory_size=read_little(end.data()+12,4);
        if(end!=end_record(N,directory,directory_size) || std::uint64_t(directory)+directory_size!=end_offset)
            invalid("unsupported ZIP directory end");
        seek(directory);
        std::array<ExchangeMember,N> members{};
        std::array<std::array<char,46>,N> central{};
        std::array<std::size_t,N> order{};
        for(auto& slot:order){
            const auto header=record<46>();
            const auto field=[&](unsigned at,unsigned width){return read_little(header.data()+at,width);};
            const auto method=field(10,2),encoded=field(20,4),decoded=field(24,4),offset=field(42,4);
            const auto flags=field(8,2),length=field(28,2);
            const auto allowed_flags=method==8?0x80eu:0x808u;
            std::array<char,64> name{};
            if(field(0,4)!=0x02014b50u || field(6,2)>20 || (flags & ~allowed_flags) ||
               (method!=0 && method!=8) || (method==0 && encoded!=decoded) ||
               !length || length>name.size() || field(32,4)!=0 || offset>=directory)
                invalid("unsupported ZIP central member header");
            read(name.data(),length);
            const auto found=std::find(names.begin(),names.end(),std::string_view(name.data(),length));
            if(found==names.end())invalid("unexpected ZIP member name");
            slot=static_cast<std::size_t>(found-names.begin());
            if(!members[slot].member.name.empty())invalid("duplicate ZIP member name");
            members[slot]={{*found,offset,decoded,field(16,4)},encoded,method,0};
            central[slot]=header;
            skip_exchange_extras(field(30,2));
        }
        if(offset_!=end_offset)invalid("unexpected bytes in ZIP central directory");
        std::sort(order.begin(),order.end(),[&](auto a,auto b){return members[a].member.offset<members[b].member.offset;});
        for(std::size_t i=1;i<N;++i)
            if(members[order[i-1]].member.offset==members[order[i]].member.offset)
                invalid("central directory repeats a ZIP member offset");
        if(members[order.front()].member.offset!=0)invalid("central directory references an unknown member offset");
        for(std::size_t i=0;i<N;++i){
            const auto slot=order[i];
            const auto boundary=i+1<N?members[order[i+1]].member.offset:directory;
            read_exchange_local(members[slot],central[slot],boundary);
        }
        return members;
    }
    template <std::size_t N> void finish(const std::array<Member, N>& members) {
        const auto start = zip_size(static_cast<std::size_t>(offset_));
        for (const auto& member : members) {
            const auto expected = central_header(member);
            expect({expected.data(), expected.size()});
            expect(member.name);
        }
        const auto size = zip_size(static_cast<std::size_t>(offset_) - start);
        finish_directory(N,start,size);
    }
private:
    void read_exchange_local(ExchangeMember& entry,const std::array<char,46>& central,std::uint32_t boundary) {
        seek(entry.member.offset);
        const auto header=record<30>();
        const bool descriptor=read_little(header.data()+6,2)&8;
        const bool local_sizes=descriptor?std::all_of(header.begin()+14,header.begin()+26,[](char c){return c==0;}):
            std::equal(header.begin()+14,header.begin()+26,central.begin()+16);
        if(read_little(header.data(),4)!=0x04034b50u || !local_sizes ||
           !std::equal(header.begin()+4,header.begin()+14,central.begin()+6) ||
           read_little(header.data()+26,2)!=entry.member.name.size())
            invalid("central directory differs from ZIP member");
        expect(entry.member.name);
        skip_exchange_extras(read_little(header.data()+28,2));
        const auto data_end=offset_+entry.encoded_size;
        if(data_end>boundary)invalid("ZIP member overlaps its next record");
        entry.data_offset=zip_size(static_cast<std::size_t>(offset_));
        if(!descriptor){
            if(data_end!=boundary)invalid("unexpected bytes after ZIP member");
            return;
        }
        const auto length=boundary-data_end;
        seek(data_end);
        if(length==16 && read_little(record<4>().data(),4)!=0x08074b50u)
            invalid("invalid ZIP data descriptor signature");
        if(length!=12 && length!=16)invalid("invalid ZIP32 data descriptor size");
        const auto fields=record<12>();
        if(!std::equal(fields.begin(),fields.end(),central.begin()+16))
            invalid("ZIP data descriptor differs from central directory");
    }
    void finish_directory(unsigned count,std::uint32_t start,std::uint32_t size) {
        const auto end = end_record(count, start, size);
        expect({end.data(), end.size()});
        if (offset_ != size_)
            invalid("unexpected bytes after ZIP directory");
    }
};
} // namespace qpx::detail::stored_zip
