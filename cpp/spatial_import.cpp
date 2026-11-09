#include "qupathxx/spatial_import.hpp"
#include "spatial_import_internal.hpp"
#include "qupathxx/backed_spatial_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "workflow_input.hpp"
#include "sha256.hpp"
#include "qupathxx/raw_image.hpp"
#include "utf8_text.hpp"
#include "ascii_case.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <future>
#include <charconv>

namespace qpx {
namespace detail::spatial_import {
void new_output(const std::string& path) {
    const auto target=std::filesystem::u8path(path);
    if(std::filesystem::exists(std::filesystem::symlink_status(target)))
        throw WorkflowOutputExists("Choose a NEW spatial-count project");
    auto parent=target.parent_path();if(parent.empty())parent=".";
    if(!std::filesystem::is_directory(parent))throw std::invalid_argument("Spatial import output directory is missing");
}
void digest_argument(const std::string& value) {
    if(!value.empty() && (value.size()!=64 || value.find_first_not_of("0123456789abcdef")!=std::string::npos))throw std::invalid_argument("Invalid import catalogue identity");
}
std::string real_text(double value) {
    char buffer[64];const auto at=std::to_chars(buffer,buffer+sizeof(buffer),value);if(at.ec!=std::errc{})throw std::invalid_argument("Cannot encode original coordinate");return {buffer,at.ptr};
}
SpatialImportBaseRequest base_request(const SpatialImportRequest& request) {
    return {request.input_project,request.output_project,request.image_path,request.image_reader,
        request.image_id,request.image_series,ObjectPlane(-1,request.options.z,request.options.t)};
}
std::shared_ptr<Project> open_base(const SpatialImportBaseRequest& request,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);std::shared_ptr<Project> project;
    if(!request.input_project.empty())project=Project::open_without_journal_recovery(request.input_project);
    else {
        auto reader=request.image_reader;
        if(reader=="auto") {auto extension=std::filesystem::u8path(request.image_path).extension().u8string();detail::ascii_lower_in_place(extension);
            if(extension!=".ppm" && extension!=".tif" && extension!=".tiff")throw std::invalid_argument("Spatial import image must use the admitted PPM/TIFF profile");
            reader=extension==".ppm"?"ppm":"tiff";}
        project=std::make_shared<Project>();
        if(reader=="tiff") {TiffOpenOptions options;options.view.series=request.image_series.value_or(0);options.view.z=request.plane.z;options.view.t=request.plane.t;
            project->add_tiff_source(open_tiff(request.image_path,options,cancelled));}
        else project->add_image(request.image_path,"ppm");
    }
    const auto document=project->document(request.image_id);const auto reader=document->server()->reader_id();
    if(reader!="qpx.dense-rgb/1" && reader!="ppm" && reader!="tiff")throw std::invalid_argument("Spatial import requires an admitted native RGB/PPM/TIFF image source");
    if(reader=="tiff") {auto view=*document->server()->tiff_view();if(request.image_series)view.series=*request.image_series;
        view.z=request.plane.z;view.t=request.plane.t;document->set_tiff_view(view);}
    else if(request.image_series.value_or(0))throw std::invalid_argument("Image series applies only to TIFF");
    workflow_checkpoint(cancelled);return project;
}
void validate_positions(const Document& document,const std::vector<Point>& positions,ObjectPlane plane,
    std::size_t work,std::size_t per_object,const std::atomic<bool>* cancelled) {
    document.validate_object_plane(plane);const auto& metadata=document.metadata();
    if(!per_object || positions.size()>work/per_object)
        throw WorkflowCaptureLimit("Spatial-count object projection exceeds work budget");
    for(const auto& position:positions) {
        workflow_checkpoint(cancelled);
        if(position.x<0 || position.y<0 || position.x>=metadata.width || position.y>=metadata.height)
            throw std::invalid_argument("Spatial observation outside selected image");
    }
}
}
using namespace detail::spatial_import;
namespace {
void validate_projection(const Document& document,const SpatialCountsProjection& projection,const std::vector<Point>& positions,const SpatialCountsOptions& options,const std::atomic<bool>* cancelled) {
    std::size_t per_object=2048;for(const auto& feature:projection.features)per_object+=512+2*feature.size();
    if(positions.size()!=projection.observations.size())throw std::invalid_argument("Import coordinate mapping shape differs");
    validate_positions(document,positions,ObjectPlane(-1,options.z,options.t),options.work_bytes,per_object,cancelled);
}
}
void SpatialImportBaseRequest::validate() const {
    workflow_path_argument(input_project,true);workflow_path_argument(image_path,true);workflow_path_argument(output_project);
    if(!image_id)throw std::invalid_argument("Image ID must be positive");
    if(input_project.empty()==image_path.empty())throw std::invalid_argument("Choose exactly one saved project or image");
    if(input_project.empty() && image_id!=1)throw std::invalid_argument("A new image uses image ID1");
    if(image_reader!="auto" && image_reader!="ppm" && image_reader!="tiff")throw std::invalid_argument("Image reader must be auto, ppm or tiff");
    if(image_series && *image_series>=65536)throw std::invalid_argument("Image series exceeds65535");
    plane.validate();if(plane.c!=-1)throw std::invalid_argument("Spatial observations use an all-channel image plane");
}
void SpatialImportRequest::validate() const {
    options.validate();base_request(*this).validate();
    workflow_path_argument(matrix);workflow_path_argument(features);workflow_path_argument(positions);
    if(position_unit!=CoordinateUnit::Pixel && position_unit!=CoordinateUnit::Micrometre)throw std::invalid_argument("Unknown import coordinate unit");
    alignment.validate();digest_argument(expected_base_sha256);digest_argument(expected_feature_sha256);
}
struct SpatialImportPreview::Data {
    SpatialImportRequest request;SpatialCountsProjection projection;
    std::array<std::shared_ptr<const detail::WorkflowInput>,4> guards;
    std::string frame="image-level-zero-px",json,recipe_sha,empty;
    CoordinateFrame target;Affine2D transform;std::vector<Point> mapped;
    std::shared_ptr<ImageServer> image;
};
SpatialImportPreview::SpatialImportPreview(std::shared_ptr<const Data> data):data_(std::move(data)) {}
const SpatialImportRequest& SpatialImportPreview::request() const {return data_->request;}
const SpatialCountsProjection& SpatialImportPreview::projection() const {return data_->projection;}
const std::string& SpatialImportPreview::project_sha256() const {return data_->request.input_project.empty()?data_->empty:data_->guards[0]->sha256();}
const std::string& SpatialImportPreview::input_sha256() const {return data_->guards[0]->sha256();}
const std::string& SpatialImportPreview::coordinate_frame() const {return data_->frame;}
const std::vector<Point>& SpatialImportPreview::mapped_positions() const {return data_->mapped;}
const CoordinateFrame& SpatialImportPreview::image_frame() const {return data_->target;}
const Affine2D& SpatialImportPreview::coordinate_transform() const {return data_->transform;}
std::shared_ptr<ImageServer> SpatialImportPreview::image_server() const {return data_->image;}
const std::string& SpatialImportPreview::json() const {return data_->json;}
std::shared_ptr<const SpatialImportPreview> preview_spatial_import(const SpatialImportRequest& request,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);request.validate();new_output(request.output_project);
    auto data=std::make_shared<SpatialImportPreview::Data>();data->request=request;
    data->guards[0]=std::make_shared<detail::WorkflowInput>(request.input_project.empty()?request.image_path:request.input_project,1024ULL*1024*1024,!request.input_project.empty(),cancelled);
    if(!request.expected_base_sha256.empty() && request.expected_base_sha256!=data->guards[0]->sha256())throw WorkflowInputChanged("Selected base catalogue changed; reload it");
    const std::array<std::string,3> paths{request.matrix,request.features,request.positions};
    for(std::size_t i=0;i<paths.size();++i)data->guards[i+1]=std::make_shared<detail::WorkflowInput>(paths[i],request.options.file_bytes,false,cancelled);
    if(!request.expected_feature_sha256.empty() && request.expected_feature_sha256!=data->guards[2]->sha256())throw WorkflowInputChanged("Feature catalogue changed; reload it");
    // Store canonical locators so changing the controller's working directory
    // cannot redirect publication after preview.
    if(request.input_project.empty())data->request.image_path=data->guards[0]->path();else data->request.input_project=data->guards[0]->path();
    data->request.matrix=data->guards[1]->path();
    data->request.features=data->guards[2]->path();data->request.positions=data->guards[3]->path();
    data->request.output_project=std::filesystem::weakly_canonical(std::filesystem::u8path(request.output_project)).u8string();
    data->projection=read_spatial_counts(data->request.matrix,data->request.features,data->request.positions,request.options,cancelled);
    for(std::size_t i=0;i<paths.size();++i)if(data->projection.source_sha256[i]!=data->guards[i+1]->sha256())
        throw WorkflowInputChanged("Spatial-count input changed during preview");
    auto project=open_base(data->request,cancelled);const auto document=project->document(request.image_id);
    const auto frame_id=request.input_project.empty()?"qpx.import-image/"+data->guards[0]->sha256():project->identity()+":"+std::to_string(request.image_id);
    data->target=image_coordinate_frame(*document,frame_id,ObjectPlane(-1,request.options.z,request.options.t),cancelled);
    CoordinateFrame source;source.id="spatial-input";source.source_id=data->projection.source_sha256[2];source.unit=request.position_unit;
    data->transform=coordinate_unit_conversion(source,data->target).then(request.alignment);
    if(data->projection.observations.size()>request.options.work_bytes/(sizeof(Point)+64))throw WorkflowCaptureLimit("Import coordinate mapping exceeds work budget");
    data->mapped.reserve(data->projection.observations.size());
    for(const auto& observation:data->projection.observations) {workflow_checkpoint(cancelled);data->mapped.push_back(data->transform.map(observation.position));}
    validate_projection(*document,data->projection,data->mapped,request.options,cancelled);data->image=document->server();
    for(const auto& guard:data->guards)guard->verify(false,cancelled);
    new_output(data->request.output_project);
    data->json=nlohmann::json{{"schema","qpx.spatial-import-preview/1"},{"project_sha256",request.input_project.empty()?"":data->guards[0]->sha256()},
        {"base_sha256",data->guards[0]->sha256()},{"image_path",data->request.image_path},{"image_reader",request.image_reader},
        {"resolved_image_reader",document->server()->reader_id()},{"image_series",document->server()->tiff_view()?document->server()->tiff_view()->series:0},
        {"position_columns",request.options.position_columns},{"position_unit",request.position_unit==CoordinateUnit::Pixel?"pixel":"micrometre"},
        {"coordinate_transform",data->transform.matrix},{"alignment",request.alignment.matrix},{"image_frame_sha256",data->target.sha256()},
        {"image_id",request.image_id},{"image_width",document->metadata().width},{"image_height",document->metadata().height},
        {"features",data->projection.features},{"observations",data->projection.observations.size()},
        {"matrix_features",data->projection.matrix_features},{"matrix_entries",data->projection.matrix_entries},
        {"source_sha256",data->projection.source_sha256},{"coordinate_frame",data->frame},{"value_domain","counts"},
        {"z",request.options.z},{"t",request.options.t},{"file_bytes",request.options.file_bytes},{"work_bytes",request.options.work_bytes},
        {"max_features",request.options.max_features},{"max_observations",request.options.max_observations},{"max_entries",request.options.max_entries},
        {"input_project",data->request.input_project},{"matrix",data->request.matrix},{"feature_table",data->request.features},{"position_table",data->request.positions},
        {"output_project",data->request.output_project},{"projection","selected genes; original full matrix not embedded"}}.dump();
    detail::bounded_utf8(data->json,4096);
    Sha256 digest;digest.update(data->json.data(),data->json.size());data->recipe_sha=digest.finish();
    return std::shared_ptr<const SpatialImportPreview>(new SpatialImportPreview(std::move(data)));
}
std::string publish_spatial_import(const SpatialImportPreview& preview,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);const auto& data=*preview.data_;const auto& request=data.request;const auto& projection=data.projection;
    new_output(request.output_project);for(const auto& guard:data.guards)guard->verify(false,cancelled);
    auto project=open_base(request,cancelled);auto document=project->document(request.image_id);
    if(image_coordinate_frame(*document,data.target.id,data.target.plane,cancelled).sha256()!=data.target.sha256())throw WorkflowInputChanged("Import target image/frame changed");
    validate_projection(*document,projection,data.mapped,request.options,cancelled);
    std::vector<Annotation> objects;objects.reserve(projection.observations.size());
    const std::map<std::string,std::string> provenance{{"omics.profile","qpx.spatial-counts/1"},
        {"omics.matrix_sha256",projection.source_sha256[0]},{"omics.features_sha256",projection.source_sha256[1]},
        {"omics.positions_sha256",projection.source_sha256[2]},{"omics.coordinate_system","level-zero pixels"},
        {"omics.import_recipe_sha256",data.recipe_sha},
        {"omics.import_scope_sha256",data.recipe_sha},
        {"omics.original_coordinate_unit",request.position_unit==CoordinateUnit::Pixel?"pixel":"micrometre"},
        {"omics.projection","selected genes; original full matrix not embedded"}};
    for(std::size_t i=0;i<projection.observations.size();++i) {
        workflow_checkpoint(cancelled);const auto& observation=projection.observations[i];std::map<std::string,double> values;
        for(std::size_t c=0;c<projection.features.size();++c)
            values.emplace("omics.count:"+projection.features[c],static_cast<double>(projection.counts[i*projection.features.size()+c]));
        auto facts=provenance;facts.emplace("omics.observation_id",observation.id);
        facts.emplace("omics.original_x",real_text(observation.position.x));facts.emplace("omics.original_y",real_text(observation.position.y));
        Annotation object{};object.id=i+1;object.name=observation.id;object.kind=ObjectKind::Detection;
        object.geometry=Roi::points({data.mapped[i]});object.plane=ObjectPlane(-1,request.options.z,request.options.t);
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
        [&]{for(const auto& guard:data.guards)guard->verify(false,cancelled);
            if(image_coordinate_frame(*document,data.target.id,data.target.plane,cancelled).sha256()!=data.target.sha256())throw WorkflowInputChanged("Import image changed before publication");});
    return report;
}
SpatialImportBaseCatalogue spatial_import_base_catalogue(const std::string& path,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);detail::WorkflowInput guard(path,1024ULL*1024*1024,true,cancelled);
    auto project=Project::open_without_journal_recovery(guard.path());SpatialImportBaseCatalogue result;result.source_sha256=guard.sha256();
    std::size_t bytes=0;
    for(const auto& image:project->images()) {
        workflow_checkpoint(cancelled);workflow_capture_charge(bytes,sizeof(SpatialImportImageInfo)+256+image.name.size(),128u*1024*1024);
        result.images.push_back({image.id,image.name,image.width,image.height});
    }
    guard.verify(false,cancelled);return result;
}
struct SpatialImportJob::Data {
    struct Outcome {std::shared_ptr<const SpatialImportPreview> preview;std::string report;
        std::shared_ptr<const SpatialFeatureCatalogue> catalogue;std::shared_ptr<const SpatialImportBaseCatalogue> base;
        std::shared_ptr<const BackedSpatialImportPreview> backed;
        std::shared_ptr<const ExpressionMatrixSource> matrix;
        std::shared_ptr<const ExpressionMatrixImportResult> indexed;
        std::shared_ptr<const ExpressionFeaturePage> matrix_page;};
    std::thread::id owner=std::this_thread::get_id();std::atomic<bool> cancelled{false};
    std::atomic<SpatialImportPhase> phase{SpatialImportPhase::Idle};std::future<Outcome> future;
    std::shared_ptr<const SpatialImportPreview> preview;std::string report,error;std::exception_ptr failure;
    std::shared_ptr<const SpatialFeatureCatalogue> catalogue;std::shared_ptr<const SpatialImportBaseCatalogue> base;
    std::shared_ptr<const BackedSpatialImportPreview> backed;
    std::shared_ptr<const ExpressionMatrixSource> matrix;
    std::shared_ptr<const ExpressionMatrixImportResult> indexed;
    std::shared_ptr<const ExpressionFeaturePage> matrix_page;
    void reset(SpatialImportPhase next) {cancelled.store(false);phase.store(next);preview.reset();backed.reset();catalogue.reset();base.reset();matrix.reset();indexed.reset();matrix_page.reset();report.clear();error.clear();failure=nullptr;}
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
bool SpatialImportJob::preview(BackedSpatialImportRequest request) {
    return preview(std::move(request),std::shared_ptr<const ExpressionMatrixSource>{});
}
bool SpatialImportJob::preview(BackedSpatialImportRequest request,std::shared_ptr<const ExpressionMatrixSource> source) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;request.validate();data_->reset(SpatialImportPhase::Reading);
    data_->future=std::async(std::launch::async,[this,request=std::move(request),source=std::move(source)] {
        Data::Outcome outcome;outcome.backed=preview_backed_spatial_import(request,source,&data_->cancelled);return outcome;
    });return true;
}
bool SpatialImportJob::publish(std::shared_ptr<const BackedSpatialImportPreview> preview) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;if(!preview)throw std::invalid_argument("Backed spatial import needs a preview");
    data_->reset(SpatialImportPhase::Publishing);
    data_->future=std::async(std::launch::async,[this,preview=std::move(preview)] {
        Data::Outcome outcome;outcome.report=publish_backed_spatial_import(*preview,&data_->cancelled);outcome.backed=preview;return outcome;
    });return true;
}
bool SpatialImportJob::catalogue(const std::string& path,const SpatialCountsOptions& limits) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;workflow_path_argument(path);data_->reset(SpatialImportPhase::CatalogueReading);
    data_->future=std::async(std::launch::async,[this,path,limits] {
        auto catalogue=std::make_shared<const SpatialFeatureCatalogue>(read_spatial_feature_catalogue(path,limits.file_bytes,limits.work_bytes,limits.max_features,&data_->cancelled));
        return Data::Outcome{{},{},std::move(catalogue),{}};
    });return true;
}
bool SpatialImportJob::base_catalogue(const std::string& path) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;workflow_path_argument(path);data_->reset(SpatialImportPhase::BaseReading);
    data_->future=std::async(std::launch::async,[this,path] {
        auto base=std::make_shared<const SpatialImportBaseCatalogue>(spatial_import_base_catalogue(path,&data_->cancelled));return Data::Outcome{{},{},{},std::move(base)};
    });return true;
}
bool SpatialImportJob::open_matrix(const std::string& path,const std::string& expected,std::size_t work) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;workflow_path_argument(path);digest_argument(expected);data_->reset(SpatialImportPhase::MatrixOpening);
    data_->future=std::async(std::launch::async,[this,path,expected,work] {
        Data::Outcome result;result.matrix=open_expression_matrix(path,expected,work,2ull*1024*1024*1024,&data_->cancelled);return result;
    });return true;
}
bool SpatialImportJob::index_matrix(const std::string& matrix,const std::string& features,const std::string& observations,
    const std::string& store,ExpressionMatrixImportOptions options) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;options.validate();data_->reset(SpatialImportPhase::MatrixIndexing);
    data_->future=std::async(std::launch::async,[this,matrix,features,observations,store,options] {
        Data::Outcome result;result.indexed=std::make_shared<const ExpressionMatrixImportResult>(import_expression_matrix(matrix,features,observations,store,options,&data_->cancelled));
        result.matrix=open_expression_matrix(result.indexed->path,result.indexed->file_sha256,options.work_bytes,options.store_bytes,&data_->cancelled);return result;
    });return true;
}
bool SpatialImportJob::open_matrix(ExpressionMatrixOpenRequest request,std::size_t work) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;request.association.validate();data_->reset(SpatialImportPhase::MatrixOpening);
    data_->future=std::async(std::launch::async,[this,request=std::move(request),work] {
        Data::Outcome result;result.matrix=request.open(work,&data_->cancelled);return result;
    });return true;
}
bool SpatialImportJob::matrix_catalogue(std::shared_ptr<const ExpressionMatrixSource> source,std::string query,
    std::uint64_t offset,std::size_t count,std::size_t work) {
    workflow_require_owner(data_->owner);poll();if(busy())return false;if(!source)throw std::invalid_argument("Catalogue needs an opened matrix");data_->reset(SpatialImportPhase::MatrixCatalogueReading);
    data_->future=std::async(std::launch::async,[this,source=std::move(source),query=std::move(query),offset,count,work] {
        Data::Outcome result;result.matrix_page=std::make_shared<const ExpressionFeaturePage>(source->search_features(query,offset,count,work,&data_->cancelled));result.matrix=source;return result;
    });return true;
}
void SpatialImportJob::poll() {
    workflow_require_owner(data_->owner);
    if(!data_->future.valid() || data_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    try {auto result=data_->future.get();data_->preview=std::move(result.preview);data_->report=std::move(result.report);
        data_->catalogue=std::move(result.catalogue);data_->base=std::move(result.base);
        data_->backed=std::move(result.backed);
        data_->matrix=std::move(result.matrix);data_->indexed=std::move(result.indexed);data_->matrix_page=std::move(result.matrix_page);
        data_->phase.store(data_->matrix_page?SpatialImportPhase::MatrixCatalogueReady:data_->matrix?SpatialImportPhase::MatrixReady:data_->catalogue?SpatialImportPhase::CatalogueReady:data_->base?SpatialImportPhase::BaseReady:data_->report.empty()?SpatialImportPhase::Ready:SpatialImportPhase::Complete);
    }catch(const WorkflowCancelled&) {data_->failure=std::current_exception();data_->error=workflow_failure_message(data_->failure);data_->phase.store(SpatialImportPhase::Cancelled);}
    catch(...) {data_->failure=std::current_exception();data_->error=workflow_failure_message(data_->failure);data_->phase.store(SpatialImportPhase::Failed);}
}
void SpatialImportJob::wait(){workflow_require_owner(data_->owner);if(data_->future.valid())data_->future.wait();poll();}
void SpatialImportJob::cancel(){data_->cancelled.store(true);}
bool SpatialImportJob::busy() const {workflow_require_owner(data_->owner);return data_->future.valid();}
SpatialImportPhase SpatialImportJob::phase() const {workflow_require_owner(data_->owner);return data_->phase.load();}
std::shared_ptr<const SpatialImportPreview> SpatialImportJob::result() const {workflow_require_owner(data_->owner);return data_->preview;}
std::shared_ptr<const BackedSpatialImportPreview> SpatialImportJob::backed_result() const {workflow_require_owner(data_->owner);return data_->backed;}
std::shared_ptr<const ExpressionMatrixSource> SpatialImportJob::matrix_source() const {workflow_require_owner(data_->owner);return data_->matrix;}
std::shared_ptr<const ExpressionMatrixImportResult> SpatialImportJob::matrix_index_result() const {workflow_require_owner(data_->owner);return data_->indexed;}
std::shared_ptr<const ExpressionFeaturePage> SpatialImportJob::matrix_catalogue_result() const {workflow_require_owner(data_->owner);return data_->matrix_page;}
std::shared_ptr<const SpatialFeatureCatalogue> SpatialImportJob::catalogue_result() const {workflow_require_owner(data_->owner);return data_->catalogue;}
std::shared_ptr<const SpatialImportBaseCatalogue> SpatialImportJob::base_result() const {workflow_require_owner(data_->owner);return data_->base;}
const std::string& SpatialImportJob::report() const {workflow_require_owner(data_->owner);return data_->report;}
const std::string& SpatialImportJob::error() const {workflow_require_owner(data_->owner);return data_->error;}
void SpatialImportJob::rethrow_error() const {workflow_require_owner(data_->owner);if(data_->failure)std::rethrow_exception(data_->failure);}
}
