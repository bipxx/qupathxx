// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/rgba8_reader.hpp"

namespace {
using namespace qpx_image_plugin;

unsigned word(const std::uint8_t* bytes) {
    return unsigned(bytes[0]) | (unsigned(bytes[1]) << 8);
}

struct PcxRaster {
    std::int64_t width, height;
    bool indexed;
    const char* vendor = "PCX";
    std::array<std::uint8_t, 768> palette{};
    std::unique_ptr<std::uint8_t[]> raw;

    unsigned channels() const { return indexed ? 1 : 3; }
    static constexpr unsigned sample_bytes() { return 1; }
    static constexpr std::uint32_t sample_type() { return QPX_READER_UINT8; }
    const char* channel_name(unsigned channel) const {
        return indexed ? "Palette index" : Rgba8Raster::channel_name(channel);
    }
    void rgba(const std::uint8_t* source, std::uint8_t* output) const {
        const auto* color = indexed ? palette.data() + 3 * source[0] : source;
        std::memcpy(output, color, 3);
        output[3] = 255;
    }
};

void decode_row(const std::uint8_t*& cursor, const std::uint8_t* end,
                std::uint8_t* row, std::size_t row_size, qpx_reader_cancel_v1 cancel) {
    for (std::size_t at = 0, next_poll = 0; at < row_size;) {
        if (at >= next_poll) {
            checkpoint(cancel);
            next_poll = at + 4096;
        }
        valid(cursor != end, "PCX scanline is truncated");
        auto value = *cursor++;
        unsigned count = 1;
        if ((value & 0xc0) == 0xc0) {
            count = value & 63;
            valid(count && cursor != end, "PCX run count or sample is missing");
            value = *cursor++;
        }
        valid(count <= row_size - at, "PCX run crosses the scanline boundary");
        std::fill_n(row + at, count, value);
        at += count;
    }
}

struct Pcx {
    static constexpr const char* provider_id = "org.qupathxx.pcx";
    static constexpr const char* name = "PCX";
    static constexpr const char* version = QPX_PCX_VERSION;
    static constexpr const char* contract = "pcx-native-samples/1";

    static PcxRaster decode(const std::uint8_t* input, std::size_t size, Budget& budget,
                            qpx_reader_cancel_v1 cancel) {
        constexpr std::size_t header_size = 128;
        valid(size >= header_size, "PCX header is truncated");
        if (input[0] != 10 || input[2] != 1)
            fail(QPX_READER_UNSUPPORTED, "PCX requires the ZSoft signature and RLE");
        const unsigned file_version = input[1], bits = input[3];
        const unsigned planes = input[65];
        const auto palette_hint = word(input + 68);
        const bool ega = bits == 1 && planes == 4 && (file_version == 2 || file_version == 5);
        const bool eight_bit = file_version == 5 && bits == 8 && (planes == 1 || planes == 3);
        if (!ega && !eight_bit)
            fail(QPX_READER_UNSUPPORTED, "PCX version or plane layout is unsupported");
        if (input[64] || (palette_hint != 1 && palette_hint != 2 &&
                          !(ega && file_version == 2 && !palette_hint)))
            fail(QPX_READER_UNSUPPORTED, "PCX palette interpretation is unsupported");
        const auto left = word(input + 4), top = word(input + 6);
        const auto right = word(input + 8), bottom = word(input + 10);
        valid(left <= right && top <= bottom, "PCX raster bounds are reversed");
        const auto width = right - left + 1, height = bottom - top + 1;
        const auto stride = word(input + 66);
        const auto visible_plane_bytes = ega ? (width + 7) / 8 : width;
        valid(stride >= visible_plane_bytes && stride % 2 == 0,
              "PCX requires an even plane stride covering the width");
        PcxRaster raster{width, height, ega || planes == 1, "PCX", {}, {}};
        const auto* end = input + size;
        if (ega) {
            std::memcpy(raster.palette.data(), input + 16, 16 * 3);
        } else if (planes == 1) {
            const auto footer_size = 1 + raster.palette.size();
            valid(size >= header_size + footer_size && *(end - footer_size) == 12,
                  "PCX indexed raster requires a 256-color palette trailer");
            end -= footer_size;
            std::memcpy(raster.palette.data(), end + 1, raster.palette.size());
        }
        const auto row_size = std::size_t(stride) * planes;
        require(row_size * height <= work_limit, "PCX padded raster exceeds the reader work limit");
        auto* cursor = input + header_size;
        const auto minimum = std::uint64_t((row_size + 62) / 63) * height;
        valid(static_cast<std::uint64_t>(end - cursor) >= minimum, "PCX data cannot fill the padded raster");
        auto row = budget.array(row_size);
        raster.raw = budget.array(std::uint64_t(width) * height * raster.channels());
        auto* output = raster.raw.get();
        for (unsigned y = 0; y < height; ++y) {
            decode_row(cursor, end, row.get(), row_size, cancel);
            if (ega) {
                for (unsigned x = 0; x < width; ++x) {
                    unsigned code = 0;
                    const auto shift = 7 - x % 8;
                    for (unsigned plane = 0; plane < 4; ++plane)
                        code |= ((row[plane * stride + x / 8] >> shift) & 1u) << plane;
                    *output++ = static_cast<std::uint8_t>(code);
                }
            } else if (planes == 1) {
                std::memcpy(output, row.get(), width);
                output += width;
            } else {
                for (unsigned x = 0; x < width; ++x) {
                    *output++ = row[x];
                    *output++ = row[stride + x];
                    *output++ = row[2 * stride + x];
                }
            }
        }
        valid(cursor == end, "PCX data remains after the last scanline");
        return raster;
    }
};
} // namespace

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return PackedImageReader<Pcx, PcxRaster>::entry(abi, out, error);
}
