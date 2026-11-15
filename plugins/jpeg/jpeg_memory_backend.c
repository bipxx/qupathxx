#include "jpeg_memory_backend.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define JPEG_INTERNALS
#include <jpeglib.h>
#include <jmemsys.h>

typedef union Header {
    uint64_t bytes;
    long double alignment;
    void* pointer;
    unsigned char padding[16];
} Header;

void qpx_jpeg_memory_error(qpx_jpeg_memory* memory,
    qpx_reader_status_v1 status, const char* text) {
    size_t length = strlen(text);
    if (length >= sizeof memory->error) length = sizeof memory->error - 1;
    memory->status = status;
    memcpy(memory->error, text, length); memory->error[length] = 0;
}

void* qpx_jpeg_memory_allocate(qpx_jpeg_memory* memory, uint64_t bytes) {
    Header* header;
    uint64_t charged;
    if (bytes > UINT64_MAX - sizeof(Header)) {
        qpx_jpeg_memory_error(memory, QPX_READER_INVALID_REQUEST, "JPEG allocation size overflow");
        return NULL;
    }
    charged = bytes + sizeof(Header);
    if (memory->used > memory->limit || charged > memory->limit - memory->used || charged > SIZE_MAX) {
        qpx_jpeg_memory_error(memory, QPX_READER_INVALID_REQUEST, "JPEG decoded data exceeds reader work budget");
        return NULL;
    }
    header = (Header*)calloc(1, (size_t)charged);
    if (!header) {
        qpx_jpeg_memory_error(memory, QPX_READER_INTERNAL_ERROR, "JPEG allocation failed");
        return NULL;
    }
    header->bytes = charged; memory->used += charged;
    return header + 1;
}

void qpx_jpeg_memory_release(qpx_jpeg_memory* memory, void* pointer) {
    Header* header;
    if (!pointer) return;
    header = (Header*)pointer - 1;
    memory->used -= header->bytes; free(header);
}

static qpx_jpeg_memory* budget(j_common_ptr cinfo) {
    return (qpx_jpeg_memory*)cinfo->client_data;
}

/* These eight backend definitions deliberately replace jmemnobs.c. No decoder
 * allocation escapes the per-open budget, and no temporary file is created. */
GLOBAL(void*) jpeg_get_small(j_common_ptr cinfo, size_t bytes) {
    return qpx_jpeg_memory_allocate(budget(cinfo), (uint64_t)bytes);
}
GLOBAL(void) jpeg_free_small(j_common_ptr cinfo, void* pointer, size_t bytes) {
    (void)bytes; qpx_jpeg_memory_release(budget(cinfo), pointer);
}
GLOBAL(void*) jpeg_get_large(j_common_ptr cinfo, size_t bytes) {
    return qpx_jpeg_memory_allocate(budget(cinfo), (uint64_t)bytes);
}
GLOBAL(void) jpeg_free_large(j_common_ptr cinfo, void* pointer, size_t bytes) {
    (void)bytes; qpx_jpeg_memory_release(budget(cinfo), pointer);
}
GLOBAL(size_t) jpeg_mem_available(j_common_ptr cinfo, size_t minimum,
    size_t maximum, size_t already_allocated) {
    qpx_jpeg_memory* memory = budget(cinfo);
    uint64_t available = memory->limit - memory->used;
    (void)minimum; (void)already_allocated;
    return available < maximum ? (size_t)available : maximum;
}
GLOBAL(void) jpeg_open_backing_store(j_common_ptr cinfo, backing_store_ptr info, long bytes) {
    (void)info; (void)bytes;
    qpx_jpeg_memory_error(budget(cinfo), QPX_READER_INVALID_REQUEST,
        "JPEG decoded data exceeds reader work budget; disk backing is disabled");
    ERREXIT(cinfo, JERR_NO_BACKING_STORE);
}
GLOBAL(long) jpeg_mem_init(j_common_ptr cinfo) {
    const uint64_t limit = budget(cinfo)->limit;
    return limit < LONG_MAX ? (long)limit : LONG_MAX;
}
GLOBAL(void) jpeg_mem_term(j_common_ptr cinfo) {(void)cinfo;}
