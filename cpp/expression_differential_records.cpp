#include "expression_differential_internal.hpp"
#include "qupathxx/project.hpp"
#include <nlohmann/json.hpp>

namespace qpx::detail::differential {
namespace {constexpr const char* prefix="qpx.expression-differential/1\n";}
std::string encode(const Recipe& r){
    options(r.options);for(const auto* id:{&r.aggregation,&r.aggregation_sha256,&r.source,&r.result})aggregation::digest(*id);
    const nlohmann::json j={{"method",method},{"backend",backend},{"aggregation",r.aggregation},{"aggregation_sha256",r.aggregation_sha256},{"source",r.source},{"group_a",r.options.group_a},{"group_b",r.options.group_b},{"library_target",r.options.library_target},{"normalization","log1p(raw-replicate-count/full-replicate-library*target)"},{"effect","mean-log1p-B-minus-A"},{"correction","BH-all-testable-full-source-features"},{"degenerate","absent-p-q;no-subject-exclusion"},{"replicates","merge-cohorts-by-replicate;sample-bijection;unpaired"},{"result",r.result}};
    const auto body=std::string(prefix)+j.dump(-1,' ',true)+"\n";if(body.size()>8192)throw WorkflowCaptureLimit("DE recipe exceeds8192 bytes");Sha256 h;h.update(body.data(),body.size());return body+h.finish();
}
Recipe parse(const std::string& saved){
    const auto first=std::char_traits<char>::length(prefix);if(saved.size()<first+64 || saved.size()>8256 || saved.rfind(prefix,0))throw std::invalid_argument("Invalid DE recipe schema/size");const auto n=saved.size()-64;Sha256 h;h.update(saved.data(),n);if(h.finish()!=saved.substr(n))throw std::invalid_argument("DE recipe checksum differs");
    const auto j=nlohmann::json::parse(saved.substr(first,n-first),[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>2)throw std::invalid_argument("DE recipe nesting exceeds schema");return true;});
    Recipe r;r.aggregation=j.at("aggregation").get<std::string>();r.aggregation_sha256=j.at("aggregation_sha256").get<std::string>();r.source=j.at("source").get<std::string>();r.result=j.at("result").get<std::string>();r.options.group_a=j.at("group_a").get<std::string>();r.options.group_b=j.at("group_b").get<std::string>();r.options.library_target=j.at("library_target").get<double>();
    if(encode(r)!=saved)throw std::invalid_argument("Noncanonical or unsupported DE recipe");return r;
}
}
namespace qpx {
std::shared_ptr<const ExpressionDifferentialResult> replay_expression_differential(std::shared_ptr<const ExpressionAggregationResult> parent,const std::string& saved,std::size_t work,const std::atomic<bool>* cancelled){
    detail::differential::work_limit(work);workflow_checkpoint(cancelled);auto r=detail::differential::parse(saved);
    if(!parent || parent->id()!=r.aggregation || parent->sha256()!=r.aggregation_sha256 || parent->source_sha256()!=r.source)throw WorkflowInputChanged("DE parent aggregation/source identity differs");
    r.options.work_bytes=work;const auto result=differential_expression(std::move(parent),r.options,cancelled);if(result->sha256()!=r.result || result->recipe()!=saved)throw WorkflowInputChanged("DE exact result/replay identity differs");return result;
}
namespace {
using V=ImageDataValue;constexpr const char* property="qupathxx.expression-differentials/1";
template<class T>const T& typed(const V& v){const auto* p=std::get_if<T>(&v.value);if(!p)throw std::invalid_argument("Invalid DE metadata type");return *p;}
const V::Map& records(const Document& d){static const V::Map empty;const auto p=d.image_data_properties();if(!p)return empty;const auto i=p->values().find(property);return i==p->values().end()?empty:typed<V::Map>(i->second);}
std::string decode(const std::string& id,const V& v){detail::aggregation::digest(id);const auto& chunks=typed<V::List>(v);if(chunks.empty() || chunks.size()>3)throw std::invalid_argument("Invalid DE recipe chunks");std::string saved;for(std::size_t i=0;i<chunks.size();++i){const auto& s=typed<std::string>(chunks[i]);if(s.empty() || s.size()>4096 || (i+1<chunks.size() && s.size()!=4096))throw std::invalid_argument("Noncanonical DE chunk");saved+=s;}detail::differential::parse(saved);if(saved.substr(saved.size()-64)!=id)throw std::invalid_argument("DE record identity differs");return saved;}
void publish(Document& d,V::Map all){auto values=d.image_data_properties()?d.image_data_properties()->values():V::Map{};if(all.empty())values.erase(property);else values[property]=V{std::move(all)};d.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));}
}
std::string keep_expression_differential(Project& p,std::uint64_t image,const ExpressionDifferentialResult& r){
    r.parent()->parent()->require_current();
    // Capture the retained native A02 record without refitting/re-reading counts.
    const auto parent=expression_aggregation_open_request(p,image,r.parent()->id());if(parent.recipe!=r.parent()->recipe())throw WorkflowInputChanged("DE retained pseudobulk recipe differs");load_expression_matrix_association(p,image,r.parent()->source_sha256()).require_source(*r.parent()->parent());
    const auto recipe=detail::differential::parse(r.recipe());if(recipe.result!=r.sha256() || r.id()!=r.recipe().substr(r.recipe().size()-64))throw std::invalid_argument("DE result identity differs");
    auto document=p.document(image);auto all=records(*document);if(all.count(r.id()))throw WorkflowOutputExists("DE result already retained");V::List chunks;for(std::size_t i=0;i<r.recipe().size();i+=4096)chunks.push_back(V{r.recipe().substr(i,4096)});all.emplace(r.id(),V{std::move(chunks)});r.parent()->parent()->require_current();publish(*document,std::move(all));return r.id();
}
std::vector<std::string> expression_differential_ids(const Project& p,std::uint64_t image){std::vector<std::string> ids;for(const auto& [id,value]:records(*p.document(image))){decode(id,value);ids.push_back(id);}return ids;}
void remove_expression_differential(Project& p,std::uint64_t image,const std::string& id){auto document=p.document(image);auto all=records(*document);const auto i=all.find(id);if(i==all.end())throw std::out_of_range("DE record absent");decode(id,i->second);all.erase(i);publish(*document,std::move(all));}
ExpressionDifferentialOpenRequest expression_differential_open_request(const Project& p,std::uint64_t image,const std::string& id){const auto& all=records(*p.document(image));const auto i=all.find(id);if(i==all.end())throw std::out_of_range("DE record absent");auto saved=decode(id,i->second);const auto r=detail::differential::parse(saved);auto parent=expression_aggregation_open_request(p,image,r.aggregation);return {std::move(parent),std::move(saved)};}
std::shared_ptr<const ExpressionDifferentialResult> ExpressionDifferentialOpenRequest::open(std::size_t work,const std::atomic<bool>* cancelled)const{detail::differential::work_limit(work);const auto r=detail::differential::parse(recipe);const auto bulk=parent.open(work,cancelled);return replay_expression_differential(bulk,recipe,work,cancelled);}
}
