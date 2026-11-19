// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/json_tree.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <qupathxx/reader_plugin.h>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace qpx_ngff {
static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 &&
                  NLOHMANN_JSON_VERSION_MINOR == 12 &&
                  NLOHMANN_JSON_VERSION_PATCH == 0,
              "Use the admitted nlohmann-json3.12.0 header selection");
using Json = nlohmann::json;
namespace fs = std::filesystem;
inline constexpr std::size_t mib = 1024u * 1024;
inline constexpr std::size_t attrs_limit = 24576;
inline constexpr std::size_t description_limit = 65536;
struct Failure {
  qpx_reader_status_v1 status;
  const char *message;
};
[[noreturn]] inline void
fail(const char *message, qpx_reader_status_v1 status = QPX_READER_IO_ERROR) {
  throw Failure{status, message};
}
inline void valid(bool value, const char *message) {
  if (!value)
    fail(message);
}
inline void require(bool value, const char *message) {
  if (!value)
    fail(message, QPX_READER_INVALID_REQUEST);
}
inline bool text(qpx_reader_text_v1 &out, std::string_view value) noexcept {
  out.size = value.size();
  if (out.capacity < value.size() || (!value.empty() && !out.data))
    return false;
  if (!value.empty())
    std::memcpy(out.data, value.data(), value.size());
  return true;
}
template <class F>
qpx_reader_status_v1 boundary(qpx_reader_text_v1 *error, F &&f) noexcept {
  if (error)
    error->size = 0;
  try {
    return f();
  } catch (...) {
    auto status = QPX_READER_INTERNAL_ERROR;
    const char *message = "NGFF label provider failure";
    try {
      throw;
    } catch (const Failure &e) {
      status = e.status;
      message = e.message;
    } catch (const qpx_binary::Failure &e) {
      status = e.status;
      message = e.message;
    } catch (const std::bad_alloc &) {
      status = QPX_READER_INVALID_REQUEST;
      message = "NGFF label work allocation exhausted";
    } catch (const std::filesystem::filesystem_error &) {
      status = QPX_READER_IO_ERROR;
      message = "NGFF label filesystem operation failed";
    } catch (const std::invalid_argument &) {
      status = QPX_READER_INVALID_REQUEST;
      message = "NGFF label/native source contract refused input";
    } catch (const std::length_error &) {
      status = QPX_READER_INVALID_REQUEST;
      message = "NGFF label/native source exceeds work limits";
    } catch (const std::out_of_range &) {
      status = QPX_READER_INVALID_REQUEST;
      message = "NGFF label source selection is outside its domain";
    } catch (const std::exception &) {
      status = QPX_READER_IO_ERROR;
      message = "NGFF label native operation failed";
    } catch (...) {
    }
    if (error && !text(*error, message))
      return QPX_READER_BUFFER_TOO_SMALL;
    return status;
  }
}
inline void checkpoint(const std::atomic<bool> *c) {
  if (c && c->load(std::memory_order_relaxed))
    fail("NGFF label operation cancelled", QPX_READER_CANCELLED);
}
// The lexical tree invokes this adapter synchronously on the native worker.
// It observes only the joined worker's atomic flag, never the host C callback.
struct TreeCancel {
  const std::atomic<bool> *flag;
  static std::int32_t QPX_READER_CALL cancelled(void *context) noexcept {
    const auto &self = *static_cast<const TreeCancel *>(context);
    return self.flag && self.flag->load(std::memory_order_relaxed) ? 1 : 0;
  }
  qpx_reader_cancel_v1 callback() noexcept { return {cancelled, this}; }
};
inline void poll(qpx_reader_cancel_v1 c) {
  if (c.is_cancelled && c.is_cancelled(c.context))
    fail("NGFF label operation cancelled", QPX_READER_CANCELLED);
}
// Native services take atomic cancellation, whereas the C callback is valid
// only on the synchronous caller thread. Join every worker before returning,
// also on callback/native failure. No callback context is cast or retained by a
// worker.
template <class F> auto native(qpx_reader_cancel_v1 c, F &&f) {
  using Result = std::invoke_result_t<F, const std::atomic<bool> *>;
  poll(c);
  std::atomic<bool> stop{false};
  auto job = std::async(std::launch::async, [&] { return f(&stop); });
  bool interrupted = false, callback_error = false;
  while (job.wait_for(std::chrono::milliseconds(2)) !=
         std::future_status::ready) {
    if (interrupted || callback_error)
      continue;
    try {
      if (c.is_cancelled && c.is_cancelled(c.context)) {
        interrupted = true;
        stop.store(true, std::memory_order_relaxed);
      }
    } catch (...) {
      callback_error = true;
      stop.store(true, std::memory_order_relaxed);
    }
  }
  auto completed = [&] {
    if (callback_error)
      fail("NGFF cancellation callback violated its C contract",
           QPX_READER_INTERNAL_ERROR);
    if (interrupted)
      fail("NGFF label operation cancelled", QPX_READER_CANCELLED);
    poll(c);
  };
  if constexpr (std::is_void_v<Result>) {
    try {
      job.get();
    } catch (...) {
      if (callback_error || interrupted)
        completed();
      throw;
    }
    completed();
  } else {
    try {
      auto result = job.get();
      completed();
      return result;
    } catch (...) {
      if (callback_error || interrupted)
        completed();
      throw;
    }
  }
}
inline std::size_t product(std::size_t a, std::size_t b) {
  require(!a || b <= std::numeric_limits<std::size_t>::max() / a,
          "NGFF label size overflow");
  return a * b;
}
inline void charge(std::size_t &used, std::size_t bytes, std::size_t limit) {
  require(used <= limit && bytes <= limit - used,
          "NGFF label work allowance exhausted");
  used += bytes;
}
struct JsonCheck final : nlohmann::json_sax<Json> {
  struct Frame {
    bool object;
    std::set<std::string> keys;
  };
  std::vector<Frame> frames;
  std::size_t nodes = 0;
  const std::atomic<bool> *cancel;
  bool allow_wide_integers;
  explicit JsonCheck(const std::atomic<bool> *c, bool wide_integers = false)
      : cancel(c), allow_wide_integers(wide_integers) {}
  bool node() {
    checkpoint(cancel);
    return ++nodes <= 16384;
  }
  bool null() override { return node(); }
  bool boolean(bool) override { return node(); }
  bool number_integer(number_integer_t) override { return node(); }
  bool number_unsigned(number_unsigned_t) override { return node(); }
  bool number_float(number_float_t n, const string_t &token) override {
    // Unknown property integer tokens may exceed UInt64 while remaining in the
    // frozen decoder's finite metadata domain. Their original lexical values
    // are preserved separately; every interpreted label ID still uses code().
    return node() && std::isfinite(n) &&
           (allow_wide_integers ||
            token.find_first_of(".eE") != std::string::npos);
  }
  bool string(string_t &) override { return node(); }
  bool binary(binary_t &) override { return false; }
  bool start_object(std::size_t) override {
    if (!node() || frames.size() >= 32)
      return false;
    frames.push_back({true, {}});
    return true;
  }
  bool key(string_t &key) override {
    return node() && !frames.empty() && frames.back().object &&
           frames.back().keys.insert(key).second;
  }
  bool end_object() override {
    if (frames.empty() || !frames.back().object)
      return false;
    frames.pop_back();
    return true;
  }
  bool start_array(std::size_t) override {
    if (!node() || frames.size() >= 32)
      return false;
    frames.push_back({false, {}});
    return true;
  }
  bool end_array() override {
    if (frames.empty() || frames.back().object)
      return false;
    frames.pop_back();
    return true;
  }
  bool parse_error(std::size_t, const std::string &,
                   const nlohmann::detail::exception &) override {
    return false;
  }
};
inline Json json(const std::string &input, const std::atomic<bool> *c,
                 bool allow_wide_integers = false) {
  checkpoint(c);
  {
    JsonCheck check(c, allow_wide_integers);
    valid(Json::sax_parse(input, &check),
          "NGFF metadata JSON is malformed, duplicated or exceeds depth/node "
          "bounds");
  }
  auto value = Json::parse(input);
  checkpoint(c);
  return value;
}
inline std::uint64_t integer(const Json &v) {
  if (v.is_number_unsigned())
    return v.get<std::uint64_t>();
  valid(v.is_number_integer() && v.get<std::int64_t>() >= 0,
        "NGFF selection requires exact nonnegative JSON integers");
  return std::uint64_t(v.get<std::int64_t>());
}
inline std::string string(const Json &v) {
  valid(v.is_string(), "NGFF metadata requires a string");
  return v.get<std::string>();
}
inline void keys(const Json &j, std::initializer_list<const char *> names) {
  valid(j.is_object() && j.size() == names.size(),
        "NGFF selection has unsupported/missing keys");
  for (auto name : names)
    valid(j.contains(name), "NGFF selection has a missing key");
}
inline bool relative_key(const std::string &value, bool empty = false) {
  if (value.empty())
    return empty;
  if (value.size() > 4096 || value.front() == '/' || value.back() == '/' ||
      value.find_first_of("\\:\0", 0, 3) != std::string::npos)
    return false;
  const auto p = fs::u8path(value);
  if (p.has_root_path() || p.is_absolute())
    return false;
  std::size_t at = 0;
  while (at < value.size()) {
    const auto end = value.find('/', at);
    const auto part = value.substr(
        at, end == std::string::npos ? std::string::npos : end - at);
    if (part.empty() || part == "." || part == "..")
      return false;
    if (end == std::string::npos)
      break;
    at = end + 1;
  }
  return true;
}
inline bool reparse(const fs::path &p) {
  const auto a = GetFileAttributesW(p.c_str());
  valid(a != INVALID_FILE_ATTRIBUTES, "Cannot inspect NGFF source path");
  return (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
         fs::is_symlink(fs::symlink_status(p));
}
inline fs::path root_path(qpx_reader_string_v1 v) {
  require(v.data && v.size && v.size <= 4096, "Invalid NGFF root path");
  std::string s(v.data, std::size_t(v.size));
  require(s.find('\0') == std::string::npos, "NGFF root path contains NUL");
  auto p = fs::u8path(s);
  require(p.is_absolute(), "NGFF root must be absolute");
  p = p.lexically_normal();
  while (p != p.root_path() && p.filename().empty())
    p = p.parent_path();
  valid(fs::is_directory(p), "NGFF root is not a local directory");
  for (auto at = p; !at.empty(); at = at.parent_path()) {
    valid(!reparse(at), "NGFF source ancestors cannot be links/reparse points");
    if (at == at.parent_path())
      break;
  }
  return p;
}
inline fs::path checked_path(const fs::path &root, const std::string &key,
                             bool directory = false) {
  valid(relative_key(key, directory),
        "NGFF group/file key is not normalized and within-store");
  for (auto at = root; !at.empty(); at = at.parent_path()) {
    valid(!reparse(at), "NGFF source ancestors cannot be links/reparse points");
    if (at == at.parent_path())
      break;
  }
  auto p = root;
  if (!key.empty())
    for (const auto &part : fs::u8path(key)) {
      p /= part;
      valid(!reparse(p),
            "NGFF source traversal cannot cross links/reparse points");
    }
  valid(p.u8string().size() <= 4096,
        "NGFF absolute metadata/group path exceeds4096 UTF-8 bytes");
  valid(directory ? fs::is_directory(p) : fs::is_regular_file(p),
        "Required NGFF group/metadata is absent");
  return p;
}
inline std::string metadata_text(const fs::path &root, const std::string &key,
                                 const std::atomic<bool> *c) {
  checkpoint(c);
  const auto p = checked_path(root, key);
  const auto n = fs::file_size(p);
  valid(n && n <= attrs_limit, "NGFF metadata file exceeds24576-byte bound");
  std::string s(std::size_t(n), '\0');
  std::ifstream in(p, std::ios::binary);
  valid(bool(in) && bool(in.read(s.data(), std::streamsize(n))) &&
            in.peek() == std::char_traits<char>::eof(),
        "NGFF metadata changed or could not be read");
  checkpoint(c);
  return s;
}
inline std::string child(const std::string &group, const char *name) {
  return group.empty() ? std::string(name) : group + "/" + name;
}
struct Selection {
  std::string label_group, reference_group;
  std::uint32_t z = 0, t = 0;
};
inline Selection selection(qpx_reader_string_v1 v, const std::atomic<bool> *c) {
  require(v.data && v.size && v.size <= 8192,
          "NGFF label options must contain the explicit selection");
  const auto j = json(std::string(v.data, std::size_t(v.size)), c);
  keys(j, {"schema", "label_group", "reference_group", "level", "z", "t"});
  valid(j.at("schema") == "qpx.ngff-label-selection/1",
        "Unsupported NGFF label selection schema");
  Selection out;
  out.label_group = string(j.at("label_group"));
  out.reference_group = string(j.at("reference_group"));
  valid(relative_key(out.label_group) &&
            relative_key(out.reference_group, true),
        "NGFF label/reference groups must be explicit normalized within-store "
        "keys");
  valid(integer(j.at("level")) == 0,
        "NGFF integral-label profile requires dataset level zero");
  const auto z = integer(j.at("z")), t = integer(j.at("t"));
  valid(z <= UINT32_MAX && t <= UINT32_MAX, "NGFF selected Z/T exceeds UInt32");
  out.z = std::uint32_t(z);
  out.t = std::uint32_t(t);
  return out;
}
struct Code {
  bool negative = false;
  std::uint64_t magnitude = 0;
  std::string decimal() const {
    return (negative ? "-" : "") + std::to_string(magnitude);
  }
  friend bool operator<(const Code &a, const Code &b) {
    if (a.negative != b.negative)
      return a.negative;
    return a.negative ? a.magnitude > b.magnitude : a.magnitude < b.magnitude;
  }
};
inline Code code(const Json &v) {
  if (v.is_number_unsigned())
    return {false, v.get<std::uint64_t>()};
  valid(v.is_number_integer(),
        "NGFF label-value must be an exact Int64/UInt64 JSON integer");
  const auto n = v.get<std::int64_t>();
  return n < 0 ? Code{true, std::uint64_t(-(n + 1)) + 1}
               : Code{false, std::uint64_t(n)};
}
template <class T> Code code(T n) {
  if constexpr (std::is_signed_v<T>)
    return n < 0 ? Code{true, std::uint64_t(-(n + 1)) + 1}
                 : Code{false, std::uint64_t(n)};
  else
    return {false, std::uint64_t(n)};
}
} // namespace qpx_ngff
