#pragma once
#include "qupathxx/pixel_prediction.hpp"
namespace qpx {
struct PixelTrainingRequest::Data {
    struct Row {std::uint64_t id;Roi roi;std::uint32_t label;std::uint64_t revision=0;ObjectKind kind=ObjectKind::Annotation;};
    PixelTrainingOptions options;StainTransform transform;PixelResolution resolution;
    std::vector<Row> rows;std::vector<std::string> classes;
    std::shared_ptr<const AnalysisProvenance> provenance;
    ImageMetadata metadata;
    std::size_t bytes=0;std::uint64_t revision=0;double capture_ms=0;
    Data(PixelTrainingOptions o,const StainTransform& t):options(std::move(o)),transform(t) {}
};

struct PixelPredictionRequest::Data {
    PixelClassifier model;PixelPredictionOptions options;Roi roi;std::string class_name;
    std::shared_ptr<const AnalysisProvenance> provenance;
    ImageMetadata metadata;std::int32_t level=0;
    std::int64_t x=0,y=0,w=0,h=0;std::size_t fixed=0,work=0,feature_budget=0;std::uint64_t tiles=0,revision=0,id=0;ObjectKind kind=ObjectKind::Annotation;
    Data(const PixelClassifier& m,PixelPredictionOptions o):model(m),options(o) {}
    bool windowed=false;
};

}
