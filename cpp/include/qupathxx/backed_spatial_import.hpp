#pragma once
#include "qupathxx/spatial_import_base.hpp"
#include "qupathxx/spatial_counts.hpp"
#include "qupathxx/expression_matrix_spatial.hpp"
namespace qpx {
class ImageServer;
struct BackedSpatialImportRequest {
    SpatialImportBaseRequest base;
    // Empty positions selects the store's embedded coordinates and declared frame.
    std::string store,positions,expected_base_sha256,expected_store_sha256;
    SpatialPositionOptions position_options;
    CoordinateUnit position_unit=CoordinateUnit::Pixel;
    Affine2D alignment;
    bool owned=true;
    std::uint64_t store_bytes=2ull*1024*1024*1024;
    void validate() const;
};
class BackedSpatialImportPreview {
public:
    const BackedSpatialImportRequest& request() const;
    const ExpressionMatrixSpatialSnapshot& spatial() const;
    const CoordinateFrame& image_frame() const;
    const Affine2D& coordinate_transform() const;
    const std::vector<Point>& mapped_positions() const;
    const std::string& position_sha256() const;
    const std::string& input_sha256() const;
    const std::string& recipe_sha256() const;
    const std::string& json() const;
    std::shared_ptr<ImageServer> image_server() const;
private:
    struct Data;
    explicit BackedSpatialImportPreview(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
    friend std::shared_ptr<const BackedSpatialImportPreview> preview_backed_spatial_import(const BackedSpatialImportRequest&,const std::atomic<bool>*);
    friend std::shared_ptr<const BackedSpatialImportPreview> preview_backed_spatial_import(const BackedSpatialImportRequest&,std::shared_ptr<const ExpressionMatrixSource>,const std::atomic<bool>*);
    friend std::string publish_backed_spatial_import(const BackedSpatialImportPreview&,const std::atomic<bool>*);
};
// No output publication during preview. Barcode joins, coordinate conversion,
// bounds and matrix identity are native; all count values stay in the sidecar.
std::shared_ptr<const BackedSpatialImportPreview> preview_backed_spatial_import(
    const BackedSpatialImportRequest&,const std::atomic<bool>* cancelled=nullptr);
// Reuse an already opened immutable source; exact locator/digest/version checks
// prevent stale preview without fingerprinting the same file again.
std::shared_ptr<const BackedSpatialImportPreview> preview_backed_spatial_import(
    const BackedSpatialImportRequest&,std::shared_ptr<const ExpressionMatrixSource>,const std::atomic<bool>* cancelled=nullptr);
// Reuses the prepared join; source/version/output checks precede atomic new-file
// publication. Owned sidecars are copied before the final project commit.
std::string publish_backed_spatial_import(const BackedSpatialImportPreview&,
    const std::atomic<bool>* cancelled=nullptr);
}
