// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include <avif/avif.h>

namespace {
using namespace qpx_image_plugin;
constexpr unsigned dimension_limit = 2048;
constexpr auto input_limit = 32u * 1024 * 1024;

void status(avifResult result, const char* message) {
    valid(result == AVIF_RESULT_OK, message);
}
struct Raster {
    const char* vendor = "AVIF";
    std::uint32_t width = 0, height = 0;
    unsigned bits = 0, colors = 0;
    bool alpha = false;
    std::unique_ptr<std::uint8_t[]> raw;
    unsigned channels() const { return colors + unsigned(alpha); }
    unsigned sample_bytes() const { return bits == 8 ? 1 : 2; }
    std::uint32_t sample_type() const { return bits == 8 ? QPX_READER_UINT8 : QPX_READER_UINT16; }
    const char* channel_name(unsigned channel) const {
        if (channel == colors) return "Alpha";
        if (colors == 1) return "Gray";
        return Rgba8Raster::channel_name(channel);
    }
    void rgba(const std::uint8_t* source, std::uint8_t* output) const {
        const auto limit = (1u << bits) - 1;
        const auto display = [&](unsigned channel) {
            const auto* sample = source + channel * sample_bytes();
            const auto code = bits == 8 ? unsigned(sample[0]) : unsigned(sample[0]) | (unsigned(sample[1]) << 8);
            return std::uint8_t((code * 255 + limit / 2) / limit);
        };
        for (unsigned channel = 0; channel < 3; ++channel)
            output[channel] = display(colors == 1 ? 0 : channel);
        output[3] = alpha ? display(colors) : 255;
    }
};

void admit(const avifImage& image) {
    valid(image.width && image.height && image.width <= dimension_limit && image.height <= dimension_limit &&
              (image.depth == 8 || image.depth == 10 || image.depth == 12),
          "AVIF dimensions or bit depth exceed the integer still profile");
    valid(image.colorPrimaries == AVIF_COLOR_PRIMARIES_BT709 &&
              image.transferCharacteristics == AVIF_TRANSFER_CHARACTERISTICS_SRGB &&
              !image.icc.size && !image.gainMap && !image.clli.maxCLL && !image.clli.maxPALL,
          "AVIF requires SDR sRGB without ICC or HDR metadata");
    valid(image.transformFlags == AVIF_TRANSFORM_NONE && !image.exif.size && !image.alphaPremultiplied,
          "AVIF geometry, Exif or premultiplied alpha is outside the still profile");
}

void copy_samples(const avifImage& image, Raster& raster, qpx_reader_cancel_v1 cancel) {
    const unsigned sample_bytes = raster.sample_bytes();
    constexpr unsigned identity_planes[] = {AVIF_CHAN_V, AVIF_CHAN_Y, AVIF_CHAN_U};
    std::array<const std::uint8_t*, 4> planes{};
    std::array<unsigned, 4> strides{};
    for (unsigned c = 0; c < raster.colors; ++c) {
        const auto channel = raster.colors == 1 ? AVIF_CHAN_Y : identity_planes[c];
        planes[c] = image.yuvPlanes[channel];
        strides[c] = image.yuvRowBytes[channel];
    }
    if (raster.alpha) {
        planes[raster.colors] = image.alphaPlane;
        strides[raster.colors] = image.alphaRowBytes;
    }
    const auto channels = raster.channels();
    for (unsigned c = 0; c < channels; ++c)
        valid(planes[c] && strides[c] >= raster.width * sample_bytes, "AVIF plane extent is incomplete");
    auto* output = raster.raw.get();
    for (unsigned y = 0; y < raster.height; ++y) {
        checkpoint(cancel);
        for (unsigned x = 0; x < raster.width; ++x)
            for (unsigned c = 0; c < channels; ++c) {
                const auto* sample = planes[c] + std::size_t(y) * strides[c] + x * sample_bytes;
                std::memcpy(output, sample, sample_bytes);
                output += sample_bytes;
            }
    }
}

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.avif";
    static constexpr const char* name = "AVIF";
    static constexpr const char* version = QPX_AVIF_VERSION;
    static constexpr const char* contract = "avif-integer-sdr-still/1";

