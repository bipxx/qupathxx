// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/point_columns.hpp"
#include "decoder_memory.h"
#include "format_plugin_profile.hpp"
#include "arrow_file.hpp"

namespace qpx_arrow_points {
using namespace qpx_annotation_source;
using namespace qpx_arrow;
using namespace qpx_point_columns;

double coordinate_at(const ArrowArrayView& column, int64_t row) {
    const auto* values = value_at(column, row);
    valid(values, "Arrow point coordinate is null");
    switch (values->storage_type) {
    case NANOARROW_TYPE_FLOAT:
    case NANOARROW_TYPE_DOUBLE:
        return ArrowArrayViewGetDoubleUnsafe(values, row);
    case NANOARROW_TYPE_INT8: case NANOARROW_TYPE_INT16:
    case NANOARROW_TYPE_INT32: case NANOARROW_TYPE_INT64:
        valid(ArrowArrayViewGetIntUnsafe(values, row) >= 0, "Arrow point coordinate is negative");
        break;
    default:
        break;
    }
    return exact_coordinate(ArrowArrayViewGetUIntUnsafe(values, row));
}

void emit(Handle& h, const ArrowArrayView& batch, const Indices& columns, ArrowType id_type,
          qpx_reader_cancel_v1 cancel) {
    for (int i : columns)
        valid(i < 0 || batch.children[i]->length == batch.length,
              "Arrow point column length differs from its record batch");
    const auto& ids = *batch.children[columns[0]];
    const bool integer = is_integer(id_type);
    for (int64_t i = 0; i < batch.length; ++i) {
        poll(cancel);
        auto& row = h.rows.emplace_back(&h.arena.memory);
        if (integer) {
            auto index = i;
            const auto* values = value_at(ids, index);
            valid(values, "Arrow point ID is empty or null");
            valid(values->storage_type == id_type, "Arrow point ID storage differs from its schema");
            row.id = is_signed_integer(id_type) ? std::to_string(ArrowArrayViewGetIntUnsafe(values, index))
                                               : std::to_string(ArrowArrayViewGetUIntUnsafe(values, index));
            fact(row.facts, "arrow.source_id_type", ArrowTypeString(id_type));
        } else {
            const auto id = string_at(ids, i, 256, cancel);
            valid(!id.empty(), "Arrow point ID is empty or null");
            row.id = id;
        }
        if (columns[3] >= 0)
            row.name = string_at(*batch.children[columns[3]], i, 4096, cancel);
        if (columns[4] >= 0)
            row.classification = string_at(*batch.children[columns[4]], i, 256, cancel);
        begin_point(row);
        GeometryEncoder wire{row.geometry};
        wire.real(coordinate_at(*batch.children[columns[1]], i));
        wire.real(coordinate_at(*batch.children[columns[2]], i));
    }
}

struct PointConsumer : Consumer {
    Handle& handle;
    const Columns& selected;
    qpx_reader_cancel_v1 cancel;
    Indices columns{};
    ArrowType id_type = NANOARROW_TYPE_UNINITIALIZED;

    PointConsumer(Handle& handle, const Columns& selected, qpx_reader_cancel_v1 cancel)
        : handle(handle), selected(selected), cancel(cancel) {}

    void schema(const ArrowSchema& schema) override {
        columns.fill(-1);
        for (std::size_t role = 0; role < selected.size(); ++role) {
            if (selected[role].empty())
                continue;
            const auto i = find_column(schema, selected[role]);
            const auto type = column_type(*schema.children[i]);
            if (role == 0) {
                valid(is_text(type) || is_integer(type), "Arrow point ID column must be UTF-8 or an integer");
                id_type = type;
            } else if (role <= 2) {
                valid(is_integer(type) || type == NANOARROW_TYPE_FLOAT || type == NANOARROW_TYPE_DOUBLE,
                      "Arrow points require numeric coordinates");
            } else {
                valid(is_text(type), "Arrow point names and classes must be UTF-8");
            }
            columns[role] = i;
        }
    }

    void batch(const ArrowArrayView& batch) override {
        emit(handle, batch, columns, id_type, cancel);
    }
};

inline constexpr char provider_version[] = QPX_ARROW_POINTS_VERSION;

struct Definition {
    static constexpr const char *id = "org.qupathxx.arrow-points", *format = "arrow-points",
        *name = "Arrow IPC image-pixel points", *contract = QPX_ARROW_POINTS_CONTRACT,
        *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;

    static void validate_options(qpx_reader_string_v1 options) {
        require(options.size && options.size <= 4096, "Arrow points require explicit column and frame options");
    }

    static void decode(Handle& h, Bytes file, const qpx_reader_open_v1& request) {
        const qpx_native_json::Tree options({request.options.data, std::size_t(request.options.size)},
                                            &h.arena.memory, request.cancel, 6, 7, 2);
        const auto selected = select(options);
        PointConsumer consumer(h, selected, request.cancel);
        read_file(file, &h.arena.memory, request.cancel, consumer);
    }
};
} // namespace qpx_arrow_points

extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi, qpx_annotation_format_api_v3* out,
                               qpx_reader_text_v1* error) {
    using Base = qpx_annotation_source::Plugin<qpx_arrow_points::Definition, 3,
        qpx_arrow_points::provider_version, QPX_ARROW_DECODER_WORK>;
    return Base::entry(abi, out, error);
}
