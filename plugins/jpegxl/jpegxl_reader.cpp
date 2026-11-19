// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"

#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace {
using namespace qpx_image_plugin;

struct Raster {
    const char* vendor = "JPEG XL";
    std::uint32_t width = 0, height = 0;
    unsigned bits = 0, colors = 0;
    bool alpha = false;
    std::unique_ptr<std::uint8_t[]> raw, linear_display;
    unsigned channels() const { return colors + unsigned(alpha); }
    unsigned sample_bytes() const { return bits <= 8 ? 1 : 2; }
    std::uint32_t sample_type() const { return bits <= 8 ? QPX_READER_UINT8 : QPX_READER_UINT16; }
    unsigned read_code(const std::uint8_t* value) const {
        return bits <= 8 ? *value : unsigned(value[0]) | (unsigned(value[1]) << 8);
    }
    const char* channel_name(unsigned channel) const {
        if (channel == colors) return "Alpha";
        if (colors == 1) return "Gray";
        return Rgba8Raster::channel_name(channel);
    }
    void rgba(const std::uint8_t* source, std::uint8_t* output) const {
        const auto maximum = (1u << bits) - 1;
        const auto convert = [&](unsigned channel, bool color) {
            const auto code = read_code(source + channel * sample_bytes());
            return color && linear_display ? linear_display[code] :
                std::uint8_t((code * 255 + maximum / 2) / maximum);
        };
        for (unsigned channel = 0; channel < 3; ++channel)
            output[channel] = convert(colors == 1 ? 0 : channel, true);
        output[3] = alpha ? convert(colors, false) : 255;
    }
};

struct alignas(std::max_align_t) Allocation {
    std::size_t bytes;
};
void* allocate(void* context, std::size_t size) noexcept {
    auto& budget = *static_cast<Budget*>(context);
    if (budget.remaining < sizeof(Allocation) || size > budget.remaining - sizeof(Allocation))
        return nullptr;
    const auto total = size + sizeof(Allocation);
    auto* memory = std::malloc(total);
    if (!memory) return nullptr;
    auto* header = new (memory) Allocation{total};
    budget.remaining -= total;
    return header + 1;
}
void release(void* context, void* address) noexcept {
    if (!address) return;
    auto* header = static_cast<Allocation*>(address) - 1;
    static_cast<Budget*>(context)->remaining += header->bytes;
    std::free(header);
}
void status(JxlDecoderStatus code, const char* message) {
    valid(code == JXL_DEC_SUCCESS, message);
}
struct Box {
    std::string_view type, body;
};
std::uint64_t big_endian(std::string_view word) {
    std::uint64_t result = 0;
    for (const unsigned char byte : word) result = (result << 8) | byte;
    return result;
}
Box box(std::string_view& input) {
    valid(input.size() >= 8, "JPEG XL box header is truncated");
    auto length = big_endian(input.substr(0, 4));
    const auto type = input.substr(4, 4);
    std::size_t header = 8;
    if (length == 1) {
        header = 16;
        valid(input.size() >= header, "JPEG XL box header is truncated");
        length = big_endian(input.substr(8, 8));
    } else if (length == 0) {
        length = input.size();
    }
    valid(length >= header && length <= input.size(), "Invalid JPEG XL box extent");
    const auto body = input.substr(header, std::size_t(length) - header);
    input.remove_prefix(std::size_t(length));
    return {type, body};
}
struct Codestream {
    std::string_view bytes;
    std::unique_ptr<std::uint8_t[]> assembled;
};
Codestream assemble_fragments(Box current, std::string_view input, bool out_of_order,
                              Budget& budget, qpx_reader_cancel_v1 cancel) {
    constexpr std::size_t limit = 4096;
    constexpr std::uint32_t final_flag = 0x80000000u;
    struct Fragment {
        std::uint32_t index;
        bool final;
        std::string_view bytes;
    };
    constexpr auto table_bytes = limit * sizeof(Fragment);
    require(budget.remaining >= table_bytes, "JPEG XL fragment table exceeds the reader work budget");
    budget.remaining -= table_bytes;
    std::vector<Fragment> fragments;
    fragments.reserve(limit);
    std::size_t total_bytes = 0;
    for (;;) {
        checkpoint(cancel);
        valid(current.type == "jxlp" && current.body.size() > 4,
              "JPEG XL requires nonempty codestream fragments");
        require(fragments.size() < limit, "JPEG XL fragment limit exceeded");
        const auto word = std::uint32_t(big_endian(current.body.substr(0, 4)));
        const auto index = word & ~final_flag;
        valid(index < limit && (out_of_order || index == fragments.size()),
              "Invalid JPEG XL fragment sequence");
        const auto fragment = current.body.substr(4);
        fragments.push_back({index, bool(word & final_flag), fragment});
        total_bytes += fragment.size();
        if (input.empty()) break;
        current = box(input);
    }
    if (out_of_order)
        std::sort(fragments.begin(), fragments.end(),
                  [](const Fragment& a, const Fragment& b){ return a.index < b.index; });
    for (std::size_t i = 0; i < fragments.size(); ++i) {
        valid(fragments[i].index == i, "Invalid JPEG XL fragment sequence");
        valid(fragments[i].final == (i + 1 == fragments.size()),
              "JPEG XL requires a final marker on exactly the last fragment");
    }
    auto assembled = budget.array(total_bytes);
    auto* output = assembled.get();
    constexpr std::size_t chunk = 65536;
    for (const auto& fragment : fragments) {
        for (std::size_t at = 0; at < fragment.bytes.size(); at += chunk) {
            checkpoint(cancel);
            const auto count = std::min(chunk, fragment.bytes.size() - at);
            std::memcpy(output, fragment.bytes.data() + at, count);
            output += count;
        }
    }
    budget.remaining += table_bytes;
    const std::string_view stream(reinterpret_cast<const char*>(assembled.get()), total_bytes);
    return {stream, std::move(assembled)};
}

