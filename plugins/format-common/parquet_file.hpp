// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "binary_input.hpp"
#include <arrow/io/memory.h>
#include <arrow/memory_pool.h>
#include <parquet/column_reader.h>
#include <parquet/file_reader.h>
#include <array>
#include <charconv>
#include <optional>
#include <type_traits>

namespace qpx_parquet {
using namespace qpx_binary;
inline constexpr std::size_t rows_limit = 100000;
inline constexpr int metadata_limit = 1024 * 1024;

class BoundedPool final : public arrow::ProxyMemoryPool {
public:
    explicit BoundedPool(std::size_t limit)
        : arrow::ProxyMemoryPool(arrow::system_memory_pool()), limit_(static_cast<int64_t>(limit)) {}

    arrow::Status Allocate(int64_t size, int64_t alignment, uint8_t** out) override {
        if (size < 0 || size > limit_ - bytes_allocated())
            return arrow::Status::OutOfMemory("Parquet decoder work limit exceeded");
        return arrow::ProxyMemoryPool::Allocate(size, alignment, out);
    }

    arrow::Status Reallocate(int64_t old_size, int64_t new_size, int64_t alignment, uint8_t** out) override {
        if (old_size < 0 || old_size > bytes_allocated() || new_size < 0 ||
            new_size > limit_ - (bytes_allocated() - old_size))
            return arrow::Status::OutOfMemory("Parquet decoder work limit exceeded");
        return arrow::ProxyMemoryPool::Reallocate(old_size, new_size, alignment, out);
    }
private:
    const int64_t limit_;
};

inline int column_index(const parquet::SchemaDescriptor& schema, std::string_view name) {
    int selected = -1;
    for (int i = 0; i < schema.num_columns(); ++i) {
        if (name != schema.Column(i)->name())
            continue;
        valid(selected == -1, "Selected Parquet column name is ambiguous");
        selected = i;
    }
    valid(selected >= 0, "Selected Parquet column is missing");
    return selected;
}

struct NumericColumn {
    int index;
    parquet::Type::type physical;
    int width;
    bool signed_value;
};

inline NumericColumn numeric_column(const parquet::SchemaDescriptor& schema, int index,
                                    bool allow_integral, bool allow_real) {
    const auto& column = *schema.Column(index);
    const auto type = column.physical_type();
    const auto& logical = *column.logical_type();
    if (allow_real && logical.is_none() && (type == parquet::Type::FLOAT || type == parquet::Type::DOUBLE))
        return {index, type, 0, true};
    valid(allow_integral && (type == parquet::Type::INT32 || type == parquet::Type::INT64) &&
              (logical.is_int() || logical.is_none()),
          allow_real ? "Expected Parquet Float32 or Float64 feature columns or an admitted coordinate type"
                     : "Parquet raw counts require integer feature columns");
    const int physical_width = type == parquet::Type::INT32 ? 32 : 64;
    if (logical.is_none())
        return {index, type, physical_width, true};
    const auto& integer = static_cast<const parquet::IntLogicalType&>(logical);
    valid(integer.bit_width() <= physical_width && integer.bit_width() > 0,
          "Parquet integer logical width exceeds its physical type");
    return {index, type, integer.bit_width(), integer.is_signed()};
}

template<class Physical>
std::uint64_t integer_bits(Physical sample, const NumericColumn& feature) {
    const auto bits = std::uint64_t(static_cast<std::make_unsigned_t<Physical>>(sample));
    valid(!feature.signed_value || sample >= 0, "Parquet selected integers cannot be negative");
    const int magnitude = feature.width - int(feature.signed_value);
    valid(magnitude == 64 || bits < (std::uint64_t{1} << magnitude),
          "Parquet value exceeds its logical integer width");
    return bits;
}

inline void validate_chunk(const parquet::ColumnChunkMetaData& chunk, std::size_t footer_at,
                           int64_t rows, std::size_t limit, bool repeated = false) {
    const auto compression = chunk.compression();
    valid(chunk.file_path().empty() &&
              (compression == parquet::Compression::UNCOMPRESSED ||
               compression == parquet::Compression::SNAPPY || compression == parquet::Compression::ZSTD),
          "Parquet column must be local and use uncompressed, Snappy or Zstandard pages");
    const auto data_at = chunk.data_page_offset();
    const auto begin = chunk.has_dictionary_page() ? chunk.dictionary_page_offset() : data_at;
    const auto compressed = chunk.total_compressed_size();
    const auto uncompressed = chunk.total_uncompressed_size();
    const auto values = chunk.num_values();
    valid((repeated ? values >= rows && values <= 1000000 : values == rows) &&
              begin >= 4 && std::uint64_t(begin) < footer_at && data_at >= begin &&
              compressed > 0 && std::uint64_t(compressed) <= footer_at - std::uint64_t(begin) &&
              data_at - begin < compressed && uncompressed > 0 && std::uint64_t(uncompressed) <= limit,
          "Parquet column pages are outside the source or decoder limit");
}

inline bool text_column(const parquet::ColumnDescriptor& column) {
    return column.physical_type() == parquet::Type::BYTE_ARRAY && column.logical_type()->is_string();
}

struct Group {
    parquet::RowGroupReader& group;
    std::size_t footer_at, pool_limit;
    int64_t rows;
    qpx_reader_cancel_v1 cancel;

