#include "qupathxx/label_geometry.hpp"
#include "qupathxx/subcellular.hpp"
#include "qupathxx/superpixels.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace qpx;
namespace fs = std::filesystem;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void close_value(double actual, double expected) {
    check(std::abs(actual - expected) < 1e-10, "Analytical raster/measurement value differs");
}
template <class E = std::exception, class F> void refuses(F f) {
    try {
        f();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("Invalid label operation was accepted");
}
PixelBuffer rgba(std::size_t w, std::size_t h) {
    PixelBuffer p(w, h);
    for (std::size_t i = 0; i < w * h; ++i) {
        p.writable_bytes()[i * 4] = 64;
        p.writable_bytes()[i * 4 + 1] = 64;
        p.writable_bytes()[i * 4 + 2] = 64;
        p.writable_bytes()[i * 4 + 3] = 255;
    }
    return p;
}
void resign(std::vector<std::uint8_t>& data) {
    Sha256 hash;
    hash.update(data.data(), data.size() - 64);
    const auto digest = hash.finish();
    std::copy(digest.begin(), digest.end(), data.end() - 64);
}
void label_values(const fs::path& root) {
    std::vector<std::uint32_t> input{0, 1, 1, 0, 2, 2, 0, 0};
    LabelRaster raster(4, 2, input);
    input[1] = 99;
    check(raster.labels()[1] == 1 && raster.foreground_pixels() == 4 &&
              raster.regions().size() == 2,
          "Label raster does not own its immutable values");
    check(raster.regions()[0].pixels == 2 && raster.regions()[0].left == 1 &&
              raster.regions()[0].right == 3 && raster.regions()[1].top == 1,
          "Label region counts/bounds differ");
    auto data = encode_label_raster(raster);
    const auto decoded = decode_label_raster(data.data(), data.size());
    check(decoded.sha256() == raster.sha256() && decoded.labels() == raster.labels(),
          "Label archive exact roundtrip differs");
    save_label_raster(raster, (root / "labels.qpl").u8string());
    check(open_label_raster((root / "labels.qpl").u8string()).sha256() == raster.sha256(),
          "Native label file open differs");
    refuses([&] { save_label_raster(raster, (root / "labels.qpl").u8string()); });
    check(open_label_raster((root / "labels.qpl").u8string()).sha256() == raster.sha256(),
          "Existing label file was overwritten");
    auto bad = data;
    bad[40] ^= 1;
    refuses<std::invalid_argument>([&] { decode_label_raster(bad.data(), bad.size()); });
    bad = data;
    bad.push_back(0);
    refuses<std::invalid_argument>([&] { decode_label_raster(bad.data(), bad.size()); });
    refuses<std::invalid_argument>([&] { decode_label_raster(data.data(), data.size() - 1); });
    bad = data;
    bad[8] = 2;
    refuses<std::invalid_argument>([&] { decode_label_raster(bad.data(), bad.size()); });
    bad = data;
    for (std::size_t i = 28; i < 36; ++i)
        bad[i] = 0;
    resign(bad);
    refuses<std::invalid_argument>([&] { decode_label_raster(bad.data(), bad.size()); });
    refuses<std::invalid_argument>([] { LabelRaster r(2, 1, {0, 2}); });
    refuses<std::invalid_argument>([] { LabelRaster r(-1, 2, {}); });
    refuses<std::invalid_argument>([] { LabelRaster r(1, 1, {1}, {0, 4096}); });
    refuses<std::invalid_argument>([&] { decode_label_raster(nullptr, data.size()); });
    LabelRaster empty(2, 1, {0, 0});
    const auto bytes = encode_label_raster(empty);
    check(decode_label_raster(bytes.data(), bytes.size()).regions().empty(),
          "All-background raster failed");
    std::atomic<bool> cancel{true};
    refuses<WorkflowCancelled>([&] { decode_label_raster(data.data(), data.size(), {}, &cancel); });
    refuses<WorkflowCancelled>(
        [&] { save_label_raster(raster, (root / "cancelled.qpl").u8string(), {}, &cancel); });
    check(!fs::exists(root / "cancelled.qpl"), "Cancelled label export left a file");
}
void superpixels() {
    auto pixels = rgba(4, 4);
    const auto original = pixels.bytes();
    SuperpixelOptions p;
    p.region_size = 2;
    p.iterations = 5;
    const auto result = slic_superpixels(pixels, {}, p);
    const std::vector<std::uint32_t> quadrants{1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 4, 3, 3, 4, 4};
    check(result.raster().labels() == quadrants,
          "Uniform SLIC analytical spatial partition differs");
    check(pixels.bytes() == original && result.raster().regions().size() == 4,
          "SLIC changed input pixels");
    auto larger = p;
    larger.work_bytes *= 2;
    check(slic_superpixels(pixels, {}, larger).sha256() == result.sha256(),
          "Work budget changed exact SLIC identity");
    larger = p;
    larger.compactness = 20;
    check(slic_superpixels(pixels, {}, larger).recipe_sha256() != result.recipe_sha256(),
          "SLIC recipe lost a scientific parameter");
    auto edges = rgba(8, 4);
    for (std::size_t i = 0; i < 32; ++i) {
        edges.writable_bytes()[i * 4] = i % 8 < 4 ? 255 : 0;
        edges.writable_bytes()[i * 4 + 1] = 0;
        edges.writable_bytes()[i * 4 + 2] = i % 8 < 4 ? 0 : 255;
    }
    p.region_size = 4;
    const auto split = slic_superpixels(edges, {}, p);
    check(split.raster().regions().size() == 2,
          "SLIC color-edge partition did not retain two regions");
    for (std::size_t i = 0; i < 32; ++i)
        check(split.raster().labels()[i] == (i % 8 < 4 ? 1u : 2u),
              "SLIC crossed the analytical strong color boundary");
    std::vector<std::uint8_t> mask(16, 0);
    for (std::size_t i = 0; i < 16; ++i)
        mask[i] = (i % 4 + i / 4) % 2 == 0;
    const auto islands = slic_superpixels(pixels, mask, p);
    check(islands.raster().regions().size() == 8,
          "Four-connected SLIC merged disconnected islands");
    for (const auto& r : islands.raster().regions())
        check(r.pixels == 1, "A disconnected SLIC label was not split");
    auto translucent = pixels;
    translucent.writable_bytes()[3] = 128;
    const auto alpha = slic_superpixels(translucent, {}, p);
    check(alpha.raster().labels()[0] == 0 && translucent.bytes()[3] == 128,
          "SLIC composed or included translucent pixels");
    refuses<std::invalid_argument>(
        [&] { slic_superpixels(pixels, std::vector<std::uint8_t>(16, 0), p); });
    refuses<std::invalid_argument>(
        [&] { slic_superpixels(pixels, std::vector<std::uint8_t>(16, 2), p); });
    auto bad = p;
    bad.compactness = NAN;
    refuses<std::invalid_argument>([&] { slic_superpixels(pixels, {}, bad); });
    bad = p;
    bad.max_assignment_tests = 1;
    refuses<WorkflowCaptureLimit>([&] { slic_superpixels(pixels, {}, bad); });
    bad = p;
    bad.max_regions = 1;
    refuses<WorkflowCaptureLimit>([&] { slic_superpixels(pixels, mask, bad); });
    std::atomic<bool> cancel{true};
    refuses<WorkflowCancelled>([&] { slic_superpixels(pixels, {}, p, &cancel); });
    if (has_geos()) {
        ScientificGrid grid(32, 32, {3, 8, 4}, {5, 12, 4});
        const auto clip = Roi::rectangle({3, 5, 8, 12});
        const auto geometry = trace_label_raster(result.raster(), grid, clip);
        check(geometry->components().size() == 4, "Shared contour adapter lost SLIC labels");
        for (const auto& c : geometry->components()) {
            check(c.pixels == 4, "Shared contour pixel count differs");
            close_value(c.geometry.area(), 24);
        }
        const auto bounds = geometry->components()[0].geometry.bounds();
        close_value(bounds.x, 3);
        close_value(bounds.y, 5);
        close_value(bounds.width, 4);
        close_value(bounds.height, 6);
        refuses<std::invalid_argument>([&] {
            trace_label_raster(result.raster(), ScientificGrid(32, 32, {3, 8, 2}, {5, 12, 4}),
                               clip);
        });
    }
}
void spots() {
    RawPixelBuffer pixels(5, 3, 2, SampleType::UInt16);
    std::fill(pixels.writable_valid().begin(), pixels.writable_valid().end(), 1);
    auto& values = std::get<std::vector<std::uint16_t>>(pixels.writable_storage());
    for (std::size_t i = 0; i < 15; ++i)
        values[i * 2 + 1] = std::uint16_t(100 + i);
    for (auto i : {0, 6, 3, 4, 13})
        values[i * 2] = 10;
    values[10 * 2] = 5;
    LabelRaster parents(5, 3, {1, 1, 3, 2, 2, 1, 1, 3, 2, 2, 1, 1, 3, 2, 2});
    SubcellularOptions p;
    p.threshold = 5;
    const auto original = values;
    const auto result = detect_subcellular_spots(pixels, parents, p);
    check(result.spots().size() == 3 && result.cells().size() == 3 &&
              result.cells()[0].spots == 1 && result.cells()[1].spots == 2 &&
              !result.cells()[2].spots,
          "Native per-cell spot associations/zero counts differ");
    check(result.spots()[0].raw.pixels == 2 && result.spots()[0].parent_label == 1,
          "Diagonal8-connected spot lost its parent");
    close_value(*result.spots()[0].raw.channels[0].sum, 20);
    close_value(*result.spots()[0].raw.channels[0].mean, 10);
    close_value(*result.spots()[0].raw.channels[1].sum, 206);
    close_value(*result.spots()[0].raw.channels[0].sample_standard_deviation, 0);
    check(!result.spots()[2].raw.channels[0].sample_standard_deviation,
          "Singleton spot manufactured a sample deviation");
    check(values == original, "Spot detector rewrote original typed samples");
    auto q = p;
    q.work_bytes *= 2;
    check(detect_subcellular_spots(pixels, parents, q).sha256() == result.sha256(),
          "Budget changed exact spot identity");
    q = p;
    q.connectivity = SpotConnectivity::Four;
    check(detect_subcellular_spots(pixels, parents, q).cells()[0].spots == 2,
          "Four-connected spot contract differs");
    q = p;
    q.min_pixels = 2;
    check(detect_subcellular_spots(pixels, parents, q).rejected_small() == 1,
          "Small spot filtering count differs");
    q = p;
    q.max_pixels = 1;
    const auto large = detect_subcellular_spots(pixels, parents, q);
    check(large.spots().size() == 1 && large.rejected_large() == 2,
          "Large spot components were silently split or retained");
    RawPixelBuffer pair(2, 1, 1, SampleType::UInt64);
    std::fill(pair.writable_valid().begin(), pair.writable_valid().end(), 1);
    auto& exact = std::get<std::vector<std::uint64_t>>(pair.writable_storage());
    exact = {10, 10};
    check(detect_subcellular_spots(pair, LabelRaster(2, 1, {1, 2}), p).spots().size() == 2,
          "Spots crossed adjacent parent-cell boundaries");
    exact = {std::uint64_t(1) << 53, 7};
    close_value(*detect_subcellular_spots(pair, LabelRaster(2, 1, {1, 1}), p)
                     .spots()[0]
                     .raw.channels[0]
                     .sum,
                double(std::uint64_t(1) << 53) + 7);
    exact[0] = (std::uint64_t(1) << 53) + 1;
    refuses<std::invalid_argument>(
        [&] { detect_subcellular_spots(pair, LabelRaster(2, 1, {1, 1}), p); });
    const auto outside = detect_subcellular_spots(pair, LabelRaster(2, 1, {0, 1}), p);
    close_value(*outside.spots()[0].raw.channels[0].sum, 7);
    RawPixelBuffer impulse(9, 9, 1, SampleType::Float32);
    std::fill(impulse.writable_valid().begin(), impulse.writable_valid().end(), 1);
    std::get<std::vector<float>>(impulse.writable_storage())[40] = 100;
    LabelRaster single(9, 9, std::vector<std::uint32_t>(81, 1));
    q = p;
    q.threshold = 10;
    q.sigma_x = q.sigma_y = 1;
    q.background_sigma_x = q.background_sigma_y = 2;
    const auto gaussian = detect_subcellular_spots(impulse, single, q);
    check(gaussian.spots().size() == 1 && gaussian.raster().labels()[40] == 1 &&
              gaussian.spots()[0].raw.pixels == 1,
          "Analytical DoG impulse did not isolate its center");
    close_value(*gaussian.spots()[0].raw.channels[0].sum,
                100); // measure original, never the processed response
    std::get<std::vector<float>>(impulse.writable_storage())[0] = NAN;
    check(detect_subcellular_spots(impulse, single, p).nonfinite_pixels() == 1,
          "Unfiltered nonfinite detection count differs");
    refuses<std::invalid_argument>([&] { detect_subcellular_spots(impulse, single, q); });
    q = p;
    q.channel = 3;
    refuses<std::invalid_argument>([&] { detect_subcellular_spots(pixels, parents, q); });
    q = p;
    q.max_spots = 1;
    refuses<WorkflowCaptureLimit>([&] { detect_subcellular_spots(pixels, parents, q); });
    std::atomic<bool> cancel{true};
    refuses<WorkflowCancelled>([&] { detect_subcellular_spots(pixels, parents, p, &cancel); });
}
} // namespace
int main() {
    try {
        const auto root =
            fs::current_path() /
            ("label-analysis-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(root);
        label_values(root);
        superpixels();
        spots();
        std::cout
            << "Native label raster: exact archives/refusals; analytical SLIC/connectivity/grid "
               "contours; per-cell spots/raw measurements/DoG/UInt64/cancel/budgets pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
