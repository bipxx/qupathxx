// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/rgba8_reader.hpp"
#include "../image-common/gzip_member.hpp"
#include "fits_rice.hpp"

#include <charconv>
#include <bitset>
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
using qpx::image_plugin::decode_gzip_member;
using qpx::image_plugin::gzip_workspace_bytes;
using qpx::image_plugin::GzipResult;

constexpr std::size_t card_size = 80;
constexpr std::size_t block_size = 2880;
constexpr std::size_t header_block_limit = 64;
constexpr unsigned hdu_limit = 64;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 &&
                  sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "FITS floating samples require IEEE-754 binary32 and binary64");

struct Window {
    double low, high;
};
struct Selection {
    std::optional<unsigned> hdu;
    std::optional<Window> window;
};

bool digit(char value) { return value >= '0' && value <= '9'; }

double json_number(std::string_view value, std::size_t& position) {
    const auto start = position;
    if (position < value.size() && value[position] == '-')
        ++position;
    require(position < value.size() && digit(value[position]), "FITS window requires JSON numbers");
    if (value[position] == '0') {
        ++position;
        require(position == value.size() || !digit(value[position]),
                "FITS window number has a leading zero");
    } else {
        while (position < value.size() && digit(value[position]))
            ++position;
    }
    if (position < value.size() && value[position] == '.') {
        const auto first = ++position;
        while (position < value.size() && digit(value[position]))
            ++position;
        require(position > first, "FITS window fraction is incomplete");
    }
    if (position < value.size() && (value[position] == 'e' || value[position] == 'E')) {
        ++position;
        if (position < value.size() && (value[position] == '+' || value[position] == '-'))
            ++position;
        const auto first = position;
        while (position < value.size() && digit(value[position]))
            ++position;
        require(position > first, "FITS window exponent is incomplete");
    }
    double result = 0;
    const auto parsed = std::from_chars(value.data() + start, value.data() + position, result,
                                        std::chars_format::general);
    require(parsed.ec == std::errc{} && parsed.ptr == value.data() + position &&
                std::isfinite(result),
            "FITS window numbers must be finite and representable");
    return result;
}

Selection parse_options(qpx_reader_string_v1 options) {
    require(options.size <= 512 && (!options.size || options.data), "FITS options are invalid");
    Selection result;
    if (!options.size)
        return result;
    std::string_view value(options.data, std::size_t(options.size));
    require(value.front() == '{', "FITS options require a compact object");
    value.remove_prefix(1);
    constexpr std::string_view hdu_prefix = "\"hdu\":";
    if (value.substr(0, hdu_prefix.size()) == hdu_prefix) {
        value.remove_prefix(hdu_prefix.size());
        const auto delimiter = value.find_first_not_of("0123456789");
        require(delimiter != std::string_view::npos && delimiter > 0 &&
                    (delimiter == 1 || value.front() != '0'),
                "FITS HDU option requires a zero-based unsigned integer");
        unsigned index = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + delimiter, index);
        require(parsed.ec == std::errc{} && index < hdu_limit, "FITS HDU index exceeds the reader limit");
        result.hdu = index;
        value.remove_prefix(delimiter);
        if (value == "}")
            return result;
        require(value.front() == ',', "FITS HDU option is followed by an unexpected field");
        value.remove_prefix(1);
    }
    constexpr std::string_view prefix = "\"window\":[";
    require(value.substr(0, prefix.size()) == prefix, "FITS options require a compact HDU or window object");
    auto position = prefix.size();
    const auto low = json_number(value, position);
    require(position < value.size() && value[position++] == ',', "FITS window requires two numbers");
    const auto high = json_number(value, position);
    require(value.substr(position) == "]}", "FITS window has unexpected or trailing fields");
    require(low < high && std::isfinite(high - low), "FITS window must have a finite positive span");
    result.window = Window{low, high};
    return result;
}


std::string_view trim_spaces(std::string_view value) {
    constexpr auto none = std::string_view::npos;
    const auto first = value.find_first_not_of(' ');
    if (first == none)
        return {};
    return value.substr(first, value.find_last_not_of(' ') - first + 1);
}

std::string_view card_value(std::string_view card) {
    valid(card.substr(8, 2) == "= ", "FITS keyword requires an indicator and a value");
    auto field = card.substr(10);
    const auto slash = field.find('/');
    field = trim_spaces(field.substr(0, slash));
    valid(!field.empty(), "FITS field value is missing");
    return field;
}

std::string_view card_string(std::string_view card) {
    valid(card.substr(8, 2) == "= ", "FITS keyword requires an indicator and a value");
    const auto field = trim_spaces(card.substr(10));
    valid(!field.empty() && field.front() == '\'', "FITS keyword requires a quoted string");
    for (std::size_t position = 1; position < field.size(); ++position) {
        if (field[position] != '\'')
            continue;
        if (position + 1 < field.size() && field[position + 1] == '\'') {
            ++position;
            continue;
        }
        const auto tail = trim_spaces(field.substr(position + 1));
        valid(tail.empty() || tail.front() == '/', "FITS string has trailing data");
        return field.substr(1, position - 1);
    }
    fail(QPX_READER_IO_ERROR, "FITS string is unterminated");
}

std::int64_t read_integer(std::string_view token) {
    if (!token.empty() && token.front() == '+') {
        token.remove_prefix(1);
        valid(!token.empty() && digit(token.front()), "Invalid FITS integer sign");
    }
    std::int64_t value = 0;
    valid(!token.empty(), "FITS integer is missing");
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    valid(parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size(),
          "FITS integer is malformed or outside its domain");
    return value;
}

bool read_logical(std::string_view token) {
    valid(token == "T" || token == "F", "FITS logical value must be T or F");
    return token == "T";
}

struct Decimal {
    std::array<char, 70> digits{};
    std::size_t size = 0;
    int exponent = 0;
    bool negative = false;

    bool equals(std::string_view integer) const {
        if (size == 0)
            return integer == "0";
        if (negative || integer == "0")
            return false;
        int trailing = 0;
        while (integer.back() == '0') {
            integer.remove_suffix(1);
            ++trailing;
        }
        return exponent == trailing && std::string_view(digits.data(), size) == integer;
    }
};

