// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/gzip_member.hpp"
#include "../image-common/rgba8_reader.hpp"

#include <bzlib.h>
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

constexpr std::size_t header_size = 1024;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "MRC mode 2 requires IEEE-754 binary32");
static_assert(std::numeric_limits<std::int8_t>::min() == -128 &&
                  std::numeric_limits<std::int16_t>::min() == -32768,
              "MRC signed modes require two's-complement integers");

struct Window {
    double low, high;
};

bool digit(char value) { return value >= '0' && value <= '9'; }

double json_number(std::string_view value, std::size_t& position) {
    const auto start = position;
    if (position < value.size() && value[position] == '-')
        ++position;
    require(position < value.size() && digit(value[position]), "MRC window requires JSON numbers");
    if (value[position] == '0') {
        ++position;
        require(position == value.size() || !digit(value[position]),
                "MRC window number has a leading zero");
    } else {
        while (position < value.size() && digit(value[position]))
            ++position;
    }
    if (position < value.size() && value[position] == '.') {
        const auto first = ++position;
        while (position < value.size() && digit(value[position]))
            ++position;
        require(position > first, "MRC window fraction is incomplete");
    }
    if (position < value.size() && (value[position] == 'e' || value[position] == 'E')) {
        ++position;
        if (position < value.size() && (value[position] == '+' || value[position] == '-'))
            ++position;
        const auto first = position;
        while (position < value.size() && digit(value[position]))
            ++position;
        require(position > first, "MRC window exponent is incomplete");
    }
    double result = 0;
    const auto parsed = std::from_chars(value.data() + start, value.data() + position, result,
                                        std::chars_format::general);
    require(parsed.ec == std::errc{} && parsed.ptr == value.data() + position &&
                std::isfinite(result),
            "MRC window numbers must be finite and representable");
    return result;
}

struct OpenOptions {
    std::uint32_t series = 0;
    std::optional<Window> window;
};

OpenOptions parse_options(qpx_reader_string_v1 options) {
    require(options.size <= 512 && (!options.size || options.data), "MRC options are invalid");
    OpenOptions result;
    if (!options.size)
        return result;
    std::string_view value(options.data, std::size_t(options.size));
    require(value.front() == '{', "MRC options require a compact object");
    value.remove_prefix(1);
    constexpr std::string_view series_key = "\"series\":";
    if (value.substr(0, series_key.size()) == series_key) {
        value.remove_prefix(series_key.size());
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result.series);
        require(parsed.ec == std::errc{} && !(parsed.ptr - value.data() > 1 && value.front() == '0'),
                "MRC series requires an unsigned decimal index");
        value.remove_prefix(std::size_t(parsed.ptr - value.data()));
        if (value == "}")
            return result;
        require(!value.empty() && value.front() == ',', "MRC options have unexpected or trailing fields");
        value.remove_prefix(1);
    }
    constexpr std::string_view window_key = "\"window\":[";
    require(value.substr(0, window_key.size()) == window_key, "MRC options require series or window");
    auto position = window_key.size();
    const auto low = json_number(value, position);
    require(position < value.size() && value[position++] == ',', "MRC window requires two numbers");
    const auto high = json_number(value, position);
    require(value.substr(position) == "]}", "MRC window has unexpected or trailing fields");
    require(low < high && std::isfinite(high - low), "MRC window must have a finite positive span");
    result.window = Window{low, high};
    return result;
}

struct Header {
    const std::array<std::uint8_t, header_size>& bytes;
    bool big_endian;

    std::uint32_t word(std::size_t offset) const {
        std::uint32_t result = 0;
        for (std::size_t byte = 0; byte < 4; ++byte) {
            const auto index = big_endian ? byte : 3 - byte;
            result = (result << 8) | bytes[offset + index];
        }
        return result;
    }

    float finite_float(std::size_t offset) const {
        const auto bits = word(offset);
        valid((bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000),
              "MRC spatial header float is nonfinite");
        float result = 0;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
};

struct SampleMode {
    std::uint32_t type, bytes;
    Window window;
    bool half = false;
    bool packed4 = false;

