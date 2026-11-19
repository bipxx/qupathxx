// SPDX-License-Identifier: AGPL-3.0-only
#include "../shard_index.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
using namespace qpx::detail;

void check(bool result, const char* message) {
    if (!result)
        throw std::runtime_error(message);
}

std::vector<std::uint8_t> bytes(std::string_view hex) {
    const auto digit = [](char value) { return value <= '9' ? value - '0' : value - 'a' + 10; };
    std::vector<std::uint8_t> result;
    for (std::size_t i = 0; i < hex.size(); i += 2)
        result.push_back(static_cast<std::uint8_t>(16 * digit(hex[i]) + digit(hex[i + 1])));
    return result;
}

void refuses(const std::vector<std::uint8_t>& index, std::uint64_t shard_bytes,
             bool starts_at_zero = false, std::size_t chunk_limit = 64) {
    try {
        (void)decode_shard_index(index, shard_bytes, starts_at_zero, chunk_limit);
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("Malformed shard index accepted");
}

void selected_index_tests() {
    // Literal vectors with independent CRC32C checksums from google-crc32c.
    const auto end = bytes("47000000000000000a00000000000000ffffffffffffffffffffffffffffffff"
                           "05000000000000000c000000000000001ac7dc31");
    const auto start = bytes("47000000000000000a00000000000000ffffffffffffffffffffffffffffffff"
                             "36000000000000000c0000000000000028de4e18");
    const auto after_data = decode_shard_index(end, 133, false, 12);
    const auto before_data = decode_shard_index(start, 81, true, 12);
    check(after_data.size() == 3 && after_data[0].offset == 71 && after_data[0].length == 10 &&
          after_data[1].length == 0 && after_data[2].offset == 5 && after_data[2].length == 12,
          "End index reordered entries, rejected a boundary or lost fill semantics");
    check(before_data.size() == 3 && before_data[0].offset == 71 && before_data[0].length == 10 &&
          before_data[1].length == 0 && before_data[2].offset == 54 && before_data[2].length == 12,
          "Start index did not retain absolute offsets");
    refuses(end, 132); // One byte into index.
    refuses(end, 133, false, 11);
    refuses(end, 133, true);
    refuses(start, 80, true);
    refuses(bytes("05000000000000000c0000000000000010000000000000000a00000000000000f92d0da5"), 80);
    refuses(bytes("05000000000000000000000000000000ff3b7876"), 80);
    refuses(bytes("ffffffffffffffff00000000000000005db5602b"), 80);
    refuses(bytes("f5ffffffffffffff280000000000000034964a0c"), UINT64_MAX, true);
    auto corrupt = end;
    corrupt[0] ^= 1;
    refuses(corrupt, 133);
    corrupt.pop_back();
    refuses(corrupt, 133);
    refuses(std::vector<std::uint8_t>(shard_index_encoded_bytes(shard_index_max_entries + 1)), UINT64_MAX);
}

void producer_index(const std::filesystem::path& store) {
    const auto member = store / "s0/c/0/0/0/0/0";
    const auto shard_bytes = std::filesystem::file_size(member);
    std::vector<std::uint8_t> index(shard_index_encoded_bytes(8));
    std::ifstream source(member, std::ios::binary);
    source.seekg(-static_cast<std::streamoff>(index.size()), std::ios::end);
    check(static_cast<bool>(source.read(reinterpret_cast<char*>(index.data()), index.size())),
          "Cannot read the upstream shard tail");
    const auto entries = decode_shard_index(index, shard_bytes, false, 64);
    check(entries.size() == 8 && entries[0].length == 0, "Producer omitted chunk was not fill");
    // Writer execution can choose a different physical ordering. The index must
    // still refer to seven separate, nonempty Zstandard chunk payloads.
    check(std::all_of(entries.begin() + 1, entries.end(),
        [](const ShardChunkRange& range) { return range.length > 0; }),
        "Producer present entries were not retained");
}
}

int main(int argc, char** argv) {
    try {
        selected_index_tests();
        if (argc == 2)
            producer_index(std::filesystem::u8path(argv[1]));
        else
            check(argc == 1, "Usage: shard_index_tests [PRODUCER_STORE]");
        std::cout << "NGFF 0.5 shard index checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