// Compare FITS scaling cards as exact decimals. Binary floating parsing can
// round distinct decimal offsets into the accepted 64-bit unsigned shift.
Decimal read_decimal(std::string_view token) {
    Decimal value;
    if (token.front() == '-' || token.front() == '+') {
        value.negative = token.front() == '-';
        token.remove_prefix(1);
    }
    bool point = false;
    bool any_digit = false;
    int fraction = 0;
    std::size_t index = 0;
    while (index < token.size()) {
        const auto next = token[index];
        if (next == '.' && !point) {
            point = true;
        } else if (digit(next)) {
            any_digit = true;
            if (point)
                ++fraction;
            if (value.size || next != '0')
                value.digits[value.size++] = next;
        } else {
            break;
        }
        ++index;
    }
    valid(any_digit, "FITS decimal has no significand");
    int exponent = 0;
    if (index < token.size()) {
        valid(token[index] == 'D' || token[index] == 'E', "FITS decimal is malformed");
        ++index;
        bool negative_exponent = false;
        if (index < token.size() && (token[index] == '+' || token[index] == '-'))
            negative_exponent = token[index++] == '-';
        valid(index < token.size(), "FITS decimal exponent is missing");
        for (; index < token.size(); ++index) {
            valid(digit(token[index]), "FITS decimal exponent is malformed");
            // Values far outside the largest possible adjustment of a
            // 70-character value field cannot match an accepted offset.
            if (exponent < 1000)
                exponent = std::min(1000, exponent * 10 + token[index] - '0');
        }
        if (negative_exponent)
            exponent = -exponent;
    }
    value.exponent = exponent - fraction;
    while (value.size && value.digits[value.size - 1] == '0') {
        --value.size;
        ++value.exponent;
    }
    return value;
}

struct FitsChecksum {
    std::uint32_t value = 0;

    void add_words(std::uint64_t words) {
        const auto sum = words + value;
        const auto folded = (sum & UINT32_MAX) + (sum >> 32);
        value = static_cast<std::uint32_t>(folded + (folded >> 32));
    }

    // The reader's chunks start on word boundaries. Only the last data
    // chunk can end mid-word, followed by the independently checked zero fill.
    void add(const std::uint8_t* bytes, std::size_t size) {
        std::uint64_t words = 0;
        std::size_t i = 0;
        for (; i + 4 <= size; i += 4)
            words += (std::uint32_t(bytes[i]) << 24) | (std::uint32_t(bytes[i + 1]) << 16) |
                     (std::uint32_t(bytes[i + 2]) << 8) | bytes[i + 3];
        for (unsigned shift = 24; i < size; ++i, shift -= 8)
            words += std::uint32_t(bytes[i]) << shift;
        add_words(words);
    }
};

enum class TableKind { none, binary, ascii };

struct TileCards {
    // Store only the reserved image and descriptor cards. Ordinary tables keep
    // their metadata opaque, including column values that resemble these keys.
    static constexpr std::array<std::string_view, 23> keys{
        "ZBITPIX", "ZNAXIS", "ZNAXIS1", "ZNAXIS2", "ZNAXIS3", "ZTILE1", "ZTILE2", "ZTILE3", "ZCMPTYPE", "TTYPE1",
        "TFORM1", "BSCALE", "BZERO", "BLANK", "ZBLANK", "ZTENSION", "ZPCOUNT", "ZGCOUNT",
        "ZQUANTIZ", "ZNAME1", "ZVAL1", "ZNAME2", "ZVAL2"};
    enum Field : std::size_t { bitpix, axes, width, height, depth, tile_width, tile_height, tile_depth, compression,
        column, format, scale, zero, blank, compressed_blank, extension, parameters, groups, quantization, name1 };
    std::array<std::optional<std::array<char, card_size>>, keys.size()> values;
    std::optional<bool> image;
    bool unsupported = false, duplicate = false;

    std::string_view get(std::size_t index) const {
        const auto& value = values[index];
        return value ? std::string_view(value->data(), value->size()) : std::string_view{};
    }
};

struct Header {
    std::array<std::uint32_t, 3> shape{1, 1, 1};
    int bitpix = 0;
    unsigned axes = 0;
    std::optional<Decimal> scale;
    std::optional<Decimal> zero;
    std::optional<std::int64_t> blank;
    std::optional<std::uint32_t> datasum;
    FitsChecksum header_sum;
    std::bitset<999> table_formats;
    std::bitset<999> table_columns;
    std::uint64_t table_extra_bytes = 0;
    std::optional<std::uint64_t> table_heap;
    std::size_t bytes = 0;
    unsigned table_fields = 0;
    TableKind table = TableKind::none;
    TileCards tiles;
    bool is_table() const { return table != TableKind::none; }
    bool have_extend = false;
    bool have_extname = false;
    bool have_extver = false;
    bool have_datasum = false;
    bool have_checksum = false;
    bool verify_checksum = false;
};

struct FitsInput {
    std::FILE* file;
    const std::uint8_t* decoded;
    std::uint64_t size;
    std::uint64_t position = 0;

    bool read(void* destination, std::size_t count) {
        if (count > size - position)
            return false;
        if (file) {
            if (std::fread(destination, 1, count, file) != count)
                return false;
        } else if (count) {
            std::memcpy(destination, decoded + static_cast<std::size_t>(position), count);
        }
        position += count;
        return true;
    }
    bool seek(std::uint64_t offset) {
        if (offset > size || (file && _fseeki64(file, static_cast<__int64>(offset), SEEK_SET) != 0))
            return false;
        position = offset;
        return true;
    }
    bool at_end() const {
        return position == size && (!file || (std::fgetc(file) == EOF && !std::ferror(file)));
    }
};

std::size_t mandatory_cards(const Header& header, bool extension) {
    return 3 + header.axes + (extension ? 2 : 0) + (header.is_table() ? 1 : 0);
}

std::size_t table_column_index(std::string_view suffix, unsigned fields) {
    unsigned field = 0;
    const auto result = std::from_chars(suffix.data(), suffix.data() + suffix.size(), field);
    valid(result.ec == std::errc{} && result.ptr == suffix.data() + suffix.size() &&
              field > 0 && field <= fields, "FITS table column index is outside TFIELDS");
    return static_cast<std::size_t>(field - 1);
}

void read_table_card(std::string_view card, std::string_view key, Header& header) {
    if (header.table == TableKind::binary) {
        const auto found = std::find(TileCards::keys.begin(), TileCards::keys.end(), key);
        if (found != TileCards::keys.end()) {
            auto& value = header.tiles.values[std::size_t(found - TileCards::keys.begin())];
            if (value)
                header.tiles.duplicate = true;
            else
                std::copy(card.begin(), card.end(), value.emplace().begin());
        } else if ((!key.empty() && key.front() == 'Z' && key != "ZIMAGE" && key != "ZTABLE") ||
                   key.substr(0, 5) == "TSCAL" || key.substr(0, 5) == "TZERO" || key.substr(0, 5) == "TNULL") {
            header.tiles.unsupported = true;
        }
    }
    if (key == "THEAP") {
        const auto value = read_integer(card_value(card));
        valid(header.table == TableKind::binary && !header.table_heap && value >= 0,
              "FITS table THEAP is duplicate or invalid");
        header.table_heap = static_cast<std::uint64_t>(value);
    } else if (key.substr(0, 5) == "TFORM") {
        const auto index = table_column_index(key.substr(5), header.table_fields);
        valid(!header.table_formats[index] && !trim_spaces(card_string(card)).empty(),
              "FITS table TFORM is duplicate or empty");
        header.table_formats.set(index);
    } else if (header.table == TableKind::ascii && key.substr(0, 5) == "TBCOL") {
        const auto index = table_column_index(key.substr(5), header.table_fields);
        const auto value = read_integer(card_value(card));
        valid(!header.table_columns[index] && value > 0 && value <= header.shape[0],
              "FITS ASCII table TBCOL is duplicate or outside its row");
        header.table_columns.set(index);
    } else if (key == "TFIELDS") {
        fail(QPX_READER_IO_ERROR, "FITS binary table TFIELDS is duplicated");
    } else if (key == "ZIMAGE" && header.table == TableKind::binary) {
        valid(!header.tiles.image, "FITS ZIMAGE is duplicated");
        header.tiles.image = read_logical(card_value(card));
    } else if (key == "ZIMAGE" || key == "ZTABLE") {
        if (read_logical(card_value(card)))
            fail(QPX_READER_UNSUPPORTED, "FITS tile compression is outside this profile");
    }
    // Column metadata is opaque here. No field, descriptor or heap reference is decoded.
}

