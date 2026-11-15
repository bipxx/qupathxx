#include "png_decoder.h"
#include <png.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Keep every object modified after setjmp on the heap. A libpng jump never
 * crosses C++ destructors or depends on indeterminate automatic variables. */
typedef struct Decode {
    qpx_png_image image;
    png_structp png;
    png_infop info, end;
    FILE* input;
    qpx_reader_cancel_v1 cancel;
    uint64_t limit, used, encoded_bytes, consumed;
    unsigned char* packed;
    qpx_reader_status_v1 status;
    char error[256];
} Decode;

typedef union AllocationHeader {
    uint64_t bytes;
    long double align_long_double;
    void* align_pointer;
    unsigned char alignment[16];
} AllocationHeader;

static void message(Decode* d, qpx_reader_status_v1 status, const char* text) {
    size_t length;
    d->status = status;
    length = strlen(text);
    if (length >= sizeof d->error) length = sizeof d->error - 1;
    memcpy(d->error, text, length);
    d->error[length] = 0;
}

static void* allocate(Decode* d, uint64_t bytes) {
    AllocationHeader* allocation;
    uint64_t charged;
    if (bytes > UINT64_MAX - sizeof(AllocationHeader)) {
        message(d, QPX_READER_INVALID_REQUEST, "PNG allocation size overflow");
        return NULL;
    }
    charged = bytes + sizeof(AllocationHeader);
    if (charged > d->limit - d->used || charged > SIZE_MAX) {
        message(d, QPX_READER_INVALID_REQUEST, "PNG decoded data exceeds reader work budget");
        return NULL;
    }
    allocation = (AllocationHeader*)calloc(1, (size_t)charged);
    if (!allocation) {
        message(d, QPX_READER_INTERNAL_ERROR, "PNG allocation failed");
        return NULL;
    }
    allocation->bytes = charged;
    d->used += charged;
    return allocation + 1;
}

static void release(Decode* d, void* pointer) {
    AllocationHeader* allocation;
    if (!pointer) return;
    allocation = ((AllocationHeader*)pointer) - 1;
    d->used -= allocation->bytes;
    free(allocation);
}

static png_voidp PNGCBAPI png_allocate(png_structp png, png_alloc_size_t bytes) {
    return allocate((Decode*)png_get_mem_ptr(png), (uint64_t)bytes);
}

static void PNGCBAPI png_release(png_structp png, png_voidp pointer) {
    release((Decode*)png_get_mem_ptr(png), pointer);
}

static void PNGCBAPI png_failure(png_structp png, png_const_charp text) {
    Decode* d = (Decode*)png_get_error_ptr(png);
    if (d->status == QPX_READER_OK) message(d, QPX_READER_IO_ERROR, text);
    png_longjmp(png, 1);
}

static void PNGCBAPI png_warning_failure(png_structp png, png_const_charp text) {
    /* Metadata and CRC warnings cannot silently discard an interpretation or
     * repair malformed input. Valid inputs require no warning recovery. */
    png_failure(png, text);
}

static void fail(Decode* d, qpx_reader_status_v1 status, const char* text) {
    message(d, status, text);
    png_error(d->png, text);
}

static void checkpoint(Decode* d) {
    if (d->cancel.is_cancelled && d->cancel.is_cancelled(d->cancel.context))
        fail(d, QPX_READER_CANCELLED, "PNG decoding cancelled");
}

static void read_bytes(Decode* d, unsigned char* data, size_t bytes) {
    checkpoint(d);
    if ((uint64_t)bytes > d->encoded_bytes - d->consumed)
        fail(d, QPX_READER_IO_ERROR, "PNG input is truncated or changed");
    if (fread(data, 1, bytes, d->input) != bytes)
        fail(d, QPX_READER_IO_ERROR, "PNG input read failed");
    d->consumed += bytes;
}

static void PNGCBAPI png_read_bytes(png_structp png, png_bytep data, png_size_t bytes) {
    read_bytes((Decode*)png_get_io_ptr(png), data, (size_t)bytes);
}

static int named(const png_byte* name, const char* wanted) {
    return memcmp(name, wanted, 4) == 0;
}

