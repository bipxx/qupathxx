// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/ome_zarr.hpp"
#include "../reader_file_guard.hpp"
#include "../sha256.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {
namespace fs = std::filesystem;
using J = nlohmann::json;
using namespace qpx;

void check(bool okay, const std::string& message) {
    if (!okay) throw std::runtime_error(message);
}

template<class T> std::string bits(T value) {
    using U = std::conditional_t<sizeof(T) == 1, std::uint8_t,
              std::conditional_t<sizeof(T) == 2, std::uint16_t,
              std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>>;
    U word = 0;
    std::memcpy(&word, &value, sizeof(T));
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(int(sizeof(T) * 2))
        << static_cast<std::uint64_t>(word);
    return out.str();
}

template<class T> std::vector<std::uint8_t> little_bytes(T value) {
    std::array<std::uint8_t, sizeof(T)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(T));
    const std::uint16_t marker = 1;
    if (*reinterpret_cast<const std::uint8_t*>(&marker) != 1)
        std::reverse(bytes.begin(), bytes.end());
    return {bytes.begin(), bytes.end()};
}

void write(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    check(bool(out), "Cannot create authored fixture: " + path.u8string());
    out.write(text.data(), std::streamsize(text.size()));
    check(bool(out), "Cannot write authored fixture: " + path.u8string());
}

void write(const fs::path& path, const J& value) {
    write(path, value.dump(2) + "\n");
}

J load(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    check(bool(in), "Cannot read report: " + path.u8string());
    return J::parse(in);
}

enum class Expect { Value, OpenRefusal, ReadRefusal };
struct Case {
    std::string name, dtype, token, expected_bits;
    SampleType type = SampleType::UInt8;
    Expect expected = Expect::Value;
    bool populated = false, escaped_key = false, nonbase = false, nested_key = false;
    std::vector<std::uint8_t> chunk;
};

template<class T> void value(std::vector<Case>& cases, const std::string& name,
        const std::string& dtype, SampleType type, const std::string& token, T expected) {
    Case c;
    c.name = name;
    c.dtype = dtype;
    c.type = type;
    c.token = token;
    c.expected_bits = bits(expected);
    cases.push_back(std::move(c));
}

void refusal(std::vector<Case>& cases, const std::string& name,
        const std::string& dtype, SampleType type, const std::string& token,
        bool populated = false, bool nonbase = false) {
    Case c;
    c.name = name;
    c.dtype = dtype;
    c.type = type;
    c.token = token;
    c.expected = Expect::OpenRefusal;
    c.populated = populated;
    c.nonbase = nonbase;
    c.chunk.assign(sample_bytes(type), 0);
    cases.push_back(std::move(c));
}

template<class T> std::string decimal(T value) {
    if constexpr (std::is_signed_v<T>) return std::to_string(std::int64_t(value));
    else return std::to_string(std::uint64_t(value));
}