void read_card(std::string_view card, std::size_t index, Header& header, bool extension) {
    const auto key = trim_spaces(card.substr(0, 8));
    if (index == 0) {
        if (extension) {
            valid(key == "XTENSION", "FITS expected an extension after its padded data");
            auto type = card_string(card);
            while (!type.empty() && type.back() == ' ')
                type.remove_suffix(1);
            if (type == "BINTABLE")
                header.table = TableKind::binary;
            else if (type == "TABLE")
                header.table = TableKind::ascii;
            else if (type != "IMAGE")
                fail(QPX_READER_UNSUPPORTED, "FITS bundle accepts IMAGE, TABLE and BINTABLE extensions");
            return;
        }
        if (key != "SIMPLE")
            fail(QPX_READER_UNSUPPORTED, "FITS primary signature is missing");
        if (!read_logical(card_value(card)))
            fail(QPX_READER_UNSUPPORTED, "FITS primary array must declare SIMPLE = T");
        return;
    }
    if (index == 1) {
        valid(key == "BITPIX", "FITS BITPIX must follow SIMPLE");
        const auto value = read_integer(card_value(card));
        if (header.is_table())
            valid(value == 8, "FITS table BITPIX must be 8");
        if (value != 8 && value != 16 && value != 32 && value != 64 && value != -32 && value != -64)
            fail(QPX_READER_UNSUPPORTED, "FITS BITPIX type is outside the primary scalar profile");
        header.bitpix = static_cast<int>(value);
        return;
    }
    if (index == 2) {
        valid(key == "NAXIS", "FITS NAXIS must follow BITPIX");
        const auto value = read_integer(card_value(card));
        if (header.is_table())
            valid(value == 2, "FITS table NAXIS must be 2");
        if (value != 2 && value != 3 && (value != 0 || extension))
            fail(QPX_READER_UNSUPPORTED, "FITS primary array must have two or three axes");
        header.axes = static_cast<unsigned>(value);
        return;
    }
    constexpr std::array<std::string_view, 3> dimension_keys{"NAXIS1", "NAXIS2", "NAXIS3"};
    if (index < 3 + header.axes) {
        const auto dimension = index - 3;
        valid(key == dimension_keys[dimension], "FITS dimension cards must immediately follow NAXIS in order");
        const auto value = read_integer(card_value(card));
        valid(header.is_table() ? value >= 0 : value > 0, "FITS axis is outside the supported domain");
        require(static_cast<std::uint64_t>(value) <= UINT32_MAX, "FITS axis exceeds the reader limit");
        header.shape[dimension] = static_cast<std::uint32_t>(value);
        return;
    }
    if (extension && index < mandatory_cards(header, extension)) {
        if (header.is_table() && index == 5 + header.axes) {
            valid(key == "TFIELDS", "FITS table TFIELDS must follow GCOUNT");
            const auto fields = read_integer(card_value(card));
            valid(fields >= 0 && fields <= 999, "FITS table TFIELDS is outside its domain");
            header.table_fields = static_cast<unsigned>(fields);
            return;
        }
        const bool parameters = index == 3 + header.axes;
        valid(key == (parameters ? "PCOUNT" : "GCOUNT"),
              "FITS extension requires PCOUNT and GCOUNT immediately after its dimensions");
        const auto value = read_integer(card_value(card));
        if (header.table == TableKind::binary && parameters) {
            valid(value >= 0, "FITS binary table PCOUNT must be nonnegative");
            require(static_cast<std::uint64_t>(value) <= work_limit, "FITS binary table heap exceeds the work limit");
            header.table_extra_bytes = static_cast<std::uint64_t>(value);
        } else if (header.is_table()) {
            valid(value == (parameters ? 0 : 1), "FITS table requires PCOUNT=0 for ASCII and GCOUNT=1");
        } else if (value != (parameters ? 0 : 1)) {
            fail(QPX_READER_UNSUPPORTED, "FITS IMAGE supports only PCOUNT=0 and GCOUNT=1");
        }
        return;
    }
    if (key.empty() || key == "COMMENT" || key == "HISTORY")
        return;
    if (key == "SIMPLE" || key == "XTENSION" || key == "BITPIX" || key.substr(0, 5) == "NAXIS" ||
        (extension && (key == "PCOUNT" || key == "GCOUNT")))
        fail(QPX_READER_IO_ERROR, "FITS structural keyword is duplicated or out of order");
    if (header.is_table() && key != "DATASUM" && key != "CHECKSUM") {
        read_table_card(card, key, header);
    } else if (key == "BSCALE") {
        valid(!header.scale, "FITS BSCALE is duplicated");
        header.scale = read_decimal(card_value(card));
    } else if (key == "BZERO") {
        valid(!header.zero, "FITS BZERO is duplicated");
        header.zero = read_decimal(card_value(card));
    } else if (key == "BLANK") {
        valid(!header.blank, "FITS BLANK is duplicated");
        header.blank = read_integer(card_value(card));
    } else if (key == "DATASUM") {
        valid(!header.have_datasum, "FITS DATASUM is duplicated");
        header.have_datasum = true;
        const auto field = trim_spaces(card_string(card));
        if (!field.empty()) {
            std::uint32_t sum = 0;
            const auto parsed = std::from_chars(field.data(), field.data() + field.size(), sum);
            valid(parsed.ec == std::errc{} && parsed.ptr == field.data() + field.size(),
                  "FITS DATASUM is outside the unsigned decimal domain");
            header.datasum = sum;
        }
    } else if (key == "CHECKSUM") {
        valid(!header.have_checksum, "FITS CHECKSUM is duplicated");
        header.have_checksum = true;
        const auto field = card_string(card);
        if (!trim_spaces(field).empty()) {
            valid(card[10] == '\'' && field.size() == 16 &&
                      std::all_of(field.begin(), field.end(), [](char c) {
                          return digit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
                      }),
                  "FITS CHECKSUM is not a canonical 16-character value");
            header.verify_checksum = true;
        }
    } else if (key == "EXTEND" && !extension) {
        valid(!header.have_extend, "FITS EXTEND is duplicated");
        (void)read_logical(card_value(card));
        header.have_extend = true;
    } else if (key == "EXTNAME" && extension) {
        valid(!header.have_extname, "FITS EXTNAME is duplicated");
        (void)card_string(card);
        header.have_extname = true;
    } else if (key == "EXTVER" && extension) {
        valid(!header.have_extver && read_integer(card_value(card)) > 0, "FITS EXTVER is duplicate or invalid");
        header.have_extver = true;
    } else {
        fail(QPX_READER_UNSUPPORTED, "FITS keyword is outside the supported primary scalar profile");
    }
}

