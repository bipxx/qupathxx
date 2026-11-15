#include "jpeg_decoder.h"
#include "jpeg_memory_backend.h"
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define JPEG_INTERNALS
#include <jpeglib.h>

typedef struct Scale {uint64_t numerator, denominator; int known;} Scale;
typedef struct Resolution {uint32_t numerator, denominator; int known;} Resolution;
typedef struct Metadata {
    int jfif, exif, adobe, adobe_transform;
    int orientation_seen, x_seen, y_seen, unit_seen, exif_pointer_seen;
    uint32_t orientation, unit, exif_pointer;
    Resolution x, y;
    Scale jfif_x, jfif_y;
} Metadata;

typedef struct Decode {
    qpx_jpeg_memory memory; /* The preserved cinfo->client_data budget prefix. */
    qpx_jpeg_image image;
    struct jpeg_decompress_struct jpeg;
    struct jpeg_error_mgr errors;
    struct jpeg_source_mgr source;
    struct jpeg_progress_mgr progress;
    jmp_buf jump;
    FILE* input;
    qpx_reader_cancel_v1 cancel;
    uint64_t encoded_bytes, read_bytes;
    JOCTET input_buffer[4096];
    unsigned char* decoded;
    uint32_t source_width, source_height;
    Metadata metadata;
    jpeg_saved_marker_ptr last_header_marker;
} Decode;

static void fail(Decode* d, qpx_reader_status_v1 status, const char* text) {
    qpx_jpeg_memory_error(&d->memory, status, text);
    longjmp(d->jump, 1);
}

static void checkpoint(Decode* d) {
    if (d->cancel.is_cancelled && d->cancel.is_cancelled(d->cancel.context))
        fail(d, QPX_READER_CANCELLED, "JPEG decoding cancelled");
}

static void error_exit(j_common_ptr common) {
    Decode* d = (Decode*)common->client_data;
    char text[JMSG_LENGTH_MAX];
    if (d->memory.status == QPX_READER_OK) {
        qpx_reader_status_v1 status = QPX_READER_IO_ERROR;
        const int code = common->err->msg_code;
        if (code == JERR_NO_SOI || code == JERR_BAD_PRECISION || code == JERR_SOF_UNSUPPORTED ||
            code == JERR_ARITH_NOTIMPL || code == JERR_NOT_COMPILED ||
            code == JWRN_ADOBE_XFORM || code == JWRN_JFIF_MAJOR)
            status = QPX_READER_UNSUPPORTED;
        (*common->err->format_message)(common, text);
        qpx_jpeg_memory_error(&d->memory, status, text);
    }
    longjmp(d->jump, 1);
}

static void emit_message(j_common_ptr common, int level) {
    /* No warning recovery, concealed corruption or synthesized pixels. */
    if (level < 0) error_exit(common);
}

static void source_init(j_decompress_ptr jpeg) {
    checkpoint((Decode*)jpeg->client_data);
}

static boolean source_fill(j_decompress_ptr jpeg) {
    Decode* d = (Decode*)jpeg->client_data;
    uint64_t remaining;
    size_t wanted;
    checkpoint(d);
    remaining = d->encoded_bytes - d->read_bytes;
    if (!remaining) fail(d, QPX_READER_IO_ERROR, "JPEG input ended before its EOI marker");
    wanted = remaining < sizeof d->input_buffer ? (size_t)remaining : sizeof d->input_buffer;
    if (fread(d->input_buffer, 1, wanted, d->input) != wanted)
        fail(d, QPX_READER_IO_ERROR, "JPEG input is truncated or changed");
    d->read_bytes += wanted;
    d->source.next_input_byte = d->input_buffer;
    d->source.bytes_in_buffer = wanted;
    return TRUE;
}

static void source_skip(j_decompress_ptr jpeg, long bytes) {
    struct jpeg_source_mgr* source = jpeg->src;
    if (bytes <= 0) return;
    while ((uint64_t)bytes > source->bytes_in_buffer) {
        bytes -= (long)source->bytes_in_buffer;
        source_fill(jpeg);
    }
    source->next_input_byte += bytes; source->bytes_in_buffer -= (size_t)bytes;
}

static boolean source_restart(j_decompress_ptr jpeg, int desired) {
    (void)desired;
    fail((Decode*)jpeg->client_data, QPX_READER_IO_ERROR, "JPEG restart resynchronization is not supported");
    return FALSE;
}

