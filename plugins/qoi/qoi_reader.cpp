// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/rgba8_reader.hpp"

namespace {
using namespace qpx_image_plugin;
std::uint32_t big_endian(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) |
           p[3];
}
struct Qoi {
    static constexpr const char* provider_id = "org.qupathxx.qoi";
    static constexpr const char* name = "QOI";
    static constexpr const char* version = "1.0.0";
    static constexpr const char* contract = "qoi-rgba8-native-samples/1";
    static Rgba8Raster decode(const std::uint8_t* input, std::size_t size, Budget& budget,
                              qpx_reader_cancel_v1 cancel) {
        checkpoint(cancel);
        if (size < 4 || std::memcmp(input, "qoif", 4))
            fail(QPX_READER_UNSUPPORTED, "Input is not QOI");
        valid(size >= 22, "QOI header/end marker is truncated");
        const auto width = big_endian(input + 4), height = big_endian(input + 8);
        valid(width && height, "QOI dimensions must be positive");
        if (input[12] != 3 && input[12] != 4)
            fail(QPX_READER_UNSUPPORTED, "QOI channel hint must be3 or4");
        if (input[13] > 1)
            fail(QPX_READER_UNSUPPORTED, "QOI colorspace is unsupported");
        constexpr std::uint8_t end[] = {0, 0, 0, 0, 0, 0, 0, 1};
        valid(!std::memcmp(input + size - sizeof(end), end, sizeof(end)),
              "QOI end marker is invalid");
        const auto pixels = std::uint64_t(width) * height;
        require(pixels <= work_limit / 4, "QOI raster exceeds reader work budget");
        Rgba8Raster result;
        result.width = width;
        result.height = height;
        result.vendor = input[13] ? (input[12] == 3 ? "QOI-linear-RGB" : "QOI-linear-RGBA")
                                  : (input[12] == 3 ? "QOI-sRGB-RGB" : "QOI-sRGB-RGBA");
        result.raw = budget.array(pixels * 4);
        result.display_transfer(input[13] == 1);
        std::array<std::array<std::uint8_t, 4>, 64> index{};
        std::array<std::uint8_t, 4> previous{0, 0, 0, 255};
        std::size_t at = 14;
        const auto last = size - sizeof(end);
        std::uint64_t written = 0;
        int previous_index = -1;
        const auto byte = [&]() {
            valid(at < last, "QOI chunk/pixel stream is truncated");
            return input[at++];
        };
        while (written < pixels) {
            checkpoint(cancel);
            const auto tag = byte();
            unsigned run = 1;
            if (tag == 0xfe) {
                for (unsigned c = 0; c < 3; ++c)
                    previous[c] = byte();
            } else if (tag == 0xff) {
                for (auto& c : previous)
                    c = byte();
            } else if ((tag & 0xc0) == 0) {
                valid(previous_index != int(tag),
                      "QOI repeats a consecutive index instead of a run");
                previous = index[tag];
            } else if ((tag & 0xc0) == 0x40) {
                previous[0] = std::uint8_t(previous[0] + int((tag >> 4) & 3) - 2);
                previous[1] = std::uint8_t(previous[1] + int((tag >> 2) & 3) - 2);
                previous[2] = std::uint8_t(previous[2] + int(tag & 3) - 2);
            } else if ((tag & 0xc0) == 0x80) {
                const auto other = byte();
                const int green = int(tag & 63) - 32;
                previous[0] = std::uint8_t(previous[0] + green + int(other >> 4) - 8);
                previous[1] = std::uint8_t(previous[1] + green);
                previous[2] = std::uint8_t(previous[2] + green + int(other & 15) - 8);
            } else {
                run = (tag & 63) + 1;
            }
            previous_index = tag < 64 ? int(tag) : -1;
            valid(run <= pixels - written, "QOI run exceeds the declared raster");
            const auto hash = (unsigned(previous[0]) * 3 + unsigned(previous[1]) * 5 +
                               unsigned(previous[2]) * 7 + unsigned(previous[3]) * 11) %
                              64;
            index[hash] = previous;
            for (unsigned i = 0; i < run; ++i, ++written)
                std::memcpy(result.raw.get() + written * 4, previous.data(), 4);
        }
        valid(at == last, "QOI has extra chunks or trailing image data");
        checkpoint(cancel);
        return result;
    }
};
} // namespace
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return qpx_image_plugin::Rgba8Reader<Qoi>::entry(abi, out, error);
}
