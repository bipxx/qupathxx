// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
// Generic bounded HDF5 input mechanisms extracted from the native H5AD reader.
// Format selection, matrix interpretation and application publication stay with
// their respective authorities. Each privately linked HDF5 instance owns a
// gate.
#include "reader_input.hpp"
#include <H5VLconnector.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <hdf5.h>
#include <limits>
#include <mutex>
#include <utility>
#include <zlib.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>

// Exact pinned native HDF5 checksum authority (H5private.h in HDF5 2.2.0).
// The static library exports this symbol; no checksum implementation is copied.
extern "C" std::uint32_t H5_checksum_fletcher32(const void *, std::size_t);

namespace qpx_hdf5 {
using qpx_binary::poll;
using qpx_binary::require;
using qpx_binary::valid;
inline constexpr std::size_t mib = 1024u * 1024;
inline constexpr std::size_t max_page = 65536;
inline constexpr std::size_t string_page = 256;
inline constexpr std::size_t string_bytes = 256;
inline std::recursive_timed_mutex library_mutex;

inline auto library_lock(qpx_reader_cancel_v1 cancel) {
  std::unique_lock<std::recursive_timed_mutex> lock(library_mutex,
                                                    std::defer_lock);
  while (!lock.try_lock_for(std::chrono::milliseconds(10)))
    poll(cancel);
  poll(cancel);
  return lock;
}
inline void status(herr_t result, const char *message) {
  valid(result >= 0, message);
}

class Handle {
  hid_t id_ = -1;

public:
  Handle() = default;
  explicit Handle(hid_t id) : id_(id) {
    valid(id >= 0, "HDF5 cannot open selected object/type/space");
  }
  Handle(Handle &&other) noexcept : id_(std::exchange(other.id_, -1)) {}
  Handle &operator=(Handle &&other) noexcept {
    if (this != &other) {
      close();
      id_ = std::exchange(other.id_, -1);
    }
    return *this;
  }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  ~Handle() { close(); }
  operator hid_t() const noexcept { return id_; }
  bool present() const noexcept { return id_ >= 0; }

private:
  void close() noexcept {
    if (id_ < 0)
      return;
    switch (H5Iget_type(id_)) {
    case H5I_FILE:
      H5Fclose(id_);
      break;
    case H5I_GROUP:
      H5Gclose(id_);
      break;
    case H5I_DATASET:
      H5Dclose(id_);
      break;
    case H5I_ATTR:
      H5Aclose(id_);
      break;
    case H5I_DATATYPE:
      H5Tclose(id_);
      break;
    case H5I_DATASPACE:
      H5Sclose(id_);
      break;
    case H5I_GENPROP_LST:
      H5Pclose(id_);
      break;
    default:
      break;
    }
    id_ = -1;
  }
};
struct Errors {
  H5E_auto2_t previous = nullptr;
  void *context = nullptr;
  bool active = false;
  explicit Errors(bool strict = true) {
    const auto captured = H5Eget_auto2(H5E_DEFAULT, &previous, &context);
    if (captured < 0) {
      if (strict)
        status(captured, "HDF5 cannot capture error handler");
      return;
    }
    const auto configured = H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);
    if (configured < 0) {
      if (strict)
        status(configured, "HDF5 cannot configure error handler");
      return;
    }
    active = true;
  }
  ~Errors() {
    if (active)
      H5Eset_auto2(H5E_DEFAULT, previous, context);
  }
};

