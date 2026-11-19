// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/rgba8_reader.hpp"
#include "../image-common/gzip_member.hpp"

#define NOMINMAX
#include <windows.h>
#include <charconv>
#include <optional>
#include <string_view>
#include <utility>

namespace {
using qpx_image_plugin::boundary;
using qpx_image_plugin::Budget;
using qpx_image_plugin::checkpoint;
using qpx_image_plugin::fail;
using qpx_image_plugin::failure;
using qpx_image_plugin::require;
using qpx_image_plugin::text;
using qpx_image_plugin::valid;
using qpx_image_plugin::work_limit;

constexpr std::size_t nifti1_header_size = 348;
constexpr std::size_t nifti2_header_size = 540;
constexpr std::size_t extender_size = 4;
constexpr std::size_t header_limit = 1024 * 1024;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 &&
                  sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "NIfTI floating samples require IEEE-754 binary32 and binary64");

struct Window {
    double low, high;
};
bool digit(char value) { return value >= '0' && value <= '9'; }

double json_number(std::string_view value, std::size_t& position) {
    const auto start = position;
    if (position < value.size() && value[position] == '-')
        ++position;
    require(position < value.size() && digit(value[position]), "NIfTI window requires JSON numbers");
    if (value[position] == '0') {
        ++position;
        require(position == value.size() || !digit(value[position]),
                "NIfTI window number has a leading zero");
    } else {
        while (position < value.size() && digit(value[position]))
            ++position;
    }
    if (position < value.size() && value[position] == '.') {
        const auto first = ++position;
        while (position < value.size() && digit(value[position]))
            ++position;
        require(position > first, "NIfTI window fraction is incomplete");
    }
    if (position < value.size() && (value[position] == 'e' || value[position] == 'E')) {
        ++position;
        if (position < value.size() && (value[position] == '+' || value[position] == '-'))
            ++position;
        const auto first = position;
        while (position < value.size() && digit(value[position]))
            ++position;
        require(position > first, "NIfTI window exponent is incomplete");
    }
    double result = 0;
    const auto parsed = std::from_chars(value.data() + start, value.data() + position, result,
                                        std::chars_format::general);
    require(parsed.ec == std::errc{} && parsed.ptr == value.data() + position &&
                std::isfinite(result),
            "NIfTI window numbers must be finite and representable");
    return result;
}

std::optional<Window> parse_window(qpx_reader_string_v1 options) {
    require(options.size <= 512 && (!options.size || options.data), "NIfTI options are invalid");
    if (!options.size)
        return {};
    const std::string_view spelling(options.data, std::size_t(options.size));
    constexpr std::string_view prefix = "{\"window\":[";
    require(spelling.substr(0, prefix.size()) == prefix, "NIfTI only supports compact window options");
    auto position = prefix.size();
    const auto low = json_number(spelling, position);
    require(position < spelling.size() && spelling[position++] == ',',
            "NIfTI window requires two numbers");
    const auto high = json_number(spelling, position);
    require(spelling.substr(position) == "]}" && high > low && std::isfinite(high - low),
            "NIfTI window requires an increasing finite span");
    return Window{low, high};
}

void support(bool condition, const char* message) {
    if (!condition)
        fail(QPX_READER_UNSUPPORTED, message);
}

struct SampleMode {
    std::uint32_t type;
    std::size_t bytes;
    Window window;
};

template <typename T>
SampleMode integer_mode(std::uint32_t type) {
    return {type, sizeof(T), {static_cast<double>(std::numeric_limits<T>::lowest()),
                              static_cast<double>(std::numeric_limits<T>::max())}};
}

SampleMode sample_mode(std::uint32_t code) {
    switch (code) {
    case 2: return integer_mode<std::uint8_t>(QPX_READER_UINT8);
    case 4: return integer_mode<std::int16_t>(QPX_READER_INT16);
    case 8: return integer_mode<std::int32_t>(QPX_READER_INT32);
    case 16: return {QPX_READER_FLOAT32, 4, {-1, 1}};
    case 64: return {QPX_READER_FLOAT64, 8, {-1, 1}};
    case 256: return integer_mode<std::int8_t>(QPX_READER_INT8);
    case 512: return integer_mode<std::uint16_t>(QPX_READER_UINT16);
    case 768: return integer_mode<std::uint32_t>(QPX_READER_UINT32);
    case 1024: return integer_mode<std::int64_t>(QPX_READER_INT64);
    case 1280: return integer_mode<std::uint64_t>(QPX_READER_UINT64);
    default: fail(QPX_READER_UNSUPPORTED, "NIfTI datatype is not a supported real scalar type");
    }
}

