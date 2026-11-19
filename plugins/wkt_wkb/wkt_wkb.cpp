// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/wkb_geometry.hpp"
#include "format_plugin_profile.hpp"
#include <array>
#include <charconv>

namespace {
using namespace qpx_annotation_source;
using namespace qpx_wkb;
bool space(std::uint8_t c) { return c == 32 || (c >= 9 && c <= 13); }
struct Wkt {
  Bytes bytes;
  std::size_t at;
  std::pmr::memory_resource *resource;
  Limits limits;
  void whitespace() {
    while (at < bytes.size && space(bytes.data[at])) {
      if ((at & 1023) == 0)
        poll(limits.cancel);
      ++at;
    }
  }
  bool take(char c) {
    whitespace();
    if (at < bytes.size && bytes.data[at] == c) {
      ++at;
      return true;
    }
    return false;
  }
  void need(char c) {
    valid(take(c), "Malformed, empty or non-2D WKT geometry");
  }
  unsigned type() {
    whitespace();
    char word[32];
    std::size_t n = 0;
    while (at < bytes.size) {
      auto c = bytes.data[at];
      if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
      if (c < 'A' || c > 'Z')
        break;
      valid(n < sizeof(word), "WKT geometry keyword exceeds profile");
      word[n++] = char(c);
      ++at;
    }
    for (unsigned i = 1; i < names.size(); ++i)
      if (std::string_view(word, n) == names[i])
        return i;
    fail("Only the seven basic 2D WKT geometry types are supported",
         QPX_READER_UNSUPPORTED);
  }
  double real() {
    const auto first = at;
    while (at < bytes.size && !space(bytes.data[at]) && bytes.data[at] != ',' &&
           bytes.data[at] != ')' && bytes.data[at] != '(') {
      valid(at - first < 256, "WKT numeric token exceeds256 bytes");
      ++at;
    }
    auto token = bytes.sub(first, at - first).view();
    valid(!token.empty(), "WKT coordinate is missing");
    if (token.front() == '+') {
      token.remove_prefix(1);
      valid(!token.empty() && token.front() != '+' && token.front() != '-',
            "Invalid WKT sign");
    }
    double out = 0;
    const auto result =
        std::from_chars(token.data(), token.data() + token.size(), out,
                        std::chars_format::general);
    valid(
        result.ec == std::errc{} && result.ptr == token.data() + token.size() &&
            std::isfinite(out),
        "WKT coordinates require representable finite decimal binary64 values");
    return out;
  }
  Point point() {
    whitespace();
    const auto x = real();
    const auto separator = at;
    whitespace();
    valid(at > separator, "WKT X and Y require a whitespace separator");
    const auto y = real();
    return limits.point(x, y);
  }
  Path path(bool closed) {
    Path out(resource);
    need('(');
    do {
      out.points.push_back(point());
    } while (take(','));
    need(')');
    if (closed)
      ring(out);
    else
      valid(out.points.size() >= 2, "LineString requires at least two points");
    return out;
  }
  Polygon polygon() {
    Polygon out(resource);
    need('(');
    do {
      valid(out.rings.size() < 65535, "Polygon ring count exceeds profile");
      out.rings.push_back(path(true));
    } while (take(','));
    need(')');
    return out;
  }
  Node value(unsigned depth = 0) {
    limits.node(depth);
    Node out(resource);
    out.type = type();
    if (out.type == 1) {
      need('(');
      out.points.push_back(point());
      need(')');
    } else if (out.type == 2)
      out.points = std::move(path(false).points);
    else if (out.type == 3)
      out.polygons.push_back(polygon());
    else {
      need('(');
      if (out.type == 4) {
        whitespace();
        const bool wrapped = at < bytes.size && bytes.data[at] == '(';
        do {
          if (wrapped)
            need('(');
          out.points.push_back(point());
          if (wrapped)
            need(')');
        } while (take(','));
      } else if (out.type == 6) {
        do {
          valid(out.polygons.size() < 100000,
                "MultiPolygon part count exceeds profile");
          out.polygons.push_back(polygon());
        } while (take(','));
      } else {
        do {
          valid(out.children.size() < 100000,
                "Geometry collection member count exceeds profile");
          if (out.type == 5) {
            limits.node(depth + 1);
            Node line(resource);
            line.type = 2;
            line.points = std::move(path(false).points);
            out.children.push_back(std::move(line));
          } else
            out.children.push_back(value(depth + 1));
        } while (take(','));
      }
      need(')');
    }
    return out;
  }
};
struct GeometrySource {
  static constexpr const char *id = "org.qupathxx.wkt-wkb";
  static constexpr const char *format = "wkt-wkb";
  static constexpr const char *name = "WKT and WKB geometry";
  static constexpr const char *contract = "image-pixel-wkt-wkb/1";
  static constexpr const char *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
  static void validate_options(qpx_reader_string_v1 options) {
    require(!options.size, "WKT/WKB accepts empty options only");
  }
  static void decode(Handle &h, Bytes input,
                     const qpx_reader_open_v1 &request) {
    auto *resource = &h.arena.memory;
    Node root(resource);
    std::string_view encoding;
    std::size_t at = 0;
    bool bom = false;
    std::string_view prefix = "none";
    if (input.data[0] <= 1) {
      Wkb reader{input, 0, resource, {0, 0, request.cancel}};
      root = reader.value();
      valid(reader.at == input.size, "Trailing WKB bytes");
      encoding = "wkb";
    } else {
      if (input.size >= 3 && input.data[0] == 0xef && input.data[1] == 0xbb &&
          input.data[2] == 0xbf) {
        at = 3;
        bom = true;
      }
      while (at < input.size && space(input.data[at])) {
        if ((at & 1023) == 0)
          poll(request.cancel);
        ++at;
      }
      valid(at < input.size, "Geometry source is empty");
      auto first = input.data[at];
      if (first >= 'a' && first <= 'z')
        first -= 'a' - 'A';
      if (first == 'P' || first == 'L' || first == 'M' || first == 'G') {
        Wkt reader{input, at, resource, {0, 0, request.cancel}};
        root = reader.value();
        reader.whitespace();
        valid(reader.at == input.size, "Trailing WKT text");
        encoding = "wkt";
      } else {
        encoding = "hex-wkb";
        if (input.size - at >= 2 &&
            ((input.data[at] == '0' &&
              (input.data[at + 1] == 'x' || input.data[at + 1] == 'X')) ||
             (input.data[at] == '\\' && input.data[at + 1] == 'x'))) {
          prefix = input.sub(at, 2).view();
          at += 2;
        }
        std::pmr::vector<std::uint8_t> decoded(resource);
        int high = -1;
        for (; at < input.size; ++at) {
          if ((at & 1023) == 0)
            poll(request.cancel);
          const auto c = input.data[at];
          if (space(c))
            continue;
          const int n = c >= '0' && c <= '9'   ? c - '0'
                        : c >= 'A' && c <= 'F' ? c - 'A' + 10
                        : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                               : -1;
          valid(n >= 0, "Hex WKB contains a non-hexadecimal character");
          if (high < 0)
            high = n;
          else {
            decoded.push_back(std::uint8_t(high * 16 + n));
            high = -1;
          }
        }
        valid(high < 0 && !decoded.empty(),
              "Hex WKB has an empty or incomplete byte");
        Wkb reader{{decoded.data(), decoded.size()},
                   0,
                   resource,
                   {0, 0, request.cancel}};
        root = reader.value();
        valid(reader.at == decoded.size(), "Trailing hex WKB bytes");
      }
    }
    Facts common(resource);
    fact(common, "geometry.source_encoding", encoding);
    fact(common, "geometry.root_type", names[root.type]);
    fact(common, "geometry.text_bom", bom ? "utf8" : "none");
    fact(common, "geometry.hex_prefix", prefix);
    fact(common, "geometry.coordinates", "literal-image-pixel-xy");
    fact(common, "geometry.dimensions", "2");
    const auto display =
        std::filesystem::u8path(
            std::string_view(request.path.data, std::size_t(request.path.size)))
            .stem()
            .u8string();
    emit(h, root, "", common, display, request.cancel);
  }
};
} // namespace
extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi,
                               qpx_annotation_format_api_v3 *out,
                               qpx_reader_text_v1 *error) {
  return Plugin<GeometrySource>::entry(abi, out, error);
}
