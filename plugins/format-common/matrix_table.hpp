// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "binary_input.hpp"
#include "json_tree.hpp"
#include "format_plugin_profile.hpp"

namespace qpx_matrix_table {
using namespace qpx_binary;

struct Source {
    Arena arena;
    qpx_reader_cancel_v1 cancel;
    std::pmr::string domain, observation_column;
    std::pmr::vector<std::pmr::string> observations, features;
    std::pmr::vector<std::uint64_t> values;

    Source(std::size_t work, qpx_reader_cancel_v1 stop)
        : arena(work), cancel(stop), domain(&arena.memory), observation_column(&arena.memory),
          observations(&arena.memory), features(&arena.memory), values(&arena.memory) {}

    bool counts() const { return domain == "raw-counts"; }
    std::uint32_t value_type() const {
        return counts() ? QPX_FORMAT_COUNTS_UINT64 : QPX_FORMAT_CONTINUOUS_FLOAT64;
    }
    void select(qpx_reader_string_v1 options) {
        namespace json = qpx_native_json;
        const json::Tree tree({options.data, std::size_t(options.size)}, &arena.memory, cancel, 3, 68, 3);
        const auto& root = tree.root();
        tree.keys(root, {"observation_id", "feature_columns", "value_domain"});
        domain = json::string(tree.get(root, "value_domain"));
        require(!domain.empty() && domain.size() <= 128, "Matrix table requires an explicit value-domain label");
        utf8(domain, cancel);
        observation_column = json::string(tree.get(root, "observation_id"));
        require(!observation_column.empty() && observation_column.size() <= 256,
                "Matrix table requires an explicit observation ID column");
        const auto& names = tree.array(tree.get(root, "feature_columns"));
        require(!names.empty() && names.size() <= 63, "Select 1 to 63 feature columns");
        for (auto i : names) {
            const auto name = json::string(tree.nodes[i]);
            require(!name.empty() && name.size() <= 256 && name != observation_column &&
                        std::find(features.begin(), features.end(), name) == features.end(),
                    "Feature selections must be distinct from each other and the observation column");
            utf8(name, cancel);
            features.emplace_back(name);
        }
    }

};

inline std::uint64_t continuous_bits(double value) {
    valid(std::isfinite(value), "Continuous feature value must be finite");
    std::uint64_t bits;
    static_assert(sizeof(bits) == sizeof(value), "Float64 ABI size");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

template<class Definition>
struct Plugin {
    static const Source& source(void* handle) {
        require(handle, "Expected a matrix table handle");
        return *static_cast<const Source*>(handle);
    }

    static qpx_reader_status_v1 QPX_FORMAT_CALL describe(qpx_format_info_v1* out, qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&] {
            require(out && out->struct_size >= sizeof(*out), "Invalid matrix table descriptor");
            out->abi_version = QPX_MATRIX_ABI_V2;
            out->domain = QPX_FORMAT_MATRIX;
            out->reserved = 0;
            bool complete = text(out->provider_id, Definition::id);
            complete = text(out->format_id, Definition::format) && complete;
            complete = text(out->display_name, Definition::name) && complete;
            complete = text(out->version, Definition::version) && complete;
            complete = text(out->contract, Definition::contract) && complete;
            complete = text(out->profile, QPX_FORMAT_PLUGIN_PROFILE_ID) && complete;
            return complete ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
        });
    }