std::uint64_t load_uint(const std::uint8_t* bytes, std::size_t width, bool little) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value = (value << 8) | bytes[little ? width - 1 - i : i];
    return value;
}

struct Header {
    const std::uint8_t* bytes;
    bool little;
    bool wide;

    std::uint64_t field(std::size_t offset, std::size_t width) const {
        return load_uint(bytes + offset, width, little);
    }

    std::uint64_t integer(std::size_t v1, std::size_t v2, std::size_t width1 = 2,
                          std::size_t width2 = 4) const {
        return wide ? field(v2, width2) : field(v1, width1);
    }

    double real(std::size_t v1, std::size_t v2) const {
        if (wide) {
            const auto bits = field(v2, 8);
            valid((bits & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000),
                  "NIfTI active header values must be finite");
            double value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        const auto bits = static_cast<std::uint32_t>(field(v1, 4));
        valid((bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000),
              "NIfTI active header values must be finite");
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
};

struct Raster {
    std::array<std::uint32_t, 4> shape{1, 1, 1, 1};
    std::array<double, 3> spacing{};
    std::uint32_t calibration = 0;
    SampleMode sample{};
    std::uint32_t rank = 2;
    double time_interval = 0;
    bool nifti2 = false;
    bool custom_window = false;
    std::size_t offset = 0;
    std::unique_ptr<std::uint8_t[]> bytes;

    const std::uint8_t* at(std::int64_t x, std::int64_t y, std::uint32_t z, std::uint32_t t) const {
        const auto plane = std::size_t(t) * shape[2] + z;
        const auto index = (plane * shape[1] + std::size_t(y)) * shape[0] + std::size_t(x);
        return bytes.get() + offset + index * sample.bytes;
    }
};

void read_calibration(const Header& header, Raster& result) {
    const auto qform = header.integer(252, 344);
    const auto sform = header.integer(254, 348);
    const auto units = header.integer(123, 500, 1);
    const auto spatial = units & 7;
    const auto temporal = units & ~UINT64_C(7);
    const auto spatial_rank = std::min<std::size_t>(result.rank, 3);
    support(qform <= 2 && sform <= 2 && spatial <= 3,
            "NIfTI requires local spatial transforms and supported spatial units");
    support(!temporal || (result.rank == 4 && (temporal == 8 || temporal == 16 || temporal == 24)),
            "NIfTI temporal units are unsupported for this axis layout");
    if (result.rank == 4) {
        const auto step = header.real(92, 136);
        valid(step > 0, "NIfTI time step must be positive");
        if (temporal) {
            constexpr std::array<double, 4> units_per_second{0, 1, 1000, 1000000};
            result.time_interval = step / units_per_second[temporal / 8];
            valid(result.time_interval > 0 && std::isfinite(result.time_interval),
                  "NIfTI time step is outside the supported seconds range");
            result.calibration |= QPX_READER_CALIBRATION_TIME;
        }
    }

    if (qform) {
        const auto qfac = header.real(76, 104);
        support(qfac == 0 || qfac == 1, "NIfTI qform cannot reflect an axis");
        for (std::size_t i = 0; i < 6; ++i)
            support(header.real(256 + 4 * i, 352 + 8 * i) == 0,
                    "NIfTI qform cannot rotate or translate the volume");
    }
    std::array<double, 3> spacing{};
    if (!sform || qform) {
        const auto axes = qform ? spacing.size() : spatial_rank;
        for (std::size_t axis = 0; axis < axes; ++axis) {
            spacing[axis] = header.real(80 + 4 * axis, 112 + 8 * axis);
            valid(spacing[axis] > 0, "NIfTI voxel spacing must be positive");
        }
    }
    if (sform) {
        for (std::size_t row = 0; row < spacing.size(); ++row) {
            for (std::size_t column = 0; column < 4; ++column) {
                const auto index = 4 * row + column;
                const auto cell = header.real(280 + 4 * index, 400 + 8 * index);
                if (row == column) {
                    valid(cell > 0, "NIfTI sform spacing must be positive");
                    support(!qform || spacing[row] == cell, "NIfTI qform and sform spacing disagree");
                    spacing[row] = cell;
                } else {
                    support(cell == 0, "NIfTI sform cannot rotate, shear or translate the volume");
                }
            }
        }
    }
    if (spatial) {
        constexpr std::array<double, 4> micrometers{0, 1000000, 1000, 1};
        for (std::size_t axis = 0; axis < spatial_rank; ++axis) {
            result.spacing[axis] = spacing[axis] * micrometers[spatial];
            valid(std::isfinite(result.spacing[axis]), "NIfTI converted spacing exceeds binary64");
        }
        result.calibration |= QPX_READER_CALIBRATION_X | QPX_READER_CALIBRATION_Y;
        if (spatial_rank == 3)
            result.calibration |= QPX_READER_CALIBRATION_Z;
    }
}

struct Layout {
    Raster raster;
    std::uint64_t extent;
    bool little;
};

void check_extensions(const std::uint8_t* data, std::size_t size, std::size_t header_size,
                      std::uint64_t end, bool little) {
    const auto first = header_size + extender_size;
    valid(size >= first, "NIfTI extender is truncated");
    support(data[header_size + 1] == 0 && data[header_size + 2] == 0 && data[header_size + 3] == 0,
            "NIfTI uses reserved extender bytes");
    if (data[header_size] == 0) {
        support(end == first, "NIfTI without extensions requires the default header extent");
        return;
    }
    valid(end > first && end <= size && end <= header_limit,
          "NIfTI extension area is empty, truncated or outside its bound");
    for (auto at = first; at < end;) {
        valid(end - at >= 16, "NIfTI extension record is truncated");
        const auto length = load_uint(data + at, 4, little);
        valid(length >= 16 && length % 16 == 0 && length <= end - at,
              "NIfTI extension length is invalid");
        support(load_uint(data + at + 4, 4, little) == 6, "NIfTI extension code is outside the comment profile");
        bool padding = false;
        for (auto i = at + 8; i < at + length; ++i) {
            if (data[i] == 0) padding = true;
            else valid(!padding && data[i] < 128, "NIfTI comment extension requires ASCII and zero padding");
        }
        at += static_cast<std::size_t>(length);
    }
}

Layout parse_header(const std::uint8_t* data, std::size_t size, bool paired,
                    const std::optional<Window>& window) {
    valid(size >= nifti1_header_size, "NIfTI header is truncated");
    const auto little_size = load_uint(data, 4, true);
    const bool little = little_size == nifti1_header_size || little_size == nifti2_header_size;
    const auto header_size = little ? little_size : load_uint(data, 4, false);
    const bool wide = header_size == nifti2_header_size;
    valid(wide || header_size == nifti1_header_size, "NIfTI sizeof_hdr must be 348 or 540");
    const auto first_voxel = header_size + extender_size;
    valid(size >= (paired ? header_size : first_voxel), "NIfTI header is truncated");
    const auto* magic = wide ? (paired ? "ni2\0\r\n\x1a\n" : "n+2\0\r\n\x1a\n")
                             : (paired ? "ni1" : "n+1");
    support(std::memcmp(data + (wide ? 4 : 344), magic, wide ? 8 : 4) == 0,
            "NIfTI header magic differs from the selected layout and version");
    const Header header{data, little, wide};
    std::uint64_t offset = 0;
    if (wide) {
        offset = header.field(168, 8);
        valid(offset <= work_limit, "NIfTI-2 voxel offset is negative or exceeds the supported limit");
    } else {
        const double raw_offset = header.real(108, 168);
        valid(raw_offset >= 0 && raw_offset <= work_limit && std::floor(raw_offset) == raw_offset,
              "NIfTI-1 requires a bounded nonnegative integral voxel offset");
        offset = static_cast<std::uint64_t>(raw_offset);
    }
    if (!paired || size != header_size)
        check_extensions(data, size, static_cast<std::size_t>(header_size), paired ? size : offset, little);

    const auto rank = header.integer(40, 16, 2, 8);
    support(rank >= 2 && rank <= 4, "NIfTI requires an XY, XYZ or XYZT scalar image");
    Raster result;
    result.rank = static_cast<std::uint32_t>(rank);
    result.nifti2 = wide;
    result.offset = static_cast<std::size_t>(offset);
    auto voxels = UINT64_C(1);
    for (std::size_t axis = 0; axis < rank; ++axis) {
        const auto dimension = header.integer(42 + 2 * axis, 24 + 8 * axis, 2, 8);
        valid(dimension && dimension <= (wide ? UINT32_MAX : INT16_MAX),
              "NIfTI active dimensions must be positive and representable");
        require(voxels <= work_limit / dimension, "NIfTI sample count exceeds the work limit");
        result.shape[axis] = static_cast<std::uint32_t>(dimension);
        voxels *= dimension;
    }
    result.sample = sample_mode(static_cast<std::uint32_t>(header.integer(70, 12, 2, 2)));
    valid(header.integer(72, 14, 2, 2) == 8 * result.sample.bytes, "NIfTI bitpix does not match its datatype");
    require(voxels <= (work_limit - offset) / result.sample.bytes, "NIfTI voxel extent exceeds the work limit");

    const auto slope = header.real(112, 176);
    support(slope == 0 || (slope == 1 && header.real(116, 184) == 0),
            "NIfTI has unsupported intensity scaling");
    support(header.integer(68, 504) == 0 && header.real(56, 80) == 0 && header.real(60, 88) == 0 &&
                header.real(64, 96) == 0 && header.integer(39, 524, 1, 1) == 0 &&
                header.integer(74, 224, 2, 8) == 0 && header.integer(120, 232, 2, 8) == 0 &&
                header.integer(122, 496, 1) == 0 && header.real(132, 208) == 0 && header.real(136, 216) == 0,
            "NIfTI has unsupported intent, slice or time metadata");
    read_calibration(header, result);
    if (window) {
        result.sample.window = *window;
        result.custom_window = true;
    }

    const auto extent = result.offset + voxels * result.sample.bytes;
    return {std::move(result), extent, header.little};
}

Raster finish_raster(Layout layout, std::unique_ptr<std::uint8_t[]> bytes, std::size_t size,
                     qpx_reader_cancel_v1 cancel) {
    valid(size == layout.extent, "NIfTI raster has an incorrect byte extent");
    auto& result = layout.raster;
    const auto step = result.sample.bytes;
    const std::uint16_t marker = 1;
    const bool native_little = *reinterpret_cast<const unsigned char*>(&marker) == 1;
    if (step > 1 && layout.little != native_little) {
        for (std::size_t offset = result.offset; offset < size; offset += step) {
            if ((offset - result.offset) % 65536 == 0)
                checkpoint(cancel);
            std::reverse(bytes.get() + offset, bytes.get() + offset + step);
        }
    }
    checkpoint(cancel);
    result.bytes = std::move(bytes);
    return std::move(result);
}

struct FileBytes {
    std::unique_ptr<std::uint8_t[]> data;
    std::size_t size;
};

struct CloseFile {
    void operator()(void* value) const noexcept { CloseHandle(value); }
};
using FileHandle = std::unique_ptr<void, CloseFile>;

FileHandle open_file(const std::filesystem::path& path, bool paired, std::uint64_t& size) {
    const auto flags = FILE_FLAG_BACKUP_SEMANTICS | (paired ? FILE_FLAG_OPEN_REPARSE_POINT : 0);
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, flags, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        fail(QPX_READER_IO_ERROR, "Cannot open NIfTI source file");
    FileHandle guard(file);
    BY_HANDLE_FILE_INFORMATION info{};
    valid(GetFileType(file) == FILE_TYPE_DISK && GetFileInformationByHandle(file, &info) &&
              !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY),
          "NIfTI source must be a regular file");
    support(!paired || (!(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && info.nNumberOfLinks == 1),
            "NIfTI pair members cannot be links or reparse points");
    size = (std::uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    return guard;
}

FileBytes read_bytes(const std::filesystem::path& path, bool paired, Budget& budget,
                     qpx_reader_cancel_v1 cancel, std::uint64_t limit = work_limit) {
    std::uint64_t size = 0;
    const auto file = open_file(path, paired, size);
    valid(size && size <= limit, "NIfTI file extent is outside the supported limits");
    FileBytes out{budget.array(size), static_cast<std::size_t>(size)};
    for (std::size_t offset = 0; offset < size;) {
        checkpoint(cancel);
        const auto count = static_cast<DWORD>(std::min<std::uint64_t>(65536, size - offset));
        DWORD received = 0;
        valid(ReadFile(file.get(), out.data.get() + offset, count, &received, nullptr) && received == count,
              "Cannot read complete NIfTI input");
        offset += count;
    }
    return out;
}

FileBytes inflate_file(FileBytes input, Budget& budget, std::uint64_t min_size,
                       std::uint64_t max_size, qpx_reader_cancel_v1 cancel) {
    using namespace qpx::image_plugin;
    valid(input.size >= 18, "NIfTI gzip stream is truncated");
    const auto decoded_size = load_uint(input.data.get() + input.size - 4, 4, true);
    valid(decoded_size >= min_size && decoded_size <= max_size,
          "NIfTI gzip decoded extent differs from the expected size");
    require(budget.remaining >= gzip_workspace_bytes, "NIfTI gzip workspace exceeds the work budget");
    budget.remaining -= gzip_workspace_bytes;
    auto decoded = budget.array(decoded_size);
    const auto status = decode_gzip_member(input.data.get(), input.size, decoded.get(), decoded_size,
                                           [&] { checkpoint(cancel); });
    if (status == GzipResult::workspace_failure)
        fail(QPX_READER_INTERNAL_ERROR, "NIfTI could not initialize or allocate gzip workspace");
    valid(status == GzipResult::ok, "NIfTI requires one complete gzip member with exact extent");
    budget.remaining += input.size + gzip_workspace_bytes;
    return {std::move(decoded), decoded_size};
}

struct Pair {
    std::filesystem::path image;
    bool header_gzip;
};

struct PairPayload {
    std::filesystem::path path;
    bool gzip;
};

std::optional<Pair> find_pair(const std::filesystem::path& path) {
    const auto name = path.filename().wstring();
    auto suffix = name;
    for (auto& character : suffix)
        if (character >= L'A' && character <= L'Z')
            character += L'a' - L'A';
    if (suffix.size() > 7 && suffix.compare(suffix.size() - 7, 7, L".hdr.gz") == 0)
        return Pair{path.parent_path() / (name.substr(0, name.size() - 7) + L".img"), true};
    if (suffix.size() > 4 && suffix.compare(suffix.size() - 4, 4, L".hdr") == 0)
        return Pair{path.parent_path() / (name.substr(0, name.size() - 4) + L".img"), false};
    return {};
}

PairPayload select_pair_payload(const Pair& pair) {
    auto gzip = pair.image;
    gzip += L".gz";
    const auto& preferred = pair.header_gzip ? gzip : pair.image;
    // Preserve matching-compression selection when both candidates exist.
    if (GetFileAttributesW(preferred.c_str()) != INVALID_FILE_ATTRIBUTES)
        return {preferred, pair.header_gzip};
    if (GetLastError() != ERROR_FILE_NOT_FOUND)
        fail(QPX_READER_IO_ERROR, "Cannot inspect the preferred NIfTI companion");
    return {pair.header_gzip ? pair.image : gzip, !pair.header_gzip};
}

Layout pair_layout(const std::filesystem::path& path, const Pair& pair, Budget& budget,
                   const std::optional<Window>& window, qpx_reader_cancel_v1 cancel) {
    auto header = read_bytes(path, true, budget, cancel,
                             pair.header_gzip ? header_limit + 65536 : header_limit);
    if (pair.header_gzip)
        header = inflate_file(std::move(header), budget, nifti1_header_size, header_limit, cancel);
    auto layout = parse_header(header.data.get(), header.size, true, window);
    budget.remaining += header.size;
    return layout;
}

Raster read_file(const std::filesystem::path& path, Budget& budget,
                 const std::optional<Window>& window, qpx_reader_cancel_v1 cancel) {
    if (const auto pair = find_pair(path)) {
        auto layout = pair_layout(path, *pair, budget, window, cancel);
        require(layout.extent <= budget.remaining, "NIfTI pair raster exceeds the work budget");
        const auto companion = select_pair_payload(*pair);
        auto payload = read_bytes(companion.path, true, budget, cancel,
                                  companion.gzip ? work_limit : layout.extent);
        if (companion.gzip)
            payload = inflate_file(std::move(payload), budget, layout.extent, layout.extent, cancel);
        return finish_raster(std::move(layout), std::move(payload.data), payload.size, cancel);
    }
    auto input = read_bytes(path, false, budget, cancel);
    if (input.size >= 2 && input.data[0] == 0x1f && input.data[1] == 0x8b)
        input = inflate_file(std::move(input), budget, nifti1_header_size + extender_size, UINT32_MAX, cancel);
    auto layout = parse_header(input.data.get(), input.size, false, window);
    return finish_raster(std::move(layout), std::move(input.data), input.size, cancel);
}

const Raster& image(void* value) {
    require(value != nullptr, "NIfTI image handle is null");
    return *static_cast<Raster*>(value);
}

qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out,
                                               qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size >= sizeof(*out), "NIfTI reader info structure is too small");
        out->abi_version = QPX_READER_ABI_V1;
        out->capabilities = QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW | QPX_READER_CAP_SOURCE_FILES;
        bool complete = text(out->provider_id, "org.qupathxx.nifti");
        complete = text(out->display_name, QPX_NIFTI_NAME) && complete;
        complete = text(out->version, QPX_NIFTI_VERSION) && complete;
        complete = text(out->contract, QPX_NIFTI_CONTRACT) && complete;
        return complete ? QPX_READER_OK
                        : failure(error, QPX_READER_BUFFER_TOO_SMALL, "NIfTI descriptor text buffer is too small");
    });
}

