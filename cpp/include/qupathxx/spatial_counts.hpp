#pragma once
#include "qupathxx/geometry.hpp"
#include <atomic>
#include <array>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace qpx {
struct SpatialCountsOptions {
    std::vector<std::string> features;
    std::uint32_t z=0,t=0;
    std::size_t file_bytes=32u*1024*1024,work_bytes=128u*1024*1024;
    std::uint32_t max_features=100000,max_observations=10000;
    std::uint64_t max_entries=1000000;
    void validate() const;
};
struct SpatialObservation {std::string id;Point position;};
struct SpatialCountsProjection {
    std::uint32_t matrix_features=0;
    std::uint64_t matrix_entries=0;
    std::vector<std::string> features;
    std::vector<SpatialObservation> observations;
    // Observation-major selected-gene counts, including explicit sparse zeros.
    std::vector<std::uint64_t> counts;
    // Exact owned input bytes: matrix, feature table, position table.
    std::array<std::string,3> source_sha256;
};
SpatialCountsProjection read_spatial_counts(const std::string& matrix,
    const std::string& features,const std::string& positions,
    const SpatialCountsOptions&,const std::atomic<bool>* cancelled=nullptr);
// Import selected genes as historical measurements on point detections in a NEW
// project. No coordinate inference, registration or full-matrix persistence.
std::string import_spatial_counts(const std::string& input_project,
    const std::string& output_project,std::uint64_t image_id,
    const std::string& matrix,const std::string& features,const std::string& positions,
    const SpatialCountsOptions&,const std::atomic<bool>* cancelled=nullptr);
}
