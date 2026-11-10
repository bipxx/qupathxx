#include "expression_differential_cli.hpp"
#include "qupathxx/expression_differential_workflow.hpp"
#include <charconv>
#include <iostream>
#include <set>
namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& s){std::uint64_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size())throw std::invalid_argument("Expected DE unsigned integer");return n;}
double real(const std::string& s){double n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size())throw std::invalid_argument("Expected DE numeric target");return n;}
}
std::optional<int> expression_differential_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled){
    if(args.size()<2 || (args[1]!="--differential-expression" && args[1]!="--verify-expression-differential" && args[1]!="--export-expression-differential"))return {};
    const bool run=args[1]=="--differential-expression",export_csv=args[1]=="--export-expression-differential";const std::size_t start=run || export_csv?4:3;if(args.size()<start)throw std::invalid_argument("DE requires PROJECT, plus NEW_PROJECT or NEW_CSV for creation/export");
    ExpressionDifferentialWorkflowOptions o;std::size_t csv_bytes=256u*1024*1024;bool replicates=false;std::set<std::string> seen;
    for(std::size_t i=start;i<args.size();++i){const auto flag=args[i];if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate DE option: "+flag);if(flag=="--replicates" && export_csv){replicates=true;continue;}if(++i==args.size())throw std::invalid_argument("Missing DE option value");const auto& v=args[i];
        if(flag=="--image")o.image=integer(v);else if(flag=="--pseudobulk" && run)o.aggregation_id=v;else if(flag=="--group-a" && run)o.statistics.group_a=v;else if(flag=="--group-b" && run)o.statistics.group_b=v;else if(flag=="--library-target" && run)o.statistics.library_target=real(v);else if(flag=="--record" && !run)o.record_id=v;else if(flag=="--work-bytes")o.statistics.work_bytes=integer(v);else if(flag=="--project-bytes")o.project_bytes=integer(v);else if(flag=="--save-snapshot-bytes" && run)o.snapshot_bytes=integer(v);else if(flag=="--csv-bytes" && export_csv)csv_bytes=integer(v);else throw std::invalid_argument("Unknown DE option: "+flag);
    }
    const auto r=run?run_expression_differential(args[2],args[3],o,&cancelled):open_project_expression_differential(args[2],o,&cancelled);std::size_t written=0;if(export_csv)written=replicates?save_expression_differential_replicates_csv(*r,args[3],csv_bytes,&cancelled):save_expression_differential_csv(*r,args[3],csv_bytes,&cancelled);
    std::cout<<"{\"schema\":\"qpx.expression-differential-report/1\",\"state\":\""<<(run?"saved":export_csv?"exported":"verified")<<"\",\"record\":\""<<r->id()<<"\",\"result_sha256\":\""<<r->sha256()<<"\",\"features\":"<<r->genes().size()<<",\"testable_features\":"<<r->correction_features()<<",\"n_a\":"<<r->n_a()<<",\"n_b\":"<<r->n_b()<<",\"peak_work_bytes\":"<<r->peak_work_bytes()<<",\"csv_bytes\":"<<written<<"}\n";return 0;
}
}