struct Prepared {
    std::filesystem::path path;
    Budget budget;
    std::optional<Window> window;
};

Prepared prepare(const qpx_reader_open_v1* request) {
    require(request && request->struct_size >= sizeof(*request) && !request->reserved,
            "NIfTI open structure is incomplete or reserved field is set");
    require(request->path.data && request->path.size && request->path.size <= 32768,
            "NIfTI path is invalid");
    require(request->work_bytes && request->work_bytes <= work_limit,
            "NIfTI work budget must be between 1 and 256 MiB");
    const auto overhead = UINT64_C(8192) + 16 * request->path.size + sizeof(Raster);
    require(request->work_bytes > overhead + nifti1_header_size + extender_size,
            "NIfTI work budget is too small");
    checkpoint(request->cancel);
    const auto window = parse_window(request->options);
    const std::string spelling(request->path.data, std::size_t(request->path.size));
    require(spelling.find('\0') == std::string::npos, "NIfTI path contains NUL");
    auto path = std::filesystem::u8path(spelling);
    require(path.is_absolute(), "NIfTI path must be absolute");
    return {std::move(path), {request->work_bytes - overhead}, window};
}

qpx_reader_status_v1 QPX_READER_CALL source_count(const qpx_reader_open_v1* request,
    std::uint32_t* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out, "NIfTI file count output is null");
        *out = 0;
        auto input = prepare(request);
        if (const auto pair = find_pair(input.path)) {
            pair_layout(input.path, *pair, input.budget, input.window, request->cancel);
            *out = 1;
        }
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request,
    std::uint32_t index, qpx_reader_text_v1* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && index == 0, "NIfTI companion index or output is invalid");
        auto input = prepare(request);
        const auto pair = find_pair(input.path);
        require(pair.has_value(), "NIfTI single-file image has no companion");
        pair_layout(input.path, *pair, input.budget, input.window, request->cancel);
        checkpoint(request->cancel);
        std::uint64_t size = 0;
        const auto companion = select_pair_payload(*pair);
        const auto file = open_file(companion.path, true, size);
        const auto name = companion.path.u8string();
        require(name.size() <= 32768, "NIfTI companion path exceeds the reader limit");
        return text(*out, name.c_str()) ? QPX_READER_OK
            : failure(error, QPX_READER_BUFFER_TOO_SMALL, "NIfTI companion text buffer is too small");
    });
}

qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request, void** out,
                                           qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out != nullptr, "NIfTI output handle is null");
        *out = nullptr;
        auto input = prepare(request);
        auto result = read_file(input.path, input.budget, input.window, request->cancel);
        *out = new Raster(std::move(result));
        return QPX_READER_OK;
    });
}

void QPX_READER_CALL destroy(void* value) noexcept { delete static_cast<Raster*>(value); }

qpx_reader_status_v1 QPX_READER_CALL metadata(void* value, qpx_reader_metadata_v1* out,
                                               qpx_reader_cancel_v1 cancel,
                                               qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        const auto& source = image(value);
        require(out && out->struct_size >= sizeof(*out), "NIfTI metadata structure is too small");
        checkpoint(cancel);
        out->level_count = out->has_raw = out->channels = out->series_count = 1;
        out->width = source.shape[0];
        out->height = source.shape[1];
        out->sample_type = source.sample.type;
        out->z_planes = source.shape[2];
        out->timepoints = source.shape[3];
        out->series = 0;
        out->calibration_flags = source.calibration;
        out->pixel_width_um = source.spacing[0];
        out->pixel_height_um = source.spacing[1];
        out->z_spacing_um = source.spacing[2];
        out->time_interval_seconds = source.time_interval;
        bool complete = text(out->vendor, source.nifti2 ? "NIfTI-2" : "NIfTI-1");
        complete = text(out->dimension_order, source.rank == 4 ? "XYZT" : source.rank == 3 ? "XYZ" : "XY") && complete;
        return complete ? QPX_READER_OK
                        : failure(error, QPX_READER_BUFFER_TOO_SMALL, "NIfTI metadata text buffer is too small");
    });
}

