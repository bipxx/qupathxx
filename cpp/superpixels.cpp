#include "qupathxx/superpixels.hpp"
#include "label_analysis_internal.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace qpx {
namespace {
using Color = std::array<double, 3>;
struct Center {
    Color color{};
    double x = 0, y = 0;
    std::uint64_t count = 0;
};
Color color_value(const std::uint8_t* rgb, SuperpixelColorSpace space) {
    if (space == SuperpixelColorSpace::EncodedRGB)
        return {rgb[0] / 2.55, rgb[1] / 2.55, rgb[2] / 2.55};
    const auto linear = [](double b) {
        const auto v = b / 255;
        return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4);
    };
    const auto r = linear(rgb[0]), g = linear(rgb[1]), b = linear(rgb[2]);
    const auto f = [](double t) {
        return t > 216. / 24389 ? std::cbrt(t) : (24389. / 27 * t + 16) / 116;
    };
    const auto x = f((.4124564 * r + .3575761 * g + .1804375 * b) / .95047);
    const auto y = f(.2126729 * r + .7151522 * g + .0721750 * b);
    const auto z = f((.0193339 * r + .1191920 * g + .9503041 * b) / 1.08883);
    return {116 * y - 16, 500 * (x - y), 200 * (y - z)};
}
std::string recipe(const SuperpixelOptions& p) {
    Sha256 h;
    detail::label_hash_text(h, "qpx.slic-superpixels/1");
    h.integer(p.region_size);
    h.integer(p.iterations);
    detail::label_hash_real(h, p.compactness);
    h.integer(std::uint64_t(p.color_space));
    return h.finish();
}
std::vector<std::uint32_t> connected(const std::vector<std::uint32_t>& input, std::size_t width,
                                     std::size_t height, std::size_t step, std::size_t maximum,
                                     const std::atomic<bool>* cancelled) {
    std::vector<std::uint32_t> out(input.size(), 0), queue;
    queue.reserve(input.size());
    std::uint32_t next = 1;
    const auto small = std::max<std::size_t>(1, step * step / 4);
    for (std::size_t start = 0; start < input.size(); ++start) {
        if (!(start & 4095))
            workflow_checkpoint(cancelled);
        if (!input[start] || out[start])
            continue;
        if (next > maximum)
            throw WorkflowCaptureLimit("Connected SLIC regions exceed label limit");
        queue.clear();
        queue.push_back(std::uint32_t(start));
        out[start] = next;
        std::uint32_t adjacent = 0;
        for (std::size_t at = 0; at < queue.size(); ++at) {
            if (!(at & 4095))
                workflow_checkpoint(cancelled);
            const auto i = std::size_t(queue[at]), x = i % width, y = i / width;
            const auto visit = [&](std::size_t j) {
                if (out[j] && out[j] != next && (!adjacent || out[j] < adjacent))
                    adjacent = out[j];
                if (!out[j] && input[j] == input[start]) {
                    out[j] = next;
                    queue.push_back(std::uint32_t(j));
                }
            };
            if (x)
                visit(i - 1);
            if (x + 1 < width)
                visit(i + 1);
            if (y)
                visit(i - width);
            if (y + 1 < height)
                visit(i + width);
        }
        // Standard SLIC cleanup merges a small fragment into an already visited
        // adjacent region. Isolated/first fragments remain distinct, never lost.
        if (queue.size() < small && adjacent)
            for (auto i : queue)
                out[i] = adjacent;
        else
            ++next;
    }
    return out;
}
} // namespace
void SuperpixelOptions::validate() const {
    detail::label_work_limit(work_bytes);
    if (region_size < 2 || region_size > 4096 || !iterations || iterations > 100 ||
        !std::isfinite(compactness) || compactness <= 0 || compactness > 1000000 ||
        (color_space != SuperpixelColorSpace::EncodedRGB &&
         color_space != SuperpixelColorSpace::LabSRGB) ||
        !max_regions || max_regions > 1000000 || !max_assignment_tests ||
        max_assignment_tests > 1000000000000ULL)
        throw std::invalid_argument("Invalid SLIC region/iteration/compactness/color/work limits");
}
SuperpixelResult::SuperpixelResult(LabelRaster raster, SuperpixelOptions options, std::string input,
                                   std::string method, std::uint64_t tests)
    : raster_(std::move(raster)), options_(options), input_(std::move(input)),
      recipe_(std::move(method)), tests_(tests) {
    Sha256 h;
    detail::label_hash_text(h, "qpx.slic-result/1");
    detail::label_hash_text(h, input_);
    detail::label_hash_text(h, recipe_);
    detail::label_hash_text(h, raster_.sha256());
    result_ = h.finish();
}
SuperpixelResult slic_superpixels(const PixelBuffer& pixels,
                                  const std::vector<std::uint8_t>& domain, SuperpixelOptions p,
                                  const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    p.validate();
    const auto n = detail::label_pixels(pixels.width(), pixels.height());
    if (pixels.bytes().size() != n * 4 || (!domain.empty() && domain.size() != n))
        throw std::invalid_argument("Malformed SLIC RGB/domain lattice");
    const auto width = std::size_t(pixels.width()), height = std::size_t(pixels.height()),
               step = std::size_t(p.region_size);
    const auto nx = (width + step - 1) / step, ny = (height + step - 1) / step;
    std::size_t work = 4096;
    detail::label_charge(work, pixels.bytes().capacity(), 1, p.work_bytes);
    detail::label_charge(work, domain.capacity(), 1, p.work_bytes);
    detail::label_charge(
        work, n, sizeof(Color) + sizeof(double) + 3 * sizeof(std::uint32_t) + sizeof(std::uint8_t),
        p.work_bytes);
    // Include final region descriptors while the two label lattices/queue and
    // clustering workspace remain alive. This is conservative owned-work admission.
    detail::label_charge(work, std::min(n, p.max_regions), sizeof(LabelRasterRegion), p.work_bytes);
    detail::label_charge(work, nx * ny, 2 * sizeof(Center), p.work_bytes);
    std::vector<Color> colors(n);
    std::vector<std::uint8_t> eligible(n);
    std::vector<std::uint32_t> labels(n, 0);
    std::vector<Center> centers;
    centers.reserve(std::min(nx * ny, p.max_regions));
    Sha256 input;
    detail::label_hash_text(input, "qpx.slic-input-rgba/1");
    input.integer(width);
    input.integer(height);
    input.update(pixels.bytes().data(), pixels.bytes().size());
    std::uint64_t participating = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        const auto selected = domain.empty() ? 1 : domain[i];
        if (selected > 1)
            throw std::invalid_argument("Invalid SLIC domain flag");
        input.integer(selected);
        eligible[i] = selected && pixels.bytes()[i * 4 + 3] == 255;
        if (eligible[i]) {
            colors[i] = color_value(pixels.bytes().data() + i * 4, p.color_space);
            ++participating;
        }
    }
    if (!participating)
        throw std::invalid_argument("SLIC requires a nonempty opaque domain");
    for (std::size_t by = 0; by < ny; ++by)
        for (std::size_t bx = 0; bx < nx; ++bx) {
            workflow_checkpoint(cancelled);
            const auto x0 = bx * step, y0 = by * step, x1 = std::min(width, x0 + step),
                       y1 = std::min(height, y0 + step);
            const auto cx = double(x0 + x1 - 1) / 2, cy = double(y0 + y1 - 1) / 2;
            std::size_t best = n;
            double distance = std::numeric_limits<double>::infinity();
            for (std::size_t y = y0; y < y1; ++y)
                for (std::size_t x = x0; x < x1; ++x) {
                    const auto i = y * width + x;
                    if (!eligible[i])
                        continue;
                    const auto d =
                        (double(x) - cx) * (double(x) - cx) + (double(y) - cy) * (double(y) - cy);
                    if (d < distance) {
                        distance = d;
                        best = i;
                    }
                }
            if (best == n)
                continue;
            if (centers.size() == p.max_regions)
                throw WorkflowCaptureLimit("SLIC seed count exceeds region limit");
            centers.push_back({colors[best], double(best % width), double(best / width), 0});
            const auto label = std::uint32_t(centers.size());
            for (std::size_t y = y0; y < y1; ++y)
                for (std::size_t x = x0; x < x1; ++x)
                    if (eligible[y * width + x])
                        labels[y * width + x] = label;
        }
    std::vector<double> distances(n);
    std::vector<Center> sums(centers.size());
    std::uint64_t tests = 0;
    const auto spatial = p.compactness * p.compactness / double(step * step);
    const auto distance = [&](std::size_t i, const Center& c) {
        if (tests == p.max_assignment_tests)
            throw WorkflowCaptureLimit("SLIC assignment work limit exceeded");
        ++tests;
        double d = 0;
        for (unsigned k = 0; k < 3; ++k) {
            const auto v = colors[i][k] - c.color[k];
            d += v * v;
        }
        const auto dx = double(i % width) - c.x, dy = double(i / width) - c.y;
        return d + spatial * (dx * dx + dy * dy);
    };
    for (std::uint32_t iteration = 0; iteration < p.iterations; ++iteration) {
        workflow_checkpoint(cancelled);
        // Previous ownership is a bounded candidate as centers move near ROI
        // holes; every eligible sample remains assigned without an all-pairs pass.
        for (std::size_t i = 0; i < n; ++i) {
            if (!(i & 4095))
                workflow_checkpoint(cancelled);
            if (eligible[i])
                distances[i] = distance(i, centers[labels[i] - 1]);
        }
        for (std::size_t c = 0; c < centers.size(); ++c) {
            workflow_checkpoint(cancelled);
            const auto& center = centers[c];
            const auto left = std::size_t(std::max(0., std::floor(center.x - double(step)))),
                       top = std::size_t(std::max(0., std::floor(center.y - double(step))));
            const auto right = std::min(width, std::size_t(std::ceil(center.x + double(step))) + 1),
                       bottom =
                           std::min(height, std::size_t(std::ceil(center.y + double(step))) + 1);
            for (std::size_t y = top; y < bottom; ++y) {
                workflow_checkpoint(cancelled);
                for (std::size_t x = left; x < right; ++x) {
                    const auto i = y * width + x;
                    if (!eligible[i])
                        continue;
                    const auto d = distance(i, center);
                    if (d < distances[i] || (d == distances[i] && c + 1 < labels[i])) {
                        distances[i] = d;
                        labels[i] = std::uint32_t(c + 1);
                    }
                }
            }
        }
        std::fill(sums.begin(), sums.end(), Center{});
        for (std::size_t i = 0; i < n; ++i) {
            if (!(i & 4095))
                workflow_checkpoint(cancelled);
            if (!eligible[i])
                continue;
            auto& c = sums[labels[i] - 1];
            for (unsigned k = 0; k < 3; ++k)
                c.color[k] += colors[i][k];
            c.x += double(i % width);
            c.y += double(i / width);
            ++c.count;
        }
        for (std::size_t c = 0; c < centers.size(); ++c)
            if (sums[c].count) {
                auto& sum = sums[c];
                for (auto& x : sum.color)
                    x /= double(sum.count);
                sum.x /= double(sum.count);
                sum.y /= double(sum.count);
                centers[c] = sum;
            }
    }
    auto final = connected(labels, width, height, step, p.max_regions, cancelled);
    return SuperpixelResult(LabelRaster(pixels.width(), pixels.height(), std::move(final),
                                        {p.max_regions, p.work_bytes}, cancelled),
                            p, input.finish(), recipe(p), tests);
}
} // namespace qpx
