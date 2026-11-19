// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include "../webp/webp_memory_backend.h"
#include <charconv>
#include <mutex>
#include <webp/decode.h>

namespace {
using namespace qpx_image_plugin;
constexpr std::uint32_t dimension_limit = 2048, frame_limit = 256;
constexpr std::uint64_t pixel_limit = UINT64_C(64) * 1024 * 1024;
constexpr std::uint8_t alpha_flag = 0x10, animation_flag = 0x02;
std::mutex codec_gate;

std::uint32_t little(const std::uint8_t* input, unsigned size) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < size; ++i)
        value |= std::uint32_t(input[i]) << (8 * i);
    return value;
}

void put_little(std::uint8_t* out, std::uint32_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i)
        out[i] = static_cast<std::uint8_t>(value >> (8 * i));
}

struct Chunk {
    std::string_view tag;
    const std::uint8_t* data;
    std::size_t size;
};

struct Chunks {
    const std::uint8_t* next;
    std::size_t remaining;

    Chunk take() {
        valid(remaining >= 8, "Truncated animated WebP chunk header");
        const auto size = std::size_t(little(next + 4, 4));
        const auto padded = size + (size & 1);
        valid(padded <= remaining - 8, "Invalid animated WebP chunk extent");
        valid(size == padded || !next[8 + size], "Animated WebP chunk padding is nonzero");
        const Chunk result{{reinterpret_cast<const char*>(next), 4}, next + 8, size};
        next += 8 + padded;
        remaining -= 8 + padded;
        return result;
    }
};

unsigned frame_option(std::string_view options) {
    constexpr std::string_view prefix = R"({"frame":)";
    require(options.size() > prefix.size() + 1 && options.substr(0, prefix.size()) == prefix && options.back() == '}',
            "Animated WebP requires an explicit integer frame option");
    options.remove_prefix(prefix.size());
    options.remove_suffix(1);
    unsigned index = 0;
    const auto end = options.data() + options.size();
    const auto decoded = std::from_chars(options.data(), end, index);
    require(decoded.ec == std::errc{} && decoded.ptr == end && index < frame_limit,
            "Animated WebP frame option is outside the supported range");
    return index;
}

struct Frame {
    std::uint32_t x, y, width, height;
    bool replace, dispose;
};

Frame parse_frame(const Chunk& chunk, unsigned width, unsigned height) {
    valid(chunk.tag == "ANMF" && chunk.size >= 24, "Animated WebP requires ANMF frame chunks");
    const auto* data = chunk.data;
    const auto flags = data[15];
    const Frame frame{2 * little(data, 3), 2 * little(data + 3, 3), 1 + little(data + 6, 3),
                      1 + little(data + 9, 3), bool(flags & 2), bool(flags & 1)};
    valid(!(flags & 0xfc) && frame.width <= width && frame.height <= height &&
          frame.x <= width - frame.width && frame.y <= height - frame.height,
          "Animated WebP frame or flags are outside the canvas profile");
    return frame;
}

void decode_frame(const Chunk& part, const Frame& frame, bool alpha_declared, std::uint8_t* output,
                  std::uint8_t* still, std::uint64_t budget, qpx_reader_cancel_v1 cancel) {
    Chunks chunks{part.data + 16, part.size - 16};
    auto bitstream = chunks.take();
    const bool alpha = bitstream.tag == "ALPH";
    if (alpha) {
        const auto count = bitstream.size;
        valid(alpha_declared && count > 1 && !(bitstream.data[0] & 0xc0) && (bitstream.data[0] & 3) <= 1 &&
              ((bitstream.data[0] >> 4) & 3) <= 1, "Invalid animated WebP alpha header");
        if (!(bitstream.data[0] & 3))
            valid(count - 1 == std::size_t(frame.width) * frame.height, "Invalid animated WebP raw alpha extent");
        bitstream = chunks.take();
    }
    const bool lossless = bitstream.tag == "VP8L";
    valid(!chunks.remaining && (bitstream.tag == "VP8 " || (lossless && !alpha)),
          "Invalid animated WebP frame payload layout");
    std::memcpy(still, "RIFF\0\0\0\0WEBP", 12);
    std::size_t prefix = 12;
    if (alpha) {
        std::memcpy(still + 12, "VP8X\x0a\0\0\0\x10\0\0\0", 12);
        put_little(still + 24, frame.width - 1, 3);
        put_little(still + 27, frame.height - 1, 3);
        prefix = 30;
    }
    const auto length = prefix + part.size - 16;
    std::memcpy(still + prefix, part.data + 16, part.size - 16);
    put_little(still + 4, static_cast<std::uint32_t>(length - 8), 4);

    qpx_reader_status_v1 status;
    {
        std::lock_guard<std::mutex> lock(codec_gate);
        checkpoint(cancel);
        WebPBitstreamFeatures info{};
        valid(WebPGetFeatures(bitstream.data, bitstream.size, &info) == VP8_STATUS_OK &&
              info.width == static_cast<int>(frame.width) && info.height == static_cast<int>(frame.height) &&
              info.format == (lossless ? 2 : 1), "Animated WebP rectangle differs from the encoded frame");
        status = qpx_webp_decode_rgba(still, length, output, std::size_t(frame.width) * frame.height * 4,
                                     info.width, info.height, budget, cancel);
    }
    if (status != QPX_READER_OK) fail(status, "Animated WebP frame decode failed or exceeded its budget");
}

