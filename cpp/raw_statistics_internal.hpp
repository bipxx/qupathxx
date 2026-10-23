#pragma once
#include "qupathxx/raw_image.hpp"
#include "sha256.hpp"
namespace qpx {
// Optional digest/progress hooks are native values, never Python callbacks.
struct RawStatisticsKernel {
    static void validate_coordinates(const RawImageMetadata&,const RawRegionRequest&);
    static RawRegionStatistics run(const ImageServer&,const RawRegionRequest&,std::size_t,
        const Roi*,std::uint64_t,const std::atomic<bool>*,Sha256* digest=nullptr,
        std::atomic<std::uint64_t>* completed=nullptr,bool collect_statistics=true);
};
}
