// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include "../format-common/json_tree.hpp"
#include <windows.h>
#include <charconv>
#include <fstream>
#include <map>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
using namespace qpx_image_plugin;
namespace fs = std::filesystem;
constexpr std::size_t header_limit = 65536;

std::string_view trim(std::string_view s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    return s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
}

std::string lowercase(std::string_view s) {
    std::string result(s);
    for (auto& c : result)
        if (c >= 'A' && c <= 'Z') c = char(c + 'a' - 'A');
    return result;
}

std::uint64_t number(std::string_view s) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), result);
    valid(parsed.ec == std::errc{} && parsed.ptr == s.data() + s.size(), "Invalid ENVI integer");
    return result;
}

enum class Interleave { Pixel, Line, Band };
struct Sample { std::uint32_t type, bytes; double low, high; };
Sample sample_type(std::uint64_t type) {
    switch (type) {
    case 1: return {QPX_READER_UINT8, 1, 0, 255};
    case 2: return {QPX_READER_INT16, 2, INT16_MIN, INT16_MAX};
    case 3: return {QPX_READER_INT32, 4, INT32_MIN, INT32_MAX};
    case 4: return {QPX_READER_FLOAT32, 4, 0, 1};
    case 5: return {QPX_READER_FLOAT64, 8, 0, 1};
    case 12: return {QPX_READER_UINT16, 2, 0, UINT16_MAX};
    case 13: return {QPX_READER_UINT32, 4, 0, UINT32_MAX};
    case 14: return {QPX_READER_INT64, 8, double(INT64_MIN), double(INT64_MAX)};
    case 15: return {QPX_READER_UINT64, 8, 0, double(UINT64_MAX)};
    default: fail(QPX_READER_UNSUPPORTED, "ENVI data type is outside the real raster profile");
    }
}

template<class T> T parse_ignore_value(std::string_view text) {
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        valid(!text.empty() && text.front() != '-', "Invalid ENVI data ignore value");
    }
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    valid(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size(),
          "Invalid ENVI data ignore value");
    if constexpr (std::is_floating_point_v<T>)
        valid(std::isfinite(value), "Invalid ENVI data ignore value");
    return value;
}

using IgnoreValue = std::variant<std::uint64_t, double>;
IgnoreValue ignore_value(std::string_view text, const Sample& sample) {
    if (sample.type == QPX_READER_FLOAT32) return double(parse_ignore_value<float>(text));
    if (sample.type == QPX_READER_FLOAT64) return parse_ignore_value<double>(text);
    if (sample.type == QPX_READER_INT64) return std::uint64_t(parse_ignore_value<std::int64_t>(text));
    if (sample.type == QPX_READER_UINT64) return parse_ignore_value<std::uint64_t>(text);
    const auto value = parse_ignore_value<std::int64_t>(text);
    valid(value >= std::int64_t(sample.low) && value <= std::int64_t(sample.high),
          "ENVI data ignore value is outside the sample range");
    return std::uint64_t(value) & ((UINT64_C(1) << (8 * sample.bytes)) - 1);
}

struct Header {
    std::uint32_t width, height, bands;
    std::uint64_t offset;
    Interleave interleave;
    Sample sample;
    bool big_endian;
    std::vector<std::string> names;
    std::optional<IgnoreValue> ignore;
    std::array<unsigned, 3> display_bands{};
};

std::map<std::string, std::string_view> fields(std::string_view input) {
    const auto line = input.find('\n');
    valid(line != std::string_view::npos && trim(input.substr(0, line)) == "ENVI", "Missing ENVI header signature");
    input.remove_prefix(line + 1);
    std::map<std::string, std::string_view> result;
    while (!(input = trim(input)).empty()) {
        const auto end = input.find('\n');
        if (input.front() == ';') {
            input = end == std::string_view::npos ? std::string_view{} : input.substr(end + 1);
            continue;
        }
        const auto eq = input.find('=');
        valid(eq != std::string_view::npos && eq < end, "Malformed ENVI header assignment");
        auto key = lowercase(trim(input.substr(0, eq)));
        if (key != "samples" && key != "lines" && key != "bands" && key != "header offset" &&
            key != "file type" && key != "data type" && key != "interleave" && key != "byte order" &&
            key != "band names" && key != "description" && key != "data ignore value" &&
            key != "default bands")
            fail(QPX_READER_UNSUPPORTED, "ENVI header field is outside the uncalibrated raster profile");
        auto rest = input.substr(eq + 1);
        const auto first = rest.find_first_not_of(" \t");
        const bool list = key == "band names" || key == "description" || key == "default bands";
        auto last = rest.find('\n');
        if (list) {
            valid(first != std::string_view::npos && rest[first] == '{', "ENVI list requires braces");
            const auto close = rest.find('}', first + 1);
            valid(close != std::string_view::npos, "Unterminated ENVI list");
            last = rest.find('\n', close + 1);
            valid(trim(rest.substr(close + 1, last - close - 1)).empty(), "Unexpected text after ENVI list");
        }
        const auto value = trim(rest.substr(0, last));
        valid(!value.empty() && result.emplace(std::move(key), value).second, "Empty or duplicate ENVI field");
        input = last == std::string_view::npos ? std::string_view{} : rest.substr(last + 1);
    }
    return result;
}

