#pragma once
#include "qupathxx/spatial_analysis.hpp"
namespace qpx {
struct SpatialDistanceRequest::Data {
    SpatialDistanceOptions options;
    std::uint64_t revision=0;
    std::vector<SpatialObjectSnapshot> objects;
    std::vector<std::size_t> sources,targets;
    std::shared_ptr<const AnalysisProvenance> provenance;
    std::size_t bytes=0,work=0;
    double sx=1,sy=1,capture_ms=0;
    std::string digest;
    std::string input_digest() const;
    bool same_geometry(const Data&) const;
    bool matches_objects(const Document&) const;
};
}
