// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/matrix_table.hpp"
#include "../format-common/parquet_file.hpp"

namespace {
using namespace qpx_matrix_table;
using namespace qpx_parquet;
constexpr int pool_limit = 8 * 1024 * 1024;

void decode_parquet(Bytes file, Source& data) {
    File source(file, pool_limit);
    const auto& schema = source.schema();
    const int id = column_index(schema, data.observation_column);
    const auto integer = numeric_id_column(schema, id);
    std::pmr::vector<NumericColumn> features(&data.arena.memory);
    for (const auto& label : data.features)
        features.push_back(numeric_column(schema, column_index(schema, label), data.counts(), !data.counts()));
    data.observations.reserve(source.rows());
    data.values.resize(source.rows() * features.size());
    source.groups(data.cancel, [&](std::size_t offset, const Group& group) {
        const auto append_id = [&](std::size_t, std::string_view label) {
            data.observations.emplace_back(label);
        };
        if (integer)
            read_integer_ids(group, *integer, append_id);
        else
            group.strings(id, false, 256, append_id);
        for (std::size_t col = 0; col < features.size(); ++col) {
            const auto put = [&](std::size_t row, std::uint64_t bits) {
                data.values[(offset + row) * features.size() + col] = bits;
            };
            group.numbers(features[col], put, [&](std::size_t row, double value) {
                put(row, continuous_bits(value));
            });
        }
    });
}

struct Definition {
    static constexpr const char *id = "org.qupathxx.parquet-matrix", *format = "parquet-matrix",
        *name = "Parquet expression matrix", *version = QPX_COLUMNAR_MATRIX_VERSION,
        *contract = QPX_COLUMNAR_MATRIX_CONTRACT;
    static constexpr std::size_t private_work = 2 * pool_limit;

    static void decode(Bytes file, Source& data) {
        try {
            decode_parquet(file, data);
        } catch (const parquet::ParquetException&) {
            fail("Parquet page or metadata decoding failed");
        }
    }
};
} // namespace

extern "C" QPX_MATRIX_TYPED_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v2(
    std::uint32_t requested, qpx_matrix_format_api_v2* out, qpx_reader_text_v1* error) {
    return qpx_matrix_table::Plugin<Definition>::entry(requested, out, error);
}
