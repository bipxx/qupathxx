#pragma once
#include "qupathxx/spatial_counts.hpp"
#include "qupathxx/registration.hpp"
#include "qupathxx/expression_matrix.hpp"
#include "qupathxx/expression_matrix_project.hpp"
#include <memory>

namespace qpx {
class ImageServer;
struct BackedSpatialImportRequest;
class BackedSpatialImportPreview;
struct SpatialImportRequest {
    std::string input_project,output_project,matrix,features,positions;
    std::uint64_t image_id=0;
    SpatialCountsOptions options;
    // Exactly one base: saved native project or an admitted image (PPM/TIFF).
    std::string image_path,image_reader="auto";
    std::optional<std::uint32_t> image_series;
    CoordinateUnit position_unit=CoordinateUnit::Pixel;
    Affine2D alignment; // applied after explicit calibrated unit conversion
    std::string expected_base_sha256,expected_feature_sha256;
    void validate() const;
};
class SpatialImportPreview {
public:
    const SpatialImportRequest& request() const;
    const SpatialCountsProjection& projection() const;
    const std::string& project_sha256() const;
    const std::string& input_sha256() const;
    const std::string& coordinate_frame() const;
    const std::vector<Point>& mapped_positions() const;
    const CoordinateFrame& image_frame() const;
    const Affine2D& coordinate_transform() const;
    std::shared_ptr<ImageServer> image_server() const;
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
struct SpatialImportImageInfo {std::uint64_t id;std::string name;std::int64_t width,height;};
struct SpatialImportBaseCatalogue {std::vector<SpatialImportImageInfo> images;std::string source_sha256;};
SpatialImportBaseCatalogue spatial_import_base_catalogue(const std::string& project,
    const std::atomic<bool>* cancelled=nullptr);
enum class SpatialImportPhase { Idle, Reading, Ready, Publishing, Complete, Cancelled, Failed, CatalogueReading, CatalogueReady, BaseReading, BaseReady,
    MatrixOpening,MatrixIndexing,MatrixReady,MatrixCatalogueReading,MatrixCatalogueReady };
class SpatialImportJob {
public:
    SpatialImportJob();~SpatialImportJob();SpatialImportJob(const SpatialImportJob&)=delete;
    bool preview(SpatialImportRequest);
    bool publish(std::shared_ptr<const SpatialImportPreview>);
    bool preview(BackedSpatialImportRequest);
    bool preview(BackedSpatialImportRequest,std::shared_ptr<const ExpressionMatrixSource>);
    bool publish(std::shared_ptr<const BackedSpatialImportPreview>);
    bool catalogue(const std::string& features,const SpatialCountsOptions& limits={});
    bool base_catalogue(const std::string& project);
    bool open_matrix(const std::string&,const std::string& expected_sha256={},std::size_t work_bytes=64u*1024*1024);
    bool open_matrix(ExpressionMatrixOpenRequest,std::size_t work_bytes=64u*1024*1024);
    bool index_matrix(const std::string& matrix,const std::string& features,const std::string& observations,
        const std::string& store,ExpressionMatrixImportOptions={});
    bool matrix_catalogue(std::shared_ptr<const ExpressionMatrixSource>,std::string query,
        std::uint64_t offset=0,std::size_t count=32,std::size_t work_bytes=64u*1024*1024);
    void poll();void wait();void cancel();bool busy() const;
    SpatialImportPhase phase() const;
    std::shared_ptr<const SpatialImportPreview> result() const;
    std::shared_ptr<const BackedSpatialImportPreview> backed_result() const;
    std::shared_ptr<const ExpressionMatrixSource> matrix_source() const;
    std::shared_ptr<const ExpressionMatrixImportResult> matrix_index_result() const;
    std::shared_ptr<const ExpressionFeaturePage> matrix_catalogue_result() const;
    std::shared_ptr<const SpatialFeatureCatalogue> catalogue_result() const;
    std::shared_ptr<const SpatialImportBaseCatalogue> base_result() const;
    const std::string& report() const;
    const std::string& error() const;
    void rethrow_error() const;
private:
    struct Data;std::unique_ptr<Data> data_;
};
}
