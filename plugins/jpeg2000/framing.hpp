// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "../image-common/rgba8_reader.hpp"
#include <bitset>
#include <string_view>
#include <utility>

namespace qpx_jpeg2000 {
using qpx_image_plugin::checkpoint;
using qpx_image_plugin::fail;
using qpx_image_plugin::require;
using qpx_image_plugin::valid;

inline void supported(bool condition, const char* message) {
    if (!condition)
        fail(QPX_READER_UNSUPPORTED, message);
}

using Bytes = std::string_view;
inline std::uint32_t be(Bytes bytes, std::size_t offset, std::size_t count) {
    valid(offset <= bytes.size() && count <= bytes.size() - offset,
          "JPEG 2000 field is truncated");
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < count; ++index)
        value = (value << 8) | static_cast<std::uint8_t>(bytes[offset + index]);
    return value;
}

struct Box {
    Bytes type, payload;
};

inline Box next_box(Bytes& input) {
    const auto size = be(input, 0, 4);
    valid(size >= 8 && size <= input.size(), "JPEG 2000 box has an invalid extent");
    const Box result{input.substr(4, 4), input.substr(8, size - 8)};
    input.remove_prefix(size);
    return result;
}

struct Input {
    Bytes codestream;
    std::uint32_t width, height, precision, tiles, channels;
    bool signed_samples;
    bool irreversible;
    bool boxed = false;
    double pixel_width_um = 0, pixel_height_um = 0;
};

inline constexpr std::uint32_t max_tiles = 4096;
inline constexpr std::uint32_t max_quality_layers = 16;
inline constexpr std::uint64_t max_precinct_packets = 2u * 1024 * 1024;
inline constexpr std::uint32_t explicit_precincts = 1, sop_markers = 2, eph_markers = 4;

inline std::uint32_t ceil_ratio(std::uint32_t value, std::uint32_t divisor) {
    return (value + divisor - 1) / divisor;
}

// Tile bounds are mapped to each resolution before counting its grid cells.
// Summing each axis separately counts the whole rectangular grid of tiles.
inline std::uint32_t axis_precincts(std::uint32_t extent, std::uint32_t tile,
                                    std::uint32_t scale, std::uint32_t precinct) {
    std::uint32_t count = 0;
    for (std::uint32_t start = 0; start < extent; start += tile) {
        const auto first = ceil_ratio(start, scale);
        const auto last = ceil_ratio(std::min(start + tile, extent), scale);
        if (first < last)
            count += ceil_ratio(last, precinct) - first / precinct;
    }
    return count;
}

inline void validate_precincts(Bytes sizes, std::uint32_t width, std::uint32_t height,
    std::uint32_t tile_width, std::uint32_t tile_height, std::uint32_t layers,
    std::uint32_t channels, qpx_reader_cancel_v1 cancel) {
    std::uint64_t packets = 0;
    for (std::size_t level = 0; level < sizes.size(); ++level) {
        checkpoint(cancel);
        const auto size = be(sizes, level, 1);
        const auto horizontal = size & 15, vertical = size >> 4;
        valid(!level || (horizontal && vertical),
              "JPEG 2000 high-resolution precinct exponent must be positive");
        const auto scale = 1u << (sizes.size() - 1 - level);
        const auto columns = axis_precincts(width, tile_width, scale, 1u << horizontal);
        const auto rows = axis_precincts(height, tile_height, scale, 1u << vertical);
        packets += std::uint64_t(columns) * rows * layers * channels;
        require(packets <= max_precinct_packets, "JPEG 2000 precinct grid exceeds 2097152 packets");
    }
}

inline std::size_t tile_body(Bytes input, std::size_t cursor, std::size_t end,
                             qpx_reader_cancel_v1 cancel) {
    std::uint32_t index = 0;
    std::uint64_t total = 0, packet = 0;
    unsigned bytes = 0;
    const auto tile_budget = end - cursor;
    while (end - cursor >= 2 && be(input, cursor, 2) == 0xff58) {
        checkpoint(cancel);
        valid(end - cursor >= 6, "JPEG 2000 PLT segment is truncated");
        const auto size = be(input, cursor + 2, 2);
        valid(size >= 4 && size <= end - cursor - 2, "JPEG 2000 PLT segment extent is invalid");
        valid(be(input, cursor + 4, 1) == index, "JPEG 2000 PLT segment indices must be consecutive");
        ++index;
        const auto limit = cursor + 2 + size;
        for (auto pos = cursor + 5; pos < limit; ++pos) {
            const auto value = static_cast<std::uint8_t>(input[pos]);
            valid(++bytes <= 5, "JPEG 2000 PLT packet length is too wide");
            packet = (packet << 7) | (value & 127);
            valid(packet <= tile_budget - total, "JPEG 2000 PLT packet lengths exceed the tile extent");
            if (!(value & 128)) {
                valid(packet != 0, "JPEG 2000 PLT packet length must be positive");
                total += packet;
                packet = 0;
                bytes = 0;
            }
        }
        cursor = limit;
    }
    supported(end - cursor >= 3 && be(input, cursor, 2) == 0xff93,
              "JPEG 2000 tile header accepts only packet-length tables before SOD");
    if (index)
        valid(bytes == 0 && total == end - cursor - 2,
              "JPEG 2000 PLT lengths must cover the complete tile body");
    return cursor + 2;
}

