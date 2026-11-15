#ifndef QUPATHXX_JPEG_MEMORY_BACKEND_H
#define QUPATHXX_JPEG_MEMORY_BACKEND_H
#include <qupathxx/reader_plugin.h>
#include <stdint.h>

/* This is the first member of the decoder context in cinfo->client_data.
 * It exists before jpeg_CreateDecompress initializes its memory manager. */
typedef struct qpx_jpeg_memory {
    uint64_t limit, used;
    qpx_reader_status_v1 status;
    char error[256];
} qpx_jpeg_memory;

void qpx_jpeg_memory_error(qpx_jpeg_memory* memory,
    qpx_reader_status_v1 status, const char* text);
void* qpx_jpeg_memory_allocate(qpx_jpeg_memory* memory, uint64_t bytes);
void qpx_jpeg_memory_release(qpx_jpeg_memory* memory, void* pointer);
#endif
