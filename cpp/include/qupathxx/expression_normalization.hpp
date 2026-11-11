#pragma once
#include "qupathxx/expression_matrix_project.hpp"
namespace qpx {
struct ExpressionFeatureGroup {
    std::string name;
    // Explicit, ascending unique source feature indices. Groups may overlap.
    std::vector<std::uint64_t> features;
};
struct ExpressionQcOptions {
    std::vector<ExpressionFeatureGroup> groups;
    std::size_t work_bytes=256u*1024*1024;
    void validate(std::uint64_t features) const;
};
class ExpressionQcResult {
public:
    const std::vector<std::uint64_t>& totals() const;
    const std::vector<std::uint64_t>& detected_features() const;
    const std::vector<ExpressionFeatureGroup>& groups() const;
    // Group-major, then source observation order. All totals remain exact uint64.
    const std::vector<std::uint64_t>& group_totals() const;
    std::optional<double> group_fraction(std::size_t group,std::uint64_t observation) const;
    std::uint64_t features() const;
    const std::string& source_sha256() const;
    const std::string& file_sha256() const;
    const std::string& sha256() const;
    std::size_t payload_bytes() const;
    // Conservative retained-state + indexed-read reservation, not allocator RSS.
    std::size_t peak_work_bytes() const;
    std::size_t batches() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    explicit ExpressionQcResult(std::shared_ptr<const Data>);
    friend std::shared_ptr<const ExpressionQcResult> expression_qc(
        std::shared_ptr<const ExpressionMatrixSource>,const ExpressionQcOptions&,const std::atomic<bool>*);
};
// One bounded pass over ALL features/observations of an exact raw-count source.
// Matrix sparse absence/explicit zero are measured zeros. This API does not accept
// a projected ExpressionDataset or infer missing values from absent sparse rows.
std::shared_ptr<const ExpressionQcResult> expression_qc(std::shared_ptr<const ExpressionMatrixSource>,
    const ExpressionQcOptions& = {},const std::atomic<bool>* cancelled=nullptr);
struct ExpressionNormalizationOptions {
    double target_sum=10000;
    bool log1p=true;
    void validate() const;
};
class ExpressionNormalization {
public:
    const std::shared_ptr<const ExpressionMatrixSource>& source() const;
    const std::shared_ptr<const ExpressionQcResult>& qc() const;
    const ExpressionNormalizationOptions& options() const;
    const std::string& recipe() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    explicit ExpressionNormalization(std::shared_ptr<const Data>);
    friend ExpressionNormalization normalize_expression(std::shared_ptr<const ExpressionMatrixSource>,
        std::shared_ptr<const ExpressionQcResult>,ExpressionNormalizationOptions,std::size_t,const std::atomic<bool>*);
};
// Reuse the retained QC for parameter/view changes. Derived sources have no raw
// sidecar locator/file digest: persist their recipe, not a raw matrix association.
// Raw uint64 buffers remain unchanged; derived double values are explicitly
// count/total*target, optionally log1p. A zero library yields valid zeros.
ExpressionNormalization normalize_expression(std::shared_ptr<const ExpressionMatrixSource>,
    std::shared_ptr<const ExpressionQcResult>,ExpressionNormalizationOptions={},
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
// Recompute QC once at a worker/replay boundary, verify the saved exact identities,
// then retain the resulting source for repeated selected-gene reads.
ExpressionNormalization replay_expression_normalization(std::shared_ptr<const ExpressionMatrixSource>,
    const std::string& recipe,std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
class Project;
// Cheap owner publication, using the existing raw-matrix association and undo.
std::string keep_expression_normalization(Project&,std::uint64_t image,const ExpressionNormalization&);
std::vector<std::string> expression_normalization_ids(const Project&,std::uint64_t image);
std::vector<std::string> expression_normalization_ids(const Project&,std::uint64_t image,
    const std::string& parent_source_sha256);
std::string expression_normalization_recipe(const Project&,std::uint64_t image,const std::string& id);
void remove_expression_normalization(Project&,std::uint64_t image,const std::string& id);
struct ExpressionNormalizationOpenRequest {
    ExpressionMatrixOpenRequest parent;
    std::string recipe;
    ExpressionNormalization open(std::size_t work_bytes=256u*1024*1024,
        const std::atomic<bool>* cancelled=nullptr) const;
};
ExpressionNormalizationOpenRequest expression_normalization_open_request(
    const Project&,std::uint64_t image,const std::string& id);
}