static int PNGCBAPI unknown_chunk(png_structp png, png_unknown_chunkp chunk) {
    Decode* d = (Decode*)png_get_user_chunk_ptr(png);
    checkpoint(d);
    if (named(chunk->name, "acTL") || named(chunk->name, "fcTL") || named(chunk->name, "fdAT"))
        fail(d, QPX_READER_UNSUPPORTED, "PNG animation is not supported");
    if (named(chunk->name, "iCCP"))
        fail(d, QPX_READER_UNSUPPORTED, "PNG ICC color profiles are not supported");
    if (named(chunk->name, "eXIf"))
        fail(d, QPX_READER_UNSUPPORTED, "PNG Exif orientation is not supported");
    if (named(chunk->name, "cICP") || named(chunk->name, "mDCV") || named(chunk->name, "cLLI"))
        fail(d, QPX_READER_UNSUPPORTED, "PNG extended color interpretation is not supported");
    if (named(chunk->name, "sCAL") || named(chunk->name, "pCAL") || named(chunk->name, "oFFs") || named(chunk->name, "sTER"))
        fail(d, QPX_READER_UNSUPPORTED, "PNG alternate calibration or placement is not supported");
    if ((chunk->name[0] & 32) == 0)
        fail(d, QPX_READER_IO_ERROR, "PNG contains an unknown critical chunk");
    /* An ancillary payload is consumed and CRC-checked but never retained. */
    return 1;
}

static uint64_t multiply(Decode* d, uint64_t a, uint64_t b) {
    if (a && b > UINT64_MAX / a)
        fail(d, QPX_READER_INVALID_REQUEST, "PNG decoded size overflow");
    return a * b;
}

static void* required_allocation(Decode* d, uint64_t bytes) {
    void* pointer = allocate(d, bytes);
    if (!pointer) png_error(d->png, d->error);
    return pointer;
}

static void check_color(Decode* d, png_infop info) {
    png_fixed_point gamma, wx, wy, rx, ry, gx, gy, bx, by;
    if (png_get_gAMA_fixed(d->png, info, &gamma) && gamma != 45455)
        fail(d, QPX_READER_UNSUPPORTED, "PNG non-sRGB gamma is not supported");
    if (png_get_cHRM_fixed(d->png, info, &wx, &wy, &rx, &ry, &gx, &gy, &bx, &by) &&
        (wx != 31270 || wy != 32900 || rx != 64000 || ry != 33000 ||
         gx != 30000 || gy != 60000 || bx != 15000 || by != 6000))
        fail(d, QPX_READER_UNSUPPORTED, "PNG non-sRGB chromaticities are not supported");
}

static uint16_t source_sample(const unsigned char* row, uint32_t x,
    uint32_t channel, uint32_t channels, uint32_t depth) {
    const uint64_t index = (uint64_t)x * channels + channel;
    if (depth < 8) {
        const uint32_t shift = 8 - depth - (uint32_t)(index * depth % 8);
        return (uint16_t)((row[index * depth / 8] >> shift) & ((1u << depth) - 1));
    }
    if (depth == 8) return row[index];
    return (uint16_t)(((uint16_t)row[index * 2] << 8) | row[index * 2 + 1]);
}

static unsigned char display_sample(uint16_t value, uint32_t depth) {
    if (depth == 16) return (unsigned char)(value >> 8);
    if (depth < 8) return (unsigned char)((uint32_t)value * 255 / ((1u << depth) - 1));
    return (unsigned char)value;
}

