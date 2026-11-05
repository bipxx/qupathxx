#pragma once
#include "qupathxx/geometry.hpp"
#include "qupathxx/provenance.hpp"
#include "qupathxx/stains.hpp"
#include "qupathxx/cell_statistics.hpp"
namespace qpx {
enum class AnalysisKind { OpticalDensityMeans, StainMeans };
const char* roi_analysis_algorithm(RoiSamplingPolicy);
const char* roi_statistics_algorithm(RoiSamplingPolicy);
RoiSamplingPolicy roi_analysis_sampling_policy(const std::string& algorithm);
const char* analysis_input_format(RoiSamplingPolicy,bool pyramid=false);
struct AnalysisResult {
    std::string algorithm="qpx.roi-channel-means/1",input_sha256;
    std::uint64_t object_id=0,object_revision=0,document_revision=0,samples=0,tiles=0;
    AnalysisKind kind=AnalysisKind::OpticalDensityMeans;
    bool nucleus=false;
    ColorVector means{},white{};
    std::array<ColorVector,3> stains{};
    double elapsed_ms=0;
    std::shared_ptr<const AnalysisProvenance> provenance;
    std::shared_ptr<const CompartmentIntensityStatistics> intensity_statistics;
    RoiSamplingPolicy sampling_policy() const {return roi_analysis_sampling_policy(algorithm);}
};
namespace detail {
inline std::size_t analysis_statistics_bytes(const AnalysisResult& r) {
    return r.intensity_statistics?sizeof(CompartmentIntensityStatistics)+2*sizeof(std::shared_ptr<const CompartmentIntensityStatistics>):0;
}
}
struct AnalysisRecord {
    std::uint64_t id=0;
    AnalysisResult result;
    Roi roi;
    std::size_t tile_size=256;
    void validate(const ImageMetadata&) const;
    std::size_t payload_bytes() const;
};
}
