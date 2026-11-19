// SPDX-License-Identifier: AGPL-3.0-only
#include "arrow_file.hpp"
#include "ipc_bounds.h"

namespace qpx_arrow {
constexpr std::size_t metadata_limit = 1024 * 1024;

struct Schema {
    ArrowSchema value{};
    ArrowIpcDictionaryEncodings encodings{};
    ~Schema() {
        ArrowIpcDictionaryEncodingsReset(&encodings);
        if (value.release)
            value.release(&value);
    }
};

struct Dictionaries {
    ArrowIpcDictionaries value{};
    ~Dictionaries() { if (value.private_data) ArrowIpcDictionariesReset(&value); }
};

struct Decoder {
    ArrowIpcDecoder value{};
    Decoder() {
        valid(ArrowIpcDecoderInit(&value) == NANOARROW_OK, "Cannot allocate Arrow IPC decoder");
    }
    ~Decoder() { ArrowIpcDecoderReset(&value); }
};

ArrowBufferView view(Bytes bytes) {
    ArrowBufferView result{};
    result.data.as_uint8 = bytes.data;
    result.size_bytes = static_cast<int64_t>(bytes.size);
    return result;
}

bool same_text(const char* a, const char* b) {
    return (!a || !b) ? a == b : std::strcmp(a, b) == 0;
}

bool same_metadata(const char* a, const char* b) {
    const auto n = ArrowMetadataSizeOf(a);
    return n == ArrowMetadataSizeOf(b) && (!n || std::memcmp(a, b, std::size_t(n)) == 0);
}

bool same_field(const ArrowSchema& a, const ArrowSchema& b) {
    return same_text(a.name, b.name) && same_text(a.format, b.format) &&
           a.flags == b.flags && same_metadata(a.metadata, b.metadata);
}

bool is_text(ArrowType type) {
    return type == NANOARROW_TYPE_STRING || type == NANOARROW_TYPE_LARGE_STRING;
}

bool is_integer(ArrowType type) {
    switch (type) {
    case NANOARROW_TYPE_UINT8: case NANOARROW_TYPE_UINT16:
    case NANOARROW_TYPE_UINT32: case NANOARROW_TYPE_UINT64:
    case NANOARROW_TYPE_INT8: case NANOARROW_TYPE_INT16:
    case NANOARROW_TYPE_INT32: case NANOARROW_TYPE_INT64:
        return true;
    default:
        return false;
    }
}

void check_schema(const ArrowIpcFooter& index, const Schema& embedded, ArrowError& error) {
    const auto& footer = index.schema;
    const auto& stream = embedded.value;
    valid(footer.n_children > 0 && footer.n_children <= 64 &&
              stream.n_children == footer.n_children &&
              same_field(footer, stream) && !footer.dictionary && !stream.dictionary,
          "Arrow file and stream must agree on a flat schema of at most 64 columns");
    for (int i = 0; i < footer.n_children; ++i) {
        const auto& field = *footer.children[i];
        const auto& copy = *stream.children[i];
        valid(!field.n_children && !copy.n_children && same_field(field, copy) &&
                  bool(field.dictionary) == bool(copy.dictionary),
              "Arrow point import requires matching flat columns");
        const ArrowSchema* logical = &field;
        if (field.dictionary) {
            logical = field.dictionary;
            const auto* encoding = ArrowIpcDictionaryEncodingsFind(&index.dictionaries, &field);
            const auto* stream_encoding = ArrowIpcDictionaryEncodingsFind(&embedded.encodings, &copy);
            valid(encoding && stream_encoding && encoding->id >= 0 &&
                      encoding->id == stream_encoding->id && encoding->kind == stream_encoding->kind &&
                      same_field(*logical, *copy.dictionary),
                  "Arrow dictionary encodings disagree between schema and footer");
            const auto* first = ArrowIpcDictionaryEncodingsFindById(&index.dictionaries, encoding->id);
            valid(first && same_text(field.format, first->schema->format) &&
                      same_field(*logical, *first->schema->dictionary),
                  "Shared Arrow dictionary ID has inconsistent value or index types");
            ArrowSchemaView value_type{};
            valid(ArrowSchemaViewInit(&value_type, logical, &error) == NANOARROW_OK &&
                      (is_text(value_type.type) || is_integer(value_type.type) ||
                       value_type.type == NANOARROW_TYPE_FLOAT || value_type.type == NANOARROW_TYPE_DOUBLE) &&
                      !value_type.extension_name.size_bytes,
                  "Arrow import supports UTF-8, integer, Float32 or Float64 dictionary values only");
        }
    }
}

int find_column(const ArrowSchema& schema, std::string_view name) {
    int selected = -1;
    for (int i = 0; i < schema.n_children; ++i) {
        const auto* field = schema.children[i];
        if (field->name && name == field->name) {
            valid(selected == -1, "Selected Arrow column name is ambiguous");
            selected = i;
        }
    }
    valid(selected >= 0, "Selected Arrow column is missing");
    return selected;
}

ArrowType column_type(const ArrowSchema& column) {
    const auto* logical = column.dictionary ? column.dictionary : &column;
    ArrowSchemaView typed{};
    ArrowError error{};
    valid(ArrowSchemaViewInit(&typed, logical, &error) == NANOARROW_OK &&
              !typed.extension_name.size_bytes,
          "Selected Arrow columns must have native primitive types");
    return typed.type;
}

bool is_signed_integer(ArrowType type) {
    return type == NANOARROW_TYPE_INT8 || type == NANOARROW_TYPE_INT16 ||
           type == NANOARROW_TYPE_INT32 || type == NANOARROW_TYPE_INT64;
}

const ArrowArrayView* value_at(const ArrowArrayView& column, int64_t& row) {
    if (ArrowArrayViewIsNull(&column, row))
        return nullptr;
    const auto* values = column.dictionary;
    if (!values)
        return &column;
    const auto index = ArrowArrayViewGetUIntUnsafe(&column, row);
    valid(index < std::uint64_t(values->length), "Arrow dictionary index is outside its values");
    row = static_cast<int64_t>(index);
    return ArrowArrayViewIsNull(values, row) ? nullptr : values;
}

std::string_view string_at(const ArrowArrayView& column, int64_t row,
                           std::size_t limit, qpx_reader_cancel_v1 cancel) {
    const auto* values = value_at(column, row);
    if (!values)
        return {};
    const auto value = ArrowArrayViewGetStringUnsafe(values, row);
    valid(value.size_bytes >= 0 && std::uint64_t(value.size_bytes) <= limit,
          "Arrow point text exceeds its native length limit");
    if (!value.size_bytes)
        return {};
    const std::string_view text(value.data, std::size_t(value.size_bytes));
    utf8(text, cancel);
    return text;
}

struct Block {
    ArrowIpcFileBlock framing;
    bool dictionary;
};

std::pmr::vector<Block> blocks(const ArrowIpcFooter& footer, std::pmr::memory_resource* memory) {
    std::pmr::vector<Block> out(memory);
    auto read = [&](const ArrowBuffer& source, bool dictionary, std::size_t limit) {
        const auto count = std::size_t(source.size_bytes) / sizeof(ArrowIpcFileBlock);
        valid(count <= limit, "Arrow IPC file has too many indexed batches");
        int64_t previous = -1;
        for (std::size_t i = 0; i < count; ++i) {
            Block block{{}, dictionary};
            std::memcpy(&block.framing, source.data + i * sizeof(block.framing), sizeof(block.framing));
            valid(block.framing.offset > previous, "Arrow batches are out of file order");
            previous = block.framing.offset;
            out.push_back(block);
        }
    };
    read(footer.record_batch_blocks, false, 4096);
    valid(!out.empty(), "Arrow point file has no record batches");
    read(footer.dictionary_blocks, true, 64);
    std::sort(out.begin(), out.end(), [](const Block& a, const Block& b) {
        return a.framing.offset < b.framing.offset;
    });
    return out;
}

ArrowBufferView read_batch(Bytes file, const Block& block, ArrowIpcDecoder& decoder,
                           std::size_t limit, ArrowError& error) {
    const auto& frame = block.framing;
    const auto at = std::size_t(frame.offset);
    valid(file.le32(at) == UINT32_MAX, "Expected a V5 Arrow batch message");
    const auto metadata = view(file.sub(at, std::size_t(frame.metadata_length)));
    const auto expected = block.dictionary ? NANOARROW_IPC_MESSAGE_TYPE_DICTIONARY_BATCH
                                           : NANOARROW_IPC_MESSAGE_TYPE_RECORD_BATCH;
    valid(ArrowIpcDecoderVerifyHeader(&decoder, metadata, &error) == NANOARROW_OK &&
              ArrowIpcDecoderDecodeHeader(&decoder, metadata, &error) == NANOARROW_OK &&
              decoder.metadata_version == NANOARROW_IPC_METADATA_VERSION_V5 &&
              decoder.message_type == expected && decoder.header_size_bytes == frame.metadata_length &&
              decoder.body_size_bytes == frame.body_length,
          "Invalid Arrow IPC batch framing");
    valid(decoder.codec == NANOARROW_IPC_COMPRESSION_TYPE_NONE ||
              decoder.codec == NANOARROW_IPC_COMPRESSION_TYPE_LZ4_FRAME ||
              decoder.codec == NANOARROW_IPC_COMPRESSION_TYPE_ZSTD,
          "Arrow IPC buffer uses an unsupported compression type");
    valid(qpx_arrow_batch_bounds(metadata.data.as_uint8 + 8, frame.body_length,
                                 static_cast<int64_t>(limit), block.dictionary),
          "Arrow IPC batch exceeds its row or buffer bounds");
    return view(file.sub(at + std::size_t(frame.metadata_length), std::size_t(frame.body_length)));
}

void read_file(Bytes file, std::pmr::memory_resource* memory,
               qpx_reader_cancel_v1 cancel, Consumer& consumer) {
    valid(file.size >= 26 && file.sub(0, 8).view() == std::string_view("ARROW1\0\0", 8) &&
              file.sub(file.size - 6, 6).view() == "ARROW1",
          "Expected an Arrow IPC file or Feather v2");
    const auto footer_size = file.le32(file.size - 10);
    valid(footer_size && footer_size <= metadata_limit && footer_size <= file.size - 26,
          "Arrow file footer is outside the metadata limit");
    const auto footer_at = file.size - 10 - footer_size;
    Decoder index;
    ArrowError error{};
    valid(ArrowIpcDecoderVerifyFooter(&index.value, view(file), &error) == NANOARROW_OK &&
              index.value.metadata_version == NANOARROW_IPC_METADATA_VERSION_V5 &&
              qpx_arrow_footer_schema(file.data + footer_at) &&
              ArrowIpcDecoderDecodeFooter(&index.value, view(file), &error) == NANOARROW_OK &&
              index.value.endianness == NANOARROW_IPC_ENDIANNESS_LITTLE,
          "Expected a verified little-endian Arrow IPC V5 footer");
    const auto& footer = *index.value.footer;
    const auto index_blocks = blocks(footer, memory);

    std::size_t at = 8;
    valid(file.le32(at) == UINT32_MAX, "Expected a V5 Arrow schema message");
    const std::size_t schema_size = std::size_t(file.le32(at + 4)) + 8;
    valid(schema_size > 8 && schema_size <= metadata_limit && schema_size % 8 == 0 &&
              schema_size <= footer_at - at,
          "Arrow IPC schema message is outside the metadata limit");
    const auto schema_message = view(file.sub(at, schema_size));
    Schema stream;
    // Verifying a new message clears that decoder's current footer.
    Decoder ipc;
    auto& decoder = ipc.value;
    valid(ArrowIpcDecoderVerifyHeader(&decoder, schema_message, &error) == NANOARROW_OK &&
              ArrowIpcDecoderDecodeHeader(&decoder, schema_message, &error) == NANOARROW_OK &&
              decoder.metadata_version == NANOARROW_IPC_METADATA_VERSION_V5 &&
              decoder.message_type == NANOARROW_IPC_MESSAGE_TYPE_SCHEMA &&
              decoder.endianness == NANOARROW_IPC_ENDIANNESS_LITTLE && !decoder.body_size_bytes &&
              qpx_arrow_stream_schema(schema_message.data.as_uint8 + 8) &&
              ArrowIpcDecoderDecodeSchemaWithDictionaries(&decoder, &stream.value,
                  &stream.encodings, &error) == NANOARROW_OK,
          "Invalid Arrow IPC stream schema");
    check_schema(footer, stream, error);
    consumer.schema(footer.schema);
    Dictionaries dictionaries;
    valid(ArrowIpcDictionariesInit(&dictionaries.value, &footer.dictionaries, &error) == NANOARROW_OK &&
              ArrowIpcDecoderSetSchemaWithDictionaries(&decoder, &footer.schema,
                  &footer.dictionaries, &error) == NANOARROW_OK &&
              ArrowIpcDecoderSetEndianness(&decoder, NANOARROW_IPC_ENDIANNESS_LITTLE) == NANOARROW_OK,
          "Cannot use Arrow IPC schema");
    at += schema_size;
    std::pmr::vector<int64_t> defined(memory);
    for (const auto& entry : index_blocks) {
        poll(cancel);
        const auto& block = entry.framing;
        valid(block.offset == static_cast<int64_t>(at) && block.metadata_length > 8 &&
                  block.metadata_length <= static_cast<int32_t>(metadata_limit) &&
                  block.metadata_length % 8 == 0 && block.body_length >= 0 &&
                  block.body_length % 8 == 0 && std::size_t(block.metadata_length) <= footer_at - at,
              "Arrow IPC record block does not match its file index");
        at += std::size_t(block.metadata_length);
        valid(std::uint64_t(block.body_length) <= footer_at - at, "Arrow IPC body is outside the file index");
        at += std::size_t(block.body_length);
        if (!entry.dictionary)
            continue;
        const auto body = read_batch(file, entry, decoder, row_limit, error);
        const auto dictionary_id = decoder.dictionary->id;
        const bool delta = decoder.dictionary->is_delta != 0;
        const bool has_base = std::find(defined.begin(), defined.end(), dictionary_id) != defined.end();
        valid(delta == has_base, "Arrow dictionary needs one base before any deltas");
        valid(ArrowIpcDecoderDecodeDictionary(&decoder, body, NANOARROW_VALIDATION_LEVEL_FULL,
                  &dictionaries.value, &error) == NANOARROW_OK,
              "Invalid Arrow IPC dictionary or decoder work limit exceeded");
        if (!delta)
            defined.push_back(dictionary_id);
    }
    valid(footer_at == at ||
              (footer_at - at == 8 && file.le32(at) == UINT32_MAX && file.le32(at + 4) == 0),
          "Arrow import requires a complete indexed file");
    std::size_t rows = 0;
    for (const auto& entry : index_blocks) {
        if (entry.dictionary)
            continue;
        poll(cancel);
        const auto body = read_batch(file, entry, decoder, row_limit - rows, error);
        ArrowArrayView* batch = nullptr;
        valid(ArrowIpcDecoderDecodeArrayViewWithDictionaries(&decoder, body, -1,
                  &dictionaries.value, &batch, &error) == NANOARROW_OK &&
                  batch->length >= 0 && std::uint64_t(batch->length) <= row_limit - rows &&
                  ArrowArrayViewValidate(batch, NANOARROW_VALIDATION_LEVEL_FULL, &error) == NANOARROW_OK,
              "Invalid Arrow IPC values or decoder work limit exceeded");
        consumer.batch(*batch);
        rows += static_cast<std::size_t>(batch->length);
    }
    valid(rows > 0, "Arrow import requires nonempty indexed rows");
}
} // namespace qpx_arrow
