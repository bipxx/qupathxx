#include "label_cli.hpp"
#include "reader_plugin.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/label_workflow.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_fixture.hpp"
#include "nucls_png_fixture.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace qpx;
namespace fs = std::filesystem;
namespace {
using Json = nlohmann::json;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class E = std::exception, class F> std::string refuses(F action) {
    try {
        action();
    } catch (const E& e) {
        return e.what();
    }
    throw std::runtime_error("Expected native PNG-label refusal");
}
void write(const fs::path& path, const std::string& value) {
    std::ofstream f(path, std::ios::binary);
    f.write(value.data(), std::streamsize(value.size()));
    f.close();
    check(bool(f), "Cannot write independent PNG-label fixture");
}
std::string bytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    check(bool(f), "Cannot read PNG-label fixture/output");
    return {std::istreambuf_iterator<char>(f), {}};
}
// Literal stored Deflate rows and PNG chunks use the existing independent test
// writer, never the production reader, an encoder library or downloaded images.
std::string png_labels(const std::vector<std::uint32_t>& labels, unsigned depth, bool indexed = false) {
    check(labels.size() == 4, "PNG-label fixture must have four samples");
    std::string raw;
    for (unsigned y = 0; y < 2; ++y) {
        raw += char(0);
        if (depth < 8) {
            unsigned packed = 0;
            for (unsigned x = 0; x < 2; ++x)
                packed |= labels[2 * y + x] << (8 - (x + 1) * depth);
            raw += char(packed);
        } else
            for (unsigned x = 0; x < 2; ++x) {
                const auto value = labels[2 * y + x];
                if (depth == 16)
                    raw += char(value >> 8);
                raw += char(value);
            }
    }
    auto png = nucls_png_fixture::png(2, 2, raw, indexed ? 3 : 0, depth);
    if (indexed) {
        const auto count = 1u << depth;
        const std::string palette(3 * count, char(91)); // All palette colors are identical.
        std::string alpha(count, char(255));
        alpha.back() = 0; // The largest positive ID must survive complete transparency.
        png.insert(33, nucls_png_fixture::chunk("PLTE", palette) + nucls_png_fixture::chunk("tRNS", alpha));
    }
    return png;
}
ExternalLabelMaskOptions options(const fs::path& mask) {
    ExternalLabelMaskOptions result;
    result.path = fs::absolute(mask).u8string();
    result.producer = "independent-png-label-control";
    result.producer_version = "1";
    result.model_sha256 = std::string(64, 'a');
    result.input_sha256 = std::string(64, 'b');
    result.format = ExternalLabelMaskFormat::Png;
    result.max_regions = 4;
    return result;
}
void missing(const fs::path& root) {
    const auto path = root / "missing-plugin.png";
    write(path, png_labels({7, 255, 7, 0}, 8));
    const auto p = options(path);
    check(refuses<std::invalid_argument>([&] {
              import_label_mask(p, 2, 2);
          }).find("configured org.qupathxx.png") != std::string::npos,
          "Missing PNG plugin did not produce its native diagnostic");
    refuses<std::invalid_argument>([&] { import_label_mask_tiff(p, 2, 2); });
}
void raw_profiles(const fs::path& root) {
    for (const unsigned depth : {1, 2, 4, 8, 16}) {
        const auto maximum = (1u << depth) - 1;
        const std::vector<std::uint32_t> labels{maximum, depth == 1 ? 0u : 1u, maximum, 0};
        const auto path = root / ("gray-" + std::to_string(depth) + ".png");
        write(path, png_labels(labels, depth));
        const auto result = import_label_mask(options(path), 2, 2, 8u * 1024 * 1024);
        check(result.source_labels.back() == maximum &&
                  result.raster.labels() == (depth == 1 ? std::vector<std::uint32_t>{1, 0, 1, 0}
                                                        : std::vector<std::uint32_t>{2, 1, 2, 0}),
              "PNG stored grayscale codes were scaled, rounded or reordered");
        check(result.source && result.source->reader.find("org.qupathxx.png:plugin/1:") == 0,
              "PNG mask lost its exact reader binding");
        auto palette = options(path);
        palette.format = ExternalLabelMaskFormat::PngIndexed;
        refuses<std::invalid_argument>([&] { import_label_mask(palette, 2, 2); });
        if (depth < 16) {
            const auto indexed = root / ("indexed-" + std::to_string(depth) + ".png");
            write(indexed, png_labels(labels, depth, true));
            palette.path = fs::absolute(indexed).u8string();
            const auto raw = import_label_mask(palette, 2, 2);
            check(raw.source_labels == result.source_labels && raw.raster.labels() == result.raster.labels(),
                  "Indexed PNG labels changed during palette or alpha handling");
            refuses<std::invalid_argument>([&] { import_label_mask(options(indexed), 2, 2); });
        }
    }
    const auto path = root / "gray-16.png";
    auto p = options(path);
    refuses<std::invalid_argument>([&] { import_label_mask(p, 3, 2); });
    refuses<WorkflowCaptureLimit>([&] { import_label_mask(p, 2, 2, 65536); });
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>([&] { import_label_mask(p, 2, 2, 8u * 1024 * 1024, &cancelled); });
    p.reader_sha256 = std::string(64, '0');
    refuses<WorkflowInputChanged>([&] { import_label_mask(p, 2, 2); });
    p = options(path);
    p.format = ExternalLabelMaskFormat(77);
    refuses<std::invalid_argument>([&] { p.validate(); });
    for (unsigned color : {2, 4, 6}) {
        const unsigned channels = color == 2 ? 3 : color == 4 ? 2 : 4;
        std::string raw;
        for (unsigned y = 0; y < 2; ++y) {
            raw += char(0);
            raw += std::string(2 * channels, char(7));
        }
        const auto other = root / ("color-" + std::to_string(color) + ".png");
        write(other, nucls_png_fixture::png(2, 2, raw, color));
        refuses<std::invalid_argument>([&] { import_label_mask(options(other), 2, 2); });
    }
    const auto broken = root / "corrupt.png";
    auto corrupt = png_labels({7, 255, 7, 0}, 8);
    corrupt[29] ^= 1; // IHDR CRC, not the independently supplied expected values.
    write(broken, corrupt);
    refuses([&] { import_label_mask(options(broken), 2, 2); });

    const auto& catalog = detail::reader_plugin_catalog();
    const auto found =
        std::find_if(catalog.readers.begin(), catalog.readers.end(),
                     [](const auto& v) { return v->info.provider_id == "org.qupathxx.png"; });
    check(found != catalog.readers.end(), "Configured manifest did not admit the PNG provider");
    const auto plugin = *found;
    refuses<std::invalid_argument>(
        [&] { detail::open_bounded_plugin_image_server(path.u8string(), plugin, 0); });
    refuses<std::invalid_argument>([&] {
        detail::open_bounded_plugin_image_server(path.u8string(), plugin, 256u * 1024 * 1024 + 1);
    });
    check(refuses<std::invalid_argument>([&] {
              detail::open_bounded_plugin_image_server(path.u8string(), plugin, 1);
          }).find("encoded input exceeds reader work budget") != std::string::npos,
          "Composed plugin allowance did not reach the actual decoder");
    const auto bounded =
        detail::open_bounded_plugin_image_server(path.u8string(), plugin, 1024 * 1024);
    check(std::get<std::vector<std::uint16_t>>(bounded->read_raw({{0, 0, 0, 2, 2}}).storage()) ==
              std::vector<std::uint16_t>{65535, 1, 65535, 0},
          "Bounded native plugin route changed original sample codes");
}
struct Capture {
    std::ostringstream out, err;
    std::streambuf *o = std::cout.rdbuf(out.rdbuf()), *e = std::cerr.rdbuf(err.rdbuf());
    ~Capture() {
        std::cout.rdbuf(o);
        std::cerr.rdbuf(e);
    }
};
Json cli(const std::vector<std::string>& args, int expected = 0) {
    std::atomic<bool> cancelled{false};
    Capture log;
    const auto result = detail::label_cli(args, cancelled);
    if (result != expected)
        throw std::runtime_error("Unexpected PNG label CLI result: " + log.err.str());
    if (expected)
        return {};
    check(log.err.str().empty(), "Successful PNG label CLI emitted an error");
    return Json::parse(log.out.str());
}
void workflow(const fs::path& root, bool indexed = false) {
    const auto profile = indexed ? ExternalLabelMaskFormat::PngIndexed : ExternalLabelMaskFormat::Png;
    const std::string profile_name = indexed ? "png-indexed" : "png";
    const unsigned largest = indexed ? 255 : 65535;
    const auto depth = indexed ? 8 : 16;
    const auto source = root / "source.ome.tif", mask = root / "labels.png";
    qpx_test::fluorescence_fixture(source, false);
    write(mask, png_labels({7, largest, 7, 0}, depth, indexed));
    const auto before = bytes(mask), source_before = bytes(source);
    Project project;
    const auto image = project.add_tiff_image(source.u8string(), {});
    auto doc = project.document(image);
    const auto area = doc->add_rectangle({0, 0, 2, 2}, "Original input");
    RawRegionRequest channels;
    channels.channels = 3;
    auto p = options(mask);
    p.format = profile;
    p.input_sha256 = label_analysis_raw_input_sha256(*doc, area, channels);
    const auto input = root / "input.qpx";
    project.save_new(input.u8string());
    const auto input_before = bytes(input);
    const auto request = LabelAnalysisRequest::external_mask(*doc, area, channels, p);
    const auto run = run_label_analysis(request);
    const auto captured = std::get<ExternalLabelMaskOptions>(run->options());
    check(captured.reader_sha256.size() == 64 && captured.format == profile,
          "New PNG capture did not pin its decoder before execution");
    check(run->source_labels() == std::vector<std::uint32_t>{7, largest} &&
              run->raster().labels() == std::vector<std::uint32_t>{1, 2, 1, 0},
          "Native PNG workflow lost sparse IDs or crop registration");
    check(run->measurements()[0].channels[0].sum == 22000 &&
              run->measurements()[1].channels[0].sum == 11000 &&
              run->contours()->components()[0].geometry.area() == 2,
          "PNG regions did not measure original raw source/geometry");
    const auto id = keep_label_analysis(*doc, request, *run);
    const auto count = doc->object_count();
    const auto published = publish_label_analysis(*doc, request, *run, "PNG labels");
    check(published.size() == 2 && doc->object_count() == count + 2 &&
              doc->annotation(published[1])
                      .properties->metadata()
                      .at("qpx.external-mask.source-label/1") == std::to_string(largest),
          "PNG publication lost exact identity or partial objects");
    doc->undo();
    check(doc->object_count() == count, "PNG publication was not one undo");
    doc->redo();
    const auto saved = root / "saved.qpx";
    project.save_new(saved.u8string());
    auto restored = Project::open_without_journal_recovery(saved.u8string())->document(image);
    const auto replay = run_label_analysis(replay_label_analysis(*restored, id));
    check(replay->sha256() == id && replay->source_labels() == run->source_labels(),
          "Saved PNG recipe failed exact native replay");
    const auto csv = root / "native.csv", raster = root / "native.qpl";
    save_label_analysis_csv(*replay, csv.u8string());
    save_label_analysis_raster(*replay, raster.u8string());
    check(bytes(csv).find(",2," + std::to_string(largest) + ",") != std::string::npos &&
              open_label_raster(raster.u8string()).sha256() == run->raster().sha256(),
          "PNG export lost original labels or raster values");
    refuses([&] { save_label_analysis_raster(*replay, mask.u8string()); });
    refuses([&] { save_label_analysis_csv(*replay, source.u8string()); });

    for (const auto& change : {"decoder", "missing-decoder", "format"}) {
        using Value = ImageDataValue;
        auto values = restored->image_data_properties()->values();
        auto& records = std::get<Value::Map>(values.at("qpx.label-analysis/1").value);
        auto& record = std::get<Value::Map>(records.at(id).value);
        auto& text = std::get<std::string>(record.at("parameters").value);
        auto recipe = Json::parse(text);
        check(recipe.at("reader_sha256") == captured.reader_sha256 && recipe.at("format") == profile_name,
              "Saved PNG recipe omitted its exact decoder/format");
        if (std::string(change) == "decoder")
            recipe["reader_sha256"] = std::string(64, '0');
        else if (std::string(change) == "missing-decoder")
            recipe.erase("reader_sha256");
        else
            recipe["format"] = indexed ? "png" : "png-indexed";
        text = recipe.dump();
        restored->set_image_data_properties(
            std::make_shared<const ImageDataProperties>(std::move(values)));
        refuses([&] { run_label_analysis(replay_label_analysis(*restored, id)); });
        restored->undo();
    }

    const auto output = root / "cli.qpx";
    const std::vector<std::string> args{"batch",
                                        "--import-label-mask",
                                        input.u8string(),
                                        output.u8string(),
                                        "--image",
                                        "1",
                                        "--roi",
                                        std::to_string(area),
                                        "--channels",
                                        "3",
                                        "--mask",
                                        mask.u8string(),
                                        "--mask-format",
                                        profile_name,
                                        "--producer",
                                        p.producer,
                                        "--producer-version",
                                        p.producer_version,
                                        "--model-sha256",
                                        p.model_sha256,
                                        "--input-sha256",
                                        p.input_sha256,
                                        "--max-regions",
                                        "4"};
    const auto report = cli(args);
    check(report.at("record") == id && report.at("published_objects") == 2,
          "PNG CLI differed from the same native captured workflow");
    check(cli({"batch", "--verify-labels", output.u8string(), "--image", "1", "--record", id})
                  .at("record") == id,
          "CLI could not reopen and verify its PNG project");
    const auto cli_csv = root / "cli.csv";
    cli({"batch", "--export-labels", output.u8string(), cli_csv.u8string(), "--image", "1",
         "--record", id});
    check(bytes(cli_csv) == bytes(csv), "PNG CLI and native CSV exports differ");
    cli(args, 1);
    for (const auto& target : {input, source, mask}) {
        auto bad = args;
        bad[3] = target.u8string();
        cli(bad, 1);
    }
    auto bad = args;
    bad[3] = (root / "bad-format.qpx").u8string();
    *std::next(std::find(bad.begin(), bad.end(), "--mask-format")) = "auto";
    refuses<std::invalid_argument>([&] { cli(bad); });
    check(!fs::exists(bad[3]), "Invalid format left a result project");
    check(bytes(input) == input_before && bytes(mask) == before && bytes(source) == source_before,
          "PNG workflow changed original project/mask/raw data");
    write(mask, png_labels({7, largest, 0, 0}, depth, indexed));
    refuses<WorkflowInputChanged>([&] { require_current_label_analysis_inputs(*run); });
    refuses<WorkflowInputChanged>(
        [&] { run_label_analysis(replay_label_analysis(*restored, id)); });
    refuses([&] { save_label_analysis_csv(*run, (root / "stale.csv").u8string()); });
    check(!fs::exists(root / "stale.csv"), "Changed PNG mask produced an accepted export");
    // Leave the original bytes available for the separate standalone CLI route.
    write(mask, before);
}
} // namespace
int main(int argc, char** argv) {
    try {
        const bool configured = argc == 3 && std::string(argv[1]) == "--reader-plugins";
        check(argc == 1 || configured, "Expected optional --reader-plugins ABS_MANIFEST");
        if (configured)
            configure_reader_plugins(argv[2]);
        if (!has_geos() || !has_tiff() || !has_project()) {
            std::cout << "PNG label workflow requires GEOS/TIFF/project capabilities\n";
            return 77;
        }
        const auto root =
            fs::current_path() /
            ("png-labels-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root), "Cannot create unique PNG-label fixture directory");
        if (configured) {
            raw_profiles(root);
            workflow(root);
            check(fs::create_directory(root / "indexed"), "Cannot create indexed workflow fixture directory");
            workflow(root / "indexed", true);
            std::cout
                << "Native PNG labels: exact grayscale/palette codes, decoder binding/work limits, "
                   "original-raw measurements, atomic publication/undo, save/reopen/replay, "
                   "CLI/CSV/raster agreement, refusals and protected sources pass\n";
        } else {
            missing(root);
            std::cout << "Native PNG labels: missing optional plugin refuses explicitly\n";
        }
        std::cout << "Fixtures: " << root.u8string() << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
