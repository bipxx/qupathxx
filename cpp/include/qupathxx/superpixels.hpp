#pragma once
#include "qupathxx/image_server.hpp"
#include "qupathxx/label_raster.hpp"

namespace qpx {
enum class SuperpixelColorSpace { EncodedRGB, LabSRGB };
struct SuperpixelOptions {
    std::uint32_t region_size = 16, iterations = 10;
    double compactness = 10;
    // LabSRGB explicitly assumes the decoded RGB samples encode sRGB; neither
    // monitor profiles nor presentation transforms enter this scientific input.
    SuperpixelColorSpace color_space = SuperpixelColorSpace::LabSRGB;
    std::size_t max_regions = 100000, work_bytes = 256u * 1024 * 1024;
    std::uint64_t max_assignment_tests = 1000000000;
    void validate() const;
};
class SuperpixelResult {
  public:
    const LabelRaster& raster() const {
        return raster_;
    }
    const SuperpixelOptions& options() const {
        return options_;
    }
    const std::string& input_sha256() const {
        return input_;
    }
    const std::string& recipe_sha256() const {
        return recipe_;
    }
    const std::string& sha256() const {
        return result_;
    }
    std::uint64_t assignment_tests() const {
        return tests_;
    }

  private:
    LabelRaster raster_;
    SuperpixelOptions options_;
    std::string input_, recipe_, result_;
    std::uint64_t tests_ = 0;
    SuperpixelResult(LabelRaster, SuperpixelOptions, std::string, std::string, std::uint64_t);
    friend SuperpixelResult slic_superpixels(const PixelBuffer&, const std::vector<std::uint8_t>&,
                                             SuperpixelOptions, const std::atomic<bool>*);
};
// Bounded deterministic2D SLIC, row-major seed/tie ordering and four-connected
// final labels. Optional domain is0/1. Only fully opaque pixels participate;
// all others remain label0. Inputs and hidden/transparent RGB remain untouched.
// Empty eligible domains refuse. The result is a local raster, not live edits.
SuperpixelResult slic_superpixels(const PixelBuffer&, const std::vector<std::uint8_t>& domain = {},
                                  SuperpixelOptions = {},
                                  const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
