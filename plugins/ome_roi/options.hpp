// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/binary_input.hpp"
namespace qpx_ome {
using namespace qpx_binary;
// Deliberately small provider-owned JSON contract. No allocation outside the
// caller's arena, no nested values, duplicate keys or coercion. The host still
// owns canonical option/receipt identity.
struct Options {
  std::pmr::string image;
  bool unbound = false;
  explicit Options(std::pmr::memory_resource *r) : image(r) {}
};
struct OptionsReader {
  std::string_view s;
  std::size_t at = 0;
  std::pmr::memory_resource *r;
  void space() {
    while (at < s.size() &&
           std::string_view(" \t\r\n").find(s[at]) != std::string_view::npos)
      ++at;
  }
  bool take(char c) {
    space();
    if (at < s.size() && s[at] == c) {
      ++at;
      return true;
    }
    return false;
  }
  unsigned unit() {
    require(s.size() - at >= 4, "Truncated OME option Unicode escape");
    unsigned v = 0;
    for (unsigned i = 0; i < 4; ++i) {
      const char c = s[at++];
      const auto n = c >= '0' && c <= '9'   ? c - '0'
                     : c >= 'a' && c <= 'f' ? c - 'a' + 10
                     : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                            : 16;
      require(n < 16, "Invalid OME option Unicode escape");
      v = 16 * v + unsigned(n);
    }
    return v;
  }
  std::pmr::string string() {
    require(take('"'), "OME option must be a JSON string");
    std::pmr::string out(r);
    while (at < s.size()) {
      const char c = s[at++];
      if (c == '"') {
        utf8(out, {});
        return out;
      }
      require(static_cast<unsigned char>(c) >= 32,
              "Control in OME option string");
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      require(at < s.size(), "Truncated OME option escape");
      const char e = s[at++];
      if (e == '"' || e == '\\' || e == '/')
        out.push_back(e);
      else if (e == 'u') {
        auto v = unit();
        if (v >= 0xd800 && v <= 0xdbff) {
          require(s.size() - at >= 2 && s.substr(at, 2) == "\\u",
                  "Missing OME option surrogate");
          at += 2;
          const auto tail = unit();
          require(tail >= 0xdc00 && tail <= 0xdfff,
                  "Invalid OME option surrogate");
          v = 0x10000 + ((v - 0xd800) << 10) + tail - 0xdc00;
        }
        append_utf8(out, v);
      } else
        fail("Unsupported OME option escape", QPX_READER_INVALID_REQUEST);
    }
    fail("Unterminated OME option string", QPX_READER_INVALID_REQUEST);
  }
  Options parse() {
    Options out(r);
    if (s.empty())
      return out;
    require(take('{'), "OME options must be an object");
    unsigned seen = 0;
    if (!take('}')) {
      do {
        const auto key = string();
        require(take(':'), "Missing OME option colon");
        if (key == "image_id") {
          require(!(seen & 1), "Duplicate OME image_id option");
          seen |= 1;
          out.image = string();
          require(!out.image.empty() && out.image.size() <= 256,
                  "OME image_id must contain 1 to 256 UTF-8 bytes");
        } else if (key == "unbound_rois") {
          require(!(seen & 2), "Duplicate OME unbound_rois option");
          seen |= 2;
          space();
          if (s.substr(at, 4) == "true") {
            out.unbound = true;
            at += 4;
          } else {
            require(s.substr(at, 5) == "false",
                    "OME unbound_rois must be boolean");
            at += 5;
          }
        } else
          fail("Unknown OME ROI option", QPX_READER_INVALID_REQUEST);
      } while (take(','));
      require(take('}'), "Invalid OME option object");
    }
    space();
    require(at == s.size() && !(out.unbound && !out.image.empty()),
            "Trailing or conflicting OME options");
    return out;
  }
};
} // namespace qpx_ome
