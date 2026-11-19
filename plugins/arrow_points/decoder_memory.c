// SPDX-License-Identifier: AGPL-3.0-only
#include "decoder_memory.h"
#include <stdlib.h>
#include <string.h>

// The Windows x64 CRT returns 16-byte-aligned allocations.
typedef struct __declspec(align(16)) {
    size_t size;
    size_t padding;
} Header;

// The decoder and the serial codec allocate and release on the calling thread.
static __declspec(thread) size_t used;

void* qpx_arrow_malloc(size_t size) {
    if (size > QPX_ARROW_DECODER_WORK - used ||
        sizeof(Header) > QPX_ARROW_DECODER_WORK - used - size)
        return NULL;
    Header* header = malloc(sizeof(Header) + size);
    if (!header)
        return NULL;
    header->size = sizeof(Header) + size;
    used += header->size;
    return header + 1;
}

void qpx_arrow_free(void* ptr) {
    if (!ptr)
        return;
    Header* header = (Header*)ptr - 1;
    used -= header->size;
    free(header);
}

void* qpx_arrow_realloc(void* ptr, size_t size) {
    if (!ptr)
        return qpx_arrow_malloc(size);
    Header* header = (Header*)ptr - 1;
    size_t other = used - header->size;
    if (size > QPX_ARROW_DECODER_WORK - other ||
        sizeof(Header) > QPX_ARROW_DECODER_WORK - other - size)
        return NULL;
    Header* resized = realloc(header, sizeof(Header) + size);
    if (!resized)
        return NULL;
    resized->size = sizeof(Header) + size;
    used = other + resized->size;
    return resized + 1;
}

void* qpx_arrow_calloc(size_t count, size_t size) {
    if (size && count > QPX_ARROW_DECODER_WORK / size)
        return NULL;
    size_t bytes = count * size;
    void* ptr = qpx_arrow_malloc(bytes);
    if (ptr)
        memset(ptr, 0, bytes);
    return ptr;
}