// A cooperating read-only HDF5 open is allowed, while existing/future writers
// or deleters cannot coexist. The lease is retained through every validated
// chunk and normal HDF5 read, preventing a filter check/read replacement race.
class ReadLease {
  struct Close {
    void operator()(void *handle) const noexcept {
      if (handle && handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);
    }
  };
  std::unique_ptr<void, Close> file_;
  FILE_ID_INFO identity_{};

public:
  std::uint64_t bytes = 0;
  ReadLease(const std::filesystem::path &path, std::uint64_t limit)
      : file_(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS |
                              FILE_FLAG_OPEN_REPARSE_POINT,
                          nullptr)) {
    valid(file_.get() != INVALID_HANDLE_VALUE,
          "HDF5 source cannot acquire immutable read lease");
    LARGE_INTEGER size{};
    BY_HANDLE_FILE_INFORMATION attributes{};
    valid(GetFileType(file_.get()) == FILE_TYPE_DISK &&
              GetFileInformationByHandle(file_.get(), &attributes) &&
              GetFileInformationByHandleEx(file_.get(), FileIdInfo, &identity_,
                                           sizeof(identity_)) &&
              !(attributes.dwFileAttributes &
                (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
              GetFileSizeEx(file_.get(), &size) && size.QuadPart > 0,
          "HDF5 source lease needs an ordinary nonempty file");
    bytes = std::uint64_t(size.QuadPart);
    require(bytes <= limit, "HDF5 source exceeds independent file allowance");
  }
  void verify_file(hid_t file) const {
    void *descriptor = nullptr;
    status(H5Fget_vfd_handle(file, H5P_DEFAULT, &descriptor),
           "HDF5 cannot inspect native local file handle");
    valid(descriptor != nullptr, "HDF5 returned no local file descriptor");
    const auto handle = _get_osfhandle(*static_cast<const int *>(descriptor));
    FILE_ID_INFO actual{};
    valid(handle != -1 &&
              GetFileInformationByHandleEx(reinterpret_cast<HANDLE>(handle),
                                           FileIdInfo, &actual,
                                           sizeof(actual)) &&
              actual.VolumeSerialNumber == identity_.VolumeSerialNumber &&
              std::memcmp(&actual.FileId, &identity_.FileId,
                          sizeof(actual.FileId)) == 0,
          "HDF5 decoder file differs from retained source lease");
  }
  void read(std::uint64_t offset, std::uint8_t *output, std::size_t count,
            qpx_reader_cancel_v1 cancel) const {
    valid(offset <= bytes && count <= bytes - offset,
          "HDF5 raw chunk is outside observed file bounds");
    LARGE_INTEGER position{};
    position.QuadPart = LONGLONG(offset);
    valid(SetFilePointerEx(file_.get(), position, nullptr, FILE_BEGIN) != 0,
          "HDF5 raw chunk seek failed");
    for (std::size_t first = 0; first < count;) {
      poll(cancel);
      const auto take = DWORD(std::min<std::size_t>(65536, count - first));
      DWORD received = 0;
      valid(ReadFile(file_.get(), output + first, take, &received, nullptr) &&
                received == take,
            "HDF5 raw chunk read failed");
      first += take;
    }
    poll(cancel);
  }
};

inline Handle open_file(const std::string &path) {
  status(H5PLset_loading_state(0), "HDF5 cannot disable external plugins");
  Handle access(H5Pcreate(H5P_FILE_ACCESS));
  status(H5Pset_vol(access, H5VL_NATIVE, H5P_DEFAULT),
         "HDF5 cannot select native connector");
  status(H5Pset_fapl_sec2(access), "HDF5 cannot select local file driver");
  status(H5Pset_cache(access, 0, 521, 256u * 1024, .75),
         "HDF5 cannot bound raw chunk cache");
  H5AC_cache_config_t cache{};
  cache.version = H5AC__CURR_CACHE_CONFIG_VERSION;
  status(H5Pget_mdc_config(access, &cache),
         "HDF5 cannot inspect metadata cache");
  cache.set_initial_size = true;
  cache.initial_size = 512u * 1024;
  cache.min_size = 256u * 1024;
  cache.max_size = 2u * mib;
  status(H5Pset_mdc_config(access, &cache), "HDF5 cannot bound metadata cache");
  return Handle(H5Fopen(path.c_str(), H5F_ACC_RDONLY, access));
}

inline void hard_path(hid_t file, const std::string &path) {
  require(!path.empty() && path.front() == '/' && path.size() <= 1024,
          "HDF5 selected path must be bounded and absolute");
  for (std::size_t begin = 1; begin < path.size();) {
    const auto slash = path.find('/', begin);
    const auto end = slash == std::string::npos ? path.size() : slash;
    const auto name = std::string_view(path).substr(begin, end - begin);
    require(!name.empty() && name != "." && name != "..",
            "HDF5 selected path has an invalid component");
    H5L_info2_t link{};
    const auto prefix = path.substr(0, end);
    status(H5Lget_info2(file, prefix.c_str(), &link, H5P_DEFAULT),
           "HDF5 selected path is absent");
    valid(link.type == H5L_TYPE_HARD,
          "HDF5 external/soft selected links are unsupported");
    if (slash == std::string::npos)
      break;
    begin = slash + 1;
  }
  require(path.back() != '/', "HDF5 selected path has a trailing separator");
}
inline Handle group(hid_t file, const std::string &path) {
  hard_path(file, path);
  return Handle(H5Gopen2(file, path.c_str(), H5P_DEFAULT));
}
inline Handle dataset(hid_t file, const std::string &path) {
  hard_path(file, path);
  return Handle(H5Dopen2(file, path.c_str(), H5P_DEFAULT));
}
struct Shape {
  unsigned rank = 0;
  std::array<hsize_t, 2> extent{};
};
inline Shape shape(hid_t data) {
  Handle space(H5Dget_space(data));
  const auto rank = H5Sget_simple_extent_ndims(space);
  valid(rank >= 0 && rank <= 2, "HDF5 selected dataset rank exceeds two");
  Shape result;
  result.rank = unsigned(rank);
  status(H5Sget_simple_extent_dims(space, result.extent.data(), nullptr),
         "HDF5 cannot inspect selected dataset shape");
  return result;
}
struct Filter {
  H5Z_filter_t kind = H5Z_FILTER_ERROR;
  unsigned parameter = 0;
};
struct ReadPlan {
  Shape dimensions;
  bool chunked = false;
  std::array<hsize_t, 2> chunk{};
  std::size_t element_bytes = 0, logical_chunk_bytes = 0;
  unsigned filter_count = 0;
  std::array<Filter, 3> filters{};
  std::array<std::array<hsize_t, 2>, 64> validated{};
  std::size_t validated_count = 0, next_slot = 0;
};
inline ReadPlan inspect_dataset(hid_t file, hid_t data,
                                std::size_t chunk_limit) {
  ReadPlan result;
  result.dimensions = shape(data);
  Handle type(H5Dget_type(data));
  result.element_bytes = H5Tget_size(type);
  if (H5Tget_class(type) == H5T_STRING && H5Tis_variable_str(type) > 0) {
    Handle creation(H5Fget_create_plist(file));
    std::size_t address_bytes = 0, size_bytes = 0;
    status(H5Pget_sizes(creation, &address_bytes, &size_bytes),
           "HDF5 cannot inspect native blob-reference width");
    valid(address_bytes > 0 && address_bytes <= 8,
          "HDF5 native file address width is unsupported");
    // Pinned native VOL: four-byte sequence length, address, four-byte heap ID.
    // H5Tvlen.c sets disk size=4+blob_id_size; H5VLnative_blob.c encodes
    // address+u32.
    result.element_bytes = address_bytes + 8;
  }
  valid(result.element_bytes > 0 && result.element_bytes <= chunk_limit,
        "HDF5 selected dtype exceeds chunk allowance");
  Handle layout(H5Dget_create_plist(data));
  const auto kind = H5Pget_layout(layout);
  valid(kind == H5D_CONTIGUOUS || kind == H5D_COMPACT || kind == H5D_CHUNKED,
        "HDF5 virtual/unknown storage is unsupported");
  valid(H5Pget_external_count(layout) == 0,
        "HDF5 external dataset storage is unsupported");
  const auto filter_count = H5Pget_nfilters(layout);
  valid(filter_count >= 0 && filter_count <= 3,
        "HDF5 filter count exceeds admitted subset");
  valid(!filter_count || kind == H5D_CHUNKED,
        "HDF5 filters require chunked storage");
  result.filter_count = unsigned(filter_count);
  unsigned seen = 0;
  for (unsigned index = 0; index < result.filter_count; ++index) {
    unsigned flags = 0, values[16]{};
    std::size_t count = 16;
    const auto filter = H5Pget_filter2(layout, unsigned(index), &flags, &count,
                                       values, 0, nullptr, nullptr);
    const auto bit = filter == H5Z_FILTER_DEFLATE      ? 1u
                     : filter == H5Z_FILTER_SHUFFLE    ? 2u
                     : filter == H5Z_FILTER_FLETCHER32 ? 4u
                                                       : 0u;
    valid(bit && !(seen & bit) && count <= 16,
          "HDF5 unknown/repeated dataset filter is unsupported");
    valid((filter == H5Z_FILTER_DEFLATE && count == 1 && values[0] <= 9) ||
              (filter == H5Z_FILTER_SHUFFLE && count == 1 &&
               values[0] == result.element_bytes) ||
              (filter == H5Z_FILTER_FLETCHER32 && count == 0),
          "HDF5 filter parameters differ from admitted native profile");
    result.filters[index] = {filter, count ? values[0] : 0};
    seen |= bit;
  }
  if (kind == H5D_CHUNKED) {
    result.chunked = true;
    const auto rank =
        H5Pget_chunk(layout, int(result.chunk.size()), result.chunk.data());
    valid(rank > 0 && unsigned(rank) == result.dimensions.rank,
          "HDF5 selected chunk rank differs from dataset");
    auto bytes = result.element_bytes;
    for (int axis = 0; axis < rank; ++axis) {
      valid(result.chunk[axis] && result.chunk[axis] <= chunk_limit / bytes,
            "HDF5 declared decoded chunk exceeds byte allowance");
      bytes *= std::size_t(result.chunk[axis]);
    }
    result.logical_chunk_bytes = bytes;
  }
  return result;
}

struct InflateMemory {
  std::size_t used = 0;
  struct alignas(std::max_align_t) Header {
    std::size_t bytes;
  };
  static voidpf allocate(voidpf context, uInt items, uInt size) noexcept {
    auto &self = *static_cast<InflateMemory *>(context);
    if (size && items > (128u * 1024) / size)
      return nullptr;
    const auto count = std::size_t(items) * size;
    if (!count || count + sizeof(Header) > 128u * 1024 - self.used)
      return nullptr;
    auto *header =
        static_cast<Header *>(std::calloc(1, count + sizeof(Header)));
    if (!header)
      return nullptr;
    header->bytes = count + sizeof(Header);
    self.used += header->bytes;
    return header + 1;
  }
  static void release(voidpf context, voidpf memory) noexcept {
    if (!memory)
      return;
    auto *header = static_cast<Header *>(memory) - 1;
    static_cast<InflateMemory *>(context)->used -= header->bytes;
    std::free(header);
  }
};
struct ChunkWorkspace {
  static constexpr std::size_t capacity = mib + 65536 + 8;
  std::unique_ptr<std::uint8_t[]> first{new std::uint8_t[capacity]};
  std::unique_ptr<std::uint8_t[]> second{new std::uint8_t[capacity]};
};
inline std::size_t inflate_chunk(const std::uint8_t *source, std::size_t size,
                                 std::uint8_t *target, std::size_t limit,
                                 qpx_reader_cancel_v1 cancel) {
  poll(cancel);
  require(size <= ChunkWorkspace::capacity && limit <= mib + 5,
          "HDF5 Deflate probe exceeds bounded workspace");
  InflateMemory memory;
  z_stream stream{};
  stream.zalloc = InflateMemory::allocate;
  stream.zfree = InflateMemory::release;
  stream.opaque = &memory;
  valid(inflateInit(&stream) == Z_OK,
        "HDF5 bounded Deflate initialization failed");
  struct End {
    z_stream &stream;
    ~End() { inflateEnd(&stream); }
  } end{stream};
  stream.next_in = const_cast<Bytef *>(source);
  stream.avail_in = uInt(size);
  stream.next_out = target;
  stream.avail_out = uInt(limit);
  const auto rc = inflate(&stream, Z_FINISH);
  poll(cancel);
  valid(rc == Z_STREAM_END && stream.total_in == size &&
            stream.total_out < limit,
        "HDF5 Deflate stream is invalid or exceeds declared decoded chunk");
  return std::size_t(stream.total_out);
}
inline void unshuffle(const std::uint8_t *source, std::uint8_t *target,
                      std::size_t size, unsigned stride,
                      qpx_reader_cancel_v1 cancel) {
  valid(stride > 0 && stride <= 16,
        "HDF5 shuffle stride exceeds admitted native type");
  const auto elements = size / stride, copied = elements * stride;
  for (std::size_t byte = 0; byte < stride; ++byte)
    for (std::size_t element = 0; element < elements; ++element) {
      if (!(element & 4095))
        poll(cancel);
      target[element * stride + byte] = source[byte * elements + element];
    }
  std::memcpy(target + copied, source + copied, size - copied);
  poll(cancel);
}
inline void prevalidate_chunk(hid_t data, ReadPlan &plan,
                              const std::array<hsize_t, 2> &coordinate,
                              const ReadLease &lease, ChunkWorkspace &scratch,
                              qpx_reader_cancel_v1 cancel) {
  for (std::size_t index = 0; index < plan.validated_count; ++index)
    if (plan.validated[index] == coordinate)
      return;
  poll(cancel);
  unsigned mask = 0;
  haddr_t address = HADDR_UNDEF;
  hsize_t encoded_size = 0;
  status(H5Dget_chunk_info_by_coord(data, coordinate.data(), &mask, &address,
                                    &encoded_size),
         "HDF5 cannot inspect selected raw chunk");
  if (!encoded_size) {
    valid(address == HADDR_UNDEF, "HDF5 empty chunk address is inconsistent");
  } else {
    valid(address != HADDR_UNDEF &&
              encoded_size <= plan.logical_chunk_bytes + 65536 &&
              encoded_size <= ChunkWorkspace::capacity &&
              (mask >> plan.filter_count) == 0,
          "HDF5 encoded chunk exceeds allowance or has invalid filter mask");
    auto *current = scratch.first.get(), *other = scratch.second.get();
    lease.read(address, current, std::size_t(encoded_size), cancel);
    auto bytes = std::size_t(encoded_size);
    for (unsigned position = plan.filter_count; position-- > 0;) {
      if (mask & (1u << position))
        continue;
      const auto filter = plan.filters[position];
      if (filter.kind == H5Z_FILTER_FLETCHER32) {
        valid(bytes >= 4, "HDF5 Fletcher32 chunk is truncated");
        bytes -= 4;
        const auto stored = std::uint32_t(current[bytes]) |
                            (std::uint32_t(current[bytes + 1]) << 8) |
                            (std::uint32_t(current[bytes + 2]) << 16) |
                            (std::uint32_t(current[bytes + 3]) << 24);
        const auto expected = H5_checksum_fletcher32(current, bytes);
        const auto legacy =
            ((expected & 0x00ff00ffu) << 8) | ((expected & 0xff00ff00u) >> 8);
        valid(stored == expected || stored == legacy,
              "HDF5 Fletcher32 chunk checksum differs");
      } else if (filter.kind == H5Z_FILTER_DEFLATE) {
        bytes = inflate_chunk(current, bytes, other,
                              plan.logical_chunk_bytes + 5, cancel);
        std::swap(current, other);
      } else {
        unshuffle(current, other, bytes, filter.parameter, cancel);
        std::swap(current, other);
      }
      valid(bytes <= plan.logical_chunk_bytes + 65536,
            "HDF5 filter expansion exceeds declared decoded chunk");
    }
    valid(bytes == plan.logical_chunk_bytes,
          "HDF5 decoded chunk size differs from declared native layout");
  }
  // A fixed ring, protected by the retained source lease; no growing chunk set.
  plan.validated[plan.next_slot] = coordinate;
  plan.validated_count =
      std::min(plan.validated.size(), plan.validated_count + 1);
  plan.next_slot = (plan.next_slot + 1) % plan.validated.size();
  poll(cancel);
}
inline void prevalidate(hid_t data, ReadPlan &plan,
                        const std::array<hsize_t, 2> &first,
                        const std::array<hsize_t, 2> &count,
                        const ReadLease &lease, ChunkWorkspace &scratch,
                        qpx_reader_cancel_v1 cancel) {
  if (!plan.chunked)
    return;
  std::array<hsize_t, 2> begin{}, end{};
  for (unsigned axis = 0; axis < plan.dimensions.rank; ++axis) {
    require(first[axis] <= plan.dimensions.extent[axis] &&
                count[axis] <= plan.dimensions.extent[axis] - first[axis],
            "HDF5 selected block exceeds dataset bounds");
    if (!count[axis])
      return;
    begin[axis] = first[axis] / plan.chunk[axis] * plan.chunk[axis];
    end[axis] =
        (first[axis] + count[axis] - 1) / plan.chunk[axis] * plan.chunk[axis];
  }
  if (plan.dimensions.rank == 1) {
    for (auto offset = begin[0];;) {
      prevalidate_chunk(data, plan, {offset, 0}, lease, scratch, cancel);
      if (offset == end[0])
        break;
      offset += plan.chunk[0];
    }
  } else {
    for (auto row = begin[0];;) {
      for (auto column = begin[1];;) {
        prevalidate_chunk(data, plan, {row, column}, lease, scratch, cancel);
        if (column == end[1])
          break;
        column += plan.chunk[1];
      }
      if (row == end[0])
        break;
      row += plan.chunk[0];
    }
  }
}
struct Dataset {
  Handle data;
  ReadPlan plan;
  Dataset() = default;
  Dataset(hid_t file, const std::string &path, std::size_t chunk_limit)
      : data(dataset(file, path)),
        plan(inspect_dataset(file, data, chunk_limit)) {}
  operator hid_t() const noexcept { return data; }
};

enum class NumericKind { Unsigned, Signed, Floating };
inline NumericKind numeric_type(hid_t type) {
  const auto kind = H5Tget_class(type);
  if (kind == H5T_INTEGER) {
    const auto size = H5Tget_size(type);
    const auto sign = H5Tget_sign(type);
    const auto order = H5Tget_order(type);
    valid((size == 1 || size == 2 || size == 4 || size == 8) &&
              H5Tget_precision(type) == size * 8 && H5Tget_offset(type) == 0 &&
              (order == H5T_ORDER_LE || order == H5T_ORDER_BE) &&
              (sign == H5T_SGN_NONE || sign == H5T_SGN_2),
          "HDF5 selected integer dtype is unsupported");
    return sign == H5T_SGN_2 ? NumericKind::Signed : NumericKind::Unsigned;
  }
  valid(kind == H5T_FLOAT && (H5Tequal(type, H5T_IEEE_F16LE) > 0 ||
                              H5Tequal(type, H5T_IEEE_F16BE) > 0 ||
                              H5Tequal(type, H5T_IEEE_F32LE) > 0 ||
                              H5Tequal(type, H5T_IEEE_F32BE) > 0 ||
                              H5Tequal(type, H5T_IEEE_F64LE) > 0 ||
                              H5Tequal(type, H5T_IEEE_F64BE) > 0),
        "HDF5 selected floating dtype is unsupported");
  return NumericKind::Floating;
}
struct Number {
  NumericKind kind;
  std::uint64_t unsigned_value = 0;
  std::int64_t signed_value = 0;
  double floating_value = 0;
};
inline std::uint64_t count(const Number &value) {
  if (value.kind == NumericKind::Unsigned)
    return value.unsigned_value;
  if (value.kind == NumericKind::Signed) {
    valid(value.signed_value >= 0, "HDF5 counts cannot be negative");
    return std::uint64_t(value.signed_value);
  }
  const auto number = value.floating_value;
  valid(std::isfinite(number) && number >= 0 && number < std::ldexp(1., 64) &&
            std::floor(number) == number,
        "HDF5 counts need finite exact nonnegative UInt64 values");
  return std::uint64_t(number);
}
inline double continuous(const Number &value) {
  if (value.kind == NumericKind::Floating) {
    valid(std::isfinite(value.floating_value),
          "HDF5 continuous values must be finite");
    return value.floating_value;
  }
  if (value.kind == NumericKind::Unsigned) {
    const auto number = double(value.unsigned_value);
    valid(number < std::ldexp(1., 64) &&
              std::uint64_t(number) == value.unsigned_value,
          "HDF5 integer to continuous conversion loses precision");
    return number;
  }
  const auto number = double(value.signed_value);
  valid(number >= -std::ldexp(1., 63) && number < std::ldexp(1., 63) &&
            std::int64_t(number) == value.signed_value,
        "HDF5 integer to continuous conversion loses precision");
  return number;
}

template <class Sink>
void numeric_block(Dataset &selected, const ReadLease &lease, ChunkWorkspace &scratch,
                   NumericKind kind, const std::array<hsize_t, 2>& start,
                   const std::array<hsize_t, 2>& size, std::size_t count,
                   Sink &&sink, qpx_reader_cancel_v1 cancel) {
  require(count > 0 && count <= 4096,
          "HDF5 numeric block exceeds bounded scratch");
  poll(cancel);
  const hid_t data = selected;
  prevalidate(data, selected.plan, start, size, lease, scratch, cancel);
  Handle file_space(H5Dget_space(data));
  status(H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start.data(), nullptr, size.data(),
                             nullptr),
         "HDF5 cannot select numeric block");
  const hsize_t length = count;
  Handle memory(H5Screate_simple(1, &length, nullptr));
  if (kind == NumericKind::Floating) {
    std::array<double, 4096> values{};
    status(H5Dread(data, H5T_NATIVE_DOUBLE, memory, file_space, H5P_DEFAULT,
                   values.data()),
           "HDF5 cannot read floating block");
    poll(cancel);
    for (std::size_t index = 0; index < count; ++index)
      sink(index, Number{kind, 0, 0, values[index]});
  } else if (kind == NumericKind::Signed) {
    std::array<std::int64_t, 4096> values{};
    status(H5Dread(data, H5T_NATIVE_INT64, memory, file_space, H5P_DEFAULT,
                   values.data()),
           "HDF5 cannot read signed integer block");
    poll(cancel);
    for (std::size_t index = 0; index < count; ++index)
      sink(index, Number{kind, 0, values[index], 0});
  } else {
    std::array<std::uint64_t, 4096> values{};
    status(H5Dread(data, H5T_NATIVE_UINT64, memory, file_space, H5P_DEFAULT,
                   values.data()),
           "HDF5 cannot read unsigned integer block");
    poll(cancel);
    for (std::size_t index = 0; index < count; ++index)
      sink(index, Number{kind, values[index], 0, 0});
  }
  poll(cancel);
}