    unsigned raw_bytes() const { return half ? 4 : bytes; }
};

SampleMode select_mode(std::uint32_t mode) {
    switch (mode) {
    case 0: return {QPX_READER_INT8, 1, {-128, 127}};
    case 1: return {QPX_READER_INT16, 2, {-32768, 32767}};
    case 2: return {QPX_READER_FLOAT32, 4, {-1, 1}};
    case 6: return {QPX_READER_UINT16, 2, {0, 65535}};
    case 12: return {QPX_READER_FLOAT32, 2, {-1, 1}, true};
    case 101: return {QPX_READER_UINT8, 1, {0, 15}, false, true};
    default: fail(QPX_READER_UNSUPPORTED, "MRC mode is not a supported real scalar type");
    }
}

struct Raster {
    std::array<std::uint32_t, 3> shape{};
    std::array<std::size_t, 3> strides{};
    std::array<double, 3> spacing{};
    std::uint32_t calibration = 0;
    std::uint32_t series = 0, series_count = 1;
    bool volume = false;
    SampleMode sample{};
    std::unique_ptr<std::uint8_t[]> pixels;
    std::size_t raster_offset = 0;
    std::size_t first_sample = 0;

    std::size_t sample_index(std::int64_t x, std::int64_t y, std::uint32_t z) const {
        return first_sample + std::size_t(x) * strides[0] + std::size_t(y) * strides[1] +
               std::size_t(z) * strides[2];
    }
};

Header inspect_header(const std::array<std::uint8_t, header_size>& bytes) {
    if (std::memcmp(bytes.data() + 208, "MAP ", 4))
        fail(QPX_READER_UNSUPPORTED, "MRC MAP signature is missing");
    const auto* stamp = bytes.data() + 212;
    const bool little = stamp[0] == 0x44 && (stamp[1] == 0x44 || stamp[1] == 0x41);
    const bool big = stamp[0] == 0x11 && stamp[1] == 0x11;
    valid((little || big) && stamp[2] == 0 && stamp[3] == 0, "MRC machine stamp is invalid");
    const Header header{bytes, big};
    const auto version = header.word(108);
    if (version != 20140 && version != 20141)
        fail(QPX_READER_UNSUPPORTED, "MRC requires version 20140 or 20141");
    return header;
}

Raster decode_header(const Header& header, const OpenOptions& options,
                     std::uint64_t& raster_bytes) {
    Raster raster;
    raster.sample = select_mode(header.word(12));
    if (options.window)
        raster.sample.window = *options.window;
    if (raster.sample.packed4 && (header.word(0) & 1u))
        fail(QPX_READER_UNSUPPORTED, "MRC mode 101 requires even storage columns");

    const auto extension_size = header.word(92);
    valid(extension_size <= INT32_MAX, "MRC extended-header length is negative");
    if (extension_size || header.word(104))
        fail(QPX_READER_UNSUPPORTED, "MRC extended headers are outside the scalar profile");

    const auto group = header.word(88);
    if (group != 0 && group != 1 && group != 401)
        fail(QPX_READER_UNSUPPORTED, "MRC requires an image, a volume, or a scalar P1 stack");
    raster.volume = group != 0;

    std::uint64_t count = 1;
    unsigned seen = 0;
    for (unsigned storage_axis = 0; storage_axis < 3; ++storage_axis) {
        const auto extent = header.word(4 * storage_axis);
        valid(extent && extent <= INT32_MAX, "MRC storage dimensions must be positive");
        const auto axis = header.word(64 + 4 * storage_axis);
        valid(axis >= 1 && axis <= 3 && !(seen & (1u << (axis - 1))),
              "MRC axis mapping must be a permutation of X, Y, Z");
        seen |= 1u << (axis - 1);
        raster.shape[axis - 1] = extent;
        raster.strides[axis - 1] = std::size_t(count);
        require(extent <= work_limit / raster.sample.bytes / count,
                "MRC raster exceeds the 256 MiB work limit");
        count *= extent;
    }
    if (group != 1) {
        if (header.word(72) != 3)
            fail(QPX_READER_UNSUPPORTED, "MRC image and volume stacks require sections along Z");
        const auto total_sections = raster.shape[2];
        const auto sections_per_series = raster.volume ? header.word(36) : 1;
        valid(sections_per_series && total_sections % sections_per_series == 0,
              "MRC volume stack requires NZ to be divisible by MZ");
        raster.shape[2] = sections_per_series;
        raster.series_count = total_sections / sections_per_series;
    }
    require(options.series < raster.series_count, "MRC series index is outside the stack");
    raster.series = options.series;
    raster.first_sample = std::size_t(count / raster.series_count) * raster.series;

    for (unsigned axis = 0; axis < 3; ++axis) {
        const auto interval_count = header.word(28 + 4 * axis);
        valid(interval_count && interval_count <= INT32_MAX, "MRC sampling grid must be positive");
        if (!raster.volume && axis == 2 && interval_count != 1)
            fail(QPX_READER_UNSUPPORTED, "MRC XY image series requires one Z sampling interval");
        const auto length = header.finite_float(40 + 4 * axis);
        valid(length >= 0, "MRC cell length must be nonnegative");
        if (header.finite_float(52 + 4 * axis) != 90)
            fail(QPX_READER_UNSUPPORTED, "MRC cell must be orthogonal");
        if (header.word(16 + 4 * axis) || header.finite_float(196 + 4 * axis) != 0)
            fail(QPX_READER_UNSUPPORTED, "MRC starts and origin must be zero");
        if (length != 0 && (axis < 2 || raster.volume)) {
            raster.spacing[axis] = double(length) / interval_count * 0.0001;
            raster.calibration |= 1u << axis;
        }
    }
    raster_bytes = raster.sample.packed4 ? count / 2 : count * raster.sample.bytes;
    return raster;
}

constexpr std::size_t io_step = 65536;

void read_bytes(std::FILE* file, std::uint8_t* bytes, std::size_t size,
                qpx_reader_cancel_v1 cancel, const char* error) {
    for (std::size_t pos = 0; pos < size;) {
        checkpoint(cancel);
        const auto count = std::min(io_step, size - pos);
        valid(std::fread(bytes + pos, 1, count, file) == count, error);
        pos += count;
    }
}

void check_eof(std::FILE* file) {
    valid(std::fgetc(file) == EOF && !std::ferror(file), "MRC input changed during reading");
}

std::unique_ptr<std::uint8_t[]> read_encoded(std::FILE* file, Budget& budget,
    std::size_t size, const std::uint8_t* prefix, qpx_reader_cancel_v1 cancel) {
    auto encoded = budget.array(size);
    std::copy_n(prefix, 2, encoded.get());
    read_bytes(file, encoded.get() + 2, size - 2, cancel, "MRC compressed input is truncated or changed");
    check_eof(file);
    return encoded;
}

class Bzip2Input {
public:
    static constexpr std::size_t workspace = 3 * 1024 * 1024;

