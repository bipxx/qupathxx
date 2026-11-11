#include "qupathxx/expression_differential_workflow.hpp"
#include "expression_differential_internal.hpp"
#include "qupathxx/project.hpp"
#include "workflow_input.hpp"
#include "spatial_output_guard.hpp"
namespace qpx {
namespace {
void options(const ExpressionDifferentialWorkflowOptions& o){detail::differential::work_limit(o.statistics.work_bytes);if(!o.image || !o.project_bytes || o.project_bytes>4ull*1024*1024*1024 || o.snapshot_bytes<1024*1024 || o.snapshot_bytes>1024ull*1024*1024)throw std::invalid_argument("Invalid DE workflow image/save limits");}
}
std::shared_ptr<const ExpressionDifferentialResult> run_expression_differential(const std::string& input,const std::string& output,const ExpressionDifferentialWorkflowOptions& o,const std::atomic<bool>* cancelled){
    options(o);detail::differential::options(o.statistics);detail::aggregation::digest(o.aggregation_id);if(!o.record_id.empty())throw std::invalid_argument("New DE accepts pseudobulk and groups, not a replay record");workflow_path_argument(output);detail::WorkflowInput file(input,o.project_bytes,true,cancelled);
    if(std::filesystem::exists(std::filesystem::u8path(output)))throw WorkflowOutputExists("DE output project already exists");const auto p=Project::open(file.path());const auto bulk=expression_aggregation_open_request(*p,o.image,o.aggregation_id).open(o.statistics.work_bytes,cancelled);const auto result=differential_expression(bulk,o.statistics,cancelled);
    const auto guard=[&]{file.verify(false,cancelled);bulk->parent()->require_current(cancelled);detail::protect_spatial_path(file.path(),"",output);detail::protect_spatial_path(bulk->parent()->path(),"",output);};guard();keep_expression_differential(*p,o.image,*result);p->save_new(output,o.snapshot_bytes,o.project_bytes,cancelled,guard);return result;
}
std::shared_ptr<const ExpressionDifferentialResult> open_project_expression_differential(const std::string& input,const ExpressionDifferentialWorkflowOptions& o,const std::atomic<bool>* cancelled){
    options(o);detail::aggregation::digest(o.record_id);if(!o.aggregation_id.empty() || !o.statistics.group_a.empty() || !o.statistics.group_b.empty() || o.statistics.library_target!=10000)throw std::invalid_argument("DE replay accepts its saved record, not new pseudobulk/groups/normalization");const detail::WorkflowInput file(input,o.project_bytes,true,cancelled);const auto p=Project::open(file.path());const auto result=expression_differential_open_request(*p,o.image,o.record_id).open(o.statistics.work_bytes,cancelled);file.verify(false,cancelled);return result;
}
}
