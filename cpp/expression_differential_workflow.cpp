#include "qupathxx/expression_differential_workflow.hpp"
#include "expression_statistics_export_internal.hpp"
#include "project_expression_export.hpp"
#include "expression_differential_internal.hpp"
#include "qupathxx/project.hpp"
#include "workflow_input.hpp"
#include "spatial_output_guard.hpp"
namespace qpx {
namespace {
void options(const ExpressionDifferentialWorkflowOptions& o){detail::differential::work_limit(o.statistics.work_bytes);if(!o.image || !o.project_bytes || o.project_bytes>4ull*1024*1024*1024 || o.snapshot_bytes<1024*1024 || o.snapshot_bytes>1024ull*1024*1024)throw std::invalid_argument("Invalid DE workflow image/save limits");}
void retained_options(const ExpressionDifferentialWorkflowOptions& o) {
    options(o);
    detail::aggregation::digest(o.record_id);
    if (!o.aggregation_id.empty() || !o.statistics.group_a.empty() ||
        !o.statistics.group_b.empty() || o.statistics.library_target != 10000)
        throw std::invalid_argument("DE replay accepts its saved record, not new pseudobulk/groups/normalization");
}

}
std::shared_ptr<const ExpressionDifferentialResult> run_expression_differential(const std::string& input,const std::string& output,const ExpressionDifferentialWorkflowOptions& o,const std::atomic<bool>* cancelled){
    options(o);detail::differential::options(o.statistics);detail::aggregation::digest(o.aggregation_id);if(!o.record_id.empty())throw std::invalid_argument("New DE accepts pseudobulk and groups, not a replay record");workflow_path_argument(output);
    detail::ProjectExpressionOutput capture(input,output,o.project_bytes,cancelled);
    if(std::filesystem::exists(std::filesystem::u8path(capture.output())))throw WorkflowOutputExists("DE output project already exists");
    auto& project=capture.project();
    const auto bulk=expression_aggregation_open_request(project,o.image,o.aggregation_id).open(o.statistics.work_bytes,cancelled);
    const auto result=differential_expression(bulk,o.statistics,cancelled);
    const auto guard=[&]{
        capture.protect();bulk->parent()->require_current(cancelled);
        detail::protect_spatial_path(bulk->parent()->path(),"",capture.output());
    };
    guard();
    keep_expression_differential(project,o.image,*result);
    project.save_new(capture.output(),o.snapshot_bytes,o.project_bytes,cancelled,guard);
    return result;
}
std::shared_ptr<const ExpressionDifferentialResult> open_project_expression_differential(const std::string& input,const ExpressionDifferentialWorkflowOptions& o,const std::atomic<bool>* cancelled){
    retained_options(o);const detail::WorkflowInput file(input,o.project_bytes,true,cancelled);const auto p=Project::open(file.path());const auto result=expression_differential_open_request(*p,o.image,o.record_id).open(o.statistics.work_bytes,cancelled);file.verify(false,cancelled);return result;
}

std::pair<std::shared_ptr<const ExpressionDifferentialResult>, std::size_t>
detail::export_project_expression_differential(
    const std::string& input, const std::string& destination, const ExpressionDifferentialWorkflowOptions& o,
    bool replicates, std::size_t bytes, const std::atomic<bool>* cancelled) {
    retained_options(o);
    const ProjectExpressionOutput capture(input, destination, o.project_bytes, cancelled);
    auto result = expression_differential_open_request(capture.project(), o.image, o.record_id)
                      .open(o.statistics.work_bytes, cancelled);
    const auto written = write_expression_differential_export(
        *result, capture.output(), replicates, bytes, cancelled, [&] { capture.protect(); });
    return {std::move(result), written};
}
}