std::array<unsigned, 3> display_bands(std::string_view value, unsigned bands) {
    auto list = value.substr(1, value.size() - 2);
    std::array<unsigned, 3> result{};
    for (unsigned channel = 0; channel < result.size(); ++channel) {
        const auto comma = list.find(',');
        const auto token = trim(list.substr(0, comma));
        valid(!token.empty(), "Empty ENVI default band");
        const auto band = number(token);
        valid(band && band <= bands, "ENVI default band is outside the raster");
        result[channel] = unsigned(band - 1);
        if (comma == std::string_view::npos) {
            valid(channel != 1, "ENVI default bands requires one or three band numbers");
            if (!channel) result.fill(result[0]);
            return result;
        }
        list.remove_prefix(comma + 1);
    }
    fail(QPX_READER_IO_ERROR, "ENVI default bands requires one or three band numbers");
}

Header read_header(const fs::path& path, qpx_reader_cancel_v1 cancel) {
    require(lowercase(path.extension().u8string()) == ".hdr", "ENVI requires its .hdr primary file");
    const auto length = fs::file_size(path);
    valid(length && length <= header_limit, "ENVI header exceeds its read bound");
    std::ifstream stream(path, std::ios::binary);
    std::string input(std::size_t(length), '\0');
    checkpoint(cancel);
    valid(bool(stream.read(input.data(), std::streamsize(length))) && stream.peek() == EOF && !stream.bad(),
          "Cannot read exact ENVI header");
    for (const auto c : input)
        valid((c >= 32 && c < 127) || c == '\t' || c == '\r' || c == '\n', "ENVI header requires ASCII text");
    const auto f = fields(input);
    const auto get = [&](const char* key) {
        const auto entry = f.find(key);
        valid(entry != f.end(), "Missing required ENVI header field");
        return entry->second;
    };
    if (lowercase(get("file type")) != "envi standard")
        fail(QPX_READER_UNSUPPORTED, "ENVI requires the Standard raster file type");
    const auto w = number(get("samples")), h = number(get("lines")), bands = number(get("bands"));
    const auto offset = number(get("header offset")), order = number(get("byte order"));
    valid(w && h && w <= INT32_MAX && h <= INT32_MAX && bands && bands <= 256 && order <= 1,
          "Invalid ENVI dimensions, band count or byte order");
    valid(offset <= work_limit, "ENVI header offset exceeds its bound");
    const auto layout = lowercase(get("interleave"));
    valid(layout == "bip" || layout == "bil" || layout == "bsq", "Unknown ENVI interleave");
    Header result{std::uint32_t(w), std::uint32_t(h), std::uint32_t(bands), offset,
                  layout == "bip" ? Interleave::Pixel : layout == "bil" ? Interleave::Line : Interleave::Band,
                  sample_type(number(get("data type"))), order != 0, {}, {}};
    if (const auto item = f.find("data ignore value"); item != f.end())
        result.ignore = ignore_value(item->second, result.sample);
    if (const auto item = f.find("default bands"); item != f.end())
        result.display_bands = display_bands(item->second, result.bands);
    const auto names = f.find("band names");
    if (names == f.end()) {
        for (std::uint32_t i = 0; i < bands; ++i) result.names.push_back("Band " + std::to_string(i + 1));
    } else {
        auto list = names->second.substr(1, names->second.size() - 2);
        for (std::uint32_t i = 0; i < bands; ++i) {
            const auto comma = list.find(',');
            valid((comma == std::string_view::npos) == (i + 1 == bands), "ENVI band-name count differs");
            const auto name = trim(list.substr(0, comma));
            valid(!name.empty() && name.size() <= 128 && name.find_first_of("{}\r\n\t") == std::string_view::npos,
                  "Invalid ENVI band name");
            result.names.emplace_back(name);
            if (comma != std::string_view::npos) list.remove_prefix(comma + 1);
        }
    }
    return result;
}

