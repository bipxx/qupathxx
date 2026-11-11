#include "qupathxx/expression_aggregation_workflow.hpp"
#include "expression_aggregation_internal.hpp"
#include "qupathxx/project.hpp"
#include "workflow_input.hpp"
#include "spatial_output_guard.hpp"
#include <nlohmann/json.hpp>
#include <set>

namespace qpx {
namespace {
using J=nlohmann::json;
void keys(const J& j,std::initializer_list<const char*> allowed){if(!j.is_object())throw std::invalid_argument("Expression specification requires an object");std::set<std::string> names;for(const auto* k:allowed)names.insert(k);for(auto i=j.begin();i!=j.end();++i)if(!names.count(i.key()))throw std::invalid_argument("Unknown expression specification key: "+i.key());}
std::uint64_t integer(const J& j,std::uint64_t maximum){if(!j.is_number_integer() || j<0 || j>maximum)throw std::invalid_argument("Invalid expression specification integer");return j.get<std::uint64_t>();}
std::string optional(const J& j,const char* name){return j.contains(name)?j.at(name).get<std::string>():std::string{};}
void options(const ExpressionAggregationWorkflowOptions& o){detail::aggregation::work_limit(o.work_bytes);if(!o.image || !o.project_bytes || o.project_bytes>4ull*1024*1024*1024 || o.snapshot_bytes<1024*1024 || o.snapshot_bytes>1024ull*1024*1024)throw std::invalid_argument("Invalid aggregation workflow image/save limits");}
}
std::shared_ptr<const ExpressionAggregationResult> run_expression_aggregation(const std::string& input,const std::string& output,const ExpressionAggregationWorkflowOptions& o,const std::atomic<bool>* cancelled){
    options(o);detail::aggregation::digest(o.matrix_id);if(!o.record_id.empty())throw std::invalid_argument("A new aggregation does not accept a replay record");workflow_path_argument(output);
    detail::WorkflowInput project_input(input,o.project_bytes,true,cancelled),spec_input(o.spec_path,4u*1024*1024,false,cancelled);
    if(std::filesystem::exists(std::filesystem::u8path(output)))throw WorkflowOutputExists("Aggregation output project already exists");
    const auto project=Project::open(project_input.path());const auto document=project->document(o.image);const auto matrix=reopen_expression_matrix(*project,o.image,o.matrix_id,o.work_bytes,cancelled);
    std::size_t reserved=8u*1024*1024;detail::aggregation::charge(reserved,std::size_t(spec_input.bytes()),64,o.work_bytes);
    const auto content=detail::read_workflow_input(spec_input,cancelled);const auto j=J::parse(content,[](int depth,J::parse_event_t,J&){if(depth>8)throw std::invalid_argument("Expression specification nesting exceeds8");return true;});
    keys(j,{"schema","kind","overlap","features","normalization","cohorts","regions","frame_id","coordinates","registration","channel","z","time"});
    if(j.at("schema")!="qpx.expression-aggregation-spec/1")throw std::invalid_argument("Unsupported expression aggregation specification");
    ExpressionAggregationOptions a;a.work_bytes=o.work_bytes;
    if(j.at("kind")=="pseudobulk")a.kind=ExpressionAggregationKind::Pseudobulk;else if(j.at("kind")!="regional-summary")throw std::invalid_argument("Unknown expression aggregation kind");
    if(j.at("overlap")=="independent")a.overlap=ExpressionOverlapPolicy::Independent;else if(j.at("overlap")!="reject")throw std::invalid_argument("Unknown expression overlap policy");
    if(j.contains("features")){const auto& f=j.at("features");if(!f.is_array() || f.empty() || f.size()>65536)throw std::invalid_argument("Explicit feature selection requires1..65536 IDs");for(const auto& id:f){workflow_checkpoint(cancelled);const auto found=matrix->feature_index(id.get<std::string>(),cancelled);if(!found)throw std::invalid_argument("Expression feature ID absent");a.features.push_back(*found);}std::sort(a.features.begin(),a.features.end());if(std::adjacent_find(a.features.begin(),a.features.end())!=a.features.end())throw std::invalid_argument("Duplicate expression feature ID");}
    if(j.contains("normalization"))a.normalization_recipe=expression_normalization_recipe(*project,o.image,j.at("normalization").get<std::string>());
    std::vector<ExpressionCohort> cohorts;
    if(j.contains("regions")){
        if(j.contains("cohorts"))throw std::invalid_argument("Choose regions or explicit observation cohorts");const auto& r=j.at("regions");if(!r.is_array() || r.empty() || r.size()>64)throw std::invalid_argument("Region specification requires1..64 regions");
        const auto channel=j.contains("channel")?std::int32_t(integer(j.at("channel"),65535)):-1;const ObjectPlane plane(channel,j.contains("z")?std::uint32_t(integer(j.at("z"),65535)):0,j.contains("time")?std::uint32_t(integer(j.at("time"),65535)):0);
        const auto frame_id=j.at("frame_id").get<std::string>();const auto spatial=ExpressionMatrixSpatialSnapshot::capture(*document,matrix,frame_id,plane,o.work_bytes,cancelled);std::vector<ExpressionRegion> regions;regions.reserve(r.size());
        for(const auto& region:r){keys(region,{"id","roi","sample","replicate","group"});const auto roi=integer(region.at("roi"),UINT64_MAX);if(!roi)throw std::invalid_argument("Region ROI ID must be positive");const auto object=document->annotation(roi);if(object.kind!=ObjectKind::Annotation || !object.geometry.is_area() || object.plane!=plane)throw std::invalid_argument("Expression region requires an area annotation on the selected plane");regions.push_back({region.at("id").get<std::string>(),object.geometry,optional(region,"sample"),optional(region,"replicate"),optional(region,"group")});}
        std::optional<RegistrationDefinition> registration;const auto coordinates=j.at("coordinates").get<std::string>();
        if(coordinates=="registered"){if(!j.contains("registration"))throw std::invalid_argument("Registered regions require an explicit registration ID");registration=stored_registration(document->image_data_properties(),j.at("registration").get<std::string>());if(!registration)throw std::invalid_argument("Requested expression registration absent");}
        else if(coordinates!="source" || j.contains("registration"))throw std::invalid_argument("Select explicit source coordinates or registered coordinates with an ID");
        cohorts=expression_region_cohorts(spatial,regions,image_coordinate_frame(*document,frame_id,plane,cancelled),a.overlap,registration,o.work_bytes,100000000,cancelled);
    }else{
        for(const auto* key:{"frame_id","coordinates","registration","channel","z","time"})if(j.contains(key))throw std::invalid_argument("Coordinate options require region selection");
        const auto& c=j.at("cohorts");if(!c.is_array() || c.empty() || c.size()>64)throw std::invalid_argument("Explicit specification requires1..64 cohorts");std::size_t memberships=0;
        for(const auto& group:c){keys(group,{"id","observations","sample","replicate","group"});const auto& observations=group.at("observations");if(!observations.is_array() || observations.size()>65536-memberships)throw WorkflowCaptureLimit("Explicit memberships exceed65536");memberships+=observations.size();std::vector<std::string> ids;ids.reserve(observations.size());for(const auto& id:observations)ids.push_back(id.get<std::string>());cohorts.push_back(expression_observation_cohort(matrix,group.at("id").get<std::string>(),ids,optional(group,"sample"),optional(group,"replicate"),optional(group,"group"),o.work_bytes,cancelled));}
    }
    // Account parsed JSON and captured membership storage while reduction runs.
    workflow_capture_charge(reserved,detail::aggregation::cohort_bytes(cohorts),o.work_bytes);a.work_bytes=o.work_bytes-reserved;
    const auto result=aggregate_expression(matrix,cohorts,a,cancelled);const auto guard=[&]{project_input.verify(false,cancelled);spec_input.verify(false,cancelled);matrix->require_current(cancelled);detail::protect_spatial_path(project_input.path(),"",output);detail::protect_spatial_path(spec_input.path(),"",output);detail::protect_spatial_path(matrix->path(),"",output);};guard();keep_expression_aggregation(*project,o.image,*result);project->save_new(output,o.snapshot_bytes,o.project_bytes,cancelled,guard);return result;
}
std::shared_ptr<const ExpressionAggregationResult> open_project_expression_aggregation(const std::string& input,const ExpressionAggregationWorkflowOptions& o,const std::atomic<bool>* cancelled){
    options(o);detail::aggregation::digest(o.record_id);if(!o.spec_path.empty() || !o.matrix_id.empty())throw std::invalid_argument("Replay accepts a retained record, not a new specification/matrix");const detail::WorkflowInput file(input,o.project_bytes,true,cancelled);const auto project=Project::open(file.path());const auto result=expression_aggregation_open_request(*project,o.image,o.record_id).open(o.work_bytes,cancelled);file.verify(false,cancelled);return result;
}
}
