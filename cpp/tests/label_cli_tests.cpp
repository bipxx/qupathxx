#include "label_cli.hpp"
#include "qupathxx/label_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_source.hpp"
#include "qupathxx/object_properties.hpp"
#include "fluorescence_fixture.hpp"
#include "label_mask_fixture.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace qpx;
namespace fs = std::filesystem;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
template <class F> void refuses(F action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("Invalid label CLI arguments were accepted");
}
struct Capture {
    std::ostringstream out, err;
    std::streambuf *o = std::cout.rdbuf(out.rdbuf()), *e = std::cerr.rdbuf(err.rdbuf());
    ~Capture() {
        std::cout.rdbuf(o);
        std::cerr.rdbuf(e);
    }
};
std::string success(const std::vector<std::string>& args, std::atomic<bool>& cancelled) {
    Capture log;
    const auto result = detail::label_cli(args, cancelled);
    if (result != 0 || !log.err.str().empty())
        throw std::runtime_error("Label CLI failed: " + log.err.str());
    return log.out.str();
}
void slic(const fs::path& root, std::atomic<bool>& cancelled) {
    Project p;
    std::vector<std::uint8_t> pixels(8 * 4 * 3, 0);
    for (std::size_t i = 0; i < 32; ++i)
        pixels[i * 3 + (i % 8 < 4 ? 0 : 2)] = 255;
    auto doc = std::make_shared<Document>(Image(8, 4, std::move(pixels)));
    doc->add_rectangle({0, 0, 8, 4}, "Area");
    p.add_document(doc);
    const auto input = root / "slic-input.qpx", output = root / "slic-output.qpx";
    p.save_new(input.u8string());
    const auto before = bytes(input);
    std::vector<std::string> args{"batch",         "--slic", input.u8string(), output.u8string(),
                                  "--image",       "1",      "--roi",          "1",
                                  "--region-size", "4"};
    const auto report = success(args, cancelled);
    check(report.find("\"regions\":2") != std::string::npos &&
              report.find("\"published_objects\":2") != std::string::npos,
          "SLIC CLI did not publish analytical regions");
    auto saved = Project::open_without_journal_recovery(output.u8string())->document(1);
    const auto id = label_analysis_ids(*saved).at(0);
    check(saved->object_count() == 3, "SLIC CLI saved incomplete objects");
    check(success({"batch", "--list-labels", output.u8string(), "--image", "1"}, cancelled)
                  .find(id) != std::string::npos,
          "List omitted kept result");
    check(success({"batch", "--verify-labels", output.u8string(), "--image", "1", "--record", id},
                  cancelled)
                  .find("\"state\":\"verified\"") != std::string::npos,
          "CLI exact verification failed");
    const auto csv = root / "slic.csv", raster = root / "slic.qpl";
    success({"batch", "--export-labels", output.u8string(), csv.u8string(), "--image", "1",
             "--record", id},
            cancelled);
    success({"batch", "--export-labels", output.u8string(), raster.u8string(), "--image", "1",
             "--record", id, "--format", "raster"},
            cancelled);
    check(bytes(csv).find("decoded-device-RGB8") != std::string::npos &&
              open_label_raster(raster.u8string()).regions().size() == 2,
          "CLI exports lost domain/labels");
    const auto saved_bytes = bytes(output);
    {
        Capture log;
        check(detail::label_cli(args, cancelled) == 1, "CLI overwrote result project");
    }
    check(bytes(output) == saved_bytes, "Existing output bytes changed");
    for (const auto& path : {input, fs::path(input.u8string() + "-journal")}) {
        auto bad = args;
        bad[3] = path.u8string();
        Capture log;
        check(detail::label_cli(bad, cancelled) == 1, "CLI accepted protected output");
    }
    auto record = args;
    record[3] = (root / "slic-record-only.qpx").u8string();
    record.push_back("--record-only");
    success(record, cancelled);
    check(Project::open(record[3])->document(1)->object_count() == 1,
          "Record-only fabricated objects");
    auto limited = args;
    limited[3] = (root / "limited.qpx").u8string();
    limited.insert(limited.end(), {"--save-snapshot-bytes", "1"});
    {
        Capture log;
        check(detail::label_cli(limited, cancelled) == 1, "CLI ignored project quota");
    }
    check(!fs::exists(limited[3]), "Failed project save left output");
    auto stopped = args;
    stopped[3] = (root / "cancelled.qpx").u8string();
    cancelled = true;
    {
        Capture log;
        check(detail::label_cli(stopped, cancelled) == 130, "CLI ignored cancellation");
    }
    cancelled = false;
    check(!fs::exists(stopped[3]), "Cancelled CLI left output");
    for (const auto& flags :
         std::vector<std::vector<std::string>>{{"--image", "1"},
                                               {"--compactness", "nan"},
                                               {"--channels", "1"},
                                               {"--unexpected", "3"},
                                               {"--region-size", "4294967296"},
                                               {"--work-bytes", "1073741825"}}) {
        auto bad = args;
        bad.insert(bad.end(), flags.begin(), flags.end());
        refuses([&] { detail::label_cli(bad, cancelled); });
    }
    check(bytes(input) == before, "CLI changed original project");
}
void raw(const fs::path& root, std::atomic<bool>& cancelled) {
    const auto source = root / "source.ome.tif", input = root / "raw-input.qpx",
               output = root / "spots-output.qpx", mask = root / "labels.tif";
    qpx_test::fluorescence_fixture(source, false);
    qpx_test::label_mask_fixture(mask, {7, 1000, 7, 0});
    Project p;
    const auto image = p.add_tiff_image(source.u8string(), {});
    auto doc = p.document(image);
    const auto roi = doc->add_rectangle({0, 0, 2, 2}, "Area");
    const auto left = doc->add_cell(Roi::rectangle({0, 0, 1, 2}), {}, "Left", roi),
               right = doc->add_cell(Roi::rectangle({1, 0, 1, 2}), {}, "Right", roi);
    p.save_new(input.u8string());
    const auto before = bytes(input), source_before = bytes(source), mask_before = bytes(mask);
    std::vector<std::string> args{"batch",          "--subcellular",
                                  input.u8string(), output.u8string(),
                                  "--image",        "1",
                                  "--roi",          std::to_string(roi),
                                  "--cell",         std::to_string(right),
                                  "--cell",         std::to_string(left),
                                  "--channels",     "3",
                                  "--threshold",    "10000"};
    const auto report = success(args, cancelled);
    check(report.find("\"regions\":2") != std::string::npos &&
              report.find("\"parent_cells\":2") != std::string::npos,
          "Spot CLI lost explicit cell cohort");
    auto saved = Project::open_without_journal_recovery(output.u8string())->document(image);
    const auto id = label_analysis_ids(*saved).at(0);
    check(saved->annotation(left).properties->measurements().at("Subcellular spot pixels") == 1 &&
              saved->annotation(right).properties->measurements().at("Subcellular spot pixels") ==
                  2,
          "CLI native parent counts differ");
    success({"batch", "--verify-labels", output.u8string(), "--image", "1", "--record", id},
            cancelled);
    const auto counts = root / "counts.csv";
    success({"batch", "--export-labels", output.u8string(), counts.u8string(), "--image", "1",
             "--record", id, "--format", "counts"},
            cancelled);
    check(bytes(counts).find("," + std::to_string(right) + ",1,2\n") != std::string::npos,
          "Count CSV differs from independent parent expectation");
    const auto input_report = success({"batch", "--label-input", input.u8string(), "--image", "1",
                                       "--roi", std::to_string(roi), "--channels", "3"},
                                      cancelled);
    RawRegionRequest channels;
    channels.channels = 3;
    const auto identity = label_analysis_raw_input_sha256(*doc, roi, channels);
    check(input_report.find(identity) != std::string::npos &&
              input_report.find("\"width\":2") != std::string::npos &&
              input_report.find("\"channels\":3") != std::string::npos,
          "CLI raw input identity/geometry differs from native capture");
    const auto imported = root / "imported.qpx";
    const std::vector<std::string> import{"batch",
                                          "--import-label-mask",
                                          input.u8string(),
                                          imported.u8string(),
                                          "--image",
                                          "1",
                                          "--roi",
                                          std::to_string(roi),
                                          "--channels",
                                          "3",
                                          "--mask",
                                          mask.u8string(),
                                          "--producer",
                                          "independent-test-producer",
                                          "--producer-version",
                                          "1",
                                          "--model-sha256",
                                          std::string(64, 'a'),
                                          "--input-sha256",
                                          identity};
    check(success(import, cancelled).find("\"method\":\"external-mask\"") != std::string::npos,
          "Offline CLI adapter was not selected");
    auto restored = Project::open_without_journal_recovery(imported.u8string())->document(image);
    const auto external_id = label_analysis_ids(*restored).at(0);
    const auto repeated = run_label_analysis(replay_label_analysis(*restored, external_id));
    check(repeated->source_labels() == std::vector<std::uint32_t>{7, 1000},
          "CLI saved wrong external IDs");
    auto wrong = import;
    wrong[3] = (root / "wrong-input.qpx").u8string();
    wrong.back() = std::string(64, '0');
    {
        Capture log;
        check(detail::label_cli(wrong, cancelled) == 1, "CLI accepted wrong raw input identity");
    }
    check(!fs::exists(wrong[3]), "Wrong-input import left project");
    for (const auto& target : {source, input, mask}) {
        auto bad = import;
        bad[3] = target.u8string();
        Capture log;
        check(detail::label_cli(bad, cancelled) == 1, "CLI accepted protected scientific input");
    }
    check(bytes(input) == before && bytes(source) == source_before && bytes(mask) == mask_before,
          "CLI changed scientific inputs");
}
void verified_raw_source(const fs::path& root, std::atomic<bool>& cancelled) {
    const auto source = root / "verified-source.tif", input = root / "verified-input.qpx",
               output = root / "unreviewed-output.qpx", mask = root / "labels.tif";
    qpx_test::label_mask_fixture(source, {23, 23, 23, 23});
    {
        Project project;
        const auto id = project.add_tiff_image(source.u8string(), {});
        project.document(id)->add_rectangle({0, 0, 2, 2}, "Crop");
        project.apply_source_review(project.review_source_verification(id, *fingerprint_file(source.u8string())));
        project.save_new(input.u8string());
    }
    const auto before = bytes(input);
    const std::vector<std::string> describe{"labels", "--label-input", input.u8string(),
                                            "--image", "1", "--roi", "1", "--channels", "1"};
    success(describe, cancelled);
    qpx_test::label_mask_fixture(source, {99, 99, 99, 99});
    const auto changed = Project::open(input.u8string());
    RawRegionRequest channels;
    channels.channels = 1;
    const auto replacement = label_analysis_raw_input_sha256(*changed->document(1), 1, channels);
    const std::vector<std::string> import{"labels", "--import-label-mask", input.u8string(), output.u8string(),
                                         "--image", "1", "--roi", "1", "--channels", "1", "--mask", mask.u8string(),
                                         "--producer", "fixture", "--producer-version", "1", "--model-sha256",
                                         std::string(64, 'a'), "--input-sha256", replacement};
    for (const auto& command : {describe, import}) {
        Capture log;
        check(detail::label_cli(command, cancelled) == 1 && log.out.str().empty() &&
              log.err.str().find("differs from saved source verification") != std::string::npos,
              "Label CLI accepted replacement pixels without reviewed adoption");
    }
    check(!fs::exists(output) && bytes(input) == before, "Refused label import changed project files");
}
} // namespace
int main() {
    try {
        std::atomic<bool> cancelled{false};
        check(!detail::label_cli({"batch", "--other"}, cancelled),
              "Label CLI intercepted unrelated command");
        refuses([&] { detail::label_cli({"batch", "--slic"}, cancelled); });
        if (!has_geos() || !has_tiff() || !has_project()) {
            std::cout << "Label CLI: optional GEOS/TIFF/project capabilities disabled\n";
            return 77;
        }
        const auto root =
            fs::current_path() /
            ("label-cli-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root), "Unique CLI fixture directory");
        slic(root, cancelled);
        raw(root, cancelled);
        verified_raw_source(root, cancelled);
        std::cout << "Native label CLI: SLIC/spots/offline masks, source preservation, "
                     "kept-project reopen/exact verification/exports, protected outputs, quotas "
                     "and cancellation pass\nFixtures: "
                  << root.u8string() << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
