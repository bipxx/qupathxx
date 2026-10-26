#pragma once
#include "qupathxx/raw_pixel_features.hpp"

// Private quantitative feature authority, pending the complete request/archive
// integration. No display conversion, automatic normalization or imputation.
namespace qpx::detail {
using RawFeatureScale=RawPixelFeatureScale;
using RawFeatureOptions=RawPixelFeatureOptions;
struct RawFeatureImage {
    std::int64_t width=0,height=0;
    std::vector<ClassifierFeature> features;
    std::vector<float> values; // HWC: channel, scale, gaussian/gradient/laplacian.
    std::vector<std::uint8_t> valid; // HWC feature support, not one shared channel mask.
    std::vector<std::uint8_t> rows_valid; // Forest admission requires every column.
    std::string input_sha256,sha256;
    std::size_t estimated_work_bytes=0;
};
std::vector<ClassifierFeature> raw_feature_schema(const ScientificRawGrid&,const RawFeatureOptions&);
std::vector<ClassifierFeature> raw_feature_schema(const RawImageMetadata&,const ScientificRawOptions&,const RawFeatureOptions&);
std::size_t raw_feature_work_bytes(const RawImageMetadata&,const ScientificGrid&,const ScientificRawOptions&,const RawFeatureOptions&);
std::shared_ptr<const RawFeatureImage> extract_raw_pixel_features(
    const ScientificRawGrid&,const RawFeatureOptions&,const std::atomic<bool>* cancelled=nullptr);
}
