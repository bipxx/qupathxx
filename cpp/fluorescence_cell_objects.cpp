#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_cell_contour_internal.hpp"
#include "cell_contours.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <charconv>
#include <algorithm>
#include <cstring>

namespace qpx {
namespace {
constexpr const char* profile="qpx.fluorescence-cell-publication/1";
void text(Sha256& s,const std::string& x){s.integer(x.size());s.update(x.data(),x.size());}
void real(Sha256& s,double x){std::uint64_t bits;std::memcpy(&bits,&x,8);s.integer(bits);}
std::string integer(std::uint64_t value,int base=10){char out[32];const auto r=std::to_chars(out,out+sizeof(out),value,base);if(r.ec!=std::errc{})throw std::runtime_error("Cannot encode fluorescence recipe integer");return {out,r.ptr};}
std::string bits(double value){std::uint64_t b;std::memcpy(&b,&value,8);return integer(b,16);}
std::string optional(std::optional<double> x){return x?bits(*x):"-";}
std::string recipe(const FluorescenceCellRun& run) {
    std::string out="qpx.fluorescence-cell-recipe/1";const auto u=[&](std::uint64_t x){out+='\t';out+=integer(x);};
    const auto s=[&](const std::string& x){out+='\t';out+=x;};const auto& m=run.source_metadata();const auto& r=run.core_region();const auto& p=run.raster().options;
    for(auto x:{run.provenance()->object_id,std::uint64_t(m.series),std::uint64_t(m.series_count),std::uint64_t(m.width),std::uint64_t(m.height),std::uint64_t(m.channels),std::uint64_t(m.z_planes),std::uint64_t(m.timepoints),std::uint64_t(m.sample_type),
        std::uint64_t(r.region.x),std::uint64_t(r.region.y),std::uint64_t(r.region.width),std::uint64_t(r.region.height),std::uint64_t(r.first_channel),std::uint64_t(r.channels),std::uint64_t(r.z),std::uint64_t(r.t),
        std::uint64_t(run.context()[0]),std::uint64_t(run.context()[1]),std::uint64_t(p.segmentation_channel),std::uint64_t(p.threshold_method),std::uint64_t(p.units),std::uint64_t(p.max_cells),std::uint64_t(p.work_bytes)})u(x);
    for(auto x:{p.threshold,p.sigma,p.peak_prominence,p.min_nucleus_area,p.cell_expansion})s(bits(x));s(optional(p.max_nucleus_area));
    s(optional(run.provenance()->effective_pixel_width_um));s(optional(run.provenance()->effective_pixel_height_um));return out;
}
struct Recipe {
    std::uint64_t object=0;std::array<std::uint64_t,8> source{};RawRegionRequest core;FluorescenceCellOptions options;std::array<std::uint32_t,2> context{};
    std::optional<double> x,y;
    explicit Recipe(const std::string& encoded) {
        std::vector<std::string> tokens;std::size_t first=0;
        for(std::size_t i=0;i<=encoded.size();++i)if(i==encoded.size() || encoded[i]=='\t'){tokens.push_back(encoded.substr(first,i-first));first=i+1;}
        if(tokens.size()!=33 || tokens[0]!="qpx.fluorescence-cell-recipe/1")throw std::invalid_argument("Unsupported fluorescence cell recipe");std::size_t at=1;
        const auto u=[&](std::uint64_t maximum=UINT64_MAX,int base=10){const auto& t=tokens.at(at++);std::uint64_t value=0;const auto r=std::from_chars(t.data(),t.data()+t.size(),value,base);if(t.empty() || r.ec!=std::errc{} || r.ptr!=t.data()+t.size() || value>maximum)throw std::invalid_argument("Invalid fluorescence recipe integer");return value;};
        const auto floating=[&](){const auto b=u(UINT64_MAX,16);double x;std::memcpy(&x,&b,8);if(!std::isfinite(x))throw std::invalid_argument("Nonfinite fluorescence recipe value");return x;};
        const auto nullable=[&]()->std::optional<double>{if(tokens.at(at)=="-"){++at;return {};}return floating();};
        object=u();for(auto& v:source)v=u();if(source[0]>UINT32_MAX || !source[1] || source[0]>=source[1] || source[1]>UINT32_MAX || !source[2] || !source[3] || source[2]>ScientificGridAxis::coordinate_limit || source[3]>ScientificGridAxis::coordinate_limit || !source[4] || source[4]>65536 || !source[5] || source[5]>65536 || !source[6] || source[6]>65536 || source[7]>9)throw std::invalid_argument("Invalid fluorescence recipe source");
        core.region.x=std::int64_t(u(ScientificGridAxis::coordinate_limit));core.region.y=std::int64_t(u(ScientificGridAxis::coordinate_limit));core.region.width=std::int64_t(u(ScientificGridAxis::coordinate_limit));core.region.height=std::int64_t(u(ScientificGridAxis::coordinate_limit));
        core.first_channel=std::uint32_t(u(65535));core.channels=std::uint32_t(u(65536));core.z=std::uint32_t(u(65535));core.t=std::uint32_t(u(65535));
        context[0]=std::uint32_t(u(4096));context[1]=std::uint32_t(u(4096));options.segmentation_channel=std::uint32_t(u(65535));options.threshold_method=static_cast<FluorescenceThreshold>(u(1));options.units=static_cast<FluorescenceCellUnits>(u(1));options.max_cells=std::size_t(u(1000000));options.work_bytes=std::size_t(u(1024ULL*1024*1024));
        options.threshold=floating();options.sigma=floating();options.peak_prominence=floating();options.min_nucleus_area=floating();options.cell_expansion=floating();options.max_nucleus_area=nullable();x=nullable();y=nullable();options.validate();
        if((x && *x<=0) || (y && *y<=0))throw std::invalid_argument("Invalid fluorescence recipe calibration");
    }
};
FluorescenceCellRequest replay_recipe(const Document& doc,const std::string& encoded) {
    const Recipe saved(encoded);FluorescenceCellRequest request(doc,saved.core,saved.object,saved.options,saved.context);const auto& m=request.source_metadata();const std::array<std::uint64_t,8> current{m.series,m.series_count,std::uint64_t(m.width),std::uint64_t(m.height),m.channels,m.z_planes,m.timepoints,std::uint64_t(m.sample_type)};
    const auto& a=request.core_region();const auto& b=saved.core;
    if(current!=saved.source || doc.pixel_width_um()!=saved.x || doc.pixel_height_um()!=saved.y || a.region.x!=b.region.x || a.region.y!=b.region.y || a.region.width!=b.region.width || a.region.height!=b.region.height)throw WorkflowInputChanged("Fluorescence replay source/calibration/ROI changed");return request;
}
std::string object_hash(const Annotation& cell) {
    Sha256 sha;text(sha,"qpx.fluorescence-cell-object/1");sha.integer(std::uint64_t(cell.kind));sha.integer(cell.parent_id);sha.integer(std::uint64_t(cell.plane.c));sha.integer(cell.plane.z);sha.integer(cell.plane.t);
    const auto geometry=[&](const Roi& roi){const auto b=GeometryArchiveCodec::encode(roi);sha.integer(b.size());sha.update(b.data(),b.size());};geometry(cell.geometry);sha.integer(bool(cell.nucleus));if(cell.nucleus)geometry(*cell.nucleus);
    if(cell.properties){for(const auto& [k,v]:cell.properties->metadata())if(k.rfind("fluorescence.",0)==0 && k!="fluorescence.object_sha256"){text(sha,k);text(sha,v);}for(const auto& [k,v]:cell.properties->measurements())if(k.rfind("fluorescence.",0)==0){text(sha,k);real(sha,v);}}
    return sha.finish();
}
const std::string& member(const Annotation& cell,const char* key){if(!cell.properties)throw std::invalid_argument("Cell has no fluorescence publication");const auto& m=cell.properties->metadata();const auto at=m.find(key);if(at==m.end())throw std::invalid_argument("Incomplete fluorescence publication");return at->second;}
bool intact(const Annotation& cell){
    if(cell.kind!=ObjectKind::Cell || !cell.nucleus || !cell.properties)return false;const auto& m=cell.properties->metadata();
    const auto p=m.find("fluorescence.profile"),h=m.find("fluorescence.object_sha256");return p!=m.end() && p->second==profile && h!=m.end() && h->second==object_hash(cell);
}
bool matches_run(const Annotation& cell,const FluorescenceCellRun& run){return intact(cell) && member(cell,"fluorescence.recipe")==recipe(run) && member(cell,"fluorescence.run_sha256")==run.sha256() && member(cell,"fluorescence.input_sha256")==run.input_sha256() && member(cell,"fluorescence.recipe_sha256")==run.raster().recipe_sha256;}
std::shared_ptr<const ObjectProperties> properties(const FluorescenceCellRun& run,std::size_t index) {
    const auto& cell=run.raster().measurements.at(index);const auto& selection=run.region();std::map<std::string,std::string> meta{
        {"fluorescence.profile",profile},{"fluorescence.recipe",recipe(run)},{"fluorescence.run_sha256",run.sha256()},{"fluorescence.input_sha256",run.input_sha256()},
        {"fluorescence.recipe_sha256",run.raster().recipe_sha256},{"fluorescence.label",integer(cell.label)},{"fluorescence.measurement_domain","original-channel-samples"},
        {"fluorescence.coordinate_frame","image-level-zero-pixels"},{"fluorescence.nucleus_boundary",run.raster().boundaries[index].nucleus?"true":"false"},{"fluorescence.cell_boundary",run.raster().boundaries[index].cell?"true":"false"},
        {"fluorescence.cytoplasm",cell.cytoplasm?"present":"absent"}};
    for(std::uint32_t c=0;c<selection.channels;++c)meta["fluorescence.C"+integer(selection.first_channel+c)+".name"]=run.source_metadata().channel_names.at(selection.first_channel+c);
    std::map<std::string,double> values;
    const auto compartment=[&](const char* name,const RawCellCompartment& c){const std::string prefix=std::string("fluorescence.")+name+".";values[prefix+"pixels"]=double(c.pixels);values[prefix+"valid_pixels"]=double(c.valid_pixels);
        for(std::size_t i=0;i<c.channels.size();++i){const auto& v=c.channels[i];const auto key=prefix+"C"+integer(selection.first_channel+i)+".";values[key+"finite_samples"]=double(v.finite_samples);values[key+"nonfinite_samples"]=double(v.nonfinite_samples);
            const auto add=[&](const char* field,std::optional<double> x){if(x)values[key+field]=*x;};add("sum",v.sum);add("mean",v.mean);add("min",v.minimum);add("max",v.maximum);add("sample_sd",v.sample_standard_deviation);}};
    compartment("nucleus",cell.nucleus);compartment("cell",cell.cell);if(cell.cytoplasm)compartment("cytoplasm",*cell.cytoplasm);
    return std::make_shared<const ObjectProperties>(std::move(meta),std::move(values));
}
}
std::size_t FluorescenceCellContours::payload_bytes() const{return sizeof(*this)+run_->payload_bytes()+nuclei_->retained_bytes()+cells_->retained_bytes();}
std::shared_ptr<const FluorescenceCellContours> trace_fluorescence_cells(std::shared_ptr<const FluorescenceCellRun> run,TissueContourOptions options,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);options.validate();if(!run || !options.clip_to_roi)throw std::invalid_argument("Fluorescence contours require a run and ROI clipping");
    const auto count=run->raster().measurements.size();std::size_t reserved=sizeof(FluorescenceCellContours)+run->payload_bytes()+4096+64*count;
    if(reserved>=options.work_bytes)throw WorkflowCaptureLimit("Fluorescence contour input exceeds work budget");
    auto out=std::shared_ptr<FluorescenceCellContours>(new FluorescenceCellContours());out->run_=std::move(run);auto stage=options;stage.work_bytes-=reserved;
    try {
        out->nuclei_=detail::trace_fluorescence_label_mask(*out->run_,false,stage,cancelled);
        workflow_capture_charge(reserved,out->nuclei_->retained_bytes(),options.work_bytes);if(reserved==options.work_bytes)throw WorkflowCaptureLimit("Fluorescence contours leave no body workspace");stage.work_bytes=options.work_bytes-reserved;
        out->cells_=detail::trace_fluorescence_label_mask(*out->run_,true,stage,cancelled);
        workflow_capture_charge(reserved,out->cells_->retained_bytes(),options.work_bytes);
        if(out->nuclei_->components().size()!=count || out->cells_->components().size()!=count)throw std::runtime_error("Fluorescence contour clipping lost a label");
        const auto clip=out->run_->roi()?*out->run_->roi():Roi::rectangle({double(out->run_->core_region().region.x),double(out->run_->core_region().region.y),double(out->run_->core_region().region.width),double(out->run_->core_region().region.height)});
        for(std::size_t i=0;i<count;++i){workflow_checkpoint(cancelled);const auto& a=out->nuclei_->components()[i];const auto& b=out->cells_->components()[i];if(a.label!=i+1 || b.label!=i+1)throw std::runtime_error("Fluorescence contour labels changed");
            normalize_cell_body_contour(*std::const_pointer_cast<TissueContours>(out->cells_),i,a.geometry,clip,out->cells_->options(),cancelled);
            if(!b.geometry.covers(a.geometry))throw std::runtime_error("Fluorescence nucleus is not covered by its body contour");}
        if(out->payload_bytes()>options.work_bytes)throw WorkflowCaptureLimit("Normalized fluorescence contours exceed work budget");
    }catch(...){workflow_checkpoint(cancelled);throw;}
    return out;
}
std::vector<std::uint64_t> publish_fluorescence_cells(Document& doc,const FluorescenceCellRequest& request,const FluorescenceCellContours& contours,std::string prefix,std::size_t work,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);detail::bounded_utf8(prefix,240);if(prefix.empty() || !work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid fluorescence publication options");
    const auto& run=*contours.run();if(run.region().channels>128)throw std::length_error("Fluorescence object publication supports at most128 selected channels");
    require_current_fluorescence_cells(doc,request,run,cancelled);
    for(const auto& [id,cell]:doc.objects()){if((id&4095)==0)workflow_checkpoint(cancelled);if(cell.parent_id==request.object_id() && cell.properties){const auto& m=cell.properties->metadata();const auto at=m.find("fluorescence.run_sha256");if(at!=m.end() && at->second==run.sha256())throw WorkflowOutputExists("Fluorescence run already has published cells");}}
    const auto count=run.raster().measurements.size();std::size_t bytes=4096;workflow_capture_charge(bytes,contours.payload_bytes(),work);workflow_capture_charge(bytes,count*(sizeof(ObjectEdit)+512),work);
    std::vector<ObjectEdit> edits;edits.reserve(count);const auto plane=request.object_id()?doc.annotation(request.object_id()).plane:ObjectPlane{-1,run.region().z,run.region().t};
    for(std::size_t i=0;i<count;++i){workflow_checkpoint(cancelled);const auto& nucleus=contours.nuclei()->components()[i];const auto& body=contours.cells()->components()[i];
        auto edit=ObjectEdit::create_geometry(body.geometry,prefix+" "+integer(i+1),request.object_id(),"",ObjectKind::Cell,nucleus.geometry,plane);edit.value.plane=plane;edit.value.properties=properties(run,i);
        auto meta=edit.value.properties->metadata();meta["fluorescence.object_sha256"]=object_hash(edit.value);edit.value.properties=std::make_shared<const ObjectProperties>(std::move(meta),edit.value.properties->measurements());
        workflow_capture_charge(bytes,3*(body.geometry.payload_bytes()+nucleus.geometry.payload_bytes()+edit.value.properties->payload_bytes()),work);edits.push_back(std::move(edit));}
    require_current_fluorescence_cells(doc,request,run,cancelled);return doc.execute(edits,"Add fluorescence cells");
}
FluorescenceCellRequest replay_fluorescence_cell(const Document& doc,std::uint64_t id) {
    const auto& cell=doc.annotation(id);if(!intact(cell))throw std::invalid_argument("Fluorescence publication has changed");return replay_recipe(doc,member(cell,"fluorescence.recipe"));
}
bool verify_fluorescence_cell_publication(const Document& doc,std::uint64_t id,const FluorescenceCellRequest& request,const FluorescenceCellRun& run,const std::atomic<bool>* cancelled) {
    require_current_fluorescence_cells(doc,request,run,cancelled);return matches_run(doc.annotation(id),run);
}
bool verify_fluorescence_cell_publications(const Document& doc,const std::vector<std::uint64_t>& ids,const FluorescenceCellRequest& request,const FluorescenceCellRun& run,const std::atomic<bool>* cancelled) {
    require_current_fluorescence_cells(doc,request,run,cancelled);const auto count=run.raster().measurements.size();if(ids.size()!=count)return false;std::vector<std::uint8_t> seen(count);
    for(auto id:ids){workflow_checkpoint(cancelled);const auto& cell=doc.annotation(id);if(!matches_run(cell,run))return false;const auto& text=member(cell,"fluorescence.label");std::uint64_t label=0;
        const auto decoded=std::from_chars(text.data(),text.data()+text.size(),label);if(decoded.ec!=std::errc{} || decoded.ptr!=text.data()+text.size() || !label || label>count || seen[label-1])return false;seen[label-1]=1;}
    return true;
}
namespace {
constexpr const char* records_key="qupathxx.fluorescence-cell-records/1";
using V=ImageDataValue;
template<class T>const T& typed(const V& value){const auto* p=std::get_if<T>(&value.value);if(!p)throw std::invalid_argument("Invalid fluorescence record type");return *p;}
const V& field(const V::Map& m,const char* name){const auto p=m.find(name);if(p==m.end())throw std::invalid_argument("Incomplete fluorescence record");return p->second;}
struct Record {
    std::string recipe,input,run,raster_recipe;std::int64_t cells=0;
    std::string id() const {Sha256 hash;text(hash,"qpx.fluorescence-cell-record/1");for(const auto* v:{&recipe,&input,&run,&raster_recipe})text(hash,*v);hash.integer(std::uint64_t(cells));return hash.finish();}
    V encode() const{return V{V::Map{{"schema",V{std::string("qpx.fluorescence-cell-record/1")}},{"recipe",V{recipe}},{"input_sha256",V{input}},{"run_sha256",V{run}},{"raster_recipe_sha256",V{raster_recipe}},{"cells",V{cells}}}};}
};
bool digest(const std::string& v){return v.size()==64 && std::all_of(v.begin(),v.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});}
Record record(const std::string& id,const V& value){const auto& m=typed<V::Map>(value);if(m.size()!=6 || typed<std::string>(field(m,"schema"))!="qpx.fluorescence-cell-record/1")throw std::invalid_argument("Invalid fluorescence record schema");
    Record r{typed<std::string>(field(m,"recipe")),typed<std::string>(field(m,"input_sha256")),typed<std::string>(field(m,"run_sha256")),typed<std::string>(field(m,"raster_recipe_sha256")),typed<std::int64_t>(field(m,"cells"))};
    if(!digest(id) || !digest(r.input) || !digest(r.run) || !digest(r.raster_recipe) || r.cells<0 || r.cells>1000000 || r.id()!=id)throw std::invalid_argument("Fluorescence record identity mismatch");Recipe decoded(r.recipe);if(std::uint64_t(r.cells)>decoded.options.max_cells)throw std::invalid_argument("Fluorescence record count exceeds recipe");return r;}
