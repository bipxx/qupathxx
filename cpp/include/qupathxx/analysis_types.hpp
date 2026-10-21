#pragma once
#include "qupathxx/geometry.hpp"
#include "qupathxx/stains.hpp"
namespace qpx {
enum class AnalysisKind { OpticalDensityMeans, StainMeans };
struct AnalysisResult {
    std::string algorithm="qpx.roi-channel-means/1",input_sha256;
    std::uint64_t object_id=0,object_revision=0,document_revision=0,samples=0,tiles=0;
    AnalysisKind kind=AnalysisKind::OpticalDensityMeans;
    bool nucleus=false;
    ColorVector means{},white{};
    std::array<ColorVector,3> stains{};
    double elapsed_ms=0;
};
struct AnalysisRecord {
    std::uint64_t id=0;
    AnalysisResult result;
    Roi roi;
    std::size_t tile_size=256;
    void validate(const ImageMetadata&) const;
    std::size_t payload_bytes() const;
};
}
