#ifndef QUPATHXX_WEBP_MEMORY_BACKEND_H
#define QUPATHXX_WEBP_MEMORY_BACKEND_H
/* Include the CRT declarations before the source-local allocator macros. */
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <qupathxx/reader_plugin.h>

#ifdef __cplusplus
extern "C" {
#endif
qpx_reader_status_v1 qpx_webp_decode_rgba(const uint8_t* data,size_t size,
    uint8_t* rgba,size_t capacity,int width,int height,uint64_t codec_work_bytes,
    qpx_reader_cancel_v1 cancel);
void* qpx_webp_malloc(size_t bytes);
void* qpx_webp_calloc(size_t count,size_t bytes);
void qpx_webp_free(void* pointer);
#ifdef __cplusplus
}
#endif

/* Only pinned upstream src/utils/utils.c receives this definition and the
 * forced include. The backend and C++ adapter always use the real CRT. */
#if defined(QPX_WEBP_ALLOCATOR_INTERPOSE)
#define malloc(bytes) qpx_webp_malloc(bytes)
#define calloc(count,bytes) qpx_webp_calloc(count,bytes)
#define free(pointer) qpx_webp_free(pointer)
#endif
#endif
