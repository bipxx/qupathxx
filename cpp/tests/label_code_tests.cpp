#include "qupathxx/label_codes.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace qpx;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class E = std::exception, class F> void refuses(F f) {
    try {
        f();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("Invalid label-code input was accepted");
}
template <class T>
RawPixelBuffer raw_values(SampleType type, std::int64_t width, std::int64_t height,
                          std::vector<T> values) {
    RawPixelBuffer raw(width, height, 1, type);
    raw.writable_storage() = std::move(values);
    std::fill(raw.writable_valid().begin(), raw.writable_valid().end(), 1);
    return raw;
}
template <class T>
void exact_case(SampleType type, std::vector<T> values,
                const std::vector<std::uint32_t>& expected_labels,
                const std::vector<std::string>& expected_codes) {
    auto raw = raw_values(type, 4, 2, std::move(values));
    const auto original = raw.storage();
    auto normalized = normalize_label_codes(raw);
    check(raw.storage() == original, "Normalization changed raw scientific values");
    check(normalized.raster.width() == 4 && normalized.raster.height() == 2 &&
              normalized.raster.labels() == expected_labels,
          "Dense labels differ from independent original-code oracle");
    check(normalized.source_codes.size() == expected_codes.size() &&
              normalized.raster.regions().size() == expected_codes.size(),
          "Distinct source code was dropped or split by connectivity");
    for (std::size_t i = 0; i < expected_codes.size(); ++i) {
        check(normalized.source_codes.at(i).decimal() == expected_codes[i],
              "Exact original source label value changed");
        const auto count = std::count(expected_labels.begin(), expected_labels.end(),
                                      std::uint32_t(i + 1));
        check(normalized.raster.regions()[i].pixels == std::uint64_t(count),
              "Repeated/disconnected source labels changed instance identity");
    }
    const auto positive = std::count_if(expected_labels.begin(), expected_labels.end(),
                                       [](auto v) { return v != 0; });
    check(normalized.raster.foreground_pixels() == std::uint64_t(positive),
          "Zero background or negative foreground classification changed");
    // An unrelated dense literal raster must have precisely the same identity.
    check(normalized.raster.sha256() == LabelRaster(4, 2, expected_labels).sha256(),
          "Dense label raster identity differs from literal expected pixels");
    std::get<std::vector<T>>(raw.writable_storage())[0] = 0;
    check(normalized.raster.labels() == expected_labels &&
              normalized.source_codes.at(0).decimal() == expected_codes[0],
          "Normalized labels or source-code map borrowed mutable input storage");
}
void native_integer_types() {
    exact_case<std::uint8_t>(SampleType::UInt8, {9, 0, 3, 9, 3, 0, 9, 3},
                             {2, 0, 1, 2, 1, 0, 2, 1}, {"3", "9"});
    exact_case<std::uint16_t>(SampleType::UInt16, {65535, 256, 0, 65535, 1, 256, 0, 1},
                              {3, 2, 0, 3, 1, 2, 0, 1}, {"1", "256", "65535"});
    exact_case<std::int8_t>(SampleType::Int8, {-128, 127, 0, -1, -1, 0, 127, -128},
                            {1, 3, 0, 2, 2, 0, 3, 1}, {"-128", "-1", "127"});
    exact_case<std::int16_t>(SampleType::Int16,
                             {-32768, 0, 32767, -256, 32767, 0, -32768, -256},
                             {1, 0, 3, 2, 3, 0, 1, 2}, {"-32768", "-256", "32767"});
    exact_case<std::uint32_t>(SampleType::UInt32,
                              {4294967295u, 0, 1, 2147483648u, 1, 4294967295u, 2147483648u, 0},
                              {3, 0, 1, 2, 1, 3, 2, 0},
                              {"1", "2147483648", "4294967295"});
    exact_case<std::int32_t>(SampleType::Int32,
                             {INT32_MIN, INT32_MAX, -1, 0, -1, INT32_MIN, 0, INT32_MAX},
                             {1, 3, 2, 0, 2, 1, 0, 3},
                             {"-2147483648", "-1", "2147483647"});
    exact_case<std::uint64_t>(
        SampleType::UInt64,
        {18446744073709551615ull, 9007199254740993ull, 9007199254740992ull, 0,
         4294967296ull, 18446744073709551615ull, 9007199254740993ull, 1},
        {5, 4, 3, 0, 2, 5, 4, 1},
        {"1", "4294967296", "9007199254740992", "9007199254740993", "18446744073709551615"});
    exact_case<std::int64_t>(
        SampleType::Int64,
        {INT64_MIN, -9007199254740993ll, -1, 0, INT64_MAX, 9007199254740993ll, INT64_MIN, 1},
        {1, 2, 3, 0, 6, 5, 1, 4},
        {"-9223372036854775808", "-9007199254740993", "-1", "1", "9007199254740993",
         "9223372036854775807"});
}
void exact_value_operations() {
    const auto zero = LabelCode::from_unsigned(0);
    const auto minimum = LabelCode::from_signed(INT64_MIN);
    const auto maximum = LabelCode::from_unsigned(UINT64_MAX);
    check(!LabelCode(true, 0).negative() && LabelCode(true, 0) == zero && zero.decimal() == "0",
          "Negative zero is not canonical");
    check(minimum.negative() && minimum.magnitude() == 9223372036854775808ull &&
              minimum.decimal() == "-9223372036854775808" &&
              maximum.magnitude() == 18446744073709551615ull &&
              maximum.decimal() == "18446744073709551615",
          "Signed minimum or unsigned maximum lost magnitude/sign");
    refuses<std::invalid_argument>([] { LabelCode(true, UINT64_MAX); });
    const auto signed_one = LabelCode::from_signed(1);
    const auto unsigned_one = LabelCode::from_unsigned(1);
    check(signed_one == unsigned_one && !(signed_one != unsigned_one) &&
              signed_one <= unsigned_one && signed_one >= unsigned_one &&
              signed_one.stable_hash() == unsigned_one.stable_hash(),
          "Equivalent signed/unsigned mathematical values compare or hash differently");
    std::vector<LabelCode> ordered{maximum, unsigned_one, LabelCode::from_signed(-1),
                                    LabelCode::from_unsigned(9007199254740993ull), minimum,
                                    zero, LabelCode::from_signed(INT64_MAX)};
    std::sort(ordered.begin(), ordered.end());
    const std::vector<std::string> expected{"-9223372036854775808", "-1", "0", "1",
                                            "9007199254740993", "9223372036854775807",
                                            "18446744073709551615"};
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        check(ordered[i].decimal() == expected[i], "Exact cross-sign source-code ordering differs");
        if (i)
            check(ordered[i] > ordered[i - 1] && ordered[i - 1] < ordered[i],
                  "Source-code comparison relations disagree");
    }
    // Pinned FNV-1a byte-level oracles, independent of native host endian/word size.
    check(zero.stable_hash() == 16574515714863409599ull &&
              unsigned_one.stable_hash() == 14342200307895820190ull &&
              LabelCode::from_signed(-1).stable_hash() == 8184434590310759885ull &&
              minimum.stable_hash() == 5951978445854759468ull &&
              maximum.stable_hash() == 8811690462992864311ull,
          "Canonical source-code hash changed");
}
void maps_and_legacy_storage() {
    auto raw = raw_values<std::uint16_t>(SampleType::UInt16, 3, 1, {65535, 0, 257});
    auto normalized = normalize_label_codes(raw);
    const auto& codes = std::get<std::vector<std::uint32_t>>(normalized.source_codes.storage());
    const auto* allocation = codes.data();
    auto legacy = std::move(normalized.source_codes).release_uint32();
    check(legacy == std::vector<std::uint32_t>{257, 65535} && legacy.data() == allocation,
          "Legacy mapping changed values or acquired an unnecessary overlapping allocation");
    LabelCodeMap signed_map(std::vector<std::int64_t>{INT64_MIN, -1, 1, INT64_MAX});
    check(signed_map.at(0) == LabelCode::from_signed(INT64_MIN) &&
              signed_map.at(3) == LabelCode::from_signed(INT64_MAX),
          "Typed64 mapping changed exact signed values");
    refuses<std::out_of_range>([&] { signed_map.at(4); });
    refuses<std::invalid_argument>([&] { std::move(signed_map).release_uint32(); });
    LabelCodeMap huge_map(std::vector<std::uint64_t>{UINT64_MAX});
    refuses<std::invalid_argument>([&] { std::move(huge_map).release_uint32(); });
    LabelCodeMap narrowable(std::vector<std::uint64_t>{1, UINT32_MAX});
    refuses<std::invalid_argument>([&] { std::move(narrowable).release_uint32(); });
    refuses<std::invalid_argument>([] { LabelCodeMap m(std::vector<std::uint32_t>{0, 1}); });
    refuses<std::invalid_argument>([] { LabelCodeMap m(std::vector<std::uint64_t>{1, 1}); });
    refuses<std::invalid_argument>([] { LabelCodeMap m(std::vector<std::int32_t>{-1, -2}); });
}
void zero_background() {
    for (const auto type : {SampleType::UInt8, SampleType::UInt16, SampleType::Int8,
                            SampleType::Int16, SampleType::UInt32, SampleType::Int32,
                            SampleType::UInt64, SampleType::Int64}) {
        RawPixelBuffer raw(4, 2, 1, type);
        std::fill(raw.writable_valid().begin(), raw.writable_valid().end(), 1);
        const auto normalized = normalize_label_codes(raw);
        check(normalized.source_codes.size() == 0 && normalized.raster.regions().empty() &&
                  normalized.raster.foreground_pixels() == 0 &&
                  normalized.raster.labels() == std::vector<std::uint32_t>(8, 0),
              "Valid zero-only integral image acquired foreground labels");
    }
}
void invalid_inputs_and_limits() {
    auto raw = raw_values<std::int64_t>(SampleType::Int64, 4, 1, {-1, 0, 1, -1});
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>([&] { normalize_label_codes(raw, {}, &cancelled); });
    refuses<WorkflowCaptureLimit>([&] { normalize_label_codes(raw, {1, 256u * 1024 * 1024}); });
    refuses<std::invalid_argument>([&] { normalize_label_codes(raw, {0, 256u * 1024 * 1024}); });
    refuses<std::invalid_argument>([&] { normalize_label_codes(raw, {1000001, 256u * 1024 * 1024}); });
    refuses<std::invalid_argument>([&] { normalize_label_codes(raw, {2, 4095}); });
    refuses<std::invalid_argument>([&] { normalize_label_codes(raw, {2, 1024ull * 1024 * 1024 + 1}); });
    refuses<WorkflowCaptureLimit>([&] { normalize_label_codes(raw, {2, 65536}); });
    auto missing = raw;
    missing.writable_valid()[1] = 0;
    refuses<std::invalid_argument>([&] { normalize_label_codes(missing); });
    auto invalid_validity = raw;
    invalid_validity.writable_valid()[1] = 2;
    refuses<std::invalid_argument>([&] { normalize_label_codes(invalid_validity); });
    auto validity_count = raw;
    validity_count.writable_valid().pop_back();
    refuses<std::invalid_argument>([&] { normalize_label_codes(validity_count); });
    auto sample_count = raw;
    std::get<std::vector<std::int64_t>>(sample_count.writable_storage()).pop_back();
    refuses<std::invalid_argument>([&] { normalize_label_codes(sample_count); });
    auto sample_type = raw;
    sample_type.writable_storage() = std::vector<std::uint64_t>{1, 0, 1, 1};
    refuses<std::invalid_argument>([&] { normalize_label_codes(sample_type); });
    RawPixelBuffer channels(2, 1, 2, SampleType::UInt8);
    channels.writable_valid() = {1, 1};
    refuses<std::invalid_argument>([&] { normalize_label_codes(channels); });
    auto f32 = raw_values<float>(SampleType::Float32, 2, 1, {0.f, 1.f});
    auto f64 = raw_values<double>(SampleType::Float64, 2, 1, {0., 9007199254740992.});
    refuses<std::invalid_argument>([&] { normalize_label_codes(f32); });
    refuses<std::invalid_argument>([&] { normalize_label_codes(f64); });
    auto repeated = raw_values<std::uint64_t>(SampleType::UInt64, 100, 100,
                                              std::vector<std::uint64_t>(10000, UINT64_MAX));
    // Retained dense output fits; borrowed raw + full sorting copy does not.
    refuses<WorkflowCaptureLimit>([&] { normalize_label_codes(repeated, {1, 220000}); });
    const auto bounded = normalize_label_codes(repeated, {1, 256000});
    check(bounded.raster.foreground_pixels() == 10000 && bounded.source_codes.size() == 1 &&
              bounded.source_codes.at(0).decimal() == "18446744073709551615",
          "A fitting bounded normalization was refused or changed");
    std::vector<std::uint32_t> distinct(2000);
    for (std::size_t i = 0; i < distinct.size(); ++i)
        distinct[i] = std::uint32_t(i + 1);
    auto many_regions = raw_values<std::uint32_t>(SampleType::UInt32, 100, 20,
                                                  std::move(distinct));
    // Sorting/mapping fit; complete retained LabelRaster regions must also fit.
    refuses<WorkflowCaptureLimit>([&] { normalize_label_codes(many_regions, {2000, 110000}); });
}
} // namespace
int main() {
    try {
        native_integer_types();
        exact_value_operations();
        maps_and_legacy_storage();
        zero_background();
        invalid_inputs_and_limits();
        std::cout << "Exact integral label-code normalization tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
