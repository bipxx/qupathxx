#include "expression_normalization_internal.hpp"
#include "qupathxx/project.hpp"
#include <nlohmann/json.hpp>
namespace qpx::detail::normalization {
namespace {
constexpr const char* prefix="qpx.expression-normalization/1\n";
constexpr std::size_t limit=768u*1024;
std::string encode(const Recipe& r){
    digest(r.source);digest(r.file);digest(r.qc);r.options.validate();r.qc_options.validate(r.features);if(!r.observations || r.observations>1000000)throw std::invalid_argument("Normalization observation count invalid");
    nlohmann::json groups=nlohmann::json::array();for(const auto& g:r.qc_options.groups)groups.push_back({{"name",g.name},{"features",g.features}});
    nlohmann::json j={{"source",r.source},{"file",r.file},{"features",r.features},{"observations",r.observations},{"qc",r.qc},{"groups",std::move(groups)},{"target_sum",r.options.target_sum},{"log1p",r.options.log1p},{"universe","all-source-features"},{"implicit_zero","measured"},{"zero_library","valid-zero"}};
    const auto body=std::string(prefix)+j.dump(-1,' ',true)+"\n";if(body.size()>limit-64)throw WorkflowCaptureLimit("Normalization recipe exceeds768KiB");Sha256 h;h.update(body.data(),body.size());return body+h.finish();
}
}
std::string recipe(const ExpressionQcResult& qc,ExpressionNormalizationOptions options){Recipe r;r.source=qc.source_sha256();r.file=qc.file_sha256();r.qc=qc.sha256();r.features=qc.features();r.observations=qc.totals().size();r.qc_options.groups=qc.groups();r.options=options;return encode(r);}
Recipe parse_recipe(const std::string& saved){
    const auto first=std::char_traits<char>::length(prefix);if(saved.size()<first+64 || saved.size()>limit || saved.rfind(prefix,0))throw std::invalid_argument("Invalid normalization recipe size/schema");const auto n=saved.size()-64;Sha256 h;h.update(saved.data(),n);if(h.finish()!=saved.substr(n))throw std::invalid_argument("Normalization recipe checksum differs");
    const auto j=nlohmann::json::parse(saved.substr(first,n-first),[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>8)throw std::invalid_argument("Normalization recipe nesting exceeds schema");return true;});if(!j.is_object() || j.size()!=11 || j.at("universe")!="all-source-features" || j.at("implicit_zero")!="measured" || j.at("zero_library")!="valid-zero")throw std::invalid_argument("Unsupported normalization semantics");
    Recipe r;r.source=j.at("source").get<std::string>();r.file=j.at("file").get<std::string>();r.qc=j.at("qc").get<std::string>();
    const auto number=[](const nlohmann::json& v,std::uint64_t maximum){if(!v.is_number_integer() || v<0 || v>maximum)throw std::invalid_argument("Invalid normalization recipe integer");return v.get<std::uint64_t>();};r.features=number(j.at("features"),1000000);r.observations=number(j.at("observations"),1000000);
    if(!j.at("target_sum").is_number_float() || !j.at("log1p").is_boolean())throw std::invalid_argument("Invalid normalization parameter types");r.options.target_sum=j.at("target_sum").get<double>();r.options.log1p=j.at("log1p").get<bool>();
    const auto& groups=j.at("groups");if(!groups.is_array() || groups.size()>16)throw std::invalid_argument("Normalization groups exceed16");std::size_t members=0;
    for(const auto& g:groups){if(!g.is_object() || g.size()!=2 || !g.at("features").is_array() || g.at("features").size()>65536-members)throw std::invalid_argument("Invalid normalization group shape");ExpressionFeatureGroup group;group.name=g.at("name").get<std::string>();for(const auto& f:g.at("features"))group.features.push_back(number(f,999999));members+=group.features.size();r.qc_options.groups.push_back(std::move(group));}
    if(encode(r)!=saved)throw std::invalid_argument("Noncanonical normalization recipe");return r;
}
}
namespace qpx {
namespace {
using V=ImageDataValue;
constexpr const char* property="qupathxx.expression-normalizations/1";
template<class T>const T& typed(const V& v){const auto* p=std::get_if<T>(&v.value);if(!p)throw std::invalid_argument("Invalid normalization record type");return *p;}
const V::Map& records(const Document& d){static const V::Map empty;const auto p=d.image_data_properties();if(!p)return empty;const auto i=p->values().find(property);return i==p->values().end()?empty:typed<V::Map>(i->second);}
std::string decode(const std::string& id,const V& value,std::string* parent=nullptr){
    detail::normalization::digest(id);const auto& chunks=typed<V::List>(value);if(chunks.empty() || chunks.size()>192)throw std::invalid_argument("Normalization recipe chunks exceed bounds");std::string result;
    for(std::size_t i=0;i<chunks.size();++i){const auto& s=typed<std::string>(chunks[i]);if(s.empty() || s.size()>4096 || (i+1<chunks.size() && s.size()!=4096))throw std::invalid_argument("Noncanonical normalization record chunks");result+=s;}
    const auto record=detail::normalization::parse_recipe(result);if(parent)*parent=record.source;if(result.substr(result.size()-64)!=id)throw std::invalid_argument("Normalization record ID differs");return result;
}
void publish(Document& d,V::Map all){auto values=d.image_data_properties()?d.image_data_properties()->values():V::Map{};if(all.empty())values.erase(property);else values[property]=V{std::move(all)};d.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));}
}
std::string keep_expression_normalization(Project& project,std::uint64_t image,const ExpressionNormalization& result){
    const auto& qc=*result.qc();const auto parent=load_expression_matrix_association(project,image,qc.source_sha256());if(parent.file_sha256!=qc.file_sha256() || parent.features!=qc.features() || parent.observations!=qc.totals().size())throw std::invalid_argument("Normalization parent association differs");
    const auto& saved=result.recipe();const auto id=result.source()->metadata().source_sha256;if(saved.substr(saved.size()-64)!=id)throw std::invalid_argument("Normalization recipe/source identity differs");auto doc=project.document(image);auto all=records(*doc);if(all.count(id))throw WorkflowOutputExists("Normalization already retained");V::List chunks;for(std::size_t i=0;i<saved.size();i+=4096)chunks.push_back(V{saved.substr(i,4096)});all.emplace(id,V{std::move(chunks)});publish(*doc,std::move(all));return id;
}
std::vector<std::string> expression_normalization_ids(const Project& project,std::uint64_t image){std::vector<std::string> ids;for(const auto& [id,v]:records(*project.document(image))){decode(id,v);ids.push_back(id);}return ids;}
std::vector<std::string> expression_normalization_ids(const Project& project,std::uint64_t image,const std::string& parent){detail::normalization::digest(parent);std::vector<std::string> ids;for(const auto& [id,v]:records(*project.document(image))){std::string source;decode(id,v,&source);if(source==parent)ids.push_back(id);}return ids;}
std::string expression_normalization_recipe(const Project& project,std::uint64_t image,const std::string& id){const auto& all=records(*project.document(image));const auto i=all.find(id);if(i==all.end())throw std::out_of_range("Normalization record absent");return decode(id,i->second);}
void remove_expression_normalization(Project& project,std::uint64_t image,const std::string& id){auto doc=project.document(image);auto all=records(*doc);const auto i=all.find(id);if(i==all.end())throw std::out_of_range("Normalization record absent");decode(id,i->second);all.erase(i);publish(*doc,std::move(all));}
ExpressionNormalizationOpenRequest expression_normalization_open_request(const Project& project,std::uint64_t image,const std::string& id){auto saved=expression_normalization_recipe(project,image,id);const auto r=detail::normalization::parse_recipe(saved);return {expression_matrix_open_request(project,image,r.source),std::move(saved)};}
ExpressionNormalization ExpressionNormalizationOpenRequest::open(std::size_t work,const std::atomic<bool>* cancelled)const{return replay_expression_normalization(parent.open(work,cancelled),recipe,work,cancelled);}
}
