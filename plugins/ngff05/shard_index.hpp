// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace qpx::detail {

struct ShardChunkRange {
    std::uint64_t offset = 0;
    std::uint64_t length = 0; // Missing inner chunk.
};

inline constexpr std::size_t shard_index_max_entries = 65536;
inline constexpr std::size_t shard_index_entry_bytes = 16;
inline constexpr std::size_t shard_index_checksum_bytes = 4;

struct ShardIndexEncoding {
    bool big_endian=false;
    bool crc32c=true;
};

// The caller validates the entry count against the configured shard shape.
inline constexpr std::size_t shard_index_encoded_bytes(std::size_t entries,ShardIndexEncoding encoding={}) {
    return entries * shard_index_entry_bytes + (encoding.crc32c?shard_index_checksum_bytes:0);
}

// Includes the encoded index, the output and the temporary overlap check.
inline constexpr std::size_t shard_index_workspace_bytes(std::size_t entries,ShardIndexEncoding encoding={}) {
    return shard_index_encoded_bytes(entries,encoding) + 2 * entries * sizeof(ShardChunkRange);
}

namespace shard_index {

template<class T>
T little_endian(const std::uint8_t* data) {
    T value = 0;
    for (std::size_t byte = 0; byte < sizeof(T); ++byte)
        value |= T(data[byte]) << (8 * byte);
    return value;
}

constexpr auto make_crc32c_table() {
    constexpr std::uint32_t polynomial = 0x82f63b78u; // Reflected Castagnoli.
    std::array<std::uint32_t, 256> table{};
    for (std::size_t byte = 0; byte < table.size(); ++byte) {
        auto value = static_cast<std::uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? polynomial : 0);
        table[byte] = value;
    }
    return table;
}

inline std::uint32_t update_crc32c(std::uint32_t value, const std::uint8_t* data, std::size_t bytes) {
    static constexpr auto table = make_crc32c_table();
    for (std::size_t byte = 0; byte < bytes; ++byte)
        value = (value >> 8) ^ table[(value ^ data[byte]) & 0xffu];
    return value;
}

inline std::uint32_t crc32c(const std::uint8_t* data, std::size_t bytes) {
    return ~update_crc32c(std::numeric_limits<std::uint32_t>::max(), data, bytes);
}

}

inline std::vector<ShardChunkRange> decode_shard_index(const std::vector<std::uint8_t>& index,
    std::uint64_t shard_bytes, bool index_at_start, std::size_t max_encoded_chunk,ShardIndexEncoding encoding={}) {
    if (index.size() < shard_index_encoded_bytes(1,encoding) ||
        index.size() > shard_index_encoded_bytes(shard_index_max_entries,encoding) ||
        index.size() > shard_bytes)
        throw std::invalid_argument("OME-Zarr: invalid shard index size");
    const auto pairs_bytes = index.size() - (encoding.crc32c?shard_index_checksum_bytes:0);
    if (pairs_bytes % shard_index_entry_bytes != 0)
        throw std::invalid_argument("OME-Zarr: invalid shard index entry width");
    if (encoding.crc32c && shard_index::crc32c(index.data(), pairs_bytes) !=
        shard_index::little_endian<std::uint32_t>(index.data() + pairs_bytes))
        throw std::invalid_argument("OME-Zarr: shard index CRC32C mismatch");

    const std::uint64_t data_begin = index_at_start ? index.size() : 0;
    const std::uint64_t data_end = index_at_start ? shard_bytes : shard_bytes - index.size();
    const auto entries = pairs_bytes / shard_index_entry_bytes;
    std::vector<ShardChunkRange> result(entries);
    std::vector<ShardChunkRange> ranges;
    ranges.reserve(entries);
    constexpr auto missing = std::numeric_limits<std::uint64_t>::max();
    const auto read=[encoding](const std::uint8_t* bytes) {
        if(!encoding.big_endian)return shard_index::little_endian<std::uint64_t>(bytes);
        std::uint64_t value=0;
        for(std::size_t i=0;i<sizeof(value);++i)value=(value<<8)|bytes[i];
        return value;
    };
    for (std::size_t ordinal = 0; ordinal < entries; ++ordinal) {
        const auto* pair = index.data() + ordinal * shard_index_entry_bytes;
        const auto offset = read(pair);
        const auto length = read(pair + sizeof(offset));
        if (offset == missing && length == missing)
            continue;
        if (offset == missing || length == missing || length == 0 || length > max_encoded_chunk ||
            offset < data_begin || offset > data_end || length > data_end - offset)
            throw std::invalid_argument("OME-Zarr: invalid encoded inner chunk range");
        result[ordinal] = {offset, length};
        ranges.push_back(result[ordinal]);
    }

    std::sort(ranges.begin(), ranges.end(),
        [](const ShardChunkRange& left, const ShardChunkRange& right) { return left.offset < right.offset; });
    auto previous_end = data_begin;
    for (const auto& range : ranges) {
        if (range.offset < previous_end)
            throw std::invalid_argument("OME-Zarr: overlapping encoded inner chunk ranges");
        previous_end = range.offset + range.length;
    }
    return result;
}

}
