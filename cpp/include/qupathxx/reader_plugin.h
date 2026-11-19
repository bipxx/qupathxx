#ifndef QUPATHXX_READER_PLUGIN_H
#define QUPATHXX_READER_PLUGIN_H
/* Native reader ABI v1. C-compatible; no allocator, STL, exceptions or retained
 * host pointers cross this boundary. Initial supported profile: Windows x64,
 * recorded MSVC series, Release /MD, default structure packing. */
#include <stdint.h>
#if defined(_WIN32)
#define QPX_READER_CALL __cdecl
#if defined(QPX_READER_PLUGIN_EXPORTS)
#define QPX_READER_EXPORT __declspec(dllexport)
#else
#define QPX_READER_EXPORT
#endif
#else
#define QPX_READER_CALL
#define QPX_READER_EXPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define QPX_READER_ABI_V1 1u
typedef uint32_t qpx_reader_status_v1;
#define QPX_READER_OK 0u
#define QPX_READER_CANCELLED 1u
#define QPX_READER_UNSUPPORTED 2u
#define QPX_READER_INVALID_REQUEST 3u
#define QPX_READER_IO_ERROR 4u
#define QPX_READER_INTERNAL_ERROR 5u
#define QPX_READER_BUFFER_TOO_SMALL 6u
#define QPX_READER_CAP_RGBA UINT64_C(1)
#define QPX_READER_CAP_RAW UINT64_C(2)
#define QPX_READER_CAP_SOURCE_FILES UINT64_C(4)
#define QPX_READER_CAP_SOURCE_STORE UINT64_C(8)
#define QPX_READER_CAP_LABEL_DESCRIPTION UINT64_C(16)
#define QPX_READER_UINT8 1u
#define QPX_READER_UINT16 2u
#define QPX_READER_INT8 3u
#define QPX_READER_INT16 4u
#define QPX_READER_UINT32 5u
#define QPX_READER_INT32 6u
#define QPX_READER_UINT64 7u
#define QPX_READER_INT64 8u
#define QPX_READER_FLOAT32 9u
#define QPX_READER_FLOAT64 10u
#define QPX_READER_CALIBRATION_X 1u
#define QPX_READER_CALIBRATION_Y 2u
#define QPX_READER_CALIBRATION_Z 4u
#define QPX_READER_CALIBRATION_TIME 8u

