#include "qupathxx/format_import.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/format_plugin.h"
#include "format_plugin_runtime.hpp"
#include "format_import_internal.hpp"
#include "project_database_output.hpp"
#include "project_export_guard.hpp"
#include "project_source_internal.hpp"
#include "expression_matrix_external.hpp"
#include "expression_matrix_writer.hpp"
#include "workflow_input.hpp"
#include "reader_file_guard.hpp"
#include "spatial_output_guard.hpp"
#include "spatial_count_text.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
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
const std::string& imported_source_id(const Annotation& object) {
    if(!object.properties)throw std::logic_error("Annotation preview has no source binding");
    return object.properties->metadata().at("qpx.annotation-plugin.source_id");
}
ObjectPlane resolve_annotation_plane(const Document& doc,const detail::PluginAnnotationObject& source,
        const AnnotationPluginImportOptions& options,const std::atomic<bool>* cancelled) {
    auto channel=source.c,z=source.z,time=source.t;
    if(source.plane_kind==QPX_ANNOTATION_PLANE_LINEAR_POSITION) {
        if(!source.linear_position || source.linear_position>65536)throw std::out_of_range("Legacy annotation position exceeds native axis bounds");
        const auto index=std::int32_t(source.linear_position-1);
        switch(options.legacy_position_axis) {
        case AnnotationLinearAxis::Channel:channel=index;break;
        case AnnotationLinearAxis::Z:z=index;break;
        case AnnotationLinearAxis::Time:time=index;break;
        default:throw std::invalid_argument("Legacy annotation stack position requires an explicit channel/Z/time axis");
        }
    }
    const auto metadata=doc.server()->raw_metadata(cancelled);
    const auto z_planes=metadata?metadata->z_planes:1,timepoints=metadata?metadata->timepoints:1;
    auto resolve=[](std::int32_t source_index,std::uint32_t count,const std::optional<std::uint32_t>& choice,const char* axis) {
        if(source_index>=0)return std::uint32_t(source_index);
        if(choice)return *choice;
        if(count==1)return std::uint32_t(0);
        throw std::invalid_argument(std::string("Unpositioned annotation requires an explicit ")+axis+" plane");
    };
    ObjectPlane plane(channel,resolve(z,z_planes,options.unpositioned_z,"Z"),resolve(time,timepoints,options.unpositioned_time,"time"));
    doc.validate_object_plane(plane);return plane;
}
template<class Iterator,class Less> void cancellable_sort(Iterator first,Iterator last,Less less,const std::atomic<bool>* c){
    std::size_t comparisons=0;workflow_checkpoint(c);
    std::sort(first,last,[&](const auto& a,const auto& b){if((comparisons++&4095)==0)workflow_checkpoint(c);return less(a,b);});workflow_checkpoint(c);
}
std::string registry_identity(const Document& doc,const std::atomic<bool>* c){
    Sha256 hash;text(hash,"qpx.annotation-plugin-class-registry/1");hash.integer(doc.classes().size());
    for(const auto& [key,value]:doc.classes()) {workflow_checkpoint(c);text(hash,key);text(hash,value.name);text(hash,value.parent);hash.integer(value.color);hash.integer(value.alpha);hash.integer(value.color_present);}
    return hash.finish();
}
CoordinateFrame target_frame(const Document& doc,const std::atomic<bool>* c){
    auto frame=image_coordinate_frame(doc,"qpx.annotation-plugin-image/1",ObjectPlane{},c);
    frame.id="qpx.annotation-plugin-image/1:"+frame.source_id;frame.validate();return frame;
}
std::map<std::string,std::string> receipt_properties(const FormatPluginReceipt& receipt){
    (void)format_plugin_receipt_json(receipt);const auto& p=receipt.provider;
    if(p.domain!=FormatPluginDomain::Annotation)throw std::invalid_argument("Annotation import needs an annotation provider");
    const std::string prefix="qpx.annotation-plugin.";
    std::map<std::string,std::string> properties;
    auto put=[&](const std::string& name,const std::string& value){field(value,false);properties.emplace(prefix+name,value);};
    put("schema",p.abi==QPX_ANNOTATION_ABI_V6?"qpx.annotation-plugin-object/6":p.abi==4?"qpx.annotation-plugin-object/4":p.abi==3?"qpx.annotation-plugin-object/3":p.abi==2?"qpx.annotation-plugin-object/2":"qpx.annotation-plugin-object/1");put("domain","annotation");put("provider_id",p.provider_id);put("format_id",p.format_id);put("display_name",p.display_name);
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
void MatrixStorePluginImportOptions::validate() const {
    matrix.validate();inventory.validate();content.validate();
    detail::bounded_utf8(primary_key,4096);
    if(primary_key.empty())throw std::invalid_argument("Matrix store requires an explicit primary metadata key");
    // Check simultaneous capture bounds before observing any directory. Actual
    // primary components/membership use reader_source's native validator.
    std::size_t capture=host_workspace;
    charge(capture,matrix.plugin_work_bytes,matrix.limits.work_bytes);
    charge(capture,inventory.inventory_bytes,matrix.limits.work_bytes);
    charge(capture,inventory.inventory_bytes,matrix.limits.work_bytes);
    charge(capture,4u*1024*1024,matrix.limits.work_bytes);
    if(matrix.limits.work_bytes-capture<4u*1024*1024)
        throw WorkflowCaptureLimit("Matrix store capture leaves no indexed writer budget");
}
namespace {
void preflight_matrix_store_outputs(const std::string& directory, const std::string& positions,
        const std::string& input_project, const std::string& new_qmx, const std::string& new_project,
        bool copies_beside_input) {
    namespace fs = std::filesystem;
    const auto input = detail::export_path(directory);
    const auto table = detail::export_path(positions);
    const auto project = detail::export_path(input_project);
    const auto qmx = detail::export_path(new_qmx);
    const auto output = detail::export_path(new_project);
    if (!fs::is_directory(fs::u8path(input)))
        throw std::invalid_argument("Matrix store requires an existing input directory");
    for (const auto* path : {&qmx, &output}) {
        detail::guard_file_output(input, "", *path);
        detail::protect_spatial_path(table, "", *path);
        detail::protect_spatial_path(project, "", *path);
        detail::guard_project_companion_output(project,*path);
        std::error_code error;
        const auto status = fs::symlink_status(fs::u8path(*path), error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw fs::filesystem_error("Inspect new matrix project output", fs::u8path(*path), error);
        if (status.type() != fs::file_type::not_found)
            throw WorkflowOutputExists("Matrix project output already exists: " + *path);
    }
    detail::protect_spatial_path(qmx, "", output);
    const auto check_managed_folder = [&](const std::string& path) {
        const auto managed = fs::u8path(path).parent_path() / ".qpx-expression";
        detail::guard_file_output(input, "", managed.u8string());
        const auto folder = managed.u8string();
        std::error_code error;
        const auto status = fs::symlink_status(managed, error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw fs::filesystem_error("Inspect managed matrix directory", managed, error);
        if (status.type() != fs::file_type::not_found && status.type() != fs::file_type::directory)
            throw std::invalid_argument("Invalid managed matrix directory: " + folder);
        // A file destination cannot double as the managed directory.
        const auto a = fs::weakly_canonical(managed);
        for (const auto* file : {&qmx, &output}) {
            const auto b = fs::weakly_canonical(fs::u8path(*file));
#ifdef _WIN32
            const auto& x = a.native(); const auto& y = b.native();
            const bool collision = CompareStringOrdinal(x.data(), int(x.size()), y.data(), int(y.size()), TRUE) == CSTR_EQUAL;
#else
            const bool collision = a == b;
#endif
            if (collision) throw std::invalid_argument("Output conflicts with managed matrix directory: " + folder);
        }
    };
    if(copies_beside_input)check_managed_folder(project);
    check_managed_folder(output);
}
}
void preflight_matrix_store_project_outputs(const std::string& directory, const std::string& positions,
        const std::string& input_project, const std::string& new_qmx, const std::string& new_project) {
    preflight_matrix_store_outputs(directory,positions,input_project,new_qmx,new_project,true);
}
#ifdef QUPATHXX_HAS_PROJECT
namespace {
// Both source factories use this same catalogue/position/value authority.
ExpressionMatrixImportResult import_matrix_session(detail::MatrixPluginSession& session,
        const detail::WorkflowInput& coordinates,const std::string& store,
        const MatrixPluginImportOptions& options,const std::string& source_sha256,
        std::uint64_t source_bytes,const std::function<void()>& verify,const std::atomic<bool>* c) {
    const auto dimensions=session.metadata();
    if(!dimensions.features || dimensions.features>options.limits.max_features || !dimensions.observations || dimensions.observations>options.limits.max_observations || dimensions.features>std::numeric_limits<std::uint64_t>::max()/dimensions.observations)throw WorkflowCaptureLimit("External matrix dimensions exceed import limits");
    const auto logical_entries=dimensions.features*dimensions.observations;
    const auto entries=dimensions.stored_entries.value_or(logical_entries);
    if(entries>options.limits.max_entries)throw WorkflowCaptureLimit("External matrix entries exceed import limits");
    if(session.receipt().source_sha256!=source_sha256)throw WorkflowInputChanged("Matrix provider source differs from native snapshot");
    std::size_t reserved=host_workspace;charge(reserved,std::size_t(session.retained_work_bytes()),options.limits.work_bytes);
    if(options.limits.work_bytes-reserved<4u*1024*1024)throw WorkflowCaptureLimit("External matrix leaves no indexed writer budget");
    auto position_options=options.positions;
    position_options.work_bytes=std::min(position_options.work_bytes,options.limits.work_bytes-reserved);
    auto table=read_spatial_positions(coordinates.path(),position_options,c);
    if(table.source_sha256!=coordinates.sha256())throw WorkflowInputChanged("Position parser source differs from native snapshot");
    if(table.observations.size()!=dimensions.observations)throw std::invalid_argument("Matrix and position observation sets differ");
    charge(reserved,product(table.observations.capacity(),sizeof(SpatialObservation)),options.limits.work_bytes);
    for(const auto& row:table.observations){workflow_checkpoint(c);charge(reserved,row.id.capacity()+32,options.limits.work_bytes);}
    cancellable_sort(table.observations.begin(),table.observations.end(),[](const auto& a,const auto& b){return a.id<b.id;},c);
    ExternalMatrixReceipt receipt{session.receipt(),coordinates.sha256(),options.positions.columns};
    const auto& store_source=session.store_source();
    std::optional<ExternalMatrixStoreReceipt> store_receipt;
    if(store_source)store_receipt=ExternalMatrixStoreReceipt{receipt,store_source->primary_key,store_source->observed_sha256,
        store_source->content_sha256,store_source->files,store_source->bytes};
    const auto abi=receipt.reader.provider.abi;
    const bool typed=abi==QPX_MATRIX_ABI_V2 || abi==QPX_MATRIX_ABI_V5;
    const auto recipe=store_receipt?detail::matrix::store_external_recipe_identity(*store_receipt,options.frame,dimensions.domain,dimensions.value_domain):
        typed?detail::matrix::typed_external_recipe_identity(receipt,options.frame,dimensions.domain,dimensions.value_domain):detail::matrix::external_recipe_identity(receipt,options.frame);
    ExpressionMatrixMetadata metadata;metadata.features=dimensions.features;metadata.observations=dimensions.observations;metadata.entries=entries;
    metadata.input_profile=store_receipt?(dimensions.stored_entries?detail::matrix::sparse_store_external_profile:
                                                                   detail::matrix::store_external_profile):
        dimensions.stored_entries?detail::matrix::sparse_file_external_profile:
        typed?detail::matrix::typed_external_profile:detail::matrix::external_profile;
    metadata.spatial_frame=options.frame;
    metadata.domain=dimensions.domain;metadata.value_domain=dimensions.value_domain;
    auto writer=store_receipt?std::make_unique<detail::matrix::Writer>(store,metadata,options.limits,reserved,c,*store_receipt):
        std::make_unique<detail::matrix::Writer>(store,metadata,options.limits,reserved,c,receipt);
    for(std::uint64_t at=0;at<dimensions.features;){const auto count=std::min<std::uint64_t>(64,dimensions.features-at);auto rows=session.features(at,count,c);if(rows.size()!=count)throw std::invalid_argument("Matrix feature page count differs");for(const auto& row:rows){if(row.index!=at)throw std::invalid_argument("Matrix feature index differs");const auto id=detail::spatial_identifier(row.id);writer->feature_row(at++,id,row.name.empty()?id:detail::spatial_identifier(row.name),"feature");}}
    Sha256 observation_hash;text(observation_hash,"qpx.external-observation-axis/1");observation_hash.integer(dimensions.observations);
    for(std::uint64_t at=0;at<dimensions.observations;){const auto count=std::min<std::uint64_t>(64,dimensions.observations-at);auto rows=session.observations(at,count,c);if(rows.size()!=count)throw std::invalid_argument("Matrix observation page count differs");for(const auto& row:rows){workflow_checkpoint(c);if(row.index!=at)throw std::invalid_argument("Matrix observation index differs");if(!row.name.empty())throw std::invalid_argument("External matrix observation names are not represented by this profile");const auto id=detail::spatial_identifier(row.id);const auto found=std::lower_bound(table.observations.begin(),table.observations.end(),id,[](const auto& a,const auto& b){return a.id<b;});if(found==table.observations.end() || found->id!=id)throw std::invalid_argument("Matrix observation is missing from positions");writer->observation_row(at++,id,found->position);text(observation_hash,id);}}
    if(dimensions.stored_entries) {
        std::optional<std::uint64_t> previous;
        for(std::uint64_t at=0;at<entries;) {
            const auto count=std::min<std::uint64_t>(4096,entries-at);
            for(const auto& entry:session.sparse_values(at,count,c)) {
                if(entry.linear>=logical_entries || (previous && entry.linear<=*previous))
                    throw std::invalid_argument("Sparse matrix coordinate is outside axes or out of order");
                previous=entry.linear;
                const auto feature=entry.linear%dimensions.features,observation=entry.linear/dimensions.features;
                if(metadata.domain==ExpressionValueDomain::Counts)writer->count(feature,observation,entry.value_bits);
                else writer->continuous(feature,observation,detail::matrix::real_value(entry.value_bits));
            }
            at+=count;
        }
    } else if(metadata.domain==ExpressionValueDomain::Counts) {
        for(std::uint64_t at=0;at<entries;){const auto count=std::min<std::uint64_t>(4096,entries-at);auto values=session.values(at,count,c);if(values.size()!=count)throw std::invalid_argument("Matrix value page count differs");for(auto value:values){if((at&4095)==0)workflow_checkpoint(c);writer->count(at%dimensions.features,at/dimensions.features,value);++at;}}
    } else {
        for(std::uint64_t at=0;at<entries;){const auto count=std::min<std::uint64_t>(4096,entries-at);auto values=session.continuous_values(at,count,c);if(values.size()!=count)throw std::invalid_argument("Matrix value page count differs");for(auto value:values){if((at&4095)==0)workflow_checkpoint(c);writer->continuous(at%dimensions.features,at/dimensions.features,value);++at;}}
    }
    if(coordinates.bytes()>std::numeric_limits<std::uint64_t>::max()-source_bytes)throw WorkflowCaptureLimit("Matrix input byte sum overflows");
    return writer->finish({source_sha256,recipe,observation_hash.finish()},source_bytes+coordinates.bytes(),verify);
}
}
#endif
ExpressionMatrixImportResult import_expression_matrix_plugin(const std::string& provider_id,const std::string& input,const std::string& positions,const std::string& store,const MatrixPluginImportOptions& options,const std::optional<std::string>& plugin_options,const std::atomic<bool>* c){
    return detail::import_matrix_file_guarded(provider_id,input,positions,store,options,plugin_options,{},c);
}
ExpressionMatrixImportResult detail::import_matrix_file_guarded(const std::string& provider_id,
        const std::string& input,const std::string& positions,const std::string& store,
        const MatrixPluginImportOptions& options,const std::optional<std::string>& plugin_options,
        const std::function<void()>& verify_target,const std::atomic<bool>* c) {
    options.validate();workflow_checkpoint(c);
#ifdef QUPATHXX_HAS_PROJECT
    detail::WorkflowInput primary(input,options.limits.file_bytes,false,c),coordinates(positions,options.positions.file_bytes,false,c);
    detail::ReaderFileGuard primary_guard(primary.path()),coordinates_guard(coordinates.path());primary.verify(true,c);coordinates.verify(true,c);
    auto session=detail::MatrixPluginSession::open_with_file_limit(provider_id,primary.path(),plugin_options,options.plugin_work_bytes,options.limits.file_bytes,c);
    return import_matrix_session(*session,coordinates,store,options,primary.sha256(),primary.bytes(),[&]{
        workflow_checkpoint(c);primary_guard.verify();coordinates_guard.verify();primary.verify(false,c);coordinates.verify(false,c);session->verify_source();
        if(verify_target)verify_target();},c);
#else
    (void)provider_id;(void)input;(void)positions;(void)store;(void)plugin_options;(void)verify_target;
    throw std::runtime_error("Expression matrix storage requires native project/SQLite support");
#endif
}
namespace {
class MatrixProjectImage {
public:
    MatrixProjectImage(const std::string& path,std::uint64_t image,const std::atomic<bool>* c)
        : base_(path,1024ull*1024*1024,true,c),guard_(base_.path()),
          project_(Project::open_without_journal_recovery(base_.path())),
          document_(project_->document(image)),target_(document_->server(),c,project_->source_verification(image).get()),
          images_(project_->images()),
          managed_((std::filesystem::u8path(base_.path()).parent_path()/".qpx-expression").u8string()) {}

    void verify(const std::string& output,const std::atomic<bool>* c) const {
        base_.verify(false,c);
        guard_.verify();
        target_.verify(c);
        target_.protect(output);
        detail::protect_spatial_path(base_.path(),guard_.version()->identity,output);
        detail::guard_project_companion_output(base_.path(),output);
        detail::guard_project_export_output(*project_,images_,output,c);
        detail::guard_directory_output(managed_,output);
    }

    CoordinateFrame frame(ObjectPlane plane,const std::atomic<bool>* c) const {
        return image_coordinate_frame(*document_,detail::indexed_matrix_frame_id(*document_),plane,c);
    }

private:
    detail::WorkflowInput base_;
    detail::ReaderFileGuard guard_;
    std::shared_ptr<Project> project_;
    std::shared_ptr<Document> document_;
    detail::FormatImportImageGuard target_;
    std::vector<ProjectImage> images_;
    std::string managed_;
};
}

ExpressionMatrixImportResult index_expression_matrix_file_for_project(const std::string& provider_id,
        const std::string& input,const std::string& positions,const std::string& new_qmx,
        const std::string& input_project,std::uint64_t image,MatrixPluginImportOptions options,
        const std::optional<std::string>& plugin_options,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const auto output=detail::export_path(new_qmx);
    const auto matrix=detail::export_path(input),table=detail::export_path(positions);
    const MatrixProjectImage target(input_project,image,cancelled);
    const auto verify=[&]{
        target.verify(output,cancelled);
        detail::protect_spatial_path(matrix,"",output);
        detail::protect_spatial_path(table,"",output);
    };
    verify();
    options.frame=target.frame(options.frame.plane,cancelled);
    return detail::import_matrix_file_guarded(provider_id,matrix,table,output,options,plugin_options,verify,cancelled);
}
ExpressionMatrixImportResult import_expression_matrix_store_plugin(const std::string& provider_id,
        const std::string& input,const std::string& positions,const std::string& store,
        const MatrixStorePluginImportOptions& options,const std::optional<std::string>& plugin_options,const std::atomic<bool>* c) {
    return detail::import_matrix_store_guarded(provider_id,input,positions,store,options,plugin_options,{},c);
}
ExpressionMatrixImportResult detail::import_matrix_store_guarded(const std::string& provider_id,
        const std::string& input,const std::string& positions,const std::string& store,
        const MatrixStorePluginImportOptions& options,const std::optional<std::string>& plugin_options,
        const std::function<void()>& verify_target,const std::atomic<bool>* c) {
    options.validate();workflow_checkpoint(c);
#ifdef QUPATHXX_HAS_PROJECT
    const auto directory=detail::export_path(input),destination=detail::export_path(store);
    detail::WorkflowInput coordinates(positions,options.matrix.positions.file_bytes,false,c);
    detail::ReaderFileGuard coordinates_guard(coordinates.path());coordinates.verify(true,c);
    const auto protect=[&]{
        detail::guard_file_output(directory,"",destination);
        detail::guard_file_output(coordinates.path(),coordinates_guard.version()->identity,destination);
    };
    protect();
    auto session=detail::MatrixPluginSession::open_store(provider_id,directory,options.primary_key,plugin_options,
        options.matrix.plugin_work_bytes,options.inventory,options.content,options.matrix.limits.work_bytes-host_workspace,c);
    const auto& source=session->store_source();
    if(!source)throw std::logic_error("Matrix directory session lacks its source receipt");
    return import_matrix_session(*session,coordinates,destination,options.matrix,source->content_sha256,source->bytes,[&]{
        workflow_checkpoint(c);coordinates_guard.verify();coordinates.verify(false,c);session->verify_source(c);protect();
        if(verify_target)verify_target();},c);
#else
    (void)provider_id;(void)input;(void)positions;(void)store;(void)plugin_options;(void)verify_target;
    throw std::runtime_error("Expression matrix storage requires native project/SQLite support");
#endif
}

ExpressionMatrixImportResult index_expression_matrix_store_for_project(const std::string& provider_id,
        const std::string& directory,const std::string& positions,const std::string& new_qmx,
        const std::string& input_project,std::uint64_t image,MatrixStorePluginImportOptions options,
        const std::optional<std::string>& plugin_options,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const auto output=detail::export_path(new_qmx);
    const MatrixProjectImage target(input_project,image,cancelled);
    const auto verify=[&]{target.verify(output,cancelled);};
    verify();
    options.matrix.frame=target.frame(options.matrix.frame.plane,cancelled);
    return detail::import_matrix_store_guarded(provider_id,directory,positions,output,options,plugin_options,verify,cancelled);
}

ExpressionMatrixImportResult index_expression_matrix_store_for_project(const std::string& provider_id,
        const std::string& directory,const std::string& positions,const std::string& new_qmx,
        const MatrixStoreProjectTarget& target,MatrixStorePluginImportOptions options,
        const std::optional<std::string>& plugin_options,const std::atomic<bool>* cancelled) {
    namespace fs=std::filesystem;
    workflow_checkpoint(cancelled);
    const auto store=detail::export_path(directory),table=detail::export_path(positions);
    const auto indexed=detail::export_path(new_qmx),output=detail::export_path(target.new_project);
    const auto folder=fs::u8path(output).parent_path();
    detail::WorkflowInput base(target.input_project,1024ull*1024*1024,true,cancelled);
    const auto project=Project::open_without_journal_recovery(base.path());
    const auto document=project->document(target.image);
    detail::FormatImportImageGuard image(document->server(),cancelled,project->source_verification(target.image).get());
    const auto images=project->images();
    const auto protect_other_inputs=[&](const std::string& path) {
        detail::guard_directory_output(store,path);
        detail::protect_spatial_path(table,"",path);
        detail::protect_spatial_path(base.path(),"",path);
    };
    const auto verify=[&] {
        base.verify(false,cancelled);
        image.verify(cancelled);
        preflight_matrix_store_outputs(store,table,base.path(),indexed,output,false);
        for(const auto& path:{indexed,output}) {
            image.protect(path);
            detail::guard_project_export_output(*project,images,path,cancelled);
        }
        const auto managed=(folder/".qpx-expression").u8string();
        detail::visit_project_inputs(*project,images,[&](const std::string& path,bool directory) {
            if(directory)detail::guard_directory_output(path,managed);
        },cancelled);
        detail::guard_project_owned_copy_outputs(*project,images,folder,cancelled,protect_other_inputs);
    };
    verify();
    options.matrix.frame=image_coordinate_frame(*document,detail::indexed_matrix_frame_id(*document),options.matrix.frame.plane,cancelled);
    return detail::import_matrix_store_guarded(provider_id,store,table,indexed,options,plugin_options,verify,cancelled);
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

std::optional<ExternalMatrixStoreReceipt> external_expression_matrix_store_receipt(const ExpressionMatrixSource& source,
        std::size_t work,const std::atomic<bool>* c) {
#ifdef QUPATHXX_HAS_PROJECT
    detail::matrix::work_limit(work);source.require_current(c);detail::ReaderFileGuard guard(source.path());detail::matrix::no_sidecars(source.path());
    detail::matrix::Database db(source.path(),false,work,c);auto metadata=detail::matrix::load_meta(db);detail::matrix::bind_file_identity(metadata,source.file_sha256());
    if(metadata.source_sha256!=source.metadata().source_sha256)throw WorkflowInputChanged("Expression receipt source identity differs");
    auto receipt=detail::matrix::load_external_store_receipt(db);
    std::size_t bytes=source.payload_bytes();charge(bytes,host_workspace,work);db.admit(bytes);guard.verify();source.require_current(c);return receipt;
#else
    (void)source;(void)work;(void)c;throw std::runtime_error("Expression matrix storage requires native project/SQLite support");
#endif
}

void AnnotationPluginImportOptions::validate() const {
    work_limits(work_bytes,plugin_work_bytes);field(frame_id,false);
    if(!file_bytes || file_bytes>1024ull*1024*1024 || !max_points || max_points>1000000 || !max_geometry_bytes || max_geometry_bytes>1024ull*1024*1024 || class_map.size()>4096)throw std::invalid_argument("Annotation plugin input/object/geometry/class-map limit exceeds bounds");
    if(!max_source_bytes || max_source_bytes>1024ull*1024*1024 || (unpositioned_z && *unpositioned_z>=65536) ||
        (unpositioned_time && *unpositioned_time>=65536) || std::uint32_t(legacy_position_axis)>3)
        throw std::invalid_argument("Annotation source metadata/plane policy exceeds bounds");
    for(const auto& [source,target]:class_map){field(source);field(target);}
    std::size_t bytes=host_workspace;charge(bytes,plugin_work_bytes,work_bytes);charge(bytes,class_map_payload(class_map),work_bytes);
}
CoordinateFrame annotation_plugin_target_frame(const Document& doc,const std::atomic<bool>* c){detail::FormatImportImageGuard guards(doc.server(),c);auto frame=target_frame(doc,c);guards.verify(c);return frame;}

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
        data->registry=registry_identity(doc,c);detail::FormatImportImageGuard target_guards(doc.server(),c);data->frame=target_frame(doc,c);
        if(!options.frame_id.empty() && options.frame_id!=data->frame.id)throw std::invalid_argument("Annotation target frame ID differs from current native image frame");
        data->input=std::make_unique<detail::WorkflowInput>(input,options.file_bytes,false,c);detail::ReaderFileGuard source_guard(data->input->path());data->input->verify(true,c);
        // WorkflowInput owns the canonical path, SHA and one bounded FileVersion.
        // Reserve that version's maximum path/tokens without exposing or growing
        // the legacy input class solely for this importer.
        charge(retained,data->input->path().capacity()+data->input->sha256().capacity()+data->frame.id.capacity()+data->frame.source_id.capacity()+data->registry.capacity()+65536,options.work_bytes);
        std::size_t live=host_workspace;charge(live,retained,options.work_bytes);charge(live,options.plugin_work_bytes,options.work_bytes);
        auto session=detail::AnnotationPluginSession::open(provider,data->input->path(),plugin_options,options.plugin_work_bytes,c);
        if(session->receipt().source_sha256!=data->input->sha256())throw WorkflowInputChanged("Annotation provider source differs from native snapshot");
        const auto count=session->metadata().points;if(!count || count>options.max_points)throw WorkflowCaptureLimit("Annotation plugin object count exceeds import limit");
        const auto abi=session->receipt().provider.abi;
        const auto& source_metadata=session->metadata();
        if(abi==QPX_ANNOTATION_ABI_V6 &&
           (source_metadata.canvas_width!=std::uint64_t(data->frame.width) ||
            source_metadata.canvas_height!=std::uint64_t(data->frame.height)))
            throw std::invalid_argument("Annotation canvas dimensions differ from the target level-zero image: "+
                std::to_string(source_metadata.canvas_width)+"x"+std::to_string(source_metadata.canvas_height)+
                " versus "+std::to_string(data->frame.width)+"x"+std::to_string(data->frame.height));
        const bool geometry=abi>=2,source_facts=abi>=3;
        if(!source_facts && (options.unpositioned_z || options.unpositioned_time || options.legacy_position_axis!=AnnotationLinearAxis::Unspecified))
            throw std::invalid_argument("Source-plane choices require an annotation ABI3 provider");
        const auto expected_source=session->metadata().source_bytes,expected_properties=session->metadata().properties;
        if(expected_source>options.max_source_bytes)throw WorkflowCaptureLimit("Annotation source metadata bytes exceed import limit");
        std::uint64_t received_source=0,received_properties=0;
        const auto expected_geometry=session->metadata().geometry_bytes;
        if(expected_geometry>options.max_geometry_bytes)throw WorkflowCaptureLimit("Annotation plugin geometry bytes exceed import limit");
        std::uint64_t received_geometry=0;
        data->receipt=session->receipt();auto common=receipt_properties(data->receipt);
        if(abi==2)common.emplace("qpx.annotation-plugin.geometry_encoding","qro/1");
        // Receipts are bounded independently; the serialized owner and all map
        // construction scratch fit host_workspace. Persisted per-row copies are
        // charged using the native ObjectProperties authority below.
        charge(retained,receipt_payload(data->receipt),options.work_bytes);
        Sha256 recipe;text(recipe,abi==QPX_ANNOTATION_ABI_V6?"qpx.annotation-plugin-recipe/6":abi==5?"qpx.annotation-plugin-recipe/5":abi==4?"qpx.annotation-plugin-recipe/4":source_facts?"qpx.annotation-plugin-recipe/3":geometry?"qpx.annotation-plugin-recipe/2":"qpx.annotation-plugin-recipe/1");text(recipe,format_plugin_receipt_identity(data->receipt));text(recipe,data->frame.sha256());text(recipe,data->registry);recipe.integer(count);recipe.integer(options.class_map.size());for(const auto& [label,key]:options.class_map){text(recipe,label);text(recipe,key);}
        if(abi==QPX_ANNOTATION_ABI_V6){
            recipe.integer(source_metadata.canvas_width);recipe.integer(source_metadata.canvas_height);
            common.emplace("qpx.annotation-plugin.canvas_width",std::to_string(source_metadata.canvas_width));
            common.emplace("qpx.annotation-plugin.canvas_height",std::to_string(source_metadata.canvas_height));
        }
        if(source_facts){recipe.integer(options.unpositioned_z?*options.unpositioned_z:UINT64_MAX);recipe.integer(options.unpositioned_time?*options.unpositioned_time:UINT64_MAX);recipe.integer(std::uint32_t(options.legacy_position_axis));}
        const auto recipe_sha=recipe.finish();
        const auto object_array=product(count,sizeof(Annotation));
        std::size_t capture_used=host_workspace;charge(capture_used,std::size_t(session->retained_work_bytes()),options.work_bytes);charge(capture_used,retained,options.work_bytes);charge(capture_used,object_array,options.work_bytes);
        data->objects.reserve(std::size_t(count));charge(retained,product(data->objects.capacity(),sizeof(Annotation)),options.work_bytes);
        Sha256 preview;text(preview,abi==QPX_ANNOTATION_ABI_V6?"qpx.annotation-plugin-preview/6":abi==5?"qpx.annotation-plugin-preview/5":abi==4?"qpx.annotation-plugin-preview/4":source_facts?"qpx.annotation-plugin-preview/3":geometry?"qpx.annotation-plugin-preview/2":"qpx.annotation-plugin-preview/1");text(preview,recipe_sha);preview.integer(count);
        for(std::uint64_t at=0;at<count;){
            const auto n=std::min<std::uint64_t>(geometry?1:64,count-at);
            std::size_t page_used=host_workspace;charge(page_used,std::size_t(session->retained_work_bytes()),options.work_bytes);charge(page_used,retained,options.work_bytes);
            auto rows=session->objects(at,n,options.work_bytes-page_used,c);
            if(rows.size()!=n)throw std::invalid_argument("Annotation object page count differs");
            for(const auto& row:rows){workflow_checkpoint(c);const auto id=detail::spatial_identifier(row.source_id);field(row.class_label,false);
                if(!source_facts && (row.z<0 || row.t<0))throw std::invalid_argument("Annotation plugin Z/time must be explicit nonnegative indices");
                if(row.canvas_width && (row.canvas_width!=data->frame.width || row.canvas_height!=data->frame.height))throw std::invalid_argument("Annotation source mask canvas differs from selected native image frame");
                const auto plane=source_facts?resolve_annotation_plane(doc,row,options,c):ObjectPlane(row.c,std::uint32_t(row.z),std::uint32_t(row.t));doc.validate_object_plane(plane);
                std::string native_class;if(!row.class_label.empty()){const auto mapped=options.class_map.find(row.class_label);if(mapped==options.class_map.end())throw std::invalid_argument("Annotation source class has no explicit native class mapping");native_class=mapped->second;}
                if(row.geometry_bytes>expected_geometry-received_geometry)throw std::invalid_argument("Annotation geometry byte total differs");received_geometry+=row.geometry_bytes;
                if(row.source_bytes>expected_source-received_source || row.source_properties.size()>expected_properties-received_properties)throw std::invalid_argument("Annotation source metadata totals differ");
                received_source+=row.source_bytes;received_properties+=row.source_properties.size();
                Sha256 row_hash;text(row_hash,abi==QPX_ANNOTATION_ABI_V6?"qpx.annotation-plugin-row/6":abi==5?"qpx.annotation-plugin-row/5":abi==4?"qpx.annotation-plugin-row/4":source_facts?"qpx.annotation-plugin-row/3":geometry?"qpx.annotation-plugin-row/2":"qpx.annotation-plugin-row/1");row_hash.integer(at);text(row_hash,id);text(row_hash,row.class_label);text(row_hash,native_class);
                if(geometry)text(row_hash,row.geometry_sha256);else{row_hash.integer(real_bits(row.x));row_hash.integer(real_bits(row.y));}row_hash.integer(std::uint64_t(std::int64_t(row.c)));row_hash.integer(std::uint32_t(row.z));row_hash.integer(std::uint32_t(row.t));
                if(source_facts){text(row_hash,row.display_name);text(row_hash,row.canonical_geometry_sha256);row_hash.integer(row.geometry_encoding);row_hash.integer(row.plane_kind);row_hash.integer(row.linear_position);row_hash.integer(std::uint64_t(std::int64_t(plane.c)));row_hash.integer(plane.z);row_hash.integer(plane.t);row_hash.integer(row.color_rgb.has_value());if(row.color_rgb)row_hash.integer(*row.color_rgb);row_hash.integer(row.source_properties.size());for(const auto& [key,value]:row.source_properties){text(row_hash,key);text(row_hash,value);}}
                if(abi>=4)row_hash.integer(row.locked);
                const auto row_sha=row_hash.finish();text(preview,row_sha);
                auto properties=common;const std::string prefix="qpx.annotation-plugin.";properties.emplace(prefix+"source_id",id);properties.emplace(prefix+"source_class",row.class_label);properties.emplace(prefix+"native_class",native_class);properties.emplace(prefix+"source_row",std::to_string(at));properties.emplace(prefix+"frame_sha256",data->frame.sha256());properties.emplace(prefix+"recipe_sha256",recipe_sha);properties.emplace(prefix+"row_sha256",row_sha);
                if(geometry)properties.emplace(prefix+"geometry_sha256",row.geometry_sha256);
                if(source_facts){
                    properties.emplace(prefix+"geometry_encoding",row.geometry_encoding==1?"qro/1":row.geometry_encoding==2?"even-odd-rings/1":"binary-mask/1");
                    properties.emplace(prefix+"canonical_geometry_sha256",row.canonical_geometry_sha256);
                    properties.emplace(prefix+"source_c",std::to_string(row.c));properties.emplace(prefix+"source_z",std::to_string(row.z));properties.emplace(prefix+"source_t",std::to_string(row.t));
                    properties.emplace(prefix+"plane_kind",std::to_string(row.plane_kind));properties.emplace(prefix+"linear_position",std::to_string(row.linear_position));
                    for(const auto& [key,value]:row.source_properties)if(!properties.emplace(prefix+"source."+key,value).second)throw std::invalid_argument("Source fact collides with native receipt");
                }
                if(abi>=4)properties.emplace(prefix+"locked",row.locked?"true":"false");
                Annotation object;object.id=at+1;object.name=source_facts && !row.display_name.empty()?row.display_name:id;object.geometry=row.geometry;object.class_name=std::move(native_class);object.plane=plane;object.properties=std::make_shared<const ObjectProperties>(std::move(properties),std::map<std::string,double>{},row.locked,row.color_rgb);
                auto peak=host_workspace;charge(peak,std::size_t(session->retained_work_bytes()),options.work_bytes);charge(peak,retained,options.work_bytes);charge(peak,object_payload(object),options.work_bytes);
                charge(retained,object_payload(object),options.work_bytes);data->objects.push_back(std::move(object));++at;
            }
        }
        if(received_geometry!=expected_geometry || received_source!=expected_source || received_properties!=expected_properties)throw std::invalid_argument("Annotation geometry/source metadata total differs");
        session->verify_source();source_guard.verify();data->input->verify(false,c);target_guards.verify(c);session.reset();
        // Exact IDs are never parsed as numbers. Sort views into immutable owned
        // names only after releasing the DLL reservation; comparisons poll cancel.
        std::size_t sort_work=host_workspace;charge(sort_work,retained,options.work_bytes);charge(sort_work,product(count,sizeof(std::string_view)),options.work_bytes);
        std::vector<std::string_view> ids;ids.reserve(std::size_t(count));for(const auto& object:data->objects){workflow_checkpoint(c);ids.emplace_back(imported_source_id(object));}
        cancellable_sort(ids.begin(),ids.end(),[](auto a,auto b){return a<b;},c);for(std::size_t i=1;i<ids.size();++i){if((i&4095)==0)workflow_checkpoint(c);if(ids[i]==ids[i-1])throw std::invalid_argument("Annotation plugin source IDs are not unique");}
        if(doc.analysis_identity_.token!=data->document || doc.revision()!=data->revision || doc.server()!=data->server || registry_identity(doc,c)!=data->registry || target_frame(doc,c).sha256()!=data->frame.sha256())throw WorkflowInputChanged("Annotation target changed during preview");
        source_guard.verify();data->input->verify(false,c);target_guards.verify(c);workflow_checkpoint(c);data->sha=preview.finish();charge(retained,data->sha.capacity(),options.work_bytes);data->payload=retained;
        admit_annotation_append(data->payload,data->objects.size(),data->work);
        return AnnotationPluginPreview(std::move(data));
    }
    static AnnotationPluginImportResult apply(Document& doc,AnnotationPluginPreview& preview,const std::atomic<bool>* c){
        if(!preview.data_ || preview.data_->consumed)throw std::logic_error("Annotation plugin preview is absent or already consumed");auto& data=*preview.data_;workflow_require_owner(data.owner);workflow_checkpoint(c);
        if(doc.analysis_identity_.token!=data.document || doc.revision()!=data.revision || doc.server()!=data.server || data.server->closed())throw WorkflowInputChanged("Annotation preview belongs to a changed or different document");
        if(registry_identity(doc,c)!=data.registry)throw WorkflowInputChanged("Annotation class registry changed since preview");
        for(const auto& [key,value]:data.mapped_classes){const auto found=doc.classes().find(key);if(found==doc.classes().end() || !(found->second==value))throw WorkflowInputChanged("Mapped annotation class changed since preview");}
        detail::FormatImportImageGuard target_guards(doc.server(),c);detail::ReaderFileGuard input_guard(data.input->path());data.input->verify(true,c);
        if(target_frame(doc,c).sha256()!=data.frame.sha256())throw WorkflowInputChanged("Annotation target frame changed since preview");
        // Account the result copy and a conservative private append transaction
        // (copied rows, ID maps, edit/undo nodes). Document retains its own quota.
        admit_annotation_append(data.payload,data.objects.size(),data.work);
        AnnotationPluginImportResult result;result.receipt=data.receipt;result.preview_sha256=data.sha;result.objects.reserve(data.objects.size());
        for(const auto& object:data.objects){workflow_checkpoint(c);doc.validate_object_plane(object.plane);if(!object.class_name.empty() && doc.classes().find(object.class_name)==doc.classes().end())throw WorkflowInputChanged("Mapped annotation class is unavailable");result.objects.push_back({imported_source_id(object),0});}
        input_guard.verify();data.input->verify(false,c);target_guards.verify(c);workflow_checkpoint(c);
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
