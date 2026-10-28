#pragma once
#include "qupathxx/analysis.hpp"
namespace qpx {
struct StainEstimateParameters {
    double min_od=.05,trim_quantile=.01,min_angle_degrees=5,max_residual_fraction=.05;
    std::size_t sample_limit=200000,min_samples=64,tile_size=256,work_bytes=64*1024*1024;
    std::uint64_t seed=0,max_read_pixels=16*1024*1024;
    std::int32_t level=-1;
    void validate() const;
};
class StainEstimateRequest {
public:
    StainEstimateRequest(const Document&,std::uint64_t object_id,StainEstimateParameters={},ColorVector white={255,255,255},bool nucleus=false);
    bool matches(const Document& d,std::uint64_t id) const {return source_.matches(d,id);}
    std::uint64_t object_id() const {return source_.object_id();}
    std::uint64_t total_tiles() const {return total_tiles_;}
    const StainEstimateParameters& parameters() const {return parameters_;}
    const std::shared_ptr<const AnalysisProvenance>& provenance() const {return source_.provenance();}
    std::size_t estimated_work_bytes() const {return estimated_work_bytes_;}
    std::int32_t level() const {return level_;}
    double downsample() const {return downsample_;}
private:
    friend struct StainEstimateKernel;
    StainEstimateParameters parameters_;AnalysisRequest source_;
    std::int64_t x0_=0,y0_=0,x1_=0,y1_=0;
    std::uint64_t total_tiles_=0;std::size_t capacity_=0,estimated_work_bytes_=0;
    std::int32_t level_=0;double downsample_=1;
};
struct StainEstimateResult {
    std::string algorithm="qpx.od-plane-stains/1",input_sha256,model_sha256;
    std::array<ColorVector,3> stains{};ColorVector white{},eigenvalues{};
    StainEstimateParameters parameters;Roi roi;
    std::shared_ptr<const AnalysisProvenance> provenance;
    std::uint64_t samples=0,eligible=0,sampled=0,tiles=0;
    std::size_t estimated_work_bytes=0;std::int32_t level=0;
    double downsample=1,angle_degrees=0,residual_fraction=0,elapsed_ms=0;
    StainTransform transform() const;
    void validate() const;
};
StainEstimateResult estimate_stains(const StainEstimateRequest&,const std::atomic<bool>* cancelled=nullptr);
std::string stain_model_sha256(const StainTransform&);
std::string stain_estimate_json(const StainEstimateResult&);
void save_stain_estimate(const StainEstimateResult&,const std::string& path);
class StainEstimateJob {
public:
    StainEstimateJob();~StainEstimateJob();StainEstimateJob(const StainEstimateJob&)=delete;
    bool request(const StainEstimateRequest&);
    AnalysisStats poll(const Document&,std::uint64_t selected_id);
    AnalysisStats wait(const Document&,std::uint64_t selected_id);
    // Native readiness only: no Document access, delivery or captured-owner release.
    void await_completion();
    void cancel();bool busy() const;
    std::optional<StainEstimateResult> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
