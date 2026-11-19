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
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_MATRIX_TYPED_EXPORTS)
#define QPX_MATRIX_TYPED_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_MATRIX_TYPED_FORMAT_EXPORT
#endif
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_MATRIX_STORE_EXPORTS)
#define QPX_MATRIX_STORE_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_MATRIX_STORE_FORMAT_EXPORT
#endif
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_ANNOTATION_EXPORTS)
#define QPX_ANNOTATION_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_ANNOTATION_FORMAT_EXPORT
#endif
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_GEOMETRY_EXPORTS)
#define QPX_ANNOTATION_GEOMETRY_EXPORT __declspec(dllexport)
#else
#define QPX_ANNOTATION_GEOMETRY_EXPORT
#endif
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_METADATA_EXPORTS)
#define QPX_ANNOTATION_METADATA_EXPORT __declspec(dllexport)
#else
#define QPX_ANNOTATION_METADATA_EXPORT
#endif
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_LOCKED_EXPORTS)
#define QPX_ANNOTATION_LOCKED_EXPORT __declspec(dllexport)
#else
#define QPX_ANNOTATION_LOCKED_EXPORT
#endif
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_MASK_EXPORTS)
#define QPX_ANNOTATION_MASK_EXPORT __declspec(dllexport)
#else
#define QPX_ANNOTATION_MASK_EXPORT
#endif
#define QPX_FORMAT_ABI_V1 1u
#define QPX_MATRIX_ABI_V2 2u
#define QPX_MATRIX_ABI_V3 3u
#define QPX_MATRIX_ABI_V4 4u
#define QPX_MATRIX_ABI_V5 5u
#define QPX_ANNOTATION_ABI_V2 2u
#define QPX_ANNOTATION_ABI_V3 3u
#define QPX_ANNOTATION_ABI_V4 4u
#define QPX_ANNOTATION_ABI_V5 5u
#define QPX_ANNOTATION_ABI_V6 6u
#define QPX_FORMAT_GEOMETRY_QRO1 1u
#define QPX_FORMAT_GEOMETRY_EVEN_ODD_RINGS1 2u
#define QPX_FORMAT_GEOMETRY_BINARY_MASK1 3u
#define QPX_ANNOTATION_PLANE_CZT 0u
#define QPX_ANNOTATION_PLANE_LINEAR_POSITION 1u
#define QPX_ANNOTATION_OBJECT_COLOR_RGB 1u
/* LOCKED is negotiated only by the separate ABI4 entry; invalid in ABI3. */
#define QPX_ANNOTATION_OBJECT_LOCKED 2u
#define QPX_FORMAT_MATRIX 1u
#define QPX_FORMAT_ANNOTATION 2u
#define QPX_FORMAT_COUNTS_UINT64 1u
#define QPX_FORMAT_CONTINUOUS_FLOAT64 2u
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

/* Separately negotiated typed matrix ABI2. No ABI1 layout or interpretation
 * changes. The independent file allowance permits bounded streamed input;
 * work_bytes remains the complete provider allowance, at most256MiB. The host
 * observes and fingerprints the whole primary file, including unread members.
 * Providers never publish QMX, interpret coordinates or retain host pointers. */
typedef struct qpx_matrix_open_v2 {
    uint32_t struct_size,reserved;
    qpx_reader_string_v1 path,options;
    uint64_t work_bytes,file_bytes;
    qpx_reader_cancel_v1 cancel;
} qpx_matrix_open_v2;

typedef struct qpx_matrix_metadata_v2 {
    uint32_t struct_size,value_type;
    uint64_t features,observations;
    qpx_reader_text_v1 value_domain;
    uint64_t reserved[2];
    /* value_type is COUNTS_UINT64 or CONTINUOUS_FLOAT64. value_domain is
     * nonempty valid UTF-8 <=128 bytes, without NUL/TAB/CR/LF. Counts use exactly
     * raw-counts; continuous uses an explicitly selected other label. A probe
     * reports required value_domain.size, with no text payload on too-small.
     * The label describes source interpretation, not a transformation. */
} qpx_matrix_metadata_v2;

