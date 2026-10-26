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
    static RawPixelClassificationPlan plan(const RawImageMetadata&,const ScientificRawWindow&,const ScientificRawOptions&,
        const RawPixelClassifier&,std::size_t,std::optional<RawClassifierCalibration> = {});
    static std::shared_ptr<const RawPixelWindowClassification> run(const ScientificRawWindowData&,const RawPixelClassifier&,
        double,std::size_t,const std::atomic<bool>*,std::optional<RawClassifierCalibration> = {});
    template<class Geometry> static RawPixelClassificationPlan plan_geometry(const RawImageMetadata&,const Geometry&,const ScientificRawOptions&,const RawPixelClassifier&,std::size_t,std::optional<RawClassifierCalibration>);
    template<class Output,class Input,class Geometry> static std::shared_ptr<const Output> run_geometry(const Input&,const Geometry&,const RawPixelClassifier&,double,std::size_t,const std::atomic<bool>*,std::optional<RawClassifierCalibration>);
};
}
