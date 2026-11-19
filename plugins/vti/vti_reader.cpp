// SPDX-License-Identifier: AGPL-3.0-only
#include "../image-common/rgba8_reader.hpp"
#include "../image-common/gzip_member.hpp"
#include "../format-common/json_tree.hpp"
#include <lz4.h>
#include <tinyxml2.h>
#include <xz.h>

#include <charconv>
#include <initializer_list>
#include <limits>
#include <type_traits>
#include <vector>

namespace {
using namespace qpx_image_plugin;
using tinyxml2::XMLElement;
using namespace std::literals;

struct Selection {
    double scale = 0;
    std::string point_array;
};

Selection select(std::string_view options, qpx_reader_cancel_v1 cancel) {
    Selection result;
    if (options.empty())
        return result;
    require(options.size() <= 1024, "VTI reader options exceed 1 KiB");
    std::array<std::byte, 8192> scratch{};
    std::pmr::monotonic_buffer_resource memory(scratch.data(), scratch.size(),
                                               std::pmr::null_memory_resource());
    namespace json = qpx_native_json;
    try {
        json::Tree tree(options, &memory, cancel, 2, 3, 2);
        const auto& root = tree.root();
        tree.keys(root, {"spacing_unit", "point_array"});
        if (const auto* unit = tree.find(root, "spacing_unit")) {
            const auto value = json::string(*unit);
            require(value == "mm" || value == "um", "VTI options require spacing_unit mm or um");
            result.scale = value == "mm" ? 1000 : 1;
        }
        if (const auto* array = tree.find(root, "point_array")) {
            const auto name = json::string(*array);
            require(!name.empty() && name.size() <= 128, "VTI point_array must be a 1 to 128 byte name");
            qpx_binary::utf8(name, cancel);
            result.point_array = name;
        }
    } catch (const qpx_binary::Failure& error) {
        fail(error.status == QPX_READER_CANCELLED ? error.status : QPX_READER_INVALID_REQUEST, error.message);
    }
    return result;
}

std::string_view attribute(const XMLElement& element, const char* name) {
    const auto* value = element.Attribute(name);
    return value ? value : "";
}
void attributes(const XMLElement& element, std::initializer_list<std::string_view> allowed) {
    for (auto* attr = element.FirstAttribute(); attr; attr = attr->Next())
        valid(std::find(allowed.begin(), allowed.end(), attr->Name()) != allowed.end(),
              "Unsupported VTI XML attribute");
}
std::string_view next_token(std::string_view& input) {
    constexpr std::string_view white = " \t\r\n";
    const auto first = input.find_first_not_of(white);
    if (first == input.npos) {
        input = {};
        return {};
    }
    input.remove_prefix(first);
    const auto last = input.find_first_of(white);
    const auto token = input.substr(0, last);
    input.remove_prefix(token.size());
    return token;
}
template<class T> T number(std::string_view token) {
    valid(!token.empty() && token.size() <= 96, "Missing or oversized VTI number");
    if (token.front() == '+') {
        token.remove_prefix(1);
        valid(!token.empty() && token.front() != '-' && token.front() != '+', "Invalid VTI numeric sign");
    }
    T value{};
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    valid(parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size(), "Invalid VTI number");
    return value;
}
template<class T, std::size_t N> std::array<T, N> numbers(std::string_view input) {
    std::array<T, N> values{};
    for (auto& value : values)
        value = number<T>(next_token(input));
    valid(next_token(input).empty(), "Extra VTI numeric values");
    return values;
}
bool nonempty(const tinyxml2::XMLNode& node) {
    if (const auto* text = node.ToText()) {
        std::string_view value(text->Value());
        return !next_token(value).empty();
    }
    return true;
}
std::string_view leaf(const XMLElement& element) {
    auto* node = element.FirstChild();
    if (!node) return {};
    valid(node->ToText() && !node->NextSibling(), "VTI array must contain only text");
    return node->Value();
}
std::string_view point_payload(const XMLElement& array, unsigned components) {
    if (components == 1 || !array.FirstChildElement())
        return leaf(array);
    // VTK writes a cached norm range after the tuple data. It is display
    // metadata and does not set the reader's raw or preview value domain.
    std::string_view payload;
    bool summary = false;
    for (auto* node = array.FirstChild(); node; node = node->NextSibling()) {
        if (!nonempty(*node)) continue;
        if (node->ToText()) {
            valid(payload.empty() && !summary, "Unexpected VTI point array text");
            payload = node->Value();
            continue;
        }
        const auto* key = node->ToElement();
        valid(!summary && key && key->Name() == "InformationKey"sv, "Unsupported VTI point array metadata");
        attributes(*key, {"name", "location", "length"});
        valid(attribute(*key, "name") == "L2_NORM_RANGE" && attribute(*key, "location") == "vtkDataArray" &&
                  attribute(*key, "length") == "2", "Unsupported VTI point array metadata");
        unsigned index = 0;
        for (auto* child = key->FirstChild(); child; child = child->NextSibling()) {
            if (!nonempty(*child)) continue;
            const auto* value = child->ToElement();
            valid(index < 2 && value && value->Name() == "Value"sv, "Invalid VTI norm range");
            attributes(*value, {"index"});
            valid(attribute(*value, "index") == (index == 0 ? "0" : "1"), "Invalid VTI norm range index");
            (void)numbers<double, 1>(leaf(*value));
            ++index;
        }
        valid(index == 2, "Invalid VTI norm range");
        summary = true;
    }
    return payload;
}
constexpr std::string_view xml_space = " \t\r\n";

std::string_view trim_right(std::string_view input) {
    const auto last = input.find_last_not_of(xml_space);
    return last == input.npos ? std::string_view{} : input.substr(0, last + 1);
}
std::string_view trim_left(std::string_view input) {
    const auto first = input.find_first_not_of(xml_space);
    return first == input.npos ? std::string_view{} : input.substr(first);
}
struct RawAppended {
    std::string_view prefix, body, suffix;
    explicit operator bool() const { return !prefix.empty(); }
};
RawAppended split_raw_appended(std::string_view file) {
    constexpr auto opener = "<AppendedData"sv;
    const auto start = file.find(opener);
    if (start == file.npos) return {};
    const auto tag = file.substr(start + opener.size(), 4096);
    const auto end = tag.find('>');
    if (end == tag.npos || tag.empty() || xml_space.find(tag.front()) == xml_space.npos) return {};
    auto attribute = trim_left(tag.substr(0, end));
    constexpr auto key = "encoding"sv;
    if (attribute.substr(0, key.size()) != key) return {};
    attribute = trim_left(attribute.substr(key.size()));
    if (attribute.empty() || attribute.front() != '=') return {};
    attribute = trim_left(trim_right(attribute.substr(1)));
    if (attribute != "\"raw\"" && attribute != "'raw'") return {};

    const auto after_tag = start + opener.size() + end + 1;
    const auto underscore = file.find_first_not_of(xml_space, after_tag);
    valid(underscore != file.npos && file[underscore] == '_', "VTI raw appended marker is missing");
    auto suffix = trim_right(file);
    constexpr auto root_close = "</VTKFile>"sv, raw_close = "</AppendedData>"sv;
    const auto ends_with = [](std::string_view text, std::string_view ending) {
        return text.size() >= ending.size() && text.substr(text.size() - ending.size()) == ending;
    };
    valid(ends_with(suffix, root_close), "VTI raw appended root close is missing");
    suffix.remove_suffix(root_close.size());
    suffix = trim_right(suffix);
    valid(ends_with(suffix, raw_close), "VTI raw appended section close is missing");
    const auto closing = suffix.size() - raw_close.size();
    const auto body = underscore + 1;
    // The writer adds one line ending and the opening tag's indentation after
    // the byte body. Keep that framing separate even if samples end in whitespace.
    const auto marker_line = file.substr(after_tag, underscore - after_tag);
    const auto newline = marker_line.find('\n');
    const auto opening_line = start ? file.rfind('\n', start - 1) : file.npos;
    valid(newline != file.npos && opening_line != file.npos, "VTI raw appended framing is invalid");
    const auto line_ending = newline && marker_line[newline - 1] == '\r' ? "\r\n"sv : "\n"sv;
    const auto indent = file.substr(opening_line + 1, start - opening_line - 1);
    valid(indent.find_first_not_of(" \t") == indent.npos && closing >= indent.size() + line_ending.size(),
          "VTI raw appended framing is invalid");
    const auto framing = closing - indent.size() - line_ending.size();
    valid(body <= framing && file.substr(framing, line_ending.size()) == line_ending &&
              file.substr(framing + line_ending.size(), indent.size()) == indent,
          "VTI raw appended framing is invalid");
    return {file.substr(0, body), file.substr(body, framing - body), file.substr(framing)};
}
void verify_markup(std::string_view xml, qpx_reader_cancel_v1 cancel) {
    std::size_t tags = 0;
    for (std::size_t at = 0; at < xml.size(); ++at) {
        if (!(at & 65535)) checkpoint(cancel);
        const auto c = static_cast<unsigned char>(xml[at]);
        valid((c >= 32 && c < 127) || c == '\n' || c == '\r' || c == '\t',
              "VTI requires ASCII XML");
        valid(c != '&', "VTI profile does not admit XML entity references");
        if (c != '<') continue;
        const auto tag = xml.substr(at, 4096);
        valid(++tags <= 1024 && tag.find('>') != tag.npos && tag.substr(0, 2) != "<!" &&
                  (tag.substr(0, 2) != "<?" || (at == 0 && tag.substr(0, 6) == "<?xml ")),
              "Unsupported or oversized VTI XML markup");
    }
}

template<class T> double display_number(const std::uint8_t* bytes) {
    T value;
    std::memcpy(&value, bytes, sizeof(value));
    return double(value);
}
template<class T> void ascii_number(std::string_view token, std::uint8_t* bytes) {
    const auto value = number<T>(token);
    std::memcpy(bytes, &value, sizeof(value));
}
struct Sample {
    std::string_view name;
    std::uint32_t code;
    unsigned bytes;
    double low, high;
    double (*view)(const std::uint8_t*);
    void (*parse)(std::string_view, std::uint8_t*);
};
template<class T> constexpr Sample sample(std::string_view name, std::uint32_t code) {
    if constexpr (std::is_floating_point_v<T>)
        return {name, code, sizeof(T), -1, 1, display_number<T>, ascii_number<T>};
    return {name, code, sizeof(T), double(std::numeric_limits<T>::lowest()),
            double(std::numeric_limits<T>::max()), display_number<T>, ascii_number<T>};
}
constexpr Sample types[] = {
    sample<std::int8_t>("Int8", QPX_READER_INT8), sample<std::uint8_t>("UInt8", QPX_READER_UINT8),
    sample<std::int16_t>("Int16", QPX_READER_INT16), sample<std::uint16_t>("UInt16", QPX_READER_UINT16),
    sample<std::int32_t>("Int32", QPX_READER_INT32), sample<std::uint32_t>("UInt32", QPX_READER_UINT32),
    sample<std::int64_t>("Int64", QPX_READER_INT64), sample<std::uint64_t>("UInt64", QPX_READER_UINT64),
    sample<float>("Float32", QPX_READER_FLOAT32), sample<double>("Float64", QPX_READER_FLOAT64),
};
struct Raster {
    const char* vendor = "VTK ImageData";
    std::int64_t width = 0, height = 0;
    std::uint32_t z_planes = 0;
    std::array<double, 3> spacing{};
    const Sample* sample = nullptr;
    std::vector<std::string> names;
    std::unique_ptr<std::uint8_t[]> raw;