    Bzip2Input(std::uint8_t* input, std::size_t size, qpx_reader_cancel_v1 cancel)
        : input_(input), size_(size), cancel_(cancel) {
        decoder_.opaque = &available_;
        decoder_.bzalloc = [](void* opaque, int count, int width) -> void* {
            auto& available = *static_cast<std::size_t*>(opaque);
            if (width <= 0 || count <= 0 || std::size_t(count) > available / std::size_t(width))
                return nullptr;
            const auto bytes = std::size_t(count) * std::size_t(width);
            auto* memory = std::malloc(bytes);
            if (memory)
                available -= bytes;
            return memory;
        };
        decoder_.bzfree = [](void*, void* memory) { std::free(memory); };
        if (BZ2_bzDecompressInit(&decoder_, 0, 1) != BZ_OK)
            fail(QPX_READER_INTERNAL_ERROR, "Cannot initialize MRC bzip2 decoder");
    }
    Bzip2Input(const Bzip2Input&) = delete;
    Bzip2Input& operator=(const Bzip2Input&) = delete;
    ~Bzip2Input() { BZ2_bzDecompressEnd(&decoder_); }

    void read(std::uint8_t* output, std::size_t size) {
        valid(consume(output, size) == size, "MRC bzip2 data is shorter than declared");
    }

