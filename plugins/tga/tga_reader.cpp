// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/rgba8_reader.hpp"

namespace {
using namespace qpx_image_plugin;

std::uint32_t little_endian(const std::uint8_t* bytes, unsigned count) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i)
        value |= std::uint32_t(bytes[i]) << (8 * i);
    return value;
}

enum class PixelLayout { indexed, gray, rgb, rgba };

struct TgaRaster {
    std::int64_t width, height;
    const char* vendor = "TGA";
    PixelLayout layout;
    std::array<std::array<std::uint8_t, 3>, 256> palette{};
    std::unique_ptr<std::uint8_t[]> raw;

    unsigned channels() const {
        if (layout == PixelLayout::rgba)
            return 4;
        return layout == PixelLayout::rgb ? 3 : 1;
    }
    static constexpr unsigned sample_bytes() { return 1; }
    static constexpr std::uint32_t sample_type() { return QPX_READER_UINT8; }
    const char* channel_name(unsigned channel) const {
        if (layout == PixelLayout::indexed)
            return "Palette index";
        if (layout == PixelLayout::gray)
            return "Gray";
        return Rgba8Raster::channel_name(channel);
    }
    void rgba(const std::uint8_t* source, std::uint8_t* output) const {
        if (layout == PixelLayout::indexed)
            std::memcpy(output, palette[source[0]].data(), 3);
        else if (layout == PixelLayout::gray)
            std::fill_n(output, 3, source[0]);
        else
            std::memcpy(output, source, 3);
        output[3] = layout == PixelLayout::rgba ? source[3] : 255;
    }
};

PixelLayout pixel_layout(unsigned type, unsigned depth, unsigned attributes) {
    if (type == 1 && depth == 8 && !attributes)
        return PixelLayout::indexed;
    if (type == 3 && depth == 8 && !attributes)
        return PixelLayout::gray;
    if (type == 2 && depth == 24 && !attributes)
        return PixelLayout::rgb;
    if (type == 2 && depth == 32 && attributes == 8)
        return PixelLayout::rgba;
    fail(QPX_READER_UNSUPPORTED, "TGA pixel type, depth or attributes are unsupported");
}

struct RasterExtent {
    std::size_t end;
    bool has_footer;
};

RasterExtent raster_end(const std::uint8_t* input, std::size_t size, std::size_t first,
                        bool alpha) {
    constexpr char signature[] = "TRUEVISION-XFILE.";
    constexpr std::size_t footer_size = 8 + sizeof(signature);
    if (size < footer_size ||
        std::memcmp(input + size - sizeof(signature), signature, sizeof(signature)))
        return {size, false};

    const auto footer = size - footer_size;
    valid(footer >= first, "TGA footer overlaps its header or image ID");
    const auto extension = little_endian(input + footer, 4);
    if (little_endian(input + footer + 4, 4))
        fail(QPX_READER_UNSUPPORTED, "TGA developer area is unsupported");
    if (!extension)
        return {footer, true};

    constexpr std::size_t extension_size = 495;
    valid(extension >= first && extension <= footer && footer - extension == extension_size &&
              little_endian(input + extension, 2) == extension_size,
          "TGA extension must be one contiguous standard block");
    const auto* metadata = input + extension;
    const auto attribute = metadata[494];
    if (attribute != (alpha ? 3 : 0))
        fail(QPX_READER_UNSUPPORTED, "TGA extension does not declare the selected alpha semantics");
    const auto raster_settings = metadata + 470;
    if (std::any_of(raster_settings, metadata + 494, [](std::uint8_t byte) { return byte != 0; }))
        fail(QPX_READER_UNSUPPORTED, "TGA extension contains unsupported raster settings");
    return {extension, true};
}

struct Tga {
    static constexpr const char* provider_id = "org.qupathxx.tga";
    static constexpr const char* name = "TGA";
    static constexpr const char* version = QPX_TGA_VERSION;
    static constexpr const char* contract = "tga-native-samples/1";

