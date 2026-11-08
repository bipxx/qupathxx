#pragma once
#include "qupathxx/cell_statistics.hpp"
#include "intensity_moments.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace qpx::cell_detail {
// Shared reducer only. Public result/codec/consumer integration is version-gated;
// legacy means keep their existing accumulation and remain independently stored.
namespace intensity_detail {
inline void checkpoint(const std::atomic<bool>* cancel) {
    if(cancel && cancel->load(std::memory_order_relaxed)) throw std::runtime_error("cell intensity statistics cancelled");
}
using Moment=detail::IntensityMoment;
using Compartment=detail::IntensityCompartment;
struct Cell {Compartment nucleus,cell,cytoplasm;};
}
inline std::size_t cell_intensity_statistics_work_bytes(std::size_t count) {
    constexpr auto per_cell=sizeof(intensity_detail::Cell)+sizeof(CellIntensityStatistics);
    if(count>std::numeric_limits<std::uint32_t>::max() || count>std::numeric_limits<std::size_t>::max()/per_cell)
        throw std::length_error("cell intensity statistics component count exceeds admission");
    return count*per_cell;
}
// RGB and channels are interleaved triples on the SAME decoded sample lattice.
// Cell bodies include their nucleus; cytoplasm is body minus ALL nuclei. No
// source IO, domain expansion, resampling or changes to the existing means.
template<class RGB,class Channels>
std::vector<CellIntensityStatistics> measure_cell_intensity_statistics(
    const RGB& rgb,const Channels& channels,const std::vector<std::uint32_t>& nuclei,
    const std::vector<std::uint32_t>& bodies,std::size_t count,std::size_t work_bytes,
    const std::atomic<bool>* cancelled=nullptr) {
    using namespace intensity_detail;checkpoint(cancelled);
    const auto n=nuclei.size();
    if(n>std::numeric_limits<std::size_t>::max()/3 || bodies.size()!=n || rgb.size()!=3*n || channels.size()!=3*n)
        throw std::invalid_argument("cell intensity statistics sample lattices differ");
    if(cell_intensity_statistics_work_bytes(count)>work_bytes)
        throw std::length_error("cell intensity statistics exceed work budget");
    std::vector<Cell> accumulators(count);
    std::vector<CellIntensityStatistics> result(count);
    for(std::size_t i=0;i<n;++i) {
        if((i&4095)==0) checkpoint(cancelled);
        const auto a=nuclei[i],b=bodies[i];
        if(a>count || b>count || (a && a!=b)) throw std::invalid_argument("cell intensity statistics labels are inconsistent");
        if(!b) continue;
        ColorVector color,transformed;
        for(std::size_t c=0;c<3;++c) {
            color[c]=double(rgb[3*i+c]);transformed[c]=double(channels[3*i+c]);
            if(!std::isfinite(color[c]) || color[c]<0 || color[c]>255)
                throw std::invalid_argument("cell intensity statistics RGB sample exceeds device range");
        }
        auto& cell=accumulators[b-1];cell.cell.add(color,transformed);
        if(a) cell.nucleus.add(color,transformed);else cell.cytoplasm.add(color,transformed);
    }
    for(std::size_t i=0;i<count;++i) {
        if((i&4095)==0) checkpoint(cancelled);const auto& cell=accumulators[i];
        if(!cell.nucleus.count) throw std::invalid_argument("cell intensity statistics component has no nucleus");
        result[i].nucleus=cell.nucleus.finish();result[i].cell=cell.cell.finish();
        if(cell.cytoplasm.count) result[i].cytoplasm=cell.cytoplasm.finish();
    }
    checkpoint(cancelled);return result;
}
}
