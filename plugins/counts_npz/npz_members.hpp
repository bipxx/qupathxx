// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "../format-common/binary_input.hpp"
#include "../image-common/gzip_member.hpp"
#include <array>

namespace qpx_npz {
using namespace qpx_binary;

struct Member {
    std::size_t index, file_offset, size, packed;
    std::uint32_t crc;
    std::uint16_t version, flags, method;
    std::uint32_t timestamp;
};

inline std::array<Bytes, 3> members(Bytes archive, std::pmr::memory_resource* memory,
                                    qpx_reader_cancel_v1 cancel, std::string_view matrix_name = "counts.npy") {
    const std::array<std::string_view, 3> member_names{matrix_name, "observation_ids.npy", "feature_ids.npy"};
    valid(archive.size >= 22, "Truncated NPZ archive end");
    const auto end = archive.sub(archive.size - 22, 22);
    valid(end.le32(0) == 0x06054b50 && !end.le32(4) && end.le16(8) == 3 && end.le16(10) == 3 && !end.le16(20),
          "NPZ requires exactly three arrays in one ZIP directory without a comment");
    const auto directory_at = end.le32(16), directory_size = end.le32(12);
    valid(std::uint64_t(directory_at) + directory_size == archive.size - 22, "NPZ directory extent differs");
    const auto directory = archive.sub(directory_at, directory_size);
    const auto payloads = archive.sub(0, directory_at);
    std::array<Member, 3> entries{};
    std::array<bool, 3> seen{};
    std::size_t position = 0;
    for (auto& entry : entries) {
        poll(cancel);
        const auto record = directory.sub(position, 46);
        const auto name = directory.sub(position + 46, record.le16(28)).view();
        const auto key = std::find(member_names.begin(), member_names.end(), name);
        valid(key != member_names.end(), "NPZ contains an unexpected array name");
        const auto index = std::size_t(key - member_names.begin());
        valid(!seen[index], "NPZ contains a duplicate array");
        seen[index] = true;
        valid(record.le32(0) == 0x02014b50 && !record.le16(30) && !record.le16(32) && !record.le16(34),
              "Unsupported NPZ central directory record");
        entry = {index, record.le32(42), record.le32(24), record.le32(20), record.le32(16),
                 record.le16(6), record.le16(8), record.le16(10), record.le32(12)};
        valid(entry.version <= 45 && !(entry.flags & ~0x0800u) && (entry.method == 0 || entry.method == 8) &&
              entry.size && entry.packed, "Unsupported NPZ member size, flags or compression");
        position += 46 + name.size();
    }
    valid(position == directory.size, "Trailing NPZ directory data");
    std::sort(entries.begin(), entries.end(), [](const Member& a, const Member& b) { return a.file_offset < b.file_offset; });
    std::array<Bytes, 3> result{};
    std::size_t local_end = 0;
    for (const auto& entry : entries) {
        poll(cancel);
        valid(entry.file_offset == local_end, "NPZ local records overlap or have a gap");
        const auto header = payloads.sub(local_end, 30);
        const auto name = payloads.sub(local_end + 30, header.le16(26)).view();
        const auto extra = payloads.sub(local_end + 30 + name.size(), header.le16(28));
        valid(header.le32(0) == 0x04034b50 && header.le16(4) == entry.version && header.le16(6) == entry.flags &&
              header.le16(8) == entry.method && header.le32(10) == entry.timestamp && header.le32(14) == entry.crc &&
              name == member_names[entry.index], "NPZ local and central records disagree");
        if (extra.size) {
            // numpy.savez forces a ZIP64 local size field even for small members.
            valid(extra.size == 20 && extra.le16(0) == 1 && extra.le16(2) == 16 &&
                  header.le32(18) == UINT32_MAX && header.le32(22) == UINT32_MAX &&
                  extra.le64(4) == entry.size && extra.le64(12) == entry.packed,
                  "Invalid NPZ ZIP64 local size field");
        } else {
            valid(header.le32(18) == entry.packed && header.le32(22) == entry.size, "NPZ member sizes disagree");
        }
        const auto data_at = local_end + 30 + name.size() + extra.size;
        const auto packed = payloads.sub(data_at, entry.packed);
        local_end = data_at + packed.size;
        auto unpacked = packed;
        if (entry.method == 8) {
            using namespace qpx::image_plugin;
            auto* bytes = static_cast<std::uint8_t*>(memory->allocate(entry.size, 1));
            const auto status = decode_deflate_member(const_cast<std::uint8_t*>(packed.data), packed.size,
                bytes, entry.size, DeflateWrapper::raw, [&] { poll(cancel); });
            if (status == GzipResult::workspace_failure) fail("NPZ decoder workspace allocation failed", QPX_READER_INTERNAL_ERROR);
            valid(status == GzipResult::ok, "NPZ deflate member is damaged or has the wrong extent");
            unpacked = {bytes, entry.size};
        } else {
            valid(entry.packed == entry.size, "NPZ stored member sizes disagree");
        }
        uLong crc = 0;
        for (std::size_t at = 0; at < unpacked.size;) {
            poll(cancel);
            const auto n = std::min<std::size_t>(65536, unpacked.size - at);
            crc = crc32(crc, unpacked.data + at, static_cast<uInt>(n));
            at += n;
        }
        valid(crc == entry.crc, "NPZ member checksum differs");
        result[entry.index] = unpacked;
    }
    valid(local_end == payloads.size, "Unexplained bytes before the NPZ directory");
    return result;
}
} // namespace qpx_npz