class Blend {
public:
    Blend() {
        for (unsigned i = 0; i < linear_.size(); ++i) {
            const auto sample = double(i) / 255;
            linear_[i] = sample <= 0.04045 ? sample / 12.92 : std::pow((sample + 0.055) / 1.055, 2.4);
        }
    }
    void over(const std::uint8_t* source, std::uint8_t* dest) const {
        const unsigned src_alpha = source[3], dst_alpha = dest[3];
        if (!src_alpha) {
            if (!dst_alpha) std::memset(dest, 0, 4);
            return;
        }
        if (src_alpha == 255 || !dst_alpha) {
            std::memcpy(dest, source, 4);
            return;
        }
        const auto source_weight = src_alpha * 255, dest_weight = dst_alpha * (255 - src_alpha);
        for (unsigned i = 0; i < 3; ++i) {
            const double value = (source_weight * linear_[source[i]] + dest_weight * linear_[dest[i]]) /
                                 (source_weight + dest_weight);
            const double srgb = value <= 0.0031308 ? 12.92 * value : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
            dest[i] = static_cast<std::uint8_t>(std::clamp(std::floor(srgb * 255 + 0.5), 0.0, 255.0));
        }
        dest[3] = static_cast<std::uint8_t>((source_weight + dest_weight + 127) / 255);
    }

private:
    std::array<double, 256> linear_{};
};

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.webp-animation";
    static constexpr const char* name = "Animated WebP";
    static constexpr const char* version = QPX_WEBP_ANIMATION_VERSION;
    static constexpr const char* contract = "webp-rendered-frame/1";

    static Rgba8Raster decode(const std::uint8_t* input, std::size_t size, Budget& budget,
                              qpx_reader_cancel_v1 cancel, std::string_view options) {
        const auto selected = frame_option(options);
        valid(size >= 12 && !std::memcmp(input, "RIFF", 4) && !std::memcmp(input + 8, "WEBP", 4) &&
              std::uint64_t(little(input + 4, 4)) + 8 == size, "Invalid animated WebP RIFF header or extent");
        Chunks chunks{input + 12, size - 12};
        const auto extended = chunks.take();
        valid(extended.tag == "VP8X" && extended.size == 10 && (extended.data[0] & animation_flag) &&
              !(extended.data[0] & ~(alpha_flag | animation_flag)) && !little(extended.data + 1, 3),
              "Animated WebP requires the animation header without ICC, EXIF or XMP");
        const bool alpha = (extended.data[0] & alpha_flag) != 0;
        const auto width = 1 + little(extended.data + 4, 3), height = 1 + little(extended.data + 7, 3);
        require(width <= dimension_limit && height <= dimension_limit, "Animated WebP canvas exceeds its dimension limit");
        const auto animation = chunks.take();
        valid(animation.tag == "ANIM" && animation.size == 6, "Invalid animated WebP ANIM record");
        const std::array<std::uint8_t, 4> background{animation.data[2], animation.data[1], animation.data[0], animation.data[3]};
        const auto pixels = std::size_t(width) * height;
        auto still = budget.array(size + 30);
        auto decoded = budget.array(pixels * 4);
        auto canvas = budget.array(pixels * 4);
        Rgba8Raster raster;
        raster.width = width;
        raster.height = height;
        raster.vendor = "WebP (rendered animation frame)";
        raster.raw = budget.array(pixels * 4);
        raster.display_transfer(false);
        for (std::size_t at = 0; at < pixels; ++at)
            std::memcpy(canvas.get() + 4 * at, background.data(), 4);
        const Blend blend;
        std::uint64_t pixels_decoded = 0;
        unsigned index = 0;
        for (; chunks.remaining; ++index) {
            checkpoint(cancel);
            require(index < frame_limit, "Animated WebP has too many frames");
            const auto chunk = chunks.take();
            const auto frame = parse_frame(chunk, width, height);
            pixels_decoded += std::uint64_t(frame.width) * frame.height;
            require(pixels_decoded <= pixel_limit, "Animated WebP exceeds its decoded pixel limit");
            decode_frame(chunk, frame, alpha, decoded.get(), still.get(), budget.remaining, cancel);
            for (unsigned row = 0; row < frame.height; ++row) {
                checkpoint(cancel);
                auto* dest = canvas.get() + (std::size_t(frame.y + row) * width + frame.x) * 4;
                const auto* src = decoded.get() + std::size_t(row) * frame.width * 4;
                for (unsigned x = 0; x < frame.width; ++x) {
                    valid(alpha || src[4 * x + 3] == 255, "Animated WebP alpha differs from the canvas flags");
                    if (frame.replace)
                        std::memcpy(dest + 4 * x, src + 4 * x, 4);
                    else
                        blend.over(src + 4 * x, dest + 4 * x);
                }
            }
            if (index == selected) std::memcpy(raster.raw.get(), canvas.get(), pixels * 4);
            if (frame.dispose) {
                for (unsigned row = 0; row < frame.height; ++row) {
                    auto* dest = canvas.get() + (std::size_t(frame.y + row) * width + frame.x) * 4;
                    for (unsigned x = 0; x < frame.width; ++x)
                        std::memcpy(dest + 4 * x, background.data(), 4);
                }
            }
        }
        require(index > selected, "Animated WebP frame index is outside the animation");
        return raster;
    }
};
using Reader = PackedImageReader<Codec, Rgba8Raster, false, false, true>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}
