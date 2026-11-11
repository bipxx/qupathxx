#pragma once
#include "qupathxx/expression_matrix_spatial.hpp"
#include "qupathxx/expression_normalization.hpp"

namespace qpx {
enum class ExpressionOverlapPolicy { Independent, Reject };
enum class ExpressionAggregationKind { RegionalSummary, Pseudobulk };
struct ExpressionCohort {
    std::string id, sample, replicate, group;
    // Exact source-axis indices, sorted and unique. Empty cohorts are retained.
    std::vector<std::uint64_t> observations;
    std::string source_sha256;
    // Captured selection provenance; replay uses these immutable memberships,
    // never silently reselects against subsequently edited ROIs/registrations.
    std::string origin="explicit-source-indices/1";
};
struct ExpressionRegion {
    std::string id;
    Roi geometry;
    std::string sample, replicate, group;
};
// Membership uses spot centres and Roi::contains, including its boundary policy.
// Source mode requires identical frames. Registered mode requires an explicit
// registration bound to spatial.dataset_frame() and the supplied target frame.
// Counts, source coordinates and out-of-image positions are never rewritten.
std::vector<ExpressionCohort> expression_region_cohorts(
    const ExpressionMatrixSpatialSnapshot&, const std::vector<ExpressionRegion>&,
    const CoordinateFrame& region_frame, ExpressionOverlapPolicy,
    const std::optional<RegistrationDefinition>& registration=std::nullopt,
    std::size_t work_bytes=256u*1024*1024,
    std::uint64_t max_membership_tests=100000000,
    const std::atomic<bool>* cancelled=nullptr);
ExpressionCohort expression_observation_cohort(
    std::shared_ptr<const ExpressionMatrixSource>, std::string id,
    const std::vector<std::string>& observation_ids,
    std::string sample={}, std::string replicate={}, std::string group={},
    std::size_t work_bytes=256u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);

struct ExpressionAggregationOptions {
    ExpressionAggregationKind kind=ExpressionAggregationKind::RegionalSummary;
    ExpressionOverlapPolicy overlap=ExpressionOverlapPolicy::Reject;
    // Empty selects the complete source feature universe. Otherwise ascending,
    // unique source indices. Pseudobulk always requires the complete universe.
    std::vector<std::uint64_t> features;
    // Empty uses the original, explicitly declared source domain. A retained A01
    // recipe selects normalized/log values for regional summaries only.
    std::string normalization_recipe;
    std::size_t work_bytes=256u*1024*1024;
};
class ExpressionAggregationResult {
public:
    ExpressionAggregationKind kind() const;
    ExpressionOverlapPolicy overlap() const;
    const std::vector<ExpressionCohort>& cohorts() const;
    const std::vector<ExpressionFeature>& features() const;
    ExpressionValueDomain domain() const;
    const std::string& value_domain() const;
    const std::string& source_sha256() const;
    const std::string& file_sha256() const;
    const std::string& value_source_sha256() const;
    // Feature-major, then cohort order. Raw sums never pass through double.
    const std::vector<std::uint64_t>& counts() const;
    const std::vector<double>& continuous() const;
    std::optional<double> mean(std::size_t feature, std::size_t cohort) const;
    const std::string& sha256() const;
    const std::string& recipe() const;
    const std::string& id() const;
    std::size_t payload_bytes() const;
    std::size_t peak_work_bytes() const;
    std::size_t batches() const;
    const std::shared_ptr<const ExpressionMatrixSource>& parent() const;
private:
    struct Data;
    explicit ExpressionAggregationResult(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
    friend std::shared_ptr<const ExpressionAggregationResult> aggregate_expression(
        std::shared_ptr<const ExpressionMatrixSource>, const std::vector<ExpressionCohort>&,
        const ExpressionAggregationOptions&, const std::atomic<bool>*);
};
// Native bounded sparse reduction. Sparse absence is a measured zero; an empty
// cohort has sum zero and an absent mean. Pseudobulk requires raw integer counts,
// disjoint cohorts and explicit sample/replicate/group labels; none are inferred.
std::shared_ptr<const ExpressionAggregationResult> aggregate_expression(
    std::shared_ptr<const ExpressionMatrixSource>, const std::vector<ExpressionCohort>&,
    const ExpressionAggregationOptions& = {}, const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const ExpressionAggregationResult> replay_expression_aggregation(
    std::shared_ptr<const ExpressionMatrixSource>, const std::string& recipe,
    std::size_t work_bytes=256u*1024*1024, const std::atomic<bool>* cancelled=nullptr);

std::string keep_expression_aggregation(Project&, std::uint64_t image, const ExpressionAggregationResult&);
std::vector<std::string> expression_aggregation_ids(const Project&, std::uint64_t image);
void remove_expression_aggregation(Project&, std::uint64_t image, const std::string& id);
struct ExpressionAggregationOpenRequest {
    ExpressionMatrixOpenRequest parent;
    std::string recipe;
    std::shared_ptr<const ExpressionAggregationResult> open(std::size_t work_bytes=256u*1024*1024,
        const std::atomic<bool>* cancelled=nullptr) const;
};
ExpressionAggregationOpenRequest expression_aggregation_open_request(const Project&, std::uint64_t image, const std::string& id);
// Atomic new-only exports. Membership export includes exact observation IDs and
// source-axis indices, linked to the result/recipe ID in every row.
std::size_t save_expression_aggregation_csv(const ExpressionAggregationResult&, const std::string& path,
    std::size_t csv_bytes=256u*1024*1024, const std::atomic<bool>* cancelled=nullptr);
std::size_t save_expression_membership_csv(const ExpressionAggregationResult&, const std::string& path,
    std::size_t csv_bytes=256u*1024*1024, std::size_t work_bytes=256u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);
}
