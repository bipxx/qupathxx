#include "qupathxx/format_plugin.h"
#include <stddef.h>
/* Compile the typed C contract independently of the host and its C++ headers. */
_Static_assert(sizeof(uint64_t)==8 && sizeof(uint32_t)==4 && sizeof(int32_t)==4,"Fixed-width ABI fields");
_Static_assert(offsetof(qpx_format_info_v1,struct_size)==0,"Descriptor size prefix");
_Static_assert(offsetof(qpx_matrix_format_api_v1,struct_size)==0,"Matrix API size prefix");
_Static_assert(offsetof(qpx_matrix_format_api_v4,directory)==8,"Embedded directory API layout");
_Static_assert(offsetof(qpx_matrix_format_api_v5,file)==8,"Embedded single-file API layout");
_Static_assert(QPX_MATRIX_ABI_V5==5u,"Single-file sparse ABI version");
_Static_assert(QPX_ANNOTATION_ABI_V6==6u,"Canvas annotation ABI version");
_Static_assert(sizeof(qpx_annotation_canvas_v1)==24,"Annotation canvas metadata layout");
_Static_assert(offsetof(qpx_annotation_format_api_v6,source)==8,"Embedded annotation source table layout");
_Static_assert(sizeof(qpx_matrix_sparse_entry_v1)==16,"Sparse entry layout");
_Static_assert(offsetof(qpx_annotation_format_api_v1,struct_size)==0,"Annotation API size prefix");
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(qpx_format_info_v1)==160,"Typed descriptor layout");
_Static_assert(sizeof(qpx_matrix_metadata_v1)==40,"Matrix metadata layout");
_Static_assert(sizeof(qpx_matrix_axis_request_v1)==24,"Axis request layout");
_Static_assert(sizeof(qpx_matrix_axis_row_v1)==40,"Axis row layout");
_Static_assert(sizeof(qpx_matrix_axis_buffer_v1)==56,"Axis caller buffer layout");
_Static_assert(sizeof(qpx_matrix_values_request_v1)==24,"Values request layout");
_Static_assert(sizeof(qpx_matrix_values_buffer_v1)==32,"Values caller buffer layout");
_Static_assert(sizeof(qpx_matrix_format_api_v1)==56,"Matrix callback table layout");
_Static_assert(sizeof(qpx_matrix_open_v2)==72,"Typed matrix independent file/work request layout");
_Static_assert(sizeof(qpx_matrix_metadata_v2)==64,"Typed matrix domain metadata layout");
_Static_assert(sizeof(qpx_matrix_values_buffer_v2)==48,"Typed matrix disjoint value buffers layout");
_Static_assert(sizeof(qpx_matrix_format_api_v2)==56,"Typed matrix callback table layout");
_Static_assert(sizeof(qpx_matrix_open_v3)==96,"Directory matrix open request layout");
_Static_assert(sizeof(qpx_matrix_format_api_v3)==56,"Directory matrix callback table layout");
_Static_assert(sizeof(qpx_matrix_format_api_v5)==80,"Sparse single-file callback table layout");
_Static_assert(offsetof(qpx_matrix_format_api_v5,sparse_metadata)==64,"Sparse single-file metadata callback offset");
_Static_assert(offsetof(qpx_matrix_open_v3,primary_key)==24,"Directory matrix primary-key offset");
_Static_assert(offsetof(qpx_matrix_open_v3,total_bytes)==72,"Directory matrix total byte allowance offset");
_Static_assert(offsetof(qpx_matrix_open_v3,cancel)==80,"Directory matrix cancellation offset");
_Static_assert(offsetof(qpx_matrix_open_v2,file_bytes)==48,"Typed matrix file allowance offset");
_Static_assert(offsetof(qpx_matrix_metadata_v2,value_domain)==24,"Typed matrix domain text offset");
_Static_assert(offsetof(qpx_matrix_values_buffer_v2,continuous)==16,"Typed matrix continuous pointer offset");
_Static_assert(sizeof(qpx_annotation_metadata_v1)==32,"Annotation metadata layout");
_Static_assert(sizeof(qpx_annotation_points_request_v1)==24,"Point request layout");
_Static_assert(sizeof(qpx_annotation_point_v1)==64,"Typed point layout");
_Static_assert(sizeof(qpx_annotation_points_buffer_v1)==56,"Point caller buffer layout");
_Static_assert(sizeof(qpx_annotation_format_api_v1)==48,"Annotation callback table layout");
_Static_assert(offsetof(qpx_annotation_point_v1,source_id_offset)==32,"Point text span offset");
_Static_assert(offsetof(qpx_matrix_axis_buffer_v1,text)==32,"Axis text buffer offset");
_Static_assert(sizeof(qpx_annotation_metadata_v2)==48,"Geometry metadata layout");
_Static_assert(sizeof(qpx_annotation_objects_request_v2)==24,"Geometry request layout");
_Static_assert(sizeof(qpx_annotation_object_v2)==64,"Geometry row layout");
_Static_assert(sizeof(qpx_annotation_geometry_bytes_v2)==24,"Geometry byte buffer layout");
_Static_assert(sizeof(qpx_annotation_objects_buffer_v2)==80,"Geometry caller buffer layout");
_Static_assert(sizeof(qpx_annotation_format_api_v2)==48,"Geometry callback table layout");
_Static_assert(offsetof(qpx_annotation_object_v2,geometry_offset)==48,"Geometry span offset");
_Static_assert(offsetof(qpx_annotation_objects_buffer_v2,geometry)==56,"Geometry byte buffer offset");
_Static_assert(sizeof(qpx_annotation_metadata_v3)==64,"Source metadata layout");
_Static_assert(sizeof(qpx_annotation_property_v3)==32,"Source fact layout");
_Static_assert(sizeof(qpx_annotation_object_v3)==128,"Source object layout");
_Static_assert(sizeof(qpx_annotation_objects_buffer_v3)==104,"Source caller buffer layout");
_Static_assert(sizeof(qpx_annotation_format_api_v3)==48,"Source callback layout");
_Static_assert(sizeof(qpx_annotation_format_api_v4)==48,"Locked source callback layout");
_Static_assert(sizeof(qpx_annotation_format_api_v5)==48,"Pixel-mask source callback layout");
_Static_assert(sizeof(qpx_annotation_format_api_v6)==64,"Annotation canvas table layout");
_Static_assert(offsetof(qpx_annotation_format_api_v6,canvas)==56,"Annotation canvas callback offset");
_Static_assert(offsetof(qpx_annotation_object_v3,name_offset)==64,"Independent source name offset");
_Static_assert(offsetof(qpx_annotation_object_v3,geometry_encoding)==96,"Source encoding offset");
_Static_assert(offsetof(qpx_annotation_objects_buffer_v3,properties)==80,"Source property buffer offset");
#endif
int main(void){return QPX_ANNOTATION_ABI_V5==5u && QPX_FORMAT_GEOMETRY_BINARY_MASK1==3u && QPX_ANNOTATION_ABI_V4==4u && QPX_ANNOTATION_OBJECT_LOCKED==2u && QPX_ANNOTATION_ABI_V3==3u && QPX_ANNOTATION_ABI_V2==2u && QPX_FORMAT_GEOMETRY_QRO1==1u && QPX_FORMAT_ABI_V1==1u && QPX_FORMAT_COUNTS_UINT64==1u && QPX_FORMAT_IMAGE_PIXELS==1u?0:1;}