    unsigned channels() const { return static_cast<unsigned>(names.size()); }
    unsigned sample_bytes() const { return sample->bytes; }
    std::uint32_t sample_type() const { return sample->code; }
    const char* channel_name(unsigned index) const { return names[index].c_str(); }
    void publish_calibration(qpx_reader_metadata_v1& out) const {
        out.pixel_width_um = spacing[0];
        out.pixel_height_um = spacing[1];
        out.z_spacing_um = z_planes > 1 ? spacing[2] : 0;
        out.calibration_flags = spacing[0] > 0 ? (z_planes > 1 ? 7 : 3) : 0;
    }
    void rgba(const std::uint8_t* bytes, std::uint8_t* dest) const {
        const auto value = sample->view(bytes);
        if (!std::isfinite(value)) return;
        const auto fraction = std::clamp((value - sample->low) / (sample->high - sample->low), 0., 1.);
        dest[0] = dest[1] = dest[2] = std::uint8_t(fraction * 255 + 0.5);
        dest[3] = 255;
    }
};

int base64_digit(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
class Base64 {
    std::string_view source_;
    qpx_reader_cancel_v1 cancel_;
    std::uint8_t decoded_[3]{};
    unsigned used_ = 0, size_ = 0;

    void refill() {
        valid(size_ == 0 || size_ == 3, "VTI binary byte count differs from geometry");
        unsigned digits[4]{};
        unsigned count = 0;
        while (count < 4) {
            valid(!source_.empty(), "VTI binary byte count differs from geometry");
            if (!(source_.size() & 4095)) checkpoint(cancel_);
            const char c = source_.front();
            source_.remove_prefix(1);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
            const auto digit = base64_digit(c);
            valid(digit >= 0 || (c == '=' && count >= 2), "Invalid VTI base64 encoding");
            digits[count++] = digit < 0 ? 64u : unsigned(digit);
        }
        valid(digits[2] != 64 || (digits[3] == 64 && !(digits[1] & 15)), "Invalid VTI base64 padding");
        valid(digits[3] != 64 || digits[2] == 64 || !(digits[2] & 3), "Invalid VTI base64 padding");
        decoded_[0] = std::uint8_t((digits[0] << 2) | (digits[1] >> 4));
        decoded_[1] = std::uint8_t(((digits[1] & 15) << 4) | (digits[2] >> 2));
        decoded_[2] = std::uint8_t(((digits[2] & 3) << 6) | digits[3]);
        used_ = 0;
        size_ = digits[2] == 64 ? 1 : digits[3] == 64 ? 2 : 3;
    }

public:
    Base64(std::string_view source, qpx_reader_cancel_v1 cancel) : source_(source), cancel_(cancel) {}
    void read(std::uint8_t* output, std::size_t bytes) {
        while (bytes) {
            if (used_ == size_) refill();
            const auto count = std::min<std::size_t>(bytes, size_ - used_);
            std::memcpy(output, decoded_ + used_, count);
            output += count;
            used_ += static_cast<unsigned>(count);
            bytes -= count;
        }
    }
    void finish_segment() {
        valid(used_ == size_, "VTI binary array exceeds declared extent");
        used_ = size_ = 0;
    }
    std::size_t remaining_byte_limit() const {
        return source_.size() / 4 * 3;
    }
    void finish() {
        finish_segment();
        valid(next_token(source_).empty(), "VTI binary array exceeds declared extent");
    }
};

class RawBytes {
    std::string_view source_;
    qpx_reader_cancel_v1 cancel_;
public:
    RawBytes(std::string_view source, qpx_reader_cancel_v1 cancel) : source_(source), cancel_(cancel) {}
    void read(std::uint8_t* output, std::size_t bytes) {
        valid(bytes <= source_.size(), "VTI raw appended payload is truncated");
        constexpr std::size_t step = 65536;
        while (bytes) {
            checkpoint(cancel_);
            const auto count = std::min(step, bytes);
            std::memcpy(output, source_.data(), count);
            source_.remove_prefix(count);
            output += count;
            bytes -= count;
        }
    }
    void finish_segment() const {}
    std::size_t remaining_byte_limit() const { return source_.size(); }
    void finish() const {
        valid(source_.empty(), "Extra bytes after VTI raw appended payload");
    }
};

template<class Input> std::uint64_t binary_word(Input& input, unsigned width, bool big) {
    std::uint8_t bytes[8]{};
    input.read(bytes, width);
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= std::uint64_t(bytes[i]) << (8 * (big ? width - 1 - i : i));
    return value;
}

enum class Compression { None, Zlib, Lz4, Lzma };

template<class Input>
void compressed_samples(Input& input, std::uint8_t* output, std::size_t expected,
                        unsigned header_bytes, bool big, Compression compression,
                        Budget& budget, qpx_reader_cancel_v1 cancel) {
    using namespace qpx::image_plugin;
    const auto block_count = binary_word(input, header_bytes, big);
    const auto block_size = binary_word(input, header_bytes, big);
    const auto partial = binary_word(input, header_bytes, big);
    const auto last = partial ? partial : block_size;
    valid(block_count && block_count <= 65536 && block_size && block_size <= work_limit &&
              partial < block_size && block_size * (block_count - 1) + last == expected,
          "VTI compressed block geometry differs from raster extent");
    // The pinned single-call XZ decoder uses the output as its dictionary.
    constexpr std::size_t xz_workspace_bytes = 64u * 1024u;
    const auto codec_workspace = compression == Compression::Zlib ? gzip_workspace_bytes :
                                 compression == Compression::Lzma ? xz_workspace_bytes : 0;
    const auto overhead = codec_workspace + block_count * sizeof(std::uint32_t);
    require(budget.remaining > overhead, "VTI compressed block table exceeds reader budget");
    budget.remaining -= overhead;
    std::vector<std::uint32_t> counts(static_cast<std::size_t>(block_count));
    std::uint64_t max_encoded = 0, total_encoded = 0;
    for (auto& count : counts) {
        const auto encoded = binary_word(input, header_bytes, big);
        valid(encoded > 0 && encoded <= work_limit, "Invalid VTI compressed block size");
        count = static_cast<std::uint32_t>(encoded);
        max_encoded = std::max(max_encoded, encoded);
        total_encoded += encoded;
    }
    // Base64 encodes the header separately. Raw input has no encoding boundary.
    input.finish_segment();
    valid(total_encoded <= input.remaining_byte_limit(), "VTI compressed block sizes exceed the encoded payload");
    auto encoded = budget.array(max_encoded);
    std::unique_ptr<xz_dec, decltype(&xz_dec_end)> xz(nullptr, &xz_dec_end);
    if (compression == Compression::Lzma) {
        [[maybe_unused]] static const bool crc_initialized = [] { xz_crc32_init(); return true; }();
        xz.reset(xz_dec_init(XZ_SINGLE, 0));
        if (!xz)
            fail(QPX_READER_INTERNAL_ERROR, "VTI XZ workspace allocation failed");
    }
    for (std::size_t i = 0; i < counts.size(); ++i) {
        checkpoint(cancel);
        input.read(encoded.get(), counts[i]);
        const auto length = static_cast<std::size_t>(i + 1 == counts.size() ? last : block_size);
        if (compression == Compression::Lz4) {
            static_assert(work_limit <= std::numeric_limits<int>::max());
            const auto decoded = LZ4_decompress_safe(reinterpret_cast<const char*>(encoded.get()),
                reinterpret_cast<char*>(output), static_cast<int>(counts[i]), static_cast<int>(length));
            valid(decoded == static_cast<int>(length), "Invalid or incorrectly sized VTI LZ4 block");
        } else if (xz) {
            // vtkLZMADataCompressor writes one CRC32 XZ stream per VTI block.
            valid(counts[i] >= 12 && encoded.get()[6] == 0 && encoded.get()[7] == 1,
                  "VTI LZMA block requires an XZ CRC32 stream");
            xz_buf buffer{encoded.get(), 0, counts[i], output, 0, length};
            const auto result = xz_dec_run(xz.get(), &buffer);
            valid(result == XZ_STREAM_END && buffer.in_pos == buffer.in_size && buffer.out_pos == length,
                  "Invalid or incorrectly sized VTI LZMA block");
        } else {
            const auto result = decode_deflate_member(encoded.get(), counts[i], output, length,
                                                      DeflateWrapper::zlib, [&]{ checkpoint(cancel); });
            if (result == GzipResult::workspace_failure)
                fail(QPX_READER_INTERNAL_ERROR, "VTI zlib workspace allocation failed");
            valid(result == GzipResult::ok, "Invalid or incorrectly sized VTI zlib block");
        }
        output += length;
    }
}

template<class Input>
void binary_samples(Input& input, std::uint8_t* output, std::size_t expected,
                    unsigned header_bytes, bool big, Compression compression,
                    Budget& budget, qpx_reader_cancel_v1 cancel) {
    if (compression != Compression::None) {
        compressed_samples(input, output, expected, header_bytes, big, compression, budget, cancel);
    } else {
        valid(binary_word(input, header_bytes, big) == expected, "VTI binary byte count differs from geometry");
        input.read(output, expected);
    }
    input.finish();
}

using Extent = std::array<std::int32_t, 6>;
struct Piece {
    Extent extent{};
    const Sample* sample = nullptr;
    std::string_view channel, format, content;
    unsigned components = 1;
    bool vector = false;
    std::vector<std::pair<unsigned, std::string_view>> component_names;
    std::size_t count = 0, position = 0;
};

Piece read_piece(const XMLElement& element, const Extent& whole, Budget& budget,
                 std::string_view selected, bool appended, std::vector<std::size_t>& offsets) {
    attributes(element, {"Extent"});
    Piece piece;
    piece.extent = numbers<std::int32_t, 6>(attribute(element, "Extent"));
    piece.count = 1;
    for (unsigned i = 0; i < 6; i += 2) {
        const auto lo = piece.extent[i], hi = piece.extent[i + 1];
        valid(lo >= whole[i] && hi >= lo && hi <= whole[i + 1], "VTI piece extends outside the whole extent");
        piece.extent[i] = static_cast<std::int32_t>(std::int64_t(lo) - whole[i]);
        piece.extent[i + 1] = static_cast<std::int32_t>(std::int64_t(hi) - whole[i]);
        piece.count *= std::size_t(piece.extent[i + 1] - piece.extent[i]) + 1;
    }
    const XMLElement* points = nullptr;
    const XMLElement* cells = nullptr;
    for (auto* node = element.FirstChild(); node; node = node->NextSibling()) {
        if (!nonempty(*node)) continue;
        const auto* child = node->ToElement();
        valid(child, "Invalid VTI piece text");
        if (!points && child->Name() == "PointData"sv) points = child;
        else if (!cells && child->Name() == "CellData"sv) cells = child;
        else valid(false, "Unsupported or duplicate VTI piece data");
    }
    valid(points, "VTI PointData is missing");
    if (cells) {
        attributes(*cells, {});
        auto content = leaf(*cells);
        valid(next_token(content).empty(), "VTI requires empty CellData");
    }
    attributes(*points, {"Scalars", "Vectors"});
    const auto scalar = attribute(*points, "Scalars"), vector = attribute(*points, "Vectors");
    if (selected.empty()) {
        valid(scalar.empty() || vector.empty() || scalar == vector,
              "VTI has distinct active point arrays; specify point_array");
        selected = scalar.empty() ? vector : scalar;
        valid(!selected.empty(), "VTI requires an active point array or explicit point_array");
    }
    std::array<std::string_view, 64> names{};
    std::size_t count = 0;
    const XMLElement* match = nullptr;
    for (auto* child = points->FirstChild(); child; child = child->NextSibling()) {
        if (!nonempty(*child)) continue;
        const auto* array = child->ToElement();
        valid(array && array->Name() == "DataArray"sv && count < names.size(),
              "Unexpected VTI point array or array limit exceeded");
        const auto name = attribute(*array, "Name"), format = attribute(*array, "format");
        valid(!name.empty() && name.size() <= 128 &&
                  std::find(names.begin(), names.begin() + count, name) == names.begin() + count,
              "Invalid or duplicate VTI point array name");
        names[count++] = name;
        const auto offset = attribute(*array, "offset");
        std::size_t position = 0;
        if (appended) {
            valid(format == "appended" && !offset.empty(), "VTI AppendedData requires an offset for every array");
            position = number<std::size_t>(offset);
            offsets.push_back(position);
        } else {
            valid((format == "ascii" || format == "binary") && offset.empty(), "Unsupported VTI array encoding");
        }
        if (name == selected) {
            match = array;
            piece.position = position;
        }
    }
    for (const auto active : {scalar, vector})
        valid(active.empty() || std::find(names.begin(), names.begin() + count, active) != names.begin() + count,
              "VTI point array name must match its active scalar or vector");
    valid(match, "Selected VTI point_array is missing from a piece");
    const auto& array = *match;
    const auto components = attribute(array, "NumberOfComponents");
    if (!components.empty())
        piece.components = number<unsigned>(components);
    valid(piece.components > 0 && piece.components <= 65536, "Invalid VTI component count");
    for (auto* attr = array.FirstAttribute(); attr; attr = attr->Next()) {
        const std::string_view name = attr->Name();
        constexpr std::string_view prefix = "ComponentName";
        if (name.substr(0, prefix.size()) == prefix) {
            const auto digits = name.substr(prefix.size());
            const auto index = number<unsigned>(digits);
            const std::string_view label = attr->Value();
            valid(index < piece.components && (digits.size() == 1 || digits.front() != '0') &&
                      !label.empty() && label.size() <= 128, "Invalid VTI component name");
            constexpr auto entry = 2 * sizeof(decltype(piece.component_names)::value_type);
            require(budget.remaining >= entry, "VTI component names exceed reader budget");
            budget.remaining -= entry;
            piece.component_names.emplace_back(index, label);
        } else {
            valid(name == "type" || name == "Name" || name == "format" || name == "RangeMin" ||
                      name == "RangeMax" || name == "NumberOfComponents" || name == "offset",
                  "Unsupported VTI XML attribute");
        }
    }
    std::sort(piece.component_names.begin(), piece.component_names.end());
    piece.channel = attribute(array, "Name");
    piece.vector = vector == piece.channel;
    valid(!piece.vector || piece.components == 3, "VTI active vectors require three components");
    const auto type = attribute(array, "type");
    piece.sample = std::find_if(std::begin(types), std::end(types),
                               [&](const auto& entry) { return entry.name == type; });
    valid(piece.sample != std::end(types), "Unsupported VTI scalar type");
    piece.format = attribute(array, "format");
    piece.content = point_payload(array, piece.components);
    if (appended)
        valid(next_token(piece.content).empty(), "VTI appended array contains inline text");
    return piece;
}

std::string_view appended_base64(std::string_view xml) {
    const auto start = xml.find("<AppendedData");
    valid(start != xml.npos, "VTI appended data is missing");
    const auto tag_end = xml.find('>', start);
    valid(tag_end != xml.npos, "VTI appended data tag is incomplete");
    const auto marker = xml.find_first_not_of(xml_space, tag_end + 1);
    const auto close = xml.rfind("</AppendedData>");
    valid(marker != xml.npos && close != xml.npos && marker < close && xml[marker] == '_',
          "VTI appended data marker is missing");
    return xml.substr(marker + 1, close - marker - 1);
}

void decode_piece(const Piece& piece, std::string_view encoded, std::uint8_t* output,
                  unsigned header_bytes, bool big, Compression compression, bool raw,
                  Budget budget, qpx_reader_cancel_v1 cancel) {
    const auto bytes = piece.sample->bytes;
    const auto values = piece.count * piece.components;
    if (piece.format == "ascii") {
        for (std::size_t i = 0; i < values; ++i) {
            if (!(i & 4095)) checkpoint(cancel);
            piece.sample->parse(next_token(encoded), output + i * bytes);
        }
        valid(next_token(encoded).empty(), "VTI ASCII scalar count differs from geometry");
        return;
    }
    if (raw) {
        RawBytes input(encoded, cancel);
        binary_samples(input, output, values * bytes, header_bytes, big, compression, budget, cancel);
    } else {
        Base64 input(encoded, cancel);
        binary_samples(input, output, values * bytes, header_bytes, big, compression, budget, cancel);
    }
    if (big && bytes > 1)
        for (std::size_t i = 0; i < values; ++i) {
            if (!(i & 4095)) checkpoint(cancel);
            auto* value = output + i * bytes;
            std::reverse(value, value + bytes);
        }
}

std::size_t merge_piece(const Piece& piece, const std::uint8_t* input, const Raster& raster,
                        std::uint8_t* seen, qpx_reader_cancel_v1 cancel) {
    const auto& e = piece.extent;
    const auto bytes = piece.sample->bytes * piece.components;
    std::size_t added = 0;
    for (std::int64_t z = e[4]; z <= e[5]; ++z)
        for (std::int64_t y = e[2]; y <= e[3]; ++y) {
            auto position = (std::uint64_t(z) * raster.height + y) * raster.width + e[0];
            for (std::int64_t x = e[0]; x <= e[1]; ++x, ++position, input += bytes) {
                if (!((x - e[0]) & 4095)) checkpoint(cancel);
                auto* target = raster.raw.get() + position * bytes;
                if (seen[position]) {
                    valid(std::memcmp(target, input, bytes) == 0, "VTI overlapping pieces disagree");
                } else {
                    std::memcpy(target, input, bytes);
                    seen[position] = 1;
                    ++added;
                }
            }
        }
    return added;
}

struct Codec {
    static constexpr const char* provider_id = "org.qupathxx.vti";
    static constexpr const char* name = "VTK ImageData";
    static constexpr const char* version = QPX_VTI_VERSION;
    static constexpr const char* contract = QPX_VTI_CONTRACT;
    static constexpr std::size_t input_limit = 32 * 1024 * 1024;

    static Raster decode(std::uint8_t* bytes, std::size_t size, Budget& budget,
                         qpx_reader_cancel_v1 cancel, std::string_view options) {
        const auto selection = select(options, cancel);
        const auto scale = selection.scale;

        std::string_view xml(reinterpret_cast<const char*>(bytes), size);
        const auto raw = split_raw_appended(xml);
        std::unique_ptr<std::uint8_t[]> metadata;
        if (raw) {
            metadata = budget.array(raw.prefix.size() + raw.suffix.size());
            std::memcpy(metadata.get(), raw.prefix.data(), raw.prefix.size());
            std::memcpy(metadata.get() + raw.prefix.size(), raw.suffix.data(), raw.suffix.size());
            xml = {reinterpret_cast<const char*>(metadata.get()), raw.prefix.size() + raw.suffix.size()};
        }
        verify_markup(xml, cancel);
        constexpr std::size_t parser_overhead = 1024 * 1024;
        require(budget.remaining > xml.size() + parser_overhead, "VTI XML parse exceeds reader budget");
        budget.remaining -= xml.size() + parser_overhead;
        tinyxml2::XMLDocument doc(false);
        valid(doc.Parse(xml.data(), xml.size()) == tinyxml2::XML_SUCCESS, "Invalid VTI XML document");
        const auto* root = doc.RootElement();
        valid(root && root->Name() == "VTKFile"sv && !root->NextSibling(), "VTI requires exactly one VTKFile root");
        attributes(*root, {"type", "version", "byte_order", "header_type", "compressor"});
        const auto compressor = attribute(*root, "compressor");
        auto compression = Compression::None;
        if (compressor == "vtkLZ4DataCompressor")
            compression = Compression::Lz4;
        else if (compressor == "vtkZLibDataCompressor")
            compression = Compression::Zlib;
        else if (compressor == "vtkLZMADataCompressor")
            compression = Compression::Lzma;
        else
            valid(compressor.empty(), "Unsupported VTI compressor");
        const auto file_version = attribute(*root, "version");
        valid(attribute(*root, "type") == "ImageData" &&
                  (file_version == "0.1" || file_version == "1.0" ||
                   file_version == "2.0" || file_version == "2.2"),
              "Unsupported VTI dataset type or version");
        const auto byte_order = attribute(*root, "byte_order");
        valid(byte_order == "LittleEndian" || byte_order == "BigEndian", "Invalid VTI byte order");
        const auto header = attribute(*root, "header_type");
        valid(header.empty() || header == "UInt32" || header == "UInt64", "Invalid VTI header type");
        const auto* image = root->FirstChildElement();
        valid(image && image->Name() == "ImageData"sv, "VTI ImageData is missing");
        const auto* appended = image->NextSiblingElement();
        valid(!appended || (appended->Name() == "AppendedData"sv && !appended->NextSiblingElement()),
              "Unsupported VTI root children");
        for (auto* node = root->FirstChild(); node; node = node->NextSibling())
            valid(node->ToElement() || !nonempty(*node), "Invalid VTI root text");
        attributes(*image, {"WholeExtent", "Origin", "Spacing", "Direction"});
        const auto extent = numbers<std::int32_t, 6>(attribute(*image, "WholeExtent"));
        const auto origin = numbers<double, 3>(attribute(*image, "Origin"));
        const auto direction = attribute(*image, "Direction");
        valid(direction.empty() ||
                  numbers<double, 9>(direction) == std::array<double, 9>{1,0,0,0,1,0,0,0,1},
              "VTI direction must be identity");
        const auto spacing = numbers<double, 3>(attribute(*image, "Spacing"));
        Raster raster;
        std::array<std::uint32_t, 3> shape{};
        Extent local_extent{};
        std::uint64_t count = 1;
        for (unsigned i = 0; i < 3; ++i) {
            const auto first = extent[i * 2], last = extent[i * 2 + 1];
            valid(last >= first, "VTI extent is reversed");
            const auto points = std::uint64_t(std::int64_t(last) - first) + 1;
            require(points <= work_limit / count, "VTI raster exceeds reader limits");
            shape[i] = static_cast<std::uint32_t>(points);
            local_extent[i * 2 + 1] = static_cast<std::int32_t>(points - 1);
            count *= shape[i];
            const auto micrometres = spacing[i] * scale;
            valid(std::isfinite(spacing[i]) && spacing[i] > 0 && std::isfinite(micrometres) &&
                      (!scale || micrometres > 0), "Invalid VTI spacing");
            valid(std::fma(double(first), spacing[i], origin[i]) == 0,
                  "VTI first sample must be at physical zero");
            require(scale || spacing[i] == 1, "Nonunit VTI spacing requires explicit spacing_unit");
            raster.spacing[i] = micrometres;
        }
        raster.width = shape[0];
        raster.height = shape[1];
        raster.z_planes = shape[2];
        std::vector<Piece> pieces;
        std::vector<std::size_t> offsets;
        for (auto* node = image->FirstChild(); node; node = node->NextSibling()) {
            if (!nonempty(*node)) continue;
            const auto* element = node->ToElement();
            valid(element && element->Name() == "Piece"sv && pieces.size() < 64,
                  "Unexpected VTI XML element or piece limit exceeded");
            pieces.push_back(read_piece(*element, extent, budget, selection.point_array, appended != nullptr, offsets));
        }
        valid(!pieces.empty(), "VTI requires at least one piece");
        if (pieces.size() == 1)
            valid(pieces.front().extent == local_extent, "VTI piece extent must equal the whole extent");
        const auto& first = pieces.front();
        raster.sample = first.sample;
        const auto voxel_bytes = raster.sample->bytes * first.components;
        const auto decoded_limit = work_limit / voxel_bytes;
        require(count <= decoded_limit, "VTI raster exceeds decoded size limit");
        const auto names_bytes = first.components * (sizeof(std::string) + 160);
        require(budget.remaining >= names_bytes, "VTI component labels exceed reader budget");
        budget.remaining -= names_bytes;
        raster.names.reserve(first.components);
        auto named = first.component_names.begin();
        for (unsigned i = 0; i < first.components; ++i) {
            if (!(i & 4095)) checkpoint(cancel);
            if (named != first.component_names.end() && named->first == i) {
                raster.names.emplace_back(named->second);
                ++named;
            } else {
                auto& label = raster.names.emplace_back();
                if (first.components > 1)
                    label.reserve(first.channel.size() + 8);
                label.append(first.channel);
                if (first.components > 1)
                    label += " [" + std::to_string(i + 1) + "]";
            }
        }
        std::uint64_t decoded = 0;
        std::size_t largest_piece = 0;
        for (auto& piece : pieces) {
            valid(piece.sample == raster.sample && piece.channel == first.channel,
                  "VTI pieces must use the same active scalar name and type");
            valid(piece.vector == first.vector && piece.components == first.components &&
                      piece.component_names == first.component_names, "VTI piece component descriptions differ");
            require(piece.count <= decoded_limit - decoded, "VTI piece data exceeds the decoded size limit");
            decoded += piece.count;
            largest_piece = std::max(largest_piece, piece.count);
        }

        bool use_raw = false;
        std::string_view appended_data;
        if (appended) {
            attributes(*appended, {"encoding"});
            (void)leaf(*appended);
            const auto encoding = attribute(*appended, "encoding");
            use_raw = encoding == "raw";
            valid(use_raw ? bool(raw) : encoding == "base64" && !raw, "Unsupported VTI appended encoding");
            appended_data = use_raw ? raw.body : appended_base64(xml);
            std::sort(offsets.begin(), offsets.end());
            valid(!offsets.empty() && offsets.front() == 0, "VTI requires the first appended array at offset zero");
            for (std::size_t i = 0; i < offsets.size(); ++i)
                valid(offsets[i] < appended_data.size() && (!i || offsets[i - 1] < offsets[i]),
                      "VTI appended offsets must be distinct and within the body");
        }
        raster.raw = budget.array(count * voxel_bytes);
        const unsigned header_bytes = header == "UInt64" ? 8 : 4;
        const auto decode = [&](std::size_t index, std::uint8_t* output) {
            checkpoint(cancel);
            const auto& piece = pieces[index];
            auto content = piece.content;
            if (appended) {
                const auto next = std::upper_bound(offsets.begin(), offsets.end(), piece.position);
                const auto end = next == offsets.end() ? appended_data.size() : *next;
                content = appended_data.substr(piece.position, end - piece.position);
            }
            decode_piece(piece, content, output, header_bytes, byte_order == "BigEndian",
                         compression, use_raw, budget, cancel);
        };
        if (pieces.size() == 1) {
            decode(0, raster.raw.get());
        } else {
            auto seen = budget.array(count);
            std::memset(seen.get(), 0, std::size_t(count));
            auto scratch = budget.array(largest_piece * voxel_bytes);
            std::size_t covered = 0;
            for (std::size_t i = 0; i < pieces.size(); ++i) {
                decode(i, scratch.get());
                covered += merge_piece(pieces[i], scratch.get(), raster, seen.get(), cancel);
            }
            valid(covered == count, "VTI pieces do not cover the whole extent");
        }
        return raster;
    }
};
using Reader = PackedImageReader<Codec, Raster, true, true>;
}
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t version, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return Reader::entry(version, out, error);
}
