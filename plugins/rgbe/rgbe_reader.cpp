// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include <charconv>

namespace {
using namespace qpx_image_plugin;
constexpr unsigned max_dimension = 32767;
constexpr std::size_t max_header = 65536;

struct Input {
    const std::uint8_t* next;
    std::size_t remaining;

    const std::uint8_t* take(std::size_t count) {
        valid(count <= remaining, "Truncated RGBE scanline");
        const auto* first = next;
        next += count;
        remaining -= count;
        return first;
    }
    std::string_view line(std::size_t& header_left) {
        const auto* newline = static_cast<const std::uint8_t*>(
            std::memchr(next, '\n', std::min(remaining, header_left)));
        valid(newline != nullptr, "Missing or oversized RGBE header line");
        const auto size = std::size_t(newline - next);
        std::string_view value(reinterpret_cast<const char*>(next), size);
        take(size + 1);
        header_left -= size + 1;
        if (!value.empty() && value.back() == '\r') value.remove_suffix(1);
        for (const auto ch : value)
            valid(ch == '\t' || (ch >= ' ' && ch <= '~'), "Non-ASCII RGBE header");
        return value;
    }
};

std::string_view token(std::string_view& line) {
    const auto start = line.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
        line = {};
        return {};
    }
    line.remove_prefix(start);
    const auto end = line.find_first_of(" \t");
    const auto value = line.substr(0, end);
    line.remove_prefix(value.size());
    return value;
}

unsigned dimension(std::string_view value) {
    valid(!value.empty(), "Missing RGBE dimension");
    unsigned result = 0;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result);
    valid(parsed.ec == std::errc{} && parsed.ptr == end && result > 0 && result <= max_dimension,
          "RGBE dimension must be between 1 and 32767");
    return result;
}

struct Raster {
    const char* vendor = "Radiance RGBE (linear)";
    unsigned width = 0, height = 0;
    std::unique_ptr<std::uint8_t[]> raw;
    static constexpr unsigned channels() { return 3; }
    static constexpr unsigned sample_bytes() { return sizeof(float); }
    static constexpr std::uint32_t sample_type() { return QPX_READER_FLOAT32; }
    static const char* channel_name(unsigned channel) { return Rgba8Raster::channel_name(channel); }

    void rgba(const std::uint8_t* pixel, std::uint8_t* output) const {
        std::array<float, 3> rgb{};
        std::memcpy(rgb.data(), pixel, 3 * sizeof(float));
        const double divisor = 1.0 + double(*std::max_element(rgb.begin(), rgb.end()));
        for (unsigned channel = 0; channel < 3; ++channel) {
            const double value = double(rgb[channel]) / divisor;
            const double encoded = value <= 0.0031308 ? 12.92 * value :
                                   1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
            output[channel] = std::uint8_t(std::clamp(std::floor(255 * encoded + 0.5), 0.0, 255.0));
        }
        output[3] = 255;
    }
};

Raster header(Input& input) {
    std::size_t header_left = max_header;
    const auto signature = input.line(header_left);
    valid(signature == "#?RADIANCE" || signature == "#?RGBE", "Unrecognized RGBE signature");
    bool format = false;
    for (auto line = input.line(header_left); !line.empty(); line = input.line(header_left)) {
        if (line.substr(0, 7) == "FORMAT=") {
            valid(!format && line == "FORMAT=32-bit_rle_rgbe", "Unsupported or duplicate RGBE format");
            format = true;
        } else if (line.front() != '#' && line.find('=') != std::string_view::npos) {
            const auto key = line.substr(0, line.find('='));
            valid(key == "SOFTWARE" || key == "CAPDATE" || key == "GMT" || key == "VIEW",
                  "RGBE metadata requires a separate interpretation profile");
        }
    }
    valid(format, "RGBE format declaration is missing");
    auto extent = input.line(header_left);
    Raster raster;
    valid(token(extent) == "-Y", "RGBE requires top-left scanline order (-Y +X)");
    raster.height = dimension(token(extent));
    valid(token(extent) == "+X", "RGBE requires top-left scanline order (-Y +X)");
    raster.width = dimension(token(extent));
    valid(token(extent).empty(), "Unexpected RGBE resolution data");
    return raster;
}

const std::uint8_t* scanline(Input& input, unsigned width, std::uint8_t* unpacked) {
    const auto* first = input.take(4);
    if (width < 8 || first[0] != 2 || first[1] != 2 || first[2] >= 128) {
        input.take(4 * (width - 1));
        for (unsigned x = 0; x < width; ++x) {
            const auto* pixel = first + 4 * x;
            valid(pixel[0] != 1 || pixel[1] != 1 || pixel[2] != 1,
                  "Legacy RGBE repeat markers are outside the flat profile");
        }
        return first;
    }
    valid((unsigned(first[2]) << 8 | first[3]) == width, "RGBE scanline width does not match the header");
    for (unsigned component = 0; component < 4; ++component) {
        for (unsigned x = 0; x < width;) {
            const unsigned packet = *input.take(1);
            const bool repeated = packet > 128;
            const unsigned count = repeated ? packet - 128 : packet;
            valid(count && count <= width - x, "Invalid RGBE scanline packet extent");
            const auto* bytes = input.take(repeated ? 1 : count);
            for (unsigned i = 0; i < count; ++i)
                unpacked[4 * (x + i) + component] = bytes[repeated ? 0 : i];
            x += count;
        }
    }
    return unpacked;
}

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.rgbe";
    static constexpr const char* name = "Radiance RGBE";
    static constexpr const char* version = QPX_RGBE_VERSION;
    static constexpr const char* contract = "radiance-linear-rgbe/1";

    static Raster decode(std::uint8_t* bytes, std::size_t size, Budget& budget, qpx_reader_cancel_v1 cancel) {
        Input input{bytes, size};
        auto raster = header(input);
        auto row = budget.array(std::uint64_t(4) * raster.width);
        raster.raw = budget.array(std::uint64_t(3) * sizeof(float) * raster.width * raster.height);
        auto* target = raster.raw.get();
        for (unsigned y = 0; y < raster.height; ++y) {
            checkpoint(cancel);
            const auto* pixel = scanline(input, raster.width, row.get());
            for (unsigned x = 0; x < raster.width; ++x, pixel += 4) {
                const int exponent = int(pixel[3]) - 137;
                for (unsigned c = 0; c < 3; ++c) {
                    const float value = pixel[3] ? std::ldexp(float(2 * pixel[c] + 1), exponent) : 0.0f;
                    std::memcpy(target, &value, sizeof(value));
                    target += sizeof(value);
                }
            }
        }
        valid(!input.remaining, "Unexpected trailing RGBE data");
        return raster;
    }
};

using Reader = PackedImageReader<Codec, Raster>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}
