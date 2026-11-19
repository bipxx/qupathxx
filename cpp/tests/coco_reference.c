/* SPDX-License-Identifier: AGPL-3.0-only
 * Development-only wrapper around exact BSD-2-Clause COCO API sources.
 * Inputs come only from the controller's admitted, small valid fixtures.
 * This oracle is never linked into the plugin or native application. */
#include "maskApi.h"
#include <stdint.h>
__declspec(dllexport) int
qpx_coco_reference_polygon(const double *xy, uint32_t vertices, uint32_t height,
                           uint32_t width, uint8_t *out) {
  RLE r = {0};
  rleFrPoly(&r, xy, vertices, height, width);
  if (!r.cnts)
    return 0;
  rleDecode(&r, out, 1);
  rleFree(&r);
  return 1;
}
__declspec(dllexport) int qpx_coco_reference_rle(const char *s, uint32_t height,
                                                 uint32_t width, uint8_t *out) {
  RLE r = {0};
  rleFrString(&r, (char *)s, height, width);
  if (!r.cnts)
    return 0;
  rleDecode(&r, out, 1);
  rleFree(&r);
  return 1;
}
