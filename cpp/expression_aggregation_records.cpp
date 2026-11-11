#include "expression_aggregation_internal.hpp"
#include "qupathxx/project.hpp"
#include <nlohmann/json.hpp>

namespace qpx::detail::aggregation {
namespace {
constexpr const char* prefix="qpx.expression-aggregation/1\n";
using J=nlohmann::json;
std::uint64_t integer(const J& j,std::uint64_t maximum){if(!j.is_number_integer() || j<0 || j>maximum)throw std::invalid_argument("Invalid aggregation recipe integer");return j.get<std::uint64_t>();}
}
std::string encode(const Recipe& r){
    ExpressionMatrixMetadata m;m.source_sha256=r.source;m.features=r.features;m.observations=r.observations;
    // The parent domain is checked against the reopened native source at replay.
    // A normalized recipe is opaque, separately versioned and validated by A01.
    validate(m,r.file,r.cohorts,r.options);digest(r.result);
    J groups=J::array();for(const auto& c:r.cohorts)groups.push_back({{"id",c.id},{"sample",c.sample},{"replicate",c.replicate},{"group",c.group},{"observations",c.observations},{"origin",c.origin}});
    J j={{"source",r.source},{"file",r.file},{"source_features",r.features},{"source_observations",r.observations},{"features",r.options.features},{"kind",r.options.kind==ExpressionAggregationKind::Pseudobulk?"pseudobulk":"regional-summary"},{"overlap",r.options.overlap==ExpressionOverlapPolicy::Reject?"reject":"independent"},{"normalization",r.options.normalization_recipe},{"cohorts",std::move(groups)},{"result",r.result},{"membership","captured-source-axis-indices"},{"implicit_zero","measured"},{"empty_mean","absent"}};
    const auto body=std::string(prefix)+j.dump(-1,' ',true)+"\n";if(body.size()>recipe_limit-64)throw WorkflowCaptureLimit("Aggregation recipe exceeds768KiB");Sha256 h;h.update(body.data(),body.size());return body+h.finish();
}
Recipe parse(const std::string& saved){
    const auto first=std::char_traits<char>::length(prefix);if(saved.size()<first+64 || saved.size()>recipe_limit || saved.rfind(prefix,0))throw std::invalid_argument("Invalid aggregation recipe size/schema");const auto size=saved.size()-64;Sha256 h;h.update(saved.data(),size);if(h.finish()!=saved.substr(size))throw std::invalid_argument("Aggregation recipe checksum differs");
    const auto j=J::parse(saved.substr(first,size-first),[](int depth,J::parse_event_t,J&){if(depth>8)throw std::invalid_argument("Aggregation recipe nesting exceeds schema");return true;});
    if(!j.is_object() || j.size()!=13 || j.at("membership")!="captured-source-axis-indices" || j.at("implicit_zero")!="measured" || j.at("empty_mean")!="absent")throw std::invalid_argument("Unsupported aggregation recipe semantics");
    Recipe r;r.source=j.at("source").get<std::string>();r.file=j.at("file").get<std::string>();r.result=j.at("result").get<std::string>();r.features=integer(j.at("source_features"),1000000);r.observations=integer(j.at("source_observations"),1000000);
    if(j.at("kind")=="pseudobulk")r.options.kind=ExpressionAggregationKind::Pseudobulk;else if(j.at("kind")!="regional-summary")throw std::invalid_argument("Unknown aggregation recipe kind");
    if(j.at("overlap")=="independent")r.options.overlap=ExpressionOverlapPolicy::Independent;else if(j.at("overlap")!="reject")throw std::invalid_argument("Unknown aggregation recipe overlap");
    r.options.normalization_recipe=j.at("normalization").get<std::string>();const auto& features=j.at("features");if(!features.is_array() || features.size()>1000000)throw std::invalid_argument("Invalid aggregation recipe feature selection");for(const auto& f:features)r.options.features.push_back(integer(f,999999));
    const auto& cohorts=j.at("cohorts");if(!cohorts.is_array() || cohorts.empty() || cohorts.size()>64)throw std::invalid_argument("Invalid aggregation recipe cohorts");std::size_t members=0;
    for(const auto& c:cohorts){if(!c.is_object() || c.size()!=6 || !c.at("observations").is_array() || c.at("observations").size()>65536-members)throw std::invalid_argument("Invalid aggregation cohort shape/membership bound");ExpressionCohort g;g.source_sha256=r.source;g.id=c.at("id").get<std::string>();g.sample=c.at("sample").get<std::string>();g.replicate=c.at("replicate").get<std::string>();g.group=c.at("group").get<std::string>();g.origin=c.at("origin").get<std::string>();for(const auto& o:c.at("observations"))g.observations.push_back(integer(o,999999));members+=g.observations.size();r.cohorts.push_back(std::move(g));}
    if(encode(r)!=saved)throw std::invalid_argument("Noncanonical aggregation recipe");return r;
}
}
namespace qpx {
std::shared_ptr<const ExpressionAggregationResult> replay_expression_aggregation(std::shared_ptr<const ExpressionMatrixSource> source,const std::string& saved,std::size_t work,const std::atomic<bool>* cancelled){
    using namespace detail::aggregation;work_limit(work);workflow_checkpoint(cancelled);std::size_t reserved=8u*recipe_limit;workflow_capture_charge(reserved,saved.capacity(),work);const auto r=parse(saved);if(!source || source->metadata().source_sha256!=r.source || source->file_sha256()!=r.file || source->metadata().features!=r.features || source->metadata().observations!=r.observations)throw WorkflowInputChanged("Saved aggregation parent identity differs");
    workflow_capture_charge(reserved,cohort_bytes(r.cohorts)+r.options.features.capacity()*8+r.options.normalization_recipe.capacity(),work);auto options=r.options;options.work_bytes=work-reserved;const auto result=aggregate_expression(std::move(source),r.cohorts,options,cancelled);if(result->recipe()!=saved || result->sha256()!=r.result)throw WorkflowInputChanged("Saved aggregation result/replay identity differs");return result;
}
namespace {
using V=ImageDataValue;
constexpr const char* property="qupathxx.expression-aggregations/1";
template<class T>const T& typed(const V& v){const auto* p=std::get_if<T>(&v.value);if(!p)throw std::invalid_argument("Invalid aggregation metadata type");return *p;}
const V::Map& records(const Document& d){static const V::Map empty;const auto p=d.image_data_properties();if(!p)return empty;const auto i=p->values().find(property);return i==p->values().end()?empty:typed<V::Map>(i->second);}
std::string decode(const std::string& id,const V& v){detail::aggregation::digest(id);const auto& chunks=typed<V::List>(v);if(chunks.empty() || chunks.size()>192)throw std::invalid_argument("Invalid aggregation recipe chunks");std::string saved;for(std::size_t i=0;i<chunks.size();++i){const auto& s=typed<std::string>(chunks[i]);if(s.empty() || s.size()>4096 || (i+1<chunks.size() && s.size()!=4096))throw std::invalid_argument("Noncanonical aggregation chunk");saved+=s;}detail::aggregation::parse(saved);if(saved.substr(saved.size()-64)!=id)throw std::invalid_argument("Aggregation record identity differs");return saved;}
void publish(Document& d,V::Map all){auto values=d.image_data_properties()?d.image_data_properties()->values():V::Map{};if(all.empty())values.erase(property);else values[property]=V{std::move(all)};d.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));}
}
std::string keep_expression_aggregation(Project& project,std::uint64_t image,const ExpressionAggregationResult& result){
    const auto parent=load_expression_matrix_association(project,image,result.source_sha256());parent.require_source(*result.parent());const auto& saved=result.recipe();if(saved.substr(saved.size()-64)!=result.id())throw std::invalid_argument("Aggregation recipe identity differs");
    auto document=project.document(image);auto all=records(*document);if(all.count(result.id()))throw WorkflowOutputExists("Aggregation already retained");V::List chunks;for(std::size_t i=0;i<saved.size();i+=4096)chunks.push_back(V{saved.substr(i,4096)});all.emplace(result.id(),V{std::move(chunks)});publish(*document,std::move(all));return result.id();
}
std::vector<std::string> expression_aggregation_ids(const Project& project,std::uint64_t image){std::vector<std::string> ids;for(const auto& [id,value]:records(*project.document(image))){decode(id,value);ids.push_back(id);}return ids;}
void remove_expression_aggregation(Project& project,std::uint64_t image,const std::string& id){auto document=project.document(image);auto all=records(*document);const auto i=all.find(id);if(i==all.end())throw std::out_of_range("Aggregation record absent");decode(id,i->second);all.erase(i);publish(*document,std::move(all));}
ExpressionAggregationOpenRequest expression_aggregation_open_request(const Project& project,std::uint64_t image,const std::string& id){const auto& all=records(*project.document(image));const auto i=all.find(id);if(i==all.end())throw std::out_of_range("Aggregation record absent");auto saved=decode(id,i->second);const auto record=detail::aggregation::parse(saved);return {expression_matrix_open_request(project,image,record.source),std::move(saved)};}
std::shared_ptr<const ExpressionAggregationResult> ExpressionAggregationOpenRequest::open(std::size_t work,const std::atomic<bool>* cancelled)const{return replay_expression_aggregation(parent.open(work,cancelled),recipe,work,cancelled);}
}