static void source_term(j_decompress_ptr jpeg) {
    Decode* d = (Decode*)jpeg->client_data;
    /* finish_decompress aborts/frees JPOOL_IMAGE immediately after this hook.
     * Inspect saved markers only here, while their allocations remain live. */
    if (d->read_bytes - d->source.bytes_in_buffer != d->encoded_bytes)
        fail(d, QPX_READER_IO_ERROR, "JPEG has concatenated images or trailing data");
    if ((d->last_header_marker && d->last_header_marker->next) ||
        (!d->last_header_marker && jpeg->marker_list))
        fail(d, QPX_READER_UNSUPPORTED, "JPEG application metadata after the first scan is not supported");
    d->last_header_marker = NULL;
}
static void progress_monitor(j_common_ptr common) {checkpoint((Decode*)common->client_data);}

static uint64_t multiply(Decode* d, uint64_t a, uint64_t b) {
    if (a && b > UINT64_MAX / a) fail(d, QPX_READER_INVALID_REQUEST, "JPEG decoded size overflow");
    return a * b;
}

static void* allocate(Decode* d, uint64_t bytes) {
    void* pointer = qpx_jpeg_memory_allocate(&d->memory, bytes);
    if (!pointer) longjmp(d->jump, 1);
    return pointer;
}

static uint16_t be16(const unsigned char* p) {return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);}
typedef struct Tiff {Decode* owner; const unsigned char* data; uint64_t size; int little;} Tiff;

static void extent(Tiff* t, uint64_t offset, uint64_t bytes) {
    if (offset > t->size || bytes > t->size - offset)
        fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif field extends outside its marker");
}

static uint16_t t16(Tiff* t, uint64_t offset) {
    const unsigned char* p;
    extent(t, offset, 2); p = t->data + offset;
    return t->little ? (uint16_t)(p[0] | ((uint16_t)p[1] << 8)) : be16(p);
}

static uint32_t t32(Tiff* t, uint64_t offset) {
    const unsigned char* p;
    extent(t, offset, 4); p = t->data + offset;
    if (t->little) return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t ifd(Tiff* t, uint32_t offset, int mode) {
    static const unsigned char sizes[14] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8, 4};
    const uint32_t count = t16(t, offset);
    const uint64_t end = (uint64_t)offset + 2 + (uint64_t)count * 12;
    uint32_t i;
    int color_seen = 0, interoperability_seen = 0;
    Metadata* m = &t->owner->metadata;
    if (offset < 8) fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif IFD points into its header");
    extent(t, offset, 2 + (uint64_t)count * 12 + 4);
    for (i = 0; i < count; ++i) {
        const uint64_t entry = (uint64_t)offset + 2 + (uint64_t)i * 12;
        const uint16_t tag = t16(t, entry), type = t16(t, entry + 2);
        const uint32_t elements = t32(t, entry + 4);
        uint64_t bytes, value;
        if (!type || type >= sizeof sizes)
            fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif field type or count is invalid");
        bytes = (uint64_t)elements * sizes[type];
        value = bytes <= 4 ? entry + 8 : t32(t, entry + 8);
        if (bytes > 4 && value < 8)
            fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif data points into its header");
        extent(t, value, bytes);
        if ((mode == 0 || mode == 1) &&
            (tag == 0xa500 || tag == 0x012d || tag == 0x013e || tag == 0x013f)) {
            fail(t->owner, QPX_READER_UNSUPPORTED, "JPEG Exif gamma or color transfer interpretation is not supported");
        } else if (mode == 1 && (tag == 0x0112 || tag == 0x011a || tag == 0x011b || tag == 0x0128)) {
            fail(t->owner, QPX_READER_UNSUPPORTED, "JPEG secondary orientation or calibration interpretation is not supported");
        } else if (mode == 0 && tag == 0x0112) {
            if (m->orientation_seen++ || type != 3 || elements != 1)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif orientation is duplicate or malformed");
            m->orientation = t16(t, value);
            if (m->orientation < 1 || m->orientation > 8)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif orientation is outside 1 through 8");
        } else if (mode == 0 && (tag == 0x011a || tag == 0x011b)) {
            Resolution* resolution = tag == 0x011a ? &m->x : &m->y;
            int* seen = tag == 0x011a ? &m->x_seen : &m->y_seen;
            if ((*seen)++ || type != 5 || elements != 1 || (value >= offset && value < end + 4))
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif resolution is duplicate or malformed");
            resolution->numerator = t32(t, value); resolution->denominator = t32(t, value + 4);
            if (!resolution->numerator || !resolution->denominator)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif resolution rational is invalid");
            resolution->known = 1;
        } else if (mode == 0 && tag == 0x0128) {
            if (m->unit_seen++ || type != 3 || elements != 1)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif resolution unit is duplicate or malformed");
            m->unit = t16(t, value);
            if (m->unit < 1 || m->unit > 3)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif resolution unit is invalid");
        } else if (mode == 0 && tag == 0x8769) {
            if (m->exif_pointer_seen++ || type != 4 || elements != 1)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif IFD pointer is duplicate or malformed");
            m->exif_pointer = t32(t, value);
            if (m->exif_pointer < 8) fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif IFD pointer is invalid");
            extent(t, m->exif_pointer, 2);
        } else if ((mode == 0 || mode == 1) && tag == 0xa001) {
            if (color_seen++ || type != 3 || elements != 1)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif color space is duplicate or malformed");
            if (t16(t, value) != 1)
                fail(t->owner, QPX_READER_UNSUPPORTED, "JPEG Exif non-sRGB color space is not supported");
        } else if ((mode == 0 || mode == 1) && tag == 0xa005) {
            const uint32_t pointer = t32(t, value);
            if (interoperability_seen++ || type != 4 || elements != 1 || pointer < 8)
                fail(t->owner, QPX_READER_IO_ERROR, "JPEG Exif interoperability pointer is malformed");
            extent(t, pointer, 2);
            fail(t->owner, QPX_READER_UNSUPPORTED, "JPEG Exif interoperability color overrides are not supported");
        }
    }
    return t32(t, end);
}

