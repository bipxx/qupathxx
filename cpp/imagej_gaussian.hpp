#pragma once
#include <atomic>
#include <cstddef>
#include <vector>

namespace qpx::detail {
// Private full-array float32 processing primitive. No ROI/validity normalization.
// Budgets include owned output and scratch, excluding borrowed input. Inputs are
// never modified, including on cancellation, invalid data or overflow.
// Sigma is in processing pixels, from zero through4096; at most2^26 finite
// samples are admitted. X precedes Y, with nearest-edge extension.
std::vector<float> imagej154p_gaussian(const std::vector<float>& input,
    std::size_t width,std::size_t height,double sigma_x,double sigma_y,
    std::size_t work_bytes,const std::atomic<bool>* cancelled=nullptr);
std::vector<float> imagej154p_laplacian(const std::vector<float>& blurred,
    std::size_t width,std::size_t height,std::size_t work_bytes,
    const std::atomic<bool>* cancelled=nullptr);
}
