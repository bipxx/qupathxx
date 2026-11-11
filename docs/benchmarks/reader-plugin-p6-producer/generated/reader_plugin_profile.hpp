#pragma once
#if !defined(_MSC_VER) || _MSC_VER != 1944 || !defined(_WIN64) || !defined(_M_X64)
#error "This reader-plugin kit requires its recorded Windows x64 MSVC compiler series"
#endif
#if !defined(_DLL) || defined(_DEBUG)
#error "This reader-plugin kit requires Release dynamic CRT (/MD)"
#endif
#include <yvals_core.h>
#if _ITERATOR_DEBUG_LEVEL != 0
#error "This reader-plugin kit requires _ITERATOR_DEBUG_LEVEL=0"
#endif
static_assert(sizeof(void*) == 8, "reader-plugin kit architecture differs");
