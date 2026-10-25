#include "qupathxx/scientific_channels.hpp"
#include "stain_matrix.hpp"
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
        const auto inverse=detail::stain_inverse(transform.stains());
        ScientificChannels result(input,transform,mode,static_cast<std::size_t>(charge));
        for(std::size_t i=0;i<input.valid().size();++i) {
            if((i&4095)==0) checkpoint(cancelled);if(!input.valid()[i]) continue;
            ColorVector od;
            for(unsigned c=0;c<3;++c) od[c]=optical_density(input.values()[i*3+c],transform.white()[c]);
            if(mode==ScientificChannelMode::Stains) {
                for(unsigned stain=0;stain<3;++stain) {
                    const auto r=od[0]*inverse.cofactors[stain][0]/inverse.determinant;
                    const auto g=od[1]*inverse.cofactors[stain][1]/inverse.determinant;
                    const auto b=od[2]*inverse.cofactors[stain][2]/inverse.determinant;
                    const auto value=(r+g)+b;
                    if(!std::isfinite(value)) throw std::overflow_error("scientific stain channel is not representable");
                    result.values_[i*3+stain]=value;
                }
            } else for(unsigned c=0;c<3;++c) result.values_[i*3+c]=od[c];
        }
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
