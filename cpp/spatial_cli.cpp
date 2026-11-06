#include "spatial_cli.hpp"
#include "qupathxx/spatial_neighborhood.hpp"
#include "qupathxx/spatial_density.hpp"
#include <type_traits>
#include "qupathxx/project.hpp"
#include "qupathxx/source_identity.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "spatial_output_guard.hpp"
#include "batch_checkpoint.hpp"
#include "utf8_text.hpp"
#include <charconv>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iomanip>
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
    bool neighborhood=false,density=false;SpatialDensityOptions density_options;std::optional<double> radius;
    SpatialDistanceOptions options;std::vector<std::uint64_t> sources,targets,tissue,excluded;std::set<std::string> seen;std::string name="Spatial distances",source_ids_file,target_ids_file,tissue_ids_file,excluded_ids_file;
    for(std::size_t i=positional;i<args.size();++i) {
        const auto flag=args[i];
        if(flag!="--source" && flag!="--target" && flag!="--tissue" && flag!="--exclude" && !seen.insert(flag).second) throw std::invalid_argument("duplicate spatial option: "+flag);
        if(measure && (flag=="--body" || flag=="--include-self" || flag=="--polygonize-curves")) {if(flag=="--body") options.prefer_nucleus=false;else if(flag=="--include-self") options.exclude_self=false;else density_options.polygonize_curves=true;continue;}
        const bool number=flag=="--image" || (!measure && flag=="--record") ||
            (measure && (flag=="--source" || flag=="--target" || flag=="--tissue" || flag=="--exclude" || flag=="--max-membership-tests" || flag=="--max-geometry-operations" || flag=="--max-geometry-vertices" || flag=="--snapshot-bytes" || flag=="--work-bytes" || flag=="--max-distance-tests" || flag=="--save-snapshot-bytes" || flag=="--project-bytes" || flag=="--ids-bytes")) || (!measure && !verify && flag=="--csv-bytes");
        if(!number && flag!="--operation" && !(measure && (flag=="--radius" || flag=="--units" || flag=="--name" || flag=="--source-ids" || flag=="--target-ids" || flag=="--tissue-ids" || flag=="--exclude-ids"))) throw std::invalid_argument("unknown spatial option: "+flag);
        if(++i==args.size()) throw std::invalid_argument("missing value for spatial option: "+flag);
        const auto& value=args[i];
        if(flag=="--operation") {if(value=="nearest-distance") {neighborhood=false;density=false;}else if(value=="radius-count") {neighborhood=true;density=false;}else if(value=="tissue-density") {density=true;neighborhood=false;}else throw std::invalid_argument("spatial operation must be nearest-distance, radius-count or tissue-density");continue;}
        if(flag=="--radius") {
            double v=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),v);
            if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !std::isfinite(v) || v<=0) throw std::invalid_argument("spatial radius must be positive and finite");
            radius=v;continue;
        }
        if(flag=="--units") {if(value=="px") options.units=SpatialDistanceUnits::Pixels;else if(value=="um") options.units=SpatialDistanceUnits::Micrometres;else throw std::invalid_argument("spatial units must be px or um");continue;}
        if(flag=="--name") {bounded_utf8(value,256);if(value.empty()) throw std::invalid_argument("spatial record name is empty");name=value;continue;}
        if(flag=="--source-ids" || flag=="--target-ids" || flag=="--tissue-ids" || flag=="--exclude-ids") {
            if(value.empty()) throw std::invalid_argument("spatial ID file path is empty");
            if(flag=="--source-ids")source_ids_file=value;else if(flag=="--target-ids")target_ids_file=value;else if(flag=="--tissue-ids")tissue_ids_file=value;else excluded_ids_file=value;continue;
        }
        std::uint64_t n=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),n);
        if(value.empty() || !n || parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) throw std::invalid_argument("spatial IDs/budgets require positive unsigned integers");
        if(flag=="--image") image=n;else if(flag=="--record") record_id=n;
        else if(flag=="--source" || flag=="--target" || flag=="--tissue" || flag=="--exclude") {auto& ids=flag=="--source"?sources:flag=="--target"?targets:flag=="--tissue"?tissue:excluded;if(ids.size()>=100000) throw std::invalid_argument("spatial cohort exceeds 100000 IDs");ids.push_back(n);}
        else if(flag=="--max-distance-tests") options.max_distance_tests=n;
        else if(flag=="--max-membership-tests") density_options.max_membership_tests=n;
        else if(flag=="--max-geometry-operations") density_options.max_geometry_operations=n;
        else if(flag=="--max-geometry-vertices") {if(n>1000000) throw std::invalid_argument("density vertex budget exceeds1000000");density_options.max_geometry_vertices=std::size_t(n);}
        else {
            if(n>4ull*1024*1024*1024 || n>SIZE_MAX) throw std::invalid_argument("spatial budget exceeds 4 GiB");
            if(flag=="--snapshot-bytes") options.snapshot_bytes=std::size_t(n);else if(flag=="--work-bytes") options.work_bytes=std::size_t(n);
            else if(flag=="--save-snapshot-bytes") save_bytes=std::size_t(n);else if(flag=="--project-bytes") project_bytes=n;
            else if(flag=="--ids-bytes") {if(n>4*1024*1024) throw std::invalid_argument("spatial ID file limit exceeds 4 MiB");ids_bytes=std::size_t(n);}
            else {if(n>1024u*1024*1024) throw std::invalid_argument("spatial CSV exceeds 1 GiB limit");csv_bytes=std::size_t(n);}
        }
    }
    if(!image || (measure?(sources.empty() && source_ids_file.empty()):!record_id)) throw std::invalid_argument("spatial commands require --image and either --source/--source-ids or --record");
    if((!source_ids_file.empty() && !sources.empty()) || (!target_ids_file.empty() && !targets.empty()) || (!tissue_ids_file.empty() && !tissue.empty()) || (!excluded_ids_file.empty() && !excluded.empty())) throw std::invalid_argument("each spatial cohort uses repeated IDs or one ID file, not both");
    for(auto* ids:{&sources,&targets,&tissue,&excluded}) {std::sort(ids->begin(),ids->end());if(std::adjacent_find(ids->begin(),ids->end())!=ids->end()) throw std::invalid_argument("duplicate spatial cohort ID");}
    options.validate();
    if(radius && !neighborhood) throw std::invalid_argument("--radius requires --operation radius-count");
    if(neighborhood && measure && !radius) throw std::invalid_argument("radius-count measurement requires --radius");
    if(neighborhood && !seen.count("--name")) name="Spatial neighborhoods";
    if(density) {
        if(seen.count("--units") || seen.count("--include-self") || seen.count("--max-distance-tests")) throw std::invalid_argument("density uses cells/mm2, all selected Cell IDs and --max-membership-tests");
        density_options.prefer_nucleus=options.prefer_nucleus;density_options.snapshot_bytes=options.snapshot_bytes;density_options.work_bytes=options.work_bytes;density_options.validate();
        if(measure && tissue.empty() && tissue_ids_file.empty()) throw std::invalid_argument("density measurement requires --tissue or --tissue-ids");
        if(!seen.count("--name")) name="Tissue cell density";
    }else for(const auto* flag:{"--tissue","--exclude","--tissue-ids","--exclude-ids","--polygonize-curves","--max-membership-tests","--max-geometry-operations","--max-geometry-vertices"})
        if(seen.count(flag) || (std::string(flag)=="--tissue" && !tissue.empty()) || (std::string(flag)=="--exclude" && !excluded.empty())) throw std::invalid_argument("density option requires --operation tissue-density");
    try {
        export_checkpoint(&cancelled);const auto source=export_path(args[2]);const auto output=verify?std::string{}:export_path(args[3]);
        ReaderFileGuard observed(source);
        if(!verify) protect_spatial_path(source,observed.version()->identity,output);
        std::unique_ptr<ReaderFileGuard> source_ids_guard,target_ids_guard,tissue_ids_guard,excluded_ids_guard;
        if(!source_ids_file.empty()) {source_ids_guard=std::make_unique<ReaderFileGuard>(export_path(source_ids_file));sources=read_ids(*source_ids_guard,ids_bytes,cancelled);if(sources.empty()) throw std::invalid_argument("spatial source ID file is empty");}
        if(!target_ids_file.empty()) {target_ids_guard=std::make_unique<ReaderFileGuard>(export_path(target_ids_file));targets=read_ids(*target_ids_guard,ids_bytes,cancelled);}
        if(!tissue_ids_file.empty()) {tissue_ids_guard=std::make_unique<ReaderFileGuard>(export_path(tissue_ids_file));tissue=read_ids(*tissue_ids_guard,ids_bytes,cancelled);if(tissue.empty()) throw std::invalid_argument("density tissue ID file is empty");}
        if(!excluded_ids_file.empty()) {excluded_ids_guard=std::make_unique<ReaderFileGuard>(export_path(excluded_ids_file));excluded=read_ids(*excluded_ids_guard,ids_bytes,cancelled);}
        const auto project=Project::open_without_journal_recovery(source);const auto document=project->document(image);const auto images=project->images();
        std::shared_ptr<const SpatialDistanceResult> result;
        std::shared_ptr<const SpatialNeighborhoodResult> neighborhood_result;
        std::shared_ptr<const SpatialDensityResult> density_result;
        std::shared_ptr<const AnalysisProvenance> provenance;
        auto protect=[&] {
            observed.verify();export_checkpoint(&cancelled);if(verify) return;
            protect_spatial_path(source,observed.version()->identity,output);
            for(const auto* guard:{source_ids_guard.get(),target_ids_guard.get(),tissue_ids_guard.get(),excluded_ids_guard.get()}) if(guard) {guard->verify();protect_spatial_path(guard->version()->path,guard->version()->identity,output);}
            for(const auto* suffix:{"-journal","-wal","-shm"}) protect_spatial_path(source+suffix,"",output);
            for(const auto& entry:images) if(!entry.embedded && !entry.source_path.empty()) protect_spatial_path(export_path(entry.source_path),"",output);
            if(provenance && provenance->reader_source) {
                const auto& files=*provenance->reader_source;
                if(files.primary) protect_spatial_path(files.primary->path,files.primary->identity,output);
                for(const auto& file:files.companions) protect_spatial_path(file.path,file.identity,output);
            }
        };
        protect();std::size_t bytes=0;
        if(density) {
            if(measure) {
                SpatialDensityRequest request(*document,sources,tissue,excluded,targets,density_options);density_result=measure_spatial_density(request,&cancelled);provenance=density_result->provenance();
                export_checkpoint(&cancelled);record_id=document->keep_density(request,density_result,name);protect();project->save_new(output,save_bytes,project_bytes,&cancelled,protect);
            }else {
                density_result=document->density_record(record_id)->result;provenance=density_result->provenance();
                if(verify) {const auto request=SpatialDensityRequest::replay(*document,record_id);const auto fresh=measure_spatial_density(request,&cancelled);if(!document->verify_density(record_id,request,fresh)) throw std::runtime_error("density verification differs from saved result");protect();}
                else bytes=save_spatial_density_csv(*density_result,output,csv_bytes,false,&cancelled,protect);
            }
        }else if(neighborhood) {
            if(measure) {
                SpatialNeighborhoodOptions parameters;static_cast<SpatialDistanceOptions&>(parameters)=options;parameters.radius=*radius;
                SpatialNeighborhoodRequest request(*document,sources,targets,parameters);neighborhood_result=measure_spatial_neighborhoods(request,&cancelled);provenance=neighborhood_result->provenance();
                export_checkpoint(&cancelled);record_id=document->keep_neighborhood(request,neighborhood_result,name);protect();
                project->save_new(output,save_bytes,project_bytes,&cancelled,protect);
            } else {
                neighborhood_result=document->neighborhood_record(record_id)->result;provenance=neighborhood_result->provenance();
                if(verify) {const auto request=SpatialNeighborhoodRequest::replay(*document,record_id);const auto fresh=measure_spatial_neighborhoods(request,&cancelled);if(!document->verify_neighborhood(record_id,request,fresh)) throw std::runtime_error("neighborhood verification differs from saved result");protect();}
                else bytes=save_spatial_neighborhood_csv(*neighborhood_result,output,csv_bytes,false,&cancelled,protect);
            }
        } else if(measure) {
            SpatialDistanceRequest request(*document,sources,targets,options);result=measure_spatial_distances(request,&cancelled);provenance=result->provenance();
            export_checkpoint(&cancelled);record_id=document->keep_spatial(request,result,name);protect();
            project->save_new(output,save_bytes,project_bytes,&cancelled,protect);
        } else {
            result=document->spatial_record(record_id)->result;provenance=result->provenance();
            if(verify) {const auto request=SpatialDistanceRequest::replay(*document,record_id);const auto fresh=measure_spatial_distances(request,&cancelled);if(!document->verify_spatial(record_id,request,fresh)) throw std::runtime_error("spatial verification differs from the saved result");protect();}
            else bytes=save_spatial_distance_csv(*result,output,csv_bytes,false,&cancelled,protect);
        }
        auto print_result=[&](const auto& result) {
        constexpr bool is_density=std::is_same_v<std::decay_t<decltype(*result)>,SpatialDensityResult>;
        std::size_t source_count=0,target_count=0;const char* units=nullptr;
        if constexpr(is_density) {source_count=result->window_count();target_count=result->cell_count();units="cells/mm2";}
        else {source_count=result->source_count();target_count=result->target_count();units=result->options().units==SpatialDistanceUnits::Pixels?"px":"um";}
        std::cout<<"{\"schema\":"<<batch_json_quote(density?"qpx.spatial-density-cli/1":neighborhood?"qpx.spatial-neighborhood-cli/1":"qpx.spatial-cli/1")<<",\"command\":"<<batch_json_quote(measure?"measure":verify?"verify":"export")
            <<",\"state\":"<<batch_json_quote(verify?"verified":"saved")<<",\"image_id\":"<<image<<",\"record_id\":"<<record_id
            <<",\"rows\":"<<result->rows().size()<<",\"sources\":"<<source_count<<",\"targets\":"<<target_count
            <<",\"units\":"<<batch_json_quote(units)
            <<",\"algorithm\":"<<batch_json_quote(result->algorithm())<<",\"input_sha256\":"<<batch_json_quote(result->input_sha256())
            <<",\"result_sha256\":"<<batch_json_quote(result->sha256())<<",\"historical\":"<<(!measure && !verify?"true":"false");
        if constexpr(is_density) std::cout<<",\"operation\":\"tissue-density\",\"tissue_rois\":"<<result->tissue_count()<<",\"exclusions\":"<<result->exclusion_count()<<",\"geometry_policy\":"<<batch_json_quote(result->geometry_policy());
        if(neighborhood) std::cout<<std::setprecision(17)<<",\"operation\":\"radius-count\",\"radius\":"<<neighborhood_result->options().radius;
        if(!verify) std::cout<<",\"output\":"<<batch_json_quote(output);
        if(!measure && !verify) std::cout<<",\"bytes\":"<<bytes;
        std::cout<<"}\n";
        };
        if(density)print_result(density_result);else if(neighborhood)print_result(neighborhood_result);else print_result(result);return 0;
    }catch(const std::exception& e) {std::cerr<<"{\"schema\":\"qpx.batch-error/1\",\"error\":"<<batch_json_quote(e.what())<<"}\n";return cancelled.load()?130:1;}
}
}
