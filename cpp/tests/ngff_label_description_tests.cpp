#include "qupathxx/ngff_label_mask.hpp"
#include "qupathxx/workflow.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace qpx;
namespace {
// Authored independently of the provider/serializer. Embedded metadata strings
// intentionally retain whitespace, ordering and a decimal integer above2^53.
constexpr const char description_json[] = R"json({
"schema":"qpx.label-mask-description/1",
"format":"ome-ngff-labels-0.4-zarr2/1",
"label_group":"labels/cells",
"reference_group":"",
"source_reference":{"path":"../../","origin":"default"},
"level":0,
"z":0,
"t":0,
"sample_type":"uint64",
"label_frame":{"axes":[
{"name":"y","unit":"micrometer","extent":"12","scale":0.5,"translation":2},
{"name":"x","unit":"nanometer","extent":"16","scale":250,"translation":1000}
]},
"reference_frame":{"axes":[
{"name":"y","unit":"micrometer","extent":"12","scale":0.5,"translation":2},
{"name":"x","unit":"micrometer","extent":"16","scale":0.25,"translation":1}
]},
"label_metadata":" {\n \"image-label\": {\"version\":\"0.4\"}, \"raw-code\":9007199254740993\n} ",
"reference_metadata":"{\"multiscales\":[{\"version\":\"0.4\"}]}\n ",
"label_properties":[
{"code":"0","properties":" { \"label-value\": 0, \"name\": \"background\" } ","rgba":[0,0,0,0]},
{"code":"9007199254740992","properties":"{\"name\":\"first\"}","rgba":null},
{"code":"9007199254740993","properties":"{\"name\":\"adjacent\"}","rgba":[1,2,254,255]},
{"code":"18446744073709551615","properties":"{\"name\":\"maximum\"}","rgba":null}
]
})json";
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class E = std::exception, class F> void refuses(const char* name, F f) {
    try {
        f();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error(std::string("Invalid NGFF label description/options accepted: ") + name);
}
std::string replace_one(std::string text, const std::string& old, const std::string& value) {
    const auto at = text.find(old);
    check(at != std::string::npos, "Independent descriptor fixture replacement did not match");
    text.replace(at, old.size(), value);
    return text;
}
// The controlled literal has one top-level scalar/object field per physical line.
std::string field(std::string text, const std::string& name, const std::string& json_value) {
    const auto marker = '"' + name + "\":";
    const auto key = text.find(marker);
    check(key != std::string::npos, "Independent descriptor fixture field did not match");
    const auto begin = key + marker.size();
    auto end = text.find('\n', begin);
    check(end != std::string::npos, "Independent descriptor fixture field has no terminator");
    if (text[end - 1] == ',')
        --end;
    text.replace(begin, end - begin, json_value);
    return text;
}
std::string properties(std::string text, const std::string& json_array) {
    const std::string marker = "\"label_properties\":";
    const auto key = text.find(marker);
    const auto end = text.rfind(']');
    check(key != std::string::npos && end != std::string::npos && end > key,
          "Independent descriptor fixture property list did not match");
    const auto begin = key + marker.size();
    text.replace(begin, end + 1 - begin, json_array);
    return text;
}
std::string one_code(const std::string& code) {
    return "[{\"code\":\"" + code + "\",\"properties\":\"{}\",\"rgba\":null}]";
}
void exact_owned_description() {
    const std::string literal = description_json;
    const auto decoded = decode_label_mask_description(literal);
    check(decoded.format == "ome-ngff-labels-0.4-zarr2/1" &&
              decoded.label_group == "labels/cells" && decoded.reference_group.empty() &&
              decoded.source_reference == "../../" && decoded.default_reference &&
              decoded.level == 0 && decoded.z == 0 && decoded.t == 0 &&
              decoded.sample_type == SampleType::UInt64,
          "Neutral label selection/profile changed or was inferred");
    check(decoded.label_frame.axes.size() == 2 && decoded.reference_frame.axes.size() == 2 &&
              decoded.label_frame.axes[0].name == "y" &&
              decoded.label_frame.axes[0].unit == "micrometer" &&
              decoded.label_frame.axes[0].extent == 12 &&
              decoded.label_frame.axes[0].scale == .5 &&
              decoded.label_frame.axes[0].translation == 2 &&
              decoded.label_frame.axes[1].name == "x" &&
              decoded.label_frame.axes[1].unit == "nanometer" &&
              decoded.label_frame.axes[1].extent == 16 &&
              decoded.label_frame.axes[1].scale == 250 &&
              decoded.label_frame.axes[1].translation == 1000 &&
              decoded.reference_frame.axes[1].unit == "micrometer" &&
              decoded.reference_frame.axes[1].scale == .25 &&
              decoded.reference_frame.axes[1].translation == 1,
          "Neutral frame changed original axis units, extents or transform values");
    const std::string expected_label =
        " {\n \"image-label\": {\"version\":\"0.4\"}, \"raw-code\":9007199254740993\n} ";
    const std::string expected_reference = "{\"multiscales\":[{\"version\":\"0.4\"}]}\n ";
    check(decoded.label_metadata == expected_label && decoded.reference_metadata == expected_reference &&
              decoded.json == literal,
          "Original metadata or neutral descriptor bytes were reserialized/lost");
    check(decoded.label_properties.size() == 4 &&
              decoded.label_properties[0].code.decimal() == "0" &&
              decoded.label_properties[1].code.decimal() == "9007199254740992" &&
              decoded.label_properties[2].code.decimal() == "9007199254740993" &&
              decoded.label_properties[3].code.decimal() == "18446744073709551615" &&
              decoded.label_properties[0].properties ==
                  " { \"label-value\": 0, \"name\": \"background\" } " &&
              decoded.label_properties[0].rgba &&
              *decoded.label_properties[0].rgba == std::array<std::uint8_t, 4>{{0, 0, 0, 0}} &&
              decoded.label_properties[2].rgba &&
              *decoded.label_properties[2].rgba == std::array<std::uint8_t, 4>{{1, 2, 254, 255}} &&
              !decoded.label_properties[1].rgba && !decoded.label_properties[3].rgba,
          "Exact source codes, opaque properties or explicit colors changed");
    const auto repeated = decode_label_mask_description(literal);
    check(decoded.sha256 == repeated.sha256 && decoded.sha256.size() == 64 &&
              decoded.sha256 == "c073c3b7df15c2b68a196b613900cb942a1896fefbb2f9c49791ae5f7d0013d2",
          "Exact neutral-description SHA identity differs from independent literal oracle");
    const auto whitespace = decode_label_mask_description("\n" + literal);
    check(whitespace.sha256 != decoded.sha256 && whitespace.json == "\n" + literal &&
              whitespace.label_metadata == expected_label,
          "Exact descriptor identity silently normalized leading whitespace");
    auto source = literal;
    auto owned = decode_label_mask_description(source);
    source.assign("changed after decode");
    check(owned.json == literal && owned.label_metadata == expected_label &&
              owned.label_properties[2].code.magnitude() == 9007199254740993ull,
          "Neutral description borrowed caller text storage");
    check(decoded.payload_bytes() >= sizeof(LabelMaskSourceDescription) + decoded.json.size() +
              decoded.label_metadata.size() + decoded.reference_metadata.size(),
          "Owned description payload omits retained descriptor/metadata bytes");
}
void all_integral_types_and_codes() {
    for (const auto& entry : {std::pair<const char*, SampleType>{"uint8", SampleType::UInt8},
                              {"uint16", SampleType::UInt16}, {"uint32", SampleType::UInt32},
                              {"uint64", SampleType::UInt64}, {"int8", SampleType::Int8},
                              {"int16", SampleType::Int16}, {"int32", SampleType::Int32},
                              {"int64", SampleType::Int64}}) {
        auto text = field(description_json, "sample_type", '"' + std::string(entry.first) + '"');
        text = properties(std::move(text), one_code("0"));
        const auto decoded = decode_label_mask_description(text);
        check(decoded.sample_type == entry.second && decoded.label_properties.size() == 1 &&
                  decoded.label_properties[0].code == LabelCode::from_unsigned(0),
              "An exact integral sample type or zero metadata code was converted");
    }
    auto signed_text = field(description_json, "sample_type", "\"int64\"");
    signed_text = properties(std::move(signed_text), R"json([
{"code":"-9223372036854775808","properties":"{}","rgba":null},
{"code":"-9007199254740993","properties":"{}","rgba":null},
{"code":"-1","properties":"{}","rgba":null},
{"code":"0","properties":"{}","rgba":null},
{"code":"9007199254740992","properties":"{}","rgba":null},
{"code":"9007199254740993","properties":"{}","rgba":null},
{"code":"9223372036854775807","properties":"{}","rgba":null}
])json");
    const auto signed_codes = decode_label_mask_description(signed_text);
    const std::string expected[]{"-9223372036854775808", "-9007199254740993", "-1", "0",
                                  "9007199254740992", "9007199254740993", "9223372036854775807"};
    check(signed_codes.label_properties.size() == 7, "Exact signed metadata codes were dropped");
    for (std::size_t i = 0; i < 7; ++i)
        check(signed_codes.label_properties[i].code.decimal() == expected[i],
              "Exact signed source-code string changed in neutral decoding");
    check(signed_codes.label_properties[0].code.negative() &&
              signed_codes.label_properties[0].code.magnitude() == 9223372036854775808ull,
          "INT64_MIN property code lost its exact sign/magnitude");
    const auto empty = decode_label_mask_description(properties(description_json, "[]"));
    check(empty.label_properties.empty(), "Missing class/color/property metadata was invented");
}
void explicit_declarations() {
    auto text = field(description_json, "source_reference",
                      R"json({"path":"../../named","origin":"explicit"})json");
    text = field(std::move(text), "reference_group", "\"named/reference\"");
    const auto explicit_source = decode_label_mask_description(text);
    check(!explicit_source.default_reference && explicit_source.source_reference == "../../named" &&
              explicit_source.reference_group == "named/reference",
          "Explicit reference origin/path/group was inferred or replaced");
    text = field(description_json, "source_reference",
                  R"json({"path":"../../","origin":"explicit"})json");
    check(!decode_label_mask_description(text).default_reference,
          "Default-reference origin was inferred from a path spelling");
    text = replace_one(description_json, "\"unit\":\"micrometer\"", "\"unit\":\"\"");
    const auto unitless = decode_label_mask_description(text);
    check(unitless.label_frame.axes[0].unit.empty() && unitless.label_frame.axes[0].scale == .5,
          "A physical unit was inferred from a scale/container");
}
void malformed_and_noncanonical() {
    const auto invalid = [&](const char* name, std::string text) {
        refuses(name, [&] { decode_label_mask_description(std::move(text)); });
    };
    invalid("empty document", "");
    invalid("wrong root type", "[]");
    invalid("truncated document", std::string(description_json).substr(0, 50));
    invalid("trailing second document", std::string(description_json) + "{}");
    invalid("unsupported schema", field(description_json, "schema", "\"other/1\""));
    invalid("unsupported format", field(description_json, "format", "\"ome-ngff-labels-0.5/1\""));
    invalid("missing required member", replace_one(description_json, "\"t\":0,\n", ""));
    invalid("extra member", replace_one(description_json, "{\n", "{\n\"extra\":0,\n"));
    invalid("unknown reference origin", field(description_json, "source_reference",
             R"json({"path":"../../","origin":"inferred"})json"));
    invalid("empty source-reference path", field(description_json, "source_reference",
             R"json({"path":"","origin":"explicit"})json"));
    invalid("missing source-reference origin", field(description_json, "source_reference",
             R"json({"path":"../../"})json"));
    invalid("source-reference has extra member", field(description_json, "source_reference",
             R"json({"path":"../../","origin":"default","extra":0})json"));
    for (const auto* type : {"float32", "float64", "uint128", "UInt64", "u8", ""})
        invalid("nonintegral or unknown dtype", field(description_json, "sample_type", '"' + std::string(type) + '"'));
    for (const auto& member : {"level", "z", "t"})
        for (const auto& value : {"-1", "4294967296", "1.0", "\"0\"", "null", "true"})
            invalid("invalid typed level/plane index", field(description_json, member, value));
    invalid("nonzero label level", field(description_json, "level", "1"));
    for (const auto* code : {"", "-0", "+0", "00", "01", "-01", "+1", " 1", "1 ",
                             "1.0", "1e3", "-", "18446744073709551616", "-9223372036854775809"})
        invalid("noncanonical/overflow exact source code", properties(description_json, one_code(code)));
    invalid("numeric source code instead of exact string", properties(description_json,
             R"json([{"code":9007199254740993,"properties":"{}","rgba":null}])json"));
    invalid("repeated source code", replace_one(description_json, "\"code\":\"9007199254740993\"",
             "\"code\":\"9007199254740992\""));
    invalid("unsorted source code", replace_one(description_json, "\"code\":\"0\"",
             "\"code\":\"18446744073709551615\""));
    for (const auto* extent : {"", "0", "-1", "+1", "01", "1.0", "1e2", " 1", "1 ",
                               "18446744073709551616"})
        invalid("noncanonical/overflow exact extent", replace_one(description_json, "\"extent\":\"12\"",
                 "\"extent\":\"" + std::string(extent) + '"'));
    invalid("numeric extent instead of exact string", replace_one(description_json,
             "\"extent\":\"12\"", "\"extent\":12"));
    for (const auto* value : {"0", "-1", "1e999", "\"1\"", "null"})
        invalid("invalid frame scale", replace_one(description_json, "\"scale\":0.5",
                 "\"scale\":" + std::string(value)));
    invalid("nonnumeric translation", replace_one(description_json, "\"translation\":2",
             "\"translation\":\"2\""));
    invalid("empty label metadata", field(description_json, "label_metadata", "\"\""));
    invalid("empty reference metadata", field(description_json, "reference_metadata", "\"\""));
    invalid("missing original metadata type", field(description_json, "label_metadata", "{}"));
    invalid("nonarray properties", properties(description_json, "{}"));
    for (const auto* rgba : {"[0,0,0]", "[0,0,0,0,0]", "[-1,0,0,0]", "[256,0,0,0]",
                             "[1.0,0,0,0]", "[true,0,0,0]", "\"red\""})
        invalid("invalid explicit RGBA", properties(description_json,
                 "[{\"code\":\"0\",\"properties\":\"{}\",\"rgba\":" + std::string(rgba) + "}]"));
    invalid("numeric property payload", properties(description_json,
             R"json([{"code":"0","properties":1,"rgba":null}])json"));
    invalid("duplicate root key", replace_one(description_json, "{\n",
             "{\n\"schema\":\"qpx.label-mask-description/1\",\n"));
    invalid("duplicate nested frame key", replace_one(description_json, "\"extent\":\"12\"",
             "\"extent\":\"12\",\"extent\":\"12\""));
    invalid("duplicate property key", replace_one(description_json, "\"code\":\"0\"",
             "\"code\":\"0\",\"code\":\"0\""));
    invalid("duplicate reference key", field(description_json, "source_reference",
             R"json({"path":"../../","path":"../../","origin":"default"})json"));
    auto invalid_utf8 = std::string(description_json);
    invalid_utf8.insert(invalid_utf8.find("cells"), 1, char(0xc0));
    invalid("invalid UTF8", std::move(invalid_utf8));
    invalid("escaped embedded NUL", field(description_json, "label_metadata", "\"\\u0000\""));
}
void size_depth_and_cancellation() {
    check(label_mask_description_limit == 65536, "Declared neutral-description byte bound changed");
    std::string boundary = description_json;
    check(boundary.size() < label_mask_description_limit, "Literal fixture exceeds decoder bound");
    boundary.append(label_mask_description_limit - boundary.size(), ' ');
    check(decode_label_mask_description(boundary).json == boundary,
          "Valid neutral description at the declared byte boundary was refused/truncated");
    refuses("total description byte bound", [&] { decode_label_mask_description(boundary + ' '); });
    const auto metadata_boundary = field(description_json, "label_metadata",
                                         '"' + std::string(24576, 'm') + '"');
    check(decode_label_mask_description(metadata_boundary).label_metadata == std::string(24576, 'm'),
          "Valid original metadata at the declared byte boundary changed");
    refuses("original metadata byte bound", [&] {
        decode_label_mask_description(field(description_json, "label_metadata",
                                             '"' + std::string(24577, 'm') + '"'));
    });
    std::string deep(24, '[');
    deep += '0';
    deep.append(24, ']');
    refuses<WorkflowCaptureLimit>("neutral JSON structural depth", [&] {
        decode_label_mask_description(properties(description_json, deep));
    });
    std::string nodes = "[";
    for (unsigned i = 0; i < 17000; ++i)
        nodes += i ? ",0" : "0";
    nodes += ']';
    refuses<WorkflowCaptureLimit>("neutral JSON structural node count", [&] {
        decode_label_mask_description(properties(description_json, nodes));
    });
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>("cancelled neutral decode", [&] {
        decode_label_mask_description(description_json, &cancelled);
    });
}
NgffLabelMaskOptions valid_options() {
    NgffLabelMaskOptions result;
    result.path = std::filesystem::absolute("description-only.zarr").u8string();
    result.label_group = "labels/cells";
    result.author = u8"Author \u03b1";
    result.input_sha256 = std::string(64, 'a');
    return result;
}
void typed_options() {
    const auto good = valid_options();
    good.validate();
    check(ngff_label_selection_json(good) ==
              R"json({"label_group":"labels/cells","level":0,"reference_group":"","schema":"qpx.ngff-label-selection/1","t":0,"z":0})json",
          "Native selection JSON inferred or changed a declared group/plane");
    auto explicit_options = good;
    explicit_options.reference_group = "named/reference";
    explicit_options.z = 3;
    explicit_options.t = 4;
    explicit_options.reference_z = 5;
    explicit_options.reference_t = 6;
    explicit_options.reader_sha256 = std::string(64, 'c');
    explicit_options.validate();
    check(ngff_label_selection_json(explicit_options) ==
              R"json({"label_group":"labels/cells","level":0,"reference_group":"named/reference","schema":"qpx.ngff-label-selection/1","t":4,"z":3})json",
          "Native decoder selection confused source and explicit reference planes");
    auto producer = good;
    producer.origin = LabelMaskOrigin::Producer;
    producer.author.clear();
    producer.producer = "Segmenter";
    producer.producer_version = "1.2.3";
    producer.model_sha256 = std::string(64, 'b');
    producer.validate();
    check(producer.payload_bytes() >= sizeof(NgffLabelMaskOptions) + producer.path.size() +
              producer.input_sha256.size() + producer.model_sha256.size(),
          "Typed source options omit retained owned identity/path bytes");
    const auto invalid = [&](const char* name, auto modify) {
        auto bad = good;
        modify(bad);
        refuses<std::invalid_argument>(name, [&] { bad.validate(); });
    };
    invalid("relative store path", [](auto& o) { o.path = "relative.zarr"; });
    invalid("empty store path", [](auto& o) { o.path.clear(); });
    invalid("embedded NUL path", [](auto& o) { o.path += std::string(1, '\0'); });
    invalid("store path byte bound", [](auto& o) { o.path.assign(2049, 'p'); });
    invalid("label level is not original lattice", [](auto& o) { o.level = 1; });
    invalid("missing authored attribution", [](auto& o) { o.author.clear(); });
    invalid("authored identity must not invent producer", [](auto& o) { o.producer = "tool"; });
    invalid("authored identity must not invent version", [](auto& o) { o.producer_version = "1"; });
    invalid("authored identity must not invent model", [](auto& o) { o.model_sha256.assign(64, 'b'); });
    invalid("unknown source origin", [](auto& o) { o.origin = LabelMaskOrigin(99); });
    invalid("empty target input identity", [](auto& o) { o.input_sha256.clear(); });
    invalid("uppercase target input identity", [](auto& o) { o.input_sha256.assign(64, 'A'); });
    invalid("nonhex target input identity", [](auto& o) { o.input_sha256.assign(64, 'g'); });
    invalid("short target input identity", [](auto& o) { o.input_sha256.assign(63, 'a'); });
    invalid("invalid reader identity", [](auto& o) { o.reader_sha256 = "reader"; });
    invalid("zero region limit", [](auto& o) { o.max_regions = 0; });
    invalid("region limit beyond native domain", [](auto& o) { o.max_regions = 1000001; });
    invalid("author byte bound", [](auto& o) { o.author.assign(257, 'a'); });
    invalid("empty label group", [](auto& o) { o.label_group.clear(); });
    for (const auto* group : {".", "..", "../cells", "labels/../cells", "labels//cells",
                              "/labels", "labels\\cells", "C:labels", "labels/"}) {
        invalid("unsafe label group", [&](auto& o) { o.label_group = group; });
        invalid("unsafe reference group", [&](auto& o) { o.reference_group = group; });
    }
    invalid("group byte bound", [](auto& o) { o.label_group.assign(1025, 'g'); });
    invalid("embedded NUL group", [](auto& o) { o.label_group += std::string(1, '\0'); });
    for (int mode = 0; mode < 5; ++mode) {
        auto bad = producer;
        if (mode == 0)
            bad.author = "not empty";
        if (mode == 1)
            bad.producer.clear();
        if (mode == 2)
            bad.producer_version.clear();
        if (mode == 3)
            bad.model_sha256.clear();
        if (mode == 4)
            bad.model_sha256.assign(64, 'B');
        refuses<std::invalid_argument>("incomplete/mixed producer attribution", [&] { bad.validate(); });
    }
}
} // namespace
int main() {
    try {
        exact_owned_description();
        all_integral_types_and_codes();
        explicit_declarations();
        malformed_and_noncanonical();
        size_depth_and_cancellation();
        typed_options();
        std::cout << "Native neutral NGFF label-description/options tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