static void convert(Decode* d, png_size_t row_bytes) {
    png_colorp palette = NULL;
    int palette_count = 0, transparent_count = 0;
    png_bytep transparency = NULL;
    png_color_16p transparent = NULL;
    uint32_t y, x, c;
    const uint32_t channels = d->image.channels, depth = d->image.bit_depth;
    const int has_transparency = png_get_tRNS(d->png, d->info, &transparency, &transparent_count, &transparent) != 0;
    if (d->image.color_type == PNG_COLOR_TYPE_PALETTE &&
        (!png_get_PLTE(d->png, d->info, &palette, &palette_count) || palette_count <= 0))
        fail(d, QPX_READER_IO_ERROR, "PNG palette is missing");
    for (y = 0; y < d->image.height; ++y) {
        const unsigned char* row = d->packed + (uint64_t)y * row_bytes;
        checkpoint(d);
        for (x = 0; x < d->image.width; ++x) {
            uint16_t samples[4] = {0, 0, 0, 0};
            const uint64_t pixel = (uint64_t)y * d->image.width + x;
            unsigned char* rgba = d->image.rgba + pixel * 4;
            for (c = 0; c < channels; ++c) {
                unsigned char* raw = d->image.raw + (pixel * channels + c) * d->image.sample_bytes;
                samples[c] = source_sample(row, x, c, channels, depth);
                if (depth == 16) memcpy(raw, samples + c, 2);
                else *raw = (unsigned char)samples[c];
            }
            rgba[3] = 255;
            if (d->image.color_type == PNG_COLOR_TYPE_PALETTE) {
                const uint16_t index = samples[0];
                if (index >= palette_count) fail(d, QPX_READER_IO_ERROR, "PNG palette index is out of range");
                rgba[0] = palette[index].red; rgba[1] = palette[index].green; rgba[2] = palette[index].blue;
                if (has_transparency && index < transparent_count) rgba[3] = transparency[index];
            } else if (d->image.color_type == PNG_COLOR_TYPE_GRAY || d->image.color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                rgba[0] = rgba[1] = rgba[2] = display_sample(samples[0], depth);
                if (channels == 2) rgba[3] = display_sample(samples[1], depth);
                else if (has_transparency && transparent && samples[0] == transparent->gray) rgba[3] = 0;
            } else {
                for (c = 0; c < 3; ++c) rgba[c] = display_sample(samples[c], depth);
                if (channels == 4) rgba[3] = display_sample(samples[3], depth);
                else if (has_transparency && transparent && samples[0] == transparent->red &&
                    samples[1] == transparent->green && samples[2] == transparent->blue) rgba[3] = 0;
            }
        }
    }
}

static void cleanup_decoder(Decode* d) {
    if (d->png) png_destroy_read_struct(&d->png, &d->info, &d->end);
    release(d, d->packed);
    d->packed = NULL;
    d->input = NULL;
    d->cancel.is_cancelled = NULL;
    d->cancel.context = NULL;
}

void qpx_png_destroy(qpx_png_image* image) {
    Decode* d;
    if (!image) return;
    d = (Decode*)image->allocation_owner;
    release(d, d->image.raw);
    release(d, d->image.rgba);
    free(d);
}

