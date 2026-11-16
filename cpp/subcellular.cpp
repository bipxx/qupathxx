#include "qupathxx/subcellular.hpp"
#include "qupathxx/label_measurements.hpp"
#include "label_analysis_internal.hpp"
#include "raw_sample_value.hpp"
#include "imagej_gaussian.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace qpx {
namespace {
bool filtered(const SubcellularOptions& p) {
    return p.sigma_x || p.sigma_y || p.background_sigma_x || p.background_sigma_y;
}
std::string recipe(const SubcellularOptions& p) {
    Sha256 h;
    detail::label_hash_text(h, "qpx.subcellular-threshold-components/1");
    h.integer(p.channel);
    for (auto v : {p.threshold, p.sigma_x, p.sigma_y, p.background_sigma_x, p.background_sigma_y})
        detail::label_hash_real(h, v);
    h.integer(p.min_pixels);
    h.integer(p.max_pixels);
    h.integer(std::uint64_t(p.connectivity));
    return h.finish();
}
} // namespace
void SubcellularOptions::validate() const {
    detail::label_work_limit(work_bytes);
    if (!std::isfinite(threshold) || !min_pixels || min_pixels > max_pixels ||
        max_pixels > UINT32_MAX || !max_spots || max_spots > 1000000 ||
        (connectivity != SpotConnectivity::Four && connectivity != SpotConnectivity::Eight))
        throw std::invalid_argument("Invalid subcellular threshold/size/connectivity/spot limits");
    for (auto s : {sigma_x, sigma_y, background_sigma_x, background_sigma_y})
        if (!std::isfinite(s) || s < 0 || s > 4096)
            throw std::invalid_argument("Invalid subcellular Gaussian sigma");
    if ((background_sigma_x || background_sigma_y) &&
        (background_sigma_x < sigma_x || background_sigma_y < sigma_y ||
         (background_sigma_x == sigma_x && background_sigma_y == sigma_y)))
        throw std::invalid_argument(
            "Subcellular background Gaussian must be wider on at least one axis");
}
SubcellularResult::SubcellularResult(LabelRaster raster, SubcellularOptions options)
    : raster_(std::move(raster)), options_(options) {}