typedef struct qpx_matrix_values_buffer_v2 {
    uint32_t struct_size,value_type;
    uint64_t* counts;
    double* continuous;
    uint64_t capacity,written,reserved;
    /* Only the pointer for the selected metadata value_type is supplied; the
     * other is NULL. No domain conversion across this boundary. Pages use the
     * same dense observation-major, feature-minor order and range contract as
     * ABI1. Continuous values are finite IEEE754 binary64, including signed
     * zero; counts retain all64 bits. No missingness/normalization inference. */
} qpx_matrix_values_buffer_v2;

typedef struct qpx_matrix_format_api_v2 {
    uint32_t struct_size,abi_version;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *describe)(qpx_format_info_v1*,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *open)(const qpx_matrix_open_v2*,void**,qpx_reader_text_v1*);
    void (QPX_FORMAT_CALL *destroy)(void*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *metadata)(void*,qpx_matrix_metadata_v2*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *axis)(void*,const qpx_matrix_axis_request_v1*,qpx_matrix_axis_buffer_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *values)(void*,const qpx_matrix_values_request_v1*,qpx_matrix_values_buffer_v2*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_matrix_format_api_v2;

typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_matrix_format_entry_fn_v2)(uint32_t,qpx_matrix_format_api_v2*,qpx_reader_text_v1*);
QPX_MATRIX_TYPED_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v2(uint32_t,qpx_matrix_format_api_v2*,qpx_reader_text_v1*);

/* Separate local-directory matrix ABI3. The host observes every regular member
 * and fingerprints canonical relative keys/lengths/bytes, independently of the
 * observed filesystem-version digest. primary_key explicitly selects an
 * existing metadata member; it is never inferred from the suffix. The provider
 * reads only this store, cannot publish results, and retains no host pointer.
 * file_bytes bounds EACH member; total_bytes bounds the complete input store.
 * work_bytes is the provider allowance, independent of host inventory storage.
 * Metadata, axis and typed page semantics retain their ABI2 definitions. */
typedef struct qpx_matrix_open_v3 {
    uint32_t struct_size,reserved;
    qpx_reader_string_v1 path,primary_key,options;
    uint64_t work_bytes,file_bytes,total_bytes;
    qpx_reader_cancel_v1 cancel;
} qpx_matrix_open_v3;

typedef struct qpx_matrix_format_api_v3 {
    uint32_t struct_size,abi_version;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *describe)(qpx_format_info_v1*,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *open)(const qpx_matrix_open_v3*,void**,qpx_reader_text_v1*);
    void (QPX_FORMAT_CALL *destroy)(void*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *metadata)(void*,qpx_matrix_metadata_v2*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *axis)(void*,const qpx_matrix_axis_request_v1*,qpx_matrix_axis_buffer_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *values)(void*,const qpx_matrix_values_request_v1*,qpx_matrix_values_buffer_v2*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_matrix_format_api_v3;

typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_matrix_format_entry_fn_v3)(uint32_t,qpx_matrix_format_api_v3*,qpx_reader_text_v1*);
QPX_MATRIX_STORE_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v3(uint32_t,qpx_matrix_format_api_v3*,qpx_reader_text_v1*);

/* Directory ABI4 retains ABI3's open/axis/dense functions and adds stored
 * entry pages. A dense source reports sparse=0 and entries=0. A sparse source
 * reports sparse=1 and the number of explicitly stored entries (possibly zero).
 * Each sparse entry has a strictly increasing observation-major linear index.
 * Absent coordinates mean positive zero. An explicit zero remains an entry.
 * value_bits is the exact uint64 count or IEEE754 finite binary64 bit pattern,
 * as selected by the unchanged matrix metadata callback. Page offsets/counts
 * index stored entries, and use the same bounds and buffer protocol as values.
 * directory is an embedded ABI3 table with its original size and version. */
typedef struct qpx_matrix_sparse_metadata_v1 {
    uint32_t struct_size,sparse;
    uint64_t entries,reserved;
} qpx_matrix_sparse_metadata_v1;

typedef struct qpx_matrix_sparse_entry_v1 {
    uint64_t linear,value_bits;
} qpx_matrix_sparse_entry_v1;

typedef struct qpx_matrix_sparse_buffer_v1 {
    uint32_t struct_size,reserved;
    qpx_matrix_sparse_entry_v1* entries;
    uint64_t capacity,written;
} qpx_matrix_sparse_buffer_v1;