static void exif(Decode* d, const unsigned char* data, unsigned int length) {
    Tiff t;
    uint32_t primary, thumbnail;
    if (d->metadata.exif++) fail(d, QPX_READER_IO_ERROR, "JPEG has duplicate Exif markers");
    if (length < 14 || memcmp(data, "Exif\0\0", 6))
        fail(d, QPX_READER_IO_ERROR, "JPEG Exif marker is malformed");
    t.owner = d; t.data = data + 6; t.size = length - 6;
    if (t.data[0] == 'I' && t.data[1] == 'I') t.little = 1;
    else if (t.data[0] == 'M' && t.data[1] == 'M') t.little = 0;
    else fail(d, QPX_READER_IO_ERROR, "JPEG Exif byte order is invalid");
    if (t16(&t, 2) != 42) fail(d, QPX_READER_IO_ERROR, "JPEG Exif TIFF version is invalid");
    primary = t32(&t, 4); thumbnail = ifd(&t, primary, 0);
    if (d->metadata.exif_pointer) {
        if (d->metadata.exif_pointer == primary || d->metadata.exif_pointer == thumbnail)
            fail(d, QPX_READER_IO_ERROR, "JPEG Exif IFD pointers are cyclic or aliased");
        if (ifd(&t, d->metadata.exif_pointer, 1))
            fail(d, QPX_READER_UNSUPPORTED, "JPEG Exif IFD chains are not supported");
    }
    if (thumbnail) {
        if (thumbnail == primary) fail(d, QPX_READER_IO_ERROR, "JPEG Exif thumbnail IFD is cyclic");
        /* Validate a single thumbnail IFD but never decode its image or apply
         * its orientation/resolution to the primary raster. */
        if (ifd(&t, thumbnail, 2))
            fail(d, QPX_READER_UNSUPPORTED, "JPEG additional thumbnail IFD chains are not supported");
    }
}

static void jfif(Decode* d, const unsigned char* data, unsigned int length) {
    const uint32_t x = length >= 14 ? be16(data + 8) : 0;
    const uint32_t y = length >= 14 ? be16(data + 10) : 0;
    uint32_t units;
    if (d->metadata.jfif++) fail(d, QPX_READER_IO_ERROR, "JPEG has duplicate JFIF markers");
    if (length < 14 || (uint64_t)length != 14 + (uint64_t)data[12] * data[13] * 3 ||
        !x || !y || data[7] > 2)
        fail(d, QPX_READER_IO_ERROR, "JPEG JFIF density or thumbnail metadata is malformed");
    if (data[5] != 1 || data[6] > 2)
        fail(d, QPX_READER_UNSUPPORTED, "JPEG JFIF version is not supported");
    units = data[7] == 1 ? 25400 : data[7] == 2 ? 10000 : 0;
    if (units) {
        d->metadata.jfif_x.numerator = units; d->metadata.jfif_x.denominator = x; d->metadata.jfif_x.known = 1;
        d->metadata.jfif_y.numerator = units; d->metadata.jfif_y.denominator = y; d->metadata.jfif_y.known = 1;
    }
}