    static qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_matrix_open_v2* request, void** out,
                                             qpx_reader_text_v1* error) noexcept {
        if (out)
            *out = nullptr;
        return boundary(error, [&] {
            require(out && request && request->struct_size >= sizeof(*request) && !request->reserved,
                    "Invalid matrix table open request");
            require(request->options.data && request->options.size && request->options.size <= 32768,
                    "Matrix table requires explicit column and value-domain options");
            require(request->path.size <= 32768 && request->file_bytes &&
                        request->work_bytes <= 256ull * 1024 * 1024, "Invalid matrix table source limits");
            const auto overhead = 65536 + 8 * request->path.size + Definition::private_work;
            require(request->work_bytes > overhead, "Matrix table work budget is too small");
            auto handle = std::make_unique<Source>(std::size_t(request->work_bytes - overhead), request->cancel);
            const auto file = read(request->path, request->cancel, &handle->arena.memory, request->file_bytes);
            handle->select(request->options);
            Definition::decode(file, *handle);
            poll(request->cancel);
            *out = handle.release();
            return QPX_READER_OK;
        });
    }

    static void QPX_FORMAT_CALL destroy(void* handle) noexcept { delete static_cast<Source*>(handle); }

    static qpx_reader_status_v1 QPX_FORMAT_CALL metadata(void* handle, qpx_matrix_metadata_v2* out,
                                                  qpx_reader_cancel_v1 cancel, qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&] {
            require(out && out->struct_size >= sizeof(*out) && (!out->value_domain.capacity || out->value_domain.data),
                    "Invalid matrix table metadata buffer");
            const auto& data = source(handle);
            poll(cancel);
            out->value_type = data.value_type();
            out->observations = data.observations.size();
            out->features = data.features.size();
            out->reserved[0] = out->reserved[1] = 0;
            return text(out->value_domain, data.domain) ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
        });
    }

    static qpx_reader_status_v1 QPX_FORMAT_CALL axis(void* handle, const qpx_matrix_axis_request_v1* request,
                                              qpx_matrix_axis_buffer_v1* out, qpx_reader_cancel_v1 cancel,
                                              qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&] {
            require(out && out->struct_size >= sizeof(*out), "Invalid matrix table axis buffer");
            out->written = out->text.size = 0;
            require(request && request->struct_size >= sizeof(*request) && !out->reserved &&
                        (request->axis == QPX_FORMAT_FEATURE_AXIS || request->axis == QPX_FORMAT_OBSERVATION_AXIS) &&
                        (!out->capacity || out->rows) && (!out->text.capacity || out->text.data),
                    "Invalid matrix table axis request");
            const auto& data = source(handle);
            const auto& ids = request->axis == QPX_FORMAT_FEATURE_AXIS ? data.features : data.observations;
            require(request->count && request->first <= ids.size(), "Matrix table axis page is outside source bounds");
            const auto count = std::min<std::uint64_t>(request->count, ids.size() - request->first);
            std::uint64_t size = 0;
            for (std::uint64_t i = 0; i < count; ++i) {
                poll(cancel);
                size += ids[request->first + i].size();
            }
            out->written = count;
            out->text.size = size;
            if (out->capacity < count || out->text.capacity < size)
                return QPX_READER_BUFFER_TOO_SMALL;
            std::uint64_t offset = 0;
            for (std::uint64_t i = 0; i < count; ++i) {
                poll(cancel);
                const auto index = request->first + i;
                const auto& id = ids[index];
                std::memcpy(out->text.data + offset, id.data(), id.size());
                out->rows[i] = {index, offset, id.size(), offset + id.size(), 0};
                offset += id.size();
            }
            return QPX_READER_OK;
        });
    }

    static qpx_reader_status_v1 QPX_FORMAT_CALL values(void* handle, const qpx_matrix_values_request_v1* request,
                                                qpx_matrix_values_buffer_v2* out, qpx_reader_cancel_v1 cancel,
                                                qpx_reader_text_v1* error) noexcept {
        return boundary(error, [&] {
            require(out && out->struct_size >= sizeof(*out), "Invalid matrix table values buffer");
            out->written = 0;
            const auto& data = source(handle);
            require(request && request->struct_size >= sizeof(*request) && !request->reserved && !out->reserved &&
                        out->value_type == data.value_type() && request->count && request->first <= data.values.size() &&
                        (data.counts() ? (!out->continuous && (!out->capacity || out->counts))
                                       : (!out->counts && (!out->capacity || out->continuous))),
                    "Invalid matrix table values request");
            const auto count = std::min<std::uint64_t>(request->count, data.values.size() - request->first);
            out->written = count;
            if (out->capacity < count)
                return QPX_READER_BUFFER_TOO_SMALL;
            for (std::uint64_t i = 0; i < count; ++i) {
                if (!(i & 511))
                    poll(cancel);
                const auto& bits = data.values[request->first + i];
                if (data.counts())
                    out->counts[i] = bits;
                else
                    std::memcpy(out->continuous + i, &bits, sizeof(bits));
            }
            return QPX_READER_OK;
        });
    }
    static qpx_reader_status_v1 entry(std::uint32_t requested, qpx_matrix_format_api_v2* out,
                                      qpx_reader_text_v1* error) {
        return boundary(error, [&] {
            require(requested == QPX_MATRIX_ABI_V2 && out && out->struct_size >= sizeof(*out),
                    "Unsupported matrix table ABI");
            *out = {sizeof(*out), QPX_MATRIX_ABI_V2, describe, open, destroy, metadata, axis, values};
            return QPX_READER_OK;
        });
    }
};
} // namespace qpx_matrix_table
