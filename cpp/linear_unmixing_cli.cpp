#include "linear_unmixing_cli.hpp"
#include "qupathxx/linear_unmixing_workflow.hpp"
#include "qupathxx/linear_unmixing_io.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include "workflow_input.hpp"
#include "spatial_output_guard.hpp"
#include "json_string.hpp"
#include <charconv>
#include <iostream>
#include <set>
namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& s,std::uint64_t maximum){std::uint64_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size() || n>maximum)throw std::invalid_argument("Invalid unmixing unsigned integer");return n;}
}
std::optional<int> linear_unmixing_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled){
    if(args.size()<2 || (args[1]!="--keep-unmix" && args[1]!="--list-unmix" && args[1]!="--verify-unmix"))return {};
    const bool keep=args[1]=="--keep-unmix",list=args[1]=="--list-unmix";const std::size_t start=keep?4:3;if(args.size()<start)throw std::invalid_argument("Unmixing needs PROJECT; Keep also needs NEW_PROJECT");
    std::uint64_t image=0,file_bytes=1024ull*1024*1024;std::size_t save_bytes=512ull*1024*1024;std::string matrix_path,correction,record;std::set<std::string> seen;
    for(std::size_t i=start;i<args.size();++i){const auto flag=args[i];if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate unmixing option: "+flag);const bool allowed=flag=="--image" || (!keep && !list && flag=="--record") || (keep && (flag=="--matrix" || flag=="--correction-record" || flag=="--save-snapshot-bytes" || flag=="--project-bytes"));if(!allowed || ++i==args.size())throw std::invalid_argument("Unknown/missing unmixing option: "+flag);const auto& value=args[i];if(flag=="--matrix")matrix_path=value;else if(flag=="--correction-record")correction=value;else if(flag=="--record")record=value;else{const auto n=integer(value,flag=="--image"?UINT64_MAX:4ull*1024*1024*1024);if(flag=="--image")image=n;else if(flag=="--project-bytes")file_bytes=n;else save_bytes=std::size_t(n);}}
    if(!image || (keep && (matrix_path.empty() || !save_bytes || !file_bytes)) || (!keep && !list && record.empty()) || (seen.count("--correction-record") && correction.empty()))throw std::invalid_argument("Unmixing needs positive --image/budgets; Keep needs --matrix; Verify needs --record");
    try{workflow_checkpoint(&cancelled);const auto input=export_path(args[2]),output=keep?export_path(args[3]):std::string{};ReaderFileGuard file(input);if(keep)protect_spatial_path(input,file.version()->identity,output);auto project=Project::open_without_journal_recovery(input);auto doc=project->document(image);
        std::optional<WorkflowInput> matrix_input;std::optional<LinearUnmixRequest> request;std::optional<DarkFlatRequest> parent;
        auto protect=[&]{workflow_checkpoint(&cancelled);file.verify();if(matrix_input)matrix_input->verify(false,&cancelled);if(request)request->require_current(*doc,&cancelled);if(!keep)return;protect_spatial_path(input,file.version()->identity,output);for(const auto* suffix:{"-journal","-wal","-shm"})protect_spatial_path(input+suffix,"",output);for(const auto& entry:project->images())if(!entry.embedded && !entry.source_path.empty())protect_spatial_path(export_path(entry.source_path),"",output);if(matrix_input)protect_spatial_path(matrix_input->path(),"",output);if(parent)for(const auto& source:parent->recipe().sources)protect_spatial_path(export_path(source.path),"",output);if(request)protect_spatial_path(export_path(request->recipe().signal.path),"",output);};
        if(list){protect();const auto ids=linear_unmix_recipe_ids(*doc);std::cout<<"{\"schema\":\"qpx.linear-unmix-cli/1\",\"state\":\"records\",\"records\":[";for(std::size_t i=0;i<ids.size();++i){if(i)std::cout<<',';std::cout<<json_string(ids[i]);}std::cout<<"]}\n";return 0;}
        if(keep){matrix_input.emplace(matrix_path,1024u*1024,false,&cancelled);auto matrix=linear_mixing_matrix_from_json(read_workflow_input(*matrix_input,&cancelled));if(!correction.empty()){parent.emplace(replay_dark_flat_recipe(*doc,correction,&cancelled));request.emplace(*doc,*parent,std::move(matrix),&cancelled);}else request.emplace(*doc,std::move(matrix),&cancelled);protect();record=keep_linear_unmix_recipe(*doc,*request,&cancelled);project->save_new(output,save_bytes,file_bytes,&cancelled,protect);}
        else{request.emplace(replay_linear_unmix_recipe(*doc,record,&cancelled));protect();}
        const auto& recipe=request->recipe();std::cout<<"{\"schema\":\"qpx.linear-unmix-cli/1\",\"state\":"<<json_string(keep?"saved":"verified")<<",\"record\":"<<json_string(record)<<",\"domain\":"<<json_string(request->source()->domain())<<",\"algorithm\":"<<json_string(recipe.algorithm)<<",\"operator_sha256\":"<<json_string(recipe.operator_sha256)<<",\"matrix_sha256\":"<<json_string(recipe.matrix.sha256())<<",\"source_sha256\":"<<json_string(request->source()->sha256())<<",\"correction_record\":"<<json_string(recipe.correction_record)<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return cancelled.load()?130:1;}
}
}
