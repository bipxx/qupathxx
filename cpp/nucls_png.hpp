#pragma once
#include "workflow_input.hpp"
#include <vector>
namespace qpx::detail {
struct NuclsRgb {
    unsigned width=0,height=0;
    std::vector<std::uint8_t> pixels;
};
class NuclsPixelLimit : public std::length_error {
public:
    using std::length_error::length_error;
};
NuclsRgb decode_nucls_rgb_png(const WorkflowInput&,std::uint64_t available_pixels=16000000,
    const std::atomic<bool>* cancelled=nullptr);
}
