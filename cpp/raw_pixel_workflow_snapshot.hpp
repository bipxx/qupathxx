#pragma once
#include "qupathxx/raw_pixel_workflows.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/provenance.hpp"
#include "qupathxx/source_identity.hpp"
#include "raw_pixel_classifier_internal.hpp"
#include "qupathxx/raw_pixel_contours.hpp"
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
std::string raw_pixel_source_metadata_epoch(const RawImageMetadata&,const ScientificRawLattice&);
RawPixelSourceSnapshot raw_pixel_capture_window_source(const Document&,const ScientificRawWindow&,std::uint64_t object,std::size_t budget);
bool raw_pixel_source_matches(const ImageServer&,const RawPixelSourceSnapshot&);
bool raw_pixel_publication_window_source_matches(const ImageServer&,const RawPixelSourceSnapshot&,const ScientificRawWindow&);
void raw_pixel_verify_window_sample(const ImageServer&,const RawPixelSourceSnapshot&,const ScientificRawWindowData&);
void raw_pixel_verify_window_sample(const ImageServer&,const RawPixelSourceSnapshot&,const ScientificRawWindow& captured_geometry,const ScientificRawWindowData&);
void raw_pixel_verify_window_digest(const ImageServer&,const RawPixelSourceSnapshot&,const ScientificRawWindow& captured_geometry,const ScientificRawDigest&);
std::size_t raw_pixel_source_snapshot_bytes(const RawPixelSourceSnapshot&);
RawPixelSourceSnapshot raw_pixel_capture_lattice_source(const Document&,const ScientificRawLattice&,std::size_t budget);
bool raw_pixel_publication_lattice_source_matches(const ImageServer&,const RawPixelSourceSnapshot&,const ScientificRawLattice&);
void raw_pixel_verify_lattice_sample(const ImageServer&,const RawPixelSourceSnapshot&,const ScientificRawLattice&,const ScientificRawWindowData&);
struct RawPixelContourKernel {
    static std::shared_ptr<const RawPixelPredictionRequest::Data> capture(const RawPixelPredictionRequest&,const std::shared_ptr<const RawPixelClassification>&);
    static bool current(const Document&,const RawPixelPredictionRequest&,const std::shared_ptr<const RawPixelClassification>&);
    static std::shared_ptr<const RawPixelContours> run(std::shared_ptr<const RawPixelPredictionRequest::Data>,std::shared_ptr<const RawPixelClassification>,TissueContourOptions,const std::atomic<bool>*);
};
}
