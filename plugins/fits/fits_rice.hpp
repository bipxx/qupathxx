// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "../image-common/rgba8_reader.hpp"

namespace qpx::fits_plugin {

class RiceBits {
public:
    RiceBits(const std::uint8_t* data, std::size_t size) : data_(data), end_(size * 8) {}

    std::uint32_t read(unsigned count) {
        qpx_image_plugin::valid(count <= end_ - bit_, "Truncated FITS Rice bitstream");
        std::uint32_t value = 0;
        while (count) {
            const auto available = 8u - unsigned(bit_ % 8);
            const auto take = std::min(count, available);
            const auto piece = unsigned(data_[bit_ / 8]) >> (available - take);
            value = (value << take) | (piece & ((1u << take) - 1));
            count -= take;
            bit_ += take;
        }
        return value;
    }

    std::uint32_t unary(std::uint32_t limit, qpx_reader_cancel_v1 cancel) {
        std::uint32_t zeroes = 0;
        while (bit_ < end_) {
            const auto available = 8u - unsigned(bit_ % 8);
            const auto tail = unsigned(data_[bit_ / 8]) & ((1u << available) - 1);
            unsigned count = available;
            if (tail) {
                count = 0;
                for (auto mask = 1u << (available - 1); !(tail & mask); mask >>= 1)
                    ++count;
            }
            qpx_image_plugin::valid(count <= limit - zeroes, "FITS Rice quotient exceeds the sample domain");
            zeroes += count;
            bit_ += count;
            if (tail) {
                ++bit_;
                return zeroes;
            }
            if (bit_ % 32768 == 0)
                qpx_image_plugin::checkpoint(cancel);
        }
        qpx_image_plugin::fail(QPX_READER_IO_ERROR, "Truncated FITS Rice unary code");
    }

    void finish() {
        const auto remaining = end_ - bit_;
        qpx_image_plugin::valid(remaining < 8 && read(static_cast<unsigned>(remaining)) == 0,
                               "Invalid FITS Rice padding or trailing bytes");
    }

private:
    const std::uint8_t* data_;
    std::size_t end_, bit_ = 0;
};

// Return FITS big-endian words for the common integer normalization path.
inline void decode_rice(const std::uint8_t* source, std::size_t size, std::uint8_t* output,
                        std::size_t samples, unsigned bytes, unsigned block_size, qpx_reader_cancel_v1 cancel) {
    using qpx_image_plugin::checkpoint;
    using qpx_image_plugin::valid;
    const auto bits = bytes * 8;
    const auto header_bits = bytes == 1 ? 3u : bytes == 2 ? 4u : 5u;
    const auto direct = bytes == 1 ? 7u : bytes == 2 ? 15u : 26u;
    const auto mask = static_cast<std::uint32_t>((UINT64_C(1) << bits) - 1);
    RiceBits input(source, size);
    auto previous = input.read(bits);
    for (std::size_t at = 0; at < samples;) {
        checkpoint(cancel);
        const auto code = input.read(header_bits);
        valid(code <= direct, "Reserved FITS Rice block code");
        const auto stop = at + std::min<std::size_t>(block_size, samples - at);
        for (; at < stop; ++at) {
            if (at % 4096 == 0)
                checkpoint(cancel);
            std::uint32_t folded = 0;
            if (code == direct) {
                folded = input.read(bits);
            } else if (code) {
                const auto split = code - 1;
                const auto quotient = input.unary(mask >> split, cancel);
                folded = (quotient << split) | input.read(split);
            }
            valid(at != 0 || folded == 0, "FITS Rice first difference does not match the tile seed");
            const auto half = folded / 2;
            previous = ((folded & 1) ? previous - half - 1 : previous + half) & mask;
            for (unsigned byte = 0; byte < bytes; ++byte)
                *output++ = static_cast<std::uint8_t>(previous >> (8 * (bytes - 1 - byte)));
        }
    }
    input.finish();
}

} // namespace qpx::fits_plugin