static void metadata(Decode* d) {
    jpeg_saved_marker_ptr marker;
    for (marker = d->jpeg.marker_list; marker; marker = marker->next) {
        const unsigned char* data = marker->data;
        const unsigned int length = marker->data_length;
        d->last_header_marker = marker;
        checkpoint(d);
        if (marker->original_length != length)
            fail(d, QPX_READER_IO_ERROR, "JPEG application marker was truncated");
        if (marker->marker == JPEG_APP0) {
            if (length >= 5 && !memcmp(data, "JFIF\0", 5)) jfif(d, data, length);
            else if (length >= 6 && !memcmp(data, "JFXX\0", 5)) {
                if (!d->metadata.jfif) fail(d, QPX_READER_IO_ERROR, "JPEG JFXX thumbnail has no JFIF header");
                if (data[5] != 0x10 && data[5] != 0x11 && data[5] != 0x13)
                    fail(d, QPX_READER_UNSUPPORTED, "JPEG JFXX thumbnail encoding is not supported");
            } else fail(d, QPX_READER_UNSUPPORTED, "JPEG APP0 interpretation is not supported");
        } else if (marker->marker == JPEG_APP0 + 1) {
            if (length >= 4 && !memcmp(data, "Exif", 4)) exif(d, data, length);
            else fail(d, QPX_READER_UNSUPPORTED, "JPEG XMP or other APP1 interpretation is not supported");
        } else if (marker->marker == JPEG_APP0 + 14) {
            if (d->metadata.adobe++) fail(d, QPX_READER_IO_ERROR, "JPEG has duplicate Adobe color markers");
            if (length != 12 || memcmp(data, "Adobe", 5))
                fail(d, QPX_READER_IO_ERROR, "JPEG Adobe marker is malformed");
            if (be16(data + 5) != 100 || be16(data + 7) || be16(data + 9) || data[11] > 1)
                fail(d, QPX_READER_UNSUPPORTED, "JPEG Adobe color interpretation is not supported");
            d->metadata.adobe_transform = data[11];
        } else if (marker->marker == JPEG_APP0 + 2) {
            fail(d, QPX_READER_UNSUPPORTED, "JPEG ICC, MPO or other APP2 interpretation is not supported");
        } else fail(d, QPX_READER_UNSUPPORTED, "JPEG application metadata interpretation is not supported");
    }
    if (d->metadata.adobe && d->jpeg.num_components == 3 &&
        ((d->metadata.adobe_transform == 0 && d->jpeg.jpeg_color_space != JCS_RGB) ||
         (d->metadata.adobe_transform == 1 && d->jpeg.jpeg_color_space != JCS_YCbCr)))
        fail(d, QPX_READER_IO_ERROR, "JPEG color-space metadata conflicts");
}

static Scale exif_scale(Decode* d, Resolution resolution) {
    Scale result = {0, 0, 0};
    const uint32_t units = d->metadata.unit == 2 ? 25400 : d->metadata.unit == 3 ? 10000 : 0;
    if (resolution.known && units) {
        result.numerator = (uint64_t)units * resolution.denominator;
        result.denominator = resolution.numerator; result.known = 1;
    }
    return result;
}

static Scale combine_scale(Decode* d, Scale exif_value, Scale jfif_value) {
    /* Maximum Exif numerator is25400*UINT32_MAX; multiplication by a JFIF
     * UInt16 denominator fits UInt64. No floating tolerance hides a conflict. */
    if (exif_value.known && jfif_value.known &&
        exif_value.numerator * jfif_value.denominator != jfif_value.numerator * exif_value.denominator)
        fail(d, QPX_READER_IO_ERROR, "JPEG Exif and JFIF physical calibration conflict");
    return exif_value.known ? exif_value : jfif_value;
}

