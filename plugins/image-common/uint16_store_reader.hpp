// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "rgba8_reader.hpp"
#include <qupathxx/raw_image.hpp>
#include <qupathxx/workflow.hpp>

#include <atomic>
#include <chrono>
#include <future>

namespace qpx_image_plugin {

// Bridge a native typed ImageServer using the empty-options store ABI.
// A provider supplies its descriptor, primary key and native open operation.
template <class Source, qpx::SampleType... ExtraTypes> class TypedStoreReader {
    static constexpr auto capabilities = QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW |
                                         QPX_READER_CAP_SOURCE_STORE;
    static constexpr std::size_t bridge_bytes = 1024 * 1024 + 65536;

    struct Handle {
        std::shared_ptr<qpx::ImageServer> image;
    };

    static constexpr std::uint32_t sample_type_code(qpx::SampleType type) {
        switch (type) {
        case qpx::SampleType::UInt8: return QPX_READER_UINT8;
        case qpx::SampleType::Int8: return QPX_READER_INT8;
        case qpx::SampleType::UInt16: return QPX_READER_UINT16;
        case qpx::SampleType::Int16: return QPX_READER_INT16;
        case qpx::SampleType::UInt32: return QPX_READER_UINT32;
        case qpx::SampleType::Int32: return QPX_READER_INT32;
        case qpx::SampleType::UInt64: return QPX_READER_UINT64;
        case qpx::SampleType::Int64: return QPX_READER_INT64;
        case qpx::SampleType::Float32: return QPX_READER_FLOAT32;
        case qpx::SampleType::Float64: return QPX_READER_FLOAT64;
        default: return 0;
        }
    }
    static_assert(((sample_type_code(ExtraTypes) != 0) && ...));

    static std::uint32_t raw_abi_type(qpx::SampleType type) {
        valid(type == qpx::SampleType::UInt16 || ((type == ExtraTypes) || ...),
              "Store reader native sample type differs");
        return sample_type_code(type);
    }

    static const qpx::ImageServer& image(void* value) {
        require(value != nullptr, "Store reader image handle is null");
        return *static_cast<Handle*>(value)->image;
    }

    // Only the caller thread may poll its C callback. The synchronous native worker
    // receives an atomic flag and is joined before any ABI call returns.
    template <class F>
    static auto native(qpx_reader_cancel_v1 cancel, F&& action) {
        if (!cancel.is_cancelled)
            return action(nullptr);
        checkpoint(cancel);
        std::atomic<bool> stop{false};
        auto work = std::async(std::launch::async, [&] { return action(&stop); });
        try {
            while (work.wait_for(std::chrono::milliseconds(2)) != std::future_status::ready)
                checkpoint(cancel);
            checkpoint(cancel);
        } catch (...) {
            stop.store(true, std::memory_order_relaxed);
            work.wait();
            throw;
        }
        return work.get();
    }

    template <class F>
    static qpx_reader_status_v1 abi(qpx_reader_text_v1* error, F&& action) noexcept {
        return boundary(error, [&]() -> qpx_reader_status_v1 {
            try {
                return action();
            } catch (const qpx::WorkflowCancelled& e) {
                return failure(error, QPX_READER_CANCELLED, e.what());
            } catch (const qpx::WorkflowInputChanged& e) {
                return failure(error, QPX_READER_IO_ERROR, e.what());
            } catch (const std::length_error& e) {
                return failure(error, QPX_READER_INVALID_REQUEST, e.what());
            } catch (const std::out_of_range& e) {
                return failure(error, QPX_READER_INVALID_REQUEST, e.what());
            }
        });
    }

    static std::string store_path(const qpx_reader_open_v1* request) {
        require(request && request->struct_size >= sizeof(*request) && !request->reserved,
                "Invalid store reader request");
        require(!request->options.size, "Store reader options must be empty");
        const auto path = request->path;
        require(path.data && path.size && path.size <= 4096, "Invalid store reader path length");
        std::string result(path.data, std::size_t(path.size));
        require(result.find('\0') == std::string::npos && std::filesystem::u8path(result).is_absolute(),
                "Store reader path must be absolute");
        return result;
    }

    static qpx_reader_status_v1 QPX_READER_CALL describe(
        qpx_reader_info_v1* out, qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            require(out && out->struct_size >= sizeof(*out), "Store reader descriptor is too small");
            out->abi_version = QPX_READER_ABI_V1;
            out->capabilities = capabilities;
            bool complete = text(out->provider_id, Source::provider_id);
            complete = text(out->display_name, Source::name) && complete;
            complete = text(out->version, Source::version) && complete;
            complete = text(out->contract, Source::contract) && complete;
            return complete ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
        });
    }

    static qpx_reader_status_v1 QPX_READER_CALL primary(
        const qpx_reader_open_v1* request, qpx_reader_text_v1* out, qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            require(out, "Store reader primary metadata output is missing");
            (void)store_path(request);
            checkpoint(request->cancel);
            // The host guards existence and canonical file identity at this key.
            return text(*out, Source::primary_key) ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
        });
    }

    static qpx_reader_status_v1 QPX_READER_CALL open(
        const qpx_reader_open_v1* request, void** out, qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            require(out, "Store reader output handle is missing");
            *out = nullptr;
            const auto path = store_path(request);
            require(request->work_bytes > bridge_bytes && request->work_bytes <= work_limit,
                    "Invalid store reader private work allowance");
            const auto work = std::size_t(request->work_bytes) - bridge_bytes;
            auto result = native(request->cancel, [&](const std::atomic<bool>* stop) {
                return Source::open(path, work, stop);
            });
            valid(bool(result), "Store decoder returned an empty image");
            auto handle = std::make_unique<Handle>();
            handle->image = std::move(result);
            *out = handle.release();
            return QPX_READER_OK;
        });
    }

    static void QPX_READER_CALL destroy(void* value) noexcept {
        delete static_cast<Handle*>(value);
    }

    static qpx_reader_status_v1 QPX_READER_CALL metadata(
        void* value, qpx_reader_metadata_v1* out, qpx_reader_cancel_v1 cancel,
        qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            require(out && out->struct_size >= sizeof(*out), "Store reader metadata is too small");
            checkpoint(cancel);
            const auto& source = image(value);
            const auto raw = source.raw_metadata();
            valid(bool(raw), "Store reader native raw metadata is unavailable");
            out->level_count = std::uint32_t(raw->levels.size());
            out->width = raw->width;
            out->height = raw->height;
            out->has_raw = 1;
            out->channels = raw->channels;
            out->sample_type = raw_abi_type(raw->sample_type);
            out->z_planes = raw->z_planes;
            out->timepoints = raw->timepoints;
            out->series = raw->series;
            out->series_count = raw->series_count;
            out->calibration_flags = 0;
            const auto calibration = [&](const std::optional<double>& source_value, double& target,
                                         std::uint32_t flag) {
                target = source_value.value_or(0);
                if (source_value)
                    out->calibration_flags |= flag;
            };
            calibration(raw->pixel_width_um, out->pixel_width_um, QPX_READER_CALIBRATION_X);
            calibration(raw->pixel_height_um, out->pixel_height_um, QPX_READER_CALIBRATION_Y);
            calibration(raw->z_spacing_um, out->z_spacing_um, QPX_READER_CALIBRATION_Z);
            calibration(raw->time_interval_seconds, out->time_interval_seconds, QPX_READER_CALIBRATION_TIME);
            bool complete = text(out->vendor, Source::name);
            complete = text(out->dimension_order, raw->dimension_order.c_str()) && complete;
            return complete ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
        });
    }

    static qpx_reader_status_v1 QPX_READER_CALL level(
        void* value, std::uint32_t index, qpx_reader_level_v1* out,
        qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            const auto& levels = image(value).metadata().levels;
            require(out && out->struct_size >= sizeof(*out) && index < levels.size(),
                    "Invalid store reader level request");
            const auto& selected = levels[index];
            out->reserved = 0;
            out->width = selected.width;
            out->height = selected.height;
            out->downsample = selected.downsample;
            return QPX_READER_OK;
        });
    }

    static qpx_reader_status_v1 QPX_READER_CALL channel_name(
        void* value, std::uint32_t index, qpx_reader_text_v1* out,
        qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            const auto raw = image(value).raw_metadata();
            require(out && raw && index < raw->channel_names.size(),
                    "Invalid store reader channel name request");
            return text(*out, raw->channel_names[index].c_str()) ? QPX_READER_OK
                                                                : QPX_READER_BUFFER_TOO_SMALL;
        });
    }

    struct OutputBytes {
        std::size_t data;
        std::size_t validity;
    };

    static OutputBytes validate_output(qpx_reader_pixels_v1* out, const qpx::RegionRequest& region,
                                       std::uint32_t channels, bool raw, std::uint32_t type, std::uint64_t sample) {
        require(out && out->struct_size >= sizeof(*out) && !out->reserved,
                "Invalid store reader pixel output");
        const auto pixel = channels * sample;
        const auto width = std::uint64_t(region.width), height = std::uint64_t(region.height);
        const auto pixel_with_validity = pixel + unsigned(raw);
        require(pixel && width && height && width <= work_limit / pixel_with_validity &&
                    height <= work_limit / (width * pixel_with_validity),
                "Store reader pixel output exceeds the ABI limit");
        const auto count = width * height;
        const auto row = width * pixel;
        require(out->width == region.width && out->height == region.height && out->channels == channels &&
                    out->sample_type == type && out->channel_stride == sample &&
                    out->pixel_stride == pixel && out->row_stride == row,
                "Store reader output type, shape or strides differ");
        require(out->data && out->capacity >= count * pixel, "Store reader output data capacity is too small");
        require(raw ? out->validity && out->validity_capacity >= count
                    : !out->validity && !out->validity_capacity,
                "Invalid store reader output validity capacity");
        return {std::size_t(count * pixel), raw ? std::size_t(count) : 0};
    }

    static qpx::RegionRequest region(const qpx_reader_region_v1* r) {
        require(r && r->struct_size >= sizeof(*r), "Store reader region request is too small");
        return {r->x, r->y, r->level, r->width, r->height};
    }

    static void clear_counts(qpx_reader_pixels_v1* out) noexcept {
        if (out && out->struct_size >= sizeof(*out))
            out->written = out->validity_written = 0;
    }

    static qpx_reader_status_v1 QPX_READER_CALL read_raw(
        void* value, const qpx_reader_region_v1* r, qpx_reader_pixels_v1* out,
        qpx_reader_cancel_v1 cancel, qpx_reader_text_v1* error) noexcept {
        clear_counts(out);
        return abi(error, [&]() -> qpx_reader_status_v1 {
            const auto coordinates = region(r);
            const auto& source = image(value);
            const auto raw = source.raw_metadata();
            valid(bool(raw), "Store reader native raw metadata is unavailable");
            const qpx::RawRegionRequest request{coordinates, r->first_channel, r->channels, r->z, r->t};
            request.validate(*raw);
            const auto bytes = validate_output(out, coordinates, r->channels, true, raw_abi_type(raw->sample_type),
                                               qpx::sample_bytes(raw->sample_type));
            const auto result = native(cancel, [&](const std::atomic<bool>* stop) {
                return source.read_raw(request, stop);
            });
            valid(result.width() == coordinates.width && result.height() == coordinates.height &&
                      result.channels() == r->channels && result.sample_type() == raw->sample_type &&
                      result.valid().size() == bytes.validity,
                  "Store reader native raw output geometry differs");
            std::visit([&](const auto& samples) {
                using Sample = typename std::decay_t<decltype(samples)>::value_type;
                valid(samples.size() * sizeof(Sample) == bytes.data, "Store reader native raw byte count differs");
                std::memcpy(out->data, samples.data(), bytes.data);
            }, result.storage());
            std::memcpy(out->validity, result.valid().data(), bytes.validity);
            out->written = bytes.data;
            out->validity_written = bytes.validity;
            return QPX_READER_OK;
        });
    }

    static qpx_reader_status_v1 QPX_READER_CALL read_rgba(
        void* value, const qpx_reader_region_v1* r, qpx_reader_pixels_v1* out,
        qpx_reader_text_v1* error) noexcept {
        clear_counts(out);
        return abi(error, [&]() -> qpx_reader_status_v1 {
            const auto request = region(r);
            require(!r->first_channel && !r->channels && !r->z && !r->t,
                    "Store reader RGBA request must use the open display view");
            const auto& source = image(value);
            qpx::validate_region(source.metadata(), request);
            const auto bytes = validate_output(out, request, 4, false, QPX_READER_UINT8, 1);
            const auto result = source.read_region(request);
            valid(result.width() == request.width && result.height() == request.height &&
                      result.bytes().size() == bytes.data,
                  "Store reader native RGBA output geometry differs");
            std::memcpy(out->data, result.bytes().data(), bytes.data);
            out->written = bytes.data;
            return QPX_READER_OK;
        });
    }

public:
    static qpx_reader_status_v1 entry(std::uint32_t requested_abi, qpx_reader_api_v1* out,
                                     qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            require(requested_abi == QPX_READER_ABI_V1 && out && out->struct_size >= sizeof(*out),
                    "Store reader ABI mismatch");
            *out = {sizeof(*out), QPX_READER_ABI_V1, describe, open, destroy,
                    metadata, level, channel_name, read_rgba, read_raw};
            return QPX_READER_OK;
        });
    }

    static qpx_reader_status_v1 store_entry(std::uint32_t requested_abi, qpx_reader_store_api_v1* out,
                                           qpx_reader_text_v1* error) noexcept {
        return abi(error, [&]() -> qpx_reader_status_v1 {
            require(requested_abi == QPX_READER_ABI_V1 && out && out->struct_size >= sizeof(*out),
                    "Store extension ABI mismatch");
            *out = {sizeof(*out), QPX_READER_ABI_V1, primary};
            return QPX_READER_OK;
        });
    }
};
template <class Source>
using UInt16StoreReader = TypedStoreReader<Source>;
} // namespace qpx_image_plugin
