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
    std::uint32_t encoded_size;
    std::uint32_t method;
    std::array<char,30> local;
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
    ExchangeMember exchange_member(std::string_view name) {
        const auto start = zip_size(static_cast<std::size_t>(offset_));
        const auto header = record<30>();
        const auto field = [&](unsigned at,unsigned width) {return read_little(header.data()+at,width);};
        const auto method=field(8,2), encoded=field(18,4), decoded=field(22,4);
        const auto flags=field(6,2);
        const auto allowed_flags=method==8?0x806u:0x800u;
        if(field(0,4)!=0x04034b50u || field(4,2)>20 || (flags & ~allowed_flags) ||
           (method!=0 && method!=8) || (method==0 && encoded!=decoded) ||
           field(26,2)!=name.size() || field(28,2)!=0)
            invalid("unsupported ZIP member header");
        expect(name);
        if(encoded>size_-offset_)invalid("truncated ZIP member");
        return {{name,start,decoded,field(14,4)},encoded,method,header};
    }
    template <std::size_t N> void finish_exchange(const std::array<ExchangeMember,N>& members) {
        const auto start=zip_size(static_cast<std::size_t>(offset_));
        for(const auto& entry:members){
            const auto header=record<46>();
            if(read_little(header.data(),4)!=0x02014b50u ||
               !std::equal(entry.local.begin()+4,entry.local.end(),header.begin()+6) ||
               read_little(header.data()+32,4)!=0 ||
               read_little(header.data()+42,4)!=entry.member.offset)
                invalid("central directory differs from ZIP member");
            expect(entry.member.name);
        }
        const auto size=zip_size(static_cast<std::size_t>(offset_)-start);
        finish_directory(N,start,size);
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
    void finish_directory(unsigned count,std::uint32_t start,std::uint32_t size) {
        const auto end = end_record(count, start, size);
        expect({end.data(), end.size()});
        if (offset_ != size_)
            invalid("unexpected bytes after ZIP directory");
    }
};
} // namespace qpx::detail::stored_zip
