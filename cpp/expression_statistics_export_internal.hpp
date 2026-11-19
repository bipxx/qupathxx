#pragma once

#include "qupathxx/expression_aggregation_workflow.hpp"
#include "qupathxx/expression_differential_workflow.hpp"
#include <functional>
#include <utility>

namespace qpx::detail {
std::size_t write_expression_aggregation_export(
    const ExpressionAggregationResult&, const std::string&, bool members,
    std::size_t bytes, std::size_t work, const std::atomic<bool>*, const std::function<void()>& protect = {});
std::size_t write_expression_differential_export(
    const ExpressionDifferentialResult&, const std::string&, bool replicates,
    std::size_t bytes, const std::atomic<bool>*, const std::function<void()>& protect = {});

std::pair<std::shared_ptr<const ExpressionAggregationResult>, std::size_t> export_project_expression_aggregation(
    const std::string& input, const std::string& output, const ExpressionAggregationWorkflowOptions&,
    bool members, std::size_t bytes, const std::atomic<bool>*);
std::pair<std::shared_ptr<const ExpressionDifferentialResult>, std::size_t> export_project_expression_differential(
    const std::string& input, const std::string& output, const ExpressionDifferentialWorkflowOptions&,
    bool replicates, std::size_t bytes, const std::atomic<bool>*);
}
