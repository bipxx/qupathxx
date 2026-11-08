#pragma once
#include "qupathxx/spatial_analysis.hpp"
namespace qpx {
struct SpatialDensityOptions {
    bool prefer_nucleus=true,polygonize_curves=false;
    std::size_t snapshot_bytes=64u*1024*1024,work_bytes=128u*1024*1024;
    std::uint64_t max_membership_tests=100000000,max_geometry_operations=10000;
    std::size_t max_geometry_vertices=10000;
    void validate() const;
};
struct SpatialDensityAreaSnapshot {
    std::uint64_t id=0,revision=0;
    Roi geometry;
    std::string geometry_sha256;
};
class SpatialDensityResult;
class SpatialDensityRequest {
public:
    SpatialDensityRequest(const Document&,const std::vector<std::uint64_t>& windows,
        const std::vector<std::uint64_t>& tissue,const std::vector<std::uint64_t>& excluded,
        const std::vector<std::uint64_t>& cells,SpatialDensityOptions={});
    bool matches(const Document&) const;
    std::size_t window_count() const;
    std::size_t tissue_count() const;
    std::size_t exclusion_count() const;
    std::size_t cell_count() const;
    const SpatialDensityAreaSnapshot& window_object(std::size_t) const;
    const SpatialDensityAreaSnapshot& tissue_object(std::size_t) const;
    const SpatialDensityAreaSnapshot& exclusion_object(std::size_t) const;
    const SpatialObjectSnapshot& cell_object(std::size_t) const;
    std::size_t snapshot_bytes() const;
    std::size_t required_work_bytes() const;
    std::uint64_t document_revision() const;
    double capture_ms() const;
    double x_scale() const;
    double y_scale() const;
    SpatialDensityOptions options() const;
    const std::string& input_sha256() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    static SpatialDensityRequest replay(const Document&,std::uint64_t record_id);
private:
    struct Data;std::shared_ptr<const Data> data_;
    std::thread::id owner_=std::this_thread::get_id();
    void on_owner() const;
    friend struct SpatialDensityKernel;
    friend class SpatialDensityResult;
    friend class SpatialDensityJob;
    friend struct SpatialDensityArchiveCodec;
    friend class Document;
};
struct SpatialDensityRow {
    std::uint64_t window_id=0,window_revision=0,count=0;
    double area_pixels=0,area_mm2=0;
    std::optional<double> density_per_mm2;
    std::optional<Roi> footprint;
    std::string geometry_sha256;
};
class SpatialDensityResult {
public:
    const std::vector<SpatialDensityRow>& rows() const {return rows_;}
    const char* algorithm() const {return "qpx.roi-tissue-density/1";}
    const char* geometry_policy() const {return "qpx.polygon512-closed-exclusion/1";}
    const std::string& input_sha256() const;
    const std::string& sha256() const {return sha256_;}
    SpatialDensityOptions options() const;
    double x_scale() const;
    double y_scale() const;
    std::uint64_t document_revision() const;
    std::uint64_t membership_tests() const {return membership_tests_;}
    std::uint64_t geometry_operations() const {return geometry_operations_;}
    std::size_t payload_bytes() const;
    double elapsed_ms() const {return elapsed_ms_;}
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    bool belongs_to(const SpatialDensityRequest&) const;
    std::size_t window_count() const;
    std::size_t tissue_count() const;
    std::size_t exclusion_count() const;
    std::size_t cell_count() const;
    const SpatialDensityAreaSnapshot& window_object(std::size_t) const;
    const SpatialDensityAreaSnapshot& tissue_object(std::size_t) const;
    const SpatialDensityAreaSnapshot& exclusion_object(std::size_t) const;
    const SpatialObjectSnapshot& cell_object(std::size_t) const;
private:
    SpatialDensityResult()=default;
    std::shared_ptr<const SpatialDensityRequest::Data> snapshot_;
    std::vector<SpatialDensityRow> rows_;
    std::string sha256_;
    std::uint64_t membership_tests_=0,geometry_operations_=0;
    double elapsed_ms_=0;
    std::string result_digest() const;
    friend struct SpatialDensityKernel;
    friend struct SpatialDensityArchiveCodec;
    friend class SpatialDensityRequest;
    friend class Document;
};
struct SpatialDensityRecord {
    std::uint64_t id=0,captured_next_id=0;
    std::string name="Tissue cell density",producer="qupathxx";
    std::shared_ptr<const SpatialDensityResult> result;
    std::size_t payload_bytes() const;
};
struct SpatialDensityStats {
    std::string state="idle",error;
    std::uint64_t generation=0,completed_rows=0,total_rows=0,membership_tests=0,geometry_operations=0;
    std::size_t snapshot_bytes=0,work_bytes=0;
    double capture_ms=0,elapsed_ms=0;
};
std::shared_ptr<const SpatialDensityResult> measure_spatial_density(const SpatialDensityRequest&,const std::atomic<bool>* cancelled=nullptr);
std::string spatial_density_csv(const SpatialDensityResult&,std::size_t byte_limit=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
std::size_t save_spatial_density_csv(const SpatialDensityResult&,const std::string&,std::size_t byte_limit=64u*1024*1024,
    bool replace=false,const std::atomic<bool>* cancelled=nullptr,const std::function<void()>& publication_guard={});
class SpatialDensityJob {
public:
    SpatialDensityJob();~SpatialDensityJob();
    SpatialDensityJob(const SpatialDensityJob&)=delete;
    SpatialDensityJob& operator=(const SpatialDensityJob&)=delete;
    bool request(const SpatialDensityRequest&);
    bool busy() const;void cancel();
    SpatialDensityStats poll(const Document&);
    SpatialDensityStats wait(const Document&);
    void wait_work();
    std::shared_ptr<const SpatialDensityResult> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
