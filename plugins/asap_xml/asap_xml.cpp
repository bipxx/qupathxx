// SPDX-License-Identifier: AGPL-3.0-only
// Independent native implementation of the openly published ASAP XML encoding.
// Upstream GPL-2.0-or-later code is a development reference, never
// linked/copied as a runtime implementation. Component terms are in
// reference-lock.json.
#include "../format-common/annotation_xml.hpp"
#include "format_plugin_profile.hpp"
#include <limits>
#include <map>
#include <xmmintrin.h>
namespace qpx_asap {
using namespace qpx_annotation_xml;
std::string_view required(const Node &n, std::string_view key) {
  valid(n.attribute(key) != nullptr, "Missing ASAP XML attribute");
  return n.get(key);
}
void whitespace(const Node &n) {
  valid(trim(n.text).empty(), "Unexpected ASAP XML text");
}
void attributes(const Node &n, std::string_view allowed) {
  for (const auto &[key, value] : n.attrs) {
    std::string word = "|";
    word.append(key);
    word.push_back('|');
    valid(allowed.find(word) != std::string_view::npos,
          "Unknown/namespaced ASAP structural attribute");
  }
}
std::uint64_t order(std::string_view s) {
  s = trim(s);
  if (!s.empty() && s.front() == '+')
    s.remove_prefix(1);
  std::uint64_t n = 0;
  const auto p = std::from_chars(s.data(), s.data() + s.size(), n);
  valid(!s.empty() && p.ec == std::errc{} && p.ptr == s.data() + s.size(),
        "Invalid ASAP Coordinate Order");
  return n;
}
double coordinate(std::string_view text) {
  static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                "ASAP source coordinates require IEEE binary32");
  const auto x = decimal64(text);
  valid(x >= 0 && x <= std::numeric_limits<float>::max(),
        "ASAP coordinate exceeds finite nonnegative binary32 range");
  // Match the upstream double XML parser -> Point(float) authority. The caller
  // environment is checked before work, never changed; preserve exact f32 bits.
  volatile float rounded = static_cast<float>(x);
  valid(std::isfinite(rounded) && (x == 0 || rounded != 0),
        "ASAP nonzero coordinate underflows binary32");
  return rounded;
}
std::uint32_t color(const Node &n) {
  const auto text = n.get("Color");
  if (text.empty())
    return 0xf4fa58; // Upstream AnnotationBase default, including groups.
  valid(text.size() == 7 && text[0] == '#',
        "ASAP color requires canonical #RRGGBB");
  std::uint32_t value = 0;
  for (char c : text.substr(1)) {
    const unsigned digit = c >= '0' && c <= '9'   ? unsigned(c - '0')
                           : c >= 'A' && c <= 'F' ? unsigned(c - 'A' + 10)
                           : c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10)
                                                  : 16u;
    valid(digit < 16, "Invalid ASAP hex color");
    value = 16 * value + digit;
  }
  return value;
}
struct Point {
  double x, y;
};
struct Group {
  std::size_t node, parent = SIZE_MAX, root = SIZE_MAX, depth = 0;
  std::uint32_t rgb = 0;
};
struct Decoder {
  Handle &h;
  Xml &xml;
  qpx_reader_cancel_v1 cancel;
  std::pmr::memory_resource *r;
  std::pmr::vector<Group> groups;
  std::pmr::map<std::string_view, std::size_t, std::less<>> group_ids;
  std::size_t annotations = SIZE_MAX, group_container = SIZE_MAX;
  Decoder(Handle &handle, Xml &input, qpx_reader_cancel_v1 c)
      : h(handle), xml(input), cancel(c), r(&handle.arena.memory), groups(r),
        group_ids(r) {}
  const Node &node(std::size_t i) const { return xml.nodes[i]; }
  void group_tree() {
    const auto &root = node(0);
    valid(root.name == "ASAP_Annotations",
          "Expected unnamespaced ASAP_Annotations root");
    attributes(root, "|");
    whitespace(root);
    for (auto child : root.children) {
      const auto &n = node(child);
      if (n.name == "Annotations") {
        valid(annotations == SIZE_MAX, "Repeated ASAP Annotations");
        annotations = child;
      } else if (n.name == "AnnotationGroups") {
        valid(group_container == SIZE_MAX, "Repeated ASAP AnnotationGroups");
        group_container = child;
      } else
        fail("Unknown/namespaced ASAP root child");
    }
    valid(annotations != SIZE_MAX && !node(annotations).children.empty(),
          "ASAP XML has no annotations");
    attributes(node(annotations), "|");
    whitespace(node(annotations));
    if (group_container != SIZE_MAX) {
      attributes(node(group_container), "|");
      whitespace(node(group_container));
      for (auto child : node(group_container).children) {
        poll(cancel);
        const auto &n = node(child);
        valid(n.name == "Group" && groups.size() < 4096,
              "Unknown ASAP group or group count exceeds4096");
        attributes(n, "|Name|PartOfGroup|Color|");
        whitespace(n);
        const auto name = required(n, "Name");
        valid(!name.empty() && name.size() <= 256 && name != "None",
              "ASAP group requires a nonempty, nonreserved name <=256 bytes");
        utf8(name, cancel);
        (void)required(n, "PartOfGroup");
        valid(group_ids.emplace(name, groups.size()).second,
              "Duplicate ASAP group name");
        groups.push_back({child, SIZE_MAX, SIZE_MAX, 0, color(n)});
        bool seen = false;
        for (auto item : n.children) {
          const auto &a = node(item);
          valid(!seen && a.name == "Attributes",
                "Unknown/repeated ASAP group attributes");
          seen = true;
          attributes(a, "|");
          whitespace(a);
          for (auto entry : a.children) {
            const auto &v = node(entry);
            valid(v.name == "Attribute" && v.children.empty(),
                  "Unsupported ASAP group attribute structure");
            whitespace(v);
          }
        }
      }
    }
    for (auto &group : groups) {
      const auto parent = node(group.node).get("PartOfGroup");
      if (parent != "None") {
        const auto found = group_ids.find(parent);
        valid(found != group_ids.end(), "Unresolved ASAP parent group");
        group.parent = found->second;
      }
    }
    for (std::size_t i = 0; i < groups.size(); ++i) {
      auto current = i;
      std::size_t depth = 0;
      for (;;) {
        poll(cancel);
        valid(++depth <= 64, "ASAP group cycle/depth exceeds64");
        if (groups[current].parent == SIZE_MAX)
          break;
        current = groups[current].parent;
      }
      groups[i].root = current;
      groups[i].depth = depth;
    }
  }
  void geometry(Row &row, const Node &annotation) {
    std::size_t coordinate_node = SIZE_MAX;
    for (auto child : annotation.children) {
      valid(coordinate_node == SIZE_MAX && node(child).name == "Coordinates",
            "Unknown/repeated ASAP annotation child");
      coordinate_node = child;
    }
    valid(coordinate_node != SIZE_MAX, "ASAP annotation has no Coordinates");
    const auto &coordinates = node(coordinate_node);
    attributes(coordinates, "|");
    whitespace(coordinates);
    const auto count = coordinates.children.size();
    valid(count && count <= 65000, "ASAP coordinate count exceeds profile");
    std::pmr::vector<Point> points(count, Point{}, r);
    std::pmr::vector<std::uint8_t> seen(count, 0, r);
    for (auto child : coordinates.children) {
      poll(cancel);
      const auto &n = node(child);
      valid(n.name == "Coordinate" && n.children.empty(),
            "Unknown/non-value ASAP Coordinate");
      attributes(n, "|Order|X|Y|");
      whitespace(n);
      const auto index = order(required(n, "Order"));
      valid(index < count && !seen[std::size_t(index)],
            "ASAP Coordinate Order must be an exact contiguous permutation");
      seen[std::size_t(index)] = 1;
      points[std::size_t(index)] = {coordinate(required(n, "X")),
                                    coordinate(required(n, "Y"))};
    }
    const auto type = required(annotation, "Type");
    GeometryEncoder e{row.geometry};
    if (type == "Dot" || type == "PointSet" || type == "Measurement" ||
        type == "Polygon") {
      valid((type != "Dot" || count == 1) &&
                (type != "Measurement" || count == 2) &&
                (type != "Polygon" || count >= 3),
            "ASAP primitive coordinate count is not representable");
      if (type == "Polygon") {
        e.header(2);
        e.u32(1);
      } else
        e.header(type == "Measurement" ? 4 : 5);
      e.u32(std::uint32_t(count));
      for (const auto &p : points) {
        e.real(p.x);
        e.real(p.y);
      }
      if (type == "Polygon")
        e.u32(0);
    } else if (type == "Rectangle") {
      valid(count == 4,
            "Canonical ASAP Rectangle requires four ordered corners");
      double left = points[0].x, right = left, top = points[0].y, bottom = top;
      for (const auto &p : points) {
        left = std::min(left, p.x);
        right = std::max(right, p.x);
        top = std::min(top, p.y);
        bottom = std::max(bottom, p.y);
      }
      valid(left < right && top < bottom, "Degenerate ASAP rectangle");
      unsigned corners = 0;
      for (std::size_t i = 0; i < 4; ++i) {
        const auto &p = points[i];
        const auto &q = points[(i + 1) % 4];
        valid((p.x == left || p.x == right) && (p.y == top || p.y == bottom) &&
                  ((p.x == q.x) != (p.y == q.y)),
              "ASAP Rectangle is not an axis-aligned four-corner path");
        const unsigned bit =
            1u << ((p.x == right ? 1 : 0) + (p.y == bottom ? 2 : 0));
        valid(!(corners & bit), "Repeated ASAP rectangle corner");
        corners |= bit;
      }
      e.header(0);
      for (auto v : {left, top, right - left, bottom - top})
        e.real(v);
    } else
      fail("Unsupported ASAP type (including Spline/None); no curve flattening "
           "or silent shape drop");
    valid(row.geometry.size() <= 1024 * 1024,
          "ASAP geometry exceeds one-MiB page");
    number(row.facts, "asap.coordinates", count);
  }
  void run() {
    group_tree();
    std::size_t index = 0;
    for (auto child : node(annotations).children) {
      poll(cancel);
      const auto &n = node(child);
      valid(n.name == "Annotation",
            "Unknown/namespaced ASAP annotation element");
      attributes(n, "|Name|Type|PartOfGroup|Color|");
      whitespace(n);
      const auto name = required(n, "Name");
      valid(name.size() <= 4096, "ASAP display name exceeds4096 bytes");
      h.rows.emplace_back(r);
      auto &row = h.rows.back();
      row.id = "annotation/" + std::to_string(index);
      row.name = name;
      const auto member = required(n, "PartOfGroup");
      auto group = SIZE_MAX;
      const auto own_rgb = color(n);
      row.color = own_rgb;
      row.flags = QPX_ANNOTATION_OBJECT_COLOR_RGB;
      if (member != "None") {
        const auto found = group_ids.find(member);
        valid(found != group_ids.end(), "Unresolved ASAP annotation group");
        group = found->second;
        row.classification = member;
        row.color = groups[groups[group].root].rgb;
      }
      geometry(row, n);
      fact(row.facts, "asap.type", n.get("Type"));
      fact(row.facts, "asap.name", name);
      number(row.facts, "asap.index", index++);
      fact(row.facts, "asap.coordinate_profile",
           "level-zero-image-pixels/asap-double-to-binary32/1");
      fact(row.facts, "asap.source_planes", "all/unpositioned");
      fact(row.facts, "asap.group", member);
      number(row.facts, "asap.groups", groups.size());
      number(row.facts, "asap.annotation_color_rgb", own_rgb);
      number(row.facts, "asap.effective_color_rgb", row.color);
      fact(row.facts, "asap.root_group",
           group == SIZE_MAX
               ? std::string_view{}
               : node(groups[groups[group].root].node).get("Name"));
      number(row.facts, "asap.group_depth",
             group == SIZE_MAX ? 0 : groups[group].depth);
      xml.attribute_facts(row.facts, "asap.root.attributes", node(0));
      xml.tree_facts(row.facts, "asap.annotation.tree", child);
      if (group_container != SIZE_MAX)
        xml.tree_facts(row.facts, "asap.groups.tree", group_container);
    }
  }
};
struct Definition {
  static constexpr const char *id = "org.qupathxx.asap-xml",
                              *format = "asap-xml",
                              *name = "ASAP XML annotations",
                              *contract = "asap-level-zero-xml/1",
                              *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
  static void validate_options(qpx_reader_string_v1 v) {
    require(!v.size, "ASAP XML accepts empty provider options only; use native "
                     "frame/class/plane options");
  }
  static void decode(Handle &h, Bytes input,
                     const qpx_reader_open_v1 &request) {
    require((_mm_getcsr() & 0xe040u) == 0 &&
                (_mm_getcsr() & _MM_MASK_MASK) == _MM_MASK_MASK,
            "ASAP coordinates require nearest rounding, preserved subnormals "
            "and masked SSE exceptions");
    Xml xml(&h.arena.memory, request.cancel);
    xml.parse(input);
    Decoder decoder(h, xml, request.cancel);
    decoder.run();
  }
};
} // namespace qpx_asap
extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi,
                               qpx_annotation_format_api_v3 *out,
                               qpx_reader_text_v1 *error) {
  return qpx_annotation_source::Plugin<qpx_asap::Definition>::entry(abi, out,
                                                                    error);
}
