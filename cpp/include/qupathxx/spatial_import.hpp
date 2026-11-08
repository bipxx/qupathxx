#pragma once
#include "qupathxx/spatial_counts.hpp"
#include <memory>

namespace qpx {
struct SpatialImportRequest {
    std::string input_project,output_project,matrix,features,positions;
    std::uint64_t image_id=0;
    SpatialCountsOptions options;
    void validate() const;
};
class SpatialImportPreview {
public:
    const SpatialImportRequest& request() const;
    const SpatialCountsProjection& projection() const;
    const std::string& project_sha256() const;
    const std::string& coordinate_frame() const;
    const std::string& json() const;
private:
    struct Data;
    explicit SpatialImportPreview(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
    friend std::shared_ptr<const SpatialImportPreview> preview_spatial_import(const SpatialImportRequest&,const std::atomic<bool>*);
    friend std::string publish_spatial_import(const SpatialImportPreview&,const std::atomic<bool>*);
};
// Pure preview: captures owned values and validates the image/plane/output, but
// does not mutate a live study or publish a file. Strict level-zero pixel profile.
std::shared_ptr<const SpatialImportPreview> preview_spatial_import(const SpatialImportRequest&,
    const std::atomic<bool>* cancelled=nullptr);
// Reuses the parsed projection; stale input versions and output collisions refuse
// publication. The original project stays untouched.
std::string publish_spatial_import(const SpatialImportPreview&,const std::atomic<bool>* cancelled=nullptr);
enum class SpatialImportPhase { Idle, Reading, Ready, Publishing, Complete, Cancelled, Failed };
class SpatialImportJob {
public:
    SpatialImportJob();~SpatialImportJob();SpatialImportJob(const SpatialImportJob&)=delete;
    bool preview(SpatialImportRequest);
    bool publish(std::shared_ptr<const SpatialImportPreview>);
    void poll();void wait();void cancel();bool busy() const;
    SpatialImportPhase phase() const;
    std::shared_ptr<const SpatialImportPreview> result() const;
    const std::string& report() const;
    const std::string& error() const;
    void rethrow_error() const;
private:
    struct Data;std::unique_ptr<Data> data_;
};
}
