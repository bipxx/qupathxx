#include "qupathxx/backed_spatial_import.hpp"
#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "spatial_import_internal.hpp"
#include "workflow_input.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include "matrix_format_registry.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>

namespace qpx {
namespace {
using namespace detail::spatial_import;
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double v){if(v==0)v=0;std::uint64_t bits;std::memcpy(&bits,&v,8);h.integer(bits);}
bool retained(const Project& project,std::uint64_t image,const std::string& id) {
    const auto ids=expression_matrix_ids(project,image);return std::find(ids.begin(),ids.end(),id)!=ids.end();
}
void new_scope(const Document& doc,const std::string& matrix,const CoordinateFrame& frame) {
    for(const auto& [id,object]:doc.objects()) {
        if(object.plane!=frame.plane || !object.properties)continue;
        const auto& m=object.properties->metadata();const auto p=m.find("omics.profile");
        const auto source=m.find("omics.matrix_source_sha256"),scope=m.find("omics.coordinate_frame_id");
        if(p!=m.end() && p->second==expression_matrix_point_profile && source!=m.end() && source->second==matrix && scope!=m.end() && scope->second==frame.id)
            throw WorkflowOutputExists("This matrix/coordinate import is already present");
    }
}
}
void BackedSpatialImportRequest::validate()const {
    base.validate();position_options.validate();workflow_path_argument(store);if(!positions.empty())workflow_path_argument(positions);
    if(position_options.work_bytes<8u*1024*1024 || !store_bytes || store_bytes>64ull*1024*1024*1024)
        throw std::invalid_argument("Invalid backed import work/store bounds");
    if(position_unit!=CoordinateUnit::Pixel && position_unit!=CoordinateUnit::Micrometre)throw std::invalid_argument("Unknown import coordinate unit");
    alignment.validate();digest_argument(expected_base_sha256);digest_argument(expected_store_sha256);
}
struct BackedSpatialImportPreview::Data {
    BackedSpatialImportRequest request;
    std::shared_ptr<const detail::WorkflowInput> base,positions;
    SpatialPositionTable table;std::optional<ExpressionMatrixSpatialSnapshot> rows;
    CoordinateFrame target;Affine2D transform;std::vector<Point> mapped;
    std::shared_ptr<ImageServer> image;std::string recipe,json;bool effective_owned=false;
};
BackedSpatialImportPreview::BackedSpatialImportPreview(std::shared_ptr<const Data> d):data_(std::move(d)){}
const BackedSpatialImportRequest& BackedSpatialImportPreview::request()const{return data_->request;}
const ExpressionMatrixSpatialSnapshot& BackedSpatialImportPreview::spatial()const{return *data_->rows;}
const CoordinateFrame& BackedSpatialImportPreview::image_frame()const{return data_->target;}
const Affine2D& BackedSpatialImportPreview::coordinate_transform()const{return data_->transform;}
const std::vector<Point>& BackedSpatialImportPreview::mapped_positions()const{return data_->mapped;}
const std::string& BackedSpatialImportPreview::position_sha256()const{return data_->table.source_sha256;}
const std::string& BackedSpatialImportPreview::input_sha256()const{return data_->base->sha256();}
const std::string& BackedSpatialImportPreview::recipe_sha256()const{return data_->recipe;}
const std::string& BackedSpatialImportPreview::json()const{return data_->json;}
std::shared_ptr<ImageServer> BackedSpatialImportPreview::image_server()const{return data_->image;}
std::shared_ptr<const BackedSpatialImportPreview> preview_backed_spatial_import(
    const BackedSpatialImportRequest& request,const std::atomic<bool>* cancelled) {
    return preview_backed_spatial_import(request,std::shared_ptr<const ExpressionMatrixSource>{},cancelled);
}
std::shared_ptr<const BackedSpatialImportPreview> preview_backed_spatial_import(
    const BackedSpatialImportRequest& request,std::shared_ptr<const ExpressionMatrixSource> source,const std::atomic<bool>* cancelled) {
    request.validate();workflow_checkpoint(cancelled);new_output(request.base.output_project);
    auto d=std::make_shared<BackedSpatialImportPreview::Data>();d->request=request;
    d->base=std::make_shared<detail::WorkflowInput>(request.base.input_project.empty()?request.base.image_path:request.base.input_project,
        1024ull*1024*1024,!request.base.input_project.empty(),cancelled);
    if(!request.expected_base_sha256.empty() && d->base->sha256()!=request.expected_base_sha256)throw WorkflowInputChanged("Selected import base changed");
    if(request.base.input_project.empty())d->request.base.image_path=d->base->path();else d->request.base.input_project=d->base->path();
    d->request.base.output_project=detail::export_path(request.base.output_project);
    const auto work=request.position_options.work_bytes;
    if(source) {
        source->require_current(cancelled);
        if(source->path()!=detail::export_path(request.store) ||
           (!request.expected_store_sha256.empty() && source->file_sha256()!=request.expected_store_sha256))
            throw WorkflowInputChanged("Prepared matrix differs from selected import source");
        if(std::filesystem::file_size(std::filesystem::u8path(source->path()))>request.store_bytes)throw WorkflowCaptureLimit("Prepared matrix exceeds import file budget");
    } else source=detail::open_matrix_format(detail::select_matrix_format(detail::MatrixFormat::Qmx),
        {request.store,request.expected_store_sha256,work/2,request.store_bytes},cancelled);
    d->request.store=source->path();d->request.expected_store_sha256=source->file_sha256();
    CoordinateFrame coordinate_source;
    if(request.positions.empty()){
        if(!source->metadata().spatial_frame)throw std::invalid_argument("Selected store has no embedded coordinates; select a position table");
        coordinate_source=*source->metadata().spatial_frame;if(coordinate_source.plane!=request.base.plane)throw std::invalid_argument("Embedded coordinate plane differs from selected import plane");
        const auto n=source->metadata().observations;if(n>request.position_options.max_observations)throw WorkflowCaptureLimit("Embedded coordinates exceed observation limit");
        std::size_t retained=2u*1024*1024;const auto per=sizeof(SpatialObservation)+256;if(n>work/per)throw WorkflowCaptureLimit("Embedded coordinate table exceeds work budget");workflow_capture_charge(retained,std::size_t(n)*per,work);if(work-retained<4u*1024*1024)throw WorkflowCaptureLimit("Embedded coordinates leave no indexed read budget");
        d->table.observations.reserve(std::size_t(n));for(std::uint64_t first=0;first<n;first+=4096){auto page=source->spatial_page(first,std::size_t(std::min<std::uint64_t>(4096,n-first)),work-retained,cancelled);for(auto& row:page)d->table.observations.push_back({std::move(row.id),row.position});}
        d->table.source_sha256=source->metadata().spatial_sha256;d->request.position_unit=coordinate_source.unit;
    }else{
        d->positions=std::make_shared<detail::WorkflowInput>(request.positions,request.position_options.file_bytes,false,cancelled);
        d->request.positions=d->positions->path();d->table=read_spatial_positions(d->request.positions,request.position_options,cancelled);
        if(d->table.source_sha256!=d->positions->sha256())throw WorkflowInputChanged("Position table changed during import preview");
        coordinate_source.id="explicit-position-table";coordinate_source.source_id=d->table.source_sha256;coordinate_source.unit=request.position_unit;coordinate_source.plane=request.base.plane;
    }
    auto project=open_base(d->request.base,cancelled);auto doc=project->document(request.base.image_id);
    const auto base_frame_id=request.base.input_project.empty()?"qpx.import-image/"+d->base->sha256():project->identity()+":"+std::to_string(request.base.image_id);
    d->target=image_coordinate_frame(*doc,base_frame_id,request.base.plane,cancelled);
    d->transform=coordinate_unit_conversion(coordinate_source,d->target).then(request.alignment);
    Sha256 recipe;text(recipe,request.positions.empty()?"qpx.backed-spatial-import/2":"qpx.backed-spatial-import/1");text(recipe,source->metadata().source_sha256);
    text(recipe,d->table.source_sha256);text(recipe,d->target.sha256());recipe.integer(std::uint64_t(d->request.position_unit));if(request.positions.empty())text(recipe,coordinate_source.sha256());
    for(auto v:request.alignment.matrix)real(recipe,v);d->recipe=recipe.finish();
    d->target.id="qpx.backed-coordinates/"+d->recipe;
    new_scope(*doc,source->metadata().source_sha256,d->target);
    std::size_t fixed=2u*1024*1024;
    workflow_capture_charge(fixed,d->table.observations.capacity()*sizeof(SpatialObservation),work);
    for(const auto& row:d->table.observations)workflow_capture_charge(fixed,row.id.capacity(),work);
    workflow_capture_charge(fixed,d->table.observations.size()*sizeof(Point),work);
    if(work-fixed<4u*1024*1024)throw WorkflowCaptureLimit("Backed import leaves no spatial join budget");
    std::vector<ExpressionObservation> mapped;mapped.reserve(d->table.observations.size());d->mapped.reserve(d->table.observations.size());
    for(const auto& row:d->table.observations){workflow_checkpoint(cancelled);const auto position=d->transform.map(row.position);d->mapped.push_back(position);mapped.push_back({0,row.id,position});}
    validate_positions(*doc,d->mapped,request.base.plane,work,2048,cancelled);
    d->rows=ExpressionMatrixSpatialSnapshot::create(source,std::move(mapped),d->target,work-fixed,cancelled);
    d->effective_owned=request.owned;
    if(retained(*project,request.base.image_id,source->metadata().source_sha256)) {
        const auto existing=load_expression_matrix_association(*project,request.base.image_id,source->metadata().source_sha256);
        existing.require_source(*source);d->effective_owned=d->effective_owned || existing.owned;
    }
    d->image=doc->server();d->base->verify(false,cancelled);if(d->positions)d->positions->verify(false,cancelled);source->require_current(cancelled);
    const auto& m=source->metadata();
    auto json=nlohmann::json{{"schema",request.positions.empty()?"qpx.backed-spatial-import/2":"qpx.backed-spatial-import/1"},{"base_sha256",d->base->sha256()},
        {"matrix_source_sha256",m.source_sha256},{"matrix_file_sha256",source->file_sha256()},
        {"position_sha256",d->table.source_sha256},{"recipe_sha256",d->recipe},{"coordinate_frame",d->target.id},
        {"coordinate_frame_sha256",d->target.sha256()},{"coordinate_transform",d->transform.matrix},
        {"position_columns",request.position_options.columns},{"position_unit",d->request.position_unit==CoordinateUnit::Pixel?"pixel":"micrometre"},
        {"features",m.features},{"matrix_observations",m.observations},{"matrix_entries",m.entries},
        {"imported_observations",d->mapped.size()},{"image_id",request.base.image_id},{"z",request.base.plane.z},{"t",request.base.plane.t},
        {"owned",d->effective_owned},{"store",d->request.store},{"positions",d->request.positions},
        {"input_project",d->request.base.input_project},{"image_path",d->request.base.image_path},{"output_project",d->request.base.output_project}};
    if(request.positions.empty()){json["position_source"]="embedded";json["source_coordinate_frame_sha256"]=coordinate_source.sha256();json["value_kind"]=m.domain==ExpressionValueDomain::Counts?"counts":"continuous";json["value_domain"]=m.value_domain;}
    d->json=json.dump();
    detail::bounded_utf8(d->json,4096);new_output(d->request.base.output_project);
    return std::shared_ptr<const BackedSpatialImportPreview>(new BackedSpatialImportPreview(std::move(d)));
}
std::string publish_backed_spatial_import(const BackedSpatialImportPreview& preview,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);const auto& d=*preview.data_;const auto& request=d.request;const auto& source=d.rows->source();
    auto current=[&]{d.base->verify(false,cancelled);if(d.positions)d.positions->verify(false,cancelled);source->require_current(cancelled);};
    new_output(request.base.output_project);current();auto project=open_base(request.base,cancelled);auto doc=project->document(request.base.image_id);
    if(image_coordinate_frame(*doc,d.target.id,request.base.plane,cancelled).sha256()!=d.target.sha256())throw WorkflowInputChanged("Backed import image/frame changed");
    new_scope(*doc,source->metadata().source_sha256,d.target);validate_positions(*doc,d.mapped,request.base.plane,request.position_options.work_bytes,2048,cancelled);
    const auto& id=source->metadata().source_sha256;
    if(retained(*project,request.base.image_id,id)) {
        if(request.owned)own_expression_matrix(*project,request.base.image_id,id,source,cancelled);
        else relink_expression_matrix(*project,request.base.image_id,id,source,cancelled);
    } else keep_expression_matrix(*project,request.base.image_id,source,request.owned,cancelled);
    std::vector<Annotation> points;points.reserve(d.table.observations.size());
    auto common=expression_matrix_point_properties(id,d.table.observations.front().id,d.target)->metadata();
    common["omics.import_recipe_sha256"]=d.recipe;
    common["omics.original_coordinate_unit"]=request.position_unit==CoordinateUnit::Pixel?"pixel":"micrometre";
    for(std::size_t i=0;i<d.table.observations.size();++i) {
        workflow_checkpoint(cancelled);const auto& row=d.table.observations[i];
        auto metadata=common;metadata["omics.observation_id"]=row.id;
        metadata["omics.original_x"]=real_text(row.position.x);metadata["omics.original_y"]=real_text(row.position.y);
        Annotation point{};point.id=i+1;point.name=row.id;point.kind=ObjectKind::Detection;point.plane=request.base.plane;
        point.geometry=Roi::points({d.mapped[i]});point.properties=std::make_shared<const ObjectProperties>(std::move(metadata));points.push_back(std::move(point));
    }
    const auto created=doc->append_objects(points,{},"Import backed spatial observations");doc->clear_history();
    auto metadata=project->image_metadata(request.base.image_id);metadata["omics.backed_import_recipe:"+d.recipe]=d.json;project->set_image_metadata(request.base.image_id,std::move(metadata));
    project->save_new(request.base.output_project,request.position_options.work_bytes,1024ull*1024*1024,cancelled,[&]{
        current();if(image_coordinate_frame(*doc,d.target.id,request.base.plane,cancelled).sha256()!=d.target.sha256())throw WorkflowInputChanged("Backed import frame changed before publication");
    });
    return nlohmann::json{{"schema","qpx.backed-spatial-import-result/1"},{"state","saved"},{"project",request.base.output_project},
        {"image_id",request.base.image_id},{"matrix_source_sha256",id},{"recipe_sha256",d.recipe},{"coordinate_frame",d.target.id},
        {"observations",created.size()},{"object_ids",created},{"owned",d.effective_owned}}.dump();
}
}
