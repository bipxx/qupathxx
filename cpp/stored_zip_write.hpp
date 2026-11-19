// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "atomic_export.hpp"
#include "sha256.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace qpx::detail {
namespace stored_zip {
constexpr auto crc_table() {
    std::array<std::uint32_t, 256> result{};
    for (std::uint32_t i = 0; i < result.size(); ++i) {
        auto value = i;
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
        result[i] = value;
    }
    return result;
}
inline constexpr auto crc_values = crc_table();

inline std::uint32_t zip_size(std::size_t size) {
    if (size >= std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Stored ZIP exceeds its 32-bit extent");
    return static_cast<std::uint32_t>(size);
}

struct Checksum {
    const std::atomic<bool>* cancelled;
    std::uint32_t crc = 0xffffffffu;
    std::size_t bytes = 0;
    Sha256* digest = nullptr;

    void write(const char* data, std::size_t size) {
        for (std::size_t offset = 0; offset < size;) {
            export_checkpoint(cancelled);
            const auto count = std::min<std::size_t>(size - offset, 65536);
            for (std::size_t i = 0; i < count; ++i)
                crc = (crc >> 8) ^ crc_values[(crc ^ static_cast<unsigned char>(data[offset + i])) & 255];
            if (digest)
                digest->update(data + offset, count);
            offset += count;
        }
        bytes += size;
    }
};

inline void little(char* out, std::uint32_t value, unsigned size) {
    for (unsigned offset = 0; offset < size; ++offset)
        out[offset] = static_cast<char>(static_cast<unsigned char>(value >> (8 * offset)));
}

struct Member {
    std::string_view name;
    std::uint32_t offset, size, crc;
};
constexpr unsigned dos_date = 33; // 1980-01-01

inline std::array<char, 30> local_header(const Member& member) {
    std::array<char, 30> record{};
    auto* fields = record.data();
    little(fields, 0x04034b50u, 4);
    little(fields + 4, 10, 2);
    little(fields + 12, dos_date, 2);
    little(fields + 14, member.crc, 4);
    little(fields + 18, member.size, 4);
    little(fields + 22, member.size, 4);
    little(fields + 26, static_cast<std::uint32_t>(member.name.size()), 2);
    return record;
}

inline std::array<char, 46> central_header(const Member& member) {
    std::array<char, 46> record{};
    auto* fields = record.data();
    little(fields, 0x02014b50u, 4);
    little(fields + 4, 10, 2);
    little(fields + 6, 10, 2);
    little(fields + 14, dos_date, 2);
    little(fields + 16, member.crc, 4);
    little(fields + 20, member.size, 4);
    little(fields + 24, member.size, 4);
    little(fields + 28, static_cast<std::uint32_t>(member.name.size()), 2);
    little(fields + 42, member.offset, 4);
    return record;
}

inline std::array<char, 22> end_record(unsigned count, std::uint32_t start, std::uint32_t size) {
    std::array<char, 22> record{};
    auto* fields = record.data();
    little(fields, 0x06054b50u, 4);
    little(fields + 8, count, 2);
    little(fields + 10, count, 2);
    little(fields + 12, size, 4);
    little(fields + 16, start, 4);
    return record;
}

// The callers supply short, literal ASCII filenames and immutable payloads.
// Computing the CRC before writing avoids extra copies or a seekable sink.
template <class Write>
Member write_member(AtomicExportFile& file, std::string_view name,
                    const std::atomic<bool>* cancelled, Write write, Sha256* digest = nullptr) {
    Checksum checksum{cancelled};
    checksum.digest = digest;
    write(checksum);
    const Member member{name, zip_size(file.bytes()), zip_size(checksum.bytes), ~checksum.crc};
    const auto local = local_header(member);
    file.write(local.data(), local.size());
    file.write(name.data(), name.size());
    const auto payload = file.bytes();
    write(file);
    if (file.bytes() - payload != member.size)
        throw std::logic_error("Stored ZIP member changed size during write");
    return member;
}

template <std::size_t N>
void finish(AtomicExportFile& file, const std::array<Member, N>& members) {
    static_assert(N < 65535);
    const auto start = zip_size(file.bytes());
    for (const auto& member : members) {
        const auto central = central_header(member);
        file.write(central.data(), central.size());
        file.write(member.name.data(), member.name.size());
    }
    const auto end = end_record(N, start, zip_size(file.bytes() - start));
    file.write(end.data(), end.size());
}
} // namespace stored_zip
} // namespace qpx::detail
