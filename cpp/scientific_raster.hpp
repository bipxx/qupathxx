#pragma once
#include "qupathxx/tissue.hpp"
#include "qupathxx/scientific_sampling.hpp"

namespace qpx {
// Internal construction for the additive cell recipe, never a way to put an
// explicit grid into QTM1/2 or QCL1-4. Canonical input identity is constructed
// from the retained decoded snapshot, never a second read of mutable input.
struct ScientificRasterSpec {
    TissueParameters parameters;
    std::shared_ptr<const char> ticket;
    std::string input_sha256; // optional expected digest; a mismatch rejects
    bool body=false;
    std::uint64_t threshold_pixels=0,raw_components=0;
    ScientificRasterSpec() {parameters.level=0;}
};
// Detector geometry needs distinct, interior-representable world centers and
// edges. The request planner can call this before any IO or raster allocation.
void validate_scientific_raster_geometry(const ScientificGrid&,const std::atomic<bool>* cancelled=nullptr);
bool scientific_sample_eligible(const AnalysisRequest&,const ScientificRgbGrid&,std::size_t x,std::size_t y);
bool scientific_core_roi_contains(const Roi&,const ScientificGrid&,RoiSamplingPolicy,std::size_t x,std::size_t y);
std::vector<TissueComponent> scientific_raster_components(const ScientificGrid&,const std::vector<std::uint32_t>&,std::uint32_t count,const std::atomic<bool>* cancelled=nullptr);
std::uint64_t scientific_raster_work_bytes(const ScientificGrid&,std::uint32_t count,const Roi&,const AnalysisProvenance&);
std::string scientific_raster_input_sha256(const AnalysisRequest&,const ScientificRgbGrid&,const std::atomic<bool>* cancelled=nullptr);
struct ScientificRasterKernel {
    static std::shared_ptr<const TissueResult> make(const AnalysisRequest&,const ScientificRgbGrid&,
        std::vector<std::uint32_t> labels,const ScientificRasterSpec&,const std::atomic<bool>* cancelled=nullptr);
};
}
