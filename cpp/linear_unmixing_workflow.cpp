#include "qupathxx/linear_unmixing_workflow.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
namespace qpx {
namespace {
using V=ImageDataValue;
constexpr const char* property="qupathxx.linear-unmix-recipes/1";
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void digest(const std::string& s){if(s.size()!=64 || !std::all_of(s.begin(),s.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))throw std::invalid_argument("Invalid linear unmixing identity digest");}
template<class T>const T& typed(const V& v){const auto* p=std::get_if<T>(&v.value);if(!p)throw std::invalid_argument("Invalid linear unmixing record type");return *p;}
const V& field(const V::Map& m,const char* key){const auto it=m.find(key);if(it==m.end())throw std::invalid_argument("Incomplete linear unmixing record");return it->second;}
const std::string& bounded(const V& v,std::size_t size){const auto& s=typed<std::string>(v);detail::bounded_utf8(s,size);return s;}
const V::Map& records(const Document& doc){static const V::Map empty;const auto p=doc.image_data_properties();if(!p)return empty;const auto it=p->values().find(property);return it==p->values().end()?empty:typed<V::Map>(it->second);}
LinearUnmixRecipe stored(const std::string& id,const V& value){auto p=LinearUnmixRecipe::from_value(value);if(p.sha256()!=id)throw std::invalid_argument("Stored linear unmixing recipe identity differs");return p;}
void signal_matches(const Document& doc,const DarkFlatSourceReference& signal){const auto server=doc.server();if(!server || server->reader_id()!=signal.reader || server->source_locator()!=signal.path || !server->tiff_view() || server->tiff_view()->series!=signal.series)throw WorkflowInputChanged("Saved linear unmixing signal source changed");}
}
void LinearUnmixRecipe::validate() const {
    matrix.validate();detail::bounded_utf8(signal.path,32768);workflow_path_argument(signal.path);
    if(signal.reader!="tiff" || signal.series>65535)throw std::invalid_argument("Saved linear unmixing currently requires native TIFF series");
    if(algorithm!="qpx.linear-unmixing-svd/1")throw std::invalid_argument("Unsupported linear unmixing algorithm");
    digest(signal.source_sha256);digest(input_source_sha256);digest(operator_sha256);digest(derived_source_sha256);
    if(correction_record.empty()){if(matrix.input_domain!="original-channel-samples" || input_source_sha256!=signal.source_sha256)throw std::invalid_argument("Original unmixing recipe domain/source differs");}
    else{digest(correction_record);if(matrix.input_domain!="qpx.dark-flat-corrected-samples/1")throw std::invalid_argument("Corrected unmixing recipe domain differs");}
}
std::string LinearUnmixRecipe::sha256() const {validate();Sha256 h;text(h,"qpx.linear-unmix-record/1");text(h,signal.path);text(h,signal.reader);h.integer(signal.series);text(h,signal.source_sha256);text(h,correction_record);text(h,matrix.sha256());text(h,algorithm);text(h,operator_sha256);text(h,input_source_sha256);text(h,derived_source_sha256);return h.finish();}
std::size_t LinearUnmixRecipe::payload_bytes() const {return sizeof(*this)+signal.path.capacity()+signal.reader.capacity()+signal.source_sha256.capacity()+correction_record.capacity()+matrix.payload_bytes()+algorithm.capacity()+operator_sha256.capacity()+input_source_sha256.capacity()+derived_source_sha256.capacity();}
ImageDataValue LinearUnmixRecipe::to_value() const {validate();return V{V::Map{{"schema",V{std::string("qpx.linear-unmix-recipe/1")}},{"domain",V{std::string("qpx.linear-unmixed-samples/1")}},{"signal",V{V::Map{{"path",V{signal.path}},{"reader",V{signal.reader}},{"series",V{std::int64_t(signal.series)}},{"source_sha256",V{signal.source_sha256}}}}},{"correction_record",V{correction_record}},{"matrix",matrix.to_value()},{"algorithm",V{algorithm}},{"operator_sha256",V{operator_sha256}},{"input_source_sha256",V{input_source_sha256}},{"derived_source_sha256",V{derived_source_sha256}}}};}
LinearUnmixRecipe LinearUnmixRecipe::from_value(const ImageDataValue& value){const auto& m=typed<V::Map>(value);if(m.size()!=9 || bounded(field(m,"schema"),64)!="qpx.linear-unmix-recipe/1" || bounded(field(m,"domain"),64)!="qpx.linear-unmixed-samples/1")throw std::invalid_argument("Unsupported linear unmixing record schema/domain");const auto& signal=typed<V::Map>(field(m,"signal"));if(signal.size()!=4)throw std::invalid_argument("Invalid linear unmixing signal fields");
    LinearUnmixRecipe p;p.signal.path=bounded(field(signal,"path"),32768);p.signal.reader=bounded(field(signal,"reader"),16);const auto series=typed<std::int64_t>(field(signal,"series"));if(series<0 || series>65535)throw std::invalid_argument("Invalid linear unmixing signal series");p.signal.series=std::uint32_t(series);p.signal.source_sha256=bounded(field(signal,"source_sha256"),64);p.correction_record=bounded(field(m,"correction_record"),64);p.matrix=LinearMixingMatrix::from_value(field(m,"matrix"));p.algorithm=bounded(field(m,"algorithm"),64);p.operator_sha256=bounded(field(m,"operator_sha256"),64);p.input_source_sha256=bounded(field(m,"input_source_sha256"),64);p.derived_source_sha256=bounded(field(m,"derived_source_sha256"),64);p.validate();return p;
}
struct LinearUnmixRequest::Data {
    std::thread::id owner=std::this_thread::get_id();RawAnalysisRequest authority;
    LinearUnmixRecipe recipe;std::shared_ptr<const ChannelSource> source;
    mutable std::shared_ptr<const ImageDataProperties> checked_properties;
    mutable bool parent_current=false;
    Data(const Document& doc,const DarkFlatRequest* correction,LinearMixingMatrix matrix,const std::atomic<bool>* cancelled):authority(doc,RawRegionRequest{}){
        workflow_checkpoint(cancelled);std::shared_ptr<const ChannelSource> input;
        if(correction){correction->require_current(doc,cancelled);const auto& parent=correction->recipe();recipe.correction_record=parent.sha256();if(load_dark_flat_recipe(doc,recipe.correction_record).sha256()!=recipe.correction_record)throw WorkflowInputChanged("Unmixing parent correction differs");recipe.signal=parent.sources[0];input=correction->source();checked_properties=doc.image_data_properties();parent_current=true;}
        else{const auto server=doc.server();if(!server || server->reader_id()!="tiff" || !server->tiff_view())throw std::invalid_argument("Saved linear unmixing requires open native TIFF reader");input=original_channel_source(server,cancelled);recipe.signal={server->source_locator(),"tiff",input->sha256(),server->tiff_view()->series};}
        recipe.input_source_sha256=input->sha256();LinearUnmixModel model(std::move(matrix),cancelled);recipe.matrix=model.matrix();recipe.operator_sha256=model.sha256();source=linear_unmix_channel_source(std::move(input),std::move(model),cancelled);recipe.derived_source_sha256=source->sha256();recipe.validate();workflow_checkpoint(cancelled);
    }
};
LinearUnmixRequest::LinearUnmixRequest(const Document& doc,LinearMixingMatrix matrix,const std::atomic<bool>* cancelled):data_(std::make_shared<const Data>(doc,nullptr,std::move(matrix),cancelled)){require_current(doc,cancelled);}
LinearUnmixRequest::LinearUnmixRequest(const Document& doc,const DarkFlatRequest& correction,LinearMixingMatrix matrix,const std::atomic<bool>* cancelled):data_(std::make_shared<const Data>(doc,&correction,std::move(matrix),cancelled)){require_current(doc,cancelled);}
bool LinearUnmixRequest::matches(const Document& doc) const {workflow_require_owner(data_->owner);if(!data_->authority.matches(doc))return false;if(data_->recipe.correction_record.empty())return true;
    const auto properties=doc.image_data_properties();if(properties!=data_->checked_properties){data_->parent_current=false;data_->checked_properties=properties;try{data_->parent_current=load_dark_flat_recipe(doc,data_->recipe.correction_record).sha256()==data_->recipe.correction_record;}catch(const std::invalid_argument&){}catch(const std::out_of_range&){}}
    return data_->parent_current;
}
void LinearUnmixRequest::require_current(const Document& doc,const std::atomic<bool>* cancelled) const {workflow_checkpoint(cancelled);if(!matches(doc))throw WorkflowInputChanged("Linear unmixing request document/source/calibration/parent changed");data_->source->require_current(cancelled);}
const LinearUnmixRecipe& LinearUnmixRequest::recipe() const {return data_->recipe;}
const std::shared_ptr<const ChannelSource>& LinearUnmixRequest::source() const {return data_->source;}
std::string keep_linear_unmix_recipe(Document& doc,const LinearUnmixRequest& request,const std::atomic<bool>* cancelled){workflow_checkpoint(cancelled);if(!request.matches(doc))throw WorkflowInputChanged("Linear unmixing capture is stale");const auto& p=request.recipe();const auto id=p.sha256();auto all=records(doc);if(all.count(id))throw WorkflowOutputExists("Linear unmixing recipe already retained");all.emplace(id,p.to_value());auto values=doc.image_data_properties()?doc.image_data_properties()->values():V::Map{};values[property]=V{std::move(all)};auto prepared=std::make_shared<const ImageDataProperties>(std::move(values));request.require_current(doc,cancelled);doc.set_image_data_properties(std::move(prepared));return id;}
std::vector<std::string> linear_unmix_recipe_ids(const Document& doc){std::vector<std::string> ids;for(const auto& [id,value]:records(doc)){stored(id,value);ids.push_back(id);}return ids;}
LinearUnmixRecipe load_linear_unmix_recipe(const Document& doc,const std::string& id){const auto& all=records(doc);const auto it=all.find(id);if(it==all.end())throw std::out_of_range("Linear unmixing recipe not found");return stored(id,it->second);}
void remove_linear_unmix_recipe(Document& doc,const std::string& id){auto all=records(doc);const auto it=all.find(id);if(it==all.end())throw std::out_of_range("Linear unmixing recipe not found");stored(id,it->second);all.erase(it);auto values=doc.image_data_properties()->values();if(all.empty())values.erase(property);else values[property]=V{std::move(all)};doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));}
bool linear_unmix_recipes_reference_correction(const Document& doc,const std::string& correction){for(const auto& [id,value]:records(doc))if(stored(id,value).correction_record==correction)return true;return false;}
LinearUnmixRequest restore_linear_unmix_recipe(const Document& doc,const LinearUnmixRecipe& recipe,const std::optional<DarkFlatRequest>& correction,const std::atomic<bool>* cancelled){workflow_checkpoint(cancelled);recipe.validate();signal_matches(doc,recipe.signal);if(bool(correction)!=!recipe.correction_record.empty() || (correction && correction->recipe().sha256()!=recipe.correction_record))throw WorkflowInputChanged("Saved unmixing parent correction differs");auto request=correction?LinearUnmixRequest(doc,*correction,recipe.matrix,cancelled):LinearUnmixRequest(doc,recipe.matrix,cancelled);if(request.recipe().sha256()!=recipe.sha256())throw WorkflowInputChanged("Saved unmixing source/operator/recipe identity changed");return request;}
LinearUnmixRequest replay_linear_unmix_recipe(const Document& doc,const std::string& id,const std::atomic<bool>* cancelled){workflow_checkpoint(cancelled);const auto recipe=load_linear_unmix_recipe(doc,id);signal_matches(doc,recipe.signal);std::optional<DarkFlatRequest> correction;if(!recipe.correction_record.empty())correction=replay_dark_flat_recipe(doc,recipe.correction_record,cancelled);return restore_linear_unmix_recipe(doc,recipe,correction,cancelled);}
}
