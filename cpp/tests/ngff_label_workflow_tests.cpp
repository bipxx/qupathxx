// SPDX-License-Identifier: AGPL-3.0-only
#include "label_cli.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/label_mask_archive.hpp"
#include "qupathxx/label_workflow.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_batch.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_fixture.hpp"
#include "nucls_png_fixture.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>

using namespace qpx;
namespace fs = std::filesystem;
namespace {
using Json = nlohmann::json;
using Value = ImageDataValue;
constexpr const char* group_name = "labels/cells";
// Opaque property numbers are original JSON tokens, not values to round through
// binary64. The oversized integers remain separate from the exact UInt64 ID.
constexpr const char lexical_property_json[] = R"json({
  "label-value" : 18446744073709551615,
  "name" : "code-18446744073709551615",
  "unknown" : {"nested":["keep",true],"exact-integer":9007199254740993},
  "beyond-uint64" : 184467440737095516160,
  "below-int64" : -92233720368547758080,
  "precise-decimal" : 0.12345678901234567890123456789
})json";
// Independent literal RFC4180 field: every JSON quote is doubled; source
// whitespace, newlines and numeric token spellings otherwise remain unchanged.
constexpr const char lexical_property_csv[] = R"csv("{
  ""label-value"" : 18446744073709551615,
  ""name"" : ""code-18446744073709551615"",
  ""unknown"" : {""nested"":[""keep"",true],""exact-integer"":9007199254740993},
  ""beyond-uint64"" : 184467440737095516160,
  ""below-int64"" : -92233720368547758080,
  ""precise-decimal"" : 0.12345678901234567890123456789
}")csv";
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(double value, double expected) {
    check(std::abs(value - expected) < 1e-9, "Original raw-channel analytical value changed");
}
template <class E = std::exception, class F> std::string refuses(F f) {
    try {
        f();
    } catch (const E& error) {
        return error.what();
    }
    throw std::runtime_error("Expected native NGFF label workflow refusal");
}
void write(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), std::streamsize(text.size()));
    out.close();
    check(bool(out), "Cannot write independently authored NGFF fixture/output");
}
std::string bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    check(bool(in), "Cannot read authored NGFF fixture/output");
    return {std::istreambuf_iterator<char>(in), {}};
}
Json json(const fs::path& path) { return Json::parse(bytes(path)); }
void write_json(const fs::path& path, const Json& value) { write(path, value.dump(2) + '\n'); }
std::vector<std::string> tree_entries(const fs::path& root) {
    std::vector<std::string> result;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        result.push_back(entry.path().lexically_relative(root).generic_u8string() +
                         (entry.is_directory() ? "/" : ""));
    std::sort(result.begin(), result.end());
    return result;
}
void little32(std::string& text, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        text += char(value >> (8 * i));
}
std::string compressed(const std::string& raw, const std::string& codec) {
    if (codec.empty())
        return raw;
    const auto zlib = nucls_png_fixture::stored(raw);
    if (codec == "zlib")
        return zlib;
    check(codec == "gzip", "Unexpected independent fixture codec");
    std::string gzip("\x1f\x8b\x08\0\0\0\0\0\0\xff", 10);
    gzip += zlib.substr(2, zlib.size() - 6); // Same literal stored Deflate block.
    little32(gzip, nucls_png_fixture::crc(raw));
    little32(gzip, std::uint32_t(raw.size()));
    return gzip;
}
template <class T> std::string samples(const std::vector<T>& values) {
    static_assert(std::is_integral_v<T>);
    using U = std::make_unsigned_t<T>;
    std::string result;
    for (const auto value : values) {
        const auto bits = U(value); // Defined modulo2^N for negative exact integers.
        for (unsigned b = 0; b < sizeof(T); ++b)
            result += char(bits >> (8 * b));
    }
    return result;
}
Json axis(const char* name, const char* type, const char* unit = "") {
    Json result{{"name", name}, {"type", type}};
    if (*unit)
        result["unit"] = unit;
    return result;
}
Json multiscales(Json axes, const std::vector<double>& scale,
                  const std::vector<double>& translation) {
    const Json dataset{{"path", "0"}, {"coordinateTransformations",
        Json::array({{{"type", "scale"}, {"scale", scale}},
                     {{"type", "translation"}, {"translation", translation}}})}};
    return Json::array({{{"version", "0.4"}, {"axes", std::move(axes)},
                          {"datasets", Json::array({dataset})}}});
}
Json array_spec(const std::vector<std::uint64_t>& shape,
                 const std::vector<std::uint64_t>& chunks, const std::string& dtype,
                 const std::string& codec) {
    return {{"zarr_format", 2}, {"shape", shape}, {"chunks", chunks}, {"dtype", dtype},
        {"compressor", codec.empty() ? Json(nullptr) : Json{{"id", codec}}},
        {"fill_value", 0}, {"order", "C"}, {"filters", nullptr}, {"dimension_separator", "."}};
}
struct Layout {
    unsigned width = 2, height = 2, z = 0, t = 0;
    double origin_x = 0, origin_y = 0, origin_z = 0, origin_t = 0;
    std::string codec;
    bool default_reference = false;
};
// Every store and sample below is authored locally. No production encoder,
// downloaded fixture, decoder fallback or Python analysis participates.
template <class T>
fs::path store(const fs::path& path, const std::vector<T>& values, Layout layout = {}) {
    check(!fs::exists(path), "Authored NGFF store already exists");
    fs::create_directories(path / "0");
    fs::create_directories(path / "labels/cells/0");
    for (const auto& directory : {path, path / "labels", path / "labels/cells"})
        write_json(directory / ".zgroup", {{"zarr_format", 2}});
    write_json(path / "labels/.zattrs", {{"labels", Json::array({"cells"})}});
    Json reference{{"multiscales", multiscales(
        Json::array({axis("c", "channel"), axis("z", "space"),
                     axis("y", "space", "micrometer"), axis("x", "space", "micrometer")}),
        {1, 1, .75, .5}, {0, 0, 0, 0})},
        {"omero", {{"channels", Json::array({{{"label", "High"}}, {{"label", "Zero"}},
                                                {{"label", "Marker"}}})}}},
        {"user-reference", {{"authored", true}, {"unknown", "retain verbatim"}}}};
    write_json(path / ".zattrs", reference);
    write_json(path / "0/.zarray", array_spec({3, 2, 2, 2}, {1, 1, 2, 2}, "<u2", ""));
    // Literal C/Z planes match fluorescence_fixture, including its original
    // unsigned16 intensities; the label mask never supplies measurement values.
    const std::vector<std::vector<std::uint16_t>> reference_values{
        {1000, 11000, 21000, 31000}, {1100, 11100, 21100, 31100},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {400, 401, 402, 403}, {410, 411, 412, 413}};
    for (unsigned c = 0; c < 3; ++c)
        for (unsigned z = 0; z < 2; ++z)
            write(path / "0" / (std::to_string(c) + "." + std::to_string(z) + ".0.0"),
                  samples(reference_values[c * 2 + z]));
    Json axes = Json::array();
    std::vector<std::uint64_t> shape, chunks;
    std::vector<double> scale, translation;
    if (layout.t) {
        axes.push_back(axis("t", "time")); shape.push_back(layout.t); chunks.push_back(1);
        scale.push_back(1); translation.push_back(layout.origin_t);
    }
    if (layout.z) {
        axes.push_back(axis("z", "space")); shape.push_back(layout.z); chunks.push_back(1);
        scale.push_back(1); translation.push_back(layout.origin_z);
    }
    axes.push_back(axis("y", "space", "micrometer"));
    axes.push_back(axis("x", "space", "micrometer"));
    shape.insert(shape.end(), {layout.height, layout.width});
    chunks.insert(chunks.end(), {2, 2});
    scale.insert(scale.end(), {.75, .5});
    translation.insert(translation.end(), {layout.origin_y * .75, layout.origin_x * .5});
    auto codes = values;
    codes.push_back(T(0));
    std::sort(codes.begin(), codes.end());
    codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
    Json properties = Json::array(), colors = Json::array();
    for (auto code : codes) {
        properties.push_back({{"label-value", code}, {"name", "code-" + std::to_string(code)},
            {"unknown", {{"nested", Json::array({"keep", true})},
                         {"exact-integer", std::uint64_t(9007199254740993ull)}}}});
        colors.push_back({{"label-value", code}, {"rgba", code == T(0) ?
            Json::array({0, 0, 0, 0}) : Json::array({1, 2, 254, 255})}});
    }
    Json image_label{{"version", "0.4"}, {"properties", properties}, {"colors", colors},
                     {"unknown-label-member", {{"original", "retain"}}}};
    if (!layout.default_reference)
        image_label["source"] = {{"image", "../../"}};
    write_json(path / "labels/cells/.zattrs", {{"multiscales", multiscales(axes, scale, translation)},
        {"image-label", image_label}, {"unknown-root-member", "retained without class inference"}});
    const auto dtype = std::string("<") + (std::is_signed_v<T> ? "i" : "u") + std::to_string(sizeof(T));
    write_json(path / "labels/cells/0/.zarray", array_spec(shape, chunks, dtype, layout.codec));
    const auto zn = std::max(1u, layout.z), tn = std::max(1u, layout.t);
    check(values.size() == std::size_t(layout.width) * layout.height * zn * tn,
          "Independent label sample count differs from authored dimensions");
    for (unsigned t = 0; t < tn; ++t)
        for (unsigned z = 0; z < zn; ++z)
            for (unsigned cy = 0; cy * 2 < layout.height; ++cy)
                for (unsigned cx = 0; cx * 2 < layout.width; ++cx) {
                    std::vector<T> tile(4, 0);
                    for (unsigned y = 0; y < 2; ++y)
                        for (unsigned x = 0; x < 2; ++x)
                            if (cy * 2 + y < layout.height && cx * 2 + x < layout.width)
                                tile[y * 2 + x] = values[((t * zn + z) * layout.height + cy * 2 + y) *
                                                         layout.width + cx * 2 + x];
                    std::string key;
                    if (layout.t) key += std::to_string(t) + '.';
                    if (layout.z) key += std::to_string(z) + '.';
                    key += std::to_string(cy) + '.' + std::to_string(cx);
                    write(path / "labels/cells/0" / key, compressed(samples(tile), layout.codec));
                }
    // Existing protected outputs are members before any inventory is captured.
    write(path / "owned-output.txt", "owner content must survive\n");
    return fs::absolute(path);
}
void inject_lexical_property(const fs::path& path) {
    const auto attributes = path / "labels/cells/.zattrs";
    auto attrs = json(attributes); // Before injection all generated values fit native JSON integers.
    constexpr const char* placeholder = "QPX_TEST_ORIGINAL_MAX_ID_PROPERTY_VALUE_7A4D21";
    unsigned replaced = 0;
    for (auto& property : attrs["image-label"]["properties"])
        if (property.at("label-value").get<std::uint64_t>() == UINT64_MAX) {
            property = placeholder;
            ++replaced;
        }
    check(replaced == 1, "Lexical fixture must replace one authored UInt64-max property");
    auto text = attrs.dump(2) + '\n';
    const auto marker = '"' + std::string(placeholder) + '"';
    const auto at = text.find(marker);
    check(at != std::string::npos && text.find(marker, at + marker.size()) == std::string::npos,
          "Lexical fixture placeholder is missing or not unique");
    // Insert raw JSON only into this newly authored store, before any capture.
    // No DOM serializer is allowed to reinterpret the three unknown numbers.
    text.replace(at, marker.size(), lexical_property_json);
    write(attributes, text);
    check(bytes(attributes).find(lexical_property_json) != std::string::npos,
          "Authored original property token bytes changed before import");
}
const std::string& lexical_property(const LabelMaskSourceDescription& description) {
    const auto found = std::find_if(description.label_properties.begin(), description.label_properties.end(),
        [](const auto& property) { return property.code == LabelCode::from_unsigned(UINT64_MAX); });
    check(found != description.label_properties.end(), "Imported source description lost UInt64-max property");
    return found->properties;
}
std::string published_property(const ObjectProperties& properties) {
    const auto& metadata = properties.metadata();
    constexpr const char* prefix = "qpx.ngff.properties/1";
    const auto size = std::stoull(metadata.at(std::string(prefix) + ".bytes"));
    check(size <= 24576, "Published original property text exceeded its native bound");
    std::string result;
    for (std::size_t piece = 0; result.size() < size; ++piece) {
        check(piece < 6, "Published original property chunks are missing or empty");
        result += metadata.at(std::string(prefix) + '.' + std::to_string(piece));
    }
    check(result.size() == size, "Published original property chunk byte count changed");
    return result;
}
NgffLabelMaskOptions options(const fs::path& path) {
    NgffLabelMaskOptions result;
    result.path = fs::absolute(path).u8string();
    result.label_group = group_name;
    result.author = "Independent native author";
    result.input_sha256 = std::string(64, 'a');
    result.max_regions = 16;
    return result;
}
LabelMaskTargetFrame target() {
    LabelMaskTargetFrame result;
    result.width = result.height = 2;
    result.crop = {0, 0, 0, 2, 2};
    result.pixel_width_um = .5;
    result.pixel_height_um = .75;
    return result;
}
void expect_codes(const LabelCodeMap& map, const std::vector<std::string>& expected) {
    check(map.size() == expected.size(), "Exact source-code map changed distinct label count");
    for (std::size_t i = 0; i < expected.size(); ++i)
        check(map.at(i).decimal() == expected[i], "Exact source-code map lost an original integer");
}
template <class T>
void profile(const fs::path& root, const char* name, const std::vector<T>& values,
              const std::vector<std::uint32_t>& dense, const std::vector<std::string>& codes,
              Layout layout = {}) {
    const auto path = store(root / name, values, layout);
    const auto imported = import_ngff_label_mask(options(path), target());
    check(imported.raster.labels() == dense && imported.source && imported.store &&
              imported.source->scope == "directory_store" &&
              imported.source->directory_sha256 == imported.store->observed_sha256() &&
              imported.reader_sha256.size() == 64,
          "Actual optional importer changed exact dense labels/store/decoder association");
    expect_codes(imported.source_codes, codes);
    check(imported.description->label_metadata == bytes(path / "labels/cells/.zattrs") &&
              imported.description->reference_metadata == bytes(path / ".zattrs") &&
              imported.description->default_reference == layout.default_reference &&
              imported.description->source_reference == "../../",
          "Actual provider dropped original attributes or default/explicit reference origin");
}
void raw_profiles(const fs::path& root) {
    profile<std::uint8_t>(root, "u1", {255, 0, 1, 255}, {2, 0, 1, 2}, {"1", "255"});
    profile<std::uint16_t>(root, "u2", {65535, 0, 1, 65535}, {2, 0, 1, 2}, {"1", "65535"});
    profile<std::uint32_t>(root, "u4", {UINT32_MAX, 0, 1, UINT32_MAX}, {2, 0, 1, 2}, {"1", "4294967295"});
    profile<std::uint64_t>(root, "u8", {UINT64_MAX, 9007199254740992ull, 9007199254740993ull, 0},
                           {3, 1, 2, 0}, {"9007199254740992", "9007199254740993", "18446744073709551615"});
    profile<std::int8_t>(root, "i1", {INT8_MIN, 0, INT8_MAX, INT8_MIN}, {1, 0, 2, 1}, {"-128", "127"});
    profile<std::int16_t>(root, "i2", {INT16_MIN, 0, INT16_MAX, INT16_MIN}, {1, 0, 2, 1}, {"-32768", "32767"});
    profile<std::int32_t>(root, "i4", {INT32_MIN, 0, INT32_MAX, INT32_MIN}, {1, 0, 2, 1},
                          {"-2147483648", "2147483647"});
    Layout defaulted; defaulted.default_reference = true;
    profile<std::int64_t>(root, "i8-default", {INT64_MIN, 0, 9007199254740993ll, INT64_MIN},
                          {1, 0, 2, 1}, {"-9223372036854775808", "9007199254740993"}, defaulted);
    profile<std::uint64_t>(root, "zero", {0, 0, 0, 0}, {0, 0, 0, 0}, {});
    for (const auto* codec : {"zlib", "gzip"}) {
        Layout encoded; encoded.codec = codec;
        profile<std::uint64_t>(root, codec, {UINT64_MAX, 0, 7, UINT64_MAX}, {2, 0, 1, 2},
                               {"7", "18446744073709551615"}, encoded);
    }
}
struct Capture {
    std::ostringstream out, err;
    std::streambuf *o = std::cout.rdbuf(out.rdbuf()), *e = std::cerr.rdbuf(err.rdbuf());
    ~Capture() { std::cout.rdbuf(o); std::cerr.rdbuf(e); }
};
Json cli(const std::vector<std::string>& args, int expected = 0) {
    std::atomic<bool> cancelled{false};
    Capture log;
    const auto status = detail::label_cli(args, cancelled);
    if (status != expected)
        throw std::runtime_error("Unexpected native NGFF CLI result: " + log.err.str());
    if (expected) return {};
    check(log.err.str().empty(), "Successful native NGFF CLI emitted an error");
    return Json::parse(log.out.str());
}
Value::Map& record(Value::Map& values, const std::string& id) {
    auto& records = std::get<Value::Map>(values.at("qpx.label-analysis/1").value);
    return std::get<Value::Map>(records.at(id).value);
}
std::string chunks_text(const Value& value) {
    std::string result;
    for (const auto& piece : std::get<Value::List>(value.value))
        result += std::get<std::string>(piece.value);
    return result;
}
Value text_chunks(const std::string& text) {
    Value::List result;
    for (std::size_t at = 0; at < text.size(); at += 4096)
        result.push_back(Value{text.substr(at, 4096)}); // Authored fixture is ASCII.
    return Value{std::move(result)};
}
void saved_tampering(Document& doc, const std::string& id, const std::string& producer_id) {
    for (const auto* field : {"reader_sha256", "missing-reader", "label_z", "reference_z", "reference_group", "model", "author", "input", "level"}) {
        auto values = doc.image_data_properties()->values();
        auto& text = std::get<std::string>(record(values, id).at("parameters").value);
        auto recipe = Json::parse(text);
        const std::string key = field;
        if (key == "reader_sha256") recipe[key] = std::string(64, '0');
        if (key == "missing-reader") recipe.erase("reader_sha256");
        if (key == "label_z") recipe[key] = 1;
        if (key == "reference_z") recipe[key] = 0;
        if (key == "reference_group") recipe[key] = "unrelated";
        if (key == "model") recipe[key] = std::string(64, 'b');
        if (key == "author") recipe[key] = "different exact author";
        if (key == "input") recipe[key] = std::string(64, '0');
        if (key == "level") recipe[key] = 1;
        text = recipe.dump();
        doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));
        refuses([&] { run_label_analysis(replay_label_analysis(doc, id)); });
        doc.undo();
    }
    for (const auto* field : {"description", "codes", "observed", "content"}) {
        auto values = doc.image_data_properties()->values();
        auto& source = std::get<Value::Map>(record(values, id).at("ngff_source").value);
        const std::string key = field;
        if (key == "description") {
            auto descriptor = Json::parse(chunks_text(source.at(key)));
            descriptor["label_metadata"] = descriptor.at("label_metadata").get<std::string>() + " ";
            source[key] = text_chunks(descriptor.dump());
        } else if (key == "codes")
            source[key] = text_chunks("2\n18446744073709551615\n");
        else
            source[key] = Value{std::string(64, '0')};
        doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));
        refuses([&] { run_label_analysis(replay_label_analysis(doc, id)); });
        doc.undo();
    }
    auto values = doc.image_data_properties()->values();
    auto& text = std::get<std::string>(record(values, producer_id).at("parameters").value);
    auto recipe = Json::parse(text);
    recipe["model"] = std::string(64, 'c'); // Valid different model, not a malformed SHA.
    text = recipe.dump();
    doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));
    refuses([&] { run_label_analysis(replay_label_analysis(doc, producer_id)); });
    doc.undo();
    doc.set_calibration(.6, .75);
    refuses([&] { run_label_analysis(replay_label_analysis(doc, id)); });
    doc.undo();
}
void full_workflow(const fs::path& root, const fs::path& source) {
    const auto mask = store<std::uint64_t>(root / "workflow.zarr", {UINT64_MAX, 0, 1, UINT64_MAX});
    inject_lexical_property(mask);
    Project project;
    TiffOpenOptions opening; opening.view.z = 1;
    const auto image = project.add_tiff_image(source.u8string(), opening);
    auto doc = project.document(image);
    const auto area = doc->add_rectangle({0, 0, 2, 2}, "Native reference plane one");
    RawRegionRequest channels; channels.channels = 3; channels.z = 1;
    auto selected = options(mask); selected.reference_z = 1;
    selected.input_sha256 = label_analysis_raw_input_sha256(*doc, area, channels);
    const auto input = root / "input.qpx";
    project.save_new(input.u8string());
    const auto original_project = bytes(input), original_source = bytes(source);
    const auto request = LabelAnalysisRequest::ngff_mask(*doc, area, channels, selected);
    auto wrong_input = selected; wrong_input.input_sha256.assign(64, '0');
    refuses<WorkflowInputChanged>([&] {
        run_label_analysis(LabelAnalysisRequest::ngff_mask(*doc, area, channels, wrong_input));
    });
    const auto run = run_label_analysis(request);
    const auto captured = std::get<std::shared_ptr<const NgffLabelMaskOptions>>(run->options());
    check(run->method() == LabelAnalysisMethod::NgffMask && run->exact_source_labels() &&
              run->raster().labels() == std::vector<std::uint32_t>{2, 0, 1, 2} &&
              captured->reader_sha256.size() == 64 && captured->model_sha256.empty() &&
              std::string(run->measurement_domain()) == "original-raw-channel-samples",
          "Native authored mask lost its exact typed source/identity/domain");
    expect_codes(*run->exact_source_labels(), {"1", "18446744073709551615"});
    check(lexical_property(*run->label_source_description()) == lexical_property_json &&
              run->label_source_description()->label_metadata == bytes(mask / "labels/cells/.zattrs"),
          "Imported description rounded or reserialized original property number tokens");
    check(run->source_code(1).magnitude() == UINT64_MAX && run->measurements().size() == 2,
          "UInt64 source code crossed a floating/narrow result slot");
    const auto& low = run->measurements()[0]; const auto& high = run->measurements()[1];
    check(low.pixels == 1 && high.pixels == 2 && low.valid_pixels == 1 && high.valid_pixels == 2,
          "Repeated/disconnected codes changed source instance counts");
    near(*low.channels[0].mean, 21100); near(*low.channels[2].mean, 412);
    near(*high.channels[0].sum, 32200); near(*high.channels[0].mean, 16100);
    near(*high.channels[1].sum, 0); near(*high.channels[2].sum, 823);
    near(*high.channels[2].mean, 411.5);
    check(run->contours()->components().size() == 2, "Native contours split or dropped source instances");
    for (const auto& contour : run->contours()->components()) {
        if (contour.label == 1)
            check(contour.geometry.area() == 1 && contour.geometry.contains({.5, 1.5}) &&
                      !contour.geometry.contains({1.5, .5}), "Low-code contour shifted from source pixels");
        else
            check(contour.label == 2 && contour.geometry.area() == 2 &&
                      contour.geometry.contains({.5, .5}) && contour.geometry.contains({1.5, 1.5}) &&
                      !contour.geometry.contains({1.5, .5}), "Disconnected high-code contour is misregistered");
    }
    const auto id = keep_label_analysis(*doc, request, *run);
    doc->undo(); check(label_analysis_ids(*doc).empty(), "NGFF keep was not one undo");
    doc->redo(); check(label_analysis_ids(*doc) == std::vector<std::string>{id}, "NGFF keep redo changed identity");
    const auto before_count = doc->object_count();
    const auto published = publish_label_analysis(*doc, request, *run, "Authored label");
    check(published.size() == 2 && doc->object_count() == before_count + 2,
          "Native NGFF publication was partial");
    for (const auto object : published) {
        const auto annotation = doc->annotation(object);
        const auto& metadata = annotation.properties->metadata();
        check(annotation.kind == ObjectKind::Detection && annotation.class_name.empty() &&
                  annotation.plane == ObjectPlane(-1, 1, 0) &&
                  metadata.at("qpx.ngff.author/1") == selected.author &&
                  metadata.at("qpx.ngff.origin/1") == "authored" &&
                  !metadata.count("qpx.external-mask.model/1") &&
                  metadata.at("qpx.ngff.rgba/1") == "[1,2,254,255]",
              "NGFF publication inferred classes/model or dropped plane/authorship/color metadata");
        const auto code = metadata.at("qpx.external-mask.source-label/1");
        check(code == "1" || code == "18446744073709551615", "Published ID was rounded/narrowed");
        const auto original_property = published_property(*annotation.properties);
        if (code == "18446744073709551615") {
            check(original_property == lexical_property_json,
                  "Published property chunks rounded opaque integer/decimal tokens");
        } else {
            const auto property = Json::parse(original_property);
            check(property.at("name") == "code-" + code &&
                      property.at("unknown").at("exact-integer").get<std::uint64_t>() == 9007199254740993ull,
                  "Unknown original label properties were discarded or rounded");
        }
    }
    doc->undo(); check(doc->object_count() == before_count, "NGFF publication was not one undo");
    doc->redo(); check(doc->object_count() == before_count + 2, "NGFF publication redo lost objects");
    auto producer = selected; producer.origin = LabelMaskOrigin::Producer; producer.author.clear();
    producer.producer = "Native fixture producer"; producer.producer_version = "1";
    producer.model_sha256 = std::string(64, 'b');
    const auto producer_request = LabelAnalysisRequest::ngff_mask(*doc, area, channels, producer);
    const auto producer_run = run_label_analysis(producer_request);
    check(producer_run->sha256() != id, "Authored/producer/model identities were conflated");
    const auto producer_id = keep_label_analysis(*doc, producer_request, *producer_run);
    const auto saved = root / "saved.qpx";
    project.save_new(saved.u8string());
    {
        const auto batch_project = Project::open(saved.u8string());
        const StainTransform stains({{1,0,0},{0,1,0}});
        const auto unrelated = std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,40)));
        unrelated->add_rectangle({0,0,2,2});
        const auto other = batch_project->add_document(unrelated);
        const auto before = tree_entries(mask);
        for (const auto target : {image,other}) {
            const auto destination = mask / ("protected-batch-" + std::to_string(target));
            const auto result = run_project_batch(ProjectBatchRequest(*batch_project,stains,{{target,{}}}),destination.u8string());
            check(result.state=="failed" && result.report_path.empty() && result.images.empty() &&
                  result.error.find("outside the protected input directory")!=std::string::npos &&
                  !fs::exists(destination) && tree_entries(mask)==before,
                  "Batch touched an attached NGFF mask store");
        }
    }
    auto restored = Project::open_without_journal_recovery(saved.u8string())->document(image);
    const auto replay = run_label_analysis(replay_label_analysis(*restored, id));
    check(replay->sha256() == id && replay->label_source_description()->json ==
              run->label_source_description()->json &&
              label_code_map_text(*replay->exact_source_labels()) == "1\n18446744073709551615\n",
          "Project save/reopen/replay lost exact NGFF source metadata/code identity");
    check(lexical_property(*replay->label_source_description()) == lexical_property_json,
          "Replayed source description changed original property token bytes");
    for (const auto object : published) {
        const auto annotation = restored->annotation(object);
        if (annotation.properties->metadata().at("qpx.external-mask.source-label/1") == "18446744073709551615")
            check(published_property(*annotation.properties) == lexical_property_json,
                  "Saved/reopened object properties changed original numeric token bytes");
    }
    const auto csv = root / "native.csv", bundle = root / "native.qplm";
    save_label_analysis_csv(*replay, csv.u8string()); save_label_analysis_raster(*replay, bundle.u8string());
    const auto archived = open_label_mask_archive(bundle.u8string());
    check(archived.raster.labels() == run->raster().labels() &&
              archived.description->json == run->label_source_description()->json &&
              archived.context.record_sha256 == id && archived.context.input_sha256 == run->input_sha256() &&
              archived.context.reader_sha256 == captured->reader_sha256 &&
              archived.context.target.width == 2 && archived.context.target.height == 2 &&
              archived.context.target.crop.x == 0 && archived.context.target.crop.y == 0 &&
              archived.context.target.crop.width == 2 && archived.context.target.crop.height == 2 &&
              archived.context.target.pixel_width_um == .5 && archived.context.target.pixel_height_um == .75 &&
              !archived.context.target.z_spacing_um && !archived.context.target.time_interval_seconds &&
              archived.context.target.z == 1 && archived.context.target.t == 0 &&
              archived.context.plane.label_z == 0 && archived.context.plane.label_t == 0 &&
              archived.context.plane.reference_z == 1 && archived.context.plane.reference_t == 0,
          "QPLM1 export lost source pixels, exact metadata/codes or declared target/plane");
    expect_codes(archived.source_codes, {"1", "18446744073709551615"});
    check(lexical_property(*archived.description) == lexical_property_json,
          "QPLM1 decode changed original property numeric token bytes");
    check(bytes(csv).find(",2,18446744073709551615,") != std::string::npos,
          "NGFF CSV did not export the exact UInt64 source ID");
    const auto csv_property = ',' + std::string(lexical_property_csv) + ",\"[1,2,254,255]\"\n";
    const auto csv_text = bytes(csv);
    std::size_t property_rows = 0;
    for (std::size_t at = 0; (at = csv_text.find(csv_property, at)) != std::string::npos;
         at += csv_property.size())
        ++property_rows;
    check(property_rows == 3,
          "CSV changed original property number tokens or their exact quoted field bytes");
    saved_tampering(*restored, id, producer_id);
    const auto output = root / "cli.qpx";
    const std::vector<std::string> args{"batch", "--import-ngff-label-mask", input.u8string(), output.u8string(),
        "--image", "1", "--roi", std::to_string(area), "--mask-directory", mask.u8string(),
        "--label-group", group_name, "--reference-group", ".", "--origin", "authored", "--author",
        selected.author, "--input-sha256", selected.input_sha256, "--reference-z", "1", "--z", "1",
        "--channels", "3", "--max-regions", "16"};
    const auto report = cli(args);
    check(report.at("record") == id && report.at("published_objects") == 2 && report.at("regions") == 2,
          "Native and CLI NGFF captures/results differ");
    check(cli({"batch", "--verify-labels", output.u8string(), "--image", "1", "--record", id}).at("record") == id,
          "CLI NGFF verify did not replay exact saved source identity");
    const auto cli_csv = root / "cli.csv", cli_bundle = root / "cli.qplm";
    cli({"batch", "--export-labels", output.u8string(), cli_csv.u8string(), "--image", "1", "--record", id});
    cli({"batch", "--export-labels", output.u8string(), cli_bundle.u8string(), "--image", "1", "--record", id,
         "--format", "raster"});
    check(bytes(cli_csv) == bytes(csv) && bytes(cli_bundle) == bytes(bundle),
          "Native/CLI CSV or QPLM1 bytes differ for the same captured workflow");
    const auto csv_before = bytes(csv), bundle_before = bytes(bundle);
    refuses<WorkflowOutputExists>([&] { save_label_analysis_csv(*run, csv.u8string()); });
    refuses<WorkflowOutputExists>([&] { save_label_analysis_raster(*run, bundle.u8string()); });
    const auto protected_existing = mask / "owned-output.txt";
    const auto owner_bytes = bytes(protected_existing);
    const auto protected_tree = tree_entries(mask);
    for (const auto& destination : {protected_existing, mask / "new-output.csv", mask / "labels/cells/new-output.qplm"}) {
        refuses([&] { project.save(destination.u8string()); });
        refuses([&] { project.save_new(destination.u8string()); });
        refuses([&] { save_label_analysis_csv(*run, destination.u8string()); });
        refuses([&] { save_label_analysis_raster(*run, destination.u8string()); });
        auto bad = args; bad[3] = destination.u8string(); cli(bad, 1);
        cli({"batch", "--export-labels", output.u8string(), destination.u8string(), "--image", "1", "--record", id}, 1);
        cli({"batch", "--export-labels", output.u8string(), destination.u8string(), "--image", "1", "--record", id,
             "--format", "raster"}, 1);
        if (destination != protected_existing)
            check(!fs::exists(destination), "Protected whole-store output refusal left an artifact");
    }
    check(bytes(protected_existing) == owner_bytes && bytes(input) == original_project && bytes(source) == original_source,
          "Protected NGFF output attempt changed owner/source/project bytes");
    check(tree_entries(mask) == protected_tree,
          "Native project/CSV/bundle store protection left an output/staging directory entry");
    require_current_label_analysis_inputs(*run);
    check(bytes(csv) == csv_before && bytes(bundle) == bundle_before,
          "A refusal changed an existing accepted CSV/bundle");
    const auto project_before = bytes(output);
    cli(args, 1); // Existing result project must not be overwritten.
    check(bytes(output) == project_before, "Existing CLI result project was overwritten");
    std::cout << "Saved project: " << saved.u8string() << '\n';
}
void selected_planes_and_crop(const fs::path& root, const fs::path& source) {
    Layout planes; planes.z = planes.t = 2; planes.origin_t = -1;
    const auto plane_store = store<std::uint64_t>(root / "selected-planes.zarr",
        {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, UINT64_MAX, 0, 7, UINT64_MAX}, planes);
    auto frame = target(); frame.z = 1;
    auto selection = options(plane_store); selection.z = 1; selection.t = 1; selection.reference_z = 1;
    const auto imported = import_ngff_label_mask(selection, frame);
    check(imported.raster.labels() == std::vector<std::uint32_t>{2, 0, 1, 2} &&
              imported.description->z == 1 && imported.description->t == 1,
          "Explicit source Z/T selection read another plane");
    expect_codes(imported.source_codes, {"7", "18446744073709551615"});
    Layout shifted; shifted.width = shifted.height = 3; shifted.origin_x = shifted.origin_y = -1;
    shifted.codec = "gzip";
    const auto shifted_store = store<std::uint16_t>(root / "shifted-edge.zarr",
        {99, 99, 99, 99, 7, 0, 99, 7, 7}, shifted);
    const auto crop = import_ngff_label_mask(options(shifted_store), target());
    check(crop.crop.source_crop.x == 1 && crop.crop.source_crop.y == 1 &&
              crop.crop.label_origin_x_in_reference == -1 && crop.crop.label_origin_y_in_reference == -1 &&
              crop.raster.labels() == std::vector<std::uint32_t>{1, 0, 1, 1},
          "Shifted chunk-edge crop was resized, padded or misregistered");
    Project project; TiffOpenOptions opening; opening.view.z = 1;
    auto doc = project.document(project.add_tiff_image(source.u8string(), opening));
    const auto area = doc->add_rectangle({1, 0, 1, 2}, "Nonzero X native ROI");
    RawRegionRequest channels; channels.channels = 3; channels.z = 1;
    auto o = options(shifted_store); o.reference_z = 1;
    o.input_sha256 = label_analysis_raw_input_sha256(*doc, area, channels);
    const auto run = run_label_analysis(LabelAnalysisRequest::ngff_mask(*doc, area, channels, o));
    check(run->raster().width() == 1 && run->raster().height() == 2 &&
              run->raster().labels() == std::vector<std::uint32_t>{0, 1} &&
              run->contours()->components().size() == 1 &&
              run->contours()->components()[0].geometry.bounds().x == 1 &&
              run->contours()->components()[0].geometry.bounds().y == 1 &&
              run->contours()->components()[0].geometry.area() == 1,
          "Nonzero ROI crop/contour was placed in mask-local rather than target coordinates");
    near(*run->measurements()[0].channels[0].mean, 31100);
    near(*run->measurements()[0].channels[2].mean, 413);
    auto zero = options(root / "zero"); zero.reference_z = 1;
    zero.input_sha256 = o.input_sha256;
    const auto zero_request = LabelAnalysisRequest::ngff_mask(*doc, area, channels, zero);
    const auto zero_run = run_label_analysis(zero_request);
    check(zero_run->raster().foreground_pixels() == 0 && zero_run->measurements().empty() &&
              zero_run->contours()->components().empty(), "Zero-only mask created foreground/results");
    keep_label_analysis(*doc, zero_request, *zero_run);
    check(publish_label_analysis(*doc, zero_request, *zero_run).empty(), "Zero-only publication invented objects");
}
fs::path copy_store(const fs::path& original, const fs::path& destination) {
    check(!fs::exists(destination), "Mutation fixture destination already exists");
    fs::copy(original, destination, fs::copy_options::recursive);
    return fs::absolute(destination);
}
void fill_value_literal(const fs::path& path, const std::string& token) {
    const auto array = path / "labels/cells/0/.zarray";
    auto spec = bytes(array);
    const std::string marker = "\"fill_value\": 0";
    const auto at = spec.find(marker);
    check(at != std::string::npos && spec.find(marker, at + marker.size()) == std::string::npos,
          "Authored fill-value fixture token is missing or repeated");
    spec.replace(at, marker.size(), "\"fill_value\": " + token);
    write(array, spec); // Never interpret invalid semantic fill tokens as binary64.
}
void strict_integral_fill_values(const fs::path& root) {
    struct Case { const char *name, *source, *token; };
    for (const auto& test : {Case{"int64-overflow", "i8-default", "-9223372036854775809"},
                              Case{"rounded-decimal", "u8", "1.00000000000000000001"},
                              Case{"floating-zero", "u8", "0.0"}}) {
        const auto path = copy_store(root / test.source,
                                      root / (std::string("invalid-fill-") + test.name + ".zarr"));
        fill_value_literal(path, test.token);
        refuses([&] { import_ngff_label_mask(options(path), target()); });
    }
    const auto path = copy_store(root / "u8", root / "null-fill.zarr");
    fill_value_literal(path, "null");
    const auto imported = import_ngff_label_mask(options(path), target());
    check(imported.raster.labels() == std::vector<std::uint32_t>{3, 1, 2, 0},
          "Null fill changed present integral chunk samples");
    expect_codes(imported.source_codes,
                 {"9007199254740992", "9007199254740993", "18446744073709551615"});
    check(fs::remove(path / "labels/cells/0/0.0"), "Cannot remove owned null-fill chunk");
    refuses([&] { import_ngff_label_mask(options(path), target()); });
}
void currentness_and_refusals(const fs::path& root, const fs::path& source) {
    const auto original = root / "workflow.zarr";
    for (const auto* mode : {"add", "delete", "change"}) {
        const auto path = copy_store(original, root / (std::string("mutation-") + mode + ".zarr"));
        Project project; TiffOpenOptions opening; opening.view.z = 1;
        auto doc = project.document(project.add_tiff_image(source.u8string(), opening));
        const auto area = doc->add_rectangle({0, 0, 2, 2}, "Currentness");
        RawRegionRequest raw; raw.channels = 3; raw.z = 1;
        auto o = options(path); o.reference_z = 1;
        o.input_sha256 = label_analysis_raw_input_sha256(*doc, area, raw);
        const auto request = LabelAnalysisRequest::ngff_mask(*doc, area, raw, o);
        const auto run = run_label_analysis(request);
        keep_label_analysis(*doc, request, *run);
        const auto objects = doc->object_count();
        const auto unread = path / "0/2.0.0.0"; // Non-primary reference chunk, outside label decoding.
        if (std::string(mode) == "add") write(path / "unread-extra.chunk", "authored extra");
        if (std::string(mode) == "delete") check(fs::remove(unread), "Cannot remove owned mutation chunk");
        if (std::string(mode) == "change") write(unread, bytes(unread) + 'x');
        refuses<WorkflowInputChanged>([&] { keep_label_analysis(*doc, request, *run); });
        refuses<WorkflowInputChanged>([&] { publish_label_analysis(*doc, request, *run); });
        const auto csv = root / (std::string("stale-") + mode + ".csv");
        const auto bundle = root / (std::string("stale-") + mode + ".qplm");
        refuses<WorkflowInputChanged>([&] { save_label_analysis_csv(*run, csv.u8string()); });
        refuses<WorkflowInputChanged>([&] { save_label_analysis_raster(*run, bundle.u8string()); });
        check(!fs::exists(csv) && !fs::exists(bundle) && doc->object_count() == objects,
              "Stale whole-store input partially published/exported");
    }
    for (const auto* mode : {"reference", "calibration", "half-pixel", "float", "metadata"}) {
        const auto path = copy_store(original, root / (std::string("invalid-") + mode + ".zarr"));
        const std::string name = mode;
        if (name == "reference" || name == "half-pixel" || name == "metadata") {
            auto attrs = bytes(path / "labels/cells/.zattrs");
            if (name == "half-pixel") {
                const auto transform = attrs.find("\"translation\": [");
                const auto comma = attrs.find(',', transform);
                const auto end = attrs.find(']', comma);
                check(transform != std::string::npos && comma != std::string::npos && end != std::string::npos,
                      "Authored translation fixture token was not found");
                attrs.replace(comma + 1, end - comma - 1, "0.25");
            } else {
                const std::string from = name == "reference" ? "\"image\": \"../../\"" : "\"version\": \"0.4\"";
                const std::string to = name == "reference" ? "\"image\": \"../../../outside\"" : "\"version\": \"0.5\"";
                const auto at = attrs.find(from);
                check(at != std::string::npos, "Authored invalid-metadata fixture token was not found");
                attrs.replace(at, from.size(), to);
            }
            check(attrs.find(lexical_property_json) != std::string::npos,
                  "Unrelated fixture edits changed opaque property tokens");
            write(path / "labels/cells/.zattrs", attrs);
        } else if (name == "calibration") {
            auto attrs = json(path / ".zattrs");
            attrs["multiscales"][0]["datasets"][0]["coordinateTransformations"][0]["scale"][3] = .6;
            write_json(path / ".zattrs", attrs);
        } else {
            auto spec = json(path / "labels/cells/0/.zarray"); spec["dtype"] = "<f8";
            write_json(path / "labels/cells/0/.zarray", spec);
        }
        refuses([&] { import_ngff_label_mask(options(path), target()); });
    }
    auto o = options(original); o.level = 1;
    refuses<std::invalid_argument>([&] { import_ngff_label_mask(o, target()); });
    o = options(original); o.reader_sha256.assign(64, '0');
    refuses<WorkflowInputChanged>([&] { import_ngff_label_mask(o, target()); });
    o = options(original); o.origin = LabelMaskOrigin::Producer; o.author.clear();
    o.producer = "producer"; o.producer_version = "1"; // Model is explicit and required.
    refuses<std::invalid_argument>([&] { o.validate(); });
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>([&] { import_ngff_label_mask(options(original), target(), 256u * 1024 * 1024, &cancelled); });
}
void unavailable(const fs::path& saved) {
    check(reader_catalog_json().find("org.qupathxx.ngff-labels") == std::string::npos,
          "Unavailable route unexpectedly loaded an optional NGFF provider");
    auto project = Project::open_without_journal_recovery(fs::absolute(saved).u8string());
    auto doc = project->document(1);
    const auto ids = label_analysis_ids(*doc);
    check(!ids.empty(), "Provider-free project open lost saved NGFF metadata");
    bool original_id = false;
    for (const auto& annotation : doc->annotations())
        if (annotation.properties) {
            const auto& m = annotation.properties->metadata();
            const auto at = m.find("qpx.external-mask.source-label/1");
            original_id = original_id || (at != m.end() && at->second == "18446744073709551615");
        }
    check(original_id, "Provider-free project open lost published exact UInt64 metadata");
    for (const auto& id : ids) {
        auto values = doc->image_data_properties()->values();
        const auto& source = std::get<Value::Map>(record(values, id).at("ngff_source").value);
        check(!chunks_text(source.at("description")).empty() &&
                  chunks_text(source.at("codes")) == "1\n18446744073709551615\n",
              "Provider-free saved source description/map is unreadable");
        check(lexical_property(decode_label_mask_description(chunks_text(source.at("description")))) ==
                  lexical_property_json,
              "Provider-free saved description changed original property token bytes");
        check(refuses([&] { run_label_analysis(replay_label_analysis(*doc, id)); }).find("configured org.qupathxx.ngff-labels") != std::string::npos,
              "Unavailable decoder did not explicitly refuse saved NGFF replay");
    }
    const auto outside_new = fs::absolute(saved).parent_path() / "provider-free-new.qpx";
    const auto outside_save = fs::absolute(saved).parent_path() / "provider-free-save.qpx";
    check(!fs::exists(outside_new) && !fs::exists(outside_save),
          "Provider-free save control requires fresh output paths");
    const auto original = bytes(saved);
    project->save_new(outside_new.u8string());
    project->save(outside_save.u8string());
    for (const auto& path : {outside_new, outside_save})
        check(label_analysis_ids(*Project::open_without_journal_recovery(path.u8string())->document(1)) == ids,
              "Provider-free native project save lost readable NGFF records");
    check(bytes(saved) == original, "Provider-free metadata/save control overwrote its input project");
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (!has_geos() || !has_tiff() || !has_project()) {
            std::cout << "NGFF workflow integration requires native GEOS/TIFF/project capabilities\n";
            return 77;
        }
        if (argc == 3 && std::string(argv[1]) == "unavailable") {
            unavailable(argv[2]);
            std::cout << "Provider-free saved NGFF metadata open and explicit replay refusal pass\n";
            return 0;
        }
        check(argc == 4 && std::string(argv[1]) == "validate", "Expected validate MANIFEST NEW_WORK or unavailable SAVED_PROJECT");
        configure_reader_plugins(fs::absolute(argv[2]).u8string());
        const auto root = fs::absolute(argv[3]).lexically_normal();
        check(!fs::exists(root) && fs::create_directories(root), "NGFF validation requires a fresh owned work directory");
        const auto source = root / "source.ome.tif";
        qpx_test::fluorescence_fixture(source, false);
        std::cout << "NGFF stage: all exact integral source profiles and codecs\n";
        raw_profiles(root);
        strict_integral_fill_values(root);
        std::cout << "NGFF stage: native keep/publish/save/replay/CLI/QPLM1\n";
        full_workflow(root, source);
        std::cout << "NGFF stage: selected planes, shifted chunk-edge/nonzero ROI crops\n";
        selected_planes_and_crop(root, source);
        std::cout << "NGFF stage: whole-store currentness and input refusals\n";
        currentness_and_refusals(root, source);
        std::cout << "Actual optional NGFF label workflow integration tests pass\nFixtures: " << root.u8string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