inline void validate_tile_parts(Bytes input, std::size_t cursor, std::uint32_t count,
                                std::uint32_t flags, qpx_reader_cancel_v1 cancel) {
    struct TileParts {
        std::uint8_t next = 0, total = 0;
    };
    std::array<TileParts, max_tiles> tiles{};
    std::uint32_t completed = 0;
    while (input.size() - cursor > 2) {
        checkpoint(cancel);
        valid(be(input, cursor, 4) == 0xff90000a,
              "JPEG 2000 tile-part marker is invalid or missing");
        const auto tile = be(input, cursor + 4, 2);
        valid(tile < count, "JPEG 2000 tile ID is outside the grid");
        const auto size = be(input, cursor + 6, 4);
        const auto part = be(input, cursor + 10, 1), total = be(input, cursor + 11, 1);
        supported(total != 0, "JPEG 2000 requires an explicit tile-part count");
        auto& sequence = tiles[tile];
        if (sequence.next == 0)
            sequence.total = static_cast<std::uint8_t>(total);
        valid(part == sequence.next && part < total && total == sequence.total,
              "JPEG 2000 tile-part sequence is incomplete, repeated or inconsistent");
        valid(size > 14 && size <= input.size() - cursor - 2,
              "JPEG 2000 tile extent is invalid");
        const auto end = cursor + size;
        const auto body = tile_body(input, cursor + 12, end, cancel);
        for (auto position = body; position + 1 < end; ++position) {
            if ((position & 65535) == 0)
                checkpoint(cancel);
            if (static_cast<std::uint8_t>(input[position]) != 0xff)
                continue;
            const auto marker = static_cast<std::uint8_t>(input[position + 1]);
            if (marker == 0x91 && (flags & sop_markers)) {
                valid(end - position >= 6 && be(input, position + 2, 2) == 4,
                      "JPEG 2000 SOP marker segment has an invalid extent");
                position += 5; // Skip the length and sequence number as well.
            } else if (marker == 0x92 && (flags & eph_markers)) {
                ++position;
            } else {
                valid(marker < 0x90, "JPEG 2000 tile contains an unexpected marker");
            }
        }
        if (++sequence.next == sequence.total)
            ++completed;
        cursor = end;
    }
    valid(completed == count && input.size() - cursor == 2 && be(input, cursor, 2) == 0xffd9,
          "JPEG 2000 expected EOC after the complete tile-part grid");
}

