#ifndef QUPATHXX_JPEG_DECODER_H
#define QUPATHXX_JPEG_DECODER_H
#include <qupathxx/reader_plugin.h>
#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Owned canonical DECODED samples, not pre-compression pixels. Orientation is
 * applied before exposure. UInt8 Gray/RGB HWC and separate opaque RGBA8. */
typedef struct qpx_jpeg_image {
    uint32_t width, height, channels, orientation;
    uint32_t calibration_flags;
    double pixel_width_um, pixel_height_um;
    uint64_t raw_bytes, rgba_bytes;
    unsigned char* raw;
    unsigned char* rgba;
    void* allocation_owner;
} qpx_jpeg_image;

qpx_reader_status_v1 qpx_jpeg_decode(FILE* input, uint64_t encoded_bytes,
    uint64_t work_bytes, qpx_reader_cancel_v1 cancel, qpx_jpeg_image** image,
    char* error, size_t error_capacity);
void qpx_jpeg_destroy(qpx_jpeg_image* image);
#ifdef __cplusplus
}
#endif
#endif
