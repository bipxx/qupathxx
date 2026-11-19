// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include "../image-common/gzip_member.hpp"

namespace {
using namespace qpx_image_plugin;
constexpr unsigned max_dimension = 32768;
constexpr std::size_t max_header = 65536;
constexpr unsigned compression_none = 0, compression_zips = 2, compression_zip = 3;

struct Input {
    std::uint8_t* next;
    std::size_t left;

    std::uint8_t* take(std::size_t count) {
        valid(count <= left, "Truncated OpenEXR data");
        auto* first = next;
        next += count;
        left -= count;
        return first;
    }
    std::string_view name() {
        const auto* end = static_cast<const std::uint8_t*>(std::memchr(next, 0, std::min<std::size_t>(left, 32)));
        valid(end != nullptr, "Missing or oversized OpenEXR name");
        const auto size = std::size_t(end - next);
        return {reinterpret_cast<const char*>(take(size + 1)), size};
    }
};

template <class T> T load_le(const void* bytes) {
    // The plugin build targets Windows x64, where both integer and Float32 storage are little-endian.
    T result;
    std::memcpy(&result, bytes, sizeof(T));
    return result;
}

struct Header {
    unsigned width = 0, height = 0, compression = 0;
    bool descending = false;
    std::array<unsigned, 4> channel_bytes{}; // A (optional), B, G, R file order.
};

struct Attribute {
    std::string_view name, type;
    Input value{nullptr, 0};
};

void validate_attribute(const Attribute& entry, std::string_view type, std::size_t size) {
    valid(entry.type == type && entry.value.left == size, "Invalid OpenEXR attribute type or size");
}

Header parse_header(Input& file) {
    valid(load_le<std::uint32_t>(file.take(4)) == 20000630, "Unrecognized OpenEXR signature");
    valid(load_le<std::uint32_t>(file.take(4)) == 2, "OpenEXR requires a version 2 single-part scanline image");
    Input input{file.next, std::min(file.left, max_header)};
    const auto available = input.left;
    std::array<Attribute, 9> attributes{{{"channels", "chlist"}, {"compression", "compression"},
        {"dataWindow", "box2i"}, {"displayWindow", "box2i"}, {"lineOrder", "lineOrder"},
        {"pixelAspectRatio", "float"}, {"screenWindowCenter", "v2f"}, {"screenWindowWidth", "float"},
        {"type", "string"}}};
    for (auto name = input.name(); !name.empty(); name = input.name()) {
        const auto type = input.name();
        const auto length = load_le<std::uint32_t>(input.take(4));
        const auto found = std::find_if(attributes.begin(), attributes.end(),
                                        [&](const auto& field) { return name == field.name; });
        valid(found != attributes.end(), "OpenEXR attribute is outside the linear RGB profile");
        valid(!found->value.next && type == found->type, "Duplicate or mistyped OpenEXR attribute");
        found->value = {input.take(length), length};
    }
    file.take(available - input.left);
    const auto& [channels, compression, data, display, order, aspect, center, screen, part_type] = attributes;
    if (part_type.value.next) {
        const std::string_view name(reinterpret_cast<const char*>(part_type.value.next), part_type.value.left);
        valid(name == "scanlineimage", "OpenEXR type must designate scanlineimage");
    }
    validate_attribute(data, "box2i", 16);
    validate_attribute(display, "box2i", 16);
    valid(!std::memcmp(data.value.next, display.value.next, 16) && !load_le<std::uint64_t>(data.value.next),
          "OpenEXR requires identical zero-origin data and display windows");
    const auto max_x = load_le<std::uint32_t>(data.value.next + 8);
    const auto max_y = load_le<std::uint32_t>(data.value.next + 12);
    valid(max_x < max_dimension && max_y < max_dimension, "OpenEXR dimensions exceed the scanline profile");
    validate_attribute(aspect, "float", 4);
    validate_attribute(center, "v2f", 8);
    validate_attribute(screen, "float", 4);
    valid(load_le<float>(aspect.value.next) == 1.0f && std::isfinite(load_le<float>(center.value.next)) &&
              std::isfinite(load_le<float>(center.value.next + 4)) && std::isfinite(load_le<float>(screen.value.next)) &&
              load_le<float>(screen.value.next) > 0,
          "OpenEXR requires square pixels and a finite screen window");
    validate_attribute(compression, "compression", 1);
    validate_attribute(order, "lineOrder", 1);
    Header header{max_x + 1, max_y + 1, *compression.value.next, *order.value.next == 1, {}};
    valid(*order.value.next <= 1, "OpenEXR requires increasing or decreasing scanline order");
    valid(header.compression == compression_none || header.compression == compression_zips ||
              header.compression == compression_zip, "OpenEXR requires NONE, ZIPS or ZIP compression");
    auto list = channels.value;
    valid(list.left > 0, "Missing OpenEXR channels");
    constexpr std::array<std::string_view, 4> names{"A", "B", "G", "R"};
    auto name = list.name();
    const auto first = name == "A" ? 0u : 1u;
    for (unsigned c = first; c < names.size(); ++c) {
        valid(name == names[c], "OpenEXR requires RGB or associated RGBA channels in file order");
        const auto* entry = list.take(16);
        const auto type = load_le<std::uint32_t>(entry);
        valid((type == 1 || type == 2) && entry[4] <= 1 && !entry[5] && !entry[6] && !entry[7] &&
                  load_le<std::uint32_t>(entry + 8) == 1 && load_le<std::uint32_t>(entry + 12) == 1,
              "OpenEXR channels must be unsampled HALF or FLOAT");
        header.channel_bytes[c] = type == 1 ? 2 : 4;
        name = list.name();
    }
    valid(name.empty() && !list.left, "Unexpected OpenEXR channel data");
    return header;
}

struct Raster {
    const char* vendor;
    unsigned width, height;
    bool alpha;
    std::unique_ptr<std::uint8_t[]> raw;
    unsigned channels() const { return 3 + unsigned(alpha); }
    static constexpr unsigned sample_bytes() { return sizeof(float); }
    static constexpr std::uint32_t sample_type() { return QPX_READER_FLOAT32; }
    static const char* channel_name(unsigned channel) { return Rgba8Raster::channel_name(channel); }
    void rgba(const std::uint8_t* pixel, std::uint8_t* output) const {
        const double opacity = alpha ? load_le<float>(pixel + 3 * sizeof(float)) : 1.0;
        if (opacity == 0) {
            std::memset(output, 0, 4);
            return;
        }
        std::array<double, 3> rgb{};
        for (unsigned c = 0; c < rgb.size(); ++c)
            rgb[c] = std::max(0.0, double(load_le<float>(pixel + c * sizeof(float))));
        // Unassociation followed by peak tone mapping reduces to this divisor.
        const double divisor = opacity + *std::max_element(rgb.begin(), rgb.end());
        for (unsigned c = 0; c < rgb.size(); ++c) {
            const double value = rgb[c] / divisor;
            const double encoded = value <= 0.0031308 ? 12.92 * value :
                                  1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
            output[c] = std::uint8_t(std::clamp(std::floor(255 * encoded + 0.5), 0.0, 255.0));
        }
        output[3] = std::uint8_t(std::floor(255 * opacity + 0.5));
    }
};

float read_finite(const std::uint8_t* source, unsigned bytes) {
    if (bytes == 4) {
        const auto sample = load_le<float>(source);
        valid(std::isfinite(sample), "Non-finite OpenEXR sample is outside the linear RGB profile");
        return sample;
    }
    const unsigned code = load_le<std::uint16_t>(source);
    const auto exponent = int((code >> 10) & 31);
    valid(exponent < 31, "Non-finite OpenEXR sample is outside the linear RGB profile");
    const unsigned fraction = code & 1023;
    const auto magnitude = std::ldexp(float(exponent ? 1024 + fraction : fraction), exponent ? exponent - 25 : -24);
    return (code & 32768) ? -magnitude : magnitude;
}

void unpack_zip(std::uint8_t* packed, std::size_t length, std::uint8_t* scratch, std::uint8_t* restored,
                std::size_t size, qpx_reader_cancel_v1 cancel) {
    using namespace qpx::image_plugin;
    const auto status = decode_deflate_member(packed, length, scratch, size, DeflateWrapper::zlib,
                                              [&] { checkpoint(cancel); });
    if (status == GzipResult::workspace_failure)
        fail(QPX_READER_INTERNAL_ERROR, "OpenEXR zlib workspace allocation failed");
    valid(status == GzipResult::ok, "Invalid or incorrectly sized OpenEXR ZIP block");
    // Predictors are deltas biased by 128 in the even-then-odd byte stream.
    unsigned running = scratch[0];
    for (std::size_t i = 1; i < size; ++i) {
        running += scratch[i] ^ 128u;
        scratch[i] = std::uint8_t(running);
    }
    const auto* odds = scratch + (size + 1) / 2;
    for (std::size_t i = 0; i < size; ++i)
        restored[i] = (i % 2 ? odds : scratch)[i / 2];
}

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.openexr";
    static constexpr const char* name = "OpenEXR";
    static constexpr const char* version = QPX_OPENEXR_VERSION;
    static constexpr const char* contract = "openexr-linear-rgb-scanline/1";