qpx_reader_status_v1 QPX_READER_CALL level(void* value, std::uint32_t index, qpx_reader_level_v1* out,
                                            qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        const auto& source = image(value);
        require(out && out->struct_size >= sizeof(*out) && index == 0,
                "NIfTI requires level zero and a complete level descriptor");
        out->reserved = 0;
        out->width = source.shape[0];
        out->height = source.shape[1];
        out->downsample = 1;
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value, std::uint32_t channel,
                                                   qpx_reader_text_v1* out,
                                                   qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        (void)image(value);
        require(out && channel == 0, "NIfTI has one scalar channel");
        return text(*out, "Scalar") ? QPX_READER_OK
                                    : failure(error, QPX_READER_BUFFER_TOO_SMALL,
                                              "NIfTI channel text buffer is too small");
    });
}

template <typename T> T sample_value(const std::uint8_t* bytes) {
    T value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

enum class FloatClass { finite, infinity, nan };

FloatClass float_class(std::uint64_t bits, std::uint64_t exponent, std::uint64_t fraction) {
    if ((bits & exponent) != exponent)
        return FloatClass::finite;
    return (bits & fraction) ? FloatClass::nan : FloatClass::infinity;
}

FloatClass float_class(const Raster& raster, const std::uint8_t* sample) {
    if (raster.sample.type == QPX_READER_FLOAT32)
        return float_class(sample_value<std::uint32_t>(sample), UINT32_C(0x7f800000),
                           UINT32_C(0x007fffff));
    if (raster.sample.type == QPX_READER_FLOAT64)
        return float_class(sample_value<std::uint64_t>(sample), UINT64_C(0x7ff0000000000000),
                           UINT64_C(0x000fffffffffffff));
    return FloatClass::finite;
}

std::uint8_t display_gray(const Raster& raster, const std::uint8_t* sample) {
    if (!raster.custom_window &&
        (raster.sample.type == QPX_READER_INT64 || raster.sample.type == QPX_READER_UINT64)) {
        auto code = sample_value<std::uint64_t>(sample);
        if (raster.sample.type == QPX_READER_INT64)
            code ^= UINT64_C(1) << 63;
        // UINT64_MAX is divisible by 255, so the rounded scale needs no wider intermediate.
        constexpr auto bin = UINT64_MAX / 255;
        return static_cast<std::uint8_t>(code / bin + (code % bin > bin / 2));
    }
    double value = 0;
    switch (raster.sample.type) {
    case QPX_READER_INT8: value = sample_value<std::int8_t>(sample); break;
    case QPX_READER_UINT8: value = sample_value<std::uint8_t>(sample); break;
    case QPX_READER_INT16: value = sample_value<std::int16_t>(sample); break;
    case QPX_READER_UINT16: value = sample_value<std::uint16_t>(sample); break;
    case QPX_READER_INT32: value = sample_value<std::int32_t>(sample); break;
    case QPX_READER_UINT32: value = sample_value<std::uint32_t>(sample); break;
    case QPX_READER_INT64: value = static_cast<double>(sample_value<std::int64_t>(sample)); break;
    case QPX_READER_UINT64: value = static_cast<double>(sample_value<std::uint64_t>(sample)); break;
    case QPX_READER_FLOAT32: value = sample_value<float>(sample); break;
    case QPX_READER_FLOAT64: value = sample_value<double>(sample); break;
    }
    const auto window = raster.sample.window;
    const auto normalized = value <= window.low ? 0.0
                          : value >= window.high ? 1.0
                          : (value - window.low) / (window.high - window.low);
    return std::uint8_t(std::floor(normalized * 255 + 0.5));
}

qpx_reader_status_v1 read_pixels(const Raster& source, const qpx_reader_region_v1* request,
                                qpx_reader_pixels_v1* out, bool raw, qpx_reader_cancel_v1 cancel) {
    require(out && out->struct_size >= sizeof(*out), "NIfTI pixel structure is too small");
    out->written = out->validity_written = 0;
    require(request && request->struct_size >= sizeof(*request), "NIfTI region structure is too small");
    require(!request->level && !request->first_channel &&
                request->channels == (raw ? 1u : 0u) &&
                request->z < (raw ? source.shape[2] : 1) && request->t < (raw ? source.shape[3] : 1),
            "NIfTI region selectors are outside the supported channel or plane");
    const auto width = request->width;
    const auto height = request->height;
    const auto channels = raw ? 1u : 4u;
    const auto channel_stride = raw ? source.sample.bytes : 1u;
    const auto stride = channels * channel_stride;
    const auto per_pixel = stride + (raw ? 1u : 0u);
    require(width > 0 && height > 0 && std::uint64_t(width) <= work_limit / per_pixel &&
                std::uint64_t(height) <= work_limit / per_pixel / std::uint64_t(width),
            "NIfTI pixel output exceeds the 256 MiB work limit");
    const auto limit = std::numeric_limits<std::int64_t>::max();
    require(request->x <= limit - width && request->y <= limit - height,
            "NIfTI region coordinate overflow");
    const auto pixels = std::uint64_t(width) * std::uint64_t(height);
    const auto bytes = pixels * stride;
    require(out->data && out->capacity >= bytes && !out->reserved &&
                out->sample_type == (raw ? source.sample.type : QPX_READER_UINT8) &&
                out->width == width && out->height == height && out->channels == channels &&
                out->row_stride == std::uint64_t(width) * stride && out->pixel_stride == stride &&
                out->channel_stride == channel_stride &&
                (raw ? out->validity && out->validity_capacity >= pixels
                     : !out->validity && !out->validity_capacity),
            "NIfTI output must have valid buffers and packed pixel strides");
    checkpoint(cancel);
    std::memset(out->data, 0, std::size_t(bytes));
    if (raw)
        std::memset(out->validity, 0, std::size_t(pixels));
    for (std::int64_t row = 0; row < height; ++row) {
        checkpoint(cancel);
        for (std::int64_t column = 0; column < width; ++column) {
            if ((column & 4095) == 0)
                checkpoint(cancel);
            const auto x = request->x + column;
            const auto y = request->y + row;
            if (x < 0 || y < 0 || x >= std::int64_t(source.shape[0]) ||
                y >= std::int64_t(source.shape[1]))
                continue;
            const auto index = std::size_t(row) * std::size_t(width) + std::size_t(column);
            const auto* sample = source.at(x, y, request->z, request->t);
            auto* target = out->data + index * stride;
            if (raw) {
                std::memcpy(target, sample, stride);
                out->validity[index] = 1;
            } else if (float_class(source, sample) == FloatClass::finite) {
                target[0] = target[1] = target[2] = display_gray(source, sample);
                target[3] = 255;
            }
        }
    }
    checkpoint(cancel);
    out->written = bytes;
    out->validity_written = raw ? pixels : 0;
    return QPX_READER_OK;
}

qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value, const qpx_reader_region_v1* request,
                                                qpx_reader_pixels_v1* out,
                                                qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] { return read_pixels(image(value), request, out, false, {}); });
}

qpx_reader_status_v1 QPX_READER_CALL read_raw(void* value, const qpx_reader_region_v1* request,
                                               qpx_reader_pixels_v1* out, qpx_reader_cancel_v1 cancel,
                                               qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] { return read_pixels(image(value), request, out, true, cancel); });
}
}  // namespace

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(requested_abi == QPX_READER_ABI_V1 && out && out->struct_size >= sizeof(*out),
                "Unsupported NIfTI ABI or incomplete API structure");
        *out = {sizeof(*out), QPX_READER_ABI_V1, describe, open, destroy, metadata, level,
                channel_name, read_rgba, read_raw};
        return QPX_READER_OK;
    });
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(
    std::uint32_t requested_abi, qpx_reader_files_api_v1* out, qpx_reader_text_v1* error) {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(requested_abi == QPX_READER_ABI_V1 && out && out->struct_size >= sizeof(*out),
                "Unsupported NIfTI file-set ABI or incomplete API structure");
        *out = {sizeof(*out), QPX_READER_ABI_V1, source_count, source_path};
        return QPX_READER_OK;
    });
}