template <class Sink>
void numbers(Dataset &selected, const ReadLease &lease, ChunkWorkspace &scratch,
             NumericKind kind, hsize_t first_feature, hsize_t observation,
             std::size_t count, Sink &&sink, qpx_reader_cancel_v1 cancel) {
  numeric_block(selected, lease, scratch, kind, {first_feature, observation},
                {count, 1}, count, std::forward<Sink>(sink), cancel);
}

inline void utf8_string_type(hid_t type) {
  valid(H5Tget_class(type) == H5T_STRING && H5Tis_variable_str(type) > 0 &&
            H5Tget_cset(type) == H5T_CSET_UTF8,
        "HDF5 Loom3 strings require variable-length UTF-8 datasets");
}
struct Strings {
  std::size_t used = 0;
  std::array<void *, string_page> allocations{};
  bool invalid_release = false;
  ~Strings() {
    for (std::size_t index = 0; index < allocations.size(); ++index)
      if (allocations[index])
        release(allocations[index], this);
  }
  static void *allocate(std::size_t size, void *context) noexcept {
    auto &self = *static_cast<Strings *>(context);
    if (!size || size > string_bytes + 1 ||
        size + sizeof(std::size_t) > 128u * 1024 - self.used)
      return nullptr;
    const auto free =
        std::find(self.allocations.begin(), self.allocations.end(), nullptr);
    if (free == self.allocations.end())
      return nullptr;
    auto *data =
        static_cast<std::size_t *>(std::malloc(size + sizeof(std::size_t)));
    if (!data)
      return nullptr;
    *data = size;
    self.used += size + sizeof(std::size_t);
    *free = data + 1;
    return data + 1;
  }
  static void release(void *value, void *context) noexcept {
    if (!value)
      return;
    auto &self = *static_cast<Strings *>(context);
    const auto found =
        std::find(self.allocations.begin(), self.allocations.end(), value);
    if (found == self.allocations.end()) {
      self.invalid_release = true;
      return;
    }
    *found = nullptr;
    auto *data = static_cast<std::size_t *>(value) - 1;
    self.used -= *data + sizeof(std::size_t);
    std::free(data);
  }
  std::string_view view(const char *value) const {
    valid(value != nullptr && std::find(allocations.begin(), allocations.end(),
                                        value) != allocations.end(),
          "HDF5 returned an unowned/null UTF-8 string");
    const auto size = *(reinterpret_cast<const std::size_t *>(value) - 1);
    valid(size > 0 && size <= string_bytes + 1 && value[size - 1] == '\0',
          "HDF5 UTF-8 allocation differs from bounded contract");
    const std::string_view text(value, size - 1);
    valid(text.find('\0') == std::string_view::npos,
          "HDF5 UTF-8 string contains an embedded NUL");
    return text;
  }
};