Header read_header(FitsInput& input, bool extension, qpx_reader_cancel_v1 cancel) {
    Header header;
    std::array<char, block_size> block{};
    std::size_t index = 0;
    for (std::size_t block_index = 0; block_index < header_block_limit; ++block_index) {
        checkpoint(cancel);
        valid(input.read(block.data(), block.size()),
              "FITS header block is truncated");
        header.bytes += block_size;
        header.header_sum.add(reinterpret_cast<const std::uint8_t*>(block.data()), block.size());
        const std::string_view bytes(block.data(), block.size());
        for (std::size_t position = 0; position < block.size(); position += card_size, ++index) {
            const auto card = bytes.substr(position, card_size);
            if (!extension && index == 0 && card.substr(0, 8) != "SIMPLE  ")
                fail(QPX_READER_UNSUPPORTED, "FITS primary signature is missing");
            valid(std::all_of(card.begin(), card.end(), [](char c) { return c >= ' ' && c <= '~'; }),
                  "FITS header cards must be printable ASCII");
            if (trim_spaces(card.substr(0, 8)) == "END") {
                const auto tail = bytes.substr(position + 3);
                valid(index >= mandatory_cards(header, extension) &&
                          std::all_of(tail.begin(), tail.end(), [](char c) { return c == ' '; }),
                      "FITS END card or header space padding is invalid");
                valid(!header.is_table() || header.table_formats.count() == header.table_fields,
                      "FITS table is missing a TFORM declaration");
                valid(header.table != TableKind::ascii || header.table_columns.count() == header.table_fields,
                      "FITS ASCII table is missing a TBCOL declaration");
                return header;
            }
            read_card(card, index, header, extension);
        }
    }
    fail(QPX_READER_INVALID_REQUEST, "FITS header exceeds the 64-block limit");
}

struct SampleMode {
    std::uint32_t type, bytes;
    Window window;
    bool flip_sign_bit = false;
};

SampleMode select_mode(const Header& header) {
    if (header.scale && !header.scale->equals("1"))
        fail(QPX_READER_UNSUPPORTED, "FITS BSCALE must equal 1 exactly");
    const bool shift = header.zero && !header.zero->equals("0");
    const auto require_offset = [&](std::string_view value) {
        if (!header.zero->equals(value))
            fail(QPX_READER_UNSUPPORTED, "FITS BZERO is outside the exact unsigned convention");
    };
    if (shift) {
        switch (header.bitpix) {
        case 16:
            require_offset("32768");
            return {QPX_READER_UINT16, 2, {0, 65535}, true};
        case 32:
            require_offset("2147483648");
            return {QPX_READER_UINT32, 4, {0, 4294967295.0}, true};
        case 64:
            require_offset("9223372036854775808");
            return {QPX_READER_UINT64, 8, {0, static_cast<double>(UINT64_MAX)}, true};
        default: fail(QPX_READER_UNSUPPORTED, "FITS BZERO is unsupported for this BITPIX");
        }
    }
    switch (header.bitpix) {
    case 8: return {QPX_READER_UINT8, 1, {0, 255}};
    case 16: return {QPX_READER_INT16, 2, {-32768, 32767}};
    case 32: return {QPX_READER_INT32, 4, {-2147483648.0, 2147483647.0}};
    case 64: return {QPX_READER_INT64, 8, {static_cast<double>(INT64_MIN), static_cast<double>(INT64_MAX)}};
    case -32: return {QPX_READER_FLOAT32, 4, {-1, 1}};
    case -64: return {QPX_READER_FLOAT64, 8, {-1, 1}};
    default: fail(QPX_READER_INTERNAL_ERROR, "FITS reader reached an unvalidated BITPIX");
    }
}

struct Raster {
    std::array<std::uint32_t, 3> shape{};
    bool volume = false;
    SampleMode sample{};
    std::optional<std::uint64_t> blank_code;
    std::unique_ptr<std::uint8_t[]> pixels;

    const std::uint8_t* at(std::int64_t x, std::int64_t y, std::uint32_t z) const {
        const auto index = (std::size_t(z) * shape[1] + std::size_t(y)) * shape[0] + std::size_t(x);
        return pixels.get() + index * sample.bytes;
    }
};

std::optional<std::uint64_t> blank_code(const Header& header, const SampleMode& sample) {
    if (!header.blank)
        return std::nullopt;
    if (header.bitpix < 0)
        fail(QPX_READER_UNSUPPORTED, "FITS BLANK is limited to integer storage");
    const auto value = *header.blank;
    if (header.bitpix == 8) {
        valid(value >= 0 && value <= UINT8_MAX, "FITS BLANK is outside the stored integer domain");
        return static_cast<std::uint64_t>(value);
    }
    const auto sign_bit = UINT64_C(1) << (header.bitpix - 1);
    const auto largest = static_cast<std::int64_t>(sign_bit - 1);
    valid(value >= -largest - 1 && value <= largest, "FITS BLANK is outside the stored integer domain");
    const auto word = static_cast<std::uint64_t>(value) & (sign_bit | (sign_bit - 1));
    return sample.flip_sign_bit ? word ^ sign_bit : word;
}

std::uint64_t raster_size(const Header& header, const SampleMode& mode) {
    if (!header.axes)
        return 0;
    std::uint64_t bytes = mode.bytes;
    for (const auto length : header.shape) {
        require(length <= work_limit / bytes, "FITS raster exceeds the 256 MiB work limit");
        bytes *= length;
    }
    return bytes;
}

std::uint64_t table_size(const Header& header) {
    const auto width = header.shape[0];
    const auto rows = header.shape[1];
    require(!width || rows <= (work_limit - header.table_extra_bytes) / width,
            "FITS table extent exceeds the work limit");
    const auto row_bytes = std::uint64_t(width) * rows;
    const auto size = row_bytes + header.table_extra_bytes;
    if (header.table_heap)
        valid(*header.table_heap >= row_bytes && *header.table_heap <= size,
              "FITS binary table THEAP lies outside its data area");
    return size;
}

