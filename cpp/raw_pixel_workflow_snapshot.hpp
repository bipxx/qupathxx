#pragma once
#include "qupathxx/raw_pixel_workflows.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/provenance.hpp"
#include "qupathxx/source_identity.hpp"
#include "raw_pixel_classifier_internal.hpp"
namespace qpx {
struct RawPixelSourceSnapshot {
    RawImageMetadata metadata;
    std::shared_ptr<const ReaderSource> observed;
    std::string epoch,reader;
    RawClassifierCalibration calibration;
    std::shared_ptr<const AnalysisProvenance> provenance;
};
struct RawPixelTrainingRequest::Data {
    struct Row {std::uint64_t id;Roi roi;std::uint32_t label;std::uint64_t revision=0;ObjectKind kind=ObjectKind::Annotation;};
    std::shared_ptr<const char> capture=std::make_shared<const char>();
    ScientificGrid grid;RawPixelRecipe recipe;RawPixelTrainingOptions options;RawPixelSourceSnapshot source;
    std::vector<Row> rows;std::vector<std::string> classes;
    std::shared_ptr<const SpatialIndex> index;
    std::size_t snapshot=0,fixed=0,work=0,capacity=0;
    RawPixelFeatureOptions feature_options;
    Data(ScientificGrid g,RawPixelRecipe r,RawPixelTrainingOptions o,RawPixelSourceSnapshot s):grid(std::move(g)),recipe(std::move(r)),options(o),source(std::move(s)) {}
};
struct RawPixelPredictionRequest::Data {
    std::shared_ptr<const char> capture=std::make_shared<const char>();
    ScientificGrid grid;RawPixelClassifier model;RawPixelSourceSnapshot source;std::optional<Roi> mask;
    double minimum=0;std::size_t budget=0,fixed=0,work=0;
    Data(ScientificGrid g,const RawPixelClassifier& m,RawPixelSourceSnapshot s):grid(std::move(g)),model(m),source(std::move(s)) {}
};

std::string raw_pixel_training_sha256(const RawPixelTrainingTable&,const std::atomic<bool>* = nullptr);
std::string raw_pixel_prediction_sha256(const RawPixelClassification&,double,const std::atomic<bool>* = nullptr);
std::string raw_pixel_source_metadata_epoch(const RawImageMetadata&,const ScientificGrid&);
}
