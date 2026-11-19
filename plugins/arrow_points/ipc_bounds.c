// SPDX-License-Identifier: AGPL-3.0-only
#include "ipc_bounds.h"
#include <nanoarrow/ipc/flatcc_generated.h>
#include <string.h>

#define arrow(x) FLATBUFFERS_WRAP_NAMESPACE(org_apache_arrow_flatbuf, x)

// Call only after nanoarrow's FlatBuffers verifier has accepted the message.
static int valid_schema(arrow(Schema_table_t) schema) {
    arrow(Field_vec_t) fields = arrow(Schema_fields(schema));
    size_t count = arrow(Field_vec_len(fields));
    if (!count || count > 64)
        return 0;
    for (size_t i = 0; i < count; ++i) {
        arrow(Field_table_t) field = arrow(Field_vec_at(fields, i));
        flatbuffers_string_t name = arrow(Field_name(field));
        if ((name && memchr(name, 0, flatbuffers_string_len(name))) ||
            arrow(Field_vec_len(arrow(Field_children(field)))))
            return 0;
    }
    return 1;
}

int qpx_arrow_footer_schema(const uint8_t* footer) {
    return valid_schema(arrow(Footer_schema(arrow(Footer_as_root(footer)))));
}

int qpx_arrow_stream_schema(const uint8_t* message) {
    return valid_schema((arrow(Schema_table_t))arrow(Message_header_get(arrow(Message_as_root(message)))));
}

int qpx_arrow_batch_bounds(const uint8_t* message, int64_t body_size, int64_t remaining_rows,
                           int dictionary) {
    flatbuffers_generic_t header = arrow(Message_header_get(arrow(Message_as_root(message))));
    arrow(RecordBatch_table_t) batch = dictionary
        ? arrow(DictionaryBatch_data((arrow(DictionaryBatch_table_t))header))
        : (arrow(RecordBatch_table_t))header;
    if (!batch)
        return 0;
    int64_t rows = arrow(RecordBatch_length(batch));
    if (rows < 0 || rows > remaining_rows)
        return 0;
    arrow(FieldNode_vec_t) nodes = arrow(RecordBatch_nodes(batch));
    for (size_t i = 0; i < arrow(FieldNode_vec_len(nodes)); ++i) {
        arrow(FieldNode_struct_t) node = arrow(FieldNode_vec_at(nodes, i));
        int64_t nulls = arrow(FieldNode_null_count(node));
        if (arrow(FieldNode_length(node)) != rows || nulls < 0 || nulls > rows)
            return 0;
    }
    arrow(Buffer_vec_t) buffers = arrow(RecordBatch_buffers(batch));
    int64_t end = 0;
    for (size_t i = 0; i < arrow(Buffer_vec_len(buffers)); ++i) {
        arrow(Buffer_struct_t) buffer = arrow(Buffer_vec_at(buffers, i));
        int64_t offset = arrow(Buffer_offset(buffer));
        int64_t size = arrow(Buffer_length(buffer));
        if (offset < end || offset > body_size || offset % 8 || size < 0 || size > body_size - offset)
            return 0;
        end = offset + size;
    }
    return 1;
}