void normalize_samples(std::uint8_t* start, std::size_t count, const SampleMode& sample) {
    if (sample.bytes <= 1)
        return;
    for (std::size_t offset = 0; offset < count; offset += sample.bytes) {
        auto* word = start + offset;
        std::reverse(word, word + sample.bytes);
        if (sample.flip_sign_bit)
            word[sample.bytes - 1] ^= 0x80;
    }
}

void read_raster(FitsInput& input, Raster& raster, std::uint64_t bytes,
                 Budget& budget, FitsChecksum* checksum, qpx_reader_cancel_v1 cancel) {
    raster.pixels = budget.array(bytes);
    for (std::uint64_t position = 0; position < bytes;) {
        checkpoint(cancel);
        const auto count = std::size_t(std::min<std::uint64_t>(65536, bytes - position));
        auto* start = raster.pixels.get() + std::size_t(position);
        valid(input.read(start, count), "FITS raster is truncated or changed");
        if (checksum)
            checksum->add(start, count);
        // Each chunk starts and ends on a sample boundary.
        normalize_samples(start, count, raster.sample);
        position += count;
    }
}

struct TileLayout {
    std::array<std::uint32_t, 3> size{1, 1, 1}, grid{1, 1, 1};
    std::uint64_t heap = 0, member_limit = 0;
    std::uint32_t count = 0;
    unsigned descriptor_word = 0, rice_block_size = 0;
    bool shuffled = false;
};

template <class Handle> void tile_parameters(const TileCards& cards, Handle&& handle) {
    for (unsigned index = TileCards::name1; index < TileCards::keys.size(); index += 2) {
        const auto name = cards.get(index), value = cards.get(index + 1);
        if (name.empty() && value.empty())
            continue;
        valid(!name.empty() && !value.empty(), "Incomplete FITS compression parameter pair");
        handle(trim_spaces(card_string(name)), card_value(value));
    }
}

unsigned rice_parameters(const TileCards& cards, unsigned sample_bytes) {
    std::optional<unsigned> block, bytepix;
    tile_parameters(cards, [&](std::string_view name, std::string_view token) {
        valid(name == "BLOCKSIZE" || name == "BYTEPIX", "Unknown FITS Rice parameter");
        auto& selected = name == "BLOCKSIZE" ? block : bytepix;
        const auto value = read_integer(token);
        valid(!selected && value > 0 && value <= UINT32_MAX, "Invalid or duplicate FITS Rice parameter");
        selected = static_cast<unsigned>(value);
    });
    valid(sample_bytes <= 4 && bytepix.value_or(4) == sample_bytes, "FITS Rice BYTEPIX does not match integer BITPIX");
    return block.value_or(32);
}

void validate_lossless_tile_parameters(const TileCards& cards, bool floating) {
    bool explicit_zero = false;
    tile_parameters(cards, [&](std::string_view name, std::string_view token) {
        if (!floating || explicit_zero || name != "NOISEBIT" || !read_decimal(token).equals("0"))
            fail(QPX_READER_UNSUPPORTED, "FITS GZIP tiles require lossless compression parameters");
        explicit_zero = true;
    });
    const auto card = cards.get(TileCards::quantization);
    if (!card.empty()) {
        const auto mode = trim_spaces(card_string(card));
        if (!floating || (mode != "NONE" && !(mode == "NO_DITHER" && explicit_zero)))
            fail(QPX_READER_UNSUPPORTED, "FITS GZIP tiles require unquantized floating storage");
    }
}

TileLayout tile_layout(const Header& table, Raster& raster) {
    const auto& cards = table.tiles;
    using F = TileCards::Field;
    const auto integer = [&](std::size_t index) { return read_integer(card_value(cards.get(index))); };
    const auto string = [&](std::size_t index) { return trim_spaces(card_string(cards.get(index))); };
    if (cards.duplicate || cards.unsupported || table.table_fields != 1 ||
        cards.get(F::bitpix).empty() || cards.get(F::axes).empty() || cards.get(F::width).empty() ||
        cards.get(F::height).empty() || cards.get(F::compression).empty() || cards.get(F::column).empty() ||
        cards.get(F::format).empty() ||
        string(F::column) != "COMPRESSED_DATA")
        fail(QPX_READER_UNSUPPORTED, "FITS tile compression requires one compressed image column");
    const auto compression = string(F::compression);
    const bool rice = compression == "RICE_1";
    const bool shuffled = compression == "GZIP_2";
    if (compression != "GZIP_1" && !shuffled && !rice)
        fail(QPX_READER_UNSUPPORTED, "FITS tile compression requires GZIP_1, GZIP_2 or RICE_1");
    if ((!cards.get(F::extension).empty() && string(F::extension) != "IMAGE") ||
        (!cards.get(F::parameters).empty() && integer(F::parameters) != 0) ||
        (!cards.get(F::groups).empty() && integer(F::groups) != 1))
        fail(QPX_READER_UNSUPPORTED, "FITS compressed image must describe an ordinary image extension");
    Header image;
    const auto axes = integer(F::axes);
    if (axes != 2 && axes != 3)
        fail(QPX_READER_UNSUPPORTED, "FITS compressed image requires XY or XYZ axes");
    valid(axes == 3 ? !cards.get(F::depth).empty() :
          cards.get(F::depth).empty() && cards.get(F::tile_depth).empty(),
          "FITS compressed image Z cards disagree with ZNAXIS");
    image.axes = static_cast<unsigned>(axes);
    const auto bits = integer(F::bitpix);
    const bool floating = bits == -32 || bits == -64;
    if ((!floating && bits != 8 && bits != 16 && bits != 32 && bits != 64) ||
        (rice && (floating || !cards.get(F::quantization).empty())))
        fail(QPX_READER_UNSUPPORTED, "FITS compressed tile BITPIX or quantization is outside this profile");
    image.bitpix = static_cast<int>(bits);
    if (!cards.get(F::scale).empty()) image.scale = read_decimal(card_value(cards.get(F::scale)));
    if (!cards.get(F::zero).empty()) image.zero = read_decimal(card_value(cards.get(F::zero)));
    valid(cards.get(F::blank).empty() || cards.get(F::compressed_blank).empty(), "Duplicate FITS compressed image blank code");
    if (!cards.get(F::blank).empty()) image.blank = integer(F::blank);
    if (!cards.get(F::compressed_blank).empty()) image.blank = integer(F::compressed_blank);
    raster.sample = select_mode(image);
    raster.blank_code = blank_code(image, raster.sample);
    TileLayout layout;
    layout.shuffled = shuffled;
    if (rice)
        layout.rice_block_size = rice_parameters(cards, raster.sample.bytes);
    else
        validate_lossless_tile_parameters(cards, floating);
    for (unsigned axis = 0; axis < image.axes; ++axis) {
        const auto extent = integer(F::width + axis);
        const auto tile_key = F::tile_width + axis;
        const auto tile = !cards.get(tile_key).empty() ? integer(tile_key) : axis == 0 ? extent : 1;
        valid(extent > 0 && extent <= UINT32_MAX && tile > 0 && tile <= UINT32_MAX, "Invalid FITS compressed image axis or tile size");
        image.shape[axis] = static_cast<std::uint32_t>(extent);
        layout.size[axis] = static_cast<std::uint32_t>(std::min(extent, tile));
        layout.grid[axis] = 1 + (image.shape[axis] - 1) / layout.size[axis];
    }
    raster.shape = image.shape;
    raster.volume = image.axes == 3;
    (void)raster_size(image, raster.sample);
    valid(std::uint64_t(layout.grid[0]) * layout.grid[1] * layout.grid[2] == table.shape[1],
          "FITS tile count does not match the image grid");
    layout.count = table.shape[1];
    auto format = string(F::format);
    if (!format.empty() && format.front() == '1') format.remove_prefix(1);
    const auto code = format.substr(0, 2);
    valid(code == "PB" || code == "QB", "FITS tile column must contain byte-array descriptors");
    layout.descriptor_word = code == "PB" ? 4 : 8;
    valid(table.shape[0] == 2 * layout.descriptor_word, "FITS tile descriptor does not match the row width");
    format.remove_prefix(2);
    layout.member_limit = table.table_extra_bytes;
    if (!format.empty()) {
        valid(format.size() >= 3 && format.front() == '(' && format.back() == ')', "Invalid FITS tile descriptor limit");
        const auto maximum = read_integer(format.substr(1, format.size() - 2));
        valid(maximum > 0, "Invalid FITS tile descriptor limit");
        layout.member_limit = std::min(layout.member_limit, static_cast<std::uint64_t>(maximum));
    }
    layout.heap = table.table_heap.value_or(std::uint64_t(table.shape[0]) * table.shape[1]);
    return layout;
}

