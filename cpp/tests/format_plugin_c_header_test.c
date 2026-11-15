#include "qupathxx/format_plugin.h"
#include <stddef.h>
/* Compile the typed C contract independently of the host and its C++ headers. */
_Static_assert(sizeof(uint64_t)==8 && sizeof(uint32_t)==4 && sizeof(int32_t)==4,"Fixed-width ABI fields");
_Static_assert(offsetof(qpx_format_info_v1,struct_size)==0,"Descriptor size prefix");
_Static_assert(offsetof(qpx_matrix_format_api_v1,struct_size)==0,"Matrix API size prefix");
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
_Static_assert(sizeof(qpx_annotation_metadata_v1)==32,"Annotation metadata layout");
_Static_assert(sizeof(qpx_annotation_points_request_v1)==24,"Point request layout");
_Static_assert(sizeof(qpx_annotation_point_v1)==64,"Typed point layout");
_Static_assert(sizeof(qpx_annotation_points_buffer_v1)==56,"Point caller buffer layout");
_Static_assert(sizeof(qpx_annotation_format_api_v1)==48,"Annotation callback table layout");
_Static_assert(offsetof(qpx_annotation_point_v1,source_id_offset)==32,"Point text span offset");
_Static_assert(offsetof(qpx_matrix_axis_buffer_v1,text)==32,"Axis text buffer offset");
#endif
int main(void){return QPX_FORMAT_ABI_V1==1u && QPX_FORMAT_COUNTS_UINT64==1u && QPX_FORMAT_IMAGE_PIXELS==1u?0:1;}
