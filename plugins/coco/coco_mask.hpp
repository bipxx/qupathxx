// SPDX-License-Identifier: BSD-2-Clause
// Bounded adaptation of COCO maskApi.c rleFrPoly / rleFrString.
// Copyright (c) 2014, Piotr Dollar and Tsung-Yi Lin. All rights reserved.
// Copyright (c) 2026 qupathxx contributors.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of the FreeBSD Project.
#pragma once
#include "../format-common/annotation_json.hpp"
#include <xmmintrin.h>
namespace qpx_coco {
using namespace qpx_annotation_json;
struct Mask {
  std::uint32_t width, height, pixels;
  std::pmr::vector<std::uint8_t> bits;
  std::pmr::vector<std::pair<int, int>> points;
  std::pmr::vector<std::uint32_t> boundaries;
  qpx_reader_cancel_v1 cancel;
  std::uint64_t steps = 0;
  Mask(std::uint32_t w, std::uint32_t h, std::pmr::memory_resource *r,
       qpx_reader_cancel_v1 c)
      : width(w), height(h), pixels(w * h), bits(r), points(r), boundaries(r),
        cancel(c) {
    bits.resize((std::size_t(pixels) + 7) / 8);
  }
  void step() {
    if (!(++steps & 4095))
      poll(cancel);
    valid(steps <= 64000000, "COCO selected-image raster work limit exceeded");
  }
  void clear() { std::fill(bits.begin(), bits.end(), std::uint8_t(0)); }
  void foreground(std::uint32_t first, std::uint32_t end) {
    valid(first <= end && end <= pixels, "COCO RLE exceeds image canvas");
    for (auto i = first; i < end; ++i) {
      step();
      // COCO linear positions are column-major; QBM1 is row-major MSB-first.
      const auto x = i / height, y = i % height;
      const auto pos = y * width + x;
      bits[pos / 8] |= std::uint8_t(128u >> (pos % 8));
    }
  }
  void rle(const Tree &t, const Node &n) {
    t.keys(n, {"size", "counts"});
    const auto &size = t.array(t.get(n, "size"));
    valid(size.size() == 2 && integer(t.nodes[size[0]]) == height &&
              integer(t.nodes[size[1]]) == width,
          "COCO RLE size differs from selected image [height,width]");
    const auto &counts = t.get(n, "counts");
    std::uint64_t at = 0, count = 0;
    auto run = [&](std::uint64_t length) {
      step();
      valid(length <= pixels - at, "COCO RLE run total exceeds canvas");
      if (count & 1)
        foreground(std::uint32_t(at), std::uint32_t(at + length));
      at += length;
      ++count;
    };
    if (counts.kind == Kind::Array) {
      valid(!counts.children.empty() &&
                counts.children.size() <= std::uint64_t(pixels) * 2 + 2,
            "COCO RLE count limit exceeded");
      for (const auto id : counts.children)
        run(integer(t.nodes[id]));
    } else {
      const auto s = string(counts);
      valid(!s.empty(), "Empty compressed COCO RLE");
      std::uint64_t previous[2] = {0, 0};
      for (std::size_t p = 0; p < s.size();) {
        std::uint64_t value = 0;
        unsigned shift = 0;
        std::uint8_t c = 0;
        do {
          step();
          valid(p < s.size() && shift < 35,
                "Truncated/oversized compressed COCO RLE integer");
          c = std::uint8_t(s[p++]);
          valid(c >= 48 && c <= 111, "Invalid compressed COCO RLE character");
          c -= 48;
          value |= std::uint64_t(c & 31) << shift;
          shift += 5;
        } while (c & 32);
        const auto signed_value =
            (c & 16) ? std::int64_t(value) - std::int64_t(UINT64_C(1) << shift)
                     : std::int64_t(value);
        const auto length =
            signed_value + (count > 2 ? std::int64_t(previous[count % 2]) : 0);
        valid(length >= 0 && std::uint64_t(length) <= pixels,
              "Compressed COCO RLE count outside canvas");
        previous[count % 2] = std::uint64_t(length);
        run(std::uint64_t(length));
        valid(count <= std::uint64_t(pixels) * 2 + 2,
              "COCO RLE count limit exceeded");
      }
    }
    valid(at == pixels, "COCO RLE run total does not fill canvas");
  }
  void polygon(const Tree &t, const Node &n) {
    const auto &coords = t.array(n);
    valid(coords.size() >= 6 && !(coords.size() & 1) && coords.size() <= 200000,
          "COCO polygon coordinate count exceeds bounds");
    // The reference uses binary64 arithmetic followed by C integer truncation.
    // Refuse altered host FP controls instead of changing caller state.
    valid((_mm_getcsr() & 0xe040u) == 0 &&
              (_mm_getcsr() & _MM_MASK_MASK) == _MM_MASK_MASK,
          "COCO polygon rasterization requires nearest rounding, gradual "
          "underflow and masked FP exceptions");
    points.clear();
    boundaries.clear();
    auto scaled = [&](const Node &v) {
      const auto d = real(v);
      valid(d >= -1000000 && d <= 1000000,
            "COCO polygon coordinate exceeds bounded raster domain");
      return int(5 * d + .5);
    };
    for (std::size_t i = 0; i < coords.size(); i += 2) {
      step();
      points.emplace_back(scaled(t.nodes[coords[i]]),
                          scaled(t.nodes[coords[i + 1]]));
    }
    bool have = false;
    int prev_x = 0, prev_y = 0;
    auto emit = [&](int u, int v) {
      step();
      if (have && u != prev_x) {
        auto xd = (double(u < prev_x ? u : u - 1) + .5) / 5 - .5;
        if (std::floor(xd) == xd && xd >= 0 && xd <= double(width) - 1) {
          auto yd = (double(std::min(v, prev_y)) + .5) / 5 - .5;
          yd = std::ceil(std::max(0., std::min(double(height), yd)));
          valid(boundaries.size() < 2000000,
                "COCO polygon boundary limit exceeded");
          boundaries.push_back(std::uint32_t(xd) * height + std::uint32_t(yd));
        }
      }
      prev_x = u;
      prev_y = v;
      have = true;
    };
    for (std::size_t j = 0; j < points.size(); ++j) {
      auto xs = points[j].first, ys = points[j].second;
      auto xe = points[(j + 1) % points.size()].first,
           ye = points[(j + 1) % points.size()].second;
      const auto dx = std::abs(xe - xs), dy = std::abs(ys - ye);
      const bool flip = (dx >= dy && xs > xe) || (dx < dy && ys > ye);
      if (flip) {
        std::swap(xs, xe);
        std::swap(ys, ye);
      }
      // Repeated vertices contribute the same boundary point; avoid the
      // reference's undefined NaN-to-int conversion on a zero-length edge.
      if (!dx && !dy) {
        emit(xs, ys);
        continue;
      }
      const auto slope = dx >= dy ? double(ye - ys) / dx : double(xe - xs) / dy;
      const auto length = std::max(dx, dy);
      valid(std::uint64_t(length) + 1 <= 64000000 - steps,
            "COCO polygon walk exceeds raster work bound");
      for (int d = 0; d <= length; ++d) {
        const auto pos = flip ? length - d : d;
        if (dx >= dy)
          emit(pos + xs, int(ys + slope * pos + .5));
        else
          emit(int(xs + slope * pos + .5), pos + ys);
      }
    }
    boundaries.push_back(pixels);
    std::uint64_t comparisons = 0;
    std::sort(boundaries.begin(), boundaries.end(), [&](auto a, auto b) {
      if (!(++comparisons & 4095))
        poll(cancel);
      return a < b;
    });
    std::uint32_t at = 0;
    bool on = false;
    for (const auto end : boundaries) {
      step();
      if (on)
        foreground(at, end);
      at = end;
      on = !on;
    }
  }
  void segmentation(const Tree &t, const Node &n) {
    clear();
    if (n.kind == Kind::Array) {
      const auto &parts = t.array(n);
      valid(!parts.empty() && parts.size() <= 1024,
            "COCO polygon part count exceeds bounds");
      // COCO merges parts by union, never by ring orientation or even-odd
      // holes.
      for (const auto part : parts)
        polygon(t, t.nodes[part]);
    } else
      rle(t, n);
    valid(std::any_of(bits.begin(), bits.end(), [](auto b) { return b != 0; }),
          "COCO segmentation has empty pixel coverage");
    poll(cancel);
  }
};
} // namespace qpx_coco
