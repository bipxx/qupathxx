#pragma once
#include "qupathxx/expression_dataset.hpp"
#include <array>
#include <optional>
namespace qpx {
enum class ExpressionCatalogueFormat { Qpx, Tenx };
struct ExpressionMatrixImportOptions {
    // Qpx: feature_id / observation_id single-column TSVs with headers.
    // Tenx: headerless ID/name/type features and single-column barcode TSV.
    ExpressionCatalogueFormat format=ExpressionCatalogueFormat::Qpx;
    std::uint64_t file_bytes=1024ull*1024*1024,store_bytes=2ull*1024*1024*1024;
    std::size_t work_bytes=256u*1024*1024;
    std::uint64_t max_features=1000000,max_observations=1000000,max_entries=100000000;
    void validate() const;
};
struct ExpressionMatrixMetadata {
    std::uint64_t features=0,observations=0,entries=0;
    ExpressionValueDomain domain=ExpressionValueDomain::Counts;
    std::string value_domain="raw-counts",input_profile="qpx-tsv",source_sha256;
    std::array<std::string,3> input_sha256; // matrix, features, observations
};
struct ExpressionMatrixImportResult {
    ExpressionMatrixMetadata metadata;
    std::string path,file_sha256;
    std::uint64_t file_bytes=0,matrix_bytes=0;
    std::size_t peak_work_bytes=0;
    double elapsed_ms=0;
};
struct ExpressionFeature {
    std::uint64_t index=0;
    std::string id,name,type,sha256;
};
struct ExpressionFeaturePage {
    std::vector<ExpressionFeature> features;
    std::uint64_t matches=0,offset=0;
};
struct ExpressionMatrixSelection {
    // Explicit unique feature indices, in requested order;1..4096 per read.
    std::vector<std::uint64_t> features;
    // nullopt means all observations. An empty vector means an empty cohort.
    // Explicit indices must be sorted and unique; sparse output uses global IDs.
    std::optional<std::vector<std::uint64_t>> observations;
};
struct ExpressionSparseBlock {
    std::vector<std::uint64_t> features,offsets,observation_indices,counts;
    std::vector<double> continuous;
    ExpressionValueDomain domain=ExpressionValueDomain::Counts;
    std::string source_sha256,sha256;
    // Offsets group entries by requested feature; within each, global observation
    // indices increase. Cohort reads stream/verify each selected gene chunk before
    // filtering; they never scan other genes. Absent entries are measured zeros, not missing values.
    // Explicit stored zeros may occur. Counts remain uint64 without conversion.
};
class ExpressionMatrixSource {
public:
    virtual ~ExpressionMatrixSource()=default;
    virtual const ExpressionMatrixMetadata& metadata() const=0;
    virtual const std::string& path() const=0;
    virtual const std::string& file_sha256() const=0;
    virtual std::size_t payload_bytes() const=0;
    virtual void require_current(const std::atomic<bool>* cancelled=nullptr) const=0;
    virtual std::vector<ExpressionFeature> feature_page(std::uint64_t first,std::size_t count,
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr) const=0;
    // Literal substring search over stable IDs and display names, ASCII case-
    // insensitive, with bounded results in source-axis order. No count rows read.
    virtual ExpressionFeaturePage search_features(const std::string& query,std::uint64_t offset,
        std::size_t count=32,std::size_t work_bytes=64u*1024*1024,
        const std::atomic<bool>* cancelled=nullptr) const=0;
    virtual std::vector<std::string> observation_page(std::uint64_t first,std::size_t count,
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr) const=0;
    virtual std::optional<std::uint64_t> feature_index(const std::string& id,const std::atomic<bool>* cancelled=nullptr) const=0;
    virtual std::optional<std::uint64_t> observation_index(const std::string& id,const std::atomic<bool>* cancelled=nullptr) const=0;
    // Exact ID join in requested order using one connection/prepared lookup.
    // Missing IDs refuse; repeated lookup IDs are allowed. No positional inference.
    virtual std::vector<std::uint64_t> observation_indices(const std::vector<std::string>& ids,
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr) const=0;
    virtual ExpressionSparseBlock read(const ExpressionMatrixSelection&,
        std::size_t work_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr) const=0;
};
bool has_expression_matrix_storage();
// Stream each input once. Refuse duplicate coordinates rather than silently sum;
// build privately and atomically publish a complete, new-only SQLite sidecar.
ExpressionMatrixImportResult import_expression_matrix(const std::string& matrix,
    const std::string& features,const std::string& observations,const std::string& store,
    const ExpressionMatrixImportOptions& = {},const std::atomic<bool>* cancelled=nullptr);
// Fingerprint sidecar bytes once on open; compare when an expected digest is supplied.
// Queries validate selected gene chunks and file versions, never reopen/rescan
// the original Matrix Market inputs. Retain the immutable source between queries.
std::shared_ptr<const ExpressionMatrixSource> open_expression_matrix(const std::string&,
    const std::string& expected_file_sha256="",std::size_t work_bytes=64u*1024*1024,
    std::uint64_t file_bytes=2ull*1024*1024*1024,const std::atomic<bool>* cancelled=nullptr);
}
