#include "qupathxx/expression_dataset.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>

namespace qpx {
namespace {
constexpr const char* count_prefix="omics.count:";
void name(const std::string& value) {
    detail::bounded_utf8(value,4096);
    if(value.empty() || value.find_first_of("\r\n\t")!=std::string::npos)
        throw std::invalid_argument("Invalid expression identifier");
}
void text(Sha256& digest,const std::string& value) {digest.integer(value.size());digest.update(value.data(),value.size());}
void real(Sha256& digest,double value) {
    if(value==0)value=0;std::uint64_t bits;std::memcpy(&bits,&value,sizeof(bits));digest.integer(bits);
}
void budget_limit(std::size_t bytes) {
    if(!bytes || bytes>1024ULL*1024*1024)throw std::invalid_argument("Invalid expression work budget");
}
std::size_t slots(std::size_t rows,std::size_t columns) {
    if(columns && rows>std::numeric_limits<std::size_t>::max()/columns)
        throw WorkflowCaptureLimit("Expression shape overflows platform range");
    return rows*columns;
}
void charge_array(std::size_t& bytes,std::size_t count,std::size_t width,std::size_t limit) {
    if(count>limit/width)throw WorkflowCaptureLimit("Expression allocation exceeds work budget");
    workflow_capture_charge(bytes,count*width,limit);
}
std::string lineage(const Annotation& object,ObjectPlane plane) {
    if(object.plane!=plane || !object.properties)return {};
    const auto& metadata=object.properties->metadata();const auto profile=metadata.find("omics.profile");
    if(profile==metadata.end() || profile->second!="qpx.spatial-counts/1")return {};
    Sha256 digest;text(digest,profile->second);
    for(const auto* key:{"omics.matrix_sha256","omics.features_sha256","omics.positions_sha256"}) {
        const auto at=metadata.find(key);
        if(at==metadata.end() || at->second.size()!=64 || at->second.find_first_not_of("0123456789abcdef")!=std::string::npos)
            throw std::invalid_argument("Expression projection lacks valid source hashes");
        text(digest,at->second);
    }
    return digest.finish();
}
}
struct ExpressionDataset::Data {
    ExpressionDatasetInput input;
    std::string sha;
    std::size_t bytes=0;
};
ExpressionDataset::ExpressionDataset(std::shared_ptr<const Data> data):data_(std::move(data)) {}
ExpressionDataset ExpressionDataset::create(ExpressionDatasetInput input,std::size_t limit,const std::atomic<bool>* cancelled) {
    budget_limit(limit);workflow_checkpoint(cancelled);name(input.source_id);name(input.coordinate_frame);input.plane.validate();
    if(input.domain!=ExpressionValueDomain::Counts && input.domain!=ExpressionValueDomain::Continuous)
        throw std::invalid_argument("Invalid expression value domain");
    const auto count=slots(input.observations.size(),input.features.size());
    if(input.features.empty())throw std::invalid_argument("Expression dataset requires feature identifiers");
    if((input.domain==ExpressionValueDomain::Counts && (input.counts.size()!=count || !input.continuous.empty())) ||
       (input.domain==ExpressionValueDomain::Continuous && (input.continuous.size()!=count || !input.counts.empty())) ||
       (!input.valid.empty() && input.valid.size()!=count))
        throw std::invalid_argument("Expression shape/value/validity buffers differ");
    std::size_t bytes=sizeof(Data);
    workflow_capture_charge(bytes,input.source_id.capacity(),limit);
    workflow_capture_charge(bytes,input.coordinate_frame.capacity(),limit);
    charge_array(bytes,input.features.capacity(),sizeof(std::string),limit);
    charge_array(bytes,input.observations.capacity(),sizeof(ExpressionObservation),limit);
    charge_array(bytes,input.counts.capacity(),sizeof(std::uint64_t),limit);
    charge_array(bytes,input.continuous.capacity(),sizeof(double),limit);
    charge_array(bytes,std::max(input.valid.capacity(),count),1,limit);
    std::set<std::string> feature_ids,observation_ids;std::set<std::uint64_t> objects;
    for(const auto& feature:input.features) {
        workflow_checkpoint(cancelled);name(feature);workflow_capture_charge(bytes,feature.capacity(),limit);
        workflow_capture_charge(bytes,256+feature.size(),limit);
        if(!feature_ids.insert(feature).second)throw std::invalid_argument("Duplicate expression feature ID");
    }
    for(const auto& observation:input.observations) {
        workflow_checkpoint(cancelled);name(observation.id);workflow_capture_charge(bytes,observation.id.capacity(),limit);
        workflow_capture_charge(bytes,512+observation.id.size(),limit);
        if(!std::isfinite(observation.position.x) || !std::isfinite(observation.position.y))
            throw std::invalid_argument("Nonfinite expression position");
        if(!observation_ids.insert(observation.id).second || (observation.object_id && !objects.insert(observation.object_id).second))
            throw std::invalid_argument("Duplicate expression observation/object ID");
    }
    if(input.valid.empty())input.valid.assign(count,1);
    Sha256 digest;text(digest,"qpx.expression-dataset/1");text(digest,input.source_id);text(digest,input.coordinate_frame);
    digest.integer(static_cast<std::uint64_t>(input.domain));digest.integer(static_cast<std::uint64_t>(input.plane.c+1));
    digest.integer(input.plane.z);digest.integer(input.plane.t);digest.integer(input.observations.size());digest.integer(input.features.size());
    for(const auto& feature:input.features)text(digest,feature);
    for(const auto& observation:input.observations) {
        digest.integer(observation.object_id);text(digest,observation.id);real(digest,observation.position.x);real(digest,observation.position.y);
    }
    for(std::size_t i=0;i<count;++i) {
        if(!(i&4095))workflow_checkpoint(cancelled);
        if(input.valid[i]>1)throw std::invalid_argument("Invalid expression validity byte");
        digest.integer(input.valid[i]);
        if(input.domain==ExpressionValueDomain::Counts) {
            if(!input.valid[i])input.counts[i]=0;digest.integer(input.counts[i]);
        } else {
            if(!input.valid[i])input.continuous[i]=0;
            else if(!std::isfinite(input.continuous[i]))throw std::invalid_argument("Measured expression value must be finite");
            real(digest,input.continuous[i]);
        }
    }
    auto data=std::make_shared<Data>();data->input=std::move(input);data->sha=digest.finish();data->bytes=bytes;
    return ExpressionDataset(std::move(data));
}
std::vector<std::string> ExpressionDataset::sources(const Document& document,ObjectPlane plane,std::size_t limit,const std::atomic<bool>* cancelled) {
    budget_limit(limit);plane.validate();std::size_t bytes=0;std::set<std::string> unique;
    for(const auto& entry:document.objects()) {
        workflow_checkpoint(cancelled);const auto id=lineage(entry.second,plane);
        if(!id.empty() && !unique.count(id)) {workflow_capture_charge(bytes,256,limit);unique.insert(id);}
    }
    return {unique.begin(),unique.end()};
}
ExpressionDataset ExpressionDataset::capture(const Document& document,const ExpressionCaptureOptions& options,const std::atomic<bool>* cancelled) {
    budget_limit(options.work_bytes);name(options.coordinate_frame);options.plane.validate();workflow_checkpoint(cancelled);
    auto source=options.source_id;
    if(source.empty()) {
        const auto available=sources(document,options.plane,options.work_bytes,cancelled);
        if(available.size()!=1)throw std::invalid_argument("Choose one expression source lineage");
        source=available.front();
    }
    name(source);
    ExpressionDatasetInput input;input.source_id=source;input.coordinate_frame=options.coordinate_frame;input.plane=options.plane;
    std::size_t bytes=0;std::vector<const Annotation*> objects;std::set<std::uint64_t> requested;
    charge_array(bytes,options.object_ids.size(),128,options.work_bytes);
    for(auto id:options.object_ids)if(!id || !requested.insert(id).second)throw std::invalid_argument("Invalid expression object cohort");
    for(const auto& entry:document.objects()) {
        workflow_checkpoint(cancelled);const auto& object=entry.second;
        if(!requested.empty() && !requested.count(object.id))continue;
        if(lineage(object,options.plane)!=source)continue;
        workflow_capture_charge(bytes,sizeof(Annotation*)+512,options.work_bytes);objects.push_back(&object);
    }
    if((!requested.empty() && requested.size()!=objects.size()) || objects.empty())
        throw std::invalid_argument("Expression cohort is empty or contains an ineligible object");
    std::set<std::string> feature_set;
    if(options.features.empty()) {
        for(const auto* object:objects)for(const auto& item:object->properties->measurements()) {
            if(item.first.compare(0,std::strlen(count_prefix),count_prefix))continue;
            const auto feature=item.first.substr(std::strlen(count_prefix));name(feature);
            if(!feature_set.count(feature)) {workflow_capture_charge(bytes,256+feature.size(),options.work_bytes);feature_set.insert(feature);}
        }
        input.features.assign(feature_set.begin(),feature_set.end());
    } else {
        for(const auto& feature:options.features) {
            name(feature);workflow_capture_charge(bytes,256+feature.size(),options.work_bytes);
        }
        input.features=options.features;
    }
    const auto count=slots(objects.size(),input.features.size());
    if(count>options.work_bytes/9)throw WorkflowCaptureLimit("Expression projection exceeds work budget");
    workflow_capture_charge(bytes,count*9,options.work_bytes);input.counts.assign(count,0);input.valid.assign(count,0);
    for(std::size_t row=0;row<objects.size();++row) {
        workflow_checkpoint(cancelled);const auto& object=*objects[row];
        if(object.kind!=ObjectKind::Detection || object.geometry.kind()!=RoiKind::Points)
            throw std::invalid_argument("Expression observation must be a point detection");
        workflow_capture_charge(bytes,object.geometry.payload_bytes(),options.work_bytes);
        const auto paths=object.geometry.paths();
        if(paths.size()!=1 || paths[0].size()!=1)throw std::invalid_argument("Expression observation requires one position");
        const auto at=object.properties->metadata().find("omics.observation_id");
        if(at==object.properties->metadata().end())throw std::invalid_argument("Expression observation ID missing");
        workflow_capture_charge(bytes,at->second.size(),options.work_bytes);
        input.observations.push_back({object.id,at->second,paths[0][0]});
        for(std::size_t column=0;column<input.features.size();++column) {
            const auto found=object.properties->measurements().find(count_prefix+input.features[column]);
            if(found==object.properties->measurements().end())continue;
            const auto value=found->second;
            if(!std::isfinite(value) || value<0 || value>9007199254740992. || std::floor(value)!=value)
                throw std::invalid_argument("Imported expression count is not an exact supported integer");
            const auto i=row*input.features.size()+column;input.counts[i]=static_cast<std::uint64_t>(value);input.valid[i]=1;
        }
    }
    std::size_t temporary=0;
    charge_array(temporary,objects.capacity(),sizeof(Annotation*),options.work_bytes);
    charge_array(temporary,requested.size(),128,options.work_bytes);
    for(const auto& feature:feature_set)workflow_capture_charge(temporary,256+feature.capacity(),options.work_bytes);
    if(temporary==options.work_bytes)throw WorkflowCaptureLimit("Expression capture leaves no snapshot budget");
    return create(std::move(input),options.work_bytes-temporary,cancelled);
}
ExpressionValueDomain ExpressionDataset::domain() const {return data_->input.domain;}
const std::string& ExpressionDataset::source_id() const {return data_->input.source_id;}
const std::string& ExpressionDataset::coordinate_frame() const {return data_->input.coordinate_frame;}
ObjectPlane ExpressionDataset::plane() const {return data_->input.plane;}
const std::vector<ExpressionObservation>& ExpressionDataset::observations() const {return data_->input.observations;}
const std::vector<std::string>& ExpressionDataset::features() const {return data_->input.features;}
const std::vector<std::uint64_t>& ExpressionDataset::counts() const {return data_->input.counts;}
const std::vector<double>& ExpressionDataset::continuous() const {return data_->input.continuous;}
const std::vector<std::uint8_t>& ExpressionDataset::valid() const {return data_->input.valid;}
std::size_t ExpressionDataset::feature_index(const std::string& id) const {
    const auto at=std::find(features().begin(),features().end(),id);
    if(at==features().end())throw std::invalid_argument("Expression feature absent");
    return static_cast<std::size_t>(at-features().begin());
}
const std::string& ExpressionDataset::sha256() const {return data_->sha;}
std::size_t ExpressionDataset::payload_bytes() const {return data_->bytes;}
}