static void orient(Decode* d) {
    const uint32_t width = d->source_width, height = d->source_height, channels = d->image.channels;
    const uint32_t orientation = d->metadata.orientation;
    const uint64_t pixels = multiply(d, width, height);
    Scale x = combine_scale(d, exif_scale(d, d->metadata.x), d->metadata.jfif_x);
    Scale y = combine_scale(d, exif_scale(d, d->metadata.y), d->metadata.jfif_y);
    uint32_t sy, sx;
    d->image.width = orientation >= 5 ? height : width;
    d->image.height = orientation >= 5 ? width : height;
    d->image.orientation = orientation;
    if (orientation >= 5) {const Scale saved = x; x = y; y = saved;}
    if (x.known) {d->image.calibration_flags |= QPX_READER_CALIBRATION_X; d->image.pixel_width_um = (double)x.numerator / (double)x.denominator;}
    if (y.known) {d->image.calibration_flags |= QPX_READER_CALIBRATION_Y; d->image.pixel_height_um = (double)y.numerator / (double)y.denominator;}
    d->image.raw_bytes = multiply(d, pixels, channels); d->image.rgba_bytes = multiply(d, pixels, 4);
    d->image.raw = (unsigned char*)allocate(d, d->image.raw_bytes);
    d->image.rgba = (unsigned char*)allocate(d, d->image.rgba_bytes);
    for (sy = 0; sy < height; ++sy) {
        checkpoint(d);
        for (sx = 0; sx < width; ++sx) {
            uint32_t xout = sx, yout = sy, channel;
            uint64_t from, to;
            unsigned char* rgba;
            switch (orientation) {
                case 2: xout = width - 1 - sx; break;
                case 3: xout = width - 1 - sx; yout = height - 1 - sy; break;
                case 4: yout = height - 1 - sy; break;
                case 5: xout = sy; yout = sx; break;
                case 6: xout = height - 1 - sy; yout = sx; break;
                case 7: xout = height - 1 - sy; yout = width - 1 - sx; break;
                case 8: xout = sy; yout = width - 1 - sx; break;
                default: break;
            }
            from = ((uint64_t)sy * width + sx) * channels;
            to = (uint64_t)yout * d->image.width + xout;
            memcpy(d->image.raw + to * channels, d->decoded + from, channels);
            rgba = d->image.rgba + to * 4;
            for (channel = 0; channel < 3; ++channel) rgba[channel] = d->decoded[from + (channels == 1 ? 0 : channel)];
            rgba[3] = 255;
        }
    }
}

static void cleanup(Decode* d) {
    if (d->jpeg.mem) jpeg_destroy_decompress(&d->jpeg);
    qpx_jpeg_memory_release(&d->memory, d->decoded); d->decoded = NULL;
    d->jpeg.client_data = NULL; d->jpeg.src = NULL; d->jpeg.progress = NULL;
    memset(&d->source, 0, sizeof d->source);
    d->input = NULL; d->cancel.context = NULL; d->cancel.is_cancelled = NULL;
    d->last_header_marker = NULL;
}

void qpx_jpeg_destroy(qpx_jpeg_image* image) {
    Decode* d;
    if (!image) return;
    d = (Decode*)image->allocation_owner;
    qpx_jpeg_memory_release(&d->memory, image->raw);
    qpx_jpeg_memory_release(&d->memory, image->rgba);
    free(d);
}

