// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include <charconv>
#include <png.h>
#include <zlib.h>

namespace {
using namespace qpx_image_plugin;
constexpr std::array<std::uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
constexpr unsigned dimension_limit = 2048, frame_limit = 256;
constexpr std::uint64_t decoded_pixel_limit = UINT64_C(64) * 1024 * 1024;
constexpr std::size_t frame_png_overhead = 57;

std::uint32_t read_be(const std::uint8_t* value) {
    return (std::uint32_t(value[0]) << 24) | (std::uint32_t(value[1]) << 16) |
           (std::uint32_t(value[2]) << 8) | value[3];
}

void write_be(std::uint8_t* dest, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        dest[i] = static_cast<std::uint8_t>(value >> (24 - 8 * i));
}

std::uint32_t chunk_crc(const std::uint8_t* data, std::uint32_t length) {
    return static_cast<std::uint32_t>(crc32(0, data, length + 4));
}

void write_chunk(std::uint8_t* dest, const char* name, std::uint32_t length) {
    write_be(dest, length);
    std::memcpy(dest + 4, name, 4);
    write_be(dest + 8 + length, chunk_crc(dest + 4, length));
}

unsigned frame_option(std::string_view options) {
    constexpr std::string_view prefix = R"({"frame":)";
    require(options.size() > prefix.size() + 1 && options.substr(0, prefix.size()) == prefix && options.back() == '}',
            "APNG requires an explicit nonnegative integer frame option");
    options.remove_prefix(prefix.size());
    options.remove_suffix(1);
    unsigned index = 0;
    const auto end = options.data() + options.size();
    const auto parsed = std::from_chars(options.data(), end, index);
    require(parsed.ec == std::errc{} && parsed.ptr == end && index < frame_limit,
            "APNG frame option is outside the supported range");
    return index;
}

struct Frame {
    std::uint32_t width = 0, height = 0, x = 0, y = 0;
    std::uint8_t dispose = 0, blend = 0;
};

void decode_frame(std::uint8_t* png, std::size_t compressed, const Frame& frame, std::uint8_t color,
                  std::uint8_t* pixels) {
    std::copy(signature.begin(), signature.end(), png);
    auto* ihdr = png + 8;
    write_be(ihdr + 8, frame.width);
    write_be(ihdr + 12, frame.height);
    ihdr[16] = 8;
    ihdr[17] = color;
    std::fill_n(ihdr + 18, 3, std::uint8_t(0));
    write_chunk(ihdr, "IHDR", 13);
    auto* idat = ihdr + 25;
    write_chunk(idat, "IDAT", static_cast<std::uint32_t>(compressed));
    write_chunk(idat + 12 + compressed, "IEND", 0);

    struct Image {
        png_image handle{};
        ~Image() { png_image_free(&handle); }
    } image;
    image.handle.version = PNG_IMAGE_VERSION;
    valid(png_image_begin_read_from_memory(&image.handle, png, frame_png_overhead + compressed) != 0,
          "Invalid APNG frame header");
    image.handle.format = PNG_FORMAT_RGBA;
    valid(png_image_finish_read(&image.handle, nullptr, pixels, 0, nullptr) != 0 && !image.handle.warning_or_error,
          "Invalid or damaged APNG frame image data");
}

struct LinearSrgb {
    std::array<double, 256> linear{};
    LinearSrgb() {
        for (unsigned code = 0; code < linear.size(); ++code) {
            const auto v = double(code) / 255;
            linear[code] = v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
        }
    }
    static std::uint8_t encode(double v) {
        const auto encoded = v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
        return static_cast<std::uint8_t>(std::clamp(std::floor(encoded * 255 + 0.5), 0.0, 255.0));
    }
    void over(const std::uint8_t* source, std::uint8_t* target) const {
        const unsigned a = source[3], b = target[3];
        if (!a) return;
        if (a == 255 || !b) {
            std::memcpy(target, source, 4);
            return;
        }
        const auto foreground = a * 255, background = b * (255 - a);
        for (unsigned c = 0; c < 3; ++c)
            target[c] = encode((foreground * linear[source[c]] + background * linear[target[c]]) /
                               (foreground + background));
        target[3] = static_cast<std::uint8_t>((foreground + background + 127) / 255);
    }
};

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.apng";
    static constexpr const char* name = "APNG";
    static constexpr const char* version = QPX_APNG_VERSION;
    static constexpr const char* contract = "apng-rendered-frame/1";

