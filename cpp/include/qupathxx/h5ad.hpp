#pragma once
#include "qupathxx/expression_matrix.hpp"
#include "qupathxx/registration.hpp"
#include <functional>
namespace qpx {
struct H5adOptions {
    std::string matrix="X",spatial="obsm/spatial";
    // Never infer the domain of X/layers, units or registration from the filename.
    std::optional<ExpressionValueDomain> domain;
    std::string value_domain;
    CoordinateFrame frame;
    std::uint64_t file_bytes=1024ull*1024*1024,max_features=1000000,max_observations=1000000,max_entries=100000000;
    std::size_t work_bytes=256u*1024*1024,chunk_bytes=16u*1024*1024;
    void validate() const;
};
struct H5adEntry {std::uint64_t observation=0,feature=0,count=0;double continuous=0;};
struct H5adMetadata {
    ExpressionValueDomain domain=ExpressionValueDomain::Counts;
    std::string value_domain,encoding,input_sha256,source_sha256;
    CoordinateFrame frame;
    // Exclusion summary; optional omitted elements are not fully validated.
    std::vector<std::string> features,observations,ignored_metadata;
    std::vector<Point> positions;
    std::uint64_t entries=0;
    std::size_t payload_bytes=0;
};
class H5adReader {
public:
    H5adReader(const std::string&,H5adOptions,const std::atomic<bool>* cancelled=nullptr);
    const H5adMetadata& metadata() const;
    // Conservative decoder reservation for composing with an indexed writer.
    // Includes catalogue/cache/block/chunk work; excludes allocator/process RSS.
    std::size_t stream_work_bytes() const;
    void require_current(const std::atomic<bool>* cancelled=nullptr) const;
    // Native streaming sink; no retained matrix or Python callback. Entries are
    // observation-major for dense/CSR, feature-major for CSC. Missing sparse
    // coordinates are measured zeros; duplicate stored coordinates refuse.
    // A sink must stage output and publish only after stream returns successfully.
    void stream(const std::function<void(const H5adEntry&)>&,const std::atomic<bool>* cancelled=nullptr) const;
private:
    struct Data;std::shared_ptr<const Data> data_;
};
bool has_h5ad();
// One new-only transactional sidecar; decoder and writer share limits.work_bytes.
// Embedded coordinates/frame survive removing the original H5AD input.
ExpressionMatrixImportResult import_h5ad_matrix(const std::string& input,const std::string& store,
    H5adOptions,const ExpressionMatrixImportOptions& limits={},const std::atomic<bool>* cancelled=nullptr);
}