typedef struct qpx_matrix_format_api_v4 {
    uint32_t struct_size,abi_version;
    qpx_matrix_format_api_v3 directory;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *sparse_metadata)(void*,qpx_matrix_sparse_metadata_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *sparse_values)(void*,const qpx_matrix_values_request_v1*,qpx_matrix_sparse_buffer_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_matrix_format_api_v4;

typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_matrix_format_entry_fn_v4)(uint32_t,qpx_matrix_format_api_v4*,qpx_reader_text_v1*);
/* ABI3-only modules must not be required to export this entry. */
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_MATRIX_SPARSE_EXPORTS)
#define QPX_MATRIX_SPARSE_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_MATRIX_SPARSE_FORMAT_EXPORT
#endif
QPX_MATRIX_SPARSE_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v4(uint32_t,qpx_matrix_format_api_v4*,qpx_reader_text_v1*);

/* Single-file ABI5 retains ABI2's bounded file open and typed callbacks and
 * adds the same stored-entry semantics as ABI4. The embedded file table
 * retains its original size and ABI2 version. No directory key is supplied. */
typedef struct qpx_matrix_format_api_v5 {
    uint32_t struct_size,abi_version;
    qpx_matrix_format_api_v2 file;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *sparse_metadata)(void*,qpx_matrix_sparse_metadata_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *sparse_values)(void*,const qpx_matrix_values_request_v1*,qpx_matrix_sparse_buffer_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_matrix_format_api_v5;

typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_matrix_format_entry_fn_v5)(uint32_t,qpx_matrix_format_api_v5*,qpx_reader_text_v1*);
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_MATRIX_SPARSE_FILE_EXPORTS)
#define QPX_MATRIX_SPARSE_FILE_FORMAT_EXPORT __declspec(dllexport)
#else
#define QPX_MATRIX_SPARSE_FILE_FORMAT_EXPORT
#endif
QPX_MATRIX_SPARSE_FILE_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v5(uint32_t,qpx_matrix_format_api_v5*,qpx_reader_text_v1*);

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

/* Annotation ABI2 is a separate entry/table. The ABI1 tables and descriptors
 * keep their exact layout and semantics. ABI2 describe uses qpx_format_info_v1
 * with abi_version=2. Matrix entries are negotiated separately. */
typedef struct qpx_annotation_metadata_v2 {
    uint32_t struct_size,coordinate_frame,geometry_encoding,reserved;
    uint64_t objects,geometry_bytes,reserved64[2];
} qpx_annotation_metadata_v2;

typedef struct qpx_annotation_objects_request_v2 {
    uint32_t struct_size,reserved;
    uint64_t first,count;
} qpx_annotation_objects_request_v2;

typedef struct qpx_annotation_object_v2 {
    int32_t c,z,t;
    uint32_t reserved;
    uint64_t source_id_offset,source_id_size,class_offset,class_size;
    uint64_t geometry_offset,geometry_size;
} qpx_annotation_object_v2;

typedef struct qpx_annotation_geometry_bytes_v2 {
    uint8_t* data;
    uint64_t capacity,size;
} qpx_annotation_geometry_bytes_v2;

typedef struct qpx_annotation_objects_buffer_v2 {
    uint32_t struct_size,reserved;
    qpx_annotation_object_v2* objects;
    uint64_t capacity,written;
    qpx_reader_text_v1 text;
    qpx_annotation_geometry_bytes_v2 geometry;
} qpx_annotation_objects_buffer_v2;

typedef struct qpx_annotation_format_api_v2 {
    uint32_t struct_size,abi_version;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *describe)(qpx_format_info_v1*,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *open)(const qpx_reader_open_v1*,void**,qpx_reader_text_v1*);
    void (QPX_FORMAT_CALL *destroy)(void*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *metadata)(void*,qpx_annotation_metadata_v2*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *objects)(void*,const qpx_annotation_objects_request_v2*,qpx_annotation_objects_buffer_v2*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_annotation_format_api_v2;

typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_annotation_format_entry_fn_v2)(uint32_t,qpx_annotation_format_api_v2*,qpx_reader_text_v1*);
QPX_ANNOTATION_GEOMETRY_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v2(uint32_t,qpx_annotation_format_api_v2*,qpx_reader_text_v1*);