template<class T> void integral(std::vector<Case>& cases, const std::string& code,
        SampleType type, const char* above, const char* below) {
    const auto prefix = code + "-";
    value(cases, prefix + "zero", "<" + code, type, "0", T(0));
    value(cases, prefix + "minimum", "<" + code, type,
          decimal(std::numeric_limits<T>::min()), std::numeric_limits<T>::min());
    value(cases, prefix + "maximum", "<" + code, type,
          decimal(std::numeric_limits<T>::max()), std::numeric_limits<T>::max());
    value(cases, prefix + "floating-one", "<" + code, type, "1.0", T(1));
    value(cases, prefix + "floating-minus-zero", "<" + code, type, "-0.0", T(0));
    value(cases, prefix + "integer-minus-zero", "<" + code, type, "-0", T(0));
    value(cases, prefix + "exponent", "<" + code, type, "1e1", T(10));
    value(cases, prefix + "fraction-and-exponent", "<" + code, type, "1.000e+1", T(10));
    value(cases, prefix + "negative-exponent-exact", "<" + code, type, "1000e-3", T(1));
    value(cases, prefix + "positive-exponent-exact", "<" + code, type, "0.001e3", T(1));
    value(cases, prefix + "zero-huge-exponent", "<" + code, type, "0e999999", T(0));
    value(cases, prefix + "negative-zero-huge-negative-exponent", "<" + code,
          type, "-0e-999999", T(0));
    if constexpr (std::is_signed_v<T>)
        value(cases, prefix + "negative-floating-one", "<" + code, type, "-1.0", T(-1));
    Case present;
    present.name = prefix + "null-present";
    present.dtype = "<" + code;
    present.type = type;
    present.token = "null";
    present.populated = true;
    present.chunk = little_bytes(T(7));
    present.expected_bits = bits(T(7));
    cases.push_back(present);
    present.name = prefix + "null-missing";
    present.populated = false;
    present.expected = Expect::ReadRefusal;
    cases.push_back(std::move(present));
    refusal(cases, prefix + "above-maximum", "<" + code, type, above);
    refusal(cases, prefix + "below-minimum", "<" + code, type, below);
    refusal(cases, prefix + "rounded-fraction", "<" + code, type,
            "1.00000000000000000001");
    refusal(cases, prefix + "rounded-fraction-present", "<" + code, type,
            "1.00000000000000000001", true);
    refusal(cases, prefix + "underflow", "<" + code, type, "1e-999");
    refusal(cases, prefix + "negative-underflow", "<" + code, type, "-1e-999");
    refusal(cases, prefix + "nonintegral-exponent", "<" + code, type, "10001e-4");
}

template<class T> void floating(std::vector<Case>& cases, const std::string& code,
        SampleType type) {
    value(cases, code + "-numeric", "<" + code, type, "1.25", T(1.25));
    value(cases, code + "-negative-zero", "<" + code, type, "-0.0", T(-0.0));
    value(cases, code + "-positive-zero", "<" + code, type, "0.0", T(0.0));
    value(cases, code + "-nan", "<" + code, type, "\"NaN\"", std::numeric_limits<T>::quiet_NaN());
    value(cases, code + "-positive-infinity", "<" + code, type,
          "\"Infinity\"", std::numeric_limits<T>::infinity());
    value(cases, code + "-negative-infinity", "<" + code, type,
          "\"-Infinity\"", -std::numeric_limits<T>::infinity());
    Case present;
    present.name = code + "-null-present-negative-zero";
    present.dtype = "<" + code;
    present.type = type;
    present.token = "null";
    present.populated = true;
    present.chunk = little_bytes(T(-0.0));
    present.expected_bits = bits(T(-0.0));
    cases.push_back(present);
    present.name = code + "-null-missing";
    present.populated = false;
    present.expected = Expect::ReadRefusal;
    cases.push_back(std::move(present));
}

