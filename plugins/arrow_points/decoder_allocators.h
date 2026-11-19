// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <stdlib.h>

void* qpx_arrow_malloc(size_t size);
void* qpx_arrow_calloc(size_t count, size_t size);
void* qpx_arrow_realloc(void* ptr, size_t size);
void qpx_arrow_free(void* ptr);

#define malloc qpx_arrow_malloc
#define calloc qpx_arrow_calloc
#define realloc qpx_arrow_realloc
#define free qpx_arrow_free