/* ABI3 adds independent display names, immutable source facts and original
 * plane descriptors. ABI1/2 entries/tables keep their original exact contracts. */
typedef struct qpx_annotation_metadata_v3 {
    uint32_t struct_size,coordinate_frame;
    uint64_t objects,geometry_bytes,source_bytes,properties,geometry_encodings;
    uint64_t reserved[2];
    /* source_bytes is the exact sum of UTF-8 name/key/value bytes, excluding
     * source IDs/classes. geometry_encodings uses bit (encoding-1). */
} qpx_annotation_metadata_v3;

typedef struct qpx_annotation_property_v3 {
    uint64_t key_offset,key_size,value_offset,value_size;
} qpx_annotation_property_v3;

typedef struct qpx_annotation_object_v3 {
    qpx_annotation_object_v2 value;
    uint64_t name_offset,name_size,first_property,properties;
    uint32_t geometry_encoding,plane_kind,flags,color_rgb;
    uint64_t linear_position,reserved;
} qpx_annotation_object_v3;

typedef struct qpx_annotation_objects_buffer_v3 {
    uint32_t struct_size,reserved;
    qpx_annotation_object_v3* objects;
    uint64_t capacity,written;
    qpx_reader_text_v1 text;
    qpx_annotation_geometry_bytes_v2 geometry;
    qpx_annotation_property_v3* properties;
    uint64_t property_capacity,properties_written;
} qpx_annotation_objects_buffer_v3;

