#pragma once

#include "qupathxx/expression_reduction.hpp"
#include <functional>
#include <utility>

namespace qpx::detail {
enum class ReductionExport { Embedding, Loadings, Plot };

std::string write_expression_reduction_archive(
    const ExpressionReductionResult&, const std::string& destination, std::size_t work,
    const std::atomic<bool>* cancelled, const std::function<void()>& protect);

std::size_t write_expression_reduction_export(
    const ExpressionReductionResult&, const std::string& destination, ReductionExport,
    std::size_t bytes, const std::atomic<bool>* cancelled, const std::function<void()>& protect = {});

std::pair<std::shared_ptr<const ExpressionReductionResult>, std::size_t>
export_project_expression_reduction(
    const std::string& input, const std::string& destination, const ExpressionReductionWorkflowOptions&,
    ReductionExport, std::size_t bytes, const std::atomic<bool>* cancelled);
}