const wchar_t* data_extension(std::string_view options, qpx_reader_cancel_v1 cancel) {
    if (options.empty()) return L".img";
    require(options.size() <= 256, "ENVI reader options exceed 256 bytes");
    std::array<std::byte, 4096> scratch{};
    std::pmr::monotonic_buffer_resource memory(scratch.data(), scratch.size(), std::pmr::null_memory_resource());
    namespace json = qpx_native_json;
    try {
        json::Tree tree(options, &memory, cancel, 2, 2, 1);
        const auto& root = tree.root();
        tree.keys(root, {"companion_layout"});
        if (const auto* layout = tree.find(root, "companion_layout")) {
            const auto value = json::string(*layout);
            if (value == "img") return L".img";
            if (value == "bin") return L".bin";
            if (value == "dat") return L".dat";
            if (value == "raw") return L".raw";
            if (value == "bil") return L".bil";
            if (value == "bip") return L".bip";
            if (value == "bsq") return L".bsq";
            if (value == "appended") return L"";
            fail(QPX_READER_INVALID_REQUEST, "Unknown ENVI companion_layout");
        }
        return L".img";
    } catch (const qpx_binary::Failure& error) {
        fail(error.status == QPX_READER_CANCELLED ? error.status : QPX_READER_INVALID_REQUEST, error.message);
    }
}

fs::path companion(const fs::path& path, std::string_view options, qpx_reader_cancel_v1 cancel) {
    require(path.is_absolute() && lowercase(path.extension().u8string()) == ".hdr", "ENVI requires an absolute .hdr path");
    auto result = path;
    result.replace_extension(data_extension(options, cancel));
    const auto leaf = result.filename().native();
    require(!leaf.empty() && leaf.back() != L'.' && leaf.back() != L' ' && leaf.find(L':') == leaf.npos,
            "Invalid ENVI companion filename");
    const auto attributes = GetFileAttributesW(result.c_str());
    valid(attributes != INVALID_FILE_ATTRIBUTES, "Selected ENVI companion is missing");
    if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) ||
        fs::equivalent(path, result))
        fail(QPX_READER_UNSUPPORTED, "ENVI companion must be a distinct regular file");
    return result;
}