inline Input parse_codestream(Bytes input, std::uint32_t channels, qpx_reader_cancel_v1 cancel) {
    supported(be(input, 0, 2) == 0xff4f, "Expected JPEG 2000 SOC marker");
    valid(be(input, 2, 2) == 0xff51, "JPEG 2000 SIZ must follow SOC");
    const auto count = be(input, 40, 2);
    supported(count == channels, "JPEG 2000 component count differs from the selected color profile");
    valid(be(input, 4, 2) == 38 + 3 * count, "JPEG 2000 SIZ length differs from the component count");
    supported(be(input, 6, 2) == 0, "JPEG 2000 requires a baseline Part 1 codestream");
    const auto width = be(input, 8, 4), height = be(input, 12, 4);
    require(width && height && width <= 8192 && height <= 8192 &&
                std::uint64_t(width) * height <= 4u * 1024 * 1024,
            "JPEG 2000 dimensions exceed the selected decode limits");
    supported(be(input, 16, 4) == 0 && be(input, 20, 4) == 0 &&
                  be(input, 32, 4) == 0 && be(input, 36, 4) == 0,
              "JPEG 2000 requires zero image and tile origins");
    const auto tile_width = be(input, 24, 4), tile_height = be(input, 28, 4);
    require(tile_width && tile_height && tile_width <= width && tile_height <= height,
            "JPEG 2000 tile dimensions are outside the selected raster profile");
    const auto tiles = (1 + (width - 1) / tile_width) * (1 + (height - 1) / tile_height);
    require(tiles <= max_tiles, "JPEG 2000 tile grid exceeds 4096 tiles");
    const auto sample = be(input, 42, 1);
    const auto depth = (sample & 127) + 1;
    const bool signed_samples = (sample & 128) != 0;
    supported(depth >= 8 && depth <= 16, "JPEG 2000 requires 8- to 16-bit samples");
    supported(!signed_samples || channels == 1, "JPEG 2000 signed samples require grayscale without alpha");
    for (std::uint32_t channel = 0; channel < channels; ++channel)
        supported(be(input, 42 + 3 * channel, 3) == (sample << 16 | 0x0101),
                  "JPEG 2000 requires full-size components with the same precision and signedness");

    bool coding = false, quantization = false;
    std::uint32_t decompositions = 0, transform = 0, flags = 0;
    std::size_t quantization_offset = 0;
    std::size_t cursor = 42 + 3 * count;
    for (;;) {
        checkpoint(cancel);
        const auto marker = be(input, cursor, 2);
        const auto length = be(input, cursor + 2, 2);
        valid(length >= 2 && length <= input.size() - cursor - 2,
              "JPEG 2000 marker has an invalid length");
        if (marker == 0xff90)
            break;
        if (marker == 0xff52) {
            valid(!coding && length >= 12, "JPEG 2000 COD is truncated or repeated");
            decompositions = be(input, cursor + 9, 1);
            transform = be(input, cursor + 13, 1);
            const auto layers = be(input, cursor + 6, 2);
            supported(layers >= 1 && layers <= max_quality_layers,
                      "JPEG 2000 supports 1 to 16 quality layers");
            const auto progression = be(input, cursor + 5, 1);
            supported(progression <= 4, "JPEG 2000 quality progression order is outside Part 1");
            flags = be(input, cursor + 4, 1);
            supported((flags & ~(explicit_precincts | sop_markers | eph_markers)) == 0 &&
                          be(input, cursor + 8, 1) <= (channels >= 3 ? 1u : 0u) &&
                          decompositions <= 6 && transform <= 1,
                      "JPEG 2000 requires Part 1 coding without reserved flags");
            const auto precinct_count = (flags & explicit_precincts) ? decompositions + 1 : 0;
            valid(length == 12 + precinct_count,
                  "JPEG 2000 precinct table length differs from the declared resolutions");
            if (precinct_count)
                validate_precincts(input.substr(cursor + 14, precinct_count), width, height,
                                   tile_width, tile_height, layers, channels, cancel);
            coding = true;
        } else if (marker == 0xff5c) {
            valid(!quantization && length >= 4, "JPEG 2000 QCD is truncated or repeated");
            quantization_offset = cursor;
            quantization = true;
        } else {
            supported(marker == 0xff64, "JPEG 2000 has an unsupported main-header marker");
        }
        cursor += 2 + length;
    }
    valid(coding && quantization, "JPEG 2000 coding markers are incomplete");
    const bool irreversible = transform == 0;
    const auto quantization_byte = be(input, quantization_offset + 4, 1);
    const auto style = quantization_byte & 31;
    if (irreversible) {
        supported(channels == 1 && depth == 8 && !signed_samples && style == 2,
                  "JPEG 2000 irreversible profile requires unsigned 8-bit grayscale and explicit scalar quantization");
        const auto bands = 1 + 3 * decompositions;
        valid(be(input, quantization_offset + 2, 2) == 3 + 2 * bands,
              "JPEG 2000 scalar quantization count differs from the declared subbands");
        for (std::uint32_t band = 0; band < bands; ++band) {
            const auto exponent = be(input, quantization_offset + 5 + 2 * band, 2) >> 11;
            supported(exponent >= 1 && exponent <= 16,
                      "JPEG 2000 scalar quantization exponent falls outside the selected profile");
        }
    } else {
        supported(style == 0, "JPEG 2000 reversible profile requires no quantization");
    }
    validate_tile_parts(input, cursor, tiles, flags, cancel);
    return {input, width, height, depth, tiles, channels, signed_samples, irreversible};
}

inline void validate_channel_definitions(Box box, std::uint32_t channels,
                                         std::uint32_t color_channels) {
    supported(box.type == "cdef", "JP2 requires explicit channel definitions for opacity");
    valid(box.payload.size() == 2 + 6 * channels && be(box.payload, 0, 2) == channels,
          "JP2 channel definition size or count disagrees with the header");
    std::bitset<4> seen;
    for (std::uint32_t row = 0; row < channels; ++row) {
        const auto offset = 2 + row * 6;
        const auto channel = be(box.payload, offset, 2);
        valid(channel < channels && !seen[channel], "JP2 channel index is duplicate or out of range");
        seen.set(channel);
        const auto mapping = be(box.payload, offset + 2, 4);
        supported(mapping == (channel < color_channels ? channel + 1 : 0x00010000u),
                  "JP2 requires canonical color order and optional straight whole-image opacity");
    }
}

