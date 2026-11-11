#pragma once
#include "qupathxx/expression_aggregation.hpp"
namespace qpx {
struct ExpressionAggregationWorkflowOptions {
    std::uint64_t image=0;
    std::string matrix_id,spec_path,record_id;
    std::size_t work_bytes=256u*1024*1024;
    std::size_t snapshot_bytes=512u*1024*1024;
    std::uint64_t project_bytes=1024ull*1024*1024;
};
// Shared native application service; no CLI/GUI/Python logic owns interpretation
// of a scientific specification, membership, persistence or replay.
std::shared_ptr<const ExpressionAggregationResult> run_expression_aggregation(
    const std::string& project,const std::string& new_project,
    const ExpressionAggregationWorkflowOptions&,const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const ExpressionAggregationResult> open_project_expression_aggregation(
    const std::string& project,const ExpressionAggregationWorkflowOptions&,
    const std::atomic<bool>* cancelled=nullptr);
}
