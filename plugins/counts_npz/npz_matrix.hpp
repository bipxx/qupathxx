// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "npy_array.hpp"
#include "npz_members.hpp"

namespace qpx_npz {

struct Handle {
    Arena arena;
    Array matrix;
    std::pmr::vector<std::pmr::string> observations, features;
    explicit Handle(std::size_t work) : arena(work), observations(&arena.memory), features(&arena.memory) {}
};

inline const Handle& handle(void* value) { require(value, "NPZ source handle is null"); return *static_cast<Handle*>(value); }

inline std::uint64_t page(std::uint64_t first, std::uint64_t count, std::uint64_t total) {
    require(count && first <= total, "NPZ page is outside the axis or values");
    return std::min(count, total - first);
}

inline void load(Handle& source, qpx_reader_string_v1 path, std::uint64_t file_limit,
                 qpx_reader_cancel_v1 cancel, bool continuous = false) {
    auto* memory = &source.arena.memory;
    const auto input = read(path, cancel, memory, file_limit);
    const auto payloads = members(input, memory, cancel, continuous ? "values.npy" : "counts.npy");
    source.matrix = array(payloads[0], 2, continuous);
    const auto observations = array(payloads[1], 1), features = array(payloads[2], 1);
    valid(observations.elements == source.matrix.shape[0] && features.elements == source.matrix.shape[1],
          "NPZ axis lengths differ from the observation-major array");
    source.observations = labels(observations, memory, cancel);
    source.features = labels(features, memory, cancel);
}

inline std::uint64_t sample_bits(const Array& matrix, std::uint64_t observation_major_index) {
    const auto physical = matrix.fortran
        ? (observation_major_index % matrix.shape[1]) * matrix.shape[0] + observation_major_index / matrix.shape[1]
        : observation_major_index;
    return matrix.payload.number(std::size_t(physical) * matrix.item, matrix.item, matrix.big_endian);
}

inline qpx_reader_status_v1 QPX_FORMAT_CALL axis(void* value, const qpx_matrix_axis_request_v1* request,
                                         qpx_matrix_axis_buffer_v1* out, qpx_reader_cancel_v1 cancel,
                                         qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        const auto& source = handle(value);
        require(out && out->struct_size >= sizeof(*out), "NPZ axis buffer is too small");
        out->written = out->text.size = 0;
        require(request && request->struct_size >= sizeof(*request) && !out->reserved &&
                (request->axis == QPX_FORMAT_FEATURE_AXIS || request->axis == QPX_FORMAT_OBSERVATION_AXIS) &&
                (!out->capacity || out->rows) && (!out->text.capacity || out->text.data), "Invalid NPZ axis request");
        const auto& labels = request->axis == QPX_FORMAT_FEATURE_AXIS ? source.features : source.observations;
        const auto count = page(request->first, request->count, labels.size());
        std::uint64_t size = 0;
        for (std::uint64_t i = 0; i < count; ++i) { poll(cancel); size += labels[request->first + i].size(); }
        if (out->capacity < count || out->text.capacity < size) {
            out->written = count;
            out->text.size = size;
            return QPX_READER_BUFFER_TOO_SMALL;
        }
        std::uint64_t at = 0;
        for (std::uint64_t i = 0; i < count; ++i) {
            poll(cancel);
            const auto index = request->first + i;
            const auto& label = labels[index];
            out->rows[i] = {index, at, label.size(), at + label.size(), 0};
            std::memcpy(out->text.data + at, label.data(), label.size());
            at += label.size();
        }
        out->written = count;
        out->text.size = size;
        return QPX_READER_OK;
    });
}

} // namespace qpx_npz