const V::Map& records(const Document& doc){static const V::Map empty;const auto p=doc.image_data_properties();if(!p)return empty;const auto at=p->values().find(records_key);return at==p->values().end()?empty:typed<V::Map>(at->second);}
Record stored(const Document& doc,const std::string& id){const auto& all=records(doc);const auto p=all.find(id);if(p==all.end())throw std::out_of_range("Fluorescence record not found");return record(id,p->second);}
}
std::string keep_fluorescence_cell_run(Document& doc,const FluorescenceCellRequest& request,const FluorescenceCellRun& run,const std::atomic<bool>* cancelled) {
    require_current_fluorescence_cells(doc,request,run,cancelled);const Record item{recipe(run),run.input_sha256(),run.sha256(),run.raster().recipe_sha256,std::int64_t(run.raster().measurements.size())};const auto id=item.id();auto all=records(doc);
    if(all.count(id))throw WorkflowOutputExists("Fluorescence recipe/result record already exists");all.emplace(id,item.encode());auto values=doc.image_data_properties()?doc.image_data_properties()->values():V::Map{};values[records_key]=V{std::move(all)};
    auto properties=std::make_shared<const ImageDataProperties>(std::move(values));require_current_fluorescence_cells(doc,request,run,cancelled);doc.set_image_data_properties(std::move(properties));return id;
}
std::vector<std::string> fluorescence_cell_record_ids(const Document& doc){std::vector<std::string> result;for(const auto& [id,v]:records(doc)){record(id,v);result.push_back(id);}return result;}
FluorescenceCellRequest replay_fluorescence_cell_record(const Document& doc,const std::string& id){return replay_recipe(doc,stored(doc,id).recipe);}
bool verify_fluorescence_cell_record(const Document& doc,const std::string& id,const FluorescenceCellRequest& request,const FluorescenceCellRun& run,const std::atomic<bool>* cancelled){require_current_fluorescence_cells(doc,request,run,cancelled);const auto r=stored(doc,id);return r.recipe==recipe(run) && r.input==run.input_sha256() && r.run==run.sha256() && r.raster_recipe==run.raster().recipe_sha256 && std::size_t(r.cells)==run.raster().measurements.size();}
void remove_fluorescence_cell_record(Document& doc,const std::string& id){auto all=records(doc);const auto p=all.find(id);if(p==all.end())throw std::out_of_range("Fluorescence record not found");record(id,p->second);all.erase(p);auto values=doc.image_data_properties()->values();if(all.empty())values.erase(records_key);else values[records_key]=V{std::move(all)};doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));}
}
