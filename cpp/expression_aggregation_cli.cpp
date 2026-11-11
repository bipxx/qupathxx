#include "expression_aggregation_cli.hpp"
#include "qupathxx/expression_aggregation_workflow.hpp"
#include "qupathxx/workflow.hpp"
#include <charconv>
#include <iostream>
#include <set>

namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& s){std::uint64_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size())throw std::invalid_argument("Expected aggregation unsigned integer");return n;}
}
std::optional<int> expression_aggregation_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled){
    if(args.size()<2 || (args[1]!="--aggregate-expression" && args[1]!="--verify-expression-aggregation" && args[1]!="--export-expression-aggregation"))return {};
    const bool run=args[1]=="--aggregate-expression",export_csv=args[1]=="--export-expression-aggregation";const std::size_t start=run || export_csv?4:3;if(args.size()<start)throw std::invalid_argument("Expression aggregation requires PROJECT, plus NEW_PROJECT or NEW_CSV for creation/export");
    ExpressionAggregationWorkflowOptions options;std::size_t csv_bytes=256u*1024*1024;bool members=false;std::set<std::string> seen;
    for(std::size_t i=start;i<args.size();++i){const auto flag=args[i];if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate aggregation option: "+flag);if(flag=="--members" && export_csv){members=true;continue;}if(++i==args.size())throw std::invalid_argument("Missing aggregation option value");const auto& value=args[i];
        if(flag=="--image")options.image=integer(value);else if(flag=="--matrix" && run)options.matrix_id=value;else if(flag=="--spec" && run)options.spec_path=value;else if(flag=="--record" && !run)options.record_id=value;else if(flag=="--work-bytes")options.work_bytes=integer(value);else if(flag=="--project-bytes")options.project_bytes=integer(value);else if(flag=="--save-snapshot-bytes" && run)options.snapshot_bytes=integer(value);else if(flag=="--csv-bytes" && export_csv)csv_bytes=integer(value);else throw std::invalid_argument("Unknown aggregation option: "+flag);
    }
    const auto result=run?run_expression_aggregation(args[2],args[3],options,&cancelled):open_project_expression_aggregation(args[2],options,&cancelled);
    std::size_t written=0;if(export_csv)written=members?save_expression_membership_csv(*result,args[3],csv_bytes,options.work_bytes,&cancelled):save_expression_aggregation_csv(*result,args[3],csv_bytes,&cancelled);
    std::cout<<"{\"schema\":\"qpx.expression-aggregation-report/1\",\"state\":\""<<(run?"saved":export_csv?"exported":"verified")<<"\",\"record\":\""<<result->id()<<"\",\"result_sha256\":\""<<result->sha256()<<"\",\"features\":"<<result->features().size()<<",\"cohorts\":"<<result->cohorts().size()<<",\"batches\":"<<result->batches()<<",\"csv_bytes\":"<<written<<"}\n";return 0;
}
}