    void finish() {
        std::uint8_t extra = 0;
        valid(consume(&extra, 1) == 0 && ended_ && supplied_ - decoder_.avail_in == size_,
              "MRC requires one complete bzip2 stream with exact extent");
    }

private:
    std::size_t consume(std::uint8_t* output, std::size_t size) {
        std::size_t written = 0;
        while (written < size && !ended_) {
            checkpoint(cancel_);
            if (!decoder_.avail_in && supplied_ < size_) {
                const auto count = std::min(io_step, size_ - supplied_);
                decoder_.next_in = reinterpret_cast<char*>(input_ + supplied_);
                decoder_.avail_in = static_cast<unsigned>(count);
                supplied_ += count;
            }
            const auto before = decoder_.avail_in;
            const auto count = static_cast<unsigned>(std::min(io_step, size - written));
            decoder_.next_out = reinterpret_cast<char*>(output + written);
            decoder_.avail_out = count;
            const auto status = BZ2_bzDecompress(&decoder_);
            if (status == BZ_MEM_ERROR)
                fail(QPX_READER_INTERNAL_ERROR, "MRC bzip2 workspace allocation failed");
            const auto produced = count - decoder_.avail_out;
            ended_ = status == BZ_STREAM_END;
            valid(ended_ || (status == BZ_OK && (before != decoder_.avail_in || produced)),
                  "MRC bzip2 stream is corrupt or truncated");
            written += produced;
        }
        return written;
    }

