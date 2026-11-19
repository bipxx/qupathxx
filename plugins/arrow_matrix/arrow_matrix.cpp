// SPDX-License-Identifier: AGPL-3.0-only
#include "../arrow_points/arrow_file.hpp"
#include "../format-common/matrix_table.hpp"

namespace {
using namespace qpx_arrow;
using qpx_matrix_table::Source;

struct MatrixConsumer final : Consumer {
    Source& data;
    std::pmr::vector<int> feature_indices;
    int observation_index = -1;
    ArrowType observation_type = NANOARROW_TYPE_UNINITIALIZED;

    explicit MatrixConsumer(Source& data) : data(data), feature_indices(&data.arena.memory) {}

    void schema(const ArrowSchema& schema) override {
        observation_index = find_column(schema, data.observation_column);
        observation_type = column_type(*schema.children[observation_index]);
        valid(is_text(observation_type) || is_integer(observation_type),
              "Arrow matrix observation ID column must be UTF-8 or an integer");
        for (const auto& feature : data.features) {
            const int index = find_column(schema, feature);
            const auto type = column_type(*schema.children[index]);
            if (data.counts()) {
                valid(is_integer(type), "Arrow raw counts require integer feature columns");
            } else {
                valid(type == NANOARROW_TYPE_FLOAT || type == NANOARROW_TYPE_DOUBLE,
                      "Arrow continuous matrices require Float32 or Float64 feature columns");
            }
            feature_indices.push_back(index);
        }
    }

    std::uint64_t sample(const ArrowArrayView& field, int64_t row) const {
        const auto* values = value_at(field, row);
        valid(values, "Arrow matrix feature value is null");
        if (data.counts()) {
            valid(!is_signed_integer(values->storage_type) || ArrowArrayViewGetIntUnsafe(values, row) >= 0,
                  "Arrow raw counts cannot be negative");
            return ArrowArrayViewGetUIntUnsafe(values, row);
        }
        return qpx_matrix_table::continuous_bits(ArrowArrayViewGetDoubleUnsafe(values, row));
    }

    void batch(const ArrowArrayView& batch) override {
        const auto& ids = *batch.children[observation_index];
        const bool integer = is_integer(observation_type);
        for (int64_t row = 0; row < batch.length; ++row) {
            poll(data.cancel);
            if (integer) {
                auto index = row;
                const auto* values = value_at(ids, index);
                valid(values, "Arrow matrix observation ID is empty or null");
                valid(values->storage_type == observation_type, "Arrow observation ID storage differs from its schema");
                const auto id = is_signed_integer(observation_type)
                    ? std::to_string(ArrowArrayViewGetIntUnsafe(values, index))
                    : std::to_string(ArrowArrayViewGetUIntUnsafe(values, index));
                data.observations.emplace_back(id);
            } else {
                const auto id = string_at(ids, row, 256, data.cancel);
                valid(!id.empty(), "Arrow matrix observation ID is empty or null");
                data.observations.emplace_back(id);
            }
            for (int index : feature_indices)
                data.values.push_back(sample(*batch.children[index], row));
        }
    }
};
struct Definition {
    static constexpr const char *id = "org.qupathxx.arrow-matrix", *format = "arrow-matrix",
        *name = "Arrow IPC expression matrix", *version = QPX_COLUMNAR_MATRIX_VERSION,
        *contract = QPX_COLUMNAR_MATRIX_CONTRACT;
    static constexpr std::size_t private_work = QPX_ARROW_DECODER_WORK;

    static void decode(Bytes file, Source& data) {
        MatrixConsumer consumer(data);
        read_file(file, &data.arena.memory, data.cancel, consumer);
    }
};
} // namespace

extern "C" QPX_MATRIX_TYPED_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v2(
    std::uint32_t requested, qpx_matrix_format_api_v2* out, qpx_reader_text_v1* error) {
    return qpx_matrix_table::Plugin<Definition>::entry(requested, out, error);
}
