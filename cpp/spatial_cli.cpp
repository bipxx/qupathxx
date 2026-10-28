#include "spatial_cli.hpp"
#include "qupathxx/spatial_analysis.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/source_identity.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "spatial_output_guard.hpp"
#include "batch_checkpoint.hpp"
#include "utf8_text.hpp"
#include <charconv>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <set>
namespace qpx::detail {
namespace {
std::vector<std::uint64_t> read_ids(const ReaderFileGuard& guard,std::size_t limit,const std::atomic<bool>& cancelled) {
    const auto version=guard.version();if(version->bytes>limit) throw std::length_error("spatial ID file exceeds its byte limit");
    std::string text(std::size_t(version->bytes),'\0');std::ifstream input(std::filesystem::u8path(version->path),std::ios::binary);
    input.read(text.data(),std::streamsize(text.size()));
    if(!input || input.gcount()!=std::streamsize(text.size()) || input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("spatial ID file changed or could not be read");
    guard.verify();std::vector<std::uint64_t> ids;ids.reserve(std::min<std::size_t>(100000,text.size()/2+1));
    auto space=[](char c){return c==' ' || c=='\t' || c=='\r' || c=='\n';};
    for(std::size_t at=0;at<text.size();) {
        export_checkpoint(&cancelled);if(space(text[at])) {++at;continue;}
        const auto begin=at;while(at<text.size() && !space(text[at])) ++at;
        std::uint64_t id=0;const auto parsed=std::from_chars(text.data()+begin,text.data()+at,id);
        if(!id || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+at) throw std::invalid_argument("spatial ID files require whitespace-separated positive unsigned integers");
        if(ids.size()==100000) throw std::length_error("spatial ID file exceeds 100000 IDs");ids.push_back(id);
    }
    std::sort(ids.begin(),ids.end());if(std::adjacent_find(ids.begin(),ids.end())!=ids.end()) throw std::invalid_argument("duplicate spatial cohort ID");return ids;
}

}
std::optional<int> spatial_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2 || (args[1]!="--measure-spatial" && args[1]!="--export-spatial" && args[1]!="--verify-spatial")) return {};
    const bool measure=args[1]=="--measure-spatial",verify=args[1]=="--verify-spatial";const std::size_t positional=verify?3:4;
    if(args.size()<positional) throw std::invalid_argument("spatial commands require PROJECT and, for measure/export, a new OUTPUT; see --help");
    std::uint64_t image=0,record_id=0,project_bytes=1024ull*1024*1024;
    std::size_t save_bytes=512ull*1024*1024,csv_bytes=64u*1024*1024,ids_bytes=4u*1024*1024;
    SpatialDistanceOptions options;std::vector<std::uint64_t> sources,targets;std::set<std::string> seen;std::string name="Spatial distances",source_ids_file,target_ids_file;
    for(std::size_t i=positional;i<args.size();++i) {
        const auto flag=args[i];
        if(flag!="--source" && flag!="--target" && !seen.insert(flag).second) throw std::invalid_argument("duplicate spatial option: "+flag);
        if(measure && (flag=="--body" || flag=="--include-self")) {if(flag=="--body") options.prefer_nucleus=false;else options.exclude_self=false;continue;}
        const bool number=flag=="--image" || (!measure && flag=="--record") ||
            (measure && (flag=="--source" || flag=="--target" || flag=="--snapshot-bytes" || flag=="--work-bytes" || flag=="--max-distance-tests" || flag=="--save-snapshot-bytes" || flag=="--project-bytes" || flag=="--ids-bytes")) || (!measure && !verify && flag=="--csv-bytes");
        if(!number && !(measure && (flag=="--units" || flag=="--name" || flag=="--source-ids" || flag=="--target-ids"))) throw std::invalid_argument("unknown spatial option: "+flag);
        if(++i==args.size()) throw std::invalid_argument("missing value for spatial option: "+flag);
        const auto& value=args[i];
        if(flag=="--units") {if(value=="px") options.units=SpatialDistanceUnits::Pixels;else if(value=="um") options.units=SpatialDistanceUnits::Micrometres;else throw std::invalid_argument("spatial units must be px or um");continue;}
        if(flag=="--name") {bounded_utf8(value,256);if(value.empty()) throw std::invalid_argument("spatial record name is empty");name=value;continue;}
        if(flag=="--source-ids" || flag=="--target-ids") {if(value.empty()) throw std::invalid_argument("spatial ID file path is empty");(flag=="--source-ids"?source_ids_file:target_ids_file)=value;continue;}
        std::uint64_t n=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),n);
        if(value.empty() || !n || parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) throw std::invalid_argument("spatial IDs/budgets require positive unsigned integers");
        if(flag=="--image") image=n;else if(flag=="--record") record_id=n;
        else if(flag=="--source" || flag=="--target") {auto& ids=flag=="--source"?sources:targets;if(ids.size()>=100000) throw std::invalid_argument("spatial cohort exceeds 100000 IDs");ids.push_back(n);}
        else if(flag=="--max-distance-tests") options.max_distance_tests=n;
        else {
            if(n>4ull*1024*1024*1024 || n>SIZE_MAX) throw std::invalid_argument("spatial budget exceeds 4 GiB");
            if(flag=="--snapshot-bytes") options.snapshot_bytes=std::size_t(n);else if(flag=="--work-bytes") options.work_bytes=std::size_t(n);
            else if(flag=="--save-snapshot-bytes") save_bytes=std::size_t(n);else if(flag=="--project-bytes") project_bytes=n;
            else if(flag=="--ids-bytes") {if(n>4*1024*1024) throw std::invalid_argument("spatial ID file limit exceeds 4 MiB");ids_bytes=std::size_t(n);}
            else {if(n>1024u*1024*1024) throw std::invalid_argument("spatial CSV exceeds 1 GiB limit");csv_bytes=std::size_t(n);}
        }
    }
    if(!image || (measure?(sources.empty() && source_ids_file.empty()):!record_id)) throw std::invalid_argument("spatial commands require --image and either --source/--source-ids or --record");
    if((!source_ids_file.empty() && !sources.empty()) || (!target_ids_file.empty() && !targets.empty())) throw std::invalid_argument("each spatial cohort uses repeated IDs or one ID file, not both");
    for(auto* ids:{&sources,&targets}) {std::sort(ids->begin(),ids->end());if(std::adjacent_find(ids->begin(),ids->end())!=ids->end()) throw std::invalid_argument("duplicate spatial cohort ID");}
    options.validate();
    try {
        export_checkpoint(&cancelled);const auto source=export_path(args[2]);const auto output=verify?std::string{}:export_path(args[3]);
        ReaderFileGuard observed(source);
        if(!verify) protect_spatial_path(source,observed.version()->identity,output);
        std::unique_ptr<ReaderFileGuard> source_ids_guard,target_ids_guard;
        if(!source_ids_file.empty()) {source_ids_guard=std::make_unique<ReaderFileGuard>(export_path(source_ids_file));sources=read_ids(*source_ids_guard,ids_bytes,cancelled);if(sources.empty()) throw std::invalid_argument("spatial source ID file is empty");}
        if(!target_ids_file.empty()) {target_ids_guard=std::make_unique<ReaderFileGuard>(export_path(target_ids_file));targets=read_ids(*target_ids_guard,ids_bytes,cancelled);}
        const auto project=Project::open_without_journal_recovery(source);const auto document=project->document(image);const auto images=project->images();
        std::shared_ptr<const SpatialDistanceResult> result;
        auto protect=[&] {
            observed.verify();export_checkpoint(&cancelled);if(verify) return;
            protect_spatial_path(source,observed.version()->identity,output);
            for(const auto* guard:{source_ids_guard.get(),target_ids_guard.get()}) if(guard) {guard->verify();protect_spatial_path(guard->version()->path,guard->version()->identity,output);}
            for(const auto* suffix:{"-journal","-wal","-shm"}) protect_spatial_path(source+suffix,"",output);
            for(const auto& entry:images) if(!entry.embedded && !entry.source_path.empty()) protect_spatial_path(export_path(entry.source_path),"",output);
            if(result && result->provenance()->reader_source) {
                const auto& files=*result->provenance()->reader_source;
                if(files.primary) protect_spatial_path(files.primary->path,files.primary->identity,output);
                for(const auto& file:files.companions) protect_spatial_path(file.path,file.identity,output);
            }
        };
        protect();std::size_t bytes=0;
        if(measure) {
            SpatialDistanceRequest request(*document,sources,targets,options);result=measure_spatial_distances(request,&cancelled);
            export_checkpoint(&cancelled);record_id=document->keep_spatial(request,result,name);protect();
            project->save_new(output,save_bytes,project_bytes,&cancelled,protect);
        } else {
            result=document->spatial_record(record_id)->result;
            if(verify) {const auto request=SpatialDistanceRequest::replay(*document,record_id);const auto fresh=measure_spatial_distances(request,&cancelled);if(!document->verify_spatial(record_id,request,fresh)) throw std::runtime_error("spatial verification differs from the saved result");protect();}
            else bytes=save_spatial_distance_csv(*result,output,csv_bytes,false,&cancelled,protect);
        }
        std::cout<<"{\"schema\":\"qpx.spatial-cli/1\",\"command\":"<<batch_json_quote(measure?"measure":verify?"verify":"export")
            <<",\"state\":"<<batch_json_quote(verify?"verified":"saved")<<",\"image_id\":"<<image<<",\"record_id\":"<<record_id
            <<",\"rows\":"<<result->rows().size()<<",\"sources\":"<<result->source_count()<<",\"targets\":"<<result->target_count()
            <<",\"units\":"<<batch_json_quote(result->options().units==SpatialDistanceUnits::Pixels?"px":"um")
            <<",\"algorithm\":"<<batch_json_quote(result->algorithm())<<",\"input_sha256\":"<<batch_json_quote(result->input_sha256())
            <<",\"result_sha256\":"<<batch_json_quote(result->sha256())<<",\"historical\":"<<(!measure && !verify?"true":"false");
        if(!verify) std::cout<<",\"output\":"<<batch_json_quote(output);
        if(!measure && !verify) std::cout<<",\"bytes\":"<<bytes;
        std::cout<<"}\n";return 0;
    }catch(const std::exception& e) {std::cerr<<"{\"schema\":\"qpx.batch-error/1\",\"error\":"<<batch_json_quote(e.what())<<"}\n";return cancelled.load()?130:1;}
}
}