std::uint64_t tile_descriptor_word(const std::uint8_t* bytes, unsigned size) {
    std::uint64_t word = 0;
    for (unsigned i = 0; i < size; ++i)
        word = (word << 8) | bytes[i];
    return word;
}

void restore_tile_row(const std::uint8_t* tile, std::uint8_t* target, std::uint32_t width, std::size_t samples,
                      std::size_t row, const SampleMode& sample, bool shuffled) {
    const auto row_bytes = std::size_t(width) * sample.bytes;
    if (shuffled && sample.bytes > 1) {
        const auto* start = tile + std::size_t(width) * row;
        for (unsigned byte = 0; byte < sample.bytes; ++byte) {
            const auto* plane = start + byte * samples;
            for (std::uint32_t x = 0; x < width; ++x)
                target[std::size_t(x) * sample.bytes + byte] = plane[x];
        }
    } else {
        std::memcpy(target, tile + row * row_bytes, row_bytes);
    }
    normalize_samples(target, row_bytes, sample);
}

void read_tiles(FitsInput& input, std::uint64_t bytes, const TileLayout& layout, Raster& raster,
                Budget budget, FitsChecksum* checksum, qpx_reader_cancel_v1 cancel) {
    auto stored = budget.array(bytes);
    for (std::uint64_t at = 0; at < bytes;) {
        checkpoint(cancel);
        const auto count = std::size_t(std::min<std::uint64_t>(65536, bytes - at));
        valid(input.read(stored.get() + at, count), "FITS compressed image table is truncated or changed");
        if (checksum) checksum->add(stored.get() + at, count);
        at += count;
    }
    const auto sample_bytes = raster.sample.bytes;
    const auto raster_bytes = std::uint64_t(raster.shape[0]) * raster.shape[1] * raster.shape[2] * sample_bytes;
    raster.pixels = budget.array(raster_bytes);
    if (!layout.rice_block_size) {
        require(budget.remaining >= gzip_workspace_bytes, "FITS tile gzip workspace exceeds reader work budget");
        budget.remaining -= gzip_workspace_bytes;
    }
    auto tile = budget.array(std::uint64_t(layout.size[0]) * layout.size[1] * layout.size[2] * sample_bytes);
    const auto heap_bytes = bytes - layout.heap;
    const auto word = layout.descriptor_word;
    const auto* descriptor = stored.get();
    for (std::uint32_t index = 0; index < layout.count; ++index, descriptor += 2 * word) {
        checkpoint(cancel);
        const auto length = tile_descriptor_word(descriptor, word);
        const auto offset = tile_descriptor_word(descriptor + word, word);
        valid(length > 0 && length <= layout.member_limit && offset <= heap_bytes && length <= heap_bytes - offset,
              "FITS tile descriptor is outside the heap or declared size");
        // FITS orders table rows with X varying fastest, then Y, then Z.
        std::array<std::uint32_t, 3> origin{}, extent{};
        auto coordinate = index;
        for (unsigned axis = 0; axis < 3; ++axis) {
            origin[axis] = (coordinate % layout.grid[axis]) * layout.size[axis];
            coordinate /= layout.grid[axis];
            extent[axis] = std::min(layout.size[axis], raster.shape[axis] - origin[axis]);
        }
        const auto [x, y, z] = origin;
        const auto [width, height, depth] = extent;
        const auto samples = std::size_t(width) * height * depth;
        auto* compressed = stored.get() + layout.heap + offset;
        if (layout.rice_block_size) {
            qpx::fits_plugin::decode_rice(compressed, std::size_t(length), tile.get(), samples,
                                         sample_bytes, layout.rice_block_size, cancel);
        } else {
            const auto result = decode_gzip_member(compressed, std::size_t(length), tile.get(),
                samples * sample_bytes, [&] { checkpoint(cancel); });
            if (result == GzipResult::workspace_failure)
                fail(QPX_READER_INTERNAL_ERROR, "FITS tile gzip workspace could not be initialized");
            valid(result == GzipResult::ok, "FITS tile requires one complete gzip member with exact extent");
        }
        for (std::uint32_t dz = 0; dz < depth; ++dz) {
            for (std::uint32_t dy = 0; dy < height; ++dy) {
                checkpoint(cancel);
                const auto destination_row = std::size_t(z + dz) * raster.shape[1] + y + dy;
                auto* target = raster.pixels.get() + (destination_row * raster.shape[0] + x) * sample_bytes;
                restore_tile_row(tile.get(), target, width, samples, std::size_t(dz) * height + dy,
                                 raster.sample, layout.shuffled);
            }
        }
    }
}

void verify_padding(const std::uint8_t* start, std::size_t padding, std::uint8_t fill) {
    valid(std::all_of(start, start + padding, [fill](std::uint8_t value) { return value == fill; }),
          "FITS data block padding differs from its HDU type");
}

