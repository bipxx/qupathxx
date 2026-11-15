#include "qupathxx/format_import.hpp"
#include "qupathxx/object_properties.hpp"
#include "format_plugin_runtime.hpp"
#include "expression_matrix_external.hpp"
#include "expression_matrix_writer.hpp"
#include "workflow_input.hpp"
#include "reader_file_guard.hpp"
#include "spatial_count_text.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>
#include <thread>

namespace qpx {
namespace {
// Covers bounded ABI pages, fingerprint/guard owners, receipt serialization,
// one row's property construction, and fixed native wrapper allocations. The
// assigned DLL allowance and variable retained vectors are charged separately.
constexpr std::size_t host_workspace=4u*1024*1024;
constexpr std::size_t max_work=1024ull*1024*1024;
void text(Sha256& hash,const std::string& value){hash.integer(value.size());hash.update(value.data(),value.size());}
void charge(std::size_t& n,std::size_t extra,std::size_t limit){workflow_capture_charge(n,extra,limit);}
std::size_t product(std::uint64_t a,std::size_t b){if(a>std::numeric_limits<std::size_t>::max()/b)throw WorkflowCaptureLimit("Format import allocation overflows address space");return std::size_t(a)*b;}
void work_limits(std::size_t work,std::size_t plugin){
    if(work>max_work || plugin<65536 || plugin>256u*1024*1024)throw std::invalid_argument("Format import work/plugin budget exceeds limits");
    std::size_t used=host_workspace;charge(used,plugin,work);
}
void field(const std::string& value,bool nonempty=true){detail::bounded_utf8(value,4096);if(nonempty && value.empty())throw std::invalid_argument("Empty format import field");}
std::uint64_t real_bits(double value){std::uint64_t bits;std::memcpy(&bits,&value,8);return bits;}
template<class Iterator,class Less> void cancellable_sort(Iterator first,Iterator last,Less less,const std::atomic<bool>* c){
    std::size_t comparisons=0;workflow_checkpoint(c);
    std::sort(first,last,[&](const auto& a,const auto& b){if((comparisons++&4095)==0)workflow_checkpoint(c);return less(a,b);});workflow_checkpoint(c);
}
std::string registry_identity(const Document& doc,const std::atomic<bool>* c){
    Sha256 hash;text(hash,"qpx.annotation-plugin-class-registry/1");hash.integer(doc.classes().size());
    for(const auto& [key,value]:doc.classes()) {workflow_checkpoint(c);text(hash,key);text(hash,value.name);text(hash,value.parent);hash.integer(value.color);hash.integer(value.alpha);hash.integer(value.color_present);}
    return hash.finish();
}
using FileGuards=std::vector<std::unique_ptr<detail::ReaderFileGuard>>;
FileGuards image_guards(const Document& doc,const std::atomic<bool>* c){
    workflow_checkpoint(c);const auto server=doc.server();if(!server || server->closed())throw std::invalid_argument("Annotation target image is closed");
    auto source=server->reader_source();
    if(source && source->scope=="unopened"){server->read_region({0,0,0,1,1});source=server->reader_source();workflow_checkpoint(c);}
    if(!source)throw std::invalid_argument("Annotation target has no stable source identity");source->validate();
    if(source->scope=="directory_store")throw std::invalid_argument("Annotation plugin import does not support directory-store targets");
    FileGuards guards;
    if(source->primary){guards.reserve(1+source->companions.size());guards.push_back(std::make_unique<detail::ReaderFileGuard>(source->primary->path));for(const auto& file:source->companions){workflow_checkpoint(c);guards.push_back(std::make_unique<detail::ReaderFileGuard>(file.path));}}
    else (void)dense_image(*server); // Only immutable native RGB has this authority.
    return guards;
}
void verify_guards(const FileGuards& guards,const std::atomic<bool>* c){for(const auto& guard:guards){workflow_checkpoint(c);guard->verify();}}
CoordinateFrame target_frame(const Document& doc,const std::atomic<bool>* c){
    auto frame=image_coordinate_frame(doc,"qpx.annotation-plugin-image/1",ObjectPlane{},c);
    frame.id="qpx.annotation-plugin-image/1:"+frame.source_id;frame.validate();return frame;
}
std::map<std::string,std::string> receipt_properties(const FormatPluginReceipt& receipt){
    (void)format_plugin_receipt_json(receipt);const auto& p=receipt.provider;
    if(p.domain!=FormatPluginDomain::Annotation)throw std::invalid_argument("Point import needs an annotation provider");
    const std::string prefix="qpx.annotation-plugin.";
    std::map<std::string,std::string> properties;
    auto put=[&](const std::string& name,const std::string& value){field(value,false);properties.emplace(prefix+name,value);};
    put("schema","qpx.annotation-plugin-object/1");put("domain","annotation");put("provider_id",p.provider_id);put("format_id",p.format_id);put("display_name",p.display_name);
    put("version",p.version);put("contract",p.contract);put("profile",p.profile);put("abi",std::to_string(p.abi));
    put("implementation_sha256",p.implementation_sha256);put("module_sha256",p.module_sha256);
    put("options",receipt.options);put("source_sha256",receipt.source_sha256);put("receipt_sha256",format_plugin_receipt_identity(receipt));
    put("dependency_count",std::to_string(p.dependencies.size()));
    for(std::size_t i=0;i<p.dependencies.size();++i){const auto key="dependency."+std::to_string(i)+".";put(key+"name",p.dependencies[i].name);put(key+"sha256",p.dependencies[i].sha256);}
    return properties;
}
std::size_t object_payload(const Annotation& object){
    std::size_t n=object.name.capacity()+object.class_name.capacity()+64;
    charge(n,object.geometry.payload_bytes(),max_work);
    if(object.properties){charge(n,object.properties->payload_bytes()+64,max_work);for(const auto& [key,value]:object.properties->metadata()){charge(n,key.capacity()-key.size()+value.capacity()-value.size(),max_work);}}
    return n;
}
std::size_t class_map_payload(const std::map<std::string,std::string>& map){std::size_t n=0;for(const auto& [a,b]:map){charge(n,128+a.capacity()+b.capacity(),max_work);}return n;}
std::size_t receipt_payload(const FormatPluginReceipt& receipt){
    std::size_t n=sizeof(receipt)+128;const auto& p=receipt.provider;
    for(const auto* s:{&p.provider_id,&p.format_id,&p.display_name,&p.version,&p.contract,&p.profile,&p.implementation_sha256,&p.module_sha256,&p.default_options,&receipt.options,&receipt.source_sha256})charge(n,s->capacity()+32,max_work);
    charge(n,product(p.dependencies.capacity(),sizeof(FormatPluginDependency)),max_work);
    for(const auto& d:p.dependencies)charge(n,d.name.capacity()+d.sha256.capacity()+64,max_work);return n;
}
void admit_annotation_append(std::size_t payload,std::size_t count,std::size_t limit){
    std::size_t work=host_workspace;charge(work,payload,limit);charge(work,payload,limit);charge(work,product(count,sizeof(AnnotationPluginObjectMapping)+256),limit);
}
}

void MatrixPluginImportOptions::validate() const {
    frame.validate();positions.validate();limits.validate();work_limits(limits.work_bytes,plugin_work_bytes);
    if(limits.format!=ExpressionCatalogueFormat::Qpx)throw std::invalid_argument("External matrix format is selected by its provider, not a legacy catalogue format");
}
ExpressionMatrixImportResult import_expression_matrix_plugin(const std::string& provider_id,const std::string& input,const std::string& positions,const std::string& store,const MatrixPluginImportOptions& options,const std::optional<std::string>& plugin_options,const std::atomic<bool>* c){
    options.validate();workflow_checkpoint(c);
#ifdef QUPATHXX_HAS_PROJECT
    detail::WorkflowInput primary(input,options.limits.file_bytes,false,c),coordinates(positions,options.positions.file_bytes,false,c);
    detail::ReaderFileGuard primary_guard(primary.path()),coordinates_guard(coordinates.path());primary.verify(true,c);coordinates.verify(true,c);
    auto session=detail::MatrixPluginSession::open(provider_id,primary.path(),plugin_options,options.plugin_work_bytes,c);
    const auto dimensions=session->metadata();
    if(!dimensions.features || dimensions.features>options.limits.max_features || !dimensions.observations || dimensions.observations>options.limits.max_observations || dimensions.features>std::numeric_limits<std::uint64_t>::max()/dimensions.observations)throw WorkflowCaptureLimit("External matrix dimensions exceed import limits");
    const auto entries=dimensions.features*dimensions.observations;
    if(entries>options.limits.max_entries)throw WorkflowCaptureLimit("External dense matrix entries exceed import limits");
    if(session->receipt().source_sha256!=primary.sha256())throw WorkflowInputChanged("Matrix provider source differs from native snapshot");
    std::size_t reserved=host_workspace;charge(reserved,std::size_t(session->retained_work_bytes()),options.limits.work_bytes);
    if(options.limits.work_bytes-reserved<4u*1024*1024)throw WorkflowCaptureLimit("External matrix leaves no indexed writer budget");
    auto position_options=options.positions;
    position_options.work_bytes=std::min(position_options.work_bytes,options.limits.work_bytes-reserved);
    auto table=read_spatial_positions(coordinates.path(),position_options,c);
    if(table.source_sha256!=coordinates.sha256())throw WorkflowInputChanged("Position parser source differs from native snapshot");
    if(table.observations.size()!=dimensions.observations)throw std::invalid_argument("Matrix and position observation sets differ");
    charge(reserved,product(table.observations.capacity(),sizeof(SpatialObservation)),options.limits.work_bytes);
    for(const auto& row:table.observations){workflow_checkpoint(c);charge(reserved,row.id.capacity()+32,options.limits.work_bytes);}
    cancellable_sort(table.observations.begin(),table.observations.end(),[](const auto& a,const auto& b){return a.id<b.id;},c);
    ExternalMatrixReceipt receipt{session->receipt(),coordinates.sha256(),options.positions.columns};
    const auto recipe=detail::matrix::external_recipe_identity(receipt,options.frame);
    ExpressionMatrixMetadata metadata;metadata.features=dimensions.features;metadata.observations=dimensions.observations;metadata.entries=entries;
    metadata.input_profile=detail::matrix::external_profile;metadata.spatial_frame=options.frame;
    detail::matrix::Writer writer(store,metadata,options.limits,reserved,c,receipt);
    for(std::uint64_t at=0;at<dimensions.features;){const auto count=std::min<std::uint64_t>(64,dimensions.features-at);auto rows=session->features(at,count,c);if(rows.size()!=count)throw std::invalid_argument("Matrix feature page count differs");for(const auto& row:rows){if(row.index!=at)throw std::invalid_argument("Matrix feature index differs");const auto id=detail::spatial_identifier(row.id);writer.feature_row(at++,id,row.name.empty()?id:detail::spatial_identifier(row.name),"feature");}}
    Sha256 observation_hash;text(observation_hash,"qpx.external-observation-axis/1");observation_hash.integer(dimensions.observations);
    for(std::uint64_t at=0;at<dimensions.observations;){const auto count=std::min<std::uint64_t>(64,dimensions.observations-at);auto rows=session->observations(at,count,c);if(rows.size()!=count)throw std::invalid_argument("Matrix observation page count differs");for(const auto& row:rows){workflow_checkpoint(c);if(row.index!=at)throw std::invalid_argument("Matrix observation index differs");if(!row.name.empty())throw std::invalid_argument("External matrix observation names are not represented by this profile");const auto id=detail::spatial_identifier(row.id);const auto found=std::lower_bound(table.observations.begin(),table.observations.end(),id,[](const auto& a,const auto& b){return a.id<b;});if(found==table.observations.end() || found->id!=id)throw std::invalid_argument("Matrix observation is missing from positions");writer.observation_row(at++,id,found->position);text(observation_hash,id);}}
    for(std::uint64_t at=0;at<entries;){const auto count=std::min<std::uint64_t>(4096,entries-at);auto values=session->values(at,count,c);if(values.size()!=count)throw std::invalid_argument("Matrix value page count differs");for(auto value:values){if((at&4095)==0)workflow_checkpoint(c);writer.count(at%dimensions.features,at/dimensions.features,value);++at;}}
    if(coordinates.bytes()>std::numeric_limits<std::uint64_t>::max()-primary.bytes())throw WorkflowCaptureLimit("Matrix input byte sum overflows");
    return writer.finish({primary.sha256(),recipe,observation_hash.finish()},primary.bytes()+coordinates.bytes(),[&]{workflow_checkpoint(c);primary_guard.verify();coordinates_guard.verify();primary.verify(false,c);coordinates.verify(false,c);session->verify_source();});
#else
    (void)provider_id;(void)input;(void)positions;(void)store;(void)plugin_options;
    throw std::runtime_error("Expression matrix storage requires native project/SQLite support");
#endif
}
std::optional<ExternalMatrixReceipt> external_expression_matrix_receipt(const ExpressionMatrixSource& source,std::size_t work,const std::atomic<bool>* c){
#ifdef QUPATHXX_HAS_PROJECT
    detail::matrix::work_limit(work);source.require_current(c);detail::ReaderFileGuard guard(source.path());detail::matrix::no_sidecars(source.path());
    detail::matrix::Database db(source.path(),false,work,c);auto metadata=detail::matrix::load_meta(db);detail::matrix::bind_file_identity(metadata,source.file_sha256());
    if(metadata.source_sha256!=source.metadata().source_sha256)throw WorkflowInputChanged("Expression receipt source identity differs");
    auto receipt=detail::matrix::load_external_receipt(db);
    std::size_t bytes=source.payload_bytes();charge(bytes,host_workspace,work);db.admit(bytes);guard.verify();source.require_current(c);return receipt;
#else
    (void)source;(void)work;(void)c;throw std::runtime_error("Expression matrix storage requires native project/SQLite support");
#endif
}

void AnnotationPluginImportOptions::validate() const {
    work_limits(work_bytes,plugin_work_bytes);field(frame_id,false);
    if(!file_bytes || file_bytes>1024ull*1024*1024 || !max_points || max_points>1000000 || class_map.size()>4096)throw std::invalid_argument("Annotation plugin input/point/class-map limit exceeds bounds");
    for(const auto& [source,target]:class_map){field(source);field(target);}
    std::size_t bytes=host_workspace;charge(bytes,plugin_work_bytes,work_bytes);charge(bytes,class_map_payload(class_map),work_bytes);
}
CoordinateFrame annotation_plugin_target_frame(const Document& doc,const std::atomic<bool>* c){auto guards=image_guards(doc,c);auto frame=target_frame(doc,c);verify_guards(guards,c);return frame;}

struct AnnotationPluginPreview::Data {
    std::shared_ptr<const char> document;std::uint64_t revision=0;std::shared_ptr<ImageServer> server;
    std::string registry,sha;CoordinateFrame frame;FormatPluginReceipt receipt;
    std::map<std::string,ObjectClass> mapped_classes;
    std::unique_ptr<detail::WorkflowInput> input;std::vector<Annotation> objects;
    std::thread::id owner;std::size_t payload=0,work=0;bool consumed=false;
};
struct FormatImportKernel {
    static AnnotationPluginPreview capture(const Document& doc,const std::string& provider,const std::string& input,const AnnotationPluginImportOptions& options,const std::optional<std::string>& plugin_options,const std::atomic<bool>* c){
        options.validate();workflow_checkpoint(c);
        auto data=std::make_unique<AnnotationPluginPreview::Data>();data->document=doc.analysis_identity_.token;data->revision=doc.revision();data->server=doc.server();data->owner=std::this_thread::get_id();data->work=options.work_bytes;
        std::size_t retained=sizeof(*data)+options.frame_id.capacity()+class_map_payload(options.class_map);
        for(const auto& [label,key]:options.class_map){workflow_checkpoint(c);const auto found=doc.classes().find(key);if(found==doc.classes().end())throw std::invalid_argument("Annotation class mapping names an absent native class key");if(data->mapped_classes.emplace(key,found->second).second)charge(retained,192+key.capacity()+found->second.name.capacity()+found->second.parent.capacity(),options.work_bytes);}
        data->registry=registry_identity(doc,c);auto target_guards=image_guards(doc,c);data->frame=target_frame(doc,c);
        if(!options.frame_id.empty() && options.frame_id!=data->frame.id)throw std::invalid_argument("Annotation target frame ID differs from current native image frame");
        data->input=std::make_unique<detail::WorkflowInput>(input,options.file_bytes,false,c);detail::ReaderFileGuard source_guard(data->input->path());data->input->verify(true,c);
        // WorkflowInput owns the canonical path, SHA and one bounded FileVersion.
        // Reserve that version's maximum path/tokens without exposing or growing
        // the legacy input class solely for this importer.
        charge(retained,data->input->path().capacity()+data->input->sha256().capacity()+data->frame.id.capacity()+data->frame.source_id.capacity()+data->registry.capacity()+65536,options.work_bytes);
        std::size_t live=host_workspace;charge(live,retained,options.work_bytes);charge(live,options.plugin_work_bytes,options.work_bytes);
        auto session=detail::AnnotationPluginSession::open(provider,data->input->path(),plugin_options,options.plugin_work_bytes,c);
        if(session->receipt().source_sha256!=data->input->sha256())throw WorkflowInputChanged("Annotation provider source differs from native snapshot");
        const auto count=session->metadata().points;if(!count || count>options.max_points)throw WorkflowCaptureLimit("Annotation plugin point count exceeds import limit");
        data->receipt=session->receipt();auto common=receipt_properties(data->receipt);
        // Receipts are bounded independently; the serialized owner and all map
        // construction scratch fit host_workspace. Persisted per-row copies are
        // charged using the native ObjectProperties authority below.
        charge(retained,receipt_payload(data->receipt),options.work_bytes);
        Sha256 recipe;text(recipe,"qpx.annotation-plugin-recipe/1");text(recipe,format_plugin_receipt_identity(data->receipt));text(recipe,data->frame.sha256());text(recipe,data->registry);recipe.integer(count);recipe.integer(options.class_map.size());for(const auto& [label,key]:options.class_map){text(recipe,label);text(recipe,key);}const auto recipe_sha=recipe.finish();
        const auto object_array=product(count,sizeof(Annotation));
        std::size_t capture_used=host_workspace;charge(capture_used,std::size_t(session->retained_work_bytes()),options.work_bytes);charge(capture_used,retained,options.work_bytes);charge(capture_used,object_array,options.work_bytes);
        data->objects.reserve(std::size_t(count));charge(retained,product(data->objects.capacity(),sizeof(Annotation)),options.work_bytes);
        Sha256 preview;text(preview,"qpx.annotation-plugin-preview/1");text(preview,recipe_sha);preview.integer(count);
        for(std::uint64_t at=0;at<count;){const auto n=std::min<std::uint64_t>(64,count-at);auto rows=session->points(at,n,c);if(rows.size()!=n)throw std::invalid_argument("Annotation point page count differs");
            for(const auto& row:rows){workflow_checkpoint(c);const auto id=detail::spatial_identifier(row.source_id);field(row.class_label,false);if(row.z<0 || row.t<0)throw std::invalid_argument("Annotation plugin Z/time must be explicit nonnegative indices");ObjectPlane plane(row.c,std::uint32_t(row.z),std::uint32_t(row.t));doc.validate_object_plane(plane);
                std::string native_class;if(!row.class_label.empty()){const auto mapped=options.class_map.find(row.class_label);if(mapped==options.class_map.end())throw std::invalid_argument("Annotation source class has no explicit native class mapping");native_class=mapped->second;}
                Sha256 row_hash;text(row_hash,"qpx.annotation-plugin-row/1");row_hash.integer(at);text(row_hash,id);text(row_hash,row.class_label);text(row_hash,native_class);row_hash.integer(real_bits(row.x));row_hash.integer(real_bits(row.y));row_hash.integer(std::uint64_t(std::int64_t(row.c)));row_hash.integer(std::uint32_t(row.z));row_hash.integer(std::uint32_t(row.t));const auto row_sha=row_hash.finish();text(preview,row_sha);
                auto properties=common;const std::string prefix="qpx.annotation-plugin.";properties.emplace(prefix+"source_id",id);properties.emplace(prefix+"source_class",row.class_label);properties.emplace(prefix+"native_class",native_class);properties.emplace(prefix+"source_row",std::to_string(at));properties.emplace(prefix+"frame_sha256",data->frame.sha256());properties.emplace(prefix+"recipe_sha256",recipe_sha);properties.emplace(prefix+"row_sha256",row_sha);
                Annotation object;object.id=at+1;object.name=id;object.geometry=Roi::points(Ring{{row.x,row.y}});object.class_name=std::move(native_class);object.plane=plane;object.properties=std::make_shared<const ObjectProperties>(std::move(properties));
                auto peak=host_workspace;charge(peak,std::size_t(session->retained_work_bytes()),options.work_bytes);charge(peak,retained,options.work_bytes);charge(peak,object_payload(object),options.work_bytes);
                charge(retained,object_payload(object),options.work_bytes);data->objects.push_back(std::move(object));++at;
            }
        }
        session->verify_source();source_guard.verify();data->input->verify(false,c);verify_guards(target_guards,c);session.reset();
        // Exact IDs are never parsed as numbers. Sort views into immutable owned
        // names only after releasing the DLL reservation; comparisons poll cancel.
        std::size_t sort_work=host_workspace;charge(sort_work,retained,options.work_bytes);charge(sort_work,product(count,sizeof(std::string_view)),options.work_bytes);
        std::vector<std::string_view> ids;ids.reserve(std::size_t(count));for(const auto& object:data->objects){workflow_checkpoint(c);ids.emplace_back(object.name);}
        cancellable_sort(ids.begin(),ids.end(),[](auto a,auto b){return a<b;},c);for(std::size_t i=1;i<ids.size();++i){if((i&4095)==0)workflow_checkpoint(c);if(ids[i]==ids[i-1])throw std::invalid_argument("Annotation plugin source IDs are not unique");}
        if(doc.analysis_identity_.token!=data->document || doc.revision()!=data->revision || doc.server()!=data->server || registry_identity(doc,c)!=data->registry || target_frame(doc,c).sha256()!=data->frame.sha256())throw WorkflowInputChanged("Annotation target changed during preview");
        source_guard.verify();data->input->verify(false,c);verify_guards(target_guards,c);workflow_checkpoint(c);data->sha=preview.finish();charge(retained,data->sha.capacity(),options.work_bytes);data->payload=retained;
        admit_annotation_append(data->payload,data->objects.size(),data->work);
        return AnnotationPluginPreview(std::move(data));
    }
    static AnnotationPluginImportResult apply(Document& doc,AnnotationPluginPreview& preview,const std::atomic<bool>* c){
        if(!preview.data_ || preview.data_->consumed)throw std::logic_error("Annotation plugin preview is absent or already consumed");auto& data=*preview.data_;workflow_require_owner(data.owner);workflow_checkpoint(c);
        if(doc.analysis_identity_.token!=data.document || doc.revision()!=data.revision || doc.server()!=data.server || data.server->closed())throw WorkflowInputChanged("Annotation preview belongs to a changed or different document");
        if(registry_identity(doc,c)!=data.registry)throw WorkflowInputChanged("Annotation class registry changed since preview");
        for(const auto& [key,value]:data.mapped_classes){const auto found=doc.classes().find(key);if(found==doc.classes().end() || !(found->second==value))throw WorkflowInputChanged("Mapped annotation class changed since preview");}
        auto target_guards=image_guards(doc,c);detail::ReaderFileGuard input_guard(data.input->path());data.input->verify(true,c);
        if(target_frame(doc,c).sha256()!=data.frame.sha256())throw WorkflowInputChanged("Annotation target frame changed since preview");
        // Account the result copy and a conservative private append transaction
        // (copied rows, ID maps, edit/undo nodes). Document retains its own quota.
        admit_annotation_append(data.payload,data.objects.size(),data.work);
        AnnotationPluginImportResult result;result.receipt=data.receipt;result.preview_sha256=data.sha;result.objects.reserve(data.objects.size());
        for(const auto& object:data.objects){workflow_checkpoint(c);doc.validate_object_plane(object.plane);if(!object.class_name.empty() && doc.classes().find(object.class_name)==doc.classes().end())throw WorkflowInputChanged("Mapped annotation class is unavailable");result.objects.push_back({object.name,0});}
        input_guard.verify();data.input->verify(false,c);verify_guards(target_guards,c);workflow_checkpoint(c);
        const auto assigned=doc.append_objects(data.objects,{},"Import plugin annotations");
        // append_objects has completed its one atomic transaction. No allocations,
        // cancellation or fallible source checks are permitted after publication.
        for(std::size_t i=0;i<assigned.size();++i)result.objects[i].object_id=assigned[i];data.consumed=true;return result;
    }
};
AnnotationPluginPreview::AnnotationPluginPreview(std::unique_ptr<Data> value):data_(std::move(value)){}
AnnotationPluginPreview::AnnotationPluginPreview(AnnotationPluginPreview&&) noexcept=default;
AnnotationPluginPreview& AnnotationPluginPreview::operator=(AnnotationPluginPreview&&) noexcept=default;
AnnotationPluginPreview::~AnnotationPluginPreview()=default;
const FormatPluginReceipt& AnnotationPluginPreview::receipt() const {if(!data_)throw std::logic_error("Moved annotation preview");return data_->receipt;}
const CoordinateFrame& AnnotationPluginPreview::frame() const {if(!data_)throw std::logic_error("Moved annotation preview");return data_->frame;}
const std::vector<Annotation>& AnnotationPluginPreview::objects() const {if(!data_)throw std::logic_error("Moved annotation preview");return data_->objects;}
const std::string& AnnotationPluginPreview::sha256() const {if(!data_)throw std::logic_error("Moved annotation preview");return data_->sha;}
std::size_t AnnotationPluginPreview::payload_bytes() const {if(!data_)throw std::logic_error("Moved annotation preview");return data_->payload;}
bool AnnotationPluginPreview::consumed() const noexcept {return data_ && data_->consumed;}
AnnotationPluginPreview preview_annotation_plugin(const Document& doc,const std::string& provider,const std::string& input,const AnnotationPluginImportOptions& options,const std::optional<std::string>& plugin_options,const std::atomic<bool>* c){return FormatImportKernel::capture(doc,provider,input,options,plugin_options,c);}
AnnotationPluginImportResult apply_annotation_plugin(Document& doc,AnnotationPluginPreview& preview,const std::atomic<bool>* c){return FormatImportKernel::apply(doc,preview,c);}
} // namespace qpx
