#include "marker_rules_internal.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
namespace qpx {
namespace {
constexpr std::size_t workspace=3u*1024*1024;
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double v){std::uint64_t bits;std::memcpy(&bits,&v,sizeof(bits));h.integer(bits);}
void truth_valid(MarkerTruth t){if(int(t)<0 || int(t)>2)throw std::invalid_argument("Invalid marker truth value");}
void count(MarkerCounts& c,MarkerTruth t){if(t==MarkerTruth::Positive)++c.positive;else if(t==MarkerTruth::Negative)++c.negative;else ++c.unknown;}
std::size_t rule_bytes(const MarkerRule& rule){auto n=sizeof(rule)+rule.gates.capacity()*sizeof(MarkerGate)+rule.nodes.capacity()*sizeof(MarkerNode);
    for(const auto* s:{&rule.name,&rule.domain,&rule.positive_class,&rule.negative_class,&rule.unknown_class})n+=s->capacity()+1;
    for(const auto& g:rule.gates)n+=g.name.capacity()+1;for(const auto& node:rule.nodes)n+=node.inputs.capacity()*sizeof(std::uint32_t);return n;}
struct Field {std::string value,finite;};
std::vector<Field> fields_for(const MarkerRule& rule){std::vector<Field> fields;for(const auto& g:rule.gates){auto key=marker_measurement_key(g);fields.push_back({key,key.substr(0,key.rfind('.')+1)+"finite_samples"});}return fields;}
void begin_input(Sha256& hash,const std::string& program,std::size_t size){text(hash,"qpx.marker-input/1");text(hash,program);hash.integer(size);}
void hash_observation(Sha256& hash,const MarkerObservation& value){hash.integer(value.finite_samples);hash.integer(bool(value.value));if(value.value)real(hash,*value.value);}
const std::string& class_for(const MarkerRule& rule,MarkerTruth truth){return truth==MarkerTruth::Positive?rule.positive_class:truth==MarkerTruth::Negative?rule.negative_class:rule.unknown_class;}

MarkerObservation observation(const ObjectProperties& p,const Field& field){
    const auto& measurements=p.measurements();const auto finite=measurements.find(field.finite),value=measurements.find(field.value);
    if(finite==measurements.end() || value==measurements.end() || !std::isfinite(finite->second) || finite->second<0 || finite->second>9007199254740992. || std::floor(finite->second)!=finite->second)return {};
    return {value->second,std::uint64_t(finite->second)};
}
}
struct MarkerRuleRequest::Data {
    struct Row {std::uint64_t id=0,revision=0;};
    std::thread::id owner=std::this_thread::get_id();
    RawAnalysisRequest authority;
    MarkerRule rule;
    std::vector<Row> rows;std::vector<MarkerObservation> values;
    std::string input,program_sha;
    std::size_t bytes=0,work_bytes=0;
    Data(const Document& doc,MarkerRule p,std::size_t work):authority(doc,RawRegionRequest{}),rule(std::move(p)),work_bytes(work){}
};
MarkerRuleRequest::MarkerRuleRequest(const Document& doc,const std::vector<std::uint64_t>& ids,MarkerRule rule,std::size_t work,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);rule.validate();if(work<4u*1024*1024 || work>512u*1024*1024 || ids.size()>100000)throw std::invalid_argument("Marker workflow requires <=100000 IDs and4..512MiB work");
    auto data=std::make_shared<Data>(doc,std::move(rule),work);auto selected=ids;std::sort(selected.begin(),selected.end());
    if(std::adjacent_find(selected.begin(),selected.end())!=selected.end())throw std::invalid_argument("Duplicate marker cohort ID");
    std::size_t charged=workspace;workflow_capture_charge(charged,sizeof(Data)+65536+rule_bytes(data->rule),work);
    workflow_capture_charge(charged,selected.size()*(sizeof(Data::Row)+sizeof(std::uint64_t)+data->rule.gates.size()*sizeof(MarkerObservation)),work);
    data->rows.reserve(selected.size());data->values.reserve(selected.size()*data->rule.gates.size());
    data->program_sha=data->rule.sha256();Sha256 input;begin_input(input,data->program_sha,selected.size());
    const auto fields=fields_for(data->rule);
    for(const auto id:selected){workflow_checkpoint(cancelled);const auto at=doc.objects().find(id);if(at==doc.objects().end())throw std::out_of_range("Marker cohort object not found");const auto& cell=at->second;
        auto temporary=charged;workflow_capture_charge(temporary,2*(cell.geometry.payload_bytes()+(cell.nucleus?cell.nucleus->payload_bytes():0))+4096,work);
        if(!fluorescence_cell_observations_intact(cell) || cell.properties->metadata().at("fluorescence.measurement_domain")!=data->rule.domain)throw std::invalid_argument("Marker rules require intact fluorescence observations in the selected domain");
        data->rows.push_back({id,cell.revision});input.integer(id);text(input,cell.properties->metadata().at("fluorescence.object_sha256"));
        for(const auto& field:fields){auto value=observation(*cell.properties,field);data->values.push_back(value);hash_observation(input,value);}
    }
    data->input=input.finish();data->bytes=charged-workspace;data_=std::move(data);
}
bool MarkerRuleRequest::matches(const Document& doc) const {workflow_require_owner(data_->owner);if(!data_->authority.matches(doc))return false;
    for(const auto& row:data_->rows){const auto at=doc.objects().find(row.id);if(at==doc.objects().end() || at->second.revision!=row.revision)return false;}return true;}