void read_checksum_data(FitsInput& input, std::uint64_t bytes, std::size_t padding,
                          std::uint8_t fill, FitsChecksum& sum,
                          qpx_reader_cancel_v1 cancel) {
    std::array<std::uint8_t, block_size> block{};
    while (bytes >= block.size()) {
        checkpoint(cancel);
        valid(input.read(block.data(), block.size()), "FITS data block is truncated or changed");
        sum.add(block.data(), block.size());
        bytes -= block.size();
    }
    if (bytes || padding) {
        checkpoint(cancel);
        const auto tail = static_cast<std::size_t>(bytes);
        valid(input.read(block.data(), tail + padding), "FITS final data block is truncated or changed");
        verify_padding(block.data() + tail, padding, fill);
        sum.add(block.data(), tail + padding);
    }
}

void read_padding(FitsInput& input, std::size_t padding, std::uint8_t fill) {
    std::array<std::uint8_t, block_size> bytes{};
    valid(input.read(bytes.data(), padding), "FITS data padding is truncated");
    verify_padding(bytes.data(), padding, fill);
}

Raster read_hdus(FitsInput& input, Budget& budget, const Selection& selection, qpx_reader_cancel_v1 cancel) {
    const auto file_size = input.size;
    valid(file_size >= block_size, "FITS primary header is truncated");
    std::optional<Raster> selected;
    std::uint64_t offset = 0;
    for (unsigned index = 0; offset < file_size; ++index) {
        require(index < hdu_limit, "FITS file exceeds the 64-HDU limit");
        const auto header = read_header(input, index != 0, cancel);
        if (!header.axes && !selection.hdu)
            fail(QPX_READER_UNSUPPORTED, "FITS primary array has no image; specify an image HDU");
        const bool chosen = index == selection.hdu.value_or(0);
        const bool tiled = header.tiles.image.value_or(false);
        if (chosen && header.is_table() && !tiled)
            fail(QPX_READER_UNSUPPORTED, header.table == TableKind::ascii ?
                 "FITS selected HDU is an ASCII table, not an image" :
                 "FITS selected HDU is a binary table, not an image");
        Raster raster;
        std::optional<TileLayout> tiles;
        if (!header.is_table()) {
            raster.shape = header.shape;
            raster.volume = header.axes == 3;
            raster.sample = select_mode(header);
            raster.blank_code = blank_code(header, raster.sample);
        } else if (tiled) {
            tiles = tile_layout(header, raster);
        }
        const auto bytes = header.is_table() ? table_size(header) : raster_size(header, raster.sample);
        const auto fill = std::uint8_t(header.table == TableKind::ascii ? ' ' : 0);
        const auto padding = std::size_t((block_size - bytes % block_size) % block_size);
        const auto data_end = offset + header.bytes + bytes;
        const auto end = data_end + padding;
        valid(end <= file_size, "FITS raster or data padding is truncated");
        if (!selection.hdu && end < file_size) {
            std::array<char, 8> next{};
            if (input.seek(end) && input.read(next.data(), next.size()) &&
                std::memcmp(next.data(), "XTENSION", next.size()) == 0)
                fail(QPX_READER_UNSUPPORTED, "FITS bundles require an explicit image HDU");
            fail(QPX_READER_IO_ERROR, "FITS raster has unexplained trailing bytes");
        }
        FitsChecksum data_sum;
        const bool verify_data = header.datasum.has_value() || header.verify_checksum;
        if (chosen && header.axes) {
            if (selection.window)
                raster.sample.window = *selection.window;
            auto* checksum = verify_data ? &data_sum : nullptr;
            if (tiles)
                read_tiles(input, bytes, *tiles, raster, budget, checksum, cancel);
            else
                read_raster(input, raster, bytes, budget, checksum, cancel);
            selected.emplace(std::move(raster));
            read_padding(input, padding, fill);
        } else if (verify_data) {
            read_checksum_data(input, bytes, padding, fill, data_sum, cancel);
        } else {
            valid(input.seek(data_end),
                  "FITS could not seek to the next HDU boundary");
            read_padding(input, padding, fill);
        }
        if (header.datasum)
            valid(data_sum.value == *header.datasum, "FITS DATASUM does not match the stored data");
        if (header.verify_checksum) {
            data_sum.add_words(header.header_sum.value);
            valid(data_sum.value == UINT32_MAX, "FITS CHECKSUM does not match the stored HDU");
        }
        offset = end;
    }
    valid(input.at_end(), "FITS input changed during reading");
    checkpoint(cancel);
    require(selected.has_value(), "FITS requested HDU is absent or has no image");
    return std::move(*selected);
}

Raster read_gzip(FitsInput& input, Budget budget, const Selection& selection, qpx_reader_cancel_v1 cancel) {
    valid(input.size >= 18, "FITS gzip stream is truncated");
    std::unique_ptr<std::uint8_t[]> decoded;
    std::uint32_t extent = 0;
    {
        auto decode_budget = budget;
        auto encoded = decode_budget.array(input.size);
        for (std::size_t offset = 0; offset < input.size;) {
            checkpoint(cancel);
            const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(65536, input.size - offset));
            valid(input.read(encoded.get() + offset, count), "FITS gzip input changed during reading");
            offset += count;
        }
        valid(input.at_end(), "FITS gzip input changed during reading");
        const auto* trailer = encoded.get() + input.size - 4;
        extent = std::uint32_t(trailer[0]) | (std::uint32_t(trailer[1]) << 8) |
                 (std::uint32_t(trailer[2]) << 16) | (std::uint32_t(trailer[3]) << 24);
        valid(extent >= block_size && extent % block_size == 0, "FITS gzip decoded extent is not block aligned");
        require(decode_budget.remaining >= gzip_workspace_bytes, "FITS gzip workspace exceeds the work budget");
        decode_budget.remaining -= gzip_workspace_bytes;
        const auto before = decode_budget.remaining;
        decoded = decode_budget.array(extent);
        budget.remaining -= before - decode_budget.remaining;
        const auto result = decode_gzip_member(encoded.get(), static_cast<std::size_t>(input.size),
            decoded.get(), extent, [&]{ checkpoint(cancel); });
        if (result == GzipResult::workspace_failure)
            fail(QPX_READER_INTERNAL_ERROR, "FITS gzip workspace could not be initialized");
        valid(result == GzipResult::ok, "FITS requires one complete gzip member with exact extent");
    }
    FitsInput unpacked{nullptr, decoded.get(), extent};
    return read_hdus(unpacked, budget, selection, cancel);
}

Raster read_file(const std::filesystem::path& path, std::uint64_t budget_bytes,
                 const Selection& selection, qpx_reader_cancel_v1 cancel) {
    const auto file_size = std::filesystem::file_size(path);
    require(file_size <= budget_bytes, "FITS file exceeds reader work budget");
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file(_wfopen(path.c_str(), L"rb"), std::fclose);
    valid(bool(file), "Cannot open FITS file");
    valid(!std::setvbuf(file.get(), nullptr, _IONBF, 0), "Cannot configure bounded FITS input");
    FitsInput input{file.get(), nullptr, file_size};
    std::array<std::uint8_t, 2> signature{};
    valid(input.read(signature.data(), signature.size()) && input.seek(0), "FITS primary header is truncated");
    Budget budget{budget_bytes - block_size};
    if (signature[0] == 0x1f && signature[1] == 0x8b)
        return read_gzip(input, budget, selection, cancel);
    return read_hdus(input, budget, selection, cancel);
}

