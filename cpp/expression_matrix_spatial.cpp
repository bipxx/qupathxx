#include "qupathxx/expression_matrix_spatial.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "spatial_count_text.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace qpx {
namespace {
constexpr const char* profile=expression_matrix_point_profile;
void budget(std::size_t n){if(n<4u*1024*1024 || n>1024ull*1024*1024)throw std::invalid_argument("Spatial matrix work must be4..1024MiB");}
void charge(std::size_t& n,std::size_t count,std::size_t width,std::size_t limit) {
    if(count>limit/width)throw WorkflowCaptureLimit("Spatial matrix allocation exceeds work budget");
    workflow_capture_charge(n,count*width,limit);
}
void digest(const std::string& s){if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid spatial matrix identity");}
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double x){if(x==0)x=0;std::uint64_t bits;std::memcpy(&bits,&x,8);h.integer(bits);}
const std::string& fact(const ObjectProperties& p,const char* key) {
    const auto it=p.metadata().find(key);if(it==p.metadata().end())throw std::invalid_argument("Incomplete backed expression point");return it->second;
}
bool eligible(const Annotation& object,const std::string& source) {
    if(!object.properties)return false;
    const auto& m=object.properties->metadata();const auto it=m.find("omics.profile");
    return it!=m.end() && it->second==profile && fact(*object.properties,"omics.matrix_source_sha256")==source;
}
std::size_t block_bytes(const ExpressionSparseBlock& b) {
    return sizeof(b)+(b.features.capacity()+b.offsets.capacity()+b.observation_indices.capacity()+b.counts.capacity())*8+
        b.continuous.capacity()*sizeof(double)+b.source_sha256.capacity()+b.sha256.capacity();
}
}
struct ExpressionMatrixSpatialSnapshot::Data {
    std::shared_ptr<const ExpressionMatrixSource> source;
    std::vector<ExpressionObservation> observations;
    std::vector<std::uint64_t> indices;
    std::vector<std::size_t> order;
    CoordinateFrame frame;std::string sha;std::size_t bytes=0;
};
ExpressionMatrixSpatialSnapshot::ExpressionMatrixSpatialSnapshot(std::shared_ptr<const Data> d):data_(std::move(d)){}
ExpressionMatrixSpatialSnapshot ExpressionMatrixSpatialSnapshot::create(
    std::shared_ptr<const ExpressionMatrixSource> source,std::vector<ExpressionObservation> observations,
    CoordinateFrame frame,std::size_t work,const std::atomic<bool>* cancelled) {
    budget(work);workflow_checkpoint(cancelled);frame.validate();
    if(!source || source->metadata().domain!=ExpressionValueDomain::Counts)
        throw std::invalid_argument("Spatial matrix snapshot requires an explicit native count source");
    if(!source->metadata().features || source->metadata().features>1000000 ||
       !source->metadata().observations || source->metadata().observations>1000000)
        throw std::invalid_argument("Spatial matrix source dimensions exceed contract");
    digest(source->metadata().source_sha256);
    if(observations.size()>source->metadata().observations)throw std::invalid_argument("Spatial rows exceed the observation axis");
    std::size_t retained=sizeof(Data)+source->payload_bytes()+frame.id.capacity()+frame.source_id.capacity()+128;
    charge(retained,observations.capacity(),sizeof(ExpressionObservation),work);
    charge(retained,observations.size(),sizeof(std::uint64_t)+sizeof(std::size_t),work);
    for(const auto& row:observations) {
        detail::spatial_identifier(row.id);workflow_capture_charge(retained,row.id.capacity(),work);
        if(!std::isfinite(row.position.x) || !std::isfinite(row.position.y))throw std::invalid_argument("Nonfinite spatial matrix coordinate");
    }
    std::size_t preparation=retained;
    charge(preparation,observations.size(),sizeof(std::uint64_t)+sizeof(std::string)+256,work);
    // Reject ambiguous joins before lookup. Sorting indices avoids copying IDs
    // into a second tree and preserves the caller's explicit row order.
    std::vector<std::size_t> order(observations.size());std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](auto a,auto b){return observations[a].id<observations[b].id;});
    for(std::size_t i=1;i<order.size();++i)if(observations[order[i-1]].id==observations[order[i]].id)throw std::invalid_argument("Duplicate spatial observation ID");
    {
        std::vector<std::uint64_t> objects;objects.reserve(observations.size());
        for(const auto& row:observations)if(row.object_id)objects.push_back(row.object_id);
        std::sort(objects.begin(),objects.end());
        if(std::adjacent_find(objects.begin(),objects.end())!=objects.end())throw std::invalid_argument("Duplicate spatial object ID");
    }
    std::vector<std::string> ids;ids.reserve(observations.size());for(const auto& row:observations)ids.push_back(row.id);
    if(work-retained<4u*1024*1024)throw WorkflowCaptureLimit("Spatial join leaves no indexed read budget");
    auto indices=source->observation_indices(ids,work-retained,cancelled);
    if(indices.size()!=observations.size())throw std::invalid_argument("Spatial matrix ID join shape differs");
    std::sort(order.begin(),order.end(),[&](auto a,auto b){return indices[a]<indices[b];});
    for(std::size_t i=0;i<order.size();++i)
        if(indices[order[i]]>=source->metadata().observations || (i && indices[order[i-1]]==indices[order[i]]))throw std::invalid_argument("Spatial matrix axis join is ambiguous");
    Sha256 h;text(h,"qpx.expression-spatial-rows/1");text(h,source->metadata().source_sha256);text(h,frame.sha256());h.integer(observations.size());
    for(std::size_t i=0;i<observations.size();++i){workflow_checkpoint(cancelled);const auto& row=observations[i];h.integer(row.object_id);text(h,row.id);real(h,row.position.x);real(h,row.position.y);h.integer(indices[i]);}
    source->require_current(cancelled);
    auto d=std::make_shared<Data>();d->source=std::move(source);d->observations=std::move(observations);d->frame=std::move(frame);
    d->indices=std::move(indices);d->order=std::move(order);d->sha=h.finish();d->bytes=retained;
    return ExpressionMatrixSpatialSnapshot(std::move(d));
}
ExpressionMatrixSpatialSnapshot ExpressionMatrixSpatialSnapshot::capture(const Document& doc,
    std::shared_ptr<const ExpressionMatrixSource> source,const std::string& frame_id,ObjectPlane plane,
    std::size_t work,const std::atomic<bool>* cancelled) {
    budget(work);if(!source)throw std::invalid_argument("Spatial capture requires a matrix source");
    doc.validate_object_plane(plane);auto frame=image_coordinate_frame(doc,frame_id,plane,cancelled);const auto frame_hash=frame.sha256();
    std::size_t bytes=0;std::vector<ExpressionObservation> rows;
    for(const auto& [id,object]:doc.objects()) {
        workflow_checkpoint(cancelled);if(object.plane!=plane || !eligible(object,source->metadata().source_sha256))continue;
        const auto declared=object.properties->metadata().find("omics.coordinate_frame_id");
        if(declared!=object.properties->metadata().end() && declared->second!=frame_id)continue;
        if(fact(*object.properties,"omics.coordinate_frame_sha256")!=frame_hash)
            throw WorkflowInputChanged("Backed expression point image frame/calibration changed");
        if(object.kind!=ObjectKind::Detection || object.geometry.kind()!=RoiKind::Points)
            throw std::invalid_argument("Backed expression observation requires a point detection");
        // Charge the geometry before asking for owned paths at the external boundary.
        auto temporary=bytes;workflow_capture_charge(temporary,object.geometry.payload_bytes(),work);
        const auto paths=object.geometry.paths();
        if(paths.size()!=1 || paths[0].size()!=1)throw std::invalid_argument("Backed expression observation requires exactly one point");
        const auto& row_id=fact(*object.properties,"omics.observation_id");detail::spatial_identifier(row_id);
        workflow_capture_charge(bytes,2*(sizeof(ExpressionObservation)+256),work);
        rows.push_back({id,row_id,paths[0][0]});
    }
    if(rows.empty())throw std::invalid_argument("No backed expression observations match the source/frame/plane");
    return create(std::move(source),std::move(rows),std::move(frame),work,cancelled);
}
const std::shared_ptr<const ExpressionMatrixSource>& ExpressionMatrixSpatialSnapshot::source()const{return data_->source;}
const std::vector<ExpressionObservation>& ExpressionMatrixSpatialSnapshot::observations()const{return data_->observations;}
const CoordinateFrame& ExpressionMatrixSpatialSnapshot::frame()const{return data_->frame;}
CoordinateFrame ExpressionMatrixSpatialSnapshot::dataset_frame()const{auto frame=data_->frame;frame.source_id=data_->sha;return frame;}
const std::string& ExpressionMatrixSpatialSnapshot::sha256()const{return data_->sha;}
std::size_t ExpressionMatrixSpatialSnapshot::payload_bytes()const{return data_->bytes;}
ExpressionDataset ExpressionMatrixSpatialSnapshot::dataset(const std::vector<std::uint64_t>& features,
    std::size_t work,const std::atomic<bool>* cancelled)const {
    budget(work);workflow_checkpoint(cancelled);
    if(features.empty() || features.size()>4096)throw std::invalid_argument("Spatial projection requires1..4096 explicit features");
    std::size_t external=payload_bytes();charge(external,features.capacity(),16,work);
    charge(external,data_->observations.size(),8,work);
    std::size_t projection=2048+data_->frame.id.capacity()+data_->source->metadata().source_sha256.capacity();
    charge(projection,data_->observations.size(),sizeof(ExpressionObservation)+1024,work);
    charge(projection,features.size(),sizeof(std::string)+1024,work);
    if(data_->observations.size()>work/9/features.size())throw WorkflowCaptureLimit("Selected spatial projection exceeds budget");
    const auto slots=data_->observations.size()*features.size();charge(projection,slots,9,work);
    auto reserved=external;workflow_capture_charge(reserved,projection,work);
    if(work-reserved<4u*1024*1024)throw WorkflowCaptureLimit("Spatial projection leaves no indexed query budget");
    ExpressionMatrixSelection query;query.features=features;query.observations.emplace();query.observations->reserve(data_->order.size());
    for(auto row:data_->order)query.observations->push_back(data_->indices[row]);
    // The source read gets only the budget left after the retained coordinate
    // snapshot and planned dense selected-feature result, never the whole matrix.
    const auto block=data_->source->read(query,work-reserved,cancelled);
    if(block.features!=features || block.offsets.size()!=features.size()+1 || block.offsets.front()!=0 ||
       block.offsets.back()!=block.counts.size() || block.counts.size()!=block.observation_indices.size() ||
       block.domain!=ExpressionValueDomain::Counts || !block.continuous.empty() || block.source_sha256!=data_->source->metadata().source_sha256)
        throw std::invalid_argument("Spatial sparse block differs from its declared source/shape");
    const auto sparse_bytes=block_bytes(block);
    auto auxiliary=reserved;workflow_capture_charge(auxiliary,sparse_bytes,work);
    if(work-auxiliary<4u*1024*1024)throw WorkflowCaptureLimit("Spatial projection leaves no catalogue read budget");
    ExpressionDatasetInput input;input.source_id=data_->sha;
    input.coordinate_frame=data_->frame.id;input.plane=data_->frame.plane;input.observations=data_->observations;
    input.features.reserve(features.size());
    for(auto feature:features){workflow_checkpoint(cancelled);input.features.push_back(data_->source->feature_page(feature,1,work-auxiliary,cancelled).at(0).id);}
    input.counts.assign(slots,0);
    for(std::size_t f=0;f<features.size();++f) {
        if(block.offsets[f]>block.offsets[f+1] || block.offsets[f+1]>block.counts.size())throw std::invalid_argument("Spatial sparse offsets differ");
        auto at=block.offsets[f];
        for(auto row:data_->order) {
            workflow_checkpoint(cancelled);
            if(at<block.offsets[f+1] && block.observation_indices[at]==data_->indices[row])input.counts[row*features.size()+f]=block.counts[at++];
        }
        if(at!=block.offsets[f+1])throw std::invalid_argument("Spatial sparse block contains an unrequested/duplicate row");
    }
    workflow_capture_charge(external,sparse_bytes,work);
    data_->source->require_current(cancelled);
    return ExpressionDataset::create(std::move(input),work-external,cancelled);
}
std::shared_ptr<const ObjectProperties> expression_matrix_point_properties(const std::string& source,
    const std::string& id,const CoordinateFrame& frame) {
    digest(source);detail::spatial_identifier(id);frame.validate();
    if(!frame.image_level_zero || frame.unit!=CoordinateUnit::Pixel)throw std::invalid_argument("Backed points require an explicit level-zero image pixel frame");
    return std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"omics.profile",profile},
        {"omics.matrix_source_sha256",source},{"omics.observation_id",id},
        {"omics.coordinate_frame_id",frame.id},{"omics.coordinate_frame_sha256",frame.sha256()}});
}
bool expression_matrix_points_reference(const Document& doc,const std::string& source) {
    for(const auto& [id,object]:doc.objects())if(eligible(object,source))return true;
    return false;
}
}
