#include "qupathxx/spatial_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "workflow_input.hpp"
#include "sha256.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <future>

namespace qpx {
namespace {
void new_output(const std::string& path) {
    const auto target=std::filesystem::u8path(path);
    if(std::filesystem::exists(std::filesystem::symlink_status(target)))
        throw WorkflowOutputExists("Choose a NEW spatial-count project");
    auto parent=target.parent_path();if(parent.empty())parent=".";
    if(!std::filesystem::is_directory(parent))throw std::invalid_argument("Spatial import output directory is missing");
}
void validate_projection(const Document& document,const SpatialCountsProjection& projection,const SpatialCountsOptions& options,const std::atomic<bool>* cancelled) {
    document.validate_object_plane(ObjectPlane(-1,options.z,options.t));
    const auto& metadata=document.metadata();std::size_t per_object=2048;
    for(const auto& feature:projection.features)per_object+=512+2*feature.size();
    if(projection.observations.size()>options.work_bytes/per_object)
        throw WorkflowCaptureLimit("Spatial-count object projection exceeds work budget");
    for(const auto& observation:projection.observations) {
        workflow_checkpoint(cancelled);
        if(observation.position.x>=metadata.width || observation.position.y>=metadata.height)
            throw std::invalid_argument("Spatial observation outside selected image");
    }
}
}
void SpatialImportRequest::validate() const {
    options.validate();workflow_path_argument(input_project);workflow_path_argument(output_project);
    workflow_path_argument(matrix);workflow_path_argument(features);workflow_path_argument(positions);
    if(!image_id)throw std::invalid_argument("Image ID must be positive");
}
struct SpatialImportPreview::Data {
    SpatialImportRequest request;SpatialCountsProjection projection;
    std::array<std::shared_ptr<const detail::WorkflowInput>,4> guards;
    std::string frame="image-level-zero-px",json,recipe_sha;
};
SpatialImportPreview::SpatialImportPreview(std::shared_ptr<const Data> data):data_(std::move(data)) {}
const SpatialImportRequest& SpatialImportPreview::request() const {return data_->request;}
const SpatialCountsProjection& SpatialImportPreview::projection() const {return data_->projection;}
const std::string& SpatialImportPreview::project_sha256() const {return data_->guards[0]->sha256();}
const std::string& SpatialImportPreview::coordinate_frame() const {return data_->frame;}
const std::string& SpatialImportPreview::json() const {return data_->json;}
std::shared_ptr<const SpatialImportPreview> preview_spatial_import(const SpatialImportRequest& request,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);request.validate();new_output(request.output_project);
    auto data=std::make_shared<SpatialImportPreview::Data>();data->request=request;
    data->guards[0]=std::make_shared<detail::WorkflowInput>(request.input_project,1024ULL*1024*1024,true,cancelled);
    const std::array<std::string,3> paths{request.matrix,request.features,request.positions};
    for(std::size_t i=0;i<paths.size();++i)data->guards[i+1]=std::make_shared<detail::WorkflowInput>(paths[i],request.options.file_bytes,false,cancelled);
    // Store canonical locators so changing the controller's working directory
    // cannot redirect publication after preview.
    data->request.input_project=data->guards[0]->path();data->request.matrix=data->guards[1]->path();
    data->request.features=data->guards[2]->path();data->request.positions=data->guards[3]->path();
    data->request.output_project=std::filesystem::weakly_canonical(std::filesystem::u8path(request.output_project)).u8string();
    data->projection=read_spatial_counts(data->request.matrix,data->request.features,data->request.positions,request.options,cancelled);
    for(std::size_t i=0;i<paths.size();++i)if(data->projection.source_sha256[i]!=data->guards[i+1]->sha256())
        throw WorkflowInputChanged("Spatial-count input changed during preview");
    auto project=Project::open_without_journal_recovery(data->request.input_project);
    const auto document=project->document(request.image_id);validate_projection(*document,data->projection,request.options,cancelled);
    for(const auto& guard:data->guards)guard->verify(false,cancelled);
    new_output(data->request.output_project);
    data->json=nlohmann::json{{"schema","qpx.spatial-import-preview/1"},{"project_sha256",data->guards[0]->sha256()},
        {"image_id",request.image_id},{"image_width",document->metadata().width},{"image_height",document->metadata().height},
        {"features",data->projection.features},{"observations",data->projection.observations.size()},
        {"matrix_features",data->projection.matrix_features},{"matrix_entries",data->projection.matrix_entries},
        {"source_sha256",data->projection.source_sha256},{"coordinate_frame",data->frame},{"value_domain","counts"},
        {"z",request.options.z},{"t",request.options.t},{"file_bytes",request.options.file_bytes},{"work_bytes",request.options.work_bytes},
        {"max_features",request.options.max_features},{"max_observations",request.options.max_observations},{"max_entries",request.options.max_entries},
        {"input_project",data->request.input_project},{"matrix",data->request.matrix},{"feature_table",data->request.features},{"position_table",data->request.positions},
        {"output_project",data->request.output_project},{"projection","selected genes; original full matrix not embedded"}}.dump();
    Sha256 digest;digest.update(data->json.data(),data->json.size());data->recipe_sha=digest.finish();
    return std::shared_ptr<const SpatialImportPreview>(new SpatialImportPreview(std::move(data)));
}
std::string publish_spatial_import(const SpatialImportPreview& preview,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);const auto& data=*preview.data_;const auto& request=data.request;const auto& projection=data.projection;
    new_output(request.output_project);for(const auto& guard:data.guards)guard->verify(false,cancelled);
    auto project=Project::open_without_journal_recovery(request.input_project);auto document=project->document(request.image_id);
    validate_projection(*document,projection,request.options,cancelled);
    std::vector<Annotation> objects;objects.reserve(projection.observations.size());
    const std::map<std::string,std::string> provenance{{"omics.profile","qpx.spatial-counts/1"},
        {"omics.matrix_sha256",projection.source_sha256[0]},{"omics.features_sha256",projection.source_sha256[1]},
        {"omics.positions_sha256",projection.source_sha256[2]},{"omics.coordinate_system","level-zero pixels"},
        {"omics.import_recipe_sha256",data.recipe_sha},
        {"omics.projection","selected genes; original full matrix not embedded"}};
    for(std::size_t i=0;i<projection.observations.size();++i) {
        workflow_checkpoint(cancelled);const auto& observation=projection.observations[i];std::map<std::string,double> values;
        for(std::size_t c=0;c<projection.features.size();++c)
            values.emplace("omics.count:"+projection.features[c],static_cast<double>(projection.counts[i*projection.features.size()+c]));
        auto facts=provenance;facts.emplace("omics.observation_id",observation.id);
        Annotation object{};object.id=i+1;object.name=observation.id;object.kind=ObjectKind::Detection;
        object.geometry=Roi::points({observation.position});object.plane=ObjectPlane(-1,request.options.z,request.options.t);
        object.properties=std::make_shared<const ObjectProperties>(std::move(facts),std::move(values));objects.push_back(std::move(object));
    }
    workflow_checkpoint(cancelled);const auto created=document->append_objects(objects,{},"Import spatial count projection");document->clear_history();
    auto image_metadata=project->image_metadata(request.image_id);
    image_metadata["omics.import_recipe"]=data.json;image_metadata["omics.import_recipe:"+data.recipe_sha]=data.json;
    project->set_image_metadata(request.image_id,std::move(image_metadata));
    const auto report=nlohmann::json{{"schema","qpx.spatial-counts/1"},{"project",request.output_project},{"image_id",request.image_id},
        {"features",projection.features},{"observations",created.size()},{"object_ids",created},
        {"matrix_features",projection.matrix_features},{"matrix_entries",projection.matrix_entries},
        {"source_sha256",projection.source_sha256},{"z",request.options.z},{"t",request.options.t},
        {"projection","selected genes; original full matrix not embedded"}}.dump();
    project->save_new(request.output_project,request.options.work_bytes,1024ULL*1024*1024,cancelled,
        [&]{for(const auto& guard:data.guards)guard->verify(false,cancelled);});
    return report;
}
struct SpatialImportJob::Data {
    struct Outcome {std::shared_ptr<const SpatialImportPreview> preview;std::string report;};
    std::thread::id owner=std::this_thread::get_id();std::atomic<bool> cancelled{false};
    std::atomic<SpatialImportPhase> phase{SpatialImportPhase::Idle};std::future<Outcome> future;
    std::shared_ptr<const SpatialImportPreview> preview;std::string report,error;std::exception_ptr failure;
    void reset(SpatialImportPhase next) {cancelled.store(false);phase.store(next);preview.reset();report.clear();error.clear();failure=nullptr;}
};
SpatialImportJob::SpatialImportJob():data_(std::make_unique<Data>()) {}
SpatialImportJob::~SpatialImportJob(){cancel();if(data_->future.valid())data_->future.wait();}
bool SpatialImportJob::preview(SpatialImportRequest request) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;request.validate();data_->reset(SpatialImportPhase::Reading);
    data_->future=std::async(std::launch::async,[this,request=std::move(request)] {
        return Data::Outcome{preview_spatial_import(request,&data_->cancelled),{}};
    });return true;
}
bool SpatialImportJob::publish(std::shared_ptr<const SpatialImportPreview> preview) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;if(!preview)throw std::invalid_argument("Spatial import needs a preview");
    data_->reset(SpatialImportPhase::Publishing);
    data_->future=std::async(std::launch::async,[this,preview=std::move(preview)] {
        auto report=publish_spatial_import(*preview,&data_->cancelled);return Data::Outcome{preview,std::move(report)};
    });return true;
}
void SpatialImportJob::poll() {
    workflow_require_owner(data_->owner);
    if(!data_->future.valid() || data_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    try {auto result=data_->future.get();data_->preview=std::move(result.preview);data_->report=std::move(result.report);
        data_->phase.store(data_->report.empty()?SpatialImportPhase::Ready:SpatialImportPhase::Complete);
    }catch(const WorkflowCancelled&) {data_->failure=std::current_exception();data_->error=workflow_failure_message(data_->failure);data_->phase.store(SpatialImportPhase::Cancelled);}
    catch(...) {data_->failure=std::current_exception();data_->error=workflow_failure_message(data_->failure);data_->phase.store(SpatialImportPhase::Failed);}
}
void SpatialImportJob::wait(){workflow_require_owner(data_->owner);if(data_->future.valid())data_->future.wait();poll();}
void SpatialImportJob::cancel(){data_->cancelled.store(true);}
bool SpatialImportJob::busy() const {workflow_require_owner(data_->owner);return data_->future.valid();}
SpatialImportPhase SpatialImportJob::phase() const {workflow_require_owner(data_->owner);return data_->phase.load();}
std::shared_ptr<const SpatialImportPreview> SpatialImportJob::result() const {workflow_require_owner(data_->owner);return data_->preview;}
const std::string& SpatialImportJob::report() const {workflow_require_owner(data_->owner);return data_->report;}
const std::string& SpatialImportJob::error() const {workflow_require_owner(data_->owner);return data_->error;}
void SpatialImportJob::rethrow_error() const {workflow_require_owner(data_->owner);if(data_->failure)std::rethrow_exception(data_->failure);}
}