    static TgaRaster decode(const std::uint8_t* input, std::size_t size, Budget& budget,
                            qpx_reader_cancel_v1 cancel) {
        constexpr std::size_t header_size = 18;
        valid(size >= header_size, "TGA header is truncated");
        const bool compressed = (input[2] & 8) != 0;
        const auto type = unsigned(input[2]) & ~8u;
        if (little_endian(input + 8, 4))
            fail(QPX_READER_UNSUPPORTED, "TGA display origin offsets are unsupported");
        const auto descriptor = input[17];
        if (descriptor & 0xc0)
            fail(QPX_READER_UNSUPPORTED, "TGA reader requires non-interleaved rows");
        const auto layout = pixel_layout(type, input[16], descriptor & 15);
        const bool indexed = layout == PixelLayout::indexed;
        const bool alpha = layout == PixelLayout::rgba;
        const auto palette_begin = little_endian(input + 3, 2);
        const auto palette_size = little_endian(input + 5, 2);
        const auto palette_end = palette_begin + palette_size;
        if (indexed) {
            if (input[1] != 1 || input[7] != 24)
                fail(QPX_READER_UNSUPPORTED, "TGA indexed images require a 24-bit color map");
            valid(palette_size && palette_end <= 256, "TGA color map range exceeds the index domain");
        } else if (input[1] || palette_begin || palette_size || input[7]) {
            fail(QPX_READER_UNSUPPORTED, "TGA direct samples require an image without a palette");
        }
        const auto width = little_endian(input + 12, 2);
        const auto height = little_endian(input + 14, 2);
        valid(width && height, "TGA dimensions must be positive");
        std::size_t cursor = header_size + input[0];
        valid(cursor <= size, "TGA image ID is truncated");
        const auto extent = raster_end(input, size, cursor, alpha);

        TgaRaster raster{width, height, "TGA", layout, {}, {}};
        if (indexed) {
            valid(palette_size * 3 <= extent.end - cursor, "TGA color map is truncated");
            for (auto index = palette_begin; index < palette_end; ++index, cursor += 3)
                raster.palette[index] = {input[cursor + 2], input[cursor + 1], input[cursor]};
        }
        const auto stride = raster.channels();
        const auto count = std::uint64_t(width) * height;
        require(count <= work_limit / stride, "TGA raster exceeds the reader work limit");
        if (!compressed) {
            valid(extent.end - cursor == count * stride, "TGA plain raster extent differs from the header");
        } else {
            const auto minimum_bytes = ((count + 127) / 128) * (stride + 1);
            valid(extent.end - cursor >= minimum_bytes, "TGA packet stream cannot fill the raster");
        }
        raster.raw = budget.array(count * stride);

        const bool right = (descriptor & 0x10) != 0;
        const bool top = (descriptor & 0x20) != 0;
        std::uint64_t pixel = 0;
        while (pixel < count) {
            checkpoint(cancel);
            auto run = static_cast<unsigned>(std::min<std::uint64_t>(width - pixel % width, 4096));
            bool repeated = false;
            if (compressed) {
                valid(cursor < extent.end, "TGA packet header is truncated");
                const auto packet = input[cursor++];
                run = (packet & 127) + 1;
                repeated = (packet & 128) != 0;
                valid(run <= count - pixel, "TGA packet exceeds the raster size");
                if (extent.has_footer && repeated)
                    valid(run <= width - pixel % width, "TGA version 2 repeat packet crosses a row");
            }
            const auto bytes = (repeated ? 1u : run) * stride;
            valid(bytes <= extent.end - cursor, "TGA packet payload is truncated");
            const auto* source = input + cursor;
            for (unsigned i = 0; i < run; ++i, ++pixel) {
                const auto x = pixel % width, y = pixel / width;
                const auto column = right ? width - 1 - x : x;
                const auto row = top ? y : height - 1 - y;
                auto* target = raster.raw.get() + (row * width + column) * stride;
                if (stride == 1) {
                    const auto code = source[0];
                    if (indexed)
                        valid(code >= palette_begin && code < palette_end,
                              "TGA pixel index is outside the declared color map");
                    target[0] = code;
                } else {
                    target[0] = source[2];
                    target[1] = source[1];
                    target[2] = source[0];
                    if (alpha)
                        target[3] = source[3];
                }
                if (!repeated)
                    source += stride;
            }
            cursor += bytes;
        }
        valid(cursor == extent.end, "TGA raster contains trailing data");
        return raster;
    }
};
} // namespace

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return PackedImageReader<Tga, TgaRaster>::entry(abi, out, error);
}
