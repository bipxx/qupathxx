// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
// Private module helper for single-level packed image sources.
// Codec implementations own interpretation; the existing C ABI/host still own
// transfer, source identity, saved projects, scientific admission and publication.
#include <qupathxx/reader_plugin.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace qpx_image_plugin {
inline constexpr std::uint64_t work_limit = UINT64_C(256) * 1024 * 1024;
struct Failure {
    qpx_reader_status_v1 status;
    const char* message;
};
[[noreturn]] inline void fail(qpx_reader_status_v1 status, const char* message) {
    throw Failure{status, message};
}
inline void require(bool condition, const char* message) {
    if (!condition)
        fail(QPX_READER_INVALID_REQUEST, message);
}
inline void valid(bool condition, const char* message) {
    if (!condition)
        fail(QPX_READER_IO_ERROR, message);
}
inline void checkpoint(qpx_reader_cancel_v1 cancel) {
    if (cancel.is_cancelled && cancel.is_cancelled(cancel.context))
        fail(QPX_READER_CANCELLED, "Image operation cancelled");
}
inline bool text(qpx_reader_text_v1& output, const char* value) noexcept {
    const auto length = std::strlen(value);
    output.size = length;
    if (output.capacity < length || (length && !output.data))
        return false;
    if (length)
        std::memcpy(output.data, value, length);
    return true;
}
inline qpx_reader_status_v1 failure(qpx_reader_text_v1* error, qpx_reader_status_v1 status,
                                    const char* message) noexcept {
    return error && !text(*error, message) ? QPX_READER_BUFFER_TOO_SMALL : status;
}
template <class F> qpx_reader_status_v1 boundary(qpx_reader_text_v1* error, F&& action) noexcept {
    if (error)
        error->size = 0;
    try {
        return action();
    } catch (const Failure& e) {
        return failure(error, e.status, e.message);
    } catch (const std::bad_alloc&) {
        return failure(error, QPX_READER_INTERNAL_ERROR, "Image allocation failed");
    } catch (const std::filesystem::filesystem_error& e) {
        return failure(error, QPX_READER_IO_ERROR, e.what());
    } catch (const std::invalid_argument& e) {
        return failure(error, QPX_READER_INVALID_REQUEST, e.what());
    } catch (const std::exception& e) {
        return failure(error, QPX_READER_INTERNAL_ERROR, e.what());
    } catch (...) {
        return failure(error, QPX_READER_INTERNAL_ERROR, "Image plugin internal failure");
    }
}
struct Budget {
    std::uint64_t remaining;
    std::unique_ptr<std::uint8_t[]> array(std::uint64_t bytes) {
        constexpr std::uint64_t bookkeeping = 32;
        require(bytes <= remaining && bookkeeping <= remaining - bytes,
                "Decoded image exceeds reader work budget");
        remaining -= bytes + bookkeeping;
        return std::unique_ptr<std::uint8_t[]>(new std::uint8_t[std::size_t(bytes)]);
    }
};
struct Rgba8Raster {
    static constexpr unsigned channels() { return 4; }
    unsigned sample_bytes() const { return 1; }
    std::uint32_t sample_type() const { return QPX_READER_UINT8; }
    static const char* channel_name(unsigned channel) {
        const char* names[] = {"Red", "Green", "Blue", "Alpha"};
        return names[channel];
    }
    std::int64_t width = 0, height = 0;
    const char* vendor = "";
    std::unique_ptr<std::uint8_t[]> raw;
    std::array<std::uint8_t, 256> display{};
    void rgba(const std::uint8_t* sample, std::uint8_t* target) const {
        for (unsigned c = 0; c < 3; ++c)
            target[c] = display[sample[c]];
        target[3] = sample[3];
    }
    void display_transfer(bool linear) {
        for (unsigned i = 0; i < display.size(); ++i) {
            const double value = double(i) / 255;
            const double encoded = !linear ? value
                                   : value <= 0.0031308
                                       ? 12.92 * value
                                       : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
            display[i] = std::uint8_t(std::clamp(std::floor(255 * encoded + 0.5), 0.0, 255.0));
        }
    }
};
// The codec provides provider_id/name/version/contract and decode(input, size,
// budget, cancel). Options adds a final string_view argument. A volume codec
// supplies input_limit and its raster supplies z_planes. All owned arrays use Budget.
template <class Codec, class Raster = Rgba8Raster, bool Calibrated = false, bool Volume = false,
          bool Options = Volume, bool Compound = false, bool SampleValidity = false>
