#include "dark_flat_cli.hpp"
#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include "spatial_output_guard.hpp"
#include "json_string.hpp"
#include <charconv>
#include <iostream>
#include <set>
namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& s,std::uint64_t maximum){std::uint64_t n=0;auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size() || n>maximum)throw std::invalid_argument("Invalid dark/flat unsigned integer");return n;}
double real(const std::string& s){double n=0;auto p=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size() || !std::isfinite(n))throw std::invalid_argument("Invalid dark/flat finite number");return n;}
std::vector<double> scales(const std::string& s){std::vector<double> values;if(s.size()>1024*1024)throw std::length_error("Dark/flat scales text limit");std::size_t pos=0;while(true){auto end=s.find(',',pos);values.push_back(real(s.substr(pos,end==std::string::npos?end:end-pos)));if(values.size()>65536)throw std::length_error("Dark/flat scale count limit");if(end==std::string::npos)break;pos=end+1;}return values;}
void counts(const std::vector<std::uint64_t>& values){std::cout<<'[';for(std::size_t i=0;i<values.size();++i){if(i)std::cout<<',';std::cout<<values[i];}std::cout<<']';}
}
std::optional<int> dark_flat_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled){
    if(args.size()<2 || (args[1]!="--keep-dark-flat" && args[1]!="--list-dark-flat" && args[1]!="--verify-dark-flat" && args[1]!="--preview-dark-flat"))return {};
    const bool keep=args[1]=="--keep-dark-flat",list=args[1]=="--list-dark-flat",preview=args[1]=="--preview-dark-flat";const std::size_t start=keep?4:3;
    if(args.size()<start)throw std::invalid_argument("Dark/flat needs PROJECT; Keep also needs NEW_PROJECT");
    std::uint64_t image=0,file_bytes=1024ull*1024*1024;std::size_t work=256u*1024*1024,save_bytes=512ull*1024*1024;std::string dark,flat,record;TiffOpenOptions dark_open,flat_open;DarkFlatOptions options;RawRegionRequest region;std::set<std::string> seen;
    for(std::size_t i=start;i<args.size();++i){const auto flag=args[i];if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate dark/flat option: "+flag);
        const bool allowed=flag=="--image" || (!keep && !list && flag=="--record") || (keep && (flag=="--dark" || flag=="--flat" || flag=="--scales" || flag=="--minimum-denominator" || flag=="--dark-series" || flag=="--flat-series" || flag=="--save-snapshot-bytes" || flag=="--project-bytes")) || (preview && (flag=="--x" || flag=="--y" || flag=="--width" || flag=="--height" || flag=="--first-channel" || flag=="--channels" || flag=="--z" || flag=="--time" || flag=="--work-bytes"));
        if(!allowed || ++i==args.size())throw std::invalid_argument("Unknown/missing dark/flat option: "+flag);const auto& value=args[i];
        if(flag=="--record"){record=value;continue;}if(flag=="--dark"){dark=value;continue;}if(flag=="--flat"){flat=value;continue;}if(flag=="--scales"){options.scales=scales(value);continue;}if(flag=="--minimum-denominator"){options.minimum_denominator=real(value);continue;}
        const auto n=integer(value,flag=="--image"?UINT64_MAX:4ull*1024*1024*1024);
        if(flag=="--image")image=n;else if(flag=="--save-snapshot-bytes")save_bytes=std::size_t(n);else if(flag=="--project-bytes")file_bytes=n;else if(flag=="--work-bytes")work=std::size_t(n);
        else if(flag=="--x")region.region.x=std::int64_t(n);else if(flag=="--y")region.region.y=std::int64_t(n);else if(flag=="--width")region.region.width=std::int64_t(n);else if(flag=="--height")region.region.height=std::int64_t(n);
        else{if(n>65535)throw std::invalid_argument("Dark/flat axis/series limit");if(flag=="--dark-series")dark_open.view.series=std::uint32_t(n);else if(flag=="--flat-series")flat_open.view.series=std::uint32_t(n);else if(flag=="--first-channel")region.first_channel=std::uint32_t(n);else if(flag=="--channels")region.channels=std::uint32_t(n);else if(flag=="--z")region.z=std::uint32_t(n);else region.t=std::uint32_t(n);}
    }
    if(!image || (!keep && !list && record.empty()))throw std::invalid_argument("Dark/flat needs positive --image and replay/preview needs --record");
    if(keep){options.validate();if(dark.empty() || flat.empty() || !save_bytes || !file_bytes)throw std::invalid_argument("Keep needs explicit dark/flat paths, scales and positive save budgets");}
    if(preview && (!seen.count("--width") || !seen.count("--height") || region.region.width<=0 || region.region.height<=0 || !work || work>1024ull*1024*1024 || !region.channels))throw std::invalid_argument("Preview needs positive --width/--height/channels and1..1024MiB work");
    try{
        workflow_checkpoint(&cancelled);const auto input=export_path(args[2]),output=keep?export_path(args[3]):std::string{};ReaderFileGuard file(input);
        if(keep)protect_spatial_path(input,file.version()->identity,output);
        auto project=Project::open_without_journal_recovery(input);auto doc=project->document(image);std::optional<DarkFlatRequest> request;
        auto protect=[&]{workflow_checkpoint(&cancelled);file.verify();if(request)request->require_current(*doc,&cancelled);if(!keep)return;
            protect_spatial_path(input,file.version()->identity,output);for(const auto* suffix:{"-journal","-wal","-shm"})protect_spatial_path(input+suffix,"",output);
            for(const auto& entry:project->images())if(!entry.embedded && !entry.source_path.empty())protect_spatial_path(export_path(entry.source_path),"",output);
            if(request)for(const auto& source:request->recipe().sources)protect_spatial_path(export_path(source.path),"",output);
        };
        if(list){protect();const auto ids=dark_flat_recipe_ids(*doc);std::cout<<"{\"schema\":\"qpx.dark-flat-cli/1\",\"state\":\"records\",\"records\":[";for(std::size_t i=0;i<ids.size();++i){if(i)std::cout<<',';std::cout<<json_string(ids[i]);}std::cout<<"]}\n";return 0;}
        if(keep){auto d=open_tiff(dark,dark_open,&cancelled),f=open_tiff(flat,flat_open,&cancelled);request.emplace(*doc,d,f,options,&cancelled);protect();record=keep_dark_flat_recipe(*doc,*request,&cancelled);project->save_new(output,save_bytes,file_bytes,&cancelled,protect);}
        else request.emplace(replay_dark_flat_recipe(*doc,record,&cancelled));
        std::shared_ptr<const DarkFlatBlock> block;if(preview)block=read_dark_flat_block(*request,region,work,&cancelled);if(!keep)protect();
        std::cout<<"{\"schema\":\"qpx.dark-flat-cli/1\",\"state\":"<<json_string(keep?"saved":preview?"preview":"verified")<<",\"record\":"<<json_string(record)<<",\"source_sha256\":"<<json_string(request->source()->sha256())<<",\"domain\":"<<json_string(request->source()->domain());
        if(block){std::cout<<",\"block_sha256\":"<<json_string(block->sha256())<<",\"valid_pixels\":"<<block->valid_pixels()<<",\"finite_samples\":";counts(block->finite_samples());std::cout<<",\"nonfinite_samples\":";counts(block->nonfinite_samples());}std::cout<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return cancelled.load()?130:1;}
}
}
