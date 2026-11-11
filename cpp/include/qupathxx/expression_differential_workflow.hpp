#pragma once
#include "qupathxx/expression_differential.hpp"
namespace qpx {
struct ExpressionDifferentialWorkflowOptions {
    std::uint64_t image=0;
    std::string aggregation_id,record_id;
    ExpressionDifferentialOptions statistics;
    std::size_t snapshot_bytes=512u*1024*1024;
    std::uint64_t project_bytes=1024ull*1024*1024;
};
std::shared_ptr<const ExpressionDifferentialResult> run_expression_differential(
    const std::string& project,const std::string& new_project,const ExpressionDifferentialWorkflowOptions&,
    const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const ExpressionDifferentialResult> open_project_expression_differential(
    const std::string& project,const ExpressionDifferentialWorkflowOptions&,const std::atomic<bool>* cancelled=nullptr);
}