class PackedImageReader {
    struct Handle {
        Raster image;
    };
    static const Raster& image(void* handle) {
        require(handle != nullptr, "Image handle is null");
        return static_cast<Handle*>(handle)->image;
    }
    static qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out,
                                                         qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            require(out && out->struct_size >= sizeof(*out), "Reader info structure is too small");
            out->abi_version = QPX_READER_ABI_V1;
            out->capabilities = QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW;
            if constexpr (Compound)
                out->capabilities |= QPX_READER_CAP_SOURCE_FILES;
            bool complete = text(out->provider_id, Codec::provider_id);
            complete = text(out->display_name, Codec::name) && complete;
            complete = text(out->version, Codec::version) && complete;
            complete = text(out->contract, Codec::contract) && complete;
            return complete ? QPX_READER_OK
                            : failure(error, QPX_READER_BUFFER_TOO_SMALL,
                                      "Reader descriptor text buffer is too small");
        });
    }
    static qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,
                                                     void** output,
                                                     qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            require(output != nullptr, "Image output handle is null");
            *output = nullptr;
            require(request && request->struct_size >= sizeof(*request),
                    "Image open structure is too small");
            require(!request->reserved &&
                        (Options ? (!request->options.size ||
                                       (request->options.data && request->options.size <= 4096))
                                : !request->options.size),
                    "Image options or reserved field is invalid");
            require(request->work_bytes && request->work_bytes <= work_limit,
                    "Image work budget must be1 byte..256MiB");
            require(request->path.data && request->path.size && request->path.size <= 32768,
                    "Image path is invalid");
            const auto overhead = UINT64_C(16384) + 8 * request->path.size + sizeof(Handle);
            require(request->work_bytes > overhead,
                    "Image work budget is too small for opening overhead");
            checkpoint(request->cancel);
            const std::string path(request->path.data, std::size_t(request->path.size));
            require(path.find('\0') == std::string::npos, "Image path contains NUL");
            const auto native = std::filesystem::u8path(path);
            require(native.is_absolute(), "Image path must be absolute");
            Budget budget{request->work_bytes - overhead};
            auto handle = std::make_unique<Handle>();
            const std::string_view options(request->options.data ? request->options.data : "",
                                           std::size_t(request->options.size));
            if constexpr (Compound) {
                if constexpr (Options)
                    handle->image = Codec::decode_compound(native, budget, request->cancel, options);
                else
                    handle->image = Codec::decode_compound(native, budget, request->cancel);
            } else {
                const auto encoded = std::filesystem::file_size(native);
                require(encoded <= request->work_bytes, "Encoded image exceeds reader work budget");
                if constexpr (Volume)
                    require(encoded <= Codec::input_limit, "Encoded volume exceeds the format input limit");
                auto input = budget.array(encoded);
                std::unique_ptr<std::FILE, decltype(&std::fclose)> file(_wfopen(native.c_str(), L"rb"),
                                                                        std::fclose);
                if (!file)
                    fail(QPX_READER_IO_ERROR, "Cannot open image input");
                if (std::setvbuf(file.get(), nullptr, _IONBF, 0))
                    fail(QPX_READER_IO_ERROR, "Cannot configure bounded image input");
                for (std::uint64_t at = 0; at < encoded;) {
                    checkpoint(request->cancel);
                    const auto count = std::size_t(std::min<std::uint64_t>(65536, encoded - at));
                    valid(std::fread(input.get() + at, 1, count, file.get()) == count,
                          "Image input is truncated or changed");
                    at += count;
                }
                valid(std::fgetc(file.get()) == EOF && !std::ferror(file.get()),
                      "Image input changed during reading");
                if constexpr (Options) {
                    handle->image =
                        Codec::decode(input.get(), std::size_t(encoded), budget, request->cancel, options);
                } else {
                    handle->image =
                        Codec::decode(input.get(), std::size_t(encoded), budget, request->cancel);
                }
            }
            checkpoint(request->cancel);
            *output = handle.release();
            return QPX_READER_OK;
        });
    }
    static void QPX_READER_CALL destroy(void* handle) noexcept {
        try {
            delete static_cast<Handle*>(handle);
        } catch (...) {
        }
    }
    static qpx_reader_status_v1 QPX_READER_CALL metadata(void* value, qpx_reader_metadata_v1* out,
                                                         qpx_reader_cancel_v1 cancel,
                                                         qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            const auto& source = image(value);
            require(out && out->struct_size >= sizeof(*out),
                    "Image metadata structure is too small");
            checkpoint(cancel);
            out->level_count = out->has_raw = out->z_planes = out->timepoints = out->series_count =
                1;
            out->width = source.width;
            out->height = source.height;
            out->channels = source.channels();
            out->sample_type = source.sample_type();
            if constexpr (Volume)
                out->z_planes = source.z_planes;
            out->series = 0;
            out->calibration_flags = 0;
            out->pixel_width_um = out->pixel_height_um = out->z_spacing_um =
                out->time_interval_seconds = 0;
            if constexpr (Calibrated)
                source.publish_calibration(*out);
            bool complete = text(out->vendor, source.vendor);
            const char* order = "XYCZT";
            if constexpr (Volume)
                order = source.channels() > 1 ? (source.z_planes > 1 ? "CXYZ" : "CXY") :
                                                 (source.z_planes > 1 ? "XYZ" : "XY");
            complete = text(out->dimension_order, order) && complete;
            return complete ? QPX_READER_OK
                            : failure(error, QPX_READER_BUFFER_TOO_SMALL,
                                      "Image metadata text buffer is too small");
        });
    }
    static qpx_reader_status_v1 QPX_READER_CALL level(void* value, std::uint32_t index,
                                                      qpx_reader_level_v1* out,
                                                      qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            const auto& source = image(value);
            require(out && out->struct_size >= sizeof(*out) && !index,
                    "Image level must be level zero with a complete descriptor");
            out->reserved = 0;
            out->width = source.width;
            out->height = source.height;
            out->downsample = 1;
            return QPX_READER_OK;
        });
    }
    static qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value, std::uint32_t channel,
                                                             qpx_reader_text_v1* out,
                                                             qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            const auto& source = image(value);
            require(out && channel < source.channels(), "Image channel/name output is invalid");
            return text(*out, source.channel_name(channel))
                       ? QPX_READER_OK
                       : failure(error, QPX_READER_BUFFER_TOO_SMALL,
                                 "Image channel-name text buffer is too small");
        });
    }
    static std::uint64_t region(const qpx_reader_region_v1* request, unsigned stride, bool raw) {
        require(request && request->struct_size >= sizeof(*request),
                "Image region structure is too small");
        require(!request->level && !request->t, "Image has only level zero and T=0");
        require(request->width > 0 && request->height > 0,
                "Image region dimensions must be positive");
        const auto width = std::uint64_t(request->width), height = std::uint64_t(request->height);
        const auto bytes = stride + unsigned(raw);
        require(width <= work_limit / bytes && height <= work_limit / (width * bytes),
                "Image region output exceeds256MiB");
        const auto limit = std::numeric_limits<std::int64_t>::max();
        require(request->x <= limit - request->width && request->y <= limit - request->height,
                "Image region coordinate overflow");
        return width * height;
    }
    static qpx_reader_status_v1 read(void* value, const qpx_reader_region_v1* request,
                                     qpx_reader_pixels_v1* out, bool raw,
                                     qpx_reader_cancel_v1 cancel,
                                     qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            const auto& source = image(value);
            require(out && out->struct_size >= sizeof(*out), "Image pixel structure is too small");
            out->written = out->validity_written = 0;
            require(request && request->struct_size >= sizeof(*request),
                    "Image region structure is too small");
            std::uint32_t planes = 1;
            if constexpr (Volume)
                planes = source.z_planes;
            require(request->z < (raw ? planes : 1), "Image Z plane is invalid");
            const auto source_channels = source.channels();
            if (raw)
                require(request->channels && request->first_channel < source_channels &&
                            request->channels <= source_channels - request->first_channel,
                        "Image raw channel range is invalid");
            else
                require(!request->first_channel && !request->channels,
                        "RGBA request has quantitative channel selectors");
            const unsigned channels = raw ? request->channels : 4;
            const auto sample_bytes = source.sample_bytes();
            const auto channel_stride = raw ? sample_bytes : 1u;
            const auto stride = channels * channel_stride;
            const auto count = region(request, stride, raw), bytes = count * stride;
            require(out->sample_type == (raw ? source.sample_type() : QPX_READER_UINT8) &&
                        out->width == request->width &&
                        out->height == request->height && out->channels == channels &&
                        !out->reserved && out->channel_stride == channel_stride &&
                        out->pixel_stride == stride &&
                        out->row_stride == std::uint64_t(request->width) * stride,
                    "Image pixels require exact packed HWC layout");
            require(out->data && out->capacity >= bytes &&
                        (raw ? out->validity && out->validity_capacity >= count
                             : !out->validity && !out->validity_capacity),
                    "Image pixel/validity capacity is invalid");
            checkpoint(cancel);
            std::fill_n(out->data, std::size_t(bytes), std::uint8_t(0));
            if (raw)
                std::fill_n(out->validity, std::size_t(count), std::uint8_t(0));
            for (std::int64_t y = 0; y < request->height; ++y) {
                checkpoint(cancel);
                for (std::int64_t x = 0; x < request->width; ++x) {
                    if (!(x & 4095))
                        checkpoint(cancel);
                    const auto sx = request->x + x, sy = request->y + y;
                    if (sx < 0 || sy < 0 || sx >= source.width || sy >= source.height)
                        continue;
                    const auto from = ((std::uint64_t(request->z) * source.height + sy) * source.width + sx) *
                                      source_channels * sample_bytes;
                    const auto pixel = std::uint64_t(y) * request->width + x;
                    auto* target = out->data + pixel * stride;
                    if (raw) {
                        const auto* samples = source.raw.get() + from + request->first_channel * sample_bytes;
                        if constexpr (SampleValidity)
                            if (!source.valid_samples(samples, channels)) continue;
                        std::memcpy(target, samples, stride);
                        out->validity[pixel] = 1;
                    } else {
                        source.rgba(source.raw.get() + from, target);
                    }
                }
            }
            checkpoint(cancel);
            out->written = bytes;
            out->validity_written = raw ? count : 0;
            return QPX_READER_OK;
        });
    }
    static qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,
                                                          const qpx_reader_region_v1* request,
                                                          qpx_reader_pixels_v1* out,
                                                          qpx_reader_text_v1* error) noexcept {
        return read(value, request, out, false, {}, error);
    }
    static qpx_reader_status_v1 QPX_READER_CALL read_raw(void* value,
                                                         const qpx_reader_region_v1* request,
                                                         qpx_reader_pixels_v1* out,
                                                         qpx_reader_cancel_v1 cancel,
                                                         qpx_reader_text_v1* error) noexcept {
        return read(value, request, out, true, cancel, error);
    }

  public:
    static qpx_reader_status_v1 entry(std::uint32_t version, qpx_reader_api_v1* out,
                                      qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            require(version == QPX_READER_ABI_V1 && out && out->struct_size >= sizeof(*out),
                    "Unsupported image ABI or short table");
            *out = {sizeof(*out), QPX_READER_ABI_V1, describe,  open,    destroy, metadata,
                    level,        channel_name,      read_rgba, read_raw};
            return QPX_READER_OK;
        });
    }
};
template <class Codec> using Rgba8Reader = PackedImageReader<Codec>;
} // namespace qpx_image_plugin
