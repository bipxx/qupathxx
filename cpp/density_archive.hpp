#pragma once
#include "qupathxx/spatial_density.hpp"
namespace qpx {
struct SpatialDensityArchiveCodec {
    static std::vector<std::uint8_t> encode(const SpatialDensityRecord&,const std::atomic<bool>* cancelled=nullptr);
    static std::shared_ptr<const SpatialDensityRecord> decode(const std::uint8_t*,std::size_t,const std::atomic<bool>* cancelled=nullptr);
};
}
