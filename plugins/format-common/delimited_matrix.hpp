// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "matrix_table.hpp"
#include <array>
#include <charconv>

namespace qpx_delimited_matrix {
using namespace qpx_matrix_table;
constexpr std::size_t column_limit = 64;
constexpr std::size_t row_limit = 100'000;
constexpr std::size_t field_limit = 4096;
constexpr std::size_t id_limit = 256;

template<char Separator>
struct Fields {
    std::string_view input;
    qpx_reader_cancel_v1 cancel;
    std::string field;

    // A separator guarantees another field in this row.
    bool next() {
        poll(cancel);
        field.clear();
        if constexpr (Separator == ',') {
            if (!input.empty() && input.front() == '"') {
                read_quoted();
                return finish();
            }
        }
        while (!input.empty() && input.front() != Separator && input.front() != '\n' && input.front() != '\r') {
            if constexpr (Separator == ',')
                valid(input.front() != '"', "CSV quote must start a field");
            append(input.front());
            input.remove_prefix(1);
        }
        return finish();
    }

    void read_quoted() {
        input.remove_prefix(1);
        for (;;) {
            valid(!input.empty(), "Unclosed CSV quoted field");
            const char ch = input.front();
            input.remove_prefix(1);
            if (ch == '"') {
                if (input.empty() || input.front() != '"')
                    return;
                input.remove_prefix(1);
            }
            append(ch);
        }
    }

    bool finish() {
        utf8(field, cancel);
        if (input.empty())
            return false;
        const char separator = input.front();
        input.remove_prefix(1);
        if (separator == Separator)
            return true;
        if (separator == '\r') {
            valid(!input.empty() && input.front() == '\n', "Matrix table requires LF or CRLF line endings");
            input.remove_prefix(1);
        } else {
            valid(separator == '\n', "Unexpected text after a quoted CSV field");
        }
        return false;
    }

    void append(char ch) {
        valid(ch != '\n' && ch != '\r', "CSV fields cannot contain line breaks");
        valid(field.size() < field_limit, "Matrix table field exceeds 4096 bytes");
        field.push_back(ch);
    }
};

inline std::uint64_t value_bits(std::string_view field, bool counts) {
    valid(!field.empty(), "Matrix table selected feature is empty");
    const auto* end = field.data() + field.size();
    if (counts) {
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(field.data(), end, value, 10);
        valid(parsed.ec == std::errc{} && parsed.ptr == end, "Matrix table raw counts require a UInt64 decimal integer");
        return value;
    }
    double value = 0;
    const auto parsed = std::from_chars(field.data(), end, value, std::chars_format::general);
    valid(parsed.ec == std::errc{} && parsed.ptr == end, "Matrix table continuous values require a Float64 decimal number");
    return continuous_bits(value);
}

template<char Separator>
struct Table {
    static constexpr std::size_t private_work = 0;

    static void decode(Bytes file, Source& data) {
        auto input = file.view();
        if (input.substr(0, 3) == "\xef\xbb\xbf")
            input.remove_prefix(3);
        Fields<Separator> fields{input, data.cancel, {}};
        std::pmr::vector<std::pmr::string> header(&data.arena.memory);
        bool more;
        do {
            valid(header.size() < column_limit, "Matrix table supports at most 64 columns");
            more = fields.next();
            valid(!fields.field.empty() && fields.field.size() <= id_limit, "Matrix header must contain 1 to 256 UTF-8 bytes");
            valid(std::find(header.begin(), header.end(), std::string_view(fields.field)) == header.end(),
                  "Matrix header contains a duplicate column name");
            header.emplace_back(fields.field.data(), fields.field.size());
        } while (more);

        const auto find = [&](std::string_view label) {
            const auto column = std::find(header.begin(), header.end(), label);
            valid(column != header.end(), "Selected matrix column is missing");
            return std::size_t(column - header.begin());
        };
        const auto observation = find(data.observation_column);
        std::array<int, column_limit> features;
        features.fill(-1);
        for (std::size_t i = 0; i < data.features.size(); ++i)
            features[find(data.features[i])] = static_cast<int>(i);

        while (!fields.input.empty()) {
            valid(data.observations.size() < row_limit, "Matrix table exceeds 100000 observations");
            std::array<std::uint64_t, column_limit> row;
            for (std::size_t i = 0; i < header.size(); ++i) {
                more = fields.next();
                valid(more == (i + 1 < header.size()), "Matrix row width differs from its header");
                if (i == observation) {
                    valid(!fields.field.empty() && fields.field.size() <= id_limit, "Matrix observation ID must contain 1 to 256 UTF-8 bytes");
                    data.observations.emplace_back(fields.field.data(), fields.field.size());
                } else if (features[i] >= 0) {
                    row[features[i]] = value_bits(fields.field, data.counts());
                }
            }
            data.values.insert(data.values.end(), row.begin(), row.begin() + data.features.size());
        }
        valid(!data.observations.empty(), "Matrix table requires at least one observation");
    }
};
} // namespace qpx_delimited_matrix
