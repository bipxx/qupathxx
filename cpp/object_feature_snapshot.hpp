#pragma once
#include <algorithm>
#include <stdexcept>
#include "qupathxx/object_classifier.hpp"
#include "qupathxx/object_classifier_archive.hpp"
namespace qpx {
struct ObjectFeatureRequest::Data {
    struct Row {std::uint64_t id,revision;std::string label;Roi body;std::optional<Roi> nucleus;ObjectKind kind;};
    using Origin=ObjectFeatureCellOrigin;
    ObjectFeatureOptions options;
    std::vector<Row> rows;
    std::vector<CellFeature> cells;
    // Only known selected rows occupy statistics storage; old mean-only tables
    // allocate none. Charge capacity growth before allocating it.
    std::vector<std::pair<std::size_t,CellIntensityStatistics>> statistics;
    void store_statistics(std::size_t row,CellIntensityStatistics value) {
        if(statistics.size()==statistics.capacity()) {
            const auto capacity=std::max<std::size_t>(1,2*statistics.capacity());
            const auto extra=(capacity-statistics.capacity())*sizeof(statistics.front());
            if(extra>options.snapshot_bytes-bytes) throw std::length_error("cell statistics exceed feature snapshot budget");
            statistics.reserve(capacity);bytes+=extra;
        }
        statistics.emplace_back(row,std::move(value));
    }
    std::vector<Origin> origins; // RGB rows only; immutable native source manifests.
    std::shared_ptr<const AnalysisProvenance> anchor;
    std::uint64_t revision=0,next_id=0;
    double sx=1,sy=1,capture_ms=0;
    bool physical=false;
    std::size_t bytes=0;
};
}