typedef struct qpx_reader_string_v1 {
    const char* data;
    uint64_t size; /* UTF-8 bytes, excludes NUL; embedded NUL is invalid. */
} qpx_reader_string_v1;
typedef struct qpx_reader_text_v1 {
    char* data;
    uint64_t capacity;
    uint64_t size; /* Required UTF-8 bytes, excludes NUL. No pointer/capacity edits.
                    * A short buffer returns BUFFER_TOO_SMALL and required size.
                    * The module never writes beyond capacity or retains data. */
} qpx_reader_text_v1;
typedef int32_t (QPX_READER_CALL *qpx_reader_cancelled_fn_v1)(void* context);
typedef struct qpx_reader_cancel_v1 {
    qpx_reader_cancelled_fn_v1 is_cancelled;
    void* context; /* Callback/context live only for the synchronous call. */
} qpx_reader_cancel_v1;
typedef struct qpx_reader_info_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t capabilities;
    qpx_reader_text_v1 provider_id,display_name,version,contract;
} qpx_reader_info_v1;
typedef struct qpx_reader_open_v1 {
    uint32_t struct_size;
    uint32_t reserved;
    qpx_reader_string_v1 path,options; /* Absolute UTF-8 path; pilot options empty. */
    uint64_t work_bytes;
    qpx_reader_cancel_v1 cancel;
} qpx_reader_open_v1;
typedef struct qpx_reader_metadata_v1 {
    uint32_t struct_size;
    uint32_t level_count;
    int64_t width,height;
    uint32_t has_raw,channels,sample_type,z_planes,timepoints,series,series_count;
    uint32_t calibration_flags;
    double pixel_width_um,pixel_height_um,z_spacing_um,time_interval_seconds;
    qpx_reader_text_v1 vendor,dimension_order;
} qpx_reader_metadata_v1;
typedef struct qpx_reader_level_v1 {
    uint32_t struct_size;
    uint32_t reserved;
    int64_t width,height;
    double downsample;
} qpx_reader_level_v1;
typedef struct qpx_reader_region_v1 {
    uint32_t struct_size;
    int32_t level;
    int64_t x,y,width,height; /* x/y are level-zero coordinates; output lattice
                              * is floor(x/downsample)+column, likewise y. */
    uint32_t first_channel,channels,z,t; /* RGBA: all four fields zero. */
} qpx_reader_region_v1;
typedef struct qpx_reader_pixels_v1 {
    uint32_t struct_size;
    uint32_t sample_type;
    int64_t width,height;
    uint32_t channels,reserved;
    uint64_t row_stride,pixel_stride,channel_stride;
    uint8_t* data;
    uint64_t capacity,written;
    uint8_t* validity;
    uint64_t validity_capacity,validity_written;
    /* All fields except written/validity_written are host input and immutable.
     * Packed native-endian HWC; exact fill lengths required. RGBA is UInt8x4,
     * straight alpha, transparent black outside bounds, no validity array.
     * Raw has one validity byte per pixel (0/1); invalid/OOB sample bytes zero.
     * Preserve valid integer bits, NaN/Inf and signed zero without conversion. */
} qpx_reader_pixels_v1;
typedef struct qpx_reader_api_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    qpx_reader_status_v1 (QPX_READER_CALL *describe)(qpx_reader_info_v1*,qpx_reader_text_v1* error);
    qpx_reader_status_v1 (QPX_READER_CALL *open)(const qpx_reader_open_v1*,void** handle,qpx_reader_text_v1* error);
    /* destroy is infallible, catches exceptions internally, accepts a valid
     * handle once. The host serializes ALL calls including destruction. */
    void (QPX_READER_CALL *destroy)(void* handle);
    qpx_reader_status_v1 (QPX_READER_CALL *metadata)(void*,qpx_reader_metadata_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1* error);
    qpx_reader_status_v1 (QPX_READER_CALL *level)(void*,uint32_t,qpx_reader_level_v1*,qpx_reader_text_v1* error);
    qpx_reader_status_v1 (QPX_READER_CALL *channel_name)(void*,uint32_t,qpx_reader_text_v1*,qpx_reader_text_v1* error);
    qpx_reader_status_v1 (QPX_READER_CALL *read_rgba)(void*,const qpx_reader_region_v1*,qpx_reader_pixels_v1*,qpx_reader_text_v1* error);
    qpx_reader_status_v1 (QPX_READER_CALL *read_raw)(void*,const qpx_reader_region_v1*,qpx_reader_pixels_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1* error);
} qpx_reader_api_v1;
/* Host initializes out->struct_size. Entry copies its complete v1 table into
 * that host allocation and rejects unknown ABI/short structures. Every entry
 * catches exceptions. No calls after host destruction; modules never unload. */
typedef qpx_reader_status_v1 (QPX_READER_CALL *qpx_reader_entry_fn_v1)(uint32_t,qpx_reader_api_v1*,qpx_reader_text_v1*);
QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(uint32_t requested_abi,qpx_reader_api_v1* out,qpx_reader_text_v1* error);
/* Optional file-set extension; the base v1 table/layout remains unchanged.
 * SOURCE_FILES providers export this entry. Discovery occurs BEFORE open under
 * the primary input guard, and repeats around reads. Callbacks are reentrant,
 * return at most 63 absolute companion paths (primary excluded), and retain no
 * request/cancel/text pointers. The host alone observes file versions/identity. */