typedef struct qpx_annotation_format_api_v3 {
    uint32_t struct_size,abi_version;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *describe)(qpx_format_info_v1*,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *open)(const qpx_reader_open_v1*,void**,qpx_reader_text_v1*);
    void (QPX_FORMAT_CALL *destroy)(void*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *metadata)(void*,qpx_annotation_metadata_v3*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
    qpx_reader_status_v1 (QPX_FORMAT_CALL *objects)(void*,const qpx_annotation_objects_request_v2*,qpx_annotation_objects_buffer_v3*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_annotation_format_api_v3;
typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_annotation_format_entry_fn_v3)(uint32_t,qpx_annotation_format_api_v3*,qpx_reader_text_v1*);
QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v3(uint32_t,qpx_annotation_format_api_v3*,qpx_reader_text_v1*);

/* ABI4 retains the ABI3 value layout and adds the LOCKED object flag. Its
 * separate symbol/version prevents an older host from dropping edit locks.
 * Native lock state participates in ABI4 row/recipe/preview identities. */
typedef qpx_annotation_format_api_v3 qpx_annotation_format_api_v4;
typedef qpx_annotation_format_entry_fn_v3 qpx_annotation_format_entry_fn_v4;
QPX_ANNOTATION_LOCKED_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v4(uint32_t,qpx_annotation_format_api_v4*,qpx_reader_text_v1*);

/* ABI5 retains ABI4's table/value layouts and lock semantics, adding QBM1.
 * The separate entry prevents older hosts from interpreting mask coverage as
 * vector paths. ABI1-4 cannot advertise or return the new encoding.
 * BINARY_MASK1: bytes 'Q','B','M',1, u32 width, u32 height (little-endian),
 * then ceil(width*height/8) bytes, row-major MSB-first, zero unused tail bits.
 * Dimensions are positive <=1,000,000; total blob <=1MiB; no trailing bytes.
 * The FULL canvas must match the selected native level-zero pixel frame.
 * Each set bit denotes the whole unit pixel cell [x,x+1] x [y,y+1]. One
 * nonempty mask becomes one object, including disconnected parts and holes.
 * The host reuses its exact native label/grid/contour services, with bounded
 * raster/edge/vertex work and cancellation. No resampling, hole filling or
 * instance splitting. Unrepresentable/empty coverage refuses atomically. */
typedef qpx_annotation_format_api_v3 qpx_annotation_format_api_v5;
typedef qpx_annotation_format_entry_fn_v3 qpx_annotation_format_entry_fn_v5;
QPX_ANNOTATION_MASK_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v5(uint32_t,qpx_annotation_format_api_v5*,qpx_reader_text_v1*);

/* ABI6 adds a required full source canvas in integral level-zero image
 * pixels. The native target dimensions must match exactly before any object
 * page can be used. The source table retains ABI5's layout and interpretation
 * and carries the negotiated ABI6 version. */
typedef struct qpx_annotation_canvas_v1 {
    uint32_t struct_size,reserved;
    uint64_t width,height;
} qpx_annotation_canvas_v1;

typedef struct qpx_annotation_format_api_v6 {
    uint32_t struct_size,abi_version;
    qpx_annotation_format_api_v5 source;
    qpx_reader_status_v1 (QPX_FORMAT_CALL *canvas)(void*,qpx_annotation_canvas_v1*,qpx_reader_cancel_v1,qpx_reader_text_v1*);
} qpx_annotation_format_api_v6;

typedef qpx_reader_status_v1 (QPX_FORMAT_CALL *qpx_annotation_format_entry_fn_v6)(uint32_t,qpx_annotation_format_api_v6*,qpx_reader_text_v1*);
#if defined(_WIN32) && defined(QPX_FORMAT_PLUGIN_ANNOTATION_CANVAS_EXPORTS)
#define QPX_ANNOTATION_CANVAS_EXPORT __declspec(dllexport)
#else
#define QPX_ANNOTATION_CANVAS_EXPORT
#endif
QPX_ANNOTATION_CANVAS_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v6(uint32_t,qpx_annotation_format_api_v6*,qpx_reader_text_v1*);

/* ABI3 uses the same synchronous page/probe/ownership rules. Property spans are
 * packed in object order with no unused records; keys are unique per object,
 * nonempty <=256-byte UTF-8 without control characters. Values are <=4096-byte
 * UTF-8 without NUL (newlines/tabs are allowed); names are <=4096 bytes. At most
 * 256 facts per object; source_bytes/properties totals are independently checked.
 * The host namespaces source facts separately from its protected receipts.
 * Empty display names select the exact source ID; source/native mappings always
 * use source IDs, never display names. flags supports only COLOR_RGB, with a
 * 24-bit color; color_rgb is zero without that flag.
 * CZT descriptors permit c,z,t=-1 for all/unpositioned axes. Native C retains
 * all-channel -1; native Z/T need a unique target plane or an explicit native
 * choice. LINEAR_POSITION requires c=z=t=-1 and positive one-based position;
 * the native caller must name its axis. No filename/axis/coordinate inference.
 * EVEN_ODD_RINGS1 is bytes 'Q','E','C',1; u32 closed-ring count; each ring is
 * u32 vertex count then binary64 x,y pairs, all little-endian. Rings use even-odd
 * nesting, not orientation; native Roi authorities reject crossings/duplicates
 * or invalid topology rather than repairing it. At most1024 rings/1,000,000
 * vertices, with a separate bounded native pair/vertex operation limit. */

/* ABI2 retains the ABI1 page/probe/ownership rules below. All byte spans are
 * host-owned. Geometry spans are nonempty, packed in object order with no gaps,
 * overlap or unused bytes; metadata.geometry_bytes is their exact source total.
 * QRO1 is the existing native, non-executable geometry value encoding:
 *   bytes 'Q','R','O',1; one kind byte; then the following little-endian payload.
 *   kind0 rectangle / kind1 ellipse: four IEEE754 binary64 x,y,width,height.
 *   kind4 polyline / kind5 points: ring.
 *   kind2 polygon / kind3 multipolygon: u32 part count; each part is ring shell,
 *      u32 hole count, then that many rings. Polygon has one part; Multi >=2.
 *   ring: u32 vertex count, then binary64 x,y pairs. Counts <=1,000,000 and
 *      combined vertex count <=1,000,000. No trailing bytes; blob <=32MiB.
 * Coordinates are level-zero image pixels. Native Roi validation owns bounds,
 * finite values and topology. Polygon rings represent closed boundaries; one
 * matching final vertex is omitted from the native path. No repair/rescaling.
 * The current host reads one ABI2 object per page, admits <=1MiB geometry per
 * page and independently charges decode scratch and retained native geometry.
 * Invalid encodings/planes/quotas fail before atomic native publication. */

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
