#ifndef QUPATHXX_FORMAT_PLUGIN_H
#define QUPATHXX_FORMAT_PLUGIN_H
/* Typed import-provider ABI. Basic text/cancellation/status buffers retain the
 * existing image ABI conventions; no image entry or C++ object is required. */
#include <qupathxx/reader_plugin.h>

#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_EXPORTS)
#define QPX_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_FORMAT_EXPORT
#endif
#define QPX_FORMAT_CALL QPX_READER_CALL
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_MATRIX_EXPORTS)
#define QPX_MATRIX_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_MATRIX_FORMAT_EXPORT
#endif
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_ANNOTATION_EXPORTS)
#define QPX_ANNOTATION_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_ANNOTATION_FORMAT_EXPORT
#endif
#define QPX_FORMAT_ABI_V1 1u
#define QPX_FORMAT_MATRIX 1u
#define QPX_FORMAT_ANNOTATION 2u
#define QPX_FORMAT_COUNTS_UINT64 1u
#define QPX_FORMAT_IMAGE_PIXELS 1u
#define QPX_FORMAT_FEATURE_AXIS 1u
#define QPX_FORMAT_OBSERVATION_AXIS 2u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qpx_format_info_v1 {
    uint32_t struct_size,abi_version,domain,reserved;
    qpx_reader_text_v1 provider_id,format_id,display_name,version,contract,profile;
} qpx_format_info_v1;

typedef struct qpx_matrix_metadata_v1 {
    uint32_t struct_size,value_type;
    uint64_t features,observations;
    uint64_t reserved[2];
} qpx_matrix_metadata_v1;

typedef struct qpx_matrix_axis_request_v1 {
    uint32_t struct_size,axis;
    uint64_t first,count;
} qpx_matrix_axis_request_v1;

typedef struct qpx_matrix_axis_row_v1 {
    uint64_t index,id_offset,id_size,name_offset,name_size;
} qpx_matrix_axis_row_v1;

typedef struct qpx_matrix_axis_buffer_v1 {
    uint32_t struct_size,reserved;
    qpx_matrix_axis_row_v1* rows;
    uint64_t capacity,written;
    qpx_reader_text_v1 text;
    /* Offsets address this host-owned UTF-8 buffer. IDs are nonempty; names may
     * be empty. No NUL or implicit normalization. On BUFFER_TOO_SMALL, written
     * gives required rows and text.size required bytes; no payload is written. */
} qpx_matrix_axis_buffer_v1;

typedef struct qpx_matrix_values_request_v1 {
    uint32_t struct_size,reserved;
    uint64_t first,count;
    /* Dense observation-major, feature-minor linear index, exact UInt64. */
} qpx_matrix_values_request_v1;

typedef struct qpx_matrix_values_buffer_v1 {
    uint32_t struct_size,reserved;
    uint64_t* values;
    uint64_t capacity,written;
} qpx_matrix_values_buffer_v1;

typedef struct qpx_matrix_format_api_v1 {
    uint32_t struct_size,abi_version;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *describe)(qpx_format_info_v1*,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *open)(const qpx_reader_open_v1*,void**,qpx_reader_text_v1*);
    void (QPX_FORMAT_CALL *destroy)(void*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *metadata)(void*,qpx_matrix_metadata_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *axis)(void*,const qpx_matrix_axis_request_v1*,qpx_matrix_axis_buffer_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *values)(void*,const qpx_matrix_values_request_v1*,qpx_matrix_values_buffer_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_matrix_format_api_v1;

typedef struct qpx_annotation_metadata_v1 {
    uint32_t struct_size,coordinate_frame;
    uint64_t points;
    uint64_t reserved[2];
} qpx_annotation_metadata_v1;

typedef struct qpx_annotation_points_request_v1 {
    uint32_t struct_size,reserved;
    uint64_t first,count;
} qpx_annotation_points_request_v1;

typedef struct qpx_annotation_point_v1 {
    double x,y;
    int32_t c,z,t;
    uint32_t reserved;
    uint64_t source_id_offset,source_id_size,class_offset,class_size;
    /* Level-zero image-pixel coordinates. C=-1 means all channels; Z/T are
     * explicit nonnegative plane indices. Class is an opaque source label,
     * empty for unclassified. Native target-plane validation still applies. */
} qpx_annotation_point_v1;

typedef struct qpx_annotation_points_buffer_v1 {
    uint32_t struct_size,reserved;
    qpx_annotation_point_v1* points;
    uint64_t capacity,written;
    qpx_reader_text_v1 text;
} qpx_annotation_points_buffer_v1;

typedef struct qpx_annotation_format_api_v1 {
    uint32_t struct_size,abi_version;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *describe)(qpx_format_info_v1*,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *open)(const qpx_reader_open_v1*,void**,qpx_reader_text_v1*);
    void (QPX_FORMAT_CALL *destroy)(void*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *metadata)(void*,qpx_annotation_metadata_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *points)(void*,const qpx_annotation_points_request_v1*,qpx_annotation_points_buffer_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_annotation_format_api_v1;

typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_matrix_format_entry_fn_v1)(uint32_t,qpx_matrix_format_api_v1*,qpx_reader_text_v1*);
typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_annotation_format_entry_fn_v1)(uint32_t,qpx_annotation_format_api_v1*,qpx_reader_text_v1*);
QPX_MATRIX_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v1(uint32_t,qpx_matrix_format_api_v1*,qpx_reader_text_v1*);
QPX_ANNOTATION_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v1(uint32_t,qpx_annotation_format_api_v1*,qpx_reader_text_v1*);

/* All entry points catch exceptions; destroy accepts a valid handle once and
 * cannot fail. Host serializes every handle call, including destruction. Open
 * paths are absolute UTF-8, options canonical native interpretation JSON or
 * empty, and owned work <=256MiB. No request, callback or output pointer survives
 * a synchronous call. No STL/allocator ownership crosses this boundary.
 * All host-input pointer/capacity/request fields are immutable. Only documented
 * output values change. Successful pages fill exactly min(count,total-first)
 * records, with first<=total and count>0; first==total yields an empty page.
 * BUFFER_TOO_SMALL reports required capacity without writing payload. Invalid
 * ranges/layouts return INVALID_REQUEST. Text is bounded and valid UTF-8.
 * The host independently validates shape, offsets, counts, IDs, geometry, plane,
 * identity and quotas before native publication; plugins never receive mutable
 * application objects or perform QMX/project publication. */

#ifdef __cplusplus
}
#endif
#endif
