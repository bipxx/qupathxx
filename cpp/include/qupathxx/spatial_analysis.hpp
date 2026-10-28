#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/provenance.hpp"
#include <thread>
#include <functional>

namespace qpx {
enum class SpatialDistanceUnits { Pixels, Micrometres };
struct SpatialDistanceOptions {
    SpatialDistanceUnits units=SpatialDistanceUnits::Pixels;
    bool prefer_nucleus=true,exclude_self=true;
    // Conservative input/capture and worker-payload admission, not RSS caps.
    std::size_t snapshot_bytes=64u*1024*1024,work_bytes=128u*1024*1024;
    std::uint64_t max_distance_tests=100000000;
    void validate() const;
};
struct SpatialObjectSnapshot {
    std::uint64_t id=0,revision=0;
    Point centroid;
    ObjectKind kind=ObjectKind::Annotation;
    bool nucleus=false;
    std::string geometry_sha256;
};
class SpatialDistanceResult;
// Capture/matches are owner-thread operations. The kernel uses only immutable
// centroid/identity/provenance values: no Document, ImageServer or pixel reads.
class SpatialDistanceRequest {
public:
    SpatialDistanceRequest(const Document&,const std::vector<std::uint64_t>& sources,
        const std::vector<std::uint64_t>& targets,SpatialDistanceOptions={});
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
    SpatialDistanceOptions options() const;
    const std::string& input_sha256() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    static SpatialDistanceRequest replay(const Document&,std::uint64_t record_id);
private:
    struct Data;
    std::shared_ptr<const Data> data_;
    std::shared_ptr<const char> identity_;
    std::thread::id owner_;
    void on_owner() const;
    friend struct SpatialDistanceKernel;
    friend class SpatialDistanceResult;
    friend class SpatialDistanceJob;
    friend struct SpatialDistanceArchiveCodec;
    friend class Document;
};
struct SpatialDistanceRow {
    std::uint64_t source_id=0,source_revision=0;
    Point source_centroid;
    ObjectKind source_kind=ObjectKind::Annotation;
    bool source_nucleus=false;
    std::optional<std::uint64_t> target_id,target_revision;
    std::optional<Point> target_centroid;
    std::optional<ObjectKind> target_kind;
    bool target_nucleus=false;
    std::optional<double> distance;
};
class SpatialDistanceResult {
public:
    const std::vector<SpatialDistanceRow>& rows() const {return rows_;}
    const char* algorithm() const {return "qpx.centroid-nearest/1";}
    const std::string& input_sha256() const;
    const std::string& sha256() const {return sha256_;}
    SpatialDistanceOptions options() const;
    double x_scale() const;
    double y_scale() const;
    std::uint64_t document_revision() const;
    std::uint64_t distance_tests() const {return distance_tests_;}
    std::size_t payload_bytes() const;
    double elapsed_ms() const {return elapsed_ms_;}
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    bool belongs_to(const SpatialDistanceRequest&) const;
    std::size_t source_count() const;
    std::size_t target_count() const;
    const SpatialObjectSnapshot& source_object(std::size_t) const;
    const SpatialObjectSnapshot& target_object(std::size_t) const;
private:
    SpatialDistanceResult()=default;
    std::shared_ptr<const SpatialDistanceRequest::Data> snapshot_;
    std::vector<SpatialDistanceRow> rows_;
    std::string sha256_;
    std::uint64_t distance_tests_=0;
    double elapsed_ms_=0;
    friend struct SpatialDistanceKernel;
    friend struct SpatialDistanceArchiveCodec;
    friend class SpatialDistanceRequest;
    friend class Document;
};
struct SpatialDistanceRecord {
    std::uint64_t id=0,captured_next_id=0;
    std::string name="Spatial distances",producer="qupathxx";
    std::shared_ptr<const SpatialDistanceResult> result;
    std::size_t payload_bytes() const;
};
// Row-wise, locale-independent float64 export; absent targets have empty numeric
// fields and an explicit no_target status. Historical tables may be exported.
std::string spatial_distance_csv(const SpatialDistanceResult&,std::size_t byte_limit=64u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);
std::size_t save_spatial_distance_csv(const SpatialDistanceResult&,const std::string& path,
    std::size_t byte_limit=64u*1024*1024,bool replace=false,const std::atomic<bool>* cancelled=nullptr,
    const std::function<void()>& publication_guard={});
std::shared_ptr<const SpatialDistanceResult> measure_spatial_distances(
    const SpatialDistanceRequest&,const std::atomic<bool>* cancelled=nullptr);
struct SpatialDistanceStats {
    std::string state="idle",error;
    std::uint64_t generation=0,completed_rows=0,total_rows=0,distance_tests=0;
    std::size_t snapshot_bytes=0,work_bytes=0;
    double capture_ms=0,elapsed_ms=0;
};
// One owner-controlled job, no backlog. busy() includes completed work awaiting
// owner collection. cancel() is thread-safe; destruction joins the worker.
class SpatialDistanceJob {
public:
    SpatialDistanceJob();~SpatialDistanceJob();
    SpatialDistanceJob(const SpatialDistanceJob&)=delete;
    SpatialDistanceJob& operator=(const SpatialDistanceJob&)=delete;
    bool request(const SpatialDistanceRequest&);
    bool busy() const;
    void cancel();
    SpatialDistanceStats poll(const Document&);
    SpatialDistanceStats wait(const Document&);
    // Wait for worker readiness without reading a live Document or collecting
    // its future. Bindings can release the GIL here, then poll with it held.
    void wait_work();
    std::shared_ptr<const SpatialDistanceResult> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
