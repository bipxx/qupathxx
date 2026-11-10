#pragma once
#include "qupathxx/expression_aggregation.hpp"

namespace qpx {
bool has_expression_statistics();
struct ExpressionDifferentialOptions {
    std::string group_a, group_b;
    double library_target=10000;
    std::size_t work_bytes=256u*1024*1024;
};
enum class ExpressionDifferentialStatus { Tested, AllZero, InsufficientReplicates, ZeroVariance, TailUnderflow };
const char* expression_differential_status(ExpressionDifferentialStatus);
struct ExpressionDifferentialGene {
    double mean_a=0, mean_b=0, effect=0;
    std::optional<double> variance_a, variance_b;
    // Absent statistics are absent values, never manufactured zeros or NaNs.
    std::optional<double> t, degrees_of_freedom, p, q;
    ExpressionDifferentialStatus status=ExpressionDifferentialStatus::AllZero;
};
struct ExpressionDifferentialReplicate {
    std::string id, sample, group;
    std::vector<std::size_t> cohorts;
    std::uint64_t library=0, observations=0;
};
class ExpressionDifferentialResult {
public:
    const std::shared_ptr<const ExpressionAggregationResult>& parent() const;
    const ExpressionDifferentialOptions& options() const;
    const std::vector<ExpressionDifferentialReplicate>& replicates() const;
    const std::vector<ExpressionDifferentialGene>& genes() const;
    std::size_t n_a() const;
    std::size_t n_b() const;
    std::size_t correction_features() const;
    const std::string& sha256() const;
    const std::string& recipe() const;
    const std::string& id() const;
    std::size_t peak_work_bytes() const;
private:
    struct Data;
    std::shared_ptr<const Data> data_;
    explicit ExpressionDifferentialResult(std::shared_ptr<const Data>);
    friend std::shared_ptr<const ExpressionDifferentialResult> differential_expression(
        std::shared_ptr<const ExpressionAggregationResult>, const ExpressionDifferentialOptions&, const std::atomic<bool>*);
};
// Full-universe raw pseudobulk only. Repeated disjoint cohorts of one biological
// replicate are summed BEFORE normalization. Sample IDs and replicate IDs must
// be one-to-one and assigned to exactly one of the two explicit groups. This is
// an unpaired Welch test, not a paired design or a count-model method.
std::shared_ptr<const ExpressionDifferentialResult> differential_expression(
    std::shared_ptr<const ExpressionAggregationResult>, const ExpressionDifferentialOptions&,
    const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const ExpressionDifferentialResult> replay_expression_differential(
    std::shared_ptr<const ExpressionAggregationResult>, const std::string& recipe,
    std::size_t work_bytes=256u*1024*1024, const std::atomic<bool>* cancelled=nullptr);
std::string keep_expression_differential(Project&, std::uint64_t image, const ExpressionDifferentialResult&);
std::vector<std::string> expression_differential_ids(const Project&, std::uint64_t image);
void remove_expression_differential(Project&, std::uint64_t image, const std::string& id);
struct ExpressionDifferentialOpenRequest {
    ExpressionAggregationOpenRequest parent;
    std::string recipe;
    std::shared_ptr<const ExpressionDifferentialResult> open(std::size_t work_bytes=256u*1024*1024,
        const std::atomic<bool>* cancelled=nullptr) const;
};
ExpressionDifferentialOpenRequest expression_differential_open_request(const Project&, std::uint64_t image, const std::string& id);
std::size_t save_expression_differential_csv(const ExpressionDifferentialResult&, const std::string& path,
    std::size_t csv_bytes=256u*1024*1024, const std::atomic<bool>* cancelled=nullptr);
std::size_t save_expression_differential_replicates_csv(const ExpressionDifferentialResult&, const std::string& path,
    std::size_t csv_bytes=256u*1024*1024, const std::atomic<bool>* cancelled=nullptr);
}