template<class T> T scalar(const std::uint8_t* data) {
    T value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

struct Raster {
    std::int64_t width, height;
    const char* vendor = "ENVI";
    Sample sample;
    std::vector<std::string> names;
    std::unique_ptr<std::uint8_t[]> raw;
    std::optional<IgnoreValue> ignore;
    std::array<unsigned, 3> display_bands;
    unsigned channels() const { return unsigned(names.size()); }
    unsigned sample_bytes() const { return sample.bytes; }
    std::uint32_t sample_type() const { return sample.type; }
    const char* channel_name(unsigned index) const { return names[index].c_str(); }

    std::optional<double> finite_sample(const std::uint8_t* data) const {
        switch (sample.type) {
        case QPX_READER_UINT8: return *data;
        case QPX_READER_INT16: return scalar<std::int16_t>(data);
        case QPX_READER_INT32: return scalar<std::int32_t>(data);
        case QPX_READER_UINT16: return scalar<std::uint16_t>(data);
        case QPX_READER_UINT32: return scalar<std::uint32_t>(data);
        case QPX_READER_INT64: return double(scalar<std::int64_t>(data));
        case QPX_READER_UINT64: return double(scalar<std::uint64_t>(data));
        case QPX_READER_FLOAT32:
            if ((scalar<std::uint32_t>(data) & 0x7f800000u) == 0x7f800000u) return {};
            return scalar<float>(data);
        case QPX_READER_FLOAT64:
            if ((scalar<std::uint64_t>(data) & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000)) return {};
            return scalar<double>(data);
        }
        return {};
    }
    bool ignored(const std::uint8_t* data) const {
        if (!ignore) return false;
        if (const auto floating = std::get_if<double>(&*ignore))
            return finite_sample(data) == *floating;
        // The Windows ABI uses little-endian samples. Zero extend the stored
        // integer bits, including negatives, without rounding through double.
        std::uint64_t bits = 0;
        std::memcpy(&bits, data, sample.bytes);
        return bits == std::get<std::uint64_t>(*ignore);
    }
    bool valid_samples(const std::uint8_t* data, unsigned count) const {
        if (!ignore) return true;
        for (unsigned i=0; i<count; ++i)
            if (ignored(data+i*sample.bytes)) return false;
        return true;
    }
    void rgba(const std::uint8_t* data, std::uint8_t* out) const {
        auto display_sample = [&](unsigned band) -> std::optional<std::uint8_t> {
            const auto* sample_data = data + band * sample.bytes;
            const auto value = finite_sample(sample_data);
            if (!value || ignored(sample_data)) return {};
            const auto scaled = std::clamp((*value - sample.low) / (sample.high - sample.low), 0., 1.);
            return std::uint8_t(std::floor(255 * scaled + .5));
        };
        const auto red = display_sample(display_bands[0]);
        if (!red) return;
        if (display_bands[0] == display_bands[1] && display_bands[0] == display_bands[2]) {
            out[0] = out[1] = out[2] = *red;
        } else {
            const auto green = display_sample(display_bands[1]), blue = display_sample(display_bands[2]);
            if (!green || !blue) return;
            out[0] = *red;
            out[1] = *green;
            out[2] = *blue;
        }
        out[3] = 255;
    }
};

std::uint64_t packed_index(std::uint64_t index, const Header& h) {
    if (h.interleave == Interleave::Pixel) return index;
    if (h.interleave == Interleave::Band) {
        const auto plane = std::uint64_t(h.width) * h.height;
        return (index % plane) * h.bands + index / plane;
    }
    const auto line = std::uint64_t(h.width) * h.bands;
    const auto x = index % h.width;
    const auto band = (index % line) / h.width;
    return (index / line) * line + x * h.bands + band;
}

struct Codec {
    static constexpr const char *provider_id = "org.qupathxx.envi", *name = "ENVI Standard",
        *version = QPX_ENVI_VERSION, *contract = QPX_ENVI_CONTRACT;
    static Raster decode_compound(const fs::path& path, Budget& budget, qpx_reader_cancel_v1 cancel,
                                  std::string_view options) {
        constexpr std::size_t block_size = 65536;
        constexpr std::uint64_t scratch = 4 * header_limit + block_size;
        require(budget.remaining > scratch, "ENVI header and read buffer exceed work budget");
        budget.remaining -= scratch;
        auto h = read_header(path, cancel);
        const auto data = companion(path, options, cancel);
        const auto pixels = std::uint64_t(h.width) * h.height;
        valid(pixels <= work_limit / h.bands / h.sample.bytes, "ENVI raster exceeds its size bound");
        const auto bytes = pixels * h.bands * h.sample.bytes;
        valid(fs::file_size(data) == h.offset + bytes, "ENVI raster has the wrong extent");
        std::ifstream stream(data, std::ios::binary);
        valid(bool(stream.seekg(std::streamoff(h.offset))), "Cannot seek to ENVI raster");
        auto raster = budget.array(bytes);
        std::array<std::uint8_t, block_size> block;
        for (std::uint64_t position = 0; position < bytes; position += block_size) {
            checkpoint(cancel);
            const auto length = std::size_t(std::min<std::uint64_t>(block_size, bytes - position));
            valid(bool(stream.read(reinterpret_cast<char*>(block.data()), std::streamsize(length))),
                  "ENVI raster was truncated");
            for (std::size_t i = 0; i < length; i += h.sample.bytes) {
                auto* out = raster.get() + packed_index((position + i) / h.sample.bytes, h) * h.sample.bytes;
                if (h.big_endian) std::reverse_copy(block.data() + i, block.data() + i + h.sample.bytes, out);
                else std::memcpy(out, block.data() + i, h.sample.bytes);
            }
        }
        valid(stream.peek() == EOF && !stream.bad(), "ENVI raster changed while reading");
        return {h.width, h.height, "ENVI", h.sample, std::move(h.names), std::move(raster), h.ignore, h.display_bands};
    }
};

fs::path request_companion(const qpx_reader_open_v1* request) {
    require(request && request->struct_size >= sizeof(*request) && !request->reserved &&
            (!request->options.size || (request->options.data && request->options.size <= 256)) &&
            request->path.data && request->path.size && request->path.size <= 32768, "Invalid ENVI source request");
    checkpoint(request->cancel);
    const std::string_view name(request->path.data, std::size_t(request->path.size));
    require(name.find('\0') == std::string_view::npos, "ENVI path contains NUL");
    const std::string_view options(request->options.data ? request->options.data : "",
                                   std::size_t(request->options.size));
    return companion(fs::u8path(name.begin(), name.end()), options, request->cancel);
}

qpx_reader_status_v1 QPX_READER_CALL source_count(const qpx_reader_open_v1* request, std::uint32_t* out,
                                                 qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        require(out, "Null ENVI source count");
        (void)request_companion(request);
        *out = 1;
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request, std::uint32_t index,
                                                qpx_reader_text_v1* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        require(out && !index, "Invalid ENVI source file index");
        const auto path = request_companion(request).u8string();
        return text(*out, path.c_str()) ? QPX_READER_OK :
            failure(error, QPX_READER_BUFFER_TOO_SMALL, "Short ENVI source path buffer");
    });
}
} // namespace

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t abi, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return PackedImageReader<Codec, Raster, false, false, true, true, true>::entry(abi, out, error);
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(
    std::uint32_t abi, qpx_reader_files_api_v1* out, qpx_reader_text_v1* error) {
    return boundary(error, [&] {
        require(abi == QPX_READER_ABI_V1 && out && out->struct_size >= sizeof(*out), "Invalid ENVI source ABI entry request");
        *out = {sizeof(*out), QPX_READER_ABI_V1, source_count, source_path};
        return QPX_READER_OK;
    });
}
