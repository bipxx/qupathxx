// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "binary_input.hpp"
#include "json_tree.hpp"
#include <array>
#include <blosc.h>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <utility>
#include <zlib.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Module-private, bounded local Zarr2 array decoding. The native host owns the
// complete store inventory/content identity and checks it around every call.
// This reader never publishes files, changes inputs or deserializes objects.
namespace qpx_zarr {
using namespace qpx_binary;
namespace json = qpx_native_json;
namespace fs = std::filesystem;
constexpr std::size_t mib = 1024u * 1024;
constexpr std::size_t metadata_limit = mib;
constexpr std::size_t chunk_limit = mib;
constexpr std::size_t encoded_limit = mib + 64u * 1024;
constexpr std::size_t json_workspace = 4u * mib;

inline bool same_metadata(const json::Tree &left, const json::Node &a,
                          const json::Tree &right, const json::Node &b,
                          qpx_reader_cancel_v1 cancel) {
  poll(cancel);
  if (a.kind != b.kind) return false;
  switch (a.kind) {
  case json::Kind::String: return a.value == b.value;
  case json::Kind::Number:
  case json::Kind::Boolean: return a.raw == b.raw;
  case json::Kind::Null: return true;
  case json::Kind::Array:
    if (a.children.size() != b.children.size()) return false;
    for (std::size_t at = 0; at < a.children.size(); ++at)
      if (!same_metadata(left, left.nodes[a.children[at]], right,
                         right.nodes[b.children[at]], cancel)) return false;
    return true;
  case json::Kind::Object:
    if (a.children.size() != b.children.size()) return false;
    for (const auto child : a.children) {
      const auto &source = left.nodes[child];
      const auto *other = right.find(b, source.key);
      if (!other || !same_metadata(left, source, right, *other, cancel)) return false;
    }
    return true;
  }
  return false;
}

struct Buffer {
  std::unique_ptr<std::uint8_t[]> data;
  std::size_t size = 0;
  Buffer() = default;
  explicit Buffer(std::size_t n) : data(n ? new std::uint8_t[n] : nullptr), size(n) {}
  Buffer(Buffer &&) = default;
  Buffer &operator=(Buffer &&) = default;
  Bytes bytes() const { return {data.get(), size}; }
  void clear() { data.reset(); size = 0; }
};

inline bool key(std::string_view value) {
  if (value.empty() || value.size() > 4096 || value.find_first_of("\\:") != std::string_view::npos)
    return false;
  std::size_t at = 0;
  while (at < value.size()) {
    const auto end = value.find('/', at);
    const auto part = value.substr(at, end == std::string_view::npos ? value.size() - at : end - at);
    if (part.empty() || part == "." || part == "..") return false;
    if (end == std::string_view::npos) return true;
    at = end + 1;
  }
  return false;
}

struct WinHandle {
  HANDLE value = INVALID_HANDLE_VALUE;
  explicit WinHandle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
  ~WinHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
  WinHandle(const WinHandle &) = delete;
  WinHandle &operator=(const WinHandle &) = delete;
};

inline std::wstring final_path(HANDLE handle) {
  std::array<wchar_t, 8192> name{};
  const auto n = GetFinalPathNameByHandleW(handle, name.data(), DWORD(name.size()), FILE_NAME_NORMALIZED);
  valid(n && n < name.size(), "Cannot resolve bounded local store file identity");
  return {name.data(), n};
}

class Store {
  fs::path root_;
  WinHandle root_handle_;
  std::wstring final_root_;
  std::uint64_t file_limit_;