std::vector<Case> cases() {
    std::vector<Case> result;
    integral<std::uint8_t>(result, "u1", SampleType::UInt8, "256", "-1");
    integral<std::uint16_t>(result, "u2", SampleType::UInt16, "65536", "-1");
    integral<std::int8_t>(result, "i1", SampleType::Int8, "128", "-129");
    integral<std::int16_t>(result, "i2", SampleType::Int16, "32768", "-32769");
    integral<std::uint32_t>(result, "u4", SampleType::UInt32, "4294967296", "-1");
    integral<std::int32_t>(result, "i4", SampleType::Int32, "2147483648", "-2147483649");
    integral<std::uint64_t>(result, "u8", SampleType::UInt64, "18446744073709551616", "-1");
    integral<std::int64_t>(result, "i8", SampleType::Int64, "9223372036854775808", "-9223372036854775809");
    value(result, "u8-exact-above-binary64", "<u8", SampleType::UInt64,
          "9007199254740993", std::uint64_t(9007199254740993ULL));
    value(result, "i8-exact-negative-above-binary64", "<i8", SampleType::Int64,
          "-9007199254740993", std::int64_t(-9007199254740993LL));
    for (const auto& typed : std::vector<std::pair<std::string, SampleType>>{
             {"u8", SampleType::UInt64}, {"i8", SampleType::Int64}}) {
        const auto& code = typed.first;
        if (typed.second == SampleType::UInt64) {
            value(result, code + "-exact-floating-binary64-boundary", "<" + code,
                  typed.second, "9007199254740992.0", std::uint64_t(9007199254740992ULL));
            value(result, code + "-exact-floating-even", "<" + code,
                  typed.second, "9007199254740994.0", std::uint64_t(9007199254740994ULL));
        } else {
            value(result, code + "-exact-floating-binary64-boundary", "<" + code,
                  typed.second, "9007199254740992.0", std::int64_t(9007199254740992LL));
            value(result, code + "-exact-floating-even", "<" + code,
                  typed.second, "9007199254740994.0", std::int64_t(9007199254740994LL));
        }
        refusal(result, code + "-rounded-floating-integer", "<" + code,
                typed.second, "9007199254740993.0");
        refusal(result, code + "-rounded-floating-exponent", "<" + code,
                typed.second, "9.007199254740993e15");
    }
    value(result, "i8-exact-floating-minimum", "<i8", SampleType::Int64,
          "-9223372036854775808.0", std::numeric_limits<std::int64_t>::min());
    value(result, "i8-exact-floating-near-maximum", "<i8", SampleType::Int64,
          "9223372036854774784.0", std::int64_t(9223372036854774784LL));
    value(result, "u8-exact-floating-near-maximum", "<u8", SampleType::UInt64,
          "18446744073709549568.0", std::uint64_t(18446744073709549568ULL));
    refusal(result, "i8-rounded-below-minimum", "<i8", SampleType::Int64,
            "-9223372036854775808.00000000000000000001");
    refusal(result, "i8-floating-maximum-rounds-up", "<i8", SampleType::Int64,
            "9223372036854775807.0");
    refusal(result, "u8-floating-maximum-rounds-up", "<u8", SampleType::UInt64,
            "18446744073709551615.0");
    refusal(result, "u1-rounded-at-maximum", "<u1", SampleType::UInt8,
            "255.00000000000000000001");
    refusal(result, "i1-rounded-at-minimum", "<i1", SampleType::Int8,
            "-128.00000000000000000001");
    for (const auto& token : std::vector<std::pair<std::string, std::string>>{
             {"boolean", "true"}, {"string", "\"1\""}, {"array", "[]"}, {"object", "{}"}})
        refusal(result, "u1-nonnumeric-" + token.first, "<u1", SampleType::UInt8, token.second);
    value(result, "u1-escaped-key-exact", "<u1", SampleType::UInt8, "1.0", std::uint8_t(1));
    result.back().escaped_key = true;
    refusal(result, "u1-escaped-key-rounded", "<u1", SampleType::UInt8,
            "1.00000000000000000001");
    result.back().escaped_key = true;
    value(result, "u1-nested-fill-key-is-not-root", "<u1", SampleType::UInt8,
          "1.0", std::uint8_t(1));
    result.back().nested_key = true;
    refusal(result, "u1-nonbase-invalid-fill-populated", "<u1", SampleType::UInt8,
            "1.00000000000000000001", true, true);
    floating<float>(result, "f4", SampleType::Float32);
    floating<double>(result, "f8", SampleType::Float64);
    return result;
}

J description(const Case& c) {
    return {{"name", c.name}, {"dtype", c.dtype}, {"fill_token", c.token},
            {"expected", c.expected == Expect::Value ? "value" :
                         c.expected == Expect::OpenRefusal ? "open-refusal" : "read-refusal"},
            {"expected_bits", c.expected_bits}, {"populated", c.populated},
            {"escaped_key", c.escaped_key}, {"nonbase", c.nonbase}, {"nested_key", c.nested_key}};
}

