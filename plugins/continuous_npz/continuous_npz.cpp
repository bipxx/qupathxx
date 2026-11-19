// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "../counts_npz/npz_matrix.hpp"
#include "../format-common/json_tree.hpp"
#include <limits>

namespace {
using namespace qpx_npz;
namespace json = qpx_native_json;

struct Source : Handle {
    explicit Source(std::size_t work) : Handle(work), domain(&arena.memory) {}
    std::pmr::string domain;
};

const Source& source(void* value) {
    require(value, "NPZ source handle is null");
    return *static_cast<Source*>(value);
}

static_assert(sizeof(float) == 4 && sizeof(double) == 8 &&
              std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);

double decode_sample(std::uint64_t bits, unsigned width) {
    if (width == 4) {
        const auto narrow = std::uint32_t(bits);
        float sample;
        std::memcpy(&sample, &narrow, sizeof(sample));
        return sample;
    }
    double sample;
    std::memcpy(&sample, &bits, sizeof(sample));
    return sample;
}

qpx_reader_status_v1 QPX_FORMAT_CALL describe(qpx_format_info_v1* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size >= sizeof(*out), "NPZ descriptor is too small");
        out->abi_version = QPX_MATRIX_ABI_V2;
        out->domain = QPX_FORMAT_MATRIX;
        out->reserved = 0;
        bool complete = text(out->provider_id, "org.qupathxx.continuous-npz");
        complete = text(out->format_id, "continuous-npz") && complete;
        complete = text(out->display_name, "NumPy continuous archive") && complete;
        complete = text(out->version, "1.0.0") && complete;
        complete = text(out->contract, "npz-observation-feature-continuous/1") && complete;
        complete = text(out->profile, QPX_FORMAT_PLUGIN_PROFILE_ID) && complete;
        return complete ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_matrix_open_v2* request, void** out,
                                         qpx_reader_text_v1* error) noexcept {
    if (out) *out = nullptr;
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && request && request->struct_size >= sizeof(*request) && !request->reserved,
                "Invalid NPZ ABI2 open request");
        require(request->options.data && request->options.size && request->options.size <= 2048,
                "Continuous NPZ requires explicit value-domain JSON");
        require(request->path.size <= 32768 && request->file_bytes &&
                request->work_bytes <= UINT64_C(256) * 1024 * 1024, "NPZ path or allowance is invalid");
        const auto overhead = 16384 + request->path.size * 8 + sizeof(Source) + qpx::image_plugin::gzip_workspace_bytes;
        require(request->work_bytes > overhead, "NPZ work budget is too small");
        poll(request->cancel);
        auto selected = std::make_unique<Source>(std::size_t(request->work_bytes - overhead));
        json::Tree options({request->options.data, std::size_t(request->options.size)}, &selected->arena.memory,
                           request->cancel, 1, 2, 1);
        options.keys(options.root(), {"value_domain"});
        const auto domain = json::string(options.get(options.root(), "value_domain"));
        require(!domain.empty() && domain.size() <= 128 && domain != "raw-counts",
                "Continuous NPZ needs a nonempty value-domain label other than raw-counts");
        utf8(domain, request->cancel);
        selected->domain = domain;
        load(*selected, request->path, request->file_bytes, request->cancel, true);
        const auto& matrix = selected->matrix;
        const auto exponent = matrix.item == 4 ? UINT64_C(0x7f800000) : UINT64_C(0x7ff0000000000000);
        for (std::size_t index = 0; index < matrix.elements; ++index) {
            if (!(index & 4095)) poll(request->cancel);
            const auto bits = matrix.payload.number(index * matrix.item, matrix.item, matrix.big_endian);
            valid((bits & exponent) != exponent, "NPY continuous values must be finite");
        }
        poll(request->cancel);
        *out = selected.release();
        return QPX_READER_OK;
    });
}

void QPX_FORMAT_CALL destroy(void* value) noexcept { delete static_cast<Source*>(value); }

qpx_reader_status_v1 QPX_FORMAT_CALL metadata(void* value, qpx_matrix_metadata_v2* out,
                                              qpx_reader_cancel_v1 cancel, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size >= sizeof(*out) && (!out->value_domain.capacity || out->value_domain.data),
                "Invalid NPZ metadata buffer");
        const auto& data = source(value);
        poll(cancel);
        out->value_type = QPX_FORMAT_CONTINUOUS_FLOAT64;
        out->observations = data.observations.size();
        out->features = data.features.size();
        out->reserved[0] = out->reserved[1] = 0;
        return text(out->value_domain, data.domain) ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL axis_page(void* value, const qpx_matrix_axis_request_v1* request,
                                              qpx_matrix_axis_buffer_v1* out, qpx_reader_cancel_v1 cancel,
                                              qpx_reader_text_v1* error) noexcept {
    return axis(static_cast<Handle*>(static_cast<Source*>(value)), request, out, cancel, error);
}

qpx_reader_status_v1 QPX_FORMAT_CALL values(void* value, const qpx_matrix_values_request_v1* request,
                                            qpx_matrix_values_buffer_v2* out, qpx_reader_cancel_v1 cancel,
                                            qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size >= sizeof(*out), "NPZ values buffer is too small");
        out->written = 0;
        require(request && request->struct_size >= sizeof(*request) && !request->reserved && !out->reserved &&
                out->value_type == QPX_FORMAT_CONTINUOUS_FLOAT64 && !out->counts && (!out->capacity || out->continuous),
                "Invalid NPZ continuous values request");
        const auto& matrix = source(value).matrix;
        const auto count = page(request->first, request->count, matrix.elements);
        poll(cancel);
        if (out->capacity < count) {
            out->written = count;
            return QPX_READER_BUFFER_TOO_SMALL;
        }
        for (std::uint64_t i = 0; i < count; ++i) {
            if (!(i & 511)) poll(cancel);
            out->continuous[i] = decode_sample(sample_bits(matrix, request->first + i), matrix.item);
        }
        out->written = count;
        return QPX_READER_OK;
    });
}
} // namespace

extern "C" QPX_MATRIX_TYPED_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v2(
    std::uint32_t requested, qpx_matrix_format_api_v2* out, qpx_reader_text_v1* error) {
    return qpx_npz::boundary(error, [&]() -> qpx_reader_status_v1 {
        qpx_npz::require(requested == QPX_MATRIX_ABI_V2 && out && out->struct_size >= sizeof(*out),
                         "Unsupported continuous NPZ matrix ABI");
        *out = {sizeof(*out), QPX_MATRIX_ABI_V2, describe, open, destroy, metadata, axis_page, values};
        return QPX_READER_OK;
    });
}