SubcellularResult detect_subcellular_spots(const RawPixelBuffer& pixels, const LabelRaster& parents,
                                           SubcellularOptions p,
                                           const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    p.validate();
    detail::require_raw_lattice(pixels);
    if (p.channel >= pixels.channels() || pixels.width() != parents.width() ||
        pixels.height() != parents.height())
        throw std::invalid_argument("Subcellular channel/parent grid differs from raw pixels");
    const auto n = parents.labels().size(), width = std::size_t(pixels.width()),
               height = std::size_t(pixels.height());
    std::size_t fixed = 4096;
    detail::label_charge(fixed, pixels.payload_bytes(), 1, p.work_bytes);
    detail::label_charge(fixed, parents.payload_bytes(), 1, p.work_bytes);
    detail::label_charge(fixed, parents.regions().size(), sizeof(SubcellularCellCount),
                         p.work_bytes);
    std::size_t work = fixed;
    detail::label_charge(work, n,
                         sizeof(double) + 2 * sizeof(std::uint32_t) + 2 * sizeof(std::uint8_t),
                         p.work_bytes);
    detail::label_charge(work, std::min(n, p.max_spots),
                         sizeof(LabelRasterRegion) + sizeof(std::uint32_t), p.work_bytes);
    if (filtered(p))
        detail::label_charge(work, n, 32,
                             p.work_bytes); // live float outputs + existing Gaussian scratch
    std::uint64_t small = 0, large = 0, nonfinite = 0, invalid = 0;
    std::vector<std::uint32_t> owners;
    const auto make_labels = [&] {
        std::vector<double> values(n);
        std::vector<std::uint8_t> foreground(n, 0), visited(n, 0);
        std::visit(
            [&](const auto& samples) {
                for (std::size_t i = 0; i < n; ++i) {
                    if (!(i & 4095))
                        workflow_checkpoint(cancelled);
                    if (!pixels.valid()[i]) {
                        if (parents.labels()[i])
                            ++invalid;
                        if (filtered(p))
                            throw std::invalid_argument(
                                "Subcellular Gaussian crop contains invalid pixels");
                        continue;
                    }
                    if (!parents.labels()[i] && !filtered(p))
                        continue;
                    const auto v =
                        detail::exact_raw_binary64(samples[i * pixels.channels() + p.channel]);
                    if (!std::isfinite(v)) {
                        if (parents.labels()[i])
                            ++nonfinite;
                        if (filtered(p))
                            throw std::invalid_argument(
                                "Subcellular Gaussian crop contains nonfinite pixels");
                        continue;
                    }
                    values[i] = v;
                    foreground[i] = parents.labels()[i] && v > p.threshold;
                }
            },
            pixels.storage());
        if (filtered(p)) {
            std::vector<float> input(n);
            for (std::size_t i = 0; i < n; ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                input[i] = float(values[i]);
                if (!std::isfinite(input[i]))
                    throw std::invalid_argument(
                        "Subcellular Gaussian sample exceeds finite float32");
            }
            auto signal = detail::imagej154p_gaussian(input, width, height, p.sigma_x, p.sigma_y,
                                                      p.work_bytes - fixed - n * 16, cancelled);
            if (p.background_sigma_x || p.background_sigma_y) {
                auto background = detail::imagej154p_gaussian(
                    input, width, height, p.background_sigma_x, p.background_sigma_y,
                    p.work_bytes - fixed - n * 20, cancelled);
                for (std::size_t i = 0; i < n; ++i) {
                    if (!(i & 4095))
                        workflow_checkpoint(cancelled);
                    signal[i] -= background[i];
                }
            }
            for (std::size_t i = 0; i < n; ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                if (!std::isfinite(signal[i]))
                    throw std::overflow_error("Nonfinite subcellular Gaussian response");
                foreground[i] = parents.labels()[i] && signal[i] > p.threshold;
            }
        }
        std::vector<std::uint32_t> labels(n, 0), queue;
        queue.reserve(n);
        for (std::size_t start = 0; start < n; ++start) {
            if (!(start & 4095))
                workflow_checkpoint(cancelled);
            if (!foreground[start] || visited[start])
                continue;
            const auto parent = parents.labels()[start];
            queue.clear();
            queue.push_back(std::uint32_t(start));
            visited[start] = 1;
            for (std::size_t at = 0; at < queue.size(); ++at) {
                if (!(at & 4095))
                    workflow_checkpoint(cancelled);
                const auto i = std::size_t(queue[at]), x = i % width, y = i / width;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if ((!dx && !dy) ||
                            (p.connectivity == SpotConnectivity::Four && dx && dy) ||
                            (dx < 0 && !x) || (dx > 0 && x + 1 == width) || (dy < 0 && !y) ||
                            (dy > 0 && y + 1 == height))
                            continue;
                        const auto j = std::size_t(std::int64_t(y) + dy) * width +
                                       std::size_t(std::int64_t(x) + dx);
                        if (foreground[j] && !visited[j] && parents.labels()[j] == parent) {
                            visited[j] = 1;
                            queue.push_back(std::uint32_t(j));
                        }
                    }
            }
            if (queue.size() < p.min_pixels) {
                ++small;
                continue;
            }
            if (queue.size() > p.max_pixels) {
                ++large;
                continue;
            }
            if (owners.size() == p.max_spots)
                throw WorkflowCaptureLimit("Subcellular spot count exceeds work limit");
            owners.push_back(parent);
            for (auto i : queue)
                labels[i] = std::uint32_t(owners.size());
        }
        return labels;
    };
    auto labels = make_labels(); // detection workspaces retire before measurement
    std::size_t retained = fixed;
    detail::label_charge(retained, labels.capacity(), 4, p.work_bytes);
    detail::label_charge(retained, owners.capacity(), 4, p.work_bytes);
    detail::label_charge(retained, owners.size(),
                         sizeof(LabelRasterRegion) + sizeof(SubcellularSpot), p.work_bytes);
    SubcellularResult result(LabelRaster(pixels.width(), pixels.height(), std::move(labels),
                                         {p.max_spots, p.work_bytes - fixed}, cancelled),
                             p);
    result.cells_.resize(parents.regions().size());
    for (std::size_t i = 0; i < result.cells_.size(); ++i)
        result.cells_[i].parent_label = std::uint32_t(i + 1);
    const auto measurement_extra = parents.payload_bytes() + 4096 + owners.capacity() * 4 +
                                   result.cells_.capacity() * sizeof(SubcellularCellCount) +
                                   result.raster_.regions().capacity() * sizeof(LabelRasterRegion) +
                                   owners.size() * sizeof(SubcellularSpot);
    if (measurement_extra >= p.work_bytes)
        throw WorkflowCaptureLimit("Subcellular measurements exceed budget");
    auto measured = measure_raw_label_regions(pixels, result.raster_,
                                              p.work_bytes - measurement_extra, cancelled);
    result.spots_.reserve(owners.size());
    for (std::size_t i = 0; i < owners.size(); ++i) {
        workflow_checkpoint(cancelled);
        auto& cell = result.cells_[owners[i] - 1];
        ++cell.spots;
        cell.spot_pixels += measured[i].pixels;
        result.spots_.push_back({std::uint32_t(i + 1), owners[i],
                                 RawCellCompartment{measured[i].pixels, measured[i].valid_pixels,
                                                    std::move(measured[i].channels)}});
    }
    result.small_ = small;
    result.large_ = large;
    result.nonfinite_ = nonfinite;
    result.invalid_ = invalid;
    Sha256 input;
    detail::label_hash_text(input, detail::label_raw_input(pixels, cancelled));
    detail::label_hash_text(input, parents.sha256());
    result.input_ = input.finish();
    result.recipe_ = recipe(p);
    Sha256 identity;
    detail::label_hash_text(identity, "qpx.subcellular-result/1");
    detail::label_hash_text(identity, result.input_);
    detail::label_hash_text(identity, result.recipe_);
    detail::label_hash_text(identity, result.raster_.sha256());
    for (auto n : {small, large, nonfinite, invalid})
        identity.integer(n);
    for (const auto& s : result.spots_) {
        identity.integer(s.label);
        identity.integer(s.parent_label);
        identity.integer(s.raw.pixels);
        identity.integer(s.raw.valid_pixels);
        for (const auto& c : s.raw.channels) {
            identity.integer(c.finite_samples);
            identity.integer(c.nonfinite_samples);
            for (const auto& v :
                 {c.sum, c.mean, c.minimum, c.maximum, c.sample_standard_deviation}) {
                identity.integer(bool(v));
                if (v)
                    detail::label_hash_real(identity, *v);
            }
        }
    }
    result.result_ = identity.finish();
    return result;
}
} // namespace qpx