const MarkerRule& MarkerRuleRequest::rule() const{return data_->rule;}
const std::string& MarkerRuleRequest::input_sha256() const{return data_->input;}
std::size_t MarkerRuleRequest::rows() const{return data_->rows.size();}
std::size_t MarkerRuleRequest::payload_bytes() const{return data_->bytes;}
const std::string& MarkerRuleResult::input_sha256() const{return snapshot_->input;}
std::size_t MarkerRuleResult::payload_bytes() const{return sizeof(*this)+snapshot_->bytes+rows_.capacity()*sizeof(MarkerRow)+gate_states_.capacity()*sizeof(MarkerTruth)+gate_counts_.capacity()*sizeof(MarkerCounts)+sha_.capacity()+1;}
std::vector<std::uint64_t> MarkerRuleResult::cohort(MarkerTruth truth) const {truth_valid(truth);std::vector<std::uint64_t> ids;for(const auto& row:rows_)if(row.phenotype==truth)ids.push_back(row.object_id);return ids;}
std::vector<std::uint64_t> MarkerRuleResult::gate_cohort(std::size_t gate,MarkerTruth truth) const {truth_valid(truth);if(gate>=gate_counts_.size())throw std::out_of_range("Marker gate index");std::vector<std::uint64_t> ids;
    for(std::size_t i=0;i<rows_.size();++i)if(gate_states_[i*gate_counts_.size()+gate]==truth)ids.push_back(rows_[i].object_id);return ids;}