inline std::string string_attribute(hid_t object, const char* key,
                                    qpx_reader_cancel_v1 cancel) {
  poll(cancel);
  Handle attr(H5Aopen(object, key, H5P_DEFAULT)), space(H5Aget_space(attr)), type(H5Aget_type(attr));
  valid(H5Sget_simple_extent_type(space) == H5S_SCALAR && H5Tget_class(type) == H5T_STRING,
        "HDF5 expected a scalar string attribute");
  if (H5Tis_variable_str(type) > 0) {
    Handle transfer(H5Pcreate(H5P_DATASET_XFER));
    Strings allocator;
    status(H5Pset_vlen_mem_manager(transfer, Strings::allocate, &allocator, Strings::release, &allocator),
           "HDF5 cannot bound attribute strings");
    // The native VOL call takes the transfer property list that H5Aread lacks.
    const auto connector = H5VLget_connector_id(attr);
    valid(connector >= 0, "HDF5 cannot identify native attribute connector");
    struct Connector {
      hid_t id;
      ~Connector() { H5VLclose(id); }
    } release{connector};
    struct Context {
      void* state = nullptr;
      Context() { status(H5VLopen_lib_context(&state), "HDF5 cannot enter native attribute context"); }
      ~Context() { H5VLclose_lib_context(state); }
    } context;
    char* value = nullptr;
    const auto rc = H5VLattr_read(H5VLobject(attr), connector, type, &value, transfer, nullptr);
    if (value && std::find(allocator.allocations.begin(), allocator.allocations.end(), value) == allocator.allocations.end()) {
      H5free_memory(value);
      qpx_binary::fail("HDF5 attribute allocation bypassed the declared bound");
    }
    status(rc, "HDF5 cannot read bounded attribute string");
    const auto text = allocator.view(value);
    qpx_binary::utf8(text, cancel);
    return std::string(text);
  }
  const auto size = H5Tget_size(type);
  valid(size && size <= string_bytes, "HDF5 fixed string attribute exceeds its bound");
  std::array<char, string_bytes> bytes{};
  status(H5Aread(attr, type, bytes.data()), "HDF5 cannot read fixed attribute string");
  const auto end = std::find(bytes.begin(), bytes.begin() + size, '\0');
  std::string value(bytes.begin(), end);
  if (H5Tget_strpad(type) == H5T_STR_SPACEPAD)
    while (!value.empty() && value.back() == ' ') value.pop_back();
  qpx_binary::utf8(value, cancel);
  return value;
}

