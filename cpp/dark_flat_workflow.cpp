#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cstring>
namespace qpx {
namespace {
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void digest(const std::string& s){if(s.size()!=64 || !std::all_of(s.begin(),s.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))throw std::invalid_argument("Invalid dark/flat identity digest");}
using V=ImageDataValue;
constexpr const char* property="qupathxx.dark-flat-recipes/1";
template<class T>const T& typed(const V& value){const auto* p=std::get_if<T>(&value.value);if(!p)throw std::invalid_argument("Invalid stored dark/flat type");return *p;}
const V& field(const V::Map& m,const char* key){const auto it=m.find(key);if(it==m.end())throw std::invalid_argument("Incomplete dark/flat recipe");return it->second;}
V encode(const DarkFlatRecipe& p){p.validate();V::List sources,scales;for(const auto& s:p.sources)sources.push_back(V{V::Map{{"path",V{s.path}},{"reader",V{s.reader}},{"series",V{std::int64_t(s.series)}},{"source_sha256",V{s.source_sha256}}}});for(auto scale:p.options.scales)scales.push_back(V{scale});
    return V{V::Map{{"schema",V{std::string("qpx.dark-flat-recipe/1")}},{"domain",V{std::string("qpx.dark-flat-corrected-samples/1")}},{"sources",V{std::move(sources)}},{"scales",V{std::move(scales)}},{"minimum_denominator",V{p.options.minimum_denominator}},{"derived_source_sha256",V{p.derived_source_sha256}}}};
}
DarkFlatRecipe decode(const V& v){const auto& m=typed<V::Map>(v);if(m.size()!=6 || typed<std::string>(field(m,"schema"))!="qpx.dark-flat-recipe/1" || typed<std::string>(field(m,"domain"))!="qpx.dark-flat-corrected-samples/1")throw std::invalid_argument("Unsupported stored dark/flat schema/domain");
    const auto& sources=typed<V::List>(field(m,"sources"));const auto& scales=typed<V::List>(field(m,"scales"));if(sources.size()!=3 || scales.empty() || scales.size()>65536)throw std::invalid_argument("Stored dark/flat source/scale shape");DarkFlatRecipe p;
    for(std::size_t i=0;i<3;++i){const auto& s=typed<V::Map>(sources[i]);if(s.size()!=4)throw std::invalid_argument("Stored dark/flat source fields");auto& out=p.sources[i];out.path=typed<std::string>(field(s,"path"));out.reader=typed<std::string>(field(s,"reader"));const auto series=typed<std::int64_t>(field(s,"series"));if(series<0 || series>65535)throw std::invalid_argument("Stored dark/flat series range");out.series=std::uint32_t(series);out.source_sha256=typed<std::string>(field(s,"source_sha256"));}
    for(const auto& scale:scales)p.options.scales.push_back(typed<double>(scale));p.options.minimum_denominator=typed<double>(field(m,"minimum_denominator"));p.derived_source_sha256=typed<std::string>(field(m,"derived_source_sha256"));p.validate();return p;
}
const V::Map& records(const Document& doc){static const V::Map empty;auto p=doc.image_data_properties();if(!p)return empty;const auto it=p->values().find(property);return it==p->values().end()?empty:typed<V::Map>(it->second);}
DarkFlatRecipe stored(const std::string& id,const V& value){auto p=decode(value);if(p.sha256()!=id)throw std::invalid_argument("Stored dark/flat recipe identity differs");return p;}
}
void DarkFlatRecipe::validate()const{options.validate();for(const auto& s:sources){detail::bounded_utf8(s.path,32768);workflow_path_argument(s.path);if(s.reader!="tiff" || s.series>65535)throw std::invalid_argument("Saved dark/flat sources currently require native TIFF series");digest(s.source_sha256);}digest(derived_source_sha256);}
std::string DarkFlatRecipe::sha256()const{validate();Sha256 h;text(h,"qpx.dark-flat-record/1");for(const auto& s:sources){text(h,s.path);text(h,s.reader);h.integer(s.series);text(h,s.source_sha256);}text(h,options.sha256());text(h,derived_source_sha256);return h.finish();}
std::size_t DarkFlatRecipe::payload_bytes()const{auto n=sizeof(*this)+options.scales.capacity()*sizeof(double)+derived_source_sha256.capacity();for(const auto& s:sources)n+=s.path.capacity()+s.reader.capacity()+s.source_sha256.capacity();return n;}
struct DarkFlatRequest::Data {
    std::thread::id owner=std::this_thread::get_id();RawAnalysisRequest authority;
    DarkFlatRecipe recipe;std::shared_ptr<const ChannelSource> source;
    Data(const Document& doc,std::shared_ptr<ImageServer> dark,std::shared_ptr<ImageServer> flat,DarkFlatOptions options,const std::atomic<bool>* cancelled):authority(doc,RawRegionRequest{}){
        std::array<std::shared_ptr<ImageServer>,3> servers{doc.server(),std::move(dark),std::move(flat)};std::array<std::shared_ptr<const ChannelSource>,3> inputs;
        for(std::size_t i=0;i<3;++i){workflow_checkpoint(cancelled);const auto& server=servers[i];if(!server || server->reader_id()!="tiff" || !server->tiff_view())throw std::invalid_argument("Saved dark/flat sources require open native TIFF readers");inputs[i]=original_channel_source(server,cancelled);recipe.sources[i]={server->source_locator(),"tiff",inputs[i]->sha256(),server->tiff_view()->series};}
        source=dark_flat_channel_source(inputs[0],inputs[1],inputs[2],options);recipe.options=std::move(options);recipe.derived_source_sha256=source->sha256();recipe.validate();workflow_checkpoint(cancelled);
    }
};
DarkFlatRequest::DarkFlatRequest(const Document& doc,std::shared_ptr<ImageServer> dark,std::shared_ptr<ImageServer> flat,DarkFlatOptions options,const std::atomic<bool>* cancelled):data_(std::make_shared<const Data>(doc,std::move(dark),std::move(flat),std::move(options),cancelled)){if(!matches(doc))throw WorkflowInputChanged("Dark/flat document changed during capture");workflow_checkpoint(cancelled);}
bool DarkFlatRequest::matches(const Document& doc)const{workflow_require_owner(data_->owner);return data_->authority.matches(doc);}
void DarkFlatRequest::require_current(const Document& doc,const std::atomic<bool>* cancelled)const{workflow_checkpoint(cancelled);if(!matches(doc))throw WorkflowInputChanged("Dark/flat request no longer matches its document/source/calibration");data_->source->require_current(cancelled);}
const DarkFlatRecipe& DarkFlatRequest::recipe()const{return data_->recipe;}
const std::shared_ptr<const ChannelSource>& DarkFlatRequest::source()const{return data_->source;}
std::string keep_dark_flat_recipe(Document& doc,const DarkFlatRequest& request,const std::atomic<bool>* cancelled){workflow_checkpoint(cancelled);if(!request.matches(doc))throw WorkflowInputChanged("Dark/flat recipe capture is stale");const auto& recipe=request.recipe();const auto id=recipe.sha256();auto all=records(doc);if(all.count(id))throw WorkflowOutputExists("Dark/flat recipe already retained");all.emplace(id,encode(recipe));auto values=doc.image_data_properties()?doc.image_data_properties()->values():V::Map{};values[property]=V{std::move(all)};auto prepared=std::make_shared<const ImageDataProperties>(std::move(values));request.require_current(doc,cancelled);doc.set_image_data_properties(std::move(prepared));return id;}
std::vector<std::string> dark_flat_recipe_ids(const Document& doc){std::vector<std::string> ids;for(const auto& [id,value]:records(doc)){stored(id,value);ids.push_back(id);}return ids;}
DarkFlatRecipe load_dark_flat_recipe(const Document& doc,const std::string& id){const auto& all=records(doc);const auto it=all.find(id);if(it==all.end())throw std::out_of_range("Dark/flat recipe not found");return stored(id,it->second);}
void remove_dark_flat_recipe(Document& doc,const std::string& id){if(fluorescence_cells_reference_correction(doc,id))throw std::invalid_argument("Dark/flat recipe is referenced by fluorescence cells or run records");auto all=records(doc);const auto it=all.find(id);if(it==all.end())throw std::out_of_range("Dark/flat recipe not found");stored(id,it->second);all.erase(it);auto values=doc.image_data_properties()->values();if(all.empty())values.erase(property);else values[property]=V{std::move(all)};doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));}
DarkFlatRequest replay_dark_flat_recipe(const Document& doc,const std::string& id,const std::atomic<bool>* cancelled){workflow_checkpoint(cancelled);const auto p=load_dark_flat_recipe(doc,id);std::array<std::shared_ptr<ImageServer>,2> fields;
    // Refuse changed signal path/series early; the capture below verifies all source versions once.
    if(!doc.server() || doc.server()->source_locator()!=p.sources[0].path || !doc.server()->tiff_view() || doc.server()->tiff_view()->series!=p.sources[0].series)throw WorkflowInputChanged("Saved dark/flat signal source changed");
    for(std::size_t i=0;i<2;++i){workflow_checkpoint(cancelled);TiffOpenOptions options;options.view.series=p.sources[i+1].series;fields[i]=open_tiff(p.sources[i+1].path,options,cancelled);}
    DarkFlatRequest request(doc,fields[0],fields[1],p.options,cancelled);if(request.recipe().sha256()!=id)throw WorkflowInputChanged("Saved dark/flat fields or derived identity changed");return request;
}
DarkFlatBlock::DarkFlatBlock(RawPixelBuffer samples,RawRegionRequest region,std::string source,const std::atomic<bool>* cancelled):samples_(std::move(samples)),region_(region),source_(std::move(source)),finite_(region.channels,0),nonfinite_(region.channels,0){
    Sha256 h;text(h,"qpx.dark-flat-block/1");text(h,source_);for(auto v:{std::uint64_t(region.region.x),std::uint64_t(region.region.y),std::uint64_t(region.region.level),std::uint64_t(region.region.width),std::uint64_t(region.region.height),std::uint64_t(region.first_channel),std::uint64_t(region.channels),std::uint64_t(region.z),std::uint64_t(region.t)})h.integer(v);
    const auto& values=std::get<std::vector<double>>(samples_.storage());for(std::size_t p=0;p<samples_.valid().size();++p){if((p&1023)==0)workflow_checkpoint(cancelled);const auto valid=samples_.valid()[p];h.integer(valid);valid_+=bool(valid);for(std::size_t c=0;c<region.channels;++c){if(c && (c&4095)==0)workflow_checkpoint(cancelled);const auto value=values[p*region.channels+c];std::uint64_t bits;std::memcpy(&bits,&value,8);h.integer(bits);if(valid){if(std::isfinite(value))++finite_[c];else ++nonfinite_[c];}}}sha_=h.finish();workflow_checkpoint(cancelled);
}
std::shared_ptr<const DarkFlatBlock> read_dark_flat_block(const DarkFlatRequest& request,RawRegionRequest region,std::size_t work,const std::atomic<bool>* cancelled){workflow_checkpoint(cancelled);if(!work || work>1024ull*1024*1024)throw std::invalid_argument("Dark/flat block work must be1..1024MiB");
    auto fixed=request.recipe().payload_bytes()+65536+std::size_t(region.channels)*16;auto required=fixed;workflow_capture_charge(required,request.source()->read_work_bytes(region),work);auto samples=request.source()->read(region,work-fixed,cancelled);
    auto result=std::shared_ptr<const DarkFlatBlock>(new DarkFlatBlock(std::move(samples),region,request.source()->sha256(),cancelled));request.source()->require_current(cancelled);return result;
}
}
