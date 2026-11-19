// SPDX-License-Identifier: AGPL-3.0-only
#include "annotation_geometry_value.hpp"
#include "geometry_archive.hpp"
#include "qupathxx/format_plugin.h"
#include "qupathxx/label_geometry.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace qpx::detail {
AnnotationPixelMask
decode_annotation_pixel_mask(const std::uint8_t *bytes, std::size_t size,
                             std::size_t work_bytes,
                             const std::atomic<bool> *cancelled) {
  workflow_checkpoint(cancelled);
  if (!bytes || size < 13 || size > 1024u * 1024 ||
      std::memcmp(bytes, "QBM\1", 4))
    throw std::invalid_argument("Invalid annotation binary-mask encoding");
  auto u32 = [&](unsigned at) {
    std::uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i)
      n |= std::uint32_t(bytes[at + i]) << (8 * i);
    return n;
  };
  const auto w = u32(4), h = u32(8);
  const auto pixels = std::uint64_t(w) * h;
  if (!w || !h || w > 1000000 || h > 1000000 || (pixels + 7) / 8 != size - 12 ||
      ((pixels & 7) && (bytes[size - 1] & ((1u << (8 - (pixels & 7))) - 1))))
    throw std::invalid_argument(
        "Annotation binary-mask dimensions/length/tail differ");
  if (work_bytes <= 131072 || pixels > (work_bytes - 131072) / 8)
    throw WorkflowCaptureLimit(
        "Annotation binary-mask raster exceeds available work");
  std::vector<std::uint32_t> labels(std::size_t(pixels), 0);
  for (std::size_t i = 0; i < pixels; ++i) {
    if (!(i & 4095))
      workflow_checkpoint(cancelled);
    labels[i] = (bytes[12 + i / 8] >> (7 - i % 8)) & 1;
  }
  const LabelRaster raster(w, h, std::move(labels), {1, work_bytes}, cancelled);
  if (!raster.foreground_pixels())
    throw std::invalid_argument("Annotation binary mask has empty coverage");
  const ScientificGrid grid(w, h, {0, w, w}, {0, h, h});
  const auto clip = Roi::rectangle({0, 0, double(w), double(h)});
  TissueContourOptions options;
  options.max_components = 1;
  // The contour authority charges the raster in its base reservation. Leave
  // half for owned output/canonical serialization and geometry validation.
  options.work_bytes = work_bytes / 2;
  const auto contours =
      trace_label_raster(raster, grid, clip, options, cancelled);
  if (contours->components().size() != 1 ||
      contours->components()[0].geometry.area() !=
          double(raster.foreground_pixels()))
    throw std::invalid_argument(
        "Annotation mask contours do not conserve exact coverage");
  auto geometry = contours->components()[0].geometry;
  if (geometry.payload_bytes() > (work_bytes / 2 - 65536) / 8)
    throw WorkflowCaptureLimit(
        "Annotation mask result exceeds geometry work reservation");
  workflow_checkpoint(cancelled);
  return {std::move(geometry), w, h};
}
Roi decode_annotation_geometry_value(std::uint32_t encoding,
                                     const std::uint8_t *bytes,
                                     std::size_t size,
                                     const std::atomic<bool> *cancelled) {
  workflow_checkpoint(cancelled);
  if (encoding == QPX_FORMAT_GEOMETRY_QRO1)
    return GeometryArchiveCodec::decode(bytes, size);
  if (encoding != QPX_FORMAT_GEOMETRY_EVEN_ODD_RINGS1 || !bytes || size < 8 ||
      size > 1024u * 1024 || std::memcmp(bytes, "QEC\1", 4) != 0)
    throw std::invalid_argument("Invalid annotation contour encoding");
  std::size_t at = 4, total = 0;
  auto integer = [&] {
    if (size - at < 4)
      throw std::invalid_argument("Truncated annotation contour count");
    std::uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i)
      n |= std::uint32_t(bytes[at++]) << (i * 8);
    return n;
  };
  auto real = [&] {
    if (size - at < 8)
      throw std::invalid_argument("Truncated annotation contour coordinate");
    std::uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i)
      bits |= std::uint64_t(bytes[at++]) << (i * 8);
    double value = 0;
    std::memcpy(&value, &bits, 8);
    if (!std::isfinite(value) || value < 0)
      throw std::invalid_argument("Invalid annotation contour coordinate");
    return value;
  };
  const auto count = integer();
  if (!count || count > 1024 || count > (size - at) / 52)
    throw std::invalid_argument("Annotation contour count exceeds bounds");
  struct Contour {
    Ring ring;
    Roi roi;
    std::size_t depth = 0, owner = 0;
  };
  std::vector<Contour> contours;
  contours.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    workflow_checkpoint(cancelled);
    const auto n = integer();
    if (n < 3 || n > 1000000 - total || n > (size - at) / 16)
      throw std::invalid_argument(
          "Annotation contour vertex count exceeds bounds");
    total += n;
    Ring ring;
    ring.reserve(n);
    for (std::uint32_t j = 0; j < n; ++j) {
      if ((j & 255) == 0)
        workflow_checkpoint(cancelled);
      const auto x = real(), y = real();
      ring.emplace_back(x, y);
    }
    auto roi = Roi::polygon(ring);
    contours.push_back({std::move(ring), std::move(roi), 0, 0});
  }
  if (at != size)
    throw std::invalid_argument("Trailing annotation contour bytes");
  if (total > 2000000 / count)
    throw WorkflowCaptureLimit(
        "Annotation contour pair/vertex work exceeds limit");
  std::stable_sort(
      contours.begin(), contours.end(),
      [](const auto &a, const auto &b) { return a.roi.area() > b.roi.area(); });
  std::vector<PolygonPart> parts;
  parts.reserve(count);
  for (std::size_t i = 0; i < contours.size(); ++i) {
    workflow_checkpoint(cancelled);
    auto &current = contours[i];
    std::size_t parent = contours.size();
    for (std::size_t j = 0; j < i; ++j) {
      workflow_checkpoint(cancelled);
      if (contours[j].roi.covers(current.roi)) {
        if (current.roi.covers(contours[j].roi))
          throw std::invalid_argument("Duplicate annotation contours");
        parent = j;
      } else if (contours[j].roi.boolean_operation(current.roi,
                                                   BooleanOp::Intersection))
        throw std::invalid_argument(
            "Annotation contours cross without containment");
    }
    current.depth = parent == contours.size() ? 0 : contours[parent].depth + 1;
    if ((current.depth & 1) == 0) {
      current.owner = parts.size();
      parts.emplace_back(current.ring);
    } else {
      current.owner = contours[parent].owner;
      parts.at(current.owner).holes.push_back(current.ring);
    }
  }
  workflow_checkpoint(cancelled);
  return Roi::multipolygon(std::move(parts));
}
} // namespace qpx::detail