void author(const fs::path& path, const Case& c) {
    check(fs::create_directory(path), "Authored store already exists");
    write(path / ".zgroup", J{{"zarr_format", 2}});
    auto datasets = J::array();
    for (int level = 0; level < (c.nonbase ? 2 : 1); ++level) {
        const auto key = std::to_string(level);
        check(fs::create_directory(path / key), "Cannot create authored array directory");
        const J transform{{"type", "scale"}, {"scale", {2 * (level + 1), 3 * (level + 1)}}};
        datasets.push_back({{"path", key}, {"coordinateTransformations", J::array({transform})}});
        J spec{{"zarr_format", 2}, {"shape", {1, 1}}, {"chunks", {1, 1}},
               {"dtype", c.dtype}, {"order", "C"}, {"compressor", nullptr},
               {"filters", nullptr}, {"fill_value", "__literal_fill__"}, {"dimension_separator", "."}};
        if (c.nested_key) spec["unused"] = {{"fill_value", 1.5}};
        auto text = spec.dump(2);
        const std::string marker = "\"__literal_fill__\"";
        const auto at = text.find(marker);
        check(at != std::string::npos && text.find(marker, at + marker.size()) == std::string::npos,
              "Literal fill marker is not unique");
        text.replace(at, marker.size(), c.nonbase && level == 0 ? "0" : c.token);
        if (c.escaped_key) {
            const auto field = text.find("\"fill_value\"");
            check(field != std::string::npos, "Cannot find authored fill key");
            text.replace(field, std::string("\"fill_value\"").size(), "\"fill_\\u0076alue\"");
        }
        write(path / key / ".zarray", text + "\n");
        if (c.populated) {
            const auto& bytes = c.chunk;
            write(path / key / "0.0", std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        }
    }
    const J axes = J::array({
        {{"name", "y"}, {"type", "space"}, {"unit", "micrometer"}},
        {{"name", "x"}, {"type", "space"}, {"unit", "micrometer"}}});
    const J multiscale{{"version", "0.4"}, {"axes", axes}, {"datasets", datasets}};
    write(path / ".zattrs", J{{"multiscales", J::array({multiscale})}});
}

J version(const FileVersion& v) {
    return {{"path", v.path}, {"identity_scheme", v.identity_scheme}, {"identity", v.identity},
            {"stamp_scheme", v.stamp_scheme}, {"modified_token", v.modified_token},
            {"change_token", v.change_token}, {"bytes", v.bytes}};
}

J inventory(const fs::path& corpus) {
    J result = J::object();
    for (const auto& entry : fs::recursive_directory_iterator(corpus)) {
        check(!fs::is_symlink(entry.symlink_status()), "Authored corpus contains a link");
        if (entry.is_directory()) continue;
        check(entry.is_regular_file(), "Authored corpus contains a nonregular file");
        detail::ReaderFileGuard guard(entry.path().u8string());
        std::ifstream in(entry.path(), std::ios::binary);
        check(bool(in), "Cannot hash authored corpus file");
        Sha256 hash;
        std::array<char, 4096> buffer{};
        while (in) {
            in.read(buffer.data(), std::streamsize(buffer.size()));
            if (const auto count = in.gcount()) hash.update(buffer.data(), std::size_t(count));
        }
        check(in.eof() && !in.bad(), "Cannot finish authored corpus hash");
        guard.verify();
        result[entry.path().lexically_relative(corpus).generic_u8string()] =
            {{"sha256", hash.finish()}, {"version", version(*guard.version())}};
    }
    return result;
}

J metadata(const OmeZarrSource& source) {
    const auto& m = source.metadata();
    J result{{"width", m.width}, {"height", m.height}, {"channels", m.channels},
             {"z_planes", m.z_planes}, {"timepoints", m.timepoints},
             {"sample_type", sample_name(m.sample_type)}, {"dimension_order", m.dimension_order},
             {"axes", source.axes()}, {"units", source.units()}, {"levels", J::array()}};
    result["pixel_width_um"] = m.pixel_width_um ? J(*m.pixel_width_um) : J(nullptr);
    result["pixel_height_um"] = m.pixel_height_um ? J(*m.pixel_height_um) : J(nullptr);
    for (const auto& level : source.levels())
        result["levels"].push_back({{"path", level.path}, {"shape", level.shape},
            {"chunks", level.chunks}, {"scale", level.scale}, {"translation", level.translation}});
    return result;
}

J run(const fs::path& path, const Case& c, bool enforce) {
    J row = description(c);
    row["fixture"] = path.u8string();
    row["opened"] = false;
    row["read"] = false;
    row["passed"] = true;
    std::unique_ptr<OmeZarrSource> source;
    try {
        source = std::make_unique<OmeZarrSource>(path.u8string());
        row["opened"] = true;
        row["source_sha256"] = source->sha256();
        row["metadata"] = metadata(*source);
        check(source->metadata().sample_type == c.type, "Authored dtype changed");
        check(source->metadata().width == 1 && source->metadata().height == 1 &&
              source->metadata().channels == 1 && source->metadata().z_planes == 1 &&
              source->metadata().timepoints == 1, "Authored dimensions changed");
        const auto observed = source->reader_source();
        check(observed && observed->scope == "directory_store", "Native store identity absent");
        row["reader_source"] = {{"reader", observed->reader}, {"locator", observed->locator},
            {"scope", observed->scope}, {"directory_sha256", observed->directory_sha256}};
    } catch (const std::exception& e) {
        row["open_error"] = e.what();
        if (row["opened"].get<bool>() || c.expected != Expect::OpenRefusal) {
            row["passed"] = false;
            row["failure"] = "Unexpected open/metadata failure";
        }
        return row;
    }
    if (enforce && c.expected == Expect::OpenRefusal) {
        row["passed"] = false;
        row["failure"] = "Malformed fill was accepted at open";
    }
    try {
        RawRegionRequest request;
        request.region = {0, 0, 0, 1, 1};
        const auto data = source->read(request);
        row["read"] = true;
        check(data.valid() == std::vector<std::uint8_t>{1}, "Valid fill pixel became padding");
        check(data.sample_type() == c.type && data.width() == 1 && data.height() == 1 &&
              data.channels() == 1, "Native fill buffer shape/type changed");
        std::visit([&](const auto& values) {
            check(values.size() == 1, "Native scalar output length differs");
            row["value_bits"] = bits(values.front());
        }, data.storage());
        row["valid"] = data.valid();
        if (c.expected == Expect::ReadRefusal) {
            row["passed"] = false;
            row["failure"] = "Undefined null fill supplied a missing chunk";
        } else if (c.expected == Expect::Value && row["value_bits"] != c.expected_bits) {
            row["passed"] = false;
            row["failure"] = "Native fill sample differs from literal expected bits";
        }
    } catch (const std::exception& e) {
        row["read_error"] = e.what();
        const bool null_refusal = c.expected == Expect::ReadRefusal &&
            std::string(e.what()).find("missing chunk has undefined null fill") != std::string::npos;
        if (!null_refusal && !(c.expected == Expect::OpenRefusal && !enforce)) {
            row["passed"] = false;
            row["failure"] = "Unexpected read/shape failure";
        }
    }
    return row;
}

void compare_valid(const J& baseline, const J& row) {
    const auto& rows = baseline.at("cases");
    const auto it = std::find_if(rows.begin(), rows.end(), [&](const J& old) {
        return old.at("name") == row.at("name");
    });
    check(it != rows.end(), "Baseline lacks named valid case");
    for (const auto* field : {"fixture", "source_sha256", "metadata", "reader_source", "value_bits", "valid"})
        check(it->at(field) == row.at(field), std::string("Valid old/new identity or pixels differ: ") + field);
}
}

