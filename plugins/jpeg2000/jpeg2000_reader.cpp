// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "framing.hpp"
#include <jasper/jasper.h>
#include <chrono>
#include <mutex>

namespace {
using namespace qpx_image_plugin;
using qpx_jpeg2000::parse_image;

struct Raster {
    std::int64_t width, height;
    const char* vendor;
    unsigned components;
    unsigned code_span;
    int min_code;
    double pixel_width_um, pixel_height_um;
    std::unique_ptr<std::uint8_t[]> raw;

    unsigned channels() const { return components; }
    bool has_alpha() const { return components == 2 || components == 4; }
    unsigned sample_bytes() const { return code_span > 255 ? 2 : 1; }
    std::uint32_t sample_type() const {
        if (min_code < 0)
            return sample_bytes() == 1 ? QPX_READER_INT8 : QPX_READER_INT16;
        return sample_bytes() == 1 ? QPX_READER_UINT8 : QPX_READER_UINT16;
    }
    void publish_calibration(qpx_reader_metadata_v1& metadata) const {
        if (pixel_width_um > 0 && pixel_height_um > 0) {
            metadata.calibration_flags = QPX_READER_CALIBRATION_X | QPX_READER_CALIBRATION_Y;
            metadata.pixel_width_um = pixel_width_um;
            metadata.pixel_height_um = pixel_height_um;
        }
    }
    const char* channel_name(unsigned channel) const {
        constexpr const char* rgb[] = {"Red", "Green", "Blue"};
        if (has_alpha() && channel == components - 1)
            return "Alpha";
        return components < 3 ? "Gray" : rgb[channel];
    }
    std::uint8_t display_code(const std::uint8_t* sample) const {
        const auto wide = sample_bytes() == 2;
        const int bits = wide ? int(sample[1]) * 256 + sample[0] : sample[0];
        const auto midpoint = wide ? 32768 : 128;
        const auto code = min_code < 0 && bits >= midpoint ? bits - 2 * midpoint : bits;
        const auto offset = unsigned(code - min_code);
        return static_cast<std::uint8_t>((offset * 255 + code_span / 2) / code_span);
    }
    void rgba(const std::uint8_t* source, std::uint8_t* target) const {
        const auto stride = sample_bytes();
        for (unsigned channel = 0; channel < 3; ++channel)
            target[channel] = display_code(source + (components < 3 ? 0 : channel * stride));
        target[3] = has_alpha() ? display_code(source + (components - 1) * stride) : 255;
    }
};

std::timed_mutex codec_lock;
thread_local bool decode_warning = false;

int record_log(jas_logtype_t type, const char*, va_list) {
    const auto severity = jas_logtype_getclass(type);
    if (severity == JAS_LOGTYPE_CLASS_WARN || severity == JAS_LOGTYPE_CLASS_ERROR)
        decode_warning = true;
    return 0;
}

// The private codec is built without threads. Hold the lock through cleanup
// because the configuration and library state are process-global to the module.
class DecoderSession {
    std::unique_lock<std::timed_mutex> lock_{codec_lock, std::defer_lock};

  public:
    DecoderSession(std::size_t memory, std::size_t samples, qpx_reader_cancel_v1 cancel) {
        while (!lock_.try_lock_for(std::chrono::milliseconds(20)))
            checkpoint(cancel);
        checkpoint(cancel);
        jas_conf_clear();
        jas_conf_set_multithread(0);
        jas_conf_set_max_mem_usage(memory);
        jas_conf_set_dec_default_max_samples(samples);
        jas_conf_set_vlogmsgf(record_log);
        if (jas_init_library())
            fail(QPX_READER_INTERNAL_ERROR, "Cannot initialize bounded JPEG 2000 codec");
        if (jas_init_thread()) {
            jas_cleanup_library();
            fail(QPX_READER_INTERNAL_ERROR, "Cannot initialize JPEG 2000 codec context");
        }
    }
    ~DecoderSession() {
        jas_cleanup_thread();
        jas_cleanup_library();
    }
};

struct Jpeg2000 {
    static constexpr const char* provider_id = "org.qupathxx.jpeg2000";
    static constexpr const char* name = "JPEG 2000";
    static constexpr const char* version = QPX_JPEG2000_VERSION;
    static constexpr const char* contract = "jpeg2000-native-samples/2";