qpx_reader_status_v1 qpx_jpeg_decode(FILE* input, uint64_t encoded_bytes,
    uint64_t work_bytes, qpx_reader_cancel_v1 cancel, qpx_jpeg_image** image,
    char* error, size_t error_capacity) {
    Decode* d;
    if (image) *image = NULL;
    if (error && error_capacity) error[0] = 0;
    if (!input || !image || work_bytes < sizeof(Decode)) {
        const char* text = "JPEG decoder work budget or arguments are invalid";
        if (error && error_capacity) {
            const size_t length = strlen(text) < error_capacity - 1 ? strlen(text) : error_capacity - 1;
            memcpy(error, text, length); error[length] = 0;
        }
        return QPX_READER_INVALID_REQUEST;
    }
    d = (Decode*)calloc(1, sizeof(Decode));
    if (!d) return QPX_READER_INTERNAL_ERROR;
    d->memory.limit = work_bytes; d->memory.used = sizeof(Decode);
    d->image.allocation_owner = d;
    d->metadata.orientation = 1;
    d->input = input; d->encoded_bytes = encoded_bytes; d->cancel = cancel;
    d->jpeg.err = jpeg_std_error(&d->errors); d->jpeg.client_data = d;
    d->errors.error_exit = error_exit; d->errors.emit_message = emit_message;
    if (setjmp(d->jump) == 0) {
        uint64_t pixels, row_bytes;
        int marker;
        checkpoint(d);
        jpeg_create_decompress(&d->jpeg);
        d->source.init_source = source_init; d->source.fill_input_buffer = source_fill;
        d->source.skip_input_data = source_skip; d->source.resync_to_restart = source_restart;
        d->source.term_source = source_term; d->jpeg.src = &d->source;
        d->progress.progress_monitor = progress_monitor; d->jpeg.progress = &d->progress;
        for (marker = 0; marker < 16; ++marker) jpeg_save_markers(&d->jpeg, JPEG_APP0 + marker, 0xffff);
        if (jpeg_read_header(&d->jpeg, TRUE) != JPEG_HEADER_OK)
            fail(d, QPX_READER_IO_ERROR, "JPEG image header is incomplete");
        if (d->jpeg.data_precision != 8 || d->jpeg.master->lossless)
            fail(d, QPX_READER_UNSUPPORTED, "JPEG reader requires 8-bit DCT samples; lossless JPEG is unsupported");
        if (d->jpeg.arith_code)
            fail(d, QPX_READER_UNSUPPORTED, "JPEG arithmetic coding is not supported");
        if ((d->jpeg.num_components != 1 || d->jpeg.jpeg_color_space != JCS_GRAYSCALE) &&
            (d->jpeg.num_components != 3 || (d->jpeg.jpeg_color_space != JCS_RGB && d->jpeg.jpeg_color_space != JCS_YCbCr)))
            fail(d, QPX_READER_UNSUPPORTED, "JPEG CMYK, YCCK or other color spaces are not supported");
        metadata(d);
        if (d->jpeg.num_components == 3 && !d->metadata.jfif && !d->metadata.adobe &&
            !((d->jpeg.comp_info[0].component_id == 1 && d->jpeg.comp_info[1].component_id == 2 && d->jpeg.comp_info[2].component_id == 3) ||
              (d->jpeg.comp_info[0].component_id == 'R' && d->jpeg.comp_info[1].component_id == 'G' && d->jpeg.comp_info[2].component_id == 'B')))
            fail(d, QPX_READER_UNSUPPORTED, "JPEG component color interpretation is ambiguous");
        d->jpeg.out_color_space = d->jpeg.num_components == 1 ? JCS_GRAYSCALE : JCS_RGB;
        d->jpeg.dct_method = JDCT_ISLOW; d->jpeg.do_fancy_upsampling = TRUE;
        d->jpeg.do_block_smoothing = FALSE; d->jpeg.scale_num = 1; d->jpeg.scale_denom = 1;
        d->jpeg.quantize_colors = FALSE; d->jpeg.raw_data_out = FALSE; d->jpeg.buffered_image = FALSE;
        d->source_width = d->jpeg.image_width; d->source_height = d->jpeg.image_height;
        d->image.channels = d->jpeg.num_components == 1 ? 1 : 3;
        pixels = multiply(d, d->source_width, d->source_height);
        row_bytes = multiply(d, d->source_width, d->image.channels);
        d->decoded = (unsigned char*)allocate(d, multiply(d, pixels, d->image.channels));
        if (!jpeg_start_decompress(&d->jpeg)) fail(d, QPX_READER_IO_ERROR, "JPEG decompression did not start");
        if (d->jpeg.output_width != d->source_width || d->jpeg.output_height != d->source_height ||
            d->jpeg.output_components != (int)d->image.channels)
            fail(d, QPX_READER_IO_ERROR, "JPEG decoder changed the selected full-resolution layout");
        while (d->jpeg.output_scanline < d->jpeg.output_height) {
            JSAMPROW row = d->decoded + (uint64_t)d->jpeg.output_scanline * row_bytes;
            checkpoint(d);
            if (jpeg_read_scanlines(&d->jpeg, &row, 1) != 1)
                fail(d, QPX_READER_IO_ERROR, "JPEG scanline is incomplete");
        }
        if (!jpeg_finish_decompress(&d->jpeg)) fail(d, QPX_READER_IO_ERROR, "JPEG image did not finish");
        orient(d); checkpoint(d);
    }
    cleanup(d);
    if (d->memory.status != QPX_READER_OK || !d->image.raw || !d->image.rgba) {
        const qpx_reader_status_v1 status = d->memory.status == QPX_READER_OK ? QPX_READER_INTERNAL_ERROR : d->memory.status;
        const char* text = d->memory.error[0] ? d->memory.error : "JPEG decoding failed";
        if (error && error_capacity) {
            const size_t length = strlen(text) < error_capacity - 1 ? strlen(text) : error_capacity - 1;
            memcpy(error, text, length); error[length] = 0;
        }
        qpx_jpeg_destroy(&d->image); return status;
    }
    *image = &d->image; return QPX_READER_OK;
}