    static Raster decode(std::uint8_t* bytes, std::size_t size, Budget& budget, qpx_reader_cancel_v1 cancel) {
        Input file{bytes, size};
        const auto h = parse_header(file);
        const unsigned lines = h.compression == compression_zip ? 16 : 1;
        const unsigned blocks = (h.height + lines - 1) / lines;
        const auto* offsets = file.take(8 * blocks);
        valid(file.left >= 9 * blocks, "Truncated OpenEXR scanline blocks");
        const bool alpha = h.channel_bytes[0] != 0;
        const auto first = alpha ? 0u : 1u;
        const unsigned row_bytes = h.width *
            (h.channel_bytes[0] + h.channel_bytes[1] + h.channel_bytes[2] + h.channel_bytes[3]);
        std::unique_ptr<std::uint8_t[]> scratch, restored;
        if (h.compression != compression_none) {
            constexpr auto inflate_bytes = qpx::image_plugin::gzip_workspace_bytes;
            require(budget.remaining >= inflate_bytes, "OpenEXR ZIP exceeds reader work budget");
            budget.remaining -= inflate_bytes;
            scratch = budget.array(row_bytes * std::min(lines, h.height));
            restored = budget.array(row_bytes * std::min(lines, h.height));
        }
        const auto channels = 4 - first;
        Raster raster{alpha ? "OpenEXR (linear associated RGBA)" : "OpenEXR (linear)", h.width, h.height, alpha,
                      budget.array(std::uint64_t(h.width) * h.height * channels * sizeof(float))};
        for (unsigned block = 0; block < blocks; ++block) {
            checkpoint(cancel);
            const auto index = h.descending ? blocks - 1 - block : block;
            const auto origin = index * lines;
            valid(load_le<std::uint64_t>(offsets + 8 * index) == size - file.left,
                  "OpenEXR scanline offsets do not match the declared block order");
            valid(load_le<std::uint32_t>(file.take(4)) == origin, "OpenEXR block starts on the wrong scanline");
            const auto packed_size = load_le<std::uint32_t>(file.take(4));
            const auto count = std::min(lines, h.height - origin);
            const auto expected = row_bytes * count;
            valid(packed_size && packed_size <= expected &&
                      (packed_size == expected || h.compression != compression_none),
                  "Invalid OpenEXR scanline block size");
            auto* packed = file.take(packed_size);
            const auto* cursor = static_cast<const std::uint8_t*>(packed);
            if (packed_size != expected) {
                unpack_zip(packed, packed_size, scratch.get(), restored.get(), expected, cancel);
                cursor = restored.get();
            }
            for (unsigned y = 0; y < count; ++y) {
                checkpoint(cancel);
                auto* row = raster.raw.get() + std::size_t(origin + y) * h.width * channels * sizeof(float);
                for (unsigned c = first; c < h.channel_bytes.size(); ++c) {
                    for (unsigned x = 0; x < h.width; ++x, cursor += h.channel_bytes[c]) {
                        const auto sample = read_finite(cursor, h.channel_bytes[c]);
                        valid(c != 0 || (sample >= 0 && sample <= 1), "OpenEXR alpha must be in [0, 1]");
                        std::memcpy(row + (channels * x + 3 - c) * sizeof(float), &sample, sizeof(float));
                    }
                }
            }
        }
        valid(!file.left, "Unexpected trailing OpenEXR data");
        return raster;
    }
};

using Reader = PackedImageReader<Codec, Raster>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}
