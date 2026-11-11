#pragma once
#include "qupathxx/expression_normalization.hpp"

namespace qpx {
struct ExpressionReductionOptions {
    std::vector<std::uint64_t> features; // Empty: complete source feature universe.
    std::string normalization_recipe;    // Empty: explicitly declared source values.
    bool center=true;                    // false names uncentered truncated SVD.
    std::uint32_t components=2,oversampling=8,power_iterations=1;
    std::uint32_t clusters=2,max_iterations=100;
    std::uint64_t seed=0;
    double cluster_tolerance=1e-8;
    std::size_t work_bytes=256u*1024*1024;
    std::uint64_t max_operations=10000000000ull,max_distance_tests=100000000;
    void validate(const ExpressionMatrixMetadata&) const;
};
bool has_expression_reduction();
class ExpressionReductionResult {
public:
    const ExpressionReductionOptions& options() const;
    const std::string& source_sha256() const;
    const std::string& file_sha256() const;
    const std::string& value_source_sha256() const;
    const std::string& value_domain() const;
    const std::string& backend() const;
    std::uint64_t source_features() const;
    const std::vector<std::uint64_t>& features() const;
    const std::vector<std::string>& feature_ids() const;
    const std::vector<std::string>& observation_ids() const;
    const std::vector<double>& means() const;
    // Observation-major scores and feature-major loadings. Scores are direct
    // projections onto fitted loadings, without a dense input/centred matrix.
    const std::vector<double>& embedding() const;
    const std::vector<double>& loadings() const;
    const std::vector<double>& singular_values() const; // Small-sketch SVD values.
    const std::vector<double>& component_energy() const; // Actual score squares.
    double total_energy() const; // Centred sum of squares or uncentred energy.
    const std::vector<std::uint32_t>& labels() const;
    const std::vector<double>& centres() const;
    double inertia() const;
    std::uint32_t iterations() const;
    bool converged() const;
    const std::string& id() const; // Exact canonical fitted-array identity.
    std::size_t payload_bytes() const;
    std::size_t peak_work_bytes() const;
    std::size_t batches() const;
    std::uint64_t operations() const;
    std::uint64_t distance_tests() const;
private:
    struct Data;
    explicit ExpressionReductionResult(std::shared_ptr<Data>);
    std::shared_ptr<const Data> data_;
    friend struct ExpressionReductionCodec;
    friend std::shared_ptr<const ExpressionReductionResult> reduce_expression(
        std::shared_ptr<const ExpressionMatrixSource>,const ExpressionReductionOptions&,const std::atomic<bool>*);
};
// Binary64 analysis only; raw matrices are never changed. Direct integer inputs
// above2^53 refuse. Choose an explicit A01 derived source recipe if appropriate.
// Repeatability is scoped to the recorded native backend/build. Numerical
// sign/subspace comparison does not establish exact persisted model identity.
std::shared_ptr<const ExpressionReductionResult> reduce_expression(
    std::shared_ptr<const ExpressionMatrixSource>,const ExpressionReductionOptions& = {},
    const std::atomic<bool>* cancelled=nullptr);
void replay_expression_reduction(std::shared_ptr<const ExpressionMatrixSource>,
    const ExpressionReductionResult&,std::size_t work_bytes=256u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr,std::uint64_t max_operations=10000000000ull,
    std::uint64_t max_distance_tests=100000000);
// New-only canonical archives retain exact arrays; opening never refits.
std::string save_expression_reduction_archive(const ExpressionReductionResult&,const std::string& path,
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const ExpressionReductionResult> open_expression_reduction_archive(
    const std::string& path,const std::string& expected_id={},
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
std::string keep_expression_reduction(Project&,std::uint64_t image,const ExpressionReductionResult&,
    const std::string& archive,bool owned=true,std::size_t work_bytes=256u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);
std::vector<std::string> expression_reduction_ids(const Project&,std::uint64_t image);
void remove_expression_reduction(Project&,std::uint64_t image,const std::string& id);
struct ExpressionReductionOpenRequest {
    ExpressionMatrixOpenRequest parent;
    std::string archive,id,file_sha256;
    std::shared_ptr<const ExpressionReductionResult> open(std::size_t work_bytes=256u*1024*1024,
        const std::atomic<bool>* cancelled=nullptr) const;
};
ExpressionReductionOpenRequest expression_reduction_open_request(const Project&,std::uint64_t image,const std::string& id);
std::size_t save_expression_embedding_csv(const ExpressionReductionResult&,const std::string&,
    std::size_t file_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
std::size_t save_expression_loadings_csv(const ExpressionReductionResult&,const std::string&,
    std::size_t file_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
std::size_t save_expression_reduction_svg(const ExpressionReductionResult&,const std::string&,
    std::size_t file_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);

struct ExpressionReductionWorkflowOptions {
    std::uint64_t image=0;
    std::string matrix_id,spec_path,record_id;
    std::size_t work_bytes=256u*1024*1024,snapshot_bytes=512u*1024*1024;
    std::uint64_t project_bytes=1024ull*1024*1024;
    std::uint64_t max_operations=10000000000ull,max_distance_tests=100000000;
};
std::shared_ptr<const ExpressionReductionResult> run_expression_reduction(const std::string& project,
    const std::string& new_project,const ExpressionReductionWorkflowOptions&,
    const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const ExpressionReductionResult> open_project_expression_reduction(const std::string& project,
    const ExpressionReductionWorkflowOptions&,bool replay=false,const std::atomic<bool>* cancelled=nullptr);
}
