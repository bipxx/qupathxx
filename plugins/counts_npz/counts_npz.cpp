// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "npz_matrix.hpp"

namespace {
using namespace qpx_npz;

qpx_reader_status_v1 QPX_FORMAT_CALL describe(qpx_format_info_v1* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size >= sizeof(*out), "NPZ descriptor is too small");
        out->abi_version = QPX_FORMAT_ABI_V1;
        out->domain = QPX_FORMAT_MATRIX;
        out->reserved = 0;
        bool complete = text(out->provider_id, "org.qupathxx.counts-npz");
        complete = text(out->format_id, "counts-npz") && complete;
        complete = text(out->display_name, "NumPy counts archive") && complete;
        complete = text(out->version, "1.0.0") && complete;
        complete = text(out->contract, "npz-observation-feature-counts/1") && complete;
        complete = text(out->profile, QPX_FORMAT_PLUGIN_PROFILE_ID) && complete;
        return complete ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_reader_open_v1* request, void** out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out, "NPZ output handle is null");
        *out = nullptr;
        require(request && request->struct_size >= sizeof(*request) && !request->reserved && !request->options.size,
                "NPZ counts require an ABI1 request with empty options");
        require(request->path.size <= 32768 && request->work_bytes <= UINT64_C(256) * 1024 * 1024,
                "NPZ path or work budget exceeds its bound");
        const auto overhead = 16384 + request->path.size * 8 + sizeof(Handle) + qpx::image_plugin::gzip_workspace_bytes;
        require(request->work_bytes > overhead, "NPZ work budget is too small");
        poll(request->cancel);
        auto source = std::make_unique<Handle>(std::size_t(request->work_bytes - overhead));
        load(*source, request->path, 64u * 1024 * 1024, request->cancel);
        if (source->matrix.kind == 'i') {
            const auto width = source->matrix.item;
            const auto sign = source->matrix.big_endian ? 0 : width - 1;
            for (std::size_t index = 0; index < source->matrix.elements; ++index) {
                if (!(index & 4095)) poll(request->cancel);
                valid(!(source->matrix.payload.data[index * width + sign] & 128), "NPY signed counts must be nonnegative");
            }
        }
        poll(request->cancel);
        *out = source.release();
        return QPX_READER_OK;
    });
}

void QPX_FORMAT_CALL destroy(void* value) noexcept { delete static_cast<Handle*>(value); }

qpx_reader_status_v1 QPX_FORMAT_CALL metadata(void* value, qpx_matrix_metadata_v1* out,
                                             qpx_reader_cancel_v1 cancel, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        const auto& source = handle(value);
        require(out && out->struct_size >= sizeof(*out), "NPZ metadata is too small");
        poll(cancel);
        out->value_type = QPX_FORMAT_COUNTS_UINT64;
        out->features = source.features.size();
        out->observations = source.observations.size();
        out->reserved[0] = out->reserved[1] = 0;
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL values(void* value, const qpx_matrix_values_request_v1* request,
                                           qpx_matrix_values_buffer_v1* out, qpx_reader_cancel_v1 cancel,
                                           qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        const auto& counts = handle(value).matrix;
        require(out && out->struct_size >= sizeof(*out), "NPZ values buffer is too small");
        out->written = 0;
        require(request && request->struct_size >= sizeof(*request) && !request->reserved && !out->reserved &&
                (!out->capacity || out->values), "Invalid NPZ values request");
        const auto count = page(request->first, request->count, counts.elements);
        poll(cancel);
        if (out->capacity < count) {
            out->written = count;
            return QPX_READER_BUFFER_TOO_SMALL;
        }
        for (std::uint64_t i = 0; i < count; ++i) {
            if (!(i & 511)) poll(cancel);
            out->values[i] = sample_bits(counts, request->first + i);
        }
        out->written = count;
        return QPX_READER_OK;
    });
}
} // namespace

extern "C" QPX_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v1(
    std::uint32_t requested, qpx_matrix_format_api_v1* out, qpx_reader_text_v1* error) {
    return qpx_npz::boundary(error, [&]() -> qpx_reader_status_v1 {
        qpx_npz::require(requested == QPX_FORMAT_ABI_V1 && out && out->struct_size >= sizeof(*out), "Unsupported NPZ matrix ABI");
        *out = {sizeof(*out), QPX_FORMAT_ABI_V1, describe, open, destroy, metadata, axis, values};
        return QPX_READER_OK;
    });
}
