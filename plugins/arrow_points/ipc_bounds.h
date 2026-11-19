// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int qpx_arrow_footer_schema(const uint8_t* footer);
int qpx_arrow_stream_schema(const uint8_t* message);
int qpx_arrow_batch_bounds(const uint8_t* message, int64_t body_size, int64_t remaining_rows,
                           int dictionary);
#ifdef __cplusplus
}
#endif
