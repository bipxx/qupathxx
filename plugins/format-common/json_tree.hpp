// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "reader_input.hpp"
#include <charconv>
#include <limits>
namespace qpx_native_json {
using namespace qpx_binary;
enum class Kind { Object, Array, String, Number, Boolean, Null };
struct Node {
  Kind kind;
  std::pmr::string key, value;
  std::pmr::vector<std::size_t> children;
  std::string_view raw;
  Node(Kind k, std::pmr::memory_resource *r)
      : kind(k), key(r), value(r), children(r) {}
};
// A strict, non-recursive-output JSON value tree. All text, nodes and child
// indices use the caller's fixed arena. Input and lexical spans are borrowed:
// caller-owned input must remain alive and unchanged while either is used.
struct Tree {
  std::pmr::vector<Node> nodes;
  std::string_view input;
  std::size_t at = 0;
  qpx_reader_cancel_v1 cancel;
  std::size_t object_member_limit, node_limit;
  unsigned depth_limit;
  // Defaults preserve the existing annotation JSON profile. Reader families
  // may explicitly select tighter node bounds or wider object dictionaries.
  explicit Tree(std::string_view s, std::pmr::memory_resource *r,
                qpx_reader_cancel_v1 c, std::size_t max_object_members = 64,
                std::size_t max_nodes = 1000000, unsigned max_depth = 32)
      : nodes(r), input(s), cancel(c), object_member_limit(max_object_members),
        node_limit(max_nodes), depth_limit(max_depth) {
    utf8(s, c, true);
    parse(0);
    space();
    valid(at == input.size(), "Trailing JSON input");
  }
  void space() {
    while (at < input.size() && (input[at] == ' ' || input[at] == '\t' ||
                                 input[at] == '\n' || input[at] == '\r')) {
      if (!(at & 4095))
        poll(cancel);
      ++at;
    }
  }
  char take() {
    valid(at < input.size(), "Truncated JSON input");
    return input[at++];
  }
  unsigned hex4() {
    unsigned n = 0;
    for (unsigned i = 0; i < 4; ++i) {
      const char c = take();
      const int v = c >= '0' && c <= '9'   ? c - '0'
                    : c >= 'a' && c <= 'f' ? c - 'a' + 10
                    : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                           : -1;
      valid(v >= 0, "Invalid JSON Unicode escape");
      n = n * 16 + unsigned(v);
    }
    return n;
  }
  std::pmr::string string() {
    valid(take() == '"', "Expected JSON string");
    std::pmr::string s(nodes.get_allocator().resource());
    for (;;) {
      if (!(at & 1023))
        poll(cancel);
      const auto c = std::uint8_t(take());
      if (c == '"')
        break;
      valid(c >= 32, "Unescaped JSON control character");
      if (c != '\\')
        s.push_back(char(c));
      else {
        switch (take()) {
        case '"':
          s.push_back('"');
          break;
        case '\\':
          s.push_back('\\');
          break;
        case '/':
          s.push_back('/');
          break;
        case 'b':
          s.push_back('\b');
          break;
        case 'f':
          s.push_back('\f');
          break;
        case 'n':
          s.push_back('\n');
          break;
        case 'r':
          s.push_back('\r');
          break;
        case 't':
          s.push_back('\t');
          break;
        case 'u': {
          auto code = hex4();
          if (code >= 0xd800 && code <= 0xdbff) {
            valid(take() == '\\' && take() == 'u',
                  "Missing JSON low surrogate");
            const auto low = hex4();
            valid(low >= 0xdc00 && low <= 0xdfff, "Invalid JSON low surrogate");
            code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
          }
          append_utf8(s, code);
          break;
        }
        default:
          fail("Invalid JSON escape");
        }
      }
      valid(s.size() <= 1024u * 1024, "JSON string exceeds native bound");
    }
    return s;
  }
  std::size_t parse(unsigned depth) {
    poll(cancel);
    space();
    valid(depth <= depth_limit && nodes.size() < node_limit,
          "JSON nesting/node limit exceeded");
    const auto begin = at, id = nodes.size();
    const char c = take();
    const Kind kind = c == '{'                 ? Kind::Object
                      : c == '['               ? Kind::Array
                      : c == '"'               ? Kind::String
                      : (c == 't' || c == 'f') ? Kind::Boolean
                      : c == 'n'               ? Kind::Null
                                               : Kind::Number;
    nodes.emplace_back(kind, nodes.get_allocator().resource());
    if (kind == Kind::Object || kind == Kind::Array) {
      const char end = kind == Kind::Object ? '}' : ']';
      space();
      if (at < input.size() && input[at] == end)
        ++at;
      else
        for (;;) {
          std::pmr::string key(nodes.get_allocator().resource());
          if (kind == Kind::Object) {
            valid(nodes[id].children.size() < object_member_limit,
                  "JSON object member limit exceeded");
            space();
            key = string();
            utf8(key, cancel);
            valid(key.size() <= 256, "JSON object key exceeds native bound");
            for (const auto n : nodes[id].children)
              valid(nodes[n].key != key, "Duplicate JSON object key");
            space();
            valid(take() == ':', "Missing JSON member colon");
          }
          const auto child = parse(depth + 1);
          nodes[child].key = std::move(key);
          nodes[id].children.push_back(child);
          space();
          const auto next = take();
          if (next == end)
            break;
          valid(next == ',', "Missing JSON value separator");
        }
    } else if (kind == Kind::String) {
      --at;
      nodes[id].value = string();
    } else if (kind == Kind::Boolean || kind == Kind::Null) {
      const std::string_view literal = c == 't'   ? "true"
                                       : c == 'f' ? "false"
                                                  : "null";
      valid(input.substr(begin, literal.size()) == literal,
            "Invalid JSON literal");
      at = begin + literal.size();
    } else {
      at = begin;
      if (at < input.size() && input[at] == '-')
        ++at;
      valid(at < input.size() && input[at] >= '0' && input[at] <= '9',
            "Invalid JSON number");
      if (input[at] == '0')
        ++at;
      else
        while (at < input.size() && input[at] >= '0' && input[at] <= '9') {
          if (!(at & 4095))
            poll(cancel);
          ++at;
          valid(at - begin <= 1024, "JSON number exceeds lexical bound");
        }
      if (at < input.size() && input[at] == '.') {
        const auto first = ++at;
        while (at < input.size() && input[at] >= '0' && input[at] <= '9') {
          if (!(at & 4095))
            poll(cancel);
          ++at;
          valid(at - begin <= 1024, "JSON number exceeds lexical bound");
        }
        valid(at != first, "Missing JSON fraction");
      }
      if (at < input.size() && (input[at] == 'e' || input[at] == 'E')) {
        ++at;
        if (at < input.size() && (input[at] == '+' || input[at] == '-'))
          ++at;
        const auto first = at;
        while (at < input.size() && input[at] >= '0' && input[at] <= '9') {
          if (!(at & 4095))
            poll(cancel);
          ++at;
          valid(at - begin <= 1024, "JSON number exceeds lexical bound");
        }
        valid(at != first, "Missing JSON exponent");
      }
    }
    nodes[id].raw = input.substr(begin, at - begin);
    return id;
  }
  const Node &root() const { return nodes[0]; }
  const Node *find(const Node &n, std::string_view key) const {
    valid(n.kind == Kind::Object, "Expected JSON object");
    for (const auto i : n.children)
      if (nodes[i].key == key)
        return &nodes[i];
    return nullptr;
  }
  const Node &get(const Node &n, std::string_view key) const {
    const auto *p = find(n, key);
    valid(p != nullptr, "Missing required JSON member");
    return *p;
  }
  void keys(const Node &n,
            std::initializer_list<std::string_view> admitted) const {
    valid(n.kind == Kind::Object, "Expected JSON object");
    for (const auto i : n.children)
      valid(std::find(admitted.begin(), admitted.end(),
                      std::string_view(nodes[i].key)) != admitted.end(),
            "Unsupported JSON member/schema extension");
  }
  const auto &array(const Node &n) const {
    valid(n.kind == Kind::Array, "Expected JSON array");
    return n.children;
  }
};
inline std::uint64_t integer(const Node &n) {
  valid(n.kind == Kind::Number && !n.raw.empty(),
        "Expected exact JSON unsigned integer");
  std::uint64_t v = 0;
  const auto r = std::from_chars(n.raw.data(), n.raw.data() + n.raw.size(), v);
  valid(r.ec == std::errc{} && r.ptr == n.raw.data() + n.raw.size(),
        "Expected exact JSON unsigned integer");
  return v;
}
inline double real(const Node &n) {
  valid(n.kind == Kind::Number, "Expected JSON number");
  double v = 0;
  const auto r = std::from_chars(n.raw.data(), n.raw.data() + n.raw.size(), v,
                                 std::chars_format::general);
  valid(r.ec == std::errc{} && r.ptr == n.raw.data() + n.raw.size() &&
            std::isfinite(v),
        "JSON number is outside finite binary64");
  return v;
}
inline std::string_view string(const Node &n) {
  valid(n.kind == Kind::String, "Expected JSON string");
  return n.value;
}
} // namespace qpx_native_json
