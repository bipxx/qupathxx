#include "marker_cli.hpp"
#include "qupathxx/marker_rule_io.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/source_identity.hpp"
#include "workflow_input.hpp"
#include "spatial_output_guard.hpp"
#include "json_string.hpp"
#include <charconv>
#include <iostream>
#include <set>
#include <algorithm>
namespace qpx::detail {
namespace {
std::uint64_t number(const std::string& text,std::uint64_t maximum){std::uint64_t n=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),n);if(text.empty() || !n || result.ec!=std::errc{} || result.ptr!=text.data()+text.size() || n>maximum)throw std::invalid_argument("Marker IDs/budgets require bounded positive integers");return n;}
}
std::optional<int> marker_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled){
    if(args.size()<2 || (args[1]!="--preview-markers" && args[1]!="--apply-markers" && args[1]!="--list-marker-rules"))return {};
    const bool apply=args[1]=="--apply-markers",list=args[1]=="--list-marker-rules";const std::size_t start=apply?4:3;
    if(args.size()<start)throw std::invalid_argument("Marker commands require PROJECT and apply also needs a new OUTPUT project; see --help");
    std::uint64_t image=0,file_bytes=1024ull*1024*1024;std::size_t work=64u*1024*1024,save_bytes=512ull*1024*1024;bool all=false;std::string rule_file,rule_id;std::vector<std::uint64_t> ids;std::set<std::string> seen;
    for(std::size_t i=start;i<args.size();++i){const auto flag=args[i];if(flag!="--cell" && !seen.insert(flag).second)throw std::invalid_argument("Duplicate marker option: "+flag);
        if(!list && flag=="--all-cells"){all=true;continue;}
        if(flag!="--image" && (list || (flag!="--rule-file" && flag!="--rule" && flag!="--cell" && flag!="--work-bytes" && !(apply && (flag=="--save-snapshot-bytes" || flag=="--project-bytes")))))throw std::invalid_argument("Unknown marker option: "+flag);
        if(++i==args.size())throw std::invalid_argument("Missing marker option value: "+flag);const auto& value=args[i];
        if(flag=="--rule-file"){rule_file=value;continue;}if(flag=="--rule"){rule_id=value;continue;}
        const auto n=number(value,flag=="--image" || flag=="--cell"?UINT64_MAX:4ull*1024*1024*1024);
        if(flag=="--image")image=n;else if(flag=="--cell"){if(ids.size()==100000)throw std::length_error("Marker cohort exceeds100000 IDs");ids.push_back(n);}else if(flag=="--work-bytes"){if(n<4u*1024*1024 || n>512u*1024*1024)throw std::invalid_argument("Marker work must be4..512MiB");work=std::size_t(n);}else if(flag=="--save-snapshot-bytes")save_bytes=std::size_t(n);else file_bytes=n;
    }
    if(!image || (!list && ((rule_file.empty()==rule_id.empty()) || (all==!ids.empty()))))throw std::invalid_argument("Markers need --image, exactly one --rule-file/--rule and exactly one --all-cells/repeated --cell");
    try {
        workflow_checkpoint(&cancelled);const auto source=export_path(args[2]);const auto output=apply?export_path(args[3]):std::string{};ReaderFileGuard guard(source);
        if(apply)protect_spatial_path(source,guard.version()->identity,output);
        std::unique_ptr<WorkflowInput> rule_input;std::optional<MarkerRule> rule;
        if(!rule_file.empty()){rule_input=std::make_unique<WorkflowInput>(rule_file,1024u*1024,false,&cancelled);rule=marker_rule_from_json(read_workflow_input(*rule_input,&cancelled));}
        const auto project=Project::open_without_journal_recovery(source);const auto doc=project->document(image);const auto images=project->images();
        std::shared_ptr<const MarkerRuleResult> result;bool published=false;
        auto protect=[&]{guard.verify();workflow_checkpoint(&cancelled);if(rule_input)rule_input->verify(false,&cancelled);if(published && !verify_marker_rule_publication(*doc,*result,&cancelled))throw WorkflowInputChanged("Published marker facts/classes changed before save");if(!apply)return;
            protect_spatial_path(source,guard.version()->identity,output);for(const auto* suffix:{"-journal","-wal","-shm"})protect_spatial_path(source+suffix,"",output);
            if(rule_input)protect_spatial_path(rule_input->path(),"",output);for(const auto& entry:images)if(!entry.embedded && !entry.source_path.empty())protect_spatial_path(export_path(entry.source_path),"",output);
            if(const auto files=doc->server()->reader_source()){if(files->primary)protect_spatial_path(files->primary->path,files->primary->identity,output);for(const auto& file:files->companions)protect_spatial_path(file.path,file.identity,output);}
        };
        protect();if(list){const auto rules=marker_rule_ids(*doc);protect();std::cout<<"{\"schema\":\"qpx.marker-cli/1\",\"state\":\"rules\",\"rules\":[";for(std::size_t i=0;i<rules.size();++i){if(i)std::cout<<',';std::cout<<json_string(rules[i]);}std::cout<<"]}\n";return 0;}
        if(!rule)rule=load_marker_rule(*doc,rule_id);if(all)ids=marker_cell_ids_for_rule(*doc,*rule,&cancelled,work);const MarkerRuleRequest request(*doc,ids,*rule,work,&cancelled);result=preview_marker_rule(request,&cancelled);std::size_t changed=0;
        if(apply){const auto existing=marker_rule_ids(*doc);const auto id=rule->sha256();if(std::find(existing.begin(),existing.end(),id)==existing.end())keep_marker_rule(*doc,*rule);changed=apply_marker_rule(*doc,request,*result,&cancelled).size();published=true;project->save_new(output,save_bytes,file_bytes,&cancelled,protect);}else protect();
        std::cout<<"{\"schema\":\"qpx.marker-cli/1\",\"state\":"<<json_string(apply?"saved":"preview")<<",\"changed_cells\":"<<changed<<",\"result\":"<<marker_result_json(request,*result)<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return cancelled.load()?130:1;}
}
}
