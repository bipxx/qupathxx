#ifndef QUPATHXX_PNG_DECODER_H
#define QUPATHXX_PNG_DECODER_H
#include <qupathxx/reader_plugin.h>
#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Decoder-owned storage. Original PNG samples are packed native-endian HWC;
 * sub-byte grayscale and palette indices retain their unscaled integer values.
 * RGBA is a separate straight-alpha display representation. */
typedef struct qpx_png_image {
    uint32_t width, height, color_type, bit_depth, channels, sample_bytes;
    uint32_t calibration_flags;
    double pixel_width_um, pixel_height_um;
    uint64_t raw_bytes, rgba_bytes;
    unsigned char* raw;
    unsigned char* rgba;
    void* allocation_owner;
} qpx_png_image;

/* All libpng errors/longjmps terminate within this C function. The FILE and
 * callback are borrowed for this call only, and are never retained in image. */
qpx_reader_status_v1 qpx_png_decode(FILE* input, uint64_t encoded_bytes,
    uint64_t work_bytes, qpx_reader_cancel_v1 cancel, qpx_png_image** image,
    char* error, size_t error_capacity);
void qpx_png_destroy(qpx_png_image* image);

#ifdef __cplusplus
}
#endif
#endif
