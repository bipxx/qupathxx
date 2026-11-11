#include "expression_reduction_cli.hpp"
#include "qupathxx/expression_reduction.hpp"
#include <charconv>
#include <iostream>
#include <limits>
#include <set>

namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& s){std::uint64_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size())throw std::invalid_argument("Expected unsigned reduction argument");return n;}
std::size_t size(const std::string& s){const auto n=integer(s);if(n>std::numeric_limits<std::size_t>::max())throw std::invalid_argument("Reduction byte argument exceeds address space");return std::size_t(n);}
}
std::optional<int> expression_reduction_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled){
    if(args.size()<2 || (args[1]!="--reduce-expression" && args[1]!="--verify-expression-reduction" && args[1]!="--replay-expression-reduction" && args[1]!="--export-expression-reduction"))return {};
    const bool run=args[1]=="--reduce-expression",export_result=args[1]=="--export-expression-reduction",replay=args[1]=="--replay-expression-reduction";const std::size_t start=run || export_result?4:3;if(args.size()<start)throw std::invalid_argument("Reduction requires PROJECT and NEW_PROJECT or NEW_EXPORT when creating/exporting");ExpressionReductionWorkflowOptions options;std::size_t file_bytes=256u*1024*1024;bool loadings=false,plot=false;std::set<std::string> seen;
    for(std::size_t i=start;i<args.size();++i){const auto flag=args[i];if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate reduction option: "+flag);if(export_result && (flag=="--loadings" || flag=="--plot")){if(flag=="--plot")plot=true;else loadings=true;continue;}if(++i==args.size())throw std::invalid_argument("Missing reduction option value");const auto& value=args[i];if(flag=="--image")options.image=integer(value);else if(flag=="--matrix" && run)options.matrix_id=value;else if(flag=="--spec" && run)options.spec_path=value;else if(flag=="--record" && !run)options.record_id=value;else if(flag=="--work-bytes")options.work_bytes=size(value);else if(flag=="--project-bytes")options.project_bytes=integer(value);else if(flag=="--save-snapshot-bytes" && run)options.snapshot_bytes=size(value);else if(flag=="--max-operations" && (run || replay))options.max_operations=integer(value);else if(flag=="--max-distance-tests" && (run || replay))options.max_distance_tests=integer(value);else if(flag=="--file-bytes" && export_result)file_bytes=size(value);else throw std::invalid_argument("Unknown reduction option: "+flag);}
    if(loadings && plot)throw std::invalid_argument("Choose loadings or plot export");const auto result=run?run_expression_reduction(args[2],args[3],options,&cancelled):open_project_expression_reduction(args[2],options,replay,&cancelled);std::size_t written=0;if(export_result)written=plot?save_expression_reduction_svg(*result,args[3],file_bytes,&cancelled):loadings?save_expression_loadings_csv(*result,args[3],file_bytes,&cancelled):save_expression_embedding_csv(*result,args[3],file_bytes,&cancelled);
    std::cout<<"{\"schema\":\"qpx.expression-reduction-report/1\",\"state\":\""<<(run?"saved":export_result?"exported":replay?"replayed":"verified")<<"\",\"record\":\""<<result->id()<<"\",\"features\":"<<result->features().size()<<",\"observations\":"<<result->observation_ids().size()<<",\"components\":"<<result->options().components<<",\"clusters\":"<<result->options().clusters<<",\"seed\":"<<result->options().seed<<",\"converged\":"<<(result->converged()?"true":"false")<<",\"file_bytes\":"<<written<<"}\n";return 0;
}
}