int main(int argc, char** argv) {
    J report{{"schema", "qpx.ome-zarr-exact-fill-tests/1"}, {"passed", false},
             {"cases", J::array()}, {"scope", "Authored native NGFF0.4/Zarr2 exact integral fill and unchanged floating controls"}};
    fs::path output;
    bool may_write = false;
    try {
        check(argc == 4 || argc == 5, "Usage: observe|verify WORK_DIRECTORY OUTPUT_JSON [BASELINE_JSON]");
        const std::string mode = argv[1];
        check(mode == "observe" || mode == "verify", "Unknown fill test mode");
        check(argc != 5 || mode == "verify", "Only verify accepts a baseline report");
        const auto root = fs::absolute(fs::u8path(argv[2])).lexically_normal();
        output = fs::absolute(fs::u8path(argv[3])).lexically_normal();
        check(!fs::exists(output), "Output report already exists");
        may_write = true;
        check(has_ome_zarr(), "Native OME-Zarr support is disabled");
        const auto corpus = root / fs::u8path("fixtures-\xCE\xBC-\xE5\x9B\xBE");
        for (auto parent = output.parent_path(); !parent.empty(); parent = parent.parent_path()) {
            check(parent != corpus, "Output report cannot enter the read-only fixture corpus");
            if (parent == parent.parent_path()) break;
        }
        const auto selected = cases();
        J definitions = J::array();
        for (const auto& c : selected) definitions.push_back(description(c));
        report["mode"] = mode;
        report["work_directory"] = root.u8string();
        report["fixture_directory"] = corpus.u8string();
        report["expected_refusals_enforced"] = mode == "verify";
        report["definitions"] = definitions;
        J baseline;
        if (argc == 5) {
            baseline = load(fs::u8path(argv[4]));
            check(baseline.at("schema") == report.at("schema") && baseline.at("mode") == "observe" &&
                  baseline.at("passed") == true, "Baseline is not a completed native observation");
            check(baseline.at("work_directory") == root.u8string() &&
                  baseline.at("fixture_directory") == corpus.u8string(), "Baseline fixture paths differ");
            check(baseline.at("definitions") == definitions, "Baseline case definitions differ");
            check(fs::is_directory(root) && fs::is_directory(corpus), "Baseline fixture directory is missing");
            check(inventory(corpus) == baseline.at("fixture_inventory"),
                  "Baseline fixture membership/bytes/native file versions changed");
            report["reused_exact_baseline_paths_and_versions"] = true;
        } else {
            check(!fs::exists(root), "Fresh fixture work directory already exists");
            check(fs::create_directories(corpus), "Cannot create authored fixture corpus");
            for (const auto& c : selected) author(corpus / (c.name + ".zarr"), c);
        }
        const auto before = inventory(corpus);
        std::size_t valid_count = 0, open_refusals = 0, read_refusals = 0, failures = 0;
        std::size_t malformed_accepted = 0;
        for (const auto& c : selected) {
            auto row = run(corpus / (c.name + ".zarr"), c, mode == "verify");
            if (c.expected == Expect::Value) {
                ++valid_count;
                if (argc == 5 && row.at("passed") == true) {
                    try { compare_valid(baseline, row); }
                    catch (const std::exception& e) { row["passed"] = false; row["failure"] = e.what(); }
                }
            } else if (c.expected == Expect::OpenRefusal) {
                ++open_refusals;
                if (row.at("opened") == true) ++malformed_accepted;
            } else ++read_refusals;
            if (row.at("passed") != true) ++failures;
            report["cases"].push_back(std::move(row));
        }
        const auto after = inventory(corpus);
        check(after == before, "Read-only native checks changed fixture files or versions");
        report["fixture_inventory"] = after;
        report["fixture_files"] = after.size();
        report["case_count"] = selected.size();
        report["valid_cases"] = valid_count;
        report["expected_open_refusals"] = open_refusals;
        report["expected_read_refusals"] = read_refusals;
        report["malformed_cases_opened"] = malformed_accepted;
        report["failures"] = failures;
        report["fixture_files_unchanged"] = true;
        report["passed"] = failures == 0;
    } catch (const std::exception& e) {
        report["error"] = e.what();
    }
    if (may_write) {
        try { write(output, report); }
        catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    }
    const J summary{{"passed", report.at("passed")}, {"report", output.u8string()},
        {"case_count", report.value("case_count", std::size_t(0))},
        {"failures", report.value("failures", std::size_t(0))},
        {"malformed_cases_opened", report.value("malformed_cases_opened", std::size_t(0))},
        {"error", report.value("error", std::string{})}};
    std::cout << summary.dump() << '\n';
    return report.at("passed") == true ? 0 : 1;
}
