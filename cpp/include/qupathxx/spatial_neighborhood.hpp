#pragma once
#include "qupathxx/spatial_analysis.hpp"

namespace qpx {
struct SpatialNeighborhoodOptions : SpatialDistanceOptions {
    double radius=50;
    void validate() const;
};
class SpatialNeighborhoodResult;
// Reuses the immutable centroid capture authority of SpatialDistanceRequest.
// No reader/pixels or live Document is accessed by the worker.
class SpatialNeighborhoodRequest {
public:
    SpatialNeighborhoodRequest(const Document&,const std::vector<std::uint64_t>& sources,
        const std::vector<std::uint64_t>& targets,SpatialNeighborhoodOptions={});
    bool matches(const Document&) const;
    std::size_t source_count() const;
    std::size_t target_count() const;
    const SpatialObjectSnapshot& source_object(std::size_t) const;
    const SpatialObjectSnapshot& target_object(std::size_t) const;
    std::size_t snapshot_bytes() const;
    std::size_t required_work_bytes() const;
    std::uint64_t document_revision() const;
    double capture_ms() const;
    double x_scale() const;
    double y_scale() const;
    SpatialNeighborhoodOptions options() const;
    const std::string& input_sha256() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    static SpatialNeighborhoodRequest replay(const Document&,std::uint64_t record_id);
private:
    struct Data;
    std::shared_ptr<const Data> data_;
    std::thread::id owner_=std::this_thread::get_id();
    void on_owner() const;
    friend struct SpatialNeighborhoodKernel;
    friend class SpatialNeighborhoodResult;
    friend class SpatialNeighborhoodJob;
    friend struct SpatialNeighborhoodArchiveCodec;
    friend class Document;
};
struct SpatialNeighborhoodRow {
    std::uint64_t source_id=0,source_revision=0;
    Point source_centroid;
    ObjectKind source_kind=ObjectKind::Annotation;
    bool source_nucleus=false;
    std::uint64_t count=0;
};
class SpatialNeighborhoodResult {
public:
    const std::vector<SpatialNeighborhoodRow>& rows() const {return rows_;}
    const char* algorithm() const {return "qpx.centroid-radius-count/1";}
    const std::string& input_sha256() const;
    const std::string& sha256() const {return sha256_;}
    SpatialNeighborhoodOptions options() const;
    double x_scale() const;
    double y_scale() const;
    std::uint64_t document_revision() const;
    std::uint64_t distance_tests() const {return distance_tests_;}
    std::size_t payload_bytes() const;
    double elapsed_ms() const {return elapsed_ms_;}
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    bool belongs_to(const SpatialNeighborhoodRequest&) const;
    std::size_t source_count() const;
    std::size_t target_count() const;
    const SpatialObjectSnapshot& source_object(std::size_t) const;
    const SpatialObjectSnapshot& target_object(std::size_t) const;
private:
    SpatialNeighborhoodResult()=default;
    std::shared_ptr<const SpatialNeighborhoodRequest::Data> snapshot_;
    std::vector<SpatialNeighborhoodRow> rows_;
    std::string sha256_;
    std::uint64_t distance_tests_=0;
    double elapsed_ms_=0;
    friend struct SpatialNeighborhoodKernel;
    friend struct SpatialNeighborhoodArchiveCodec;
    friend class SpatialNeighborhoodRequest;
    friend class Document;
};
struct SpatialNeighborhoodRecord {
    std::uint64_t id=0,captured_next_id=0;
    std::string name="Spatial neighborhoods",producer="qupathxx";
    std::shared_ptr<const SpatialNeighborhoodResult> result;
    std::size_t payload_bytes() const;
};
std::shared_ptr<const SpatialNeighborhoodResult> measure_spatial_neighborhoods(
    const SpatialNeighborhoodRequest&,const std::atomic<bool>* cancelled=nullptr);
using SpatialNeighborhoodStats=SpatialDistanceStats;
class SpatialNeighborhoodJob {
public:
    SpatialNeighborhoodJob();~SpatialNeighborhoodJob();
    SpatialNeighborhoodJob(const SpatialNeighborhoodJob&)=delete;
    SpatialNeighborhoodJob& operator=(const SpatialNeighborhoodJob&)=delete;
    bool request(const SpatialNeighborhoodRequest&);
    bool busy() const;
    void cancel();
    SpatialNeighborhoodStats poll(const Document&);
    SpatialNeighborhoodStats wait(const Document&);
    void wait_work();
    std::shared_ptr<const SpatialNeighborhoodResult> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