    static Raster decode(std::uint8_t* bytes, std::size_t size, Budget& budget,
                         qpx_reader_cancel_v1 cancel) {
        const auto input = parse_image({reinterpret_cast<char*>(bytes), size}, cancel);
        const auto samples = std::size_t(input.width) * input.height * input.channels;
        const auto* vendor = input.boxed ? (input.irreversible ? "JP2 (irreversible)" : "JP2")
                                         : (input.irreversible ? "J2K (irreversible)" : "J2K");
        Raster result{input.width, input.height, vendor,
                      input.channels, (1u << input.precision) - 1,
                      input.signed_samples ? -(1 << (input.precision - 1)) : 0,
                      input.pixel_width_um, input.pixel_height_um, {}};
        const auto sample_bytes = result.sample_bytes();
        result.raw = budget.array(samples * sample_bytes);
        require(budget.remaining >= 4096 && input.width * sizeof(long) <= budget.remaining - 4096,
                "JPEG 2000 work budget is too small for decoding");
        budget.remaining -= input.width * sizeof(long) + 32;
        auto row = std::make_unique<long[]>(input.width);
        // JasPer's tile-count guard derives its bound from 16 x 16 minimum
        // tiles. Framing has already committed the complete grid and raster size.
        DecoderSession session(static_cast<std::size_t>(budget.remaining),
                               std::max(samples, std::size_t(input.tiles) * 256), cancel);
        const auto* codec = jas_image_lookupfmtbyid(jas_image_strtofmt("jpc"));
        require(codec && codec->ops.decode, "Private JPEG 2000 codestream codec is unavailable");
        // JasPer's memory-stream API takes a writable pointer even for decoding.
        auto* data = const_cast<char*>(input.codestream.data());
        std::unique_ptr<jas_stream_t, decltype(&jas_stream_close)> stream(
            jas_stream_memopen(data, input.codestream.size()), jas_stream_close);
        valid(stream != nullptr, "Cannot open bounded JPEG 2000 codestream");
        checkpoint(cancel);
        decode_warning = false;
        // Decode the codestream directly. The generic image wrapper attaches a synthetic
        // ICC profile. JP2 framing already established the color interpretation.
        std::unique_ptr<jas_image_t, decltype(&jas_image_destroy)> image(
            codec->ops.decode(stream.get(), nullptr), jas_image_destroy);
        checkpoint(cancel);
        valid(image && !decode_warning, "JPEG 2000 codec refused the codestream");
        valid(jas_stream_tell(stream.get()) == static_cast<long>(input.codestream.size()),
              "JPEG 2000 codec did not consume the complete codestream");
        auto* raster = image.get();
        valid(jas_image_numcmpts(raster) == input.channels, "JPEG 2000 decoded component count differs");
        const auto pixel_bytes = input.channels * sample_bytes;
        const auto maximum = result.min_code + static_cast<long>(result.code_span);
        for (unsigned channel = 0; channel < input.channels; ++channel) {
            valid(jas_image_cmptwidth(raster, channel) == static_cast<jas_image_coord_t>(input.width) &&
                      jas_image_cmptheight(raster, channel) == static_cast<jas_image_coord_t>(input.height) &&
                      jas_image_cmptprec(raster, channel) == input.precision &&
                      bool(jas_image_cmptsgnd(raster, channel)) == input.signed_samples &&
                      !jas_image_cmpttlx(raster, channel) &&
                      !jas_image_cmpttly(raster, channel) && jas_image_cmpthstep(raster, channel) == 1 &&
                      jas_image_cmptvstep(raster, channel) == 1,
                  "JPEG 2000 decoded component disagrees with the declared raster");
            for (std::uint32_t y = 0; y < input.height; ++y) {
                checkpoint(cancel);
                valid(!jas_image_readcmpt2(raster, channel, 0, y, input.width, 1, row.get()),
                      "JPEG 2000 component row is unreadable");
                auto* row_output = result.raw.get() + std::size_t(y) * input.width * pixel_bytes +
                                   channel * sample_bytes;
                for (std::uint32_t x = 0; x < input.width; ++x) {
                    auto* output = row_output + x * pixel_bytes;
                    const auto code = row[x];
                    valid(code >= result.min_code && code <= maximum, "JPEG 2000 decoded sample is out of range");
                    const auto bits = static_cast<std::uint16_t>(code);
                    output[0] = static_cast<std::uint8_t>(bits);
                    if (sample_bytes == 2)
                        output[1] = static_cast<std::uint8_t>(bits >> 8);
                }
            }
        }
        return result;
    }
};
} // namespace

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return PackedImageReader<Jpeg2000, Raster, true>::entry(abi, out, error);
}
