// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "../format-common/binary_input.hpp"
#include <array>
#include <charconv>

namespace qpx_npz {
using namespace qpx_binary;

struct HeaderCursor {
    std::string_view left;

    bool take(std::string_view token) {
        while (!left.empty() && left.front() == ' ') left.remove_prefix(1);
        if (left.substr(0, token.size()) != token) return false;
        left.remove_prefix(token.size());
        return true;
    }
    void expect(std::string_view token) { valid(take(token), "Malformed NPY header literal"); }
    std::string_view quoted() {
        const auto single = take("'");
        if (!single) expect("\"");
        const auto end = left.find(single ? '\'' : '"');
        valid(end != left.npos, "Unclosed NPY string literal");
        const auto result = left.substr(0, end);
        left.remove_prefix(end + 1);
        return result;
    }
    std::uint64_t number() {
        while (!left.empty() && left.front() == ' ') left.remove_prefix(1);
        valid(!left.empty(), "Missing NPY shape dimension");
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(left.data(), left.data() + left.size(), value);
        valid(parsed.ec == std::errc{} && value > 0, "Invalid NPY shape dimension");
        left.remove_prefix(std::size_t(parsed.ptr - left.data()));
        return value;
    }
};

struct Array {
    Bytes payload;
    std::string_view descriptor;
    std::array<std::uint64_t, 2> shape{1, 1};
    std::uint64_t elements = 1;
    unsigned rank = 0, item = 0;
    char kind = 0;
    bool fortran = false, big_endian = false;
};

inline Array array(Bytes input, unsigned rank, bool continuous = false) {
    valid(input.size >= 10 && input.sub(0, 6).view() == std::string_view("\x93NUMPY", 6), "Invalid NPY signature");
    const auto version = input.data[6];
    valid(version >= 1 && version <= 3 && input.data[7] == 0, "Unsupported NPY version");
    const auto prefix = version == 1 ? 10u : 12u;
    const auto length = version == 1 ? input.le16(8) : input.le32(8);
    valid(length && length <= 65536 && (prefix + length) % 16 == 0, "NPY header size or alignment is invalid");
    const auto header = input.sub(prefix, length).view();
    valid(header.back() == '\n' && std::all_of(header.begin(), header.end() - 1, [](unsigned char c) {
        return c >= 32 && c <= 126;
    }), "NPY header is not a bounded ASCII dictionary");
    HeaderCursor cursor{header.substr(0, header.size() - 1)};
    Array result;
    cursor.expect("{");
    unsigned seen = 0;
    for (;;) {
        const auto key = cursor.quoted();
        cursor.expect(":");
        if (key == "descr") {
            valid(!(seen & 1), "Duplicate NPY descriptor");
            result.descriptor = cursor.quoted();
            seen |= 1;
        } else if (key == "fortran_order") {
            valid(!(seen & 2), "Duplicate NPY storage order");
            result.fortran = cursor.take("True");
            if (!result.fortran) cursor.expect("False");
            seen |= 2;
        } else if (key == "shape") {
            valid(!(seen & 4), "Duplicate NPY shape");
            cursor.expect("(");
            result.shape[0] = cursor.number();
            cursor.expect(",");
            if (!cursor.take(")")) {
                result.shape[1] = cursor.number();
                (void)cursor.take(",");
                cursor.expect(")");
                result.rank = 2;
            } else {
                result.rank = 1;
            }
            seen |= 4;
        } else {
            fail("Unsupported NPY header key");
        }
        if (cursor.take("}")) break;
        cursor.expect(",");
        if (cursor.take("}")) break;
    }
    while (!cursor.left.empty() && cursor.left.front() == ' ') cursor.left.remove_prefix(1);
    valid(cursor.left.empty() && seen == 7 && result.rank == rank, "NPY array rank or header fields differ");
    const auto descriptor = result.descriptor;
    valid(descriptor.size() >= 3, "Unsupported NPY scalar type");
    const auto parsed = std::from_chars(descriptor.data() + 2, descriptor.data() + descriptor.size(), result.item);
    valid(parsed.ec == std::errc{} && parsed.ptr == descriptor.data() + descriptor.size() && result.item > 0,
          "Unsupported NPY scalar width");
    const char order = descriptor.front();
    result.kind = descriptor[1];
    result.big_endian = order == '>';
    if (rank == 1) {
        valid(result.item <= 256 && ((result.kind == 'S' && order == '|') ||
              (result.kind == 'U' && (order == '<' || order == '>'))), "NPY IDs require fixed byte or Unicode strings");
        if (result.kind == 'U') result.item *= 4;
    } else if (continuous) {
        valid(result.kind == 'f' && (result.item == 4 || result.item == 8) && (order == '<' || order == '>'),
              "NPY continuous values require a Float32 or Float64 dtype");
    } else {
        valid((result.kind == 'u' || result.kind == 'i') &&
              ((result.item == 1 && order == '|') ||
               ((result.item == 2 || result.item == 4 || result.item == 8) && (order == '<' || order == '>'))),
              "NPY counts require a fixed-width integer dtype");
    }
    result.payload = input.sub(prefix + length, input.size - (prefix + length));
    valid(result.shape[0] <= result.payload.size / result.item / result.shape[1], "NPY array shape exceeds the payload");
    result.elements = result.shape[0] * result.shape[1];
    valid(result.elements * result.item == result.payload.size, "NPY array payload has the wrong extent");
    return result;
}

inline std::pmr::vector<std::pmr::string> labels(const Array& source, std::pmr::memory_resource* memory,
                                               qpx_reader_cancel_v1 cancel) {
    std::pmr::vector<std::pmr::string> out(memory);
    out.reserve(static_cast<std::size_t>(source.elements));
    for (std::size_t index = 0; index < source.elements; ++index) {
        poll(cancel);
        const auto field = source.payload.sub(index * source.item, source.item);
        auto& value = out.emplace_back();
        if (source.kind == 'S') {
            auto data = field.view();
            while (!data.empty() && !data.back()) data.remove_suffix(1);
            value = data;
        } else {
            auto length = field.size;
            while (length && !field.number(length - 4, 4, source.big_endian)) length -= 4;
            for (std::size_t at = 0; at < length; at += 4)
                append_utf8(value, static_cast<std::uint32_t>(field.number(at, 4, source.big_endian)));
        }
        valid(!value.empty() && value.size() <= 256, "NPY axis ID is empty or exceeds 256 UTF-8 bytes");
        utf8(value, cancel);
    }
    std::pmr::vector<std::string_view> ordered(memory);
    ordered.reserve(out.size());
    for (const auto& value : out) ordered.emplace_back(value);
    std::sort(ordered.begin(), ordered.end(), [&](auto a, auto b) { poll(cancel); return a < b; });
    valid(std::adjacent_find(ordered.begin(), ordered.end()) == ordered.end(), "NPY axis IDs are duplicated");
    return out;
}
} // namespace qpx_npz