struct MarkerRuleKernel {
    static std::shared_ptr<const MarkerRuleResult> preview(const MarkerRuleRequest& request,const std::atomic<bool>* cancelled){
        workflow_checkpoint(cancelled);const auto& data=*request.data_;const auto n=data.rows.size(),g=data.rule.gates.size();std::size_t charged=workspace+data.bytes;
        workflow_capture_charge(charged,sizeof(MarkerRuleResult)+n*(sizeof(MarkerRow)+g*sizeof(MarkerTruth))+g*sizeof(MarkerCounts)+data.rule.nodes.size()*sizeof(MarkerTruth)+256,data.work_bytes);
        auto out=std::shared_ptr<MarkerRuleResult>(new MarkerRuleResult());out->snapshot_=request.data_;out->rows_.reserve(n);out->gate_states_.resize(n*g);out->gate_counts_.resize(g);std::vector<MarkerTruth> scratch;
        Sha256 hash;text(hash,"qpx.marker-result/1");text(hash,data.input);text(hash,data.program_sha);
        for(std::size_t i=0;i<n;++i){workflow_checkpoint(cancelled);auto* gates=out->gate_states_.data()+i*g;detail::evaluate_marker_observations(data.rule,data.values.data()+i*g,gates,scratch);
            const auto root=scratch.back();out->rows_.push_back({data.rows[i].id,root});count(out->counts_,root);hash.integer(data.rows[i].id);hash.integer(std::uint64_t(root));for(std::size_t j=0;j<g;++j){count(out->gate_counts_[j],gates[j]);hash.integer(std::uint64_t(gates[j]));}}
        const auto counts=[&](const MarkerCounts& c){hash.integer(c.positive);hash.integer(c.negative);hash.integer(c.unknown);};counts(out->counts_);for(const auto& c:out->gate_counts_)counts(c);out->sha_=hash.finish();return out;
    }
    static bool verify(const Document& doc,const MarkerRuleResult& result,const std::atomic<bool>* cancelled){
        const auto& data=*result.snapshot_;workflow_require_owner(data.owner);workflow_checkpoint(cancelled);if(!data.authority.matches(doc))return false;
        const auto fields=fields_for(data.rule);Sha256 input;begin_input(input,data.program_sha,data.rows.size());
        for(const auto& row:result.rows_){workflow_checkpoint(cancelled);const auto at=doc.objects().find(row.object_id);if(at==doc.objects().end())return false;const auto& cell=at->second;
            auto charged=workspace+result.payload_bytes();workflow_capture_charge(charged,2*(cell.geometry.payload_bytes()+(cell.nucleus?cell.nucleus->payload_bytes():0))+4096,data.work_bytes);
            if(cell.class_name!=class_for(data.rule,row.phenotype) || !fluorescence_cell_observations_intact(cell) || cell.properties->metadata().at("fluorescence.measurement_domain")!=data.rule.domain)return false;
            input.integer(row.object_id);text(input,cell.properties->metadata().at("fluorescence.object_sha256"));for(const auto& field:fields)hash_observation(input,observation(*cell.properties,field));
        }return input.finish()==data.input;
    }
    static std::vector<std::uint64_t> apply(Document& doc,const MarkerRuleRequest& request,const MarkerRuleResult& result,const std::atomic<bool>* cancelled){
        workflow_checkpoint(cancelled);if(result.snapshot_!=request.data_ || !request.matches(doc))throw WorkflowInputChanged("Marker preview is stale or belongs to another capture");
        const auto& data=*request.data_;std::size_t charged=workspace+result.payload_bytes();std::vector<std::pair<std::uint64_t,std::string>> assignments;std::vector<std::uint64_t> changed;
        for(const auto& row:result.rows_){const auto& name=class_for(data.rule,row.phenotype);
            workflow_capture_charge(charged,3*(sizeof(std::pair<std::uint64_t,std::string>)+name.size()+doc.objects().at(row.object_id).class_name.size()+128),data.work_bytes);}
        assignments.reserve(result.rows_.size());changed.reserve(result.rows_.size());
        for(const auto& row:result.rows_){workflow_checkpoint(cancelled);const auto& name=class_for(data.rule,row.phenotype);
            const auto& cell=doc.objects().at(row.object_id);if(cell.class_name!=name || cell.class_probability() || cell.color_rgb())changed.push_back(row.object_id);assignments.emplace_back(row.object_id,name);}
        workflow_checkpoint(cancelled);if(!request.matches(doc))throw WorkflowInputChanged("Marker cohort changed before publication");doc.apply_classifications(assignments,"Apply marker phenotype rule");return changed;
    }
};
bool verify_marker_rule_publication(const Document& doc,const MarkerRuleResult& result,const std::atomic<bool>* cancelled){return MarkerRuleKernel::verify(doc,result,cancelled);}
std::vector<std::uint64_t> marker_cell_ids(const Document& doc,const std::atomic<bool>* cancelled){return marker_cell_ids_for_domain(doc,"original-channel-samples",cancelled);}
std::vector<std::uint64_t> marker_cell_ids_for_domain(const Document& doc,const std::string& domain,const std::atomic<bool>* cancelled){
    if(domain!="original-channel-samples" && domain!="qpx.dark-flat-corrected-samples/1")throw std::invalid_argument("Unsupported marker cohort domain");std::vector<std::uint64_t> ids;
    for(const auto& [id,cell]:doc.objects()){workflow_checkpoint(cancelled);if(cell.kind!=ObjectKind::Cell || !cell.properties)continue;const auto& meta=cell.properties->metadata();const auto p=meta.find("fluorescence.profile");
        if((p==meta.end() || p->second.rfind("qpx.fluorescence-cell-publication/",0)!=0) && !meta.count("fluorescence.object_sha256"))continue;
        const auto value=meta.find("fluorescence.measurement_domain");
        if(p!=meta.end() && value!=meta.end()){
            const bool known=(p->second=="qpx.fluorescence-cell-publication/1" && value->second=="original-channel-samples") || (p->second=="qpx.fluorescence-cell-publication/2" && value->second=="qpx.dark-flat-corrected-samples/1");
            if(known && value->second!=domain)continue;
        }
        if(ids.size()==100000)throw std::length_error("Marker cohort exceeds100000 cells");ids.push_back(id);
    }return ids;
}
std::shared_ptr<const MarkerRuleResult> preview_marker_rule(const MarkerRuleRequest& request,const std::atomic<bool>* cancelled){return MarkerRuleKernel::preview(request,cancelled);}
std::vector<std::uint64_t> apply_marker_rule(Document& doc,const MarkerRuleRequest& request,const MarkerRuleResult& result,const std::atomic<bool>* cancelled){return MarkerRuleKernel::apply(doc,request,result,cancelled);}
}
