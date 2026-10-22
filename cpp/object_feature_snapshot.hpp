#pragma once
#include "qupathxx/object_classifier.hpp"
#include "qupathxx/object_classifier_archive.hpp"
namespace qpx {
struct ObjectFeatureRequest::Data {
    struct Row {std::uint64_t id,revision;std::string label;Roi body;std::optional<Roi> nucleus;ObjectKind kind;};
    using Origin=ObjectFeatureCellOrigin;
    ObjectFeatureOptions options;
    std::vector<Row> rows;
    std::vector<CellFeature> cells;
    std::vector<Origin> origins; // RGB rows only; immutable native source manifests.
    std::shared_ptr<const AnalysisProvenance> anchor;
    std::uint64_t revision=0,next_id=0;
    double sx=1,sy=1,capture_ms=0;
    bool physical=false;
    std::size_t bytes=0;
};
}
