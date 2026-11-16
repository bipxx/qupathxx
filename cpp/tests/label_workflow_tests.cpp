#include "qupathxx/label_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_fixture.hpp"
#include "label_mask_fixture.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace qpx;
namespace fs = std::filesystem;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(double value, double expected) {
    check(std::isfinite(value) && std::abs(value - expected) < 1e-10,
          "Independent workflow value differs");
}
template <class E = std::exception, class F> void refuses(F action) {
    try {
        action();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("Invalid label workflow was accepted");
}
std::string bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
std::shared_ptr<Document> scene() {
    std::vector<std::uint8_t> rgb(8 * 4 * 3, 0);
    for (std::size_t i = 0; i < 32; ++i)
        rgb[i * 3 + (i % 8 < 4 ? 0 : 2)] = 255;
    auto doc = std::make_shared<Document>(Image(8, 4, std::move(rgb)));
    doc->set_calibration(.5, .75);
    doc->add_rectangle({0, 0, 8, 4}, "Area");
    return doc;
}
void slic(const fs::path& root) {
    auto doc = scene();
    using V = ImageDataValue;
    doc->set_image_data_properties(
        std::make_shared<const ImageDataProperties>(V::Map{{"user", V{std::string("preserve")}}}));
    const auto original = doc->image_data_properties();
    SuperpixelOptions options;
    options.region_size = 4;
    const LabelAnalysisRequest request(*doc, 1, options);
    const auto run = run_label_analysis(request);
    check(run->raster().regions().size() == 2 && run->contours()->components().size() == 2,
          "Analytical SLIC workflow lost regions");
    check(run->matches(request) && request.matches(*doc),
          "Native capture does not match its own source");
    check(std::string(run->measurement_domain()) == "decoded-device-RGB8",
          "RGB statistics incorrectly claim raw channels");
    check(run->measurements().size() == 2 && run->measurements()[0].pixels == 16,
          "SLIC statistics have wrong domain");
    near(*run->measurements()[0].channels[0].sum, 4080);
    near(*run->measurements()[0].channels[2].sum, 0);
    for (const auto& c : run->contours()->components())
        near(c.geometry.area(), 16);
    refuses<std::invalid_argument>([&] { publish_label_analysis(*doc, request, *run); });
    const auto id = keep_label_analysis(*doc, request, *run);
    check(label_analysis_ids(*doc) == std::vector<std::string>{id}, "Keep lost exact ID");
    refuses<WorkflowOutputExists>([&] { keep_label_analysis(*doc, request, *run); });
    doc->undo();
    check(doc->image_data_properties()->equals(*original), "Keep needs more than one undo");
    doc->redo();
    auto corrupt = doc->image_data_properties()->values();
    std::get<V::Map>(
        std::get<V::Map>(corrupt.at("qpx.label-analysis/1").value).at(id).value)["input"] =
        V{std::string(64, '0')};
    doc->set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(corrupt)));
    refuses<WorkflowInputChanged>([&] { publish_label_analysis(*doc, request, *run); });
    check(doc->object_count() == 1, "Corrupt kept association partially published objects");
    doc->undo();
    const auto count = doc->object_count();
    const auto ids = publish_label_analysis(*doc, request, *run, "SLIC");
    check(ids.size() == 2 && doc->object_count() == count + 2,
          "Native publication did not create the complete cohort");
    for (auto child : ids) {
        const auto a = doc->annotation(child);
        check(a.kind == ObjectKind::Detection && a.parent_id == 1,
              "SLIC publication hierarchy differs");
        near(a.properties->measurements().at("Label pixels"), 16);
        check(a.properties->metadata().at("qpx.label-analysis.run/1") == id,
              "Publication lacks exact run association");
    }
    refuses<WorkflowOutputExists>([&] { publish_label_analysis(*doc, request, *run); });
    refuses<std::invalid_argument>([&] { remove_label_analysis(*doc, id); });
    doc->undo();
    check(doc->object_count() == count, "SLIC publication needs more than one undo");
    doc->redo();
    Project project;
    project.add_document(doc);
    const auto saved = root / "slic.qpx";
    project.save_new(saved.u8string());
    auto opened = Project::open_without_journal_recovery(saved.u8string());
    auto restored = opened->document(1);
    const auto replay = replay_label_analysis(*restored, id);
    const auto repeated = run_label_analysis(replay);
    check(repeated->sha256() == id && repeated->input_sha256() == run->input_sha256() &&
              repeated->raster().labels() == run->raster().labels(),
          "Native save/reopen changed exact SLIC replay");
    check(restored->annotation(ids[0]).properties->equals(*doc->annotation(ids[0]).properties),
          "Native project changed published statistics");
    const auto csv = root / "slic.csv", again = root / "slic-again.csv", raster = root / "slic.qpl";
    check(save_label_analysis_csv(*run, csv.u8string()) > 0, "CSV export is empty");
    save_label_analysis_csv(*repeated, again.u8string());
    check(bytes(csv) == bytes(again) && bytes(csv).find("decoded-device-RGB8") != std::string::npos,
          "Exact replay changed CSV bytes/domain");
    save_label_analysis_raster(*run, raster.u8string());
    check(open_label_raster(raster.u8string()).sha256() == run->raster().sha256(),
          "Raster export changed exact labels");
    const auto old_csv = bytes(csv);
    refuses([&] { save_label_analysis_csv(*run, csv.u8string()); });
    check(bytes(csv) == old_csv, "Export overwrote existing CSV");
    const auto protected_new = root / "protected-new.csv";
    refuses([&] {
        save_label_analysis_csv(*run, protected_new.u8string(), 4096, {protected_new.u8string()});
    });
    check(!fs::exists(protected_new), "Export created a protected path");
    refuses([&] { save_label_analysis_csv(*run, (root / "too-small.csv").u8string(), 1); });
    check(!fs::exists(root / "too-small.csv"), "Failed CSV quota left a result");
    const auto user_child =
        doc->add_cell(Roi::rectangle({.1, .1, .2, .2}), {}, "User cell", ids[0]);
    refuses<std::invalid_argument>([&] { remove_label_publication(*doc, id); });
    check(doc->annotation(user_child).parent_id == ids[0], "Removal damaged user descendants");
    doc->undo();
    doc->update_roi(ids[0], Roi::rectangle({0, 0, 1, 1}));
    refuses<std::invalid_argument>([&] { remove_label_publication(*doc, id); });
    doc->undo();
    const auto old_facts = doc->annotation(ids[0]).properties;
    auto edited = old_facts->measurements();
    edited["Label pixels"] = 99;
    doc->set_object_properties(
        ids[0], std::make_shared<const ObjectProperties>(old_facts->metadata(), std::move(edited)));
    refuses<std::invalid_argument>([&] { remove_label_publication(*doc, id); });
    doc->undo();
    check(remove_label_publication(*doc, id) == ids && doc->object_count() == count,
          "Native publication removal differs");
    doc->undo();
    check(doc->object_count() == count + 2, "Publication removal needs more than one undo");
    doc->redo();
    remove_label_analysis(*doc, id);
    check(label_analysis_ids(*doc).empty() && doc->image_data_properties()->equals(*original),
          "Record removal damaged user data");
    doc->undo();
    const LabelAnalysisRequest fresh(*doc, 1, options);
    refuses<WorkflowInputChanged>([&] { keep_label_analysis(*doc, fresh, *run); });
    doc->update_rectangle(1, {0, 0, 7, 4});
    check(!request.matches(*doc), "Changed ROI still matches capture");
    refuses<WorkflowInputChanged>([&] { publish_label_analysis(*doc, request, *run); });
    refuses<WorkflowInputChanged>([&] { run_label_analysis(replay_label_analysis(*doc, id)); });
    doc->undo();
    doc->set_calibration(1, 1);
    check(!request.matches(*doc), "Changed calibration still matches capture");
    doc->undo();
    auto other = scene();
    check(!request.matches(*other), "Different document acquired capture authority");
    std::atomic<bool> cancel{true};
    refuses<WorkflowCancelled>([&] { run_label_analysis(fresh, &cancel); });
    refuses<WorkflowCancelled>([&] {
        save_label_analysis_raster(*run, (root / "cancelled.qpl").u8string(), {}, &cancel);
    });
    check(!fs::exists(root / "cancelled.qpl"), "Cancelled export left a file");
    const auto boundary =
        doc->add_roi(Roi::polygon({{.5, .5}, {1.5, .5}, {1.5, 1.5}}), "Center-boundary polygon");
    check(run_label_analysis(LabelAnalysisRequest(*doc, boundary, options))
                  ->raster()
                  .foreground_pixels() == 3,
          "Polygon coverage lost centers on its maximum boundary");
    const auto half_open = doc->add_rectangle({.5, .5, 1, 1}, "Half-open rectangle");
    check(run_label_analysis(LabelAnalysisRequest(*doc, half_open, options))
                  ->raster()
                  .foreground_pixels() == 1,
          "Conservative crop changed native half-open rectangle membership");
}
void spots(const fs::path& root) {
    const auto source = root / "spots.ome.tif";
    qpx_test::fluorescence_fixture(source, false);
    const auto input = bytes(source);
    Project project;
    const auto image = project.add_tiff_image(source.u8string(), {});
    auto doc = project.document(image);
    const auto area = doc->add_rectangle({0, 0, 2, 2}, "Area");
    const auto left =
        doc->add_cell(Roi::rectangle({0, 0, 1, 2}), Roi::rectangle({0, 0, 1, 2}), "Left", area);
    const auto right =
        doc->add_cell(Roi::rectangle({1, 0, 1, 2}), Roi::rectangle({1, 0, 1, 2}), "Right", area);
    doc->set_object_properties(left, std::make_shared<const ObjectProperties>(
                                         std::map<std::string, std::string>{{"user", "preserve"}},
                                         std::map<std::string, double>{{"User value", 17}}));
    const auto before_properties = doc->annotation(left).properties;
    RawRegionRequest block;
    block.channels = 3;
    SubcellularOptions options;
    options.threshold = 10000;
    const auto request =
        LabelAnalysisRequest::subcellular(*doc, area, {right, left}, block, options);
    const auto run = run_label_analysis(request);
    check(run->measurements().size() == 2 &&
              run->parent_ids() == std::vector<std::uint64_t>{right, left},
          "Spots lost row-major labels/explicit parent identity");
    check(run->cell_counts().size() == 2 && run->cell_counts()[0].spots == 1 &&
              run->cell_counts()[0].spot_pixels == 1 && run->cell_counts()[1].spot_pixels == 2,
          "Per-cell native counts differ");
    near(*run->measurements()[0].channels[0].sum, 42000);
    near(*run->measurements()[0].channels[2].sum, 804);
    near(*run->measurements()[1].channels[0].sum, 21000);
    check(std::string(run->measurement_domain()) == "original-raw-channel-samples",
          "Spot measurements do not identify original raw samples");
    const auto id = keep_label_analysis(*doc, request, *run);
    const auto n = doc->object_count();
    const auto ids = publish_label_analysis(*doc, request, *run, "Spot");
    check(ids.size() == 2 && doc->object_count() == n + 2, "Spot publication is incomplete");
    for (auto child : ids) {
        const auto a = doc->annotation(child);
        check(doc->annotation(a.parent_id).geometry.covers(a.geometry),
              "Spot extends outside its parent cell");
    }
    const auto properties = doc->annotation(left).properties;
    near(properties->measurements().at("Subcellular spot count"), 1);
    near(properties->measurements().at("Subcellular spot pixels"), 1);
    near(properties->measurements().at("User value"), 17);
    check(properties->metadata().at("user") == "preserve", "Counts discarded user metadata");
    doc->undo();
    check(doc->object_count() == n && doc->annotation(left).properties->equals(*before_properties),
          "One undo did not restore objects and cell counts");
    doc->redo();
    const auto saved = root / "spots.qpx";
    project.save_new(saved.u8string());
    auto restored = Project::open_without_journal_recovery(saved.u8string())->document(image);
    const auto replay = replay_label_analysis(*restored, id);
    const auto repeated = run_label_analysis(replay);
    check(repeated->sha256() == id && repeated->raster().sha256() == run->raster().sha256(),
          "Spot recipe/source exact replay failed after native open");
    const auto csv = root / "spots.csv", counts = root / "counts.csv",
               again = root / "spots-again.csv";
    save_label_analysis_csv(*run, csv.u8string());
    save_label_analysis_csv(*repeated, again.u8string());
    save_subcellular_counts_csv(*run, counts.u8string());
    check(bytes(csv) == bytes(again) &&
              bytes(counts).find("," + std::to_string(left) + ",1,1\n") != std::string::npos,
          "Spot replay/count CSV differs");
    auto user_counts = properties->measurements();
    user_counts["Subcellular spot count"] = 9;
    doc->set_object_properties(left, std::make_shared<const ObjectProperties>(
                                         properties->metadata(), std::move(user_counts)));
    refuses<std::invalid_argument>([&] { remove_label_publication(*doc, id); });
    check(doc->object_count() == n + 2, "Edited-count refusal partially removed spots");
    doc->undo();
    remove_label_publication(*doc, id);
    check(doc->object_count() == n &&
              doc->annotation(left).properties->equals(*before_properties) &&
              !doc->annotation(right).properties,
          "Spot removal discarded pre-existing parent facts/absence");
    doc->undo();
    check(doc->object_count() == n + 2 && doc->annotation(left).properties->equals(*properties),
          "Spot removal undo is incomplete");
    doc->redo();
    auto bounded = options;
    bounded.max_pixels = 1;
    const auto rejected = run_label_analysis(
        LabelAnalysisRequest::subcellular(*doc, area, {left, right}, block, bounded));
    check(rejected->report().rejected_large == 1 && rejected->measurements().size() == 1,
          "Workflow hid rejected large components");
    options.threshold = 1000000;
    const auto empty_request =
        LabelAnalysisRequest::subcellular(*doc, area, {left, right}, block, options);
    const auto empty = run_label_analysis(empty_request);
    check(empty->raster().regions().empty() && empty->cell_counts().size() == 2 &&
              empty->cell_counts()[0].spots == 0,
          "Zero-spot cells were omitted");
    keep_label_analysis(*doc, empty_request, *empty);
    const auto zero_ids = publish_label_analysis(*doc, empty_request, *empty);
    check(zero_ids.empty() && doc->object_count() == n &&
              doc->annotation(right).properties->measurements().at("Subcellular spot count") == 0,
          "Empty result fabricated objects or omitted zero counts");
    const auto zeros = root / "zeros.csv";
    save_subcellular_counts_csv(*empty, zeros.u8string());
    check(bytes(zeros).find("," + std::to_string(right) + ",0,0\n") != std::string::npos,
          "Zero-count CSV omitted selected cell");
    doc->undo();
    doc->update_roi(left, Roi::rectangle({0, 0, 1, .5}));
    check(!empty_request.matches(*doc), "Edited parent still matches spot capture");
    refuses<WorkflowInputChanged>([&] { keep_label_analysis(*doc, empty_request, *empty); });
    doc->undo();
    refuses<std::invalid_argument>(
        [&] { LabelAnalysisRequest::subcellular(*doc, area, {left, left}, block, options); });
    const auto boundary =
        doc->add_cell(Roi::polygon({{.5, .5}, {1.5, .5}, {1.5, 1.5}}), {}, "Boundary cell", area);
    auto boundary_options = options;
    boundary_options.threshold = 0;
    const auto boundary_run = run_label_analysis(
        LabelAnalysisRequest::subcellular(*doc, area, {boundary}, block, boundary_options));
    check(boundary_run->cell_counts()[0].spot_pixels == 3,
          "Parent raster bounds lost polygon boundary centers");
    near(*boundary_run->measurements()[0].channels[0].sum, 43000);
    check(bytes(source) == input, "Native spot workflow altered input bytes");
    {
        std::ofstream file(source, std::ios::binary | std::ios::app);
        file.put('\0');
    }
    refuses([&] { run_label_analysis(empty_request); });
    refuses([&] { save_subcellular_counts_csv(*run, (root / "stale-counts.csv").u8string()); });
    check(!fs::exists(root / "stale-counts.csv"), "Stale source produced an accepted export");
}
void jobs() {
    auto doc = scene();
    SuperpixelOptions options;
    options.region_size = 4;
    const LabelAnalysisRequest request(*doc, 1, options);
    LabelAnalysisJob job;
    check(job.request(request) && !job.request(request),
          "Job accepted overlapping/unconsumed work");
    check(job.wait(*doc).state == "ready" && job.result() && job.result()->matches(request),
          "Native job failed to retain result");
    const auto previous = job.result();
    check(job.request(request), "Completed job refused next work");
    job.cancel();
    check(job.wait(*doc).state == "cancelled" && job.result() == previous,
          "Cancelled job lost immutable prior preview");
    check(job.request(request), "Job refused work after cancel");
    doc->update_rectangle(1, {0, 0, 7, 4});
    check(job.wait(*doc).state == "stale" && job.result() == previous,
          "Stale job stamped old preview as current");
    doc->undo();
    const LabelAnalysisRequest fresh(*doc, 1, options);
    check(job.request(fresh) && job.wait(*doc).state == "ready",
          "Job did not recover with fresh capture");
}
void external_masks(const fs::path& root) {
    const auto source = root / "external-source.ome.tif", mask = root / "external-labels.tif";
    qpx_test::fluorescence_fixture(source, false);
    qpx_test::label_mask_fixture(mask, {7, 1000, 7, 0});
    const auto mask_before = bytes(mask);
    Project project;
    const auto image = project.add_tiff_image(source.u8string(), {});
    auto doc = project.document(image);
    const auto area = doc->add_rectangle({0, 0, 2, 2}, "Producer input");
    RawRegionRequest channels;
    channels.channels = 3;
    ExternalLabelMaskOptions options;
    options.path = fs::absolute(mask).u8string();
    options.producer = "independent-test-producer";
    options.producer_version = "1";
    options.model_sha256 = std::string(64, 'a');
    options.input_sha256 = label_analysis_raw_input_sha256(*doc, area, channels);
    const auto request = LabelAnalysisRequest::external_mask(*doc, area, channels, options);
    const auto run = run_label_analysis(request);
    check(run->method() == LabelAnalysisMethod::ExternalMask &&
              run->source_labels() == std::vector<std::uint32_t>{7, 1000} &&
              run->raster().labels() == std::vector<std::uint32_t>{1, 2, 1, 0},
          "Sparse external object IDs were lost or misregistered");
    near(*run->measurements()[0].channels[0].sum, 22000);
    near(*run->measurements()[1].channels[0].sum, 11000);
    near(run->contours()->components()[0].geometry.area(), 2);
    const auto id = keep_label_analysis(*doc, request, *run);
    const auto ids = publish_label_analysis(*doc, request, *run, "Imported");
    check(ids.size() == 2 && doc->annotation(ids[1]).properties->metadata().at(
                                 "qpx.external-mask.source-label/1") == "1000",
          "Published mask lost original ID/model association");
    check(doc->annotation(ids[0]).properties->metadata().at("qpx.external-mask.model/1") ==
              options.model_sha256,
          "Published mask lost producer model identity");
    const auto saved = root / "external.qpx";
    project.save_new(saved.u8string());
    auto restored = Project::open_without_journal_recovery(saved.u8string())->document(image);
    const auto repeated = run_label_analysis(replay_label_analysis(*restored, id));
    check(repeated->sha256() == id && repeated->source_labels() == run->source_labels(),
          "Offline mask recipe/file/raw input failed exact save/open replay");
    const auto csv = root / "external.csv";
    save_label_analysis_csv(*run, csv.u8string());
    check(bytes(csv).find(",2,1000,") != std::string::npos, "External CSV dropped source label");
    refuses([&] { save_label_analysis_raster(*run, options.path); });
    check(bytes(mask) == mask_before, "External mask export overwrote source");
    auto wrong = options;
    wrong.input_sha256[0] = wrong.input_sha256[0] == '0' ? '1' : '0';
    refuses<WorkflowInputChanged>([&] {
        run_label_analysis(LabelAnalysisRequest::external_mask(*doc, area, channels, wrong));
    });
    wrong = options;
    wrong.model_sha256 = std::string(64, 'b');
    check(run_label_analysis(LabelAnalysisRequest::external_mask(*doc, area, channels, wrong))
                  ->sha256() != id,
          "Distinct external model received prior result identity");
    wrong = options;
    wrong.max_regions = 1;
    refuses<WorkflowCaptureLimit>([&] {
        run_label_analysis(LabelAnalysisRequest::external_mask(*doc, area, channels, wrong));
    });
    wrong = options;
    wrong.path = fs::absolute(source).u8string();
    refuses<std::invalid_argument>([&] {
        run_label_analysis(LabelAnalysisRequest::external_mask(*doc, area, channels, wrong));
    });
    refuses<std::invalid_argument>([&] { import_label_mask_tiff(options, 3, 2); });
    const auto maximum = root / "maximum-label.tif";
    qpx_test::label_mask_fixture(maximum, {UINT32_MAX, 0, 0, 1});
    wrong = options;
    wrong.path = fs::absolute(maximum).u8string();
    const auto exact = import_label_mask_tiff(wrong, 2, 2);
    check(exact.source_labels == std::vector<std::uint32_t>{1, UINT32_MAX} &&
              exact.raster.labels() == std::vector<std::uint32_t>{2, 0, 0, 1},
          "UInt32 external IDs were rounded/truncated");
    const auto diagonal = root / "diagonal-label.tif";
    qpx_test::label_mask_fixture(diagonal, {7, 0, 0, 7});
    wrong.path = fs::absolute(diagonal).u8string();
    const auto multipart =
        run_label_analysis(LabelAnalysisRequest::external_mask(*doc, area, channels, wrong));
    check(multipart->raster().regions().size() == 1 &&
              multipart->contours()->components().size() == 1,
          "Shared contours split one external identity");
    near(multipart->contours()->components()[0].geometry.area(), 2);
    for (const auto type : {2, 3}) {
        const auto path = root / ("unsupported-label-type-" + std::to_string(type) + ".tif");
        auto data = bytes(maximum);
        data[138] = char(type);
        std::ofstream file(path, std::ios::binary);
        file.write(data.data(), data.size());
        file.close();
        wrong.path = fs::absolute(path).u8string();
        refuses<std::invalid_argument>([&] { import_label_mask_tiff(wrong, 2, 2); });
    }
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>(
        [&] { import_label_mask_tiff(options, 2, 2, 256u * 1024 * 1024, &cancelled); });
    refuses<WorkflowCaptureLimit>([&] { import_label_mask_tiff(options, 2, 2, 65536); });
    doc->set_calibration(2, 2);
    refuses<WorkflowInputChanged>([&] {
        run_label_analysis(LabelAnalysisRequest::external_mask(*doc, area, channels, options));
    });
    doc->undo();
    const auto fractional = doc->add_roi(Roi::polygon({{0, 0}, {2, 0}, {2, 2}}), "Triangle");
    auto clipped = options;
    clipped.input_sha256 = label_analysis_raw_input_sha256(*doc, fractional, channels);
    refuses<std::invalid_argument>([&] {
        run_label_analysis(
            LabelAnalysisRequest::external_mask(*doc, fractional, channels, clipped));
    });
    {
        std::ofstream out(mask, std::ios::binary | std::ios::app);
        out.put('\0');
    }
    refuses<WorkflowInputChanged>([&] { require_current_label_analysis_inputs(*run); });
    refuses([&] { save_label_analysis_csv(*run, (root / "changed-mask.csv").u8string()); });
    refuses<WorkflowInputChanged>(
        [&] { run_label_analysis(replay_label_analysis(*restored, id)); });
    check(!fs::exists(root / "changed-mask.csv"), "Changed external file produced accepted export");
}
} // namespace
int main() {
    try {
        if (!has_geos() || !has_tiff() || !has_project()) {
            std::cout << "Label workflow: optional GEOS/TIFF/project capabilities disabled\n";
            return 77;
        }
        const auto root =
            fs::current_path() /
            ("label-workflow-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root), "Unique workflow fixture directory");
        slic(root);
        spots(root);
        jobs();
        external_masks(root);
        std::cout << "Native label workflow: analytical regions/spots/offline masks, atomic "
                     "publication/counts/undo, exact save/open/replay/CSV/raster, protected "
                     "exports and stale/cancel jobs pass\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