    static Raster decode(std::uint8_t* bytes, std::size_t size, Budget& budget, qpx_reader_cancel_v1 cancel,
                         std::string_view options) {
        const bool allow_yuv = options == R"({"allow_yuv":true})";
        require(allow_yuv || options.empty() || options == "{}" || options == R"({"allow_yuv":false})",
                "AVIF options require a boolean allow_yuv");
        require(size <= input_limit, "AVIF encoded input exceeds 32 MiB");
        const avifROData body{bytes, size};
        valid(avifPeekCompatibleFileType(&body), "Unrecognized AVIF file type");
        const auto pixels = std::min<std::uint64_t>(dimension_limit * dimension_limit, budget.remaining / 64);
        require(pixels > 0, "AVIF workspace is too small");
        std::unique_ptr<avifDecoder, decltype(&avifDecoderDestroy)> decoder(avifDecoderCreate(), avifDecoderDestroy);
        require(bool(decoder), "Cannot create AVIF decoder");
        decoder->codecChoice = AVIF_CODEC_CHOICE_AOM;
        decoder->maxThreads = 1;
        decoder->imageDimensionLimit = dimension_limit;
        decoder->imageSizeLimit = static_cast<std::uint32_t>(pixels);
        decoder->imageCountLimit = 1;
        decoder->imageContentToDecode = AVIF_IMAGE_CONTENT_ALL;
        status(avifDecoderSetIOMemory(decoder.get(), bytes, size), "Cannot configure AVIF input");
        checkpoint(cancel);
        status(avifDecoderParse(decoder.get()), "Invalid AVIF container or image count");
        valid(decoder->image && decoder->imageCount == 1 && !decoder->imageSequenceTrackPresent &&
                  decoder->progressiveState == AVIF_PROGRESSIVE_STATE_UNAVAILABLE,
              "AVIF profile requires one non-progressive still");
        admit(*decoder->image);
        checkpoint(cancel);
        status(avifDecoderNextImage(decoder.get()), "AVIF image decode failed or the data is truncated");
        checkpoint(cancel);
        const auto& image = *decoder->image;
        admit(image);
        Raster raster;
        raster.width = image.width;
        raster.height = image.height;
        raster.bits = image.depth;
        raster.colors = image.yuvFormat == AVIF_PIXEL_FORMAT_YUV400 ? 1 : 3;
        raster.alpha = image.alphaPlane != nullptr;
        valid(raster.alpha == bool(decoder->alphaPresent), "AVIF alpha plane is missing");
        const bool identity = image.yuvRange == AVIF_RANGE_FULL &&
            (raster.colors == 1 || (image.yuvFormat == AVIF_PIXEL_FORMAT_YUV444 &&
                                   image.matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY));
        if (!identity) {
            if (!allow_yuv)
                fail(QPX_READER_UNSUPPORTED, "AVIF YUV reconstruction requires explicit allow_yuv");
            valid(raster.colors == 3 && (image.matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_BT601 ||
                  image.matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_BT709 ||
                  image.matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_BT470BG) &&
                  (image.yuvFormat == AVIF_PIXEL_FORMAT_YUV444 ||
                   image.yuvChromaSamplePosition == AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN),
                  "AVIF YUV matrix or chroma position is outside the sRGB reconstruction profile");
            raster.vendor = "AVIF (decoded sRGB)";
        }
        const auto row_bytes = raster.width * raster.channels() * raster.sample_bytes();
        raster.raw = budget.array(std::uint64_t(row_bytes) * raster.height);
        if (identity) {
            copy_samples(image, raster, cancel);
        } else {
            avifRGBImage rgb{};
            avifRGBImageSetDefaults(&rgb, &image);
            rgb.format = raster.alpha ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;
            rgb.avoidLibYUV = AVIF_TRUE;
            rgb.chromaUpsampling = AVIF_CHROMA_UPSAMPLING_BILINEAR;
            rgb.pixels = raster.raw.get();
            rgb.rowBytes = row_bytes;
            status(avifImageYUVToRGB(&image, &rgb), "AVIF YUV conversion failed");
        }
        checkpoint(cancel);
        return raster;
    }
};
using Reader = PackedImageReader<Codec, Raster, false, false, true>;
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}
