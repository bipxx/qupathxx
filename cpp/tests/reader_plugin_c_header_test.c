#include "qupathxx/reader_plugin.h"
#include <stddef.h>
/* Compile as C, independently of the C++ host. Freeze v1 x64 structure layout
 * so an accidental public-header edit cannot silently change field offsets. */
_Static_assert(offsetof(qpx_reader_api_v1,struct_size)==0,"ABI size prefix");
_Static_assert(sizeof(uint64_t)==8 && sizeof(uint32_t)==4,"ABI fixed-width fields");
#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(qpx_reader_api_v1)==72,"ABI table layout");
_Static_assert(sizeof(qpx_reader_files_api_v1)==24,"Optional file-set ABI layout");
_Static_assert(sizeof(qpx_reader_open_v1)==64,"ABI open layout");
_Static_assert(sizeof(qpx_reader_info_v1)==112,"ABI descriptor layout");
_Static_assert(sizeof(qpx_reader_metadata_v1)==136,"ABI metadata layout");
_Static_assert(sizeof(qpx_reader_level_v1)==32,"ABI level layout");
_Static_assert(sizeof(qpx_reader_region_v1)==56,"ABI request layout");
_Static_assert(sizeof(qpx_reader_pixels_v1)==104,"ABI host-buffer layout");
#endif
int main(void){return QPX_READER_ABI_V1==1u?0:1;}
