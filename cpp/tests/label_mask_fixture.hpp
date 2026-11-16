#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace qpx_test {
// Independent little-endian scalar UInt32 TIFF, with no encoder dependency.
inline void label_mask_fixture(const std::filesystem::path& path,
                               const std::vector<std::uint32_t>& labels) {
    if (labels.size() != 4)
        throw std::invalid_argument("Independent mask writer expects2x2");
    std::vector<std::uint8_t> b(176, 0);
    const auto u16 = [&](std::size_t at, std::uint16_t v) {
        b[at] = std::uint8_t(v);
        b[at + 1] = std::uint8_t(v >> 8);
    };
    const auto u32 = [&](std::size_t at, std::uint32_t v) {
        for (unsigned i = 0; i < 4; ++i)
            b[at + i] = std::uint8_t(v >> (8 * i));
    };
    b[0] = b[1] = 'I';
    u16(2, 42);
    u32(4, 8);
    u16(8, 11);
    const std::uint32_t tags[][4] = {{256, 4, 1, 2}, {257, 4, 1, 2}, {258, 3, 1, 32},
                                     {259, 3, 1, 1}, {262, 3, 1, 1}, {273, 4, 1, 160},
                                     {277, 3, 1, 1}, {278, 4, 1, 2}, {279, 4, 1, 16},
                                     {284, 3, 1, 1}, {339, 3, 1, 1}};
    for (unsigned i = 0; i < 11; ++i) {
        const auto at = 10 + 12 * i;
        u16(at, std::uint16_t(tags[i][0]));
        u16(at + 2, std::uint16_t(tags[i][1]));
        u32(at + 4, tags[i][2]);
        u32(at + 8, tags[i][3]);
    }
    for (unsigned i = 0; i < 4; ++i)
        u32(160 + 4 * i, labels[i]);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()), b.size());
    out.close();
    if (!out)
        throw std::runtime_error("Could not author independent mask TIFF");
}
} // namespace qpx_test
