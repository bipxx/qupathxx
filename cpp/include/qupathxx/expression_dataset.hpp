#pragma once
#include "qupathxx/geometry.hpp"
#include "qupathxx/object_plane.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace qpx {
class Document;
enum class ExpressionValueDomain { Counts, Continuous };
struct ExpressionObservation {
    // Zero means the observation is not materialized as a Document object.
    std::uint64_t object_id=0;
    std::string id;
    Point position;
};
struct ExpressionDatasetInput {
    std::string source_id,coordinate_frame;
    ObjectPlane plane;
    ExpressionValueDomain domain=ExpressionValueDomain::Counts;
    std::vector<ExpressionObservation> observations;
    std::vector<std::string> features;
    // Observation-major. Exactly one value buffer is populated according to domain.
    std::vector<std::uint64_t> counts;
    std::vector<double> continuous;
    // Empty means all values are measured. Otherwise one0/1 byte per value.
    std::vector<std::uint8_t> valid;
};
struct ExpressionCaptureOptions {
    std::string coordinate_frame;
    ObjectPlane plane;
    // Empty selects the single eligible lineage. Multiple lineages require a choice.
    std::string source_id;
    std::vector<std::uint64_t> object_ids;
    // Empty discovers the union. A requested absent feature stays missing, not zero.
    std::vector<std::string> features;
    std::size_t work_bytes=64u*1024*1024;
};
class ExpressionDataset {
public:
    static ExpressionDataset create(ExpressionDatasetInput,
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
    // Owner-thread capture of the existing qpx.spatial-counts/1 point projection.
    // No live Document, decoder or UI reference survives in the returned snapshot.
    static ExpressionDataset capture(const Document&,const ExpressionCaptureOptions&,
        const std::atomic<bool>* cancelled=nullptr);
    static std::vector<std::string> sources(const Document&,ObjectPlane={},
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
    ExpressionValueDomain domain() const;
    const std::string& source_id() const;
    const std::string& coordinate_frame() const;
    ObjectPlane plane() const;
    const std::vector<ExpressionObservation>& observations() const;
    const std::vector<std::string>& features() const;
    const std::vector<std::uint64_t>& counts() const;
    const std::vector<double>& continuous() const;
    const std::vector<std::uint8_t>& valid() const;
    std::size_t feature_index(const std::string&) const;
    const std::string& sha256() const;
    // Conservative accounted buffers/validation storage, including vector
    // capacities. This is not a claim about allocator/process RSS.
    std::size_t payload_bytes() const;
private:
    struct Data;
    explicit ExpressionDataset(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
};
}