Codestream codestream(const std::uint8_t* bytes, std::size_t size, Budget& budget,
                      qpx_reader_cancel_v1 cancel) {
    const std::string_view raw_magic("\xff\x0a", 2);
    const std::string_view signature("\0\0\0\x0cJXL \x0d\x0a\x87\x0a", 12);
    std::string_view input(reinterpret_cast<const char*>(bytes), size);
    if (input.substr(0, raw_magic.size()) == raw_magic) return {input, {}};
    valid(input.substr(0, signature.size()) == signature, "Unrecognized JPEG XL codestream or container");
    input.remove_prefix(signature.size());
    const auto ftyp = box(input);
    valid(ftyp.type == "ftyp" && ftyp.body.size() == 12 && ftyp.body.substr(0, 4) == "jxl " &&
              ftyp.body.substr(8, 4) == "jxl ",
          "Unsupported JPEG XL file type box");
    const auto minor = big_endian(ftyp.body.substr(4, 4));
    valid(minor <= 1, "Unsupported JPEG XL container version");
    auto next = box(input);
    if (next.type == "jxll") {
        valid(next.body == "\x05" || next.body == "\x0a", "Unsupported JPEG XL level box");
        next = box(input);
    }
    if (next.type == "jxlp") {
        auto stream = assemble_fragments(next, input, minor == 1, budget, cancel);
        valid(stream.bytes.substr(0, raw_magic.size()) == raw_magic, "JPEG XL fragments do not form a codestream");
        return stream;
    }
    valid(next.type == "jxlc" && next.body.substr(0, raw_magic.size()) == raw_magic && input.empty(),
          "JPEG XL container requires one complete codestream box");
    return {next.body, {}};
}

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.jpegxl";
    static constexpr const char* name = "JPEG XL";
    static constexpr const char* version = QPX_JPEGXL_VERSION;
    static constexpr const char* contract = "jpegxl-native-integer/1";

    static Raster decode(std::uint8_t* bytes, std::size_t size, Budget& budget, qpx_reader_cancel_v1 cancel,
                         std::string_view options) {
        const bool allow_xyb = options == R"({"allow_xyb":true})";
        require(allow_xyb || options.empty() || options == "{}" || options == R"({"allow_xyb":false})",
                "JPEG XL options require a boolean allow_xyb");
        const auto stream = codestream(bytes, size, budget, cancel);
        const JxlMemoryManager memory{&budget, allocate, release};
        std::unique_ptr<JxlDecoder, decltype(&JxlDecoderDestroy)> owner(JxlDecoderCreate(&memory), JxlDecoderDestroy);
        require(bool(owner), "JPEG XL decoder exceeds its workspace");
        auto* decoder = owner.get();
        status(JxlDecoderSubscribeEvents(decoder, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE),
               "Cannot subscribe to JPEG XL header and image events");
        // This also makes BasicInfo report the normalized raster dimensions.
        status(JxlDecoderSetKeepOrientation(decoder, JXL_FALSE), "Cannot normalize JPEG XL orientation");
        status(JxlDecoderSetInput(decoder, reinterpret_cast<const std::uint8_t*>(stream.bytes.data()), stream.bytes.size()),
               "Cannot submit JPEG XL input");
        JxlDecoderCloseInput(decoder);
        Raster raster;
        bool color = false, complete = false, xyb = false;
        for (;;) {
            checkpoint(cancel);
            switch (JxlDecoderProcessInput(decoder)) {
            case JXL_DEC_BASIC_INFO: {
                JxlBasicInfo info{};
                status(JxlDecoderGetBasicInfo(decoder, &info), "JPEG XL basic info is missing");
                valid(info.xsize && info.ysize && info.bits_per_sample && info.bits_per_sample <= 16 &&
                          !info.exponent_bits_per_sample && !info.have_animation && !info.have_preview &&
                          info.intensity_target > 0 && info.intensity_target <= 255 &&
                          info.orientation == JXL_ORIENT_IDENTITY &&
                          (info.num_color_channels == 1 || info.num_color_channels == 3) &&
                          info.num_extra_channels == unsigned(info.alpha_bits != 0) &&
                          (!info.alpha_bits || (info.alpha_bits == info.bits_per_sample &&
                                              !info.alpha_exponent_bits && !info.alpha_premultiplied)),
                      "Unsupported JPEG XL sample, channel, geometry, or animation profile");
                xyb = !info.uses_original_profile;
                if (xyb) {
                    if (!allow_xyb)
                        fail(QPX_READER_UNSUPPORTED, "JPEG XL XYB requires explicit allow_xyb");
                    raster.vendor = "JPEG XL (decoded sRGB)";
                }
                raster.width = info.xsize;
                raster.height = info.ysize;
                raster.bits = info.bits_per_sample;
                raster.colors = info.num_color_channels;
                raster.alpha = info.alpha_bits != 0;
                break;
            }
            case JXL_DEC_COLOR_ENCODING: {
                JxlColorEncoding encoding{};
                status(JxlDecoderGetColorAsEncodedProfile(decoder, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &encoding),
                       "JPEG XL profile requires a structured sRGB or linear sRGB encoding");
                const bool linear = encoding.transfer_function == JXL_TRANSFER_FUNCTION_LINEAR;
                valid(encoding.color_space == (raster.colors == 1 ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB) &&
                          encoding.white_point == JXL_WHITE_POINT_D65 &&
                          (raster.colors == 1 || encoding.primaries == JXL_PRIMARIES_SRGB) &&
                          (linear || encoding.transfer_function == JXL_TRANSFER_FUNCTION_SRGB),
                      "JPEG XL profile requires a structured sRGB or linear sRGB encoding");
                if (xyb) {
                    valid(!linear, "JPEG XL XYB profile requires an sRGB transfer function");
                    status(JxlDecoderSetOutputColorProfile(decoder, &encoding, nullptr, 0),
                           "Cannot configure decoded JPEG XL sRGB output");
                }
                if (linear) {
                    const auto maximum = (1u << raster.bits) - 1;
                    raster.linear_display = budget.array(maximum + 1);
                    for (unsigned code = 0; code <= maximum; ++code) {
                        if (!(code & 4095)) checkpoint(cancel);
                        const auto t = double(code) / maximum;
                        const auto display = t <= 0.0031308 ? 12.92 * t : 1.055 * std::pow(t, 1.0 / 2.4) - 0.055;
                        raster.linear_display[code] = std::uint8_t(std::floor(255 * display + 0.5));
                    }
                }
                color = true;
                break;
            }
            case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
                valid(color && !raster.raw, "JPEG XL profile requires one color-admitted frame");
                const JxlPixelFormat format{raster.channels(), raster.sample_bytes() == 1 ? JXL_TYPE_UINT8 : JXL_TYPE_UINT16,
                                            JXL_LITTLE_ENDIAN, 0};
                const auto stride = raster.channels() * raster.sample_bytes();
                require(raster.width <= work_limit / stride && raster.height <= work_limit / (std::uint64_t(raster.width) * stride),
                        "JPEG XL raster exceeds the reader work budget");
                std::size_t output_size = 0;
                status(JxlDecoderImageOutBufferSize(decoder, &format, &output_size), "JPEG XL raster size is unavailable");
                valid(output_size == std::uint64_t(raster.width) * raster.height * stride, "JPEG XL output extent differs");
                raster.raw = budget.array(output_size);
                status(JxlDecoderSetImageOutBuffer(decoder, &format, raster.raw.get(), output_size),
                       "Cannot configure JPEG XL output");
                const JxlBitDepth depth{JXL_BIT_DEPTH_FROM_CODESTREAM, 0, 0};
                status(JxlDecoderSetImageOutBitDepth(decoder, &depth), "Cannot retain JPEG XL sample code range");
                break;
            }
            case JXL_DEC_FULL_IMAGE: {
                valid(raster.raw && !complete, "JPEG XL returned an unsupported frame sequence");
                const auto stride = raster.sample_bytes(), maximum = (1u << raster.bits) - 1;
                if (raster.bits != stride * 8) {
                    const auto count = std::uint64_t(raster.width) * raster.height * raster.channels();
                    for (std::uint64_t i = 0; i < count; ++i) {
                        if (!(i & 4095)) checkpoint(cancel);
                        valid(raster.read_code(raster.raw.get() + i * stride) <= maximum,
                              "JPEG XL code exceeds the declared bit depth");
                    }
                }
                complete = true;
                break;
            }
            case JXL_DEC_SUCCESS:
                valid(complete && JxlDecoderReleaseInput(decoder) == 0, "JPEG XL is incomplete or has trailing bytes");
                return raster;
            default:
                fail(QPX_READER_IO_ERROR, "JPEG XL decode failed or input is truncated");
            }
        }
    }
};
using Reader = PackedImageReader<Codec, Raster, false, false, true>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}
