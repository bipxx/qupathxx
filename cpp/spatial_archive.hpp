#pragma once
#include "qupathxx/spatial_analysis.hpp"
namespace qpx {
struct SpatialDistanceArchiveCodec {
    static std::vector<std::uint8_t> encode(const SpatialDistanceRecord&,const std::atomic<bool>* cancelled=nullptr);
    static std::shared_ptr<const SpatialDistanceRecord> decode(const std::uint8_t*,std::size_t,
        const std::atomic<bool>* cancelled=nullptr);
};
}
