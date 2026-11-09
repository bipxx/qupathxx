#pragma once
#include "qupathxx/expression_matrix.hpp"
#include "qupathxx/registration.hpp"
namespace qpx {
inline constexpr const char* expression_matrix_point_profile="qpx.expression-matrix-points/1";
struct ExpressionMatrixSpatialScope {
    std::string matrix_source_sha256,frame_id;
    ObjectPlane plane;std::size_t observations=0;
    std::string sha256() const;
};
std::vector<ExpressionMatrixSpatialScope> expression_matrix_spatial_scopes(const Document&,
    ObjectPlane={},std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
class ObjectProperties;
class ExpressionMatrixSpatialRequest;
// Immutable, explicit coordinate/ID join. No counts are stored in point objects.
class ExpressionMatrixSpatialSnapshot {
public:
    static ExpressionMatrixSpatialSnapshot create(std::shared_ptr<const ExpressionMatrixSource>,
        std::vector<ExpressionObservation>,CoordinateFrame,std::size_t work_bytes=64u*1024*1024,
        const std::atomic<bool>* cancelled=nullptr);
    // Owner capture of qpx.expression-matrix-points/1 detections on one plane.
    // The current native image frame must match their retained explicit frame.
    static ExpressionMatrixSpatialSnapshot capture(const Document&,
        std::shared_ptr<const ExpressionMatrixSource>,const std::string& frame_id,
        ObjectPlane={},std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
    const std::shared_ptr<const ExpressionMatrixSource>& source() const;
    const std::vector<ExpressionObservation>& observations() const;
    const CoordinateFrame& frame() const;
    // The derived expression lineage binds matrix, coordinates, units and cohort.
    // Use this frame with the projected dataset in existing registration services.
    CoordinateFrame dataset_frame() const;
    const std::string& sha256() const;
    std::size_t payload_bytes() const;
    // Only the requested features are projected densely for existing display /
    // registration services; the full matrix remains sparse and disk-backed.
    ExpressionDataset dataset(const std::vector<std::uint64_t>& features,
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr) const;
private:
    struct Data;
    explicit ExpressionMatrixSpatialSnapshot(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
};
// Owner capture copies only document facts. Axis joins/file IO run in resolve()
// on an existing native worker; no live Document survives this request.
class ExpressionMatrixSpatialRequest {
public:
    ExpressionMatrixSpatialRequest(std::shared_ptr<const ExpressionMatrixSource>,
        std::vector<ExpressionObservation>,CoordinateFrame,std::size_t work_bytes=64u*1024*1024);
    static ExpressionMatrixSpatialRequest capture(const Document&,
        std::shared_ptr<const ExpressionMatrixSource>,const std::string& frame_id,
        ObjectPlane={},std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
    ExpressionMatrixSpatialSnapshot resolve(const std::atomic<bool>* cancelled=nullptr) const;
    ExpressionMatrixSpatialSnapshot resolve(std::size_t work_bytes,const std::atomic<bool>* cancelled) const;
    std::size_t payload_bytes() const;
    const std::shared_ptr<const ExpressionMatrixSource>& source() const;
    const std::string& sha256() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
};
// Native point-import vocabulary: identity and explicit level-zero image frame,
// not sample values. Coordinates are supplied separately to existing Roi/commands.
std::shared_ptr<const ObjectProperties> expression_matrix_point_properties(
    const std::string& source_sha256,const std::string& observation_id,const CoordinateFrame&);
bool expression_matrix_points_reference(const Document&,const std::string& source_sha256);
}
