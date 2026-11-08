#pragma once
#include "qupathxx/stains.hpp"
#include <optional>
#include <tuple>
namespace qpx {
// Statistics of the actual sampled RGB/transformed values, not display units.
// Sample SD is unavailable at n=1; an empty cytoplasm is absent altogether.
struct ChannelIntensityStatistics {
    ColorVector sum{},minimum{},maximum{};
    std::optional<ColorVector> sample_standard_deviation;
};
struct CompartmentIntensityStatistics {
    std::uint64_t samples=0;
    ChannelIntensityStatistics rgb,channels;
};
struct CellIntensityStatistics {
    CompartmentIntensityStatistics nucleus,cell;
    std::optional<CompartmentIntensityStatistics> cytoplasm;
};
inline bool operator==(const ChannelIntensityStatistics& a,const ChannelIntensityStatistics& b) {
    return std::tie(a.sum,a.minimum,a.maximum,a.sample_standard_deviation)==std::tie(b.sum,b.minimum,b.maximum,b.sample_standard_deviation);
}
inline bool operator==(const CompartmentIntensityStatistics& a,const CompartmentIntensityStatistics& b) {
    return a.samples==b.samples && a.rgb==b.rgb && a.channels==b.channels;
}
inline bool operator==(const CellIntensityStatistics& a,const CellIntensityStatistics& b) {
    return a.nucleus==b.nucleus && a.cell==b.cell && a.cytoplasm==b.cytoplasm;
}
inline bool operator!=(const CellIntensityStatistics& a,const CellIntensityStatistics& b) {return !(a==b);}

}