    std::size_t available_ = workspace;
    bz_stream decoder_{};
    std::uint8_t* input_;
    std::size_t size_, supplied_ = 0;
    qpx_reader_cancel_v1 cancel_;
    bool ended_ = false;
};

void swap_samples(std::uint8_t* pixels, std::size_t size, unsigned sample_bytes,
                  qpx_reader_cancel_v1 cancel) {
    for (std::size_t pos = 0; pos < size;) {
        checkpoint(cancel);
        const auto end = pos + std::min(io_step, size - pos);
        for (; pos < end; pos += sample_bytes)
            std::reverse(pixels + pos, pixels + pos + sample_bytes);
    }
}

Raster read_file(const std::filesystem::path& path, std::uint64_t budget_bytes,
                 const OpenOptions& options, qpx_reader_cancel_v1 cancel) {
    const auto file_size = std::filesystem::file_size(path);
    require(file_size <= budget_bytes, "MRC file exceeds reader work budget");
    Budget budget{budget_bytes - header_size};
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file(_wfopen(path.c_str(), L"rb"),
                                                            std::fclose);
    valid(bool(file), "Cannot open MRC file");
    valid(!std::setvbuf(file.get(), nullptr, _IONBF, 0), "Cannot configure bounded MRC input");
    std::array<std::uint8_t, header_size> bytes{};
    read_bytes(file.get(), bytes.data(), 2, cancel, "MRC primary header is truncated");
    std::unique_ptr<std::uint8_t[]> decoded;
    std::uint64_t decoded_size = file_size;
    auto suffix = path.extension().native();
    for (auto& letter : suffix) {
        if (letter >= L'A' && letter <= L'Z')
            letter += L'a' - L'A';
    }
    std::unique_ptr<std::uint8_t[]> bzip2_encoded;
    std::optional<Bzip2Input> bzip2;
    // A plain NX can start with compression magic. Require the suffix too.
    if (suffix == L".gz" && bytes[0] == 0x1f && bytes[1] == 0x8b) {
        using qpx::image_plugin::decode_gzip_member;
        using qpx::image_plugin::GzipResult;
        using qpx::image_plugin::gzip_workspace_bytes;
        valid(file_size >= 18, "MRC gzip member is truncated");
        require(budget.remaining >= gzip_workspace_bytes, "MRC gzip workspace exceeds reader work budget");
        budget.remaining -= gzip_workspace_bytes;
        auto encoded = read_encoded(file.get(), budget, std::size_t(file_size), bytes.data(), cancel);
        const auto* isize = encoded.get() + std::size_t(file_size) - 4;
        decoded_size = std::uint64_t(isize[0]) | (std::uint64_t(isize[1]) << 8) |
                       (std::uint64_t(isize[2]) << 16) | (std::uint64_t(isize[3]) << 24);
        valid(decoded_size >= header_size, "MRC gzip primary header is truncated");
        decoded = budget.array(decoded_size);
        const auto result = decode_gzip_member(encoded.get(), std::size_t(file_size), decoded.get(),
                                               std::size_t(decoded_size), [&] { checkpoint(cancel); });
        if (result == GzipResult::workspace_failure)
            fail(QPX_READER_INTERNAL_ERROR, "MRC gzip workspace could not be initialized");
        valid(result == GzipResult::ok, "MRC requires one complete gzip member with exact extent");
        std::copy_n(decoded.get(), header_size, bytes.data());
    } else if (suffix == L".bz2" && bytes[0] == 'B' && bytes[1] == 'Z') {
        require(budget.remaining >= Bzip2Input::workspace, "MRC bzip2 workspace exceeds reader work budget");
        budget.remaining -= Bzip2Input::workspace;
        bzip2_encoded = read_encoded(file.get(), budget, std::size_t(file_size), bytes.data(), cancel);
        bzip2.emplace(bzip2_encoded.get(), std::size_t(file_size), cancel);
        bzip2->read(bytes.data(), header_size);
    } else {
        valid(file_size >= header_size, "MRC primary header is truncated");
        read_bytes(file.get(), bytes.data() + 2, header_size - 2, cancel, "MRC primary header is truncated");
    }
    const auto header = inspect_header(bytes);
    std::uint64_t raster_bytes = 0;
    auto raster = decode_header(header, options, raster_bytes);
    if (bzip2) {
        raster.pixels = budget.array(raster_bytes);
        bzip2->read(raster.pixels.get(), std::size_t(raster_bytes));
        bzip2->finish();
    } else if (decoded) {
        valid(decoded_size - header_size == raster_bytes, "MRC raster is truncated or has trailing data");
        raster.pixels = std::move(decoded);
        raster.raster_offset = header_size;
    } else {
        valid(decoded_size - header_size == raster_bytes, "MRC raster is truncated or has trailing data");
        raster.pixels = budget.array(raster_bytes);
        read_bytes(file.get(), raster.pixels.get(), std::size_t(raster_bytes), cancel,
                   "MRC raster is truncated or changed");
        check_eof(file.get());
    }
    if (header.big_endian && raster.sample.bytes > 1)
        swap_samples(raster.pixels.get() + raster.raster_offset, std::size_t(raster_bytes),
                     raster.sample.bytes, cancel);
    checkpoint(cancel);
    return raster;
}

const Raster& image(void* value) {
    require(value != nullptr, "MRC image handle is null");
    return *static_cast<Raster*>(value);
}

qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out,
                                               qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size >= sizeof(*out), "MRC reader info structure is too small");
        out->abi_version = QPX_READER_ABI_V1;
        out->capabilities = QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW;
        bool complete = text(out->provider_id, "org.qupathxx.mrc");
        complete = text(out->display_name, "MRC2014") && complete;
        complete = text(out->version, QPX_MRC_VERSION) && complete;
        complete = text(out->contract, QPX_MRC_CONTRACT) && complete;
        return complete ? QPX_READER_OK
                        : failure(error, QPX_READER_BUFFER_TOO_SMALL, "MRC descriptor text buffer is too small");
    });
}

qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request, void** out,
                                           qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out != nullptr, "MRC output handle is null");
        *out = nullptr;
        require(request && request->struct_size >= sizeof(*request) && !request->reserved,
                "MRC open structure is incomplete or reserved field is set");
        require(request->path.data && request->path.size && request->path.size <= 32768,
                "MRC path is invalid");
        require(request->work_bytes && request->work_bytes <= work_limit,
                "MRC work budget must be between 1 and 256 MiB");
        const auto overhead = UINT64_C(4096) + 8 * request->path.size + sizeof(Raster);
        require(request->work_bytes > overhead + header_size, "MRC work budget is too small");
        checkpoint(request->cancel);
        const auto options = parse_options(request->options);
        const std::string spelling(request->path.data, std::size_t(request->path.size));
        require(spelling.find('\0') == std::string::npos, "MRC path contains NUL");
        const auto path = std::filesystem::u8path(spelling);
        require(path.is_absolute(), "MRC path must be absolute");
        auto result = read_file(path, request->work_bytes - overhead, options, request->cancel);
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
        require(out && out->struct_size >= sizeof(*out), "MRC metadata structure is too small");
        checkpoint(cancel);
        out->level_count = out->has_raw = out->channels = out->timepoints = 1;
        out->width = source.shape[0];
        out->height = source.shape[1];
        out->sample_type = source.sample.type;
        out->z_planes = source.shape[2];
        out->series = source.series;
        out->series_count = source.series_count;
        out->calibration_flags = source.calibration;
        out->pixel_width_um = source.spacing[0];
        out->pixel_height_um = source.spacing[1];
        out->z_spacing_um = source.spacing[2];
        out->time_interval_seconds = 0;
        bool complete = text(out->vendor, "MRC2014");
        complete = text(out->dimension_order, source.volume ? "XYZ" : "XY") && complete;
        return complete ? QPX_READER_OK
                        : failure(error, QPX_READER_BUFFER_TOO_SMALL, "MRC metadata text buffer is too small");
    });
}

qpx_reader_status_v1 QPX_READER_CALL level(void* value, std::uint32_t index, qpx_reader_level_v1* out,
                                            qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        const auto& source = image(value);
        require(out && out->struct_size >= sizeof(*out) && index == 0,
                "MRC requires level zero and a complete level descriptor");
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
        require(out && channel == 0, "MRC has one scalar channel");
        return text(*out, "Scalar") ? QPX_READER_OK
                                    : failure(error, QPX_READER_BUFFER_TOO_SMALL,
                                              "MRC channel text buffer is too small");
    });
}

template <typename T> T sample_value(const std::uint8_t* bytes) {
    T value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

std::uint32_t float_bits(const SampleMode& mode, const std::uint8_t* sample) {
    if (!mode.half)
        return sample_value<std::uint32_t>(sample);
    const auto half = std::uint32_t(sample_value<std::uint16_t>(sample));
    const auto sign = (half & 0x8000u) << 16;
    const auto exponent = (half >> 10) & 31u;
    auto fraction = half & 1023u;
    if (exponent == 31)
        return sign | 0x7f800000u | (fraction << 13);
    if (exponent)
        return sign | ((exponent + 112) << 23) | (fraction << 13);
    if (!fraction)
        return sign;
    unsigned adjusted = 113;
    while (fraction < 1024) {
        fraction <<= 1;
        --adjusted;
    }
    return sign | (adjusted << 23) | ((fraction & 1023u) << 13);
}

void put_rgba(const Raster& raster, const std::uint8_t* sample, std::uint8_t* out) {
    double value = 0;
    switch (raster.sample.type) {
    case QPX_READER_UINT8: value = *sample; break;
    case QPX_READER_INT8: value = sample_value<std::int8_t>(sample); break;
    case QPX_READER_INT16: value = sample_value<std::int16_t>(sample); break;
    case QPX_READER_UINT16: value = sample_value<std::uint16_t>(sample); break;
    case QPX_READER_FLOAT32: {
        // Check the expanded bits without evaluating a signaling NaN.
        const auto bits = float_bits(raster.sample, sample);
        if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000))
            return;
        float decoded = 0;
        std::memcpy(&decoded, &bits, sizeof(decoded));
        value = decoded;
        break;
    }
    }
    const auto window = raster.sample.window;
    const auto normalized = value <= window.low ? 0.0
                          : value >= window.high ? 1.0
                          : (value - window.low) / (window.high - window.low);
    out[0] = out[1] = out[2] = std::uint8_t(std::floor(normalized * 255 + 0.5));
    out[3] = 255;
}