qpx_reader_status_v1 qpx_png_decode(FILE* input, uint64_t encoded_bytes,
    uint64_t work_bytes, qpx_reader_cancel_v1 cancel, qpx_png_image** image,
    char* error, size_t error_capacity) {
    Decode* d;
    if (image) *image = NULL;
    if (error && error_capacity) error[0] = 0;
    if (!input || !image || work_bytes < sizeof(Decode)) {
        if (error && error_capacity) {
            const char* text = "PNG decoder work budget or arguments are invalid";
            const size_t length = strlen(text) < error_capacity - 1 ? strlen(text) : error_capacity - 1;
            memcpy(error, text, length); error[length] = 0;
        }
        return QPX_READER_INVALID_REQUEST;
    }
    d = (Decode*)calloc(1, sizeof(Decode));
    if (!d) return QPX_READER_INTERNAL_ERROR;
    d->limit = work_bytes; d->used = sizeof(Decode); d->input = input;
    d->encoded_bytes = encoded_bytes; d->cancel = cancel;
    d->image.allocation_owner = d;
    d->png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, d, png_failure,
        png_warning_failure, d, png_allocate, png_release);
    if (!d->png) {
        if (!d->error[0]) message(d, QPX_READER_INTERNAL_ERROR, "PNG decoder initialization failed");
    } else if (setjmp(png_jmpbuf(d->png)) == 0) {
        png_byte signature[8];
        png_uint_32 width, height, ppm_x, ppm_y;
        int depth, color, interlace, compression, filter, passes, pass, unit;
        png_size_t row_bytes;
        uint32_t y;
        uint64_t pixels;
        static const png_byte blocked[] =
            "iCCP\0eXIf\0acTL\0fcTL\0fdAT\0cICP\0mDCV\0cLLI\0sCAL\0pCAL\0oFFs\0sTER\0";
        static const png_byte unused[] = "bKGD\0hIST\0iTXt\0sPLT\0tEXt\0tIME\0zTXt\0sBIT\0";
        d->info = png_create_info_struct(d->png);
        d->end = png_create_info_struct(d->png);
        if (!d->info || !d->end) png_error(d->png, "PNG metadata allocation failed");
        png_set_read_fn(d->png, d, png_read_bytes);
        png_set_crc_action(d->png, PNG_CRC_ERROR_QUIT, PNG_CRC_ERROR_QUIT);
        /* The PNG dimension bound is independent of resource admission. The
         * checked sizes/custom allocator reject any raster exceeding budget. */
        png_set_user_limits(d->png, PNG_UINT_31_MAX, PNG_UINT_31_MAX);
        png_set_chunk_malloc_max(d->png, (png_alloc_size_t)work_bytes);
        png_set_chunk_cache_max(d->png, 1);
        png_set_read_user_chunk_fn(d->png, d, unknown_chunk);
        png_set_keep_unknown_chunks(d->png, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);
        png_set_keep_unknown_chunks(d->png, PNG_HANDLE_CHUNK_ALWAYS, blocked, 12);
        png_set_keep_unknown_chunks(d->png, PNG_HANDLE_CHUNK_NEVER, unused, 8);
        read_bytes(d, signature, sizeof signature);
        if (png_sig_cmp(signature, 0, sizeof signature))
            fail(d, QPX_READER_UNSUPPORTED, "PNG signature is not recognized");
        png_set_sig_bytes(d->png, sizeof signature);
        png_read_info(d->png, d->info);
        if (!png_get_IHDR(d->png, d->info, &width, &height, &depth, &color, &interlace, &compression, &filter))
            fail(d, QPX_READER_IO_ERROR, "PNG header is missing");
        check_color(d, d->info);
        d->image.width = width; d->image.height = height;
        d->image.bit_depth = (uint32_t)depth; d->image.color_type = (uint32_t)color;
        d->image.channels = png_get_channels(d->png, d->info);
        d->image.sample_bytes = depth == 16 ? 2 : 1;
        if (png_get_pHYs(d->png, d->info, &ppm_x, &ppm_y, &unit) && unit == PNG_RESOLUTION_METER) {
            if (ppm_x) {d->image.calibration_flags |= QPX_READER_CALIBRATION_X; d->image.pixel_width_um = 1000000.0 / ppm_x;}
            if (ppm_y) {d->image.calibration_flags |= QPX_READER_CALIBRATION_Y; d->image.pixel_height_um = 1000000.0 / ppm_y;}
        }
        passes = png_set_interlace_handling(d->png);
        png_read_update_info(d->png, d->info);
        row_bytes = png_get_rowbytes(d->png, d->info);
        pixels = multiply(d, width, height);
        d->image.raw_bytes = multiply(d, multiply(d, pixels, d->image.channels), d->image.sample_bytes);
        d->image.rgba_bytes = multiply(d, pixels, 4);
        d->packed = (unsigned char*)required_allocation(d, multiply(d, row_bytes, height));
        d->image.raw = (unsigned char*)required_allocation(d, d->image.raw_bytes);
        d->image.rgba = (unsigned char*)required_allocation(d, d->image.rgba_bytes);
        for (pass = 0; pass < passes; ++pass) for (y = 0; y < height; ++y) {
            checkpoint(d);
            png_read_row(d->png, d->packed + (uint64_t)y * row_bytes, NULL);
        }
        png_read_end(d->png, d->end);
        check_color(d, d->end);
        if (d->consumed != d->encoded_bytes)
            fail(d, QPX_READER_IO_ERROR, "PNG contains trailing data after IEND");
        convert(d, row_bytes);
        checkpoint(d);
    }
    cleanup_decoder(d);
    if (d->status != QPX_READER_OK || !d->image.raw || !d->image.rgba) {
        const qpx_reader_status_v1 status = d->status == QPX_READER_OK ? QPX_READER_INTERNAL_ERROR : d->status;
        const char* text = d->error[0] ? d->error : "PNG decoding failed";
        if (error && error_capacity) {
            const size_t length = strlen(text) < error_capacity - 1 ? strlen(text) : error_capacity - 1;
            memcpy(error, text, length); error[length] = 0;
        }
        qpx_png_destroy(&d->image);
        return status;
    }
    *image = &d->image;
    return QPX_READER_OK;
}
