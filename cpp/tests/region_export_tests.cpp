#include "qupathxx/region_export.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_fixture.hpp"
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
template <class E = std::exception, class F> void refuses(F action) {
    try {
        action();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("Expected raw export refusal");
}
std::string bytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace
int main() {
    try {
        RawRegionExportOptions p;
        p.validate();
        p.reader = "auto";
        refuses<std::invalid_argument>([&] { p.validate(); });
        p = {};
        p.level_coordinates = true;
        refuses<std::invalid_argument>([&] { p.validate(); });
        p.reader = "ome-zarr";
        p.validate();
        p = {};
        p.reader = "org.qupathxx.qoi";
        p.tiff.cache_bytes = 0;
        refuses<std::invalid_argument>([&] { p.validate(); });
        p = {};
        p.reader_plugin_options = "{}";
        refuses<std::invalid_argument>([&] { p.validate(); });
        p.reader = "org.qupathxx.fits";
        p.reader_plugin_options = "";
        refuses<std::invalid_argument>([&] { p.validate(); });
        p.reader_plugin_options = "{}";
        p.validate();
        p = {};
        p.request.channels = 0;
        refuses<std::invalid_argument>([&] { p.validate(); });
        p = {};
        p.full_width = false;
        p.request.region.width = 0;
        refuses<std::invalid_argument>([&] { p.validate(); });
        p = {};
        if (!has_tiff()) {
            std::cout << "Raw file export: optional TIFF disabled\n";
            return 77;
        }
        const auto root =
            fs::current_path() /
            ("raw-export-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root), "Cannot create raw export test directory");
        const auto source = root / "source.ome.tif", output = root / "raw.npy";
        qpx_test::fluorescence_fixture(source, false);
        const auto before = bytes(source);
        p.request.channels = 3;
        p.full_width = p.full_height = false;
        p.request.region = {0, 0, 0, 2, 2};
        const auto result = export_raw_region(source.u8string(), output.u8string(), p);
        check(result.reader == "tiff" && result.metadata->sample_type == SampleType::UInt16 &&
                  result.request.channels == 3 && result.valid_pixels == 4 && result.source &&
                  result.reader_snapshot_json.empty(),
              "Native TIFF export report lost its exact domain");
        const auto server = open_tiff(source.u8string());
        const auto reference = root / "reference.npy";
        save_raw_numpy(server->read_raw(p.request), reference.u8string());
        check(bytes(output) == bytes(reference), "Shared export changed native NumPy samples");
        auto padded = p;
        padded.request.region.x = -1;
        const auto padded_file = root / "padded.npy",
                   padded_reference = root / "padded-reference.npy";
        const auto padding = export_raw_region(source.u8string(), padded_file.u8string(), padded);
        save_raw_numpy(server->read_raw(padded.request), padded_reference.u8string());
        check(padding.valid_pixels == 2 && bytes(padded_file) == bytes(padded_reference),
              "Legacy TIFF zero padding/validity changed");
        const auto json = raw_region_export_report_json(result);
        check(json.find("\"schema\":\"qpx.raw-region/1\"") != std::string::npos,
              "Legacy TIFF report schema changed");
        refuses<WorkflowOutputExists>(
            [&] { export_raw_region(source.u8string(), output.u8string(), p); });
        refuses([&] { export_raw_region(source.u8string(), source.u8string(), p); });
        std::atomic<bool> cancelled{true};
        const auto cancelled_file = root / "cancelled.npy";
        refuses<WorkflowCancelled>([&] {
            export_raw_region(source.u8string(), cancelled_file.u8string(), p, &cancelled);
        });
        auto structured = padded;
        structured.with_validity = true;
        const auto structured_file = root / "structured.npy";
        refuses<WorkflowCancelled>([&] {
            export_raw_region(source.u8string(), structured_file.u8string(), structured, &cancelled);
        });
        check(!fs::exists(cancelled_file) && !fs::exists(structured_file) && bytes(source) == before,
              "Raw export changed inputs or published cancelled data");
        auto bundle = structured;
        bundle.bundle = true;
        const auto cancelled_bundle = root / "cancelled.zip";
        refuses<WorkflowCancelled>([&] {
            export_raw_region(source.u8string(), cancelled_bundle.u8string(), bundle, &cancelled);
        });
        refuses<WorkflowCancelled>([&] {
            verify_raw_region_bundle(cancelled_bundle.u8string(), &cancelled);
        });
        const auto cancelled_extraction = root / "cancelled-extract.npy";
        refuses<WorkflowCancelled>([&] {
            extract_raw_region_bundle(cancelled_bundle.u8string(), cancelled_extraction.u8string(), &cancelled);
        });
        check(!fs::exists(cancelled_bundle), "Cancelled raw bundle published an archive");
        check(!fs::exists(cancelled_extraction), "Cancelled extraction published an array");
        const auto structured_report = export_raw_region(source.u8string(), structured_file.u8string(),
                                                          structured);
        const auto structured_json = raw_region_export_report_json(structured_report);
        check(structured_report.valid_pixels == 2 &&
                  structured_json.find("\"schema\":\"qpx.raw-region/3\"") != std::string::npos &&
                  structured_json.find("\"array_layout\":\"values-valid\"") != std::string::npos,
              "Structured TIFF report lost its layout or validity count");
        p.request.channels = 4;
        const auto bad = root / "bad-channel.npy";
        refuses<std::out_of_range>(
            [&] { export_raw_region(source.u8string(), bad.u8string(), p); });
        check(!fs::exists(bad), "Invalid native channel selection published an array");
        const auto pam = root / "display.pam";
        const auto display = server->read_region({0, 0, 0, 2, 2});
        save_region_pam(display, pam.u8string(), server->reader_source());
        const auto encoded = bytes(pam);
        const auto payload = encoded.find("ENDHDR\n") + 7;
        check(payload >= 7 && encoded.substr(payload) ==
                                  std::string(reinterpret_cast<const char*>(display.bytes().data()),
                                              display.bytes().size()),
              "Shared PAM export changed display pixels");
        refuses<WorkflowOutputExists>(
            [&] { save_region_pam(display, pam.u8string(), server->reader_source()); });
        refuses([&] { save_region_pam(display, source.u8string(), server->reader_source()); });
        const auto alias = root / "source-alias.pam";
        fs::create_hard_link(source, alias);
        refuses([&] { save_region_pam(display, alias.u8string(), server->reader_source()); });
        check(bytes(source) == before, "PAM export changed a hard-linked source");
        const auto cancelled_pam = root / "cancelled.pam";
        refuses<WorkflowCancelled>([&] {
            save_region_pam(display, cancelled_pam.u8string(), server->reader_source(), &cancelled);
        });
        check(!fs::exists(cancelled_pam), "Cancelled PAM export left output");
        std::cout << "Native raw-file export: typed validation, exact TIFF array/schema, "
                     "new-only/protected inputs and cancel refusals pass\n";
        std::cout << "Native PAM export: exact pixels, atomic/new-only, protected hard links and "
                     "cancellation pass\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