qpx_reader_status_v1 read_pixels(const Raster& source, const qpx_reader_region_v1* request,
                                qpx_reader_pixels_v1* out, bool raw, qpx_reader_cancel_v1 cancel) {
    require(out && out->struct_size >= sizeof(*out), "MRC pixel structure is too small");
    out->written = out->validity_written = 0;
    require(request && request->struct_size >= sizeof(*request), "MRC region structure is too small");
    require(!request->level && !request->t && !request->first_channel &&
                request->channels == (raw ? 1u : 0u) &&
                request->z < (raw ? source.shape[2] : 1),
            "MRC region selectors are outside the supported channel or plane");
    const auto width = request->width;
    const auto height = request->height;
    const auto channels = raw ? 1u : 4u;
    const auto channel_stride = raw ? source.sample.raw_bytes() : 1u;
    const auto stride = channels * channel_stride;
    const auto per_pixel = stride + (raw ? 1u : 0u);
    require(width > 0 && height > 0 && std::uint64_t(width) <= work_limit / per_pixel &&
                std::uint64_t(height) <= work_limit / per_pixel / std::uint64_t(width),
            "MRC pixel output exceeds the 256 MiB work limit");
    const auto limit = std::numeric_limits<std::int64_t>::max();
    require(request->x <= limit - width && request->y <= limit - height,
            "MRC region coordinate overflow");
    const auto pixels = std::uint64_t(width) * std::uint64_t(height);
    const auto bytes = pixels * stride;
    require(out->data && out->capacity >= bytes && !out->reserved &&
                out->sample_type == (raw ? source.sample.type : QPX_READER_UINT8) &&
                out->width == width && out->height == height && out->channels == channels &&
                out->row_stride == std::uint64_t(width) * stride && out->pixel_stride == stride &&
                out->channel_stride == channel_stride &&
                (raw ? out->validity && out->validity_capacity >= pixels
                     : !out->validity && !out->validity_capacity),
            "MRC output must have valid buffers and packed pixel strides");
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
            const auto stored_index = source.sample_index(x, y, request->z);
            const auto* base = source.pixels.get() + source.raster_offset;
            std::uint8_t unpacked = 0;
            const std::uint8_t* sample;
            if (source.sample.packed4) {
                const auto packed = base[stored_index / 2];
                unpacked = std::uint8_t((packed >> (4 * (stored_index & 1u))) & 0x0fu);
                sample = &unpacked;
            } else {
                sample = base + stored_index * source.sample.bytes;
            }
            auto* target = out->data + index * stride;
            if (raw) {
                if (source.sample.half) {
                    const auto bits = float_bits(source.sample, sample);
                    std::memcpy(target, &bits, sizeof(bits));
                } else {
                    std::memcpy(target, sample, stride);
                }
                out->validity[index] = 1;
            } else {
                put_rgba(source, sample, target);
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
                "Unsupported MRC ABI or incomplete API structure");
        *out = {sizeof(*out), QPX_READER_ABI_V1, describe, open, destroy, metadata, level,
                channel_name, read_rgba, read_raw};
        return QPX_READER_OK;
    });
}
