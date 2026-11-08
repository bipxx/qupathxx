#include "qupathxx/scientific_channels.hpp"
#include "scientific_channel_values.hpp"
#include <cmath>
#include <stdexcept>
namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("scientific channel transform cancelled");
}
}
std::uint64_t scientific_channel_work_bytes(const ScientificRgbGrid& input) {
    return scientific_channel_work_bytes(input.grid());
}
std::uint64_t scientific_channel_work_bytes(const ScientificGrid& grid) {
    return 50ULL*std::uint64_t(grid.width())*std::uint64_t(grid.height())+sizeof(StainTransform)+4096;
}
ScientificChannels::ScientificChannels(const ScientificRgbGrid& input,const StainTransform& transform,
    ScientificChannelMode mode,std::size_t work):grid_(input.grid()),mode_(mode),white_(transform.white()),stains_(transform.stains()),
    values_(input.values().size(),0),valid_(input.valid()),work_bytes_(work) {}
struct ScientificChannelKernel {
    static ScientificChannels run(const ScientificRgbGrid& input,const StainTransform& transform,
        ScientificChannelMode mode,std::size_t work_bytes,const std::atomic<bool>* cancelled) {
        checkpoint(cancelled);
        if(!work_bytes || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("invalid scientific channel work budget");
        const auto charge=scientific_channel_work_bytes(input);
        if(charge>work_bytes) throw std::length_error("scientific channel input/output exceed work budget");
        ScientificChannels result(input,transform,mode,static_cast<std::size_t>(charge));
        detail::scientific_channel_values(input.values(),input.valid(),transform,mode==ScientificChannelMode::Stains,result.values_,cancelled);
        checkpoint(cancelled);return result;
    }
};
ScientificChannels scientific_optical_densities(const ScientificRgbGrid& input,const StainTransform& transform,
    std::size_t work,const std::atomic<bool>* cancelled) {
    return ScientificChannelKernel::run(input,transform,ScientificChannelMode::OpticalDensity,work,cancelled);
}
ScientificChannels scientific_deconvolve(const ScientificRgbGrid& input,const StainTransform& transform,
    std::size_t work,const std::atomic<bool>* cancelled) {
    return ScientificChannelKernel::run(input,transform,ScientificChannelMode::Stains,work,cancelled);
}
}
