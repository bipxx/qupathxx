#pragma once
#include "qupathxx/raw_pixel_classifier.hpp"
namespace qpx {
struct RawClassifierCalibration {std::optional<double> x,y;};
struct RawPixelClassificationPlan {RawPixelFeatureOptions features;std::size_t work;};
struct RawPixelClassifierKernel {
    static RawPixelClassificationPlan plan(const RawImageMetadata&,const ScientificGrid&,const ScientificRawOptions&,
        const RawPixelClassifier&,std::size_t,std::optional<RawClassifierCalibration> = {});
    static std::shared_ptr<const RawPixelClassification> run(const ScientificRawGrid&,const RawPixelClassifier&,
        double,std::size_t,const std::atomic<bool>*,std::optional<RawClassifierCalibration> = {});
};
}