inline std::pair<double, double> capture_spacing(Box resolution) {
    valid(!resolution.payload.empty(), "JP2 resolution box is empty");
    bool capture = false, display = false;
    double width = 0, height = 0;
    while (!resolution.payload.empty()) {
        const auto part = next_box(resolution.payload);
        supported(part.type == "resc" || part.type == "resd", "Unsupported JP2 resolution box");
        auto& seen = part.type == "resc" ? capture : display;
        valid(!seen && part.payload.size() == 10, "JP2 resolution box is repeated or malformed");
        seen = true;
        const auto spacing = [&](std::size_t offset, std::size_t exponent_offset) {
            const auto numerator = be(part.payload, offset, 2);
            const auto denominator = be(part.payload, offset + 2, 2);
            const auto code = static_cast<int>(be(part.payload, exponent_offset, 1));
            const auto exponent = code < 128 ? code : code - 256;
            valid(numerator && denominator, "JP2 resolution ratios must be positive");
            return 1.0e6 / (double(numerator) / denominator * std::pow(10.0, exponent));
        };
        const auto vertical = spacing(0, 8);
        const auto horizontal = spacing(4, 9);
        if (part.type == "resc") {
            width = horizontal;
            height = vertical;
        }
    }
    return {width, height};
}

inline Input parse_image(Bytes input, qpx_reader_cancel_v1 cancel) {
    valid(input.size() >= 2, "JPEG 2000 input is truncated");
    if (be(input, 0, 2) == 0xff4f)
        return parse_codestream(input, 1, cancel);

    const auto signature = next_box(input);
    supported(signature.type == "jP  " && signature.payload.size() == 4 &&
                  be(signature.payload, 0, 4) == 0x0d0a870a,
              "Input has no standard JP2 signature");
    const auto file_type = next_box(input);
    supported(file_type.type == "ftyp" && file_type.payload.size() == 12 &&
                  file_type.payload.substr(0, 4) == "jp2 " && be(file_type.payload, 4, 4) == 0 &&
                  file_type.payload.substr(8, 4) == "jp2 ",
              "JPEG 2000 reader requires the baseline JP2 brand");
    auto header = next_box(input);
    supported(header.type == "jp2h", "JP2 image header box is missing");
    const auto image = next_box(header.payload);
    Box color{}, definitions{}, resolution{};
    while (!header.payload.empty()) {
        const auto part = next_box(header.payload);
        Box* slot = nullptr;
        if (part.type == "colr") slot = &color;
        else if (part.type == "cdef") slot = &definitions;
        else if (part.type == "res ") slot = &resolution;
        supported(slot && slot->type.empty(), "JP2 header box is unsupported or repeated");
        *slot = part;
    }
    supported(image.type == "ihdr" && image.payload.size() == 14 &&
                  be(image.payload, 11, 3) == 0x070000 &&
                  color.type == "colr" && color.payload.size() == 7 &&
                  be(color.payload, 0, 3) == 0x010000,
              "JP2 requires enumerated gray or sRGB");
    const auto channels = be(image.payload, 8, 2);
    const auto colorspace = be(color.payload, 3, 4);
    supported(colorspace == 17 || colorspace == 16, "JP2 requires enumerated gray or sRGB");
    const auto color_channels = colorspace == 17 ? 1u : 3u;
    supported(channels == color_channels || channels == color_channels + 1,
              "JP2 component count differs from the color and opacity profile");
    if (!definitions.type.empty() || channels != color_channels)
        validate_channel_definitions(definitions, channels, color_channels);
    const auto spacing = resolution.type.empty() ? std::pair{0.0, 0.0} : capture_spacing(resolution);
    const auto codestream = next_box(input);
    supported(codestream.type == "jp2c" && input.empty(),
              "JP2 requires exactly one codestream and no trailing boxes");
    auto result = parse_codestream(codestream.payload, channels, cancel);
    const auto sample = (result.precision - 1) | (result.signed_samples ? 128u : 0u);
    valid(result.width == be(image.payload, 4, 4) && result.height == be(image.payload, 0, 4) &&
              sample == be(image.payload, 10, 1),
          "JP2 image header disagrees with the codestream");
    result.boxed = true;
    result.pixel_width_um = spacing.first;
    result.pixel_height_um = spacing.second;
    return result;
}
}  // namespace qpx_jpeg2000
