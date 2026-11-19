#pragma once
#include "qupathxx/geometry.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace qpx::detail {
// Data-only native geometry transport, shared by format providers/consumers.
// QRO retains the established codec. Even-odd contours use existing Roi
// topology.
Roi decode_annotation_geometry_value(
    std::uint32_t encoding, const std::uint8_t *, std::size_t,
    const std::atomic<bool> *cancelled = nullptr);
struct AnnotationPixelMask {
  Roi geometry;
  std::uint32_t width = 0, height = 0;
};
AnnotationPixelMask
decode_annotation_pixel_mask(const std::uint8_t *, std::size_t,
                             std::size_t work_bytes,
                             const std::atomic<bool> *cancelled = nullptr);
} // namespace qpx::detail