    template<class ParquetType, class Append>
    void read(int index, bool nullable, Append append) const {
        validate_chunk(*group.metadata()->ColumnChunk(index), footer_at, rows, pool_limit);
        auto input = group.Column(index);
        auto& reader = static_cast<parquet::TypedColumnReader<ParquetType>&>(*input);
        const auto max_definition = reader.descr()->max_definition_level();
        constexpr int batch_size = 512;
        std::array<typename ParquetType::c_type, batch_size> samples;
        std::array<int16_t, batch_size> definitions;
        for (int64_t offset = 0; offset < rows;) {
            poll(cancel);
            int64_t filled = 0;
            const auto read = reader.ReadBatch(std::min<int64_t>(batch_size, rows - offset),
                                               definitions.data(), nullptr, samples.data(), &filled);
            valid(read > 0 && read <= rows - offset && filled >= 0 && filled <= read &&
                      (nullable || read == filled), "Parquet column has missing or null values");
            int64_t at = 0;
            for (int64_t row = 0; row < read; ++row) {
                if (!max_definition || definitions[std::size_t(row)] == max_definition) {
                    valid(at < filled, "Parquet definition levels differ from its values");
                    append(std::size_t(offset + row), &samples[std::size_t(at++)]);
                } else {
                    valid(nullable && definitions[std::size_t(row)] == 0,
                          "Parquet column has a null value or invalid definition level");
                    append(std::size_t(offset + row), nullptr);
                }
            }
            valid(at == filled, "Parquet definition levels differ from its values");
            offset += read;
        }
    }

    template<class Append>
    void strings(int index, bool nullable, std::size_t max_size, Append append) const {
        read<parquet::ByteArrayType>(index, nullable, [&](std::size_t row, const parquet::ByteArray* sample) {
            if (!sample || !sample->len) {
                valid(nullable, "Parquet ID is empty or null");
                append(row, std::string_view{});
                return;
            }
            valid(sample->ptr && sample->len <= max_size, "Invalid Parquet text length");
            const std::string_view value(reinterpret_cast<const char*>(sample->ptr), sample->len);
            utf8(value, cancel);
            append(row, value);
        });
    }

    template<class Integer, class Real>
    void numbers(const NumericColumn& column, Integer integer, Real real) const {
        switch (column.physical) {
        case parquet::Type::INT32:
            read<parquet::Int32Type>(column.index, false, [&](std::size_t row, const int32_t* sample) {
                integer(row, integer_bits(*sample, column));
            });
            break;
        case parquet::Type::INT64:
            read<parquet::Int64Type>(column.index, false, [&](std::size_t row, const int64_t* sample) {
                integer(row, integer_bits(*sample, column));
            });
            break;
        case parquet::Type::FLOAT:
            read<parquet::FloatType>(column.index, false, [&](std::size_t row, const float* sample) {
                real(row, *sample);
            });
            break;
        case parquet::Type::DOUBLE:
            read<parquet::DoubleType>(column.index, false, [&](std::size_t row, const double* sample) {
                real(row, *sample);
            });
            break;
        default:
            fail("Unexpected Parquet numeric column type");
        }
    }
};

inline std::optional<NumericColumn> numeric_id_column(const parquet::SchemaDescriptor& schema, int index) {
    const auto& column = *schema.Column(index);
    if (text_column(column))
        return {};
    const auto type = column.physical_type();
    valid((type == parquet::Type::INT32 || type == parquet::Type::INT64) &&
              (column.logical_type()->is_none() || column.logical_type()->is_int()),
          "Parquet ID column must be UTF-8 or an integer");
    const auto numeric = numeric_column(schema, index, true, false);
    valid(type == parquet::Type::INT64 ? numeric.width == 64
                                      : numeric.width == 8 || numeric.width == 16 || numeric.width == 32,
          "Parquet ID has an invalid logical integer width");
    return numeric;
}

template<class Physical>
std::string_view integer_id(Physical sample, const NumericColumn& type, std::array<char, 24>& text) {
    std::to_chars_result encoded;
    if (type.signed_value) {
        if (type.width < 64) {
            const auto bound = std::int64_t{1} << (type.width - 1);
            valid(sample >= -bound && sample < bound, "Parquet ID exceeds its logical integer width");
        }
        encoded = std::to_chars(text.data(), text.data() + text.size(), sample);
    } else {
        encoded = std::to_chars(text.data(), text.data() + text.size(), integer_bits(sample, type));
    }
    return {text.data(), std::size_t(encoded.ptr - text.data())};
}

template<class Append>
void read_integer_ids(const Group& group, const NumericColumn& type, const Append& append) {
    auto read = [&]<class Storage>() {
        group.read<Storage>(type.index, false,
                            [&](std::size_t row, const typename Storage::c_type* sample) {
            std::array<char, 24> text;
            append(row, integer_id(*sample, type, text));
        });
    };
    if (type.physical == parquet::Type::INT32)
        read.template operator()<parquet::Int32Type>();
    else
        read.template operator()<parquet::Int64Type>();
}

inline std::string integer_id_type(const NumericColumn& type) {
    return (type.signed_value ? "int" : "uint") + std::to_string(type.width);
}

class File {
public:
    enum class Layout { Flat, Nested };
    File(Bytes file, std::size_t pool_limit, Layout layout = Layout::Flat)
        : pool_limit_(pool_limit), pool_(pool_limit) {
        valid(file.size >= 12 && file.sub(0, 4).view() == "PAR1" && file.sub(file.size - 4, 4).view() == "PAR1",
              "Expected a local unencrypted Parquet file");
        const auto footer_size = file.le32(file.size - 8);
        valid(footer_size && footer_size <= metadata_limit && footer_size <= file.size - 12,
              "Parquet footer is outside the metadata limit");
        footer_at_ = file.size - 8 - footer_size;
        parquet::ReaderProperties properties(&pool_);
        properties.set_thrift_string_size_limit(metadata_limit);
        properties.set_thrift_container_size_limit(4096);
        properties.set_page_checksum_verification(true);
        auto bytes = std::make_shared<arrow::Buffer>(file.data, static_cast<int64_t>(file.size));
        reader_ = parquet::ParquetFileReader::Open(std::make_shared<arrow::io::BufferReader>(bytes), properties);
        metadata_ = reader_->metadata();
        const auto& meta = *metadata_;
        valid(meta.num_rows() > 0 && std::uint64_t(meta.num_rows()) <= rows_limit && meta.num_row_groups() > 0 &&
                  meta.num_row_groups() <= 4096 && meta.num_columns() <= 64 && meta.num_columns() > 0,
              "Parquet file exceeds the row, row group or column limit");
        if (layout == Layout::Flat)
            require_flat();
    }