  static void directory(const fs::path &path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    valid(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
              !(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
          "Store ancestors must be existing ordinary directories");
  }
public:
  Store(const fs::path &root, std::uint64_t file_limit, qpx_reader_cancel_v1 cancel)
      : root_(root), file_limit_(file_limit) {
    require(root.is_absolute() && root.lexically_normal() == root &&
                root.u8string().size() <= 4096,
            "Zarr store path must be absolute and lexically normalized");
    const auto drive = GetDriveTypeW(root.root_path().c_str());
    require(drive == DRIVE_FIXED || drive == DRIVE_REMOVABLE || drive == DRIVE_RAMDISK,
            "Zarr provider accepts a local filesystem store only");
    for (auto path = root; !path.empty();) {
      poll(cancel); directory(path);
      const auto parent = path.parent_path();
      if (parent == path) break;
      path = parent;
    }
    root_handle_.value = CreateFileW(root.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                                     nullptr, OPEN_EXISTING,
                                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    valid(root_handle_.value != INVALID_HANDLE_VALUE, "Cannot lease local Zarr store directory");
    final_root_ = final_path(root_handle_.value);
    while (!final_root_.empty() && final_root_.back() == L'\\') final_root_.pop_back();
  }
  fs::path path(std::string_view relative, qpx_reader_cancel_v1 cancel) const {
    require(key(relative), "Zarr member key is not a bounded relative path");
    utf8(relative, cancel);
    auto result = root_ / fs::u8path(relative);
    valid(result.u8string().size() <= 4096, "Zarr member absolute path exceeds limit");
    return result;
  }
  bool exists(std::string_view relative, qpx_reader_cancel_v1 cancel) const {
    poll(cancel);
    const auto full = path(relative, cancel);
    const auto attributes = GetFileAttributesW(full.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
      const auto error = GetLastError();
      valid(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND,
            "Cannot observe Zarr member");
      return false;
    }
    valid(!(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)),
          "Zarr selected member must be an ordinary file");
    return true;
  }
  Buffer read(std::string_view relative, std::size_t allowance,
              qpx_reader_cancel_v1 cancel) const {
    poll(cancel);
    const auto full = path(relative, cancel);
    for (auto parent = full.parent_path(); parent != root_;) {
      directory(parent);
      const auto next = parent.parent_path();
      valid(next != parent && !next.empty(), "Zarr member escapes the selected store");
      parent = next;
    }
    WinHandle file(CreateFileW(full.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr));
    valid(file.value != INVALID_HANDLE_VALUE && GetFileType(file.value) == FILE_TYPE_DISK,
          "Cannot lease ordinary Zarr member");
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    valid(GetFileInformationByHandleEx(file.value, FileAttributeTagInfo, &attributes, sizeof(attributes)) &&
              !(attributes.FileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)),
          "Zarr member must not be a directory or reparse point");
    const auto actual = final_path(file.value);
    valid(actual.size() > final_root_.size() && actual[final_root_.size()] == L'\\' &&
              CompareStringOrdinal(actual.data(), int(final_root_.size()), final_root_.data(),
                                   int(final_root_.size()), TRUE) == CSTR_EQUAL,
          "Opened Zarr member is outside the selected directory");
    LARGE_INTEGER size{};
    valid(GetFileSizeEx(file.value, &size) && size.QuadPart > 0 &&
              std::uint64_t(size.QuadPart) <= file_limit_ &&
              std::uint64_t(size.QuadPart) <= allowance,
          "Zarr member exceeds selected file/decoded allowance");
    Buffer result(std::size_t(size.QuadPart));
    for (std::size_t at = 0; at < result.size;) {
      poll(cancel);
      const auto take = DWORD(std::min<std::size_t>(64u * 1024, result.size - at));
      DWORD got = 0;
      valid(ReadFile(file.value, result.data.get() + at, take, &got, nullptr) && got == take,
            "Zarr member changed or became truncated during read");
      at += got;
    }
    std::uint8_t extra = 0; DWORD got = 0;
    valid(ReadFile(file.value, &extra, 1, &got, nullptr) && got == 0,
          "Zarr member grew during read");
    poll(cancel); return result;
  }
  template<class F> auto metadata(std::string_view relative, qpx_reader_cancel_v1 cancel,
                                  F &&action) const {
    const auto bytes = read(relative, metadata_limit, cancel);
    Arena arena(json_workspace);
    json::Tree tree(bytes.bytes().view(), &arena.memory, cancel, 4096, 8192, 16);
    if (relative != ".zmetadata" && exists(".zmetadata", cancel)) {
      // Two bounded parse trees coexist only during this comparison. The
      // provider reserves12MiB fixed workspace before any sparse-index storage.
      // Never allow a stale consolidated view to disagree with selected member
      // bytes that the native source/content authority actually observes.
      const auto consolidated = read(".zmetadata", metadata_limit, cancel);
      Arena comparison(json_workspace);
      json::Tree other(consolidated.bytes().view(), &comparison.memory, cancel, 4096, 8192, 18);
      const auto &root = other.root();
      other.keys(root, {"zarr_consolidated_format", "metadata"});
      valid(json::integer(other.get(root, "zarr_consolidated_format")) == 1,
            "Unsupported Zarr consolidated metadata version");
      const auto &members = other.get(root, "metadata");
      const auto *selected = other.find(members, relative);
      valid(selected && same_metadata(tree, tree.root(), other, *selected, cancel),
            "Selected consolidated metadata disagrees with physical member");
    }
    return action(tree, tree.root());
  }
};

enum class Kind { Unsigned, Signed, Floating, Utf8 };
struct Number { Kind kind = Kind::Unsigned; std::uint64_t u = 0; std::int64_t i = 0; double f = 0; };
inline std::uint64_t index(const Number &n) {
  if (n.kind == Kind::Unsigned) return n.u;
  valid(n.kind == Kind::Signed && n.i >= 0, "Sparse index/shape must be a nonnegative integer");
  return std::uint64_t(n.i);
}
inline std::uint64_t count(const Number &n) {
  if (n.kind != Kind::Floating) return index(n);
  valid(std::isfinite(n.f) && n.f >= 0 && n.f < std::ldexp(1., 64) && n.f == std::floor(n.f),
        "Count values must be finite exact nonnegative UInt64 integers");
  return std::uint64_t(n.f);
}
inline bool exactly_representable(std::uint64_t magnitude) {
  if (!magnitude) return true;
  while (!(magnitude & 1)) magnitude >>= 1;
  return magnitude <= UINT64_C(9007199254740991);
}
inline double continuous(const Number &n) {
  if (n.kind == Kind::Floating) {
    valid(std::isfinite(n.f), "Continuous matrix values must be finite"); return n.f;
  }
  if (n.kind == Kind::Unsigned) {
    valid(exactly_representable(n.u), "Integer to continuous conversion loses precision"); return double(n.u);
  }
  valid(n.kind == Kind::Signed, "Invalid matrix numeric kind");
  const auto magnitude = n.i < 0 ? std::uint64_t(-(n.i + 1)) + 1 : std::uint64_t(n.i);
  valid(exactly_representable(magnitude), "Integer to continuous conversion loses precision"); return double(n.i);
}

struct InflateAllocator {
  std::size_t used = 0;
  static voidpf allocate(voidpf context, uInt count, uInt width) {
    auto &self = *static_cast<InflateAllocator *>(context);
    const auto available = 512u * 1024 - self.used;
    if (!width || available < sizeof(std::size_t) ||
        std::size_t(count) > (available - sizeof(std::size_t)) / width) return nullptr;
    const auto n = std::size_t(count) * width;
    auto *p = static_cast<std::size_t *>(std::malloc(n + sizeof(std::size_t)));
    if (!p) return nullptr;
    *p = n + sizeof(std::size_t); self.used += *p; return p + 1;
  }
  static void release(voidpf context, voidpf value) {
    if (!value) return;
    auto *p = static_cast<std::size_t *>(value) - 1;
    static_cast<InflateAllocator *>(context)->used -= *p; std::free(p);
  }
};

enum class Codec { Raw, Zlib, Gzip, BloscLz4, BloscLz };

inline Codec parse_codec(const json::Tree &tree, const json::Node &compressor) {
  if (compressor.kind == json::Kind::Null) return Codec::Raw;
  const auto id = json::string(tree.get(compressor, "id"));
  if (id == "blosc") {
    tree.keys(compressor, {"id", "cname", "clevel", "shuffle", "blocksize"});
    const auto name = json::string(tree.get(compressor, "cname"));
    valid(name == "lz4" || name == "blosclz", "Zarr Blosc requires LZ4 or BloscLZ");
    const auto &shuffle = tree.get(compressor, "shuffle");
    valid(json::integer(tree.get(compressor, "clevel")) <= 9 &&
              ((shuffle.kind == json::Kind::Number && shuffle.raw == "-1") || json::integer(shuffle) <= 2) &&
              json::integer(tree.get(compressor, "blocksize")) <= chunk_limit,
          "Invalid Zarr Blosc compressor metadata");
    return name == "lz4" ? Codec::BloscLz4 : Codec::BloscLz;
  }
  valid(id == "zlib" || id == "gzip", "Unadmitted Zarr compressor");
  tree.keys(compressor, {"id", "level"});
  if (const auto *level = tree.find(compressor, "level")) {
    std::int64_t n = 0;
    const auto parsed = std::from_chars(level->raw.data(), level->raw.data() + level->raw.size(), n);
    valid(level->kind == json::Kind::Number && parsed.ec == std::errc{} &&
              parsed.ptr == level->raw.data() + level->raw.size() && n >= (id == "gzip" ? 0 : -1) && n <= 9,
          "Invalid Zarr compression-level metadata");
  }
  return id == "gzip" ? Codec::Gzip : Codec::Zlib;
}

inline Buffer decode_blosc(const Buffer &encoded, Codec codec, std::size_t maximum,
                           std::size_t expected, qpx_reader_cancel_v1 cancel) {
  poll(cancel);
  const auto *input = encoded.data.get();
  std::size_t size = 0;
  valid(encoded.size >= BLOSC_MIN_HEADER_LENGTH && input[0] == BLOSC_VERSION_FORMAT &&
            blosc_cbuffer_validate(input, encoded.size, &size) == 0 && size && size <= maximum &&
            (!expected || size == expected),
        "Invalid Zarr Blosc header or decoded chunk size");
  std::size_t header_size = 0, compressed_size = 0, block_size = 0, type_size = 0;
  int flags = 0;
  blosc_cbuffer_sizes(input, &header_size, &compressed_size, &block_size);
  blosc_cbuffer_metainfo(input, &type_size, &flags);
  const auto *library = blosc_cbuffer_complib(input);
  valid(header_size == size && compressed_size == encoded.size && block_size && block_size <= size &&
            type_size && type_size <= BLOSC_MAX_TYPESIZE && library &&
            std::string_view(library) == (codec == Codec::BloscLz4 ? "LZ4" : "BloscLZ") &&
            (!(flags & BLOSC_MEMCPYED) || compressed_size == size + BLOSC_MIN_HEADER_LENGTH),
        "Zarr Blosc frame does not match compressor metadata or chunk bounds");
  Buffer decoded(size);
  const auto written = blosc_decompress_ctx(input, decoded.data.get(), size, 1);
  poll(cancel);
  valid(written > 0 && std::size_t(written) == size, "Corrupt Zarr Blosc payload");
  return decoded;
}

inline Buffer decode_chunk(Buffer encoded, Codec codec, std::size_t maximum,
                      std::size_t expected, qpx_reader_cancel_v1 cancel) {
  if (codec == Codec::Raw) {
    valid(encoded.size <= maximum && (!expected || encoded.size == expected), "Raw Zarr chunk size differs");
    return encoded;
  }
  if (codec == Codec::BloscLz4 || codec == Codec::BloscLz)
    return decode_blosc(encoded, codec, maximum, expected, cancel);
  Buffer decoded(maximum);
  InflateAllocator allocator; z_stream stream{};
  stream.zalloc = InflateAllocator::allocate; stream.zfree = InflateAllocator::release; stream.opaque = &allocator;
  valid(inflateInit2(&stream, codec == Codec::Gzip ? 31 : 15) == Z_OK, "Cannot initialize bounded Zarr codec");
  struct End { z_stream *s; ~End() { inflateEnd(s); } } end{&stream};
  stream.next_in = encoded.data.get(); stream.avail_in = uInt(encoded.size);
  stream.next_out = decoded.data.get(); stream.avail_out = uInt(decoded.size);
  int status = Z_OK;
  while (status == Z_OK) { poll(cancel); status = ::inflate(&stream, Z_FINISH); }
  valid(status == Z_STREAM_END && stream.total_in == encoded.size &&
            stream.total_out <= maximum && (!expected || stream.total_out == expected),
        "Corrupt, oversized, truncated or trailing Zarr compressed chunk");
  decoded.size = std::size_t(stream.total_out); poll(cancel); return decoded;
}

inline Number integer_fill(const json::Node &node, Kind kind) {
  valid(node.kind == json::Kind::Number, "Integral Zarr fills require an exact JSON integer token");
  Number n; n.kind = kind;
  if (kind == Kind::Unsigned) n.u = node.raw == "-0" ? 0 : json::integer(node);
  else {
    const auto r = std::from_chars(node.raw.data(), node.raw.data() + node.raw.size(), n.i);
    valid(r.ec == std::errc{} && r.ptr == node.raw.data() + node.raw.size(),
          "Signed Zarr fill is outside exact integer range");
  }
  return n;
}

class Array {
  Store &store_;
  std::string prefix_, separator_ = ".", cached_key_;
  Codec codec_ = Codec::Raw;
  Buffer cache_;
  Number fill_;
  bool undefined_fill_ = false, fortran_ = false, big_ = false;
  std::size_t element_bytes_ = 0, chunk_elements_ = 0, decoded_bytes_ = 0;
  void load(const std::array<std::uint64_t, 2> &coordinate, qpx_reader_cancel_v1 cancel) {
    const auto relative = prefix_ + "/" + std::to_string(coordinate[0]) +
        (rank == 2 ? separator_ + std::to_string(coordinate[1]) : std::string{});
    if (cached_key_ == relative) return;
    if (!store_.exists(relative, cancel)) {
      valid(kind != Kind::Utf8 && !undefined_fill_, "Missing Zarr chunk has undefined fill");
      cache_.clear(); cached_key_ = relative; return;
    }
    auto next = decode_chunk(store_.read(relative, encoded_limit, cancel), codec_,
                        kind == Kind::Utf8 ? chunk_limit : decoded_bytes_,
                        kind == Kind::Utf8 ? 0 : decoded_bytes_, cancel);
    if (kind == Kind::Utf8) {
      const auto bytes = next.bytes();
      valid(bytes.size >= 4 && bytes.le32(0) == chunk_elements_, "VLenUTF8 chunk item count differs");
      std::size_t at = 4;
      for (std::size_t i = 0; i < chunk_elements_; ++i) {
        if (!(i & 255)) poll(cancel);
        valid(at <= bytes.size && bytes.size - at >= 4, "Truncated VLenUTF8 string length");
        const auto size = bytes.le32(at); at += 4;
        valid(size <= 4096 && size <= bytes.size - at, "VLenUTF8 string exceeds native size/chunk bound");
        utf8(bytes.sub(at, size).view(), cancel);
        at += size;
      }
      valid(at == bytes.size, "Trailing bytes after VLenUTF8 chunk");
    }
    cache_ = std::move(next); cached_key_ = relative;
  }
public:
  Kind kind = Kind::Unsigned;
  std::size_t rank = 0;
  std::array<std::uint64_t, 2> shape{}, chunks{};
  Array(Store &store, std::string prefix, qpx_reader_cancel_v1 cancel)
      : store_(store), prefix_(std::move(prefix)) {
    store.metadata(prefix_ + "/.zarray", cancel, [&](const json::Tree &tree, const json::Node &root) {
      tree.keys(root, {"zarr_format", "shape", "chunks", "dtype", "compressor", "filters", "fill_value", "order", "dimension_separator"});
      valid(json::integer(tree.get(root, "zarr_format")) == 2, "Requires Zarr2 array metadata");
      const auto &dims = tree.array(tree.get(root, "shape"));
      const auto &block = tree.array(tree.get(root, "chunks"));
      valid((dims.size() == 1 || dims.size() == 2) && block.size() == dims.size(), "Zarr array rank/chunk rank is unsupported");
      rank = dims.size(); chunk_elements_ = 1;
      for (std::size_t axis = 0; axis < rank; ++axis) {
        shape[axis] = json::integer(tree.nodes[dims[axis]]); chunks[axis] = json::integer(tree.nodes[block[axis]]);
        valid(shape[axis] <= INT64_MAX && chunks[axis] && chunks[axis] <= chunk_limit / chunk_elements_, "Zarr dimensions/chunks exceed bounds");
        chunk_elements_ *= std::size_t(chunks[axis]);
      }
      const auto dtype = json::string(tree.get(root, "dtype"));
      if (dtype == "|O") { kind = Kind::Utf8; valid(rank == 1, "String index arrays must be one-dimensional"); }
      else {
        valid(dtype.size() == 3 && (dtype[0] == '<' || dtype[0] == '>' || dtype[0] == '|'), "Unsupported Zarr numeric dtype");
        big_ = dtype[0] == '>';
        kind = dtype[1] == 'u' ? Kind::Unsigned : dtype[1] == 'i' ? Kind::Signed : Kind::Floating;
        valid(dtype[1] == 'u' || dtype[1] == 'i' || dtype[1] == 'f', "Unsupported Zarr scalar type");
        element_bytes_ = std::size_t(dtype[2] - '0');
        valid((element_bytes_ == 1 || element_bytes_ == 2 || element_bytes_ == 4 || element_bytes_ == 8) &&
                  (kind != Kind::Floating || element_bytes_ == 4 || element_bytes_ == 8) &&
                  (dtype[0] != '|' || element_bytes_ == 1), "Unsupported Zarr scalar precision/endian");
      }
      const auto &filters = tree.get(root, "filters");
      if (kind == Kind::Utf8) {
        valid(filters.kind == json::Kind::Array && filters.children.size() == 1,
              "Object arrays require exactly one VLenUTF8 filter");
        const auto &filter = tree.nodes[filters.children[0]];
        tree.keys(filter, {"id"});
        valid(json::string(tree.get(filter, "id")) == "vlen-utf8", "Arbitrary object codecs are not admitted");
        valid(chunk_elements_ <= (chunk_limit - 4) / 4, "String chunk has too many encoded items");
      } else {
        valid(filters.kind == json::Kind::Null || (filters.kind == json::Kind::Array && filters.children.empty()), "Numeric Zarr filters are unsupported");
        valid(chunk_elements_ <= chunk_limit / element_bytes_, "Decoded numeric chunk exceeds1MiB");
        decoded_bytes_ = chunk_elements_ * element_bytes_;
      }
      codec_ = parse_codec(tree, tree.get(root, "compressor"));
      const auto order = json::string(tree.get(root, "order"));
      valid(order == "C" || order == "F", "Unsupported Zarr chunk order"); fortran_ = order == "F";
      if (const auto *separator = tree.find(root, "dimension_separator")) separator_ = json::string(*separator);
      valid(separator_ == "." || separator_ == "/", "Unsupported Zarr chunk-key separator");
      const auto &fill = tree.get(root, "fill_value"); undefined_fill_ = fill.kind == json::Kind::Null;
      fill_.kind = kind;
      if (kind == Kind::Utf8) {
        valid(undefined_fill_ || (fill.kind == json::Kind::Number && fill.raw == "0") ||
                  (fill.kind == json::Kind::String && fill.value.empty()), "Unsupported object-array fill metadata");
      } else if (!undefined_fill_) {
        if (kind == Kind::Floating) {
          fill_.f = json::real(fill);
          if (element_bytes_ == 4) { fill_.f = float(fill_.f); valid(std::isfinite(fill_.f), "Float32 fill overflow"); }
        } else {
          fill_ = integer_fill(fill, kind);
          if (kind == Kind::Unsigned && element_bytes_ < 8) valid(fill_.u < (UINT64_C(1) << (8 * element_bytes_)), "Unsigned fill overflow");
          if (kind == Kind::Signed && element_bytes_ < 8) {
            const auto maximum = std::int64_t(UINT64_C(1) << (8 * element_bytes_ - 1));
            valid(fill_.i >= -maximum && fill_.i < maximum, "Signed fill overflow");
          }
        }
      }
      return 0;
    });
  }
  void clear_cache() { cache_.clear(); cached_key_.clear(); }
  void validate_domain(bool counts) const {
    valid(kind != Kind::Utf8, "Matrix values require a numeric array");
    if (!undefined_fill_) { if (counts) (void)count(fill_); else (void)continuous(fill_); }
  }
  void validate_index_fill() const {
    valid(kind == Kind::Unsigned || kind == Kind::Signed, "Sparse indices require an integer array");
    if (!undefined_fill_) (void)index(fill_);
  }
  Number number(std::uint64_t first, std::uint64_t second, qpx_reader_cancel_v1 cancel) {
    valid(kind != Kind::Utf8 && first < shape[0] && (rank == 1 || second < shape[1]), "Zarr numeric index is outside array bounds");
    const std::array<std::uint64_t, 2> coordinate{first / chunks[0], rank == 2 ? second / chunks[1] : 0};
    load(coordinate, cancel);
    if (!cache_.size) return fill_;
    const auto row = first % chunks[0], column = rank == 2 ? second % chunks[1] : 0;
    const auto linear = rank == 1 ? row : fortran_ ? column * chunks[0] + row : row * chunks[1] + column;
    const auto word = cache_.bytes().number(std::size_t(linear) * element_bytes_, unsigned(element_bytes_), big_);
    Number result; result.kind = kind;
    if (kind == Kind::Unsigned) result.u = word;
    else if (kind == Kind::Signed) {
      auto extended = word;
      if (element_bytes_ < 8 && (word & (UINT64_C(1) << (8 * element_bytes_ - 1)))) extended |= UINT64_MAX << (8 * element_bytes_);
      std::memcpy(&result.i, &extended, sizeof(result.i));
    } else if (element_bytes_ == 4) { const auto narrow = std::uint32_t(word); float f; std::memcpy(&f, &narrow, 4); result.f = f; }
    else std::memcpy(&result.f, &word, 8);
    return result;
  }
  Number number(std::uint64_t first, qpx_reader_cancel_v1 cancel) { return number(first, 0, cancel); }
  template<class F> void strings(std::uint64_t first, std::size_t count, std::size_t text_limit,
                                 bool empty_allowed, F &&sink, qpx_reader_cancel_v1 cancel) {
    valid(kind == Kind::Utf8 && rank == 1 && first <= shape[0] && count <= shape[0] - first,
          "String page is outside array bounds");
    std::size_t done = 0;
    while (done < count) {
      poll(cancel); const auto start = first + done;
      load({start / chunks[0], 0}, cancel);
      const auto local = std::size_t(start % chunks[0]);
      const auto take = std::min(count - done, std::size_t(chunks[0]) - local);
      const auto bytes = cache_.bytes(); std::size_t at = 4;
      for (std::size_t index = 0; index < local + take; ++index) {
        const auto size = bytes.le32(at); at += 4;
        if (index >= local) {
          const auto value = bytes.sub(at, size).view();
          valid(value.size() <= text_limit && (empty_allowed || !value.empty()), "String ID/name exceeds native field bounds");
          sink(done + index - local, value);
        }
        at += size;
      }
      done += take;
    }
  }
};
} // namespace qpx_zarr