template <class Sink>
void strings(Dataset &selected, const ReadLease &lease, ChunkWorkspace &scratch,
             std::uint64_t first, std::size_t count, bool scalar, bool empty,
             Sink &&sink, qpx_reader_cancel_v1 cancel) {
  require(count > 0 && count <= string_page && (!scalar || count == 1),
          "HDF5 string page exceeds bounded scratch");
  poll(cancel);
  const hid_t data = selected;
  prevalidate(data, selected.plan, {first, 0}, {count, 1}, lease, scratch,
              cancel);
  Handle type(H5Dget_type(data));
  utf8_string_type(type);
  Handle file_space(H5Dget_space(data));
  const hsize_t start = first, length = count;
  if (!scalar)
    status(H5Sselect_hyperslab(file_space, H5S_SELECT_SET, &start, nullptr,
                               &length, nullptr),
           "HDF5 cannot select string page");
  Handle memory(scalar ? H5Screate(H5S_SCALAR)
                       : H5Screate_simple(1, &length, nullptr));
  Handle transfer(H5Pcreate(H5P_DATASET_XFER));
  Strings allocator;
  status(H5Pset_vlen_mem_manager(transfer, Strings::allocate, &allocator,
                                 Strings::release, &allocator),
         "HDF5 cannot bound UTF-8 string allocations");
  std::array<char *, string_page> values{};
  const auto rc =
      H5Dread(data, type, memory, file_space, transfer, values.data());
  try {
    status(rc, "HDF5 cannot read bounded UTF-8 strings");
    poll(cancel);
    for (std::size_t index = 0; index < count; ++index) {
      const auto text = allocator.view(values[index]);
      valid(empty || !text.empty(), "HDF5 selected ID string is empty");
      qpx_binary::utf8(text, cancel);
      sink(index, text);
    }
  } catch (...) {
    H5Treclaim(type, memory, transfer, values.data());
    throw;
  }
  status(H5Treclaim(type, memory, transfer, values.data()),
         "HDF5 cannot release bounded UTF-8 strings");
  valid(allocator.used == 0 && !allocator.invalid_release,
        "HDF5 UTF-8 allocations were not reclaimed exactly once");
  poll(cancel);
}
} // namespace qpx_hdf5
