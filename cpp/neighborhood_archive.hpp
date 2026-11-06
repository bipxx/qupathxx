#pragma once
#include "qupathxx/spatial_neighborhood.hpp"
namespace qpx {
struct SpatialNeighborhoodArchiveCodec {
    static std::vector<std::uint8_t> encode(const SpatialNeighborhoodRecord&,const std::atomic<bool>* cancelled=nullptr);
    static std::shared_ptr<const SpatialNeighborhoodRecord> decode(const std::uint8_t*,std::size_t,
        const std::atomic<bool>* cancelled=nullptr);
};
}