    void require_flat() const {
        valid(schema().group_node()->field_count() == schema().num_columns(), "Parquet file requires flat columns");
        for (int i = 0; i < schema().num_columns(); ++i) {
            const auto* column = schema().Column(i);
            valid(!column->max_repetition_level() && column->max_definition_level() <= 1 &&
                      schema().group_node()->field(i)->is_primitive(), "Parquet file requires flat nonrepeated columns");
        }
    }

    const parquet::SchemaDescriptor& schema() const { return *metadata_->schema(); }
    const auto& key_value_metadata() const { return metadata_->key_value_metadata(); }
    std::size_t rows() const { return static_cast<std::size_t>(metadata_->num_rows()); }
    int group_count() const { return metadata_->num_row_groups(); }
    std::unique_ptr<parquet::ParquetFileReader> take_reader() { return std::move(reader_); }
    arrow::MemoryPool* pool() { return &pool_; }
    std::unique_ptr<parquet::RowGroupMetaData> projection(int group, const std::vector<int>& indices) const {
        auto meta = metadata_->RowGroup(group);
        valid(meta->num_rows() > 0 && std::uint64_t(meta->num_rows()) <= rows() &&
                  meta->num_columns() == schema().num_columns(), "Parquet row group dimensions differ");
        for (int index : indices) {
            const auto level = schema().Column(index)->max_repetition_level();
            valid(level <= 3, "GeoArrow repetition exceeds the separated XY profile");
            validate_chunk(*meta->ColumnChunk(index), footer_at_, meta->num_rows(), pool_limit_, level != 0);
        }
        return meta;
    }

    template<class Visit>
    void groups(qpx_reader_cancel_v1 cancel, Visit visit) {
        std::size_t offset = 0;
        for (int i = 0; i < metadata_->num_row_groups(); ++i) {
            poll(cancel);
            const auto group = reader_->RowGroup(i);
            const auto* meta = group->metadata();
            const auto size = meta->num_rows();
            valid(size > 0 && std::uint64_t(size) <= rows() - offset &&
                      meta->num_columns() == schema().num_columns(), "Parquet row group dimensions differ");
            visit(offset, Group{*group, footer_at_, pool_limit_, size, cancel});
            offset += std::size_t(size);
        }
        valid(offset == rows(), "Parquet row groups do not cover their declared rows");
    }

private:
    const std::size_t pool_limit_;
    std::size_t footer_at_ = 0;
    BoundedPool pool_;
    std::unique_ptr<parquet::ParquetFileReader> reader_;
    std::shared_ptr<parquet::FileMetaData> metadata_;
};
} // namespace qpx_parquet