typedef struct qpx_reader_files_api_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    qpx_reader_status_v1 (QPX_READER_CALL *count)(const qpx_reader_open_v1*,uint32_t*,qpx_reader_text_v1* error);
    qpx_reader_status_v1 (QPX_READER_CALL *path)(const qpx_reader_open_v1*,uint32_t,qpx_reader_text_v1*,qpx_reader_text_v1* error);
} qpx_reader_files_api_v1;
typedef qpx_reader_status_v1 (QPX_READER_CALL *qpx_reader_files_entry_fn_v1)(uint32_t,qpx_reader_files_api_v1*,qpx_reader_text_v1*);
#if defined(QPX_READER_PLUGIN_SOURCE_FILES)
QPX_READER_EXPORT
#endif
qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(uint32_t requested_abi,qpx_reader_files_api_v1* out,qpx_reader_text_v1* error);
/* Optional local-directory extension; base ABI1/layout stays unchanged.
 * SOURCE_STORE is mutually exclusive with SOURCE_FILES. Old hosts refuse this
 * capability rather than treating a directory as a single file. The host owns
 * bounded recursive membership/file-version observation before open and around
 * every metadata/read operation. Its digest is NOT a content fingerprint.
 * primary_metadata returns one existing regular metadata file as a nonempty
 * <=4096-byte UTF-8 key relative to request.path (the absolute store root).
 * Use '/' separators; no absolute/rooted/escaping/dot components, backslashes,
 * NUL, links/reparse traversal or alternate streams. The callback is reentrant,
 * polls request.cancel and retains no pointers. Input request/pointers/capacities
 * are immutable. Output obeys the usual caller-owned text/status rules.
 * The host's directory profile bounds absolute paths to4096 UTF-8 bytes and
 * descendant depth to64. A total allowance of4MiB..256MiB reserves1MiB for
 * filesystem/path working storage and both host inventories (each at most
 * min(64MiB, (total-1MiB)/4), retained + simultaneous re-observation).
 * Directory opens reserve both host inventories (retained + re-observation)
 * inside the bounded store allowance; request.work_bytes gives the remaining
 * private decoder allowance. File/companion work allowances remain unchanged. */
typedef struct qpx_reader_store_api_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    qpx_reader_status_v1 (QPX_READER_CALL *primary_metadata)(const qpx_reader_open_v1*,qpx_reader_text_v1*,qpx_reader_text_v1* error);
} qpx_reader_store_api_v1;
typedef qpx_reader_status_v1 (QPX_READER_CALL *qpx_reader_store_entry_fn_v1)(uint32_t,qpx_reader_store_api_v1*,qpx_reader_text_v1*);
#if defined(QPX_READER_PLUGIN_SOURCE_STORE)
QPX_READER_EXPORT
#endif
qpx_reader_status_v1 QPX_READER_CALL qpx_reader_store_entry_v1(uint32_t requested_abi,qpx_reader_store_api_v1* out,qpx_reader_text_v1* error);
/* Optional label-description extension; base72/files24/store16 stay unchanged.
 * LABEL_DESCRIPTION requires RAW and SOURCE_STORE and excludes SOURCE_FILES.
 * The host calls describe once after successful open and source/image metadata
 * validation, with the whole store observed before and after the callback.
 * Return nonempty <=65536-byte UTF-8 neutral JSON text in caller-owned output.
 * The bridge treats this text as opaque; shared native label services validate
 * its schema, exact source codes, frame/linkage and selected-array semantics.
 * On OK, output.size is the exact byte count and error.size must be zero.
 * Other statuses set output.size=0; BUFFER_TOO_SMALL instead reports the
 * required output size>capacity. Error text follows the usual bounded rules.
 * Neither output/error data pointers nor capacities may be edited or retained.
 * describe catches all exceptions and polls cancel during native work.
 * LABEL totals8..256MiB reserve2MiB of host filesystem/description work and two
 * inventories (each min(64MiB,(total-2MiB)/4)); open.work_bytes is the remaining
 * private decoder allowance. Plain STORE and file allowances are unchanged. */
typedef struct qpx_reader_label_api_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    qpx_reader_status_v1 (QPX_READER_CALL *describe)(void* handle,qpx_reader_text_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error);
} qpx_reader_label_api_v1;

typedef qpx_reader_status_v1 (QPX_READER_CALL *qpx_reader_label_entry_fn_v1)(uint32_t,qpx_reader_label_api_v1*,qpx_reader_text_v1*);
#if defined(QPX_READER_PLUGIN_LABEL_DESCRIPTION)
QPX_READER_EXPORT
#endif
qpx_reader_status_v1 QPX_READER_CALL qpx_reader_label_entry_v1(uint32_t requested_abi,qpx_reader_label_api_v1* out,qpx_reader_text_v1* error);
#ifdef __cplusplus
}
#endif
#endif
