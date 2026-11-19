// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "annotation_source.hpp"
#include <charconv>
#include <cstddef>
#include <exception>
#include <expat.h>
namespace qpx_annotation_xml {
using namespace qpx_annotation_source;
inline std::string_view trim(std::string_view s) {
  const auto white = [](char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  };
  while (!s.empty() && white(s.front()))
    s.remove_prefix(1);
  while (!s.empty() && white(s.back()))
    s.remove_suffix(1);
  return s;
}
inline double decimal64(std::string_view s) {
  s = trim(s);
  valid(!s.empty(), "Missing annotation XML number");
  if (s.front() == '+') {
    s.remove_prefix(1);
    valid(!s.empty() && s.front() != '-' && s.front() != '+',
          "Invalid annotation XML numeric sign");
  }
  for (char c : s)
    valid((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' ||
              c == 'e' || c == 'E',
          "Unsupported annotation XML numeric syntax");
  double x = 0;
  const auto parsed = std::from_chars(s.data(), s.data() + s.size(), x,
                                      std::chars_format::general);
  valid(!s.empty() && parsed.ec == std::errc{} &&
            parsed.ptr == s.data() + s.size() && std::isfinite(x),
        "Nonfinite, out-of-range or malformed annotation XML number");
  return x;
}
inline constexpr char separator = '\x1f';
inline std::string_view local(std::string_view name) {
  const auto at = name.find(separator);
  return at == std::string_view::npos ? name : name.substr(at + 1);
}
using Attrs = std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>>;
struct Content {
  std::size_t child, offset, length;
};
struct Node {
  std::pmr::string name, text;
  Attrs attrs, namespaces;
  std::pmr::vector<std::size_t> children;
  std::pmr::vector<Content> content;
  std::size_t parent;
  Node(std::pmr::memory_resource *r, const char *n, std::size_t p)
      : name(n, r), text(r), attrs(r), namespaces(r), children(r), content(r),
        parent(p) {}
  const std::pmr::string *attribute(std::string_view key) const {
    for (const auto &[k, v] : attrs)
      if (k == key)
        return &v;
    return nullptr;
  }
  std::string_view get(std::string_view key) const {
    const auto *v = attribute(key);
    return v ? std::string_view(*v) : std::string_view{};
  }
};
// Expat's C allocator has no context parameter. A private, scoped TLS resource
// covers the parser's entire synchronous lifetime, including ParserFree. No
// allocation/exception crosses the DLL's C ABI or an Expat callback.
inline thread_local std::pmr::memory_resource *xml_resource = nullptr;
struct alignas(std::max_align_t) Allocation {
  std::size_t size;
};
inline void *xml_malloc(std::size_t n) noexcept {
  try {
    if (!xml_resource || n > SIZE_MAX - sizeof(Allocation))
      return nullptr;
    auto *p = static_cast<Allocation *>(
        xml_resource->allocate(sizeof(Allocation) + n, alignof(Allocation)));
    p->size = n;
    return p + 1;
  } catch (...) {
    return nullptr;
  }
}
inline void xml_free(void *) noexcept {} // The fixed arena owns every block.
inline void *xml_realloc(void *old, std::size_t n) noexcept {
  if (!old)
    return xml_malloc(n);
  if (!n)
    return nullptr;
  auto *p = xml_malloc(n);
  if (p)
    std::memcpy(p, old,
                std::min(n, (static_cast<Allocation *>(old) - 1)->size));
  return p;
}
struct ResourceScope {
  std::pmr::memory_resource *previous;
  explicit ResourceScope(std::pmr::memory_resource *r)
      : previous(xml_resource) {
    xml_resource = r;
  }
  ~ResourceScope() { xml_resource = previous; }
};
struct Xml {
  std::pmr::memory_resource *r;
  qpx_reader_cancel_v1 cancel;
  std::pmr::vector<Node> nodes;
  std::pmr::vector<std::size_t> stack;
  Attrs pending_namespaces;
  std::exception_ptr failure;
  XML_Parser parser = nullptr;
  std::size_t characters = 0;
  Xml(std::pmr::memory_resource *resource, qpx_reader_cancel_v1 c)
      : r(resource), cancel(c), nodes(r), stack(r), pending_namespaces(r) {}
  template <class F> void callback(F &&call) noexcept {
    if (failure)
      return;
    try {
      poll(cancel);
      call();
    } catch (...) {
      failure = std::current_exception();
      XML_StopParser(parser, XML_FALSE);
    }
  }
  static void XMLCALL namespace_start(void *ctx, const XML_Char *prefix,
                                      const XML_Char *uri) noexcept {
    auto &x = *static_cast<Xml *>(ctx);
    x.callback([&] {
      valid(x.pending_namespaces.size() < 64 &&
                (!prefix || std::strlen(prefix) <= 256) &&
                (!uri || std::strlen(uri) <= 4096),
            "Annotation namespace binding limit exceeded");
      x.pending_namespaces.emplace_back(
          std::pmr::string(prefix ? prefix : "", x.r),
          std::pmr::string(uri ? uri : "", x.r));
    });
  }
  static void XMLCALL start(void *ctx, const XML_Char *name,
                            const XML_Char **attrs) noexcept {
    auto &x = *static_cast<Xml *>(ctx);
    x.callback([&] {
      valid(x.stack.size() < 64 && x.nodes.size() < 100000 &&
                std::strlen(name) <= 1024,
            "Annotation XML depth/node/name limit exceeded");
      const auto index = x.nodes.size(),
                 parent = x.stack.empty() ? SIZE_MAX : x.stack.back();
      x.nodes.emplace_back(x.r, name, parent);
      if (parent != SIZE_MAX) {
        x.nodes[parent].children.push_back(index);
        x.nodes[parent].content.push_back({index, 0, 0});
      }
      auto &n = x.nodes.back();
      // Preserve inherited prefix bindings too: QName-valued metadata can refer
      // to a declaration outside the retained XMLAnnotation subtree.
      if (parent != SIZE_MAX)
        for (const auto &[prefix, uri] : x.nodes[parent].namespaces)
          n.namespaces.emplace_back(std::pmr::string(prefix, x.r),
                                    std::pmr::string(uri, x.r));
      for (const auto &[prefix, uri] : x.pending_namespaces) {
        const auto found = std::find_if(
            n.namespaces.begin(), n.namespaces.end(),
            [&](const auto &value) { return value.first == prefix; });
        if (found != n.namespaces.end())
          found->second = uri;
        else {
          valid(n.namespaces.size() < 64,
                "Annotation in-scope namespace limit exceeded");
          n.namespaces.emplace_back(std::pmr::string(prefix, x.r),
                                    std::pmr::string(uri, x.r));
        }
      }
      x.pending_namespaces.clear();
      std::sort(n.namespaces.begin(), n.namespaces.end(),
                [](const auto &a, const auto &b) { return a.first < b.first; });
      for (std::size_t i = 0; attrs[i]; i += 2) {
        poll(x.cancel);
        valid(n.attrs.size() < 64 && std::strlen(attrs[i]) <= 1024 &&
                  std::strlen(attrs[i + 1]) <= 1024 * 1024,
              "Annotation XML attribute limit exceeded");
        n.attrs.emplace_back(std::pmr::string(attrs[i], x.r),
                             std::pmr::string(attrs[i + 1], x.r));
      }
      x.stack.push_back(index);
    });
  }
  static void XMLCALL end(void *ctx, const XML_Char *) noexcept {
    auto &x = *static_cast<Xml *>(ctx);
    x.callback([&] {
      valid(!x.stack.empty(), "Annotation XML stack mismatch");
      x.stack.pop_back();
    });
  }
  static void XMLCALL chars(void *ctx, const XML_Char *data,
                            int length) noexcept {
    auto &x = *static_cast<Xml *>(ctx);
    x.callback([&] {
      valid(length >= 0 &&
                std::size_t(length) <= 8u * 1024 * 1024 - x.characters,
            "Annotation XML text limit exceeded");
      x.characters += std::size_t(length);
      if (!x.stack.empty() && length) {
        auto &node = x.nodes[x.stack.back()];
        if (node.content.empty() || node.content.back().child != SIZE_MAX)
          node.content.push_back({SIZE_MAX, node.text.size(), 0});
        node.content.back().length += std::size_t(length);
        node.text.append(data, std::size_t(length));
      }
    });
  }
  static void XMLCALL doctype(void *ctx, const XML_Char *, const XML_Char *,
                              const XML_Char *, int) noexcept {
    auto &x = *static_cast<Xml *>(ctx);
    x.callback([] {
      fail("Annotation XML DTD/entity declarations are not permitted");
    });
  }
  static int XMLCALL external(XML_Parser p, const XML_Char *, const XML_Char *,
                              const XML_Char *, const XML_Char *) noexcept {
    auto &x = *static_cast<Xml *>(XML_GetUserData(p));
    x.callback(
        [] { fail("Annotation XML external entities are not permitted"); });
    return XML_STATUS_ERROR;
  }
  static void XMLCALL declaration(void *ctx, const XML_Char *version,
                                  const XML_Char *encoding, int) noexcept {
    auto &x = *static_cast<Xml *>(ctx);
    x.callback([&] {
      valid(version && std::string_view(version) == "1.0",
            "Annotation ROI requires XML 1.0");
      if (encoding) {
        char name[16]{};
        const auto len = std::strlen(encoding);
        valid(len < sizeof(name), "Annotation XML encoding is unsupported");
        for (std::size_t i = 0; i < len; ++i)
          name[i] = encoding[i] >= 'a' && encoding[i] <= 'z'
                        ? char(encoding[i] - 32)
                        : encoding[i];
        valid(std::string_view(name) == "UTF-8" ||
                  std::string_view(name) == "UTF-16" ||
                  std::string_view(name) == "UTF-16LE" ||
                  std::string_view(name) == "UTF-16BE",
              "Annotation ROI XML must use UTF-8 or UTF-16");
      }
    });
  }
  static void XMLCALL processing_instruction(void *ctx, const XML_Char *,
                                             const XML_Char *) noexcept {
    auto &x = *static_cast<Xml *>(ctx);
    x.callback([] { fail("Annotation XML processing instructions are not permitted"); });
  }
  void parse(Bytes source, bool reject_processing_instructions = false) {
    ResourceScope scope(r);
    XML_Memory_Handling_Suite memory{xml_malloc, xml_realloc, xml_free};
    const XML_Char sep[] = {separator, 0};
    parser = XML_ParserCreate_MM(nullptr, &memory, sep);
    valid(parser != nullptr, "Annotation XML parser work budget exhausted");
    struct Destroy {
      XML_Parser p;
      ~Destroy() { XML_ParserFree(p); }
    } destroy{parser};
    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, start, end);
    XML_SetNamespaceDeclHandler(parser, namespace_start, nullptr);
    XML_SetCharacterDataHandler(parser, chars);
    if (reject_processing_instructions)
      XML_SetProcessingInstructionHandler(parser, processing_instruction);
    XML_SetStartDoctypeDeclHandler(parser, doctype);
    XML_SetExternalEntityRefHandler(parser, external);
    XML_SetXmlDeclHandler(parser, declaration);
    XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_NEVER);
    for (std::size_t at = 0; at < source.size;) {
      poll(cancel);
      const auto n = std::min<std::size_t>(4096, source.size - at);
      const auto status =
          XML_Parse(parser, reinterpret_cast<const char *>(source.data + at),
                    int(n), at + n == source.size);
      if (failure)
        std::rethrow_exception(failure);
      valid(status == XML_STATUS_OK,
            "Malformed, unsupported or over-budget Annotation XML");
      at += n;
    }
    valid(!nodes.empty() && stack.empty(), "Missing annotation XML root");
  }
  // QOX2 retains namespace contexts and XML metadata: length-prefixed expanded
  // names, attributes (source order), normalized text and child values.
  // Prefixes, comments and lexical whitespace/entities are represented by the
  // source SHA.
  void field(std::pmr::string &out, std::string_view value) const {
    const auto n = std::to_string(value.size());
    out.append(n);
    out.push_back(':');
    out.append(value);
    valid(out.size() <= 256 * 1024,
          "Annotation source metadata subtree exceeds 256 KiB");
  }
  void tree(std::pmr::string &out, std::size_t index) const {
    const auto qname = [&](std::string_view name) {
      const auto at = name.find(separator);
      field(out, at == std::string_view::npos ? std::string_view{}
                                              : name.substr(0, at));
      field(out, local(name));
    };
    poll(cancel);
    const auto &n = nodes[index];
    qname(n.name);
    namespace_fields(out, n);
    field(out, std::to_string(n.attrs.size()));
    for (const auto &[k, v] : n.attrs) {
      qname(k);
      field(out, v);
    }
    field(out, std::to_string(n.content.size()));
    for (const auto &v : n.content) {
      field(out, v.child == SIZE_MAX ? "T" : "E");
      if (v.child == SIZE_MAX)
        field(out, std::string_view(n.text).substr(v.offset, v.length));
      else
        tree(out, v.child);
    }
  }
  void namespace_fields(std::pmr::string &out, const Node &n) const {
    field(out, std::to_string(n.namespaces.size()));
    for (const auto &[prefix, uri] : n.namespaces) {
      field(out, prefix);
      field(out, uri);
    }
  }
  void tree_facts(Facts &facts, std::string_view key, std::size_t index) const {
    std::pmr::string value("QOX2", r);
    tree(value, index);
    chunks(facts, key, value);
  }
  void attribute_facts(Facts &facts, std::string_view key,
                       const Node &n) const {
    std::pmr::string value("QOX2A", r);
    namespace_fields(value, n);
    field(value, std::to_string(n.attrs.size()));
    for (const auto &[k, v] : n.attrs) {
      const std::string_view name(k);
      const auto at = name.find(separator);
      field(value, at == std::string_view::npos ? std::string_view{}
                                                : name.substr(0, at));
      field(value, local(name));
      field(value, v);
    }
    chunks(facts, key, value);
  }
};
} // namespace qpx_annotation_xml