    static Rgba8Raster decode(std::uint8_t* input, std::size_t size, Budget& budget, qpx_reader_cancel_v1 cancel,
                              std::string_view options) {
        const auto selected = frame_option(options);
        valid(size >= 45 && std::equal(signature.begin(), signature.end(), input) && read_be(input + 8) == 13 &&
              std::memcmp(input + 12, "IHDR", 4) == 0 && read_be(input + 29) == chunk_crc(input + 12, 13),
              "Invalid APNG signature or canvas header");
        const auto width = read_be(input + 16), height = read_be(input + 20);
        valid(width && height && width <= dimension_limit && height <= dimension_limit,
              "APNG canvas exceeds the bounded frame profile");
        const auto color = input[25];
        valid(input[24] == 8 && (color == 2 || color == 6) && !input[26] && !input[27] && !input[28],
              "APNG requires non-interlaced RGB8 or RGBA8");
        const auto canvas_bytes = std::size_t(width) * height * 4;
        auto frame_png = budget.array(size + frame_png_overhead);
        auto frame_pixels = budget.array(canvas_bytes);
        auto canvas = budget.array(canvas_bytes);
        auto previous = budget.array(canvas_bytes);
        Rgba8Raster raster;
        raster.vendor = "APNG (rendered frame)";
        raster.width = width;
        raster.height = height;
        raster.raw = budget.array(canvas_bytes);
        raster.display_transfer(false);
        std::fill_n(canvas.get(), canvas_bytes, std::uint8_t(0));
        const LinearSrgb srgb;

        Frame frame{width, height};
        std::size_t compressed = 0;
        std::uint32_t frames = 0, current = 0, sequence = 0;
        std::uint64_t decoded_pixels = 0;
        bool controlled = false, frame_uses_idat = false, have_idat = false, idat_closed = false;
        bool have_srgb = false, saved = false;
        const auto append = [&](const std::uint8_t* source, std::uint32_t length) {
            valid(length <= size - compressed, "APNG frame data exceeds its encoded extent");
            std::memcpy(frame_png.get() + 41 + compressed, source, length);
            compressed += length;
        };
        const auto finish = [&] {
            valid(compressed > 0, "APNG frame is missing image data");
            decoded_pixels += std::uint64_t(frame.width) * frame.height;
            require(decoded_pixels <= decoded_pixel_limit, "APNG decoded frames exceed the pixel limit");
            checkpoint(cancel);
            decode_frame(frame_png.get(), compressed, frame, color, frame_pixels.get());
            compressed = 0;
            if (!controlled) return; // A separate fallback PNG starts no animation frame.
            const auto row_bytes = std::size_t(frame.width) * 4;
            const bool restore = frame.dispose == 2 && current > 1;
            for (unsigned y = 0; y < frame.height; ++y) {
                checkpoint(cancel);
                auto* dest = canvas.get() + (std::size_t(frame.y + y) * width + frame.x) * 4;
                const auto* src = frame_pixels.get() + y * row_bytes;
                if (restore)
                    std::memcpy(previous.get() + y * row_bytes, dest, row_bytes);
                if (!frame.blend)
                    std::memcpy(dest, src, row_bytes);
                else
                    for (unsigned x = 0; x < frame.width; ++x)
                        srgb.over(src + x * 4, dest + x * 4);
            }
            if (current - 1 == selected) {
                std::memcpy(raster.raw.get(), canvas.get(), canvas_bytes);
                saved = true;
            }
            if (frame.dispose) {
                for (unsigned y = 0; y < frame.height; ++y) {
                    auto* dest = canvas.get() + (std::size_t(frame.y + y) * width + frame.x) * 4;
                    if (restore)
                        std::memcpy(dest, previous.get() + y * row_bytes, row_bytes);
                    else
                        std::memset(dest, 0, row_bytes);
                }
            }
        };

        for (std::size_t at = 33; at < size;) {
            checkpoint(cancel);
            valid(size - at >= 12, "Truncated APNG chunk");
            const auto length = read_be(input + at);
            valid(length <= size - at - 12, "Truncated APNG chunk data");
            const auto* tag = input + at + 4;
            const auto* data = tag + 4;
            const std::string_view kind(reinterpret_cast<const char*>(tag), 4);
            valid(read_be(data + length) == chunk_crc(tag, length), "APNG chunk CRC differs");
            at += std::size_t(length) + 12;
            if (have_idat && kind != "IDAT") idat_closed = true;
            if (kind == "acTL") {
                valid(!frames && !have_idat && length == 8, "Invalid APNG animation control");
                frames = read_be(data);
                valid(frames && frames <= frame_limit, "APNG frame count exceeds the bounded profile");
                require(selected < frames, "APNG frame index is outside the animation");
            } else if (kind == "sRGB") {
                valid(!have_srgb && !have_idat && length == 1 && data[0] <= 3, "Invalid APNG sRGB declaration");
                have_srgb = true;
            } else if (kind == "fcTL") {
                valid(frames && current < frames && length == 26 && read_be(data) == sequence++,
                      "Invalid APNG frame control or sequence");
                if (controlled || have_idat) finish();
                controlled = true;
                frame_uses_idat = !have_idat;
                ++current;
                frame = {read_be(data + 4), read_be(data + 8), read_be(data + 12), read_be(data + 16), data[24], data[25]};
                valid(frame.width && frame.height && frame.width <= width && frame.height <= height &&
                      frame.x <= width - frame.width && frame.y <= height - frame.height &&
                      frame.dispose <= 2 && frame.blend <= 1, "Invalid APNG frame geometry or render operation");
                valid(have_idat || (frame.width == width && frame.height == height && !frame.x && !frame.y),
                      "APNG animated IDAT frame must cover the entire canvas");
            } else if (kind == "IDAT") {
                valid(frames && !idat_closed, "APNG requires consecutive IDAT after animation control");
                have_idat = true;
                append(data, length);
            } else if (kind == "fdAT") {
                valid(idat_closed && controlled && !frame_uses_idat && length >= 4 && read_be(data) == sequence++,
                      "Invalid APNG image data sequence");
                append(data + 4, length - 4);
            } else if (kind == "IEND") {
                valid(!length && at == size && have_idat && current == frames && frames,
                      "Invalid APNG end or declared frame count");
                finish();
                valid(saved, "APNG selected frame is missing");
                return raster;
            } else {
                fail(QPX_READER_UNSUPPORTED, "APNG chunk is outside the RGB8 animation profile");
            }
        }
        fail(QPX_READER_IO_ERROR, "APNG has no complete IEND chunk");
    }
};
using Reader = PackedImageReader<Codec, Rgba8Raster, false, false, true>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}
