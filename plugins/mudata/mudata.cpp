// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "mudata_source.hpp"

namespace {
using namespace qpx_mudata;
constexpr auto abi = QPX_MATRIX_ABI_V5;

Source& source(void* value) {
    require(value, "MuData source handle is null");
    return *static_cast<Source*>(value);
}

std::uint64_t page_size(std::uint64_t first, std::uint64_t count, std::uint64_t total, std::uint64_t maximum) {
    require(count && count <= maximum && first <= total, "MuData page is outside source bounds");
    return std::min(count, total - first);
}

qpx_reader_status_v1 QPX_FORMAT_CALL describe(qpx_format_info_v1* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        require(out && out->struct_size >= sizeof(*out), "MuData format descriptor is too small");
        out->abi_version = abi;
        out->domain = QPX_FORMAT_MATRIX;
        out->reserved = 0;
        bool complete = text(out->provider_id, "org.qupathxx.mudata");
        complete = text(out->format_id, "h5mu") && complete;
        complete = text(out->display_name, "MuData modality") && complete;
        complete = text(out->version, QPX_FORMAT_PLUGIN_PROVIDER_VERSION) && complete;
        complete = text(out->contract, "mudata-selected-modality/2") && complete;
        complete = text(out->profile, QPX_FORMAT_PLUGIN_PROFILE_ID) && complete;
        return complete ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_matrix_open_v2* request, void** out,
                                         qpx_reader_text_v1* error) noexcept {
    if (out) *out = nullptr;
    return boundary(error, [&] {
        require(request && out && request->struct_size >= sizeof(*request) && !request->reserved,
                "Invalid MuData ABI2 open request");
        require(request->work_bytes >= fixed_work && request->work_bytes <= 256 * hdf::mib &&
                request->file_bytes && request->file_bytes <= UINT64_C(1024) * hdf::mib,
                "Invalid MuData file/work allowances");
        require(request->path.data && request->path.size && request->path.size <= 32768, "Invalid MuData source path");
        const std::string_view name(request->path.data, std::size_t(request->path.size));
        utf8(name, request->cancel);
        const auto path = std::filesystem::u8path(name);
        require(path.is_absolute(), "MuData requires an absolute file path");
        auto options = select(request->options, request->cancel);
        auto gate = hdf::library_lock(request->cancel);
        hdf::Errors errors;
        auto opened = std::make_unique<Source>(path, std::move(options), request->file_bytes,
                                               std::size_t(request->work_bytes), request->cancel);
        poll(request->cancel);
        *out = opened.release();
        return QPX_READER_OK;
    });
}

void QPX_FORMAT_CALL destroy(void* value) noexcept {
    try {
        auto gate = hdf::library_lock({});
        hdf::Errors errors(false);
        delete static_cast<Source*>(value);
    } catch (...) {}
}

qpx_reader_status_v1 QPX_FORMAT_CALL metadata(void* value, qpx_matrix_metadata_v2* out,
                                              qpx_reader_cancel_v1 cancel, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        const auto& data = source(value);
        require(out && out->struct_size >= sizeof(*out) && (!out->value_domain.capacity || out->value_domain.data),
                "Invalid MuData metadata buffer");
        poll(cancel);
        out->value_type = data.chosen.value_type;
        out->observations = data.observations;
        out->features = data.features;
        out->reserved[0] = out->reserved[1] = 0;
        return text(out->value_domain, data.chosen.domain) ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL axis_page(void* value, const qpx_matrix_axis_request_v1* request,
                                              qpx_matrix_axis_buffer_v1* out, qpx_reader_cancel_v1 cancel,
                                              qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        auto& data = source(value);
        require(out && out->struct_size >= sizeof(*out), "MuData axis buffer is too small");
        out->written = out->text.size = 0;
        require(request && request->struct_size >= sizeof(*request) && !out->reserved &&
                (request->axis == QPX_FORMAT_FEATURE_AXIS || request->axis == QPX_FORMAT_OBSERVATION_AXIS) &&
                (!out->capacity || out->rows) && (!out->text.capacity || out->text.data), "Invalid MuData axis request");
        const bool features = request->axis == QPX_FORMAT_FEATURE_AXIS;
        const auto count = page_size(request->first, request->count,
                                     features ? data.features : data.observations, hdf::string_page);
        if (!count) return QPX_READER_OK;
        auto gate = hdf::library_lock(cancel);
        hdf::Errors errors;
        auto& ids = features ? data.feature_ids : data.observation_ids;
        std::array<char, hdf::string_page * hdf::string_bytes> bytes;
        std::array<qpx_matrix_axis_row_v1, hdf::string_page> rows;
        std::uint64_t used = 0;
        hdf::strings(ids, data.lease, data.scratch, request->first, std::size_t(count), false, false,
            [&](std::size_t i, std::string_view id) {
                rows[i] = {request->first + i, used, id.size(), used + id.size(), 0};
                std::memcpy(bytes.data() + used, id.data(), id.size());
                used += id.size();
            }, cancel);
        out->written = count;
        out->text.size = used;
        if (out->capacity < count || out->text.capacity < used) return QPX_READER_BUFFER_TOO_SMALL;
        std::copy_n(rows.data(), count, out->rows);
        std::memcpy(out->text.data, bytes.data(), std::size_t(used));
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL values(void* value, const qpx_matrix_values_request_v1* request,
                                            qpx_matrix_values_buffer_v2* out, qpx_reader_cancel_v1 cancel,
                                            qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        auto& data = source(value);
        require(out && out->struct_size >= sizeof(*out), "MuData value buffer is too small");
        out->written = 0;
        const bool counts = data.chosen.value_type == QPX_FORMAT_COUNTS_UINT64;
        require(request && request->struct_size >= sizeof(*request) && !request->reserved && !out->reserved &&
                out->value_type == data.chosen.value_type &&
                (counts ? !out->continuous && (!out->capacity || out->counts)
                        : !out->counts && (!out->capacity || out->continuous)), "Invalid MuData value request");
        const auto count = page_size(request->first, request->count, data.observations * data.features, hdf::max_page);
        poll(cancel);
        if (out->capacity < count) {
            out->written = count;
            return QPX_READER_BUFFER_TOO_SMALL;
        }
        const auto put = [&](std::uint64_t i, std::uint64_t bits) {
            if (counts) out->counts[i] = bits;
            else std::memcpy(out->continuous + i, &bits, sizeof(bits));
        };
        if (data.dense.data.present() && count) {
            auto gate = hdf::library_lock(cancel);
            hdf::Errors errors;
            for (std::uint64_t at = 0; at < count;) {
                const auto logical = request->first + at;
                const auto row = logical / data.features, col = logical % data.features;
                const auto length = std::size_t(std::min({count - at, data.features - col, UINT64_C(4096)}));
                hdf::numeric_block(data.dense, data.lease, data.scratch, data.dense_kind,
                    {row, col}, {1, length}, length,
                    [&](std::size_t i, const hdf::Number& sample) { put(at + i, data.value_bits(sample)); }, cancel);
                at += length;
            }
        } else if (count) {
            auto entry = std::lower_bound(data.sparse.begin(), data.sparse.end(), request->first,
                [](const Entry& item, auto logical) { return item.logical < logical; });
            for (std::uint64_t i = 0; i < count; ++i) {
                if (!(i & 4095)) poll(cancel);
                const auto logical = request->first + i;
                put(i, entry != data.sparse.end() && entry->logical == logical ? (entry++)->bits : 0);
            }
        }
        poll(cancel);
        out->written = count;
        return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_FORMAT_CALL sparse_metadata(void* value, qpx_matrix_sparse_metadata_v1* out,
                                                    qpx_reader_cancel_v1 cancel, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        const auto& data = source(value);
        require(out && out->struct_size >= sizeof(*out), "MuData sparse metadata is too small");
        poll(cancel);
        out->sparse = !data.dense.data.present();
        out->entries = data.sparse.size();
        out->reserved = 0;
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_FORMAT_CALL sparse_values(void* value, const qpx_matrix_values_request_v1* request,
                                                  qpx_matrix_sparse_buffer_v1* out, qpx_reader_cancel_v1 cancel,
                                                  qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        require(out && out->struct_size >= sizeof(*out), "MuData sparse buffer is too small");
        out->written = 0;
        const auto& data = source(value);
        require(request && request->struct_size >= sizeof(*request) && !request->reserved && !out->reserved &&
                (!out->capacity || out->entries) && !data.dense.data.present(), "Invalid MuData sparse page request");
        const auto count = page_size(request->first, request->count, data.sparse.size(), hdf::max_page);
        poll(cancel);
        if (out->capacity < count) {
            out->written = count;
            return QPX_READER_BUFFER_TOO_SMALL;
        }
        for (std::uint64_t i = 0; i < count; ++i) {
            if (!(i & 4095)) poll(cancel);
            const auto& entry = data.sparse[std::size_t(request->first + i)];
            out->entries[i] = {entry.logical, entry.bits};
        }
        poll(cancel);
        out->written = count;
        return QPX_READER_OK;
    });
}
} // namespace

extern "C" QPX_MATRIX_SPARSE_FILE_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v5(
    std::uint32_t requested, qpx_matrix_format_api_v5* out, qpx_reader_text_v1* error) {
    return qpx_binary::boundary(error, [&] {
        qpx_binary::require(requested == abi && out && out->struct_size >= sizeof(*out), "Unsupported MuData matrix ABI");
        *out = {sizeof(*out), abi,
                {sizeof(qpx_matrix_format_api_v2), QPX_MATRIX_ABI_V2, describe, open, destroy, metadata, axis_page, values},
                sparse_metadata, sparse_values};
        return QPX_READER_OK;
    });
}
