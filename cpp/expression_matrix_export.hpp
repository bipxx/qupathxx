#pragma once
#include "qupathxx/expression_matrix.hpp"
#include <functional>

namespace qpx::detail {
std::size_t backup_expression_matrix(
    const ExpressionMatrixSource&,const std::string& output,const std::atomic<bool>* cancelled,
    const std::function<void()>& protect_output);
std::size_t export_expression_matrix_csv(
    const ExpressionMatrixSource&, const std::string& output,
    const std::vector<std::string>& features, const std::atomic<bool>* cancelled,
    const std::function<void()>& protect_output);
std::size_t export_expression_matrix_positions(
    const ExpressionMatrixSource&, const std::string& output, const std::atomic<bool>* cancelled,
    const std::function<void()>& protect_output);
std::size_t export_expression_matrix_market(
    const ExpressionMatrixSource&, const std::string& output, const std::atomic<bool>* cancelled,
    const std::function<void()>& protect_output);
}
