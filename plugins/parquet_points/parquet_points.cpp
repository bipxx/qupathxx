// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/parquet_file.hpp"
#include "../format-common/point_columns.hpp"
#include "format_plugin_profile.hpp"

namespace {
using namespace qpx_point_columns;
using namespace qpx_parquet;
constexpr std::size_t pool_limit = 4 * 1024 * 1024;

void decode_points(Handle& handle, Bytes file, const Columns& names, qpx_reader_cancel_v1 cancel) {
    File source(file, pool_limit);
    const auto& schema = source.schema();
    Indices indices{};
    indices.fill(-1);
    for (std::size_t role = 0; role < names.size(); ++role) {
        if (names[role].empty())
            continue;
        indices[role] = column_index(schema, names[role]);
        if (role >= 3)
            valid(text_column(*schema.Column(indices[role])), "Parquet point names and classes must be UTF-8");
    }
    const auto integer = numeric_id_column(schema, indices[0]);
    const auto id_type = integer ? integer_id_type(*integer) : std::string{};
    const std::array coordinates{numeric_column(schema, indices[1], true, true),
                                 numeric_column(schema, indices[2], true, true)};
    auto* memory = &handle.arena.memory;
    handle.rows.reserve(source.rows());
    source.groups(cancel, [&](std::size_t offset, const Group& group) {
        auto add_id = [&](std::size_t, std::string_view id) {
            auto& row = handle.rows.emplace_back(memory);
            row.id = id;
            if (integer)
                fact(row.facts, "parquet.source_id_type", id_type);
            begin_point(row);
        };
        if (integer)
            read_integer_ids(group, *integer, add_id);
        else
            group.strings(indices[0], false, text_limit(0), add_id);
        for (const auto& column : coordinates) {
            const auto put = [&](std::size_t row, double value) {
                GeometryEncoder{handle.rows[offset + row].geometry}.real(value);
            };
            group.numbers(column, [&](std::size_t row, std::uint64_t value) {
                put(row, exact_coordinate(value));
            }, put);
        }
        if (indices[3] >= 0)
            group.strings(indices[3], true, text_limit(3), [&](std::size_t row, std::string_view label) {
                handle.rows[offset + row].name = label;
            });
        if (indices[4] >= 0)
            group.strings(indices[4], true, text_limit(4), [&](std::size_t row, std::string_view label) {
                handle.rows[offset + row].classification = label;
            });
    });
}

inline constexpr char provider_version[] = QPX_PARQUET_POINTS_VERSION;
struct Definition {
    static constexpr const char *id = "org.qupathxx.parquet-points", *format = "parquet-points",
        *name = "Parquet image-pixel points", *contract = QPX_PARQUET_POINTS_CONTRACT,
        *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;

    static void validate_options(qpx_reader_string_v1 options) {
        require(options.size && options.size <= 4096, "Parquet points require explicit column and frame options");
    }

    static void decode(Handle& handle, Bytes file, const qpx_reader_open_v1& request) {
        const qpx_native_json::Tree options({request.options.data, std::size_t(request.options.size)},
                                            &handle.arena.memory, request.cancel, 6, 7, 2);
        try {
            decode_points(handle, file, select(options), request.cancel);
        } catch (const parquet::ParquetException&) {
            fail("Parquet page or metadata decoding failed");
        }
    }
};
} // namespace

extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi, qpx_annotation_format_api_v3* out,
                               qpx_reader_text_v1* error) {
    using Plugin = qpx_annotation_source::Plugin<Definition, 3,
        provider_version, 2 * pool_limit>;
    return Plugin::entry(abi, out, error);
}