const Raster& image(void* value) {
    require(value != nullptr, "FITS image handle is null");
    return *static_cast<Raster*>(value);
}

qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out,
                                               qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out && out->struct_size >= sizeof(*out), "FITS reader info structure is too small");
        out->abi_version = QPX_READER_ABI_V1;
        out->capabilities = QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW;
        bool complete = text(out->provider_id, "org.qupathxx.fits");
        complete = text(out->display_name, "FITS") && complete;
        complete = text(out->version, QPX_FITS_VERSION) && complete;
        complete = text(out->contract, "fits-image-hdu-scalar-window/2") && complete;
        return complete ? QPX_READER_OK
                        : failure(error, QPX_READER_BUFFER_TOO_SMALL, "FITS descriptor text buffer is too small");
    });
}

qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request, void** out,
                                           qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        require(out != nullptr, "FITS output handle is null");
        *out = nullptr;
        require(request && request->struct_size >= sizeof(*request) && !request->reserved,
                "FITS open structure is incomplete or reserved field is set");
        require(request->path.data && request->path.size && request->path.size <= 32768,
                "FITS path is invalid");
        require(request->work_bytes && request->work_bytes <= work_limit,
                "FITS work budget must be between 1 and 256 MiB");
        const auto overhead = UINT64_C(8192) + 8 * request->path.size + sizeof(Raster);
        require(request->work_bytes > overhead + block_size, "FITS work budget is too small");
        checkpoint(request->cancel);
        const auto selection = parse_options(request->options);
        const std::string spelling(request->path.data, std::size_t(request->path.size));
        require(spelling.find('\0') == std::string::npos, "FITS path contains NUL");
        const auto path = std::filesystem::u8path(spelling);
        require(path.is_absolute(), "FITS path must be absolute");
        auto result = read_file(path, request->work_bytes - overhead, selection, request->cancel);
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
        require(out && out->struct_size >= sizeof(*out), "FITS metadata structure is too small");
        checkpoint(cancel);
        out->level_count = out->has_raw = out->channels = out->timepoints = out->series_count = 1;
        out->width = source.shape[0];
        out->height = source.shape[1];
        out->sample_type = source.sample.type;
        out->z_planes = source.shape[2];
        out->series = 0;
        out->calibration_flags = 0;
        out->pixel_width_um = 0;
        out->pixel_height_um = 0;
        out->z_spacing_um = 0;
        out->time_interval_seconds = 0;
        bool complete = text(out->vendor, "FITS");
        complete = text(out->dimension_order, source.volume ? "XYZ" : "XY") && complete;
        return complete ? QPX_READER_OK
                        : failure(error, QPX_READER_BUFFER_TOO_SMALL, "FITS metadata text buffer is too small");
    });
}

qpx_reader_status_v1 QPX_READER_CALL level(void* value, std::uint32_t index, qpx_reader_level_v1* out,
                                            qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&]() -> qpx_reader_status_v1 {
        const auto& source = image(value);
        require(out && out->struct_size >= sizeof(*out) && index == 0,
                "FITS requires level zero and a complete level descriptor");
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
        require(out && channel == 0, "FITS has one scalar channel");
        return text(*out, "Scalar") ? QPX_READER_OK
                                    : failure(error, QPX_READER_BUFFER_TOO_SMALL,
                                              "FITS channel text buffer is too small");
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

bool is_blank(const Raster& raster, const std::uint8_t* sample) {
    if (!raster.blank_code)
        return false;
    std::uint64_t word = 0;
    std::memcpy(&word, sample, raster.sample.bytes);
    return word == *raster.blank_code;
}

void put_rgba(const Raster& raster, const std::uint8_t* sample, std::uint8_t* out) {
    double value = 0;
    switch (raster.sample.type) {
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
    out[0] = out[1] = out[2] = std::uint8_t(std::floor(normalized * 255 + 0.5));
    out[3] = 255;
}

qpx_reader_status_v1 read_pixels(const Raster& source, const qpx_reader_region_v1* request,
                                qpx_reader_pixels_v1* out, bool raw, qpx_reader_cancel_v1 cancel) {
    require(out && out->struct_size >= sizeof(*out), "FITS pixel structure is too small");
    out->written = out->validity_written = 0;
    require(request && request->struct_size >= sizeof(*request), "FITS region structure is too small");
    require(!request->level && !request->t && !request->first_channel &&
                request->channels == (raw ? 1u : 0u) &&
                request->z < (raw ? source.shape[2] : 1),
            "FITS region selectors are outside the supported channel or plane");
    const auto width = request->width;
    const auto height = request->height;
    const auto channels = raw ? 1u : 4u;
    const auto channel_stride = raw ? source.sample.bytes : 1u;
    const auto stride = channels * channel_stride;
    const auto per_pixel = stride + (raw ? 1u : 0u);
    require(width > 0 && height > 0 && std::uint64_t(width) <= work_limit / per_pixel &&
                std::uint64_t(height) <= work_limit / per_pixel / std::uint64_t(width),
            "FITS pixel output exceeds the 256 MiB work limit");
    const auto limit = std::numeric_limits<std::int64_t>::max();
    require(request->x <= limit - width && request->y <= limit - height,
            "FITS region coordinate overflow");
    const auto pixels = std::uint64_t(width) * std::uint64_t(height);
    const auto bytes = pixels * stride;
    require(out->data && out->capacity >= bytes && !out->reserved &&
                out->sample_type == (raw ? source.sample.type : QPX_READER_UINT8) &&
                out->width == width && out->height == height && out->channels == channels &&
                out->row_stride == std::uint64_t(width) * stride && out->pixel_stride == stride &&
                out->channel_stride == channel_stride &&
                (raw ? out->validity && out->validity_capacity >= pixels
                     : !out->validity && !out->validity_capacity),
            "FITS output must have valid buffers and packed pixel strides");
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
            const auto* sample = source.at(x, y, request->z);
            auto* target = out->data + index * stride;
            // The host requires zero data bytes for every invalid raw pixel.
            // Classify the stored bits before any floating-point operation.
            const auto classification = float_class(source, sample);
            if (classification == FloatClass::nan || is_blank(source, sample))
                continue;
            if (raw) {
                std::memcpy(target, sample, stride);
                out->validity[index] = 1;
            } else if (classification == FloatClass::finite) {
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
                "Unsupported FITS ABI or incomplete API structure");
        *out = {sizeof(*out), QPX_READER_ABI_V1, describe, open, destroy, metadata, level,
                channel_name, read_rgba, read_raw};
        return QPX_READER_OK;
    });
}
