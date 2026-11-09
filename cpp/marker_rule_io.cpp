#include "qupathxx/marker_rule_io.hpp"
#include "qupathxx/workflow.hpp"
#ifdef QUPATHXX_HAS_PROJECT
#include "workflow_input.hpp"
#include <nlohmann/json.hpp>
#include <set>
#include <cmath>
namespace qpx {
namespace {
using Json=nlohmann::json;
constexpr std::size_t rule_limit=1024u*1024;
void shape(const Json& v,std::initializer_list<const char*> keys){if(!v.is_object() || v.size()!=keys.size())throw std::invalid_argument("Marker JSON object fields differ from schema");for(const auto* key:keys)if(!v.contains(key))throw std::invalid_argument(std::string("Missing marker JSON field: ")+key);}
std::uint64_t index(const Json& v,std::uint64_t maximum){if(!v.is_number_integer() || (!v.is_number_unsigned() && v.get<std::int64_t>()<0))throw std::invalid_argument("Marker JSON indices/counts must be unsigned integers");const auto n=v.get<std::uint64_t>();if(n>maximum)throw std::invalid_argument("Marker JSON index/count exceeds limit");return n;}
template<std::size_t N>unsigned choice(const Json& v,const char* const(&names)[N]){const auto text=v.get<std::string>();for(unsigned i=0;i<N;++i)if(text==names[i])return i;throw std::invalid_argument("Unknown marker JSON enum: "+text);}
constexpr const char* compartments[]={"nucleus","cell","cytoplasm"};
constexpr const char* statistics[]={"mean","sum","min","max","sample_sd"};
constexpr const char* comparisons[]={"gt","ge","lt","le"};
constexpr const char* units[]={"source-sample","source-sample-sum"};
constexpr const char* operations[]={"gate","and","or"};
}
bool has_marker_rule_io(){return true;}
MarkerRule marker_rule_from_json(const std::string& text){
    if(text.empty() || text.size()>rule_limit)throw std::length_error("Marker JSON must be1..1048576 bytes");
    try {
        std::vector<std::set<std::string>> keys;
        const auto callback=[&](int depth,Json::parse_event_t event,Json& value){if(depth>16)throw std::invalid_argument("Marker JSON nesting limit");
            if(event==Json::parse_event_t::object_start)keys.emplace_back();else if(event==Json::parse_event_t::object_end)keys.pop_back();else if(event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)throw std::invalid_argument("Duplicate marker JSON key");return true;};
        const auto j=Json::parse(text,callback);const bool unmixed=j.at("schema")=="qpx.marker-rule/2";
        if(unmixed)shape(j,{"schema","name","domain","positive_class","negative_class","unknown_class","gates","nodes","unmix_record"});else{shape(j,{"schema","name","domain","positive_class","negative_class","unknown_class","gates","nodes"});if(j.at("schema")!="qpx.marker-rule/1")throw std::invalid_argument("Unsupported marker JSON schema");}
        MarkerRule p;if(unmixed){p.unmix_record=j.at("unmix_record").get<std::string>();if(p.unmix_record.empty())throw std::invalid_argument("Empty unmixed marker JSON scope");}p.name=j.at("name");p.domain=j.at("domain");p.positive_class=j.at("positive_class");p.negative_class=j.at("negative_class");p.unknown_class=j.at("unknown_class");
        const auto& gates=j.at("gates");const auto& nodes=j.at("nodes");if(!gates.is_array() || gates.empty() || gates.size()>64 || !nodes.is_array() || nodes.empty() || nodes.size()>256)throw std::invalid_argument("Marker JSON graph limits");
        for(const auto& g:gates){shape(g,{"name","channel","compartment","statistic","comparison","units","threshold","minimum_finite_samples"});MarkerGate v;v.name=g.at("name");v.channel=std::uint32_t(index(g.at("channel"),65535));v.compartment=MarkerCompartment(choice(g.at("compartment"),compartments));v.statistic=MarkerStatistic(choice(g.at("statistic"),statistics));v.comparison=MarkerComparison(choice(g.at("comparison"),comparisons));v.units=MarkerUnits(choice(g.at("units"),units));
            if(!g.at("threshold").is_number())throw std::invalid_argument("Marker threshold must be numeric");v.threshold=g.at("threshold");v.minimum_finite_samples=index(g.at("minimum_finite_samples"),9007199254740992ULL);p.gates.push_back(std::move(v));}
        for(const auto& n:nodes){MarkerNode v;v.operation=MarkerOperation(choice(n.at("operation"),operations));if(v.operation==MarkerOperation::Gate){shape(n,{"operation","gate"});v.gate=std::uint32_t(index(n.at("gate"),63));}else{shape(n,{"operation","inputs"});const auto& inputs=n.at("inputs");if(!inputs.is_array() || inputs.size()>64)throw std::invalid_argument("Marker JSON node input limit");for(const auto& i:inputs)v.inputs.push_back(std::uint32_t(index(i,255)));}p.nodes.push_back(std::move(v));}
        p.validate();return p;
    }catch(const Json::exception& e){throw std::invalid_argument(std::string("Invalid marker JSON: ")+e.what());}
}
std::string marker_rule_to_json(const MarkerRule& p){p.validate();Json gates=Json::array(),nodes=Json::array();for(const auto& g:p.gates)gates.push_back({{"name",g.name},{"channel",g.channel},{"compartment",compartments[int(g.compartment)]},{"statistic",statistics[int(g.statistic)]},{"comparison",comparisons[int(g.comparison)]},{"units",units[int(g.units)]},{"threshold",g.threshold},{"minimum_finite_samples",g.minimum_finite_samples}});
    for(const auto& n:p.nodes){Json value={{"operation",operations[int(n.operation)]}};if(n.operation==MarkerOperation::Gate)value["gate"]=n.gate;else value["inputs"]=n.inputs;nodes.push_back(std::move(value));}
    Json j={{"schema",p.unmix_record.empty()?"qpx.marker-rule/1":"qpx.marker-rule/2"},{"name",p.name},{"domain",p.domain},{"positive_class",p.positive_class},{"negative_class",p.negative_class},{"unknown_class",p.unknown_class},{"gates",gates},{"nodes",nodes}};if(!p.unmix_record.empty())j["unmix_record"]=p.unmix_record;auto text=j.dump(2);if(text.size()>rule_limit)throw std::length_error("Marker JSON output limit");return text;
}
MarkerRule load_marker_rule_file(const std::string& path,const std::atomic<bool>* cancelled){detail::WorkflowInput input(path,rule_limit,false,cancelled);auto p=marker_rule_from_json(detail::read_workflow_input(input,cancelled));input.verify(false,cancelled);return p;}
void save_marker_rule_file(const MarkerRule& p,const std::string& path,const std::atomic<bool>* cancelled,const std::function<void()>& guard){workflow_checkpoint(cancelled);const auto text=marker_rule_to_json(p);if(guard)guard();detail::AtomicExportFile output(path,rule_limit,cancelled);output.write(text.data(),text.size());if(guard)guard();output.commit(false);}
std::string marker_result_json(const MarkerRuleRequest& request,const MarkerRuleResult& result){if(request.input_sha256()!=result.input_sha256())throw std::invalid_argument("Marker report request/result scientific inputs differ");
    const auto counts=[](const MarkerCounts& c){return Json{{"positive",c.positive},{"negative",c.negative},{"unknown",c.unknown}};};Json gates=Json::array();for(std::size_t i=0;i<result.gate_counts().size();++i)gates.push_back({{"name",request.rule().gates.at(i).name},{"counts",counts(result.gate_counts()[i])}});
    const auto& p=request.rule();Json j={{"schema","qpx.marker-result/1"},{"rule_sha256",p.sha256()},{"input_sha256",result.input_sha256()},{"result_sha256",result.sha256()},{"counts",counts(result.counts())},{"gates",gates},{"classes",{{"positive",p.positive_class},{"negative",p.negative_class},{"unknown",p.unknown_class}}},{"cohorts",{{"positive",result.cohort(MarkerTruth::Positive)},{"negative",result.cohort(MarkerTruth::Negative)},{"unknown",result.cohort(MarkerTruth::Unknown)}}}};if(!p.unmix_record.empty()){j["unmix_record"]=p.unmix_record;j["domain"]=p.domain;}return j.dump();
}
}
#else
namespace qpx {
namespace {void unsupported(){throw std::runtime_error("Marker rule file/report IO requires project/JSON support");}}
bool has_marker_rule_io(){return false;}
MarkerRule marker_rule_from_json(const std::string&){unsupported();return {};}
std::string marker_rule_to_json(const MarkerRule&){unsupported();return {};}
MarkerRule load_marker_rule_file(const std::string&,const std::atomic<bool>*){unsupported();return {};}
void save_marker_rule_file(const MarkerRule&,const std::string&,const std::atomic<bool>*,const std::function<void()>&){unsupported();}
std::string marker_result_json(const MarkerRuleRequest&,const MarkerRuleResult&){unsupported();return {};}
}
#endif
