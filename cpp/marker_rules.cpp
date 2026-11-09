#include "marker_rules_internal.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/workflow.hpp"
#include "image_data_properties_codec.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace qpx {
namespace {
template<class T>bool enumeration(T value,int maximum){return int(value)>=0 && int(value)<=maximum;}
void valid_text(const std::string& value,std::size_t maximum){detail::bounded_utf8(value,maximum);if(value.empty())throw std::invalid_argument("Marker names/classes must not be empty");}
void gate_valid(const MarkerGate& g){valid_text(g.name,96);
    if(g.channel>65535 || !enumeration(g.compartment,2) || !enumeration(g.statistic,4) || !enumeration(g.comparison,3) || !enumeration(g.units,1) || !std::isfinite(g.threshold) || !g.minimum_finite_samples || g.minimum_finite_samples>9007199254740992ULL)
        throw std::invalid_argument("Invalid marker channel/statistic/comparison/threshold/sample limit");
    if(g.units!=(g.statistic==MarkerStatistic::Sum?MarkerUnits::SourceSampleSum:MarkerUnits::SourceSample))throw std::invalid_argument("Marker threshold units do not match its statistic");
}
MarkerTruth gate_truth(const MarkerGate& g,const MarkerObservation& value){
    if(!value.value || !std::isfinite(*value.value) || value.finite_samples<g.minimum_finite_samples || (g.statistic==MarkerStatistic::SampleSD && value.finite_samples<2))return MarkerTruth::Unknown;
    bool positive=false;switch(g.comparison){case MarkerComparison::Greater:positive=*value.value>g.threshold;break;case MarkerComparison::GreaterEqual:positive=*value.value>=g.threshold;break;case MarkerComparison::Less:positive=*value.value<g.threshold;break;case MarkerComparison::LessEqual:positive=*value.value<=g.threshold;break;}
    return positive?MarkerTruth::Positive:MarkerTruth::Negative;
}
using V=ImageDataValue;
template<class T>const T& typed(const V& v){const auto* p=std::get_if<T>(&v.value);if(!p)throw std::invalid_argument("Invalid stored marker rule type");return *p;}
const V& field(const V::Map& m,const char* k){const auto p=m.find(k);if(p==m.end())throw std::invalid_argument("Incomplete stored marker rule");return p->second;}
std::int64_t integer(const V::Map& m,const char* k,std::int64_t maximum){const auto n=typed<std::int64_t>(field(m,k));if(n<0 || n>maximum)throw std::invalid_argument("Stored marker enum/index is out of range");return n;}
constexpr const char* rules_key="qupathxx.marker-rules/1";
const V::Map& rules(const Document& doc){static const V::Map empty;const auto p=doc.image_data_properties();if(!p)return empty;const auto at=p->values().find(rules_key);return at==p->values().end()?empty:typed<V::Map>(at->second);}
MarkerRule stored_rule(const std::string& id,const V& v){auto p=detail::decode_marker_rule(v);if(p.sha256()!=id)throw std::invalid_argument("Stored marker rule identity differs");return p;}
}
void MarkerRule::validate() const {
    valid_text(name,128);valid_text(positive_class,128);valid_text(negative_class,128);valid_text(unknown_class,128);
    if(positive_class==negative_class || positive_class==unknown_class || negative_class==unknown_class)throw std::invalid_argument("Marker positive, negative and unknown classes must be distinct");
    if((domain!="original-channel-samples" && domain!="qpx.dark-flat-corrected-samples/1") || gates.empty() || gates.size()>64 || nodes.empty() || nodes.size()>256)throw std::invalid_argument("Marker rule domain/shape unsupported");
    std::set<std::string> names;for(const auto& g:gates){gate_valid(g);if(!names.insert(g.name).second)throw std::invalid_argument("Duplicate marker name");}
    std::size_t edges=0;std::vector<bool> used_gates(gates.size()),used_nodes(nodes.size());
    for(std::size_t i=0;i<nodes.size();++i){const auto& n=nodes[i];if(!enumeration(n.operation,2))throw std::invalid_argument("Unknown marker operation");
        if(n.operation==MarkerOperation::Gate){if(!n.inputs.empty() || n.gate>=gates.size())throw std::invalid_argument("Invalid marker leaf");used_gates[n.gate]=true;}
        else {if(n.gate || n.inputs.size()<2 || n.inputs.size()>64)throw std::invalid_argument("Marker AND/OR requires2..64 earlier nodes");std::set<std::uint32_t> seen;
            for(const auto child:n.inputs)if(child>=i || !seen.insert(child).second)throw std::invalid_argument("Marker nodes require unique topological inputs");edges+=n.inputs.size();}
    }
    if(edges>1024 || std::find(used_gates.begin(),used_gates.end(),false)!=used_gates.end())throw std::invalid_argument("Marker graph exceeds edge limit or has unused gates");
    used_nodes.back()=true;for(std::size_t i=nodes.size();i-->0;)if(used_nodes[i])for(const auto child:nodes[i].inputs)used_nodes[child]=true;
    if(std::find(used_nodes.begin(),used_nodes.end(),false)!=used_nodes.end())throw std::invalid_argument("Marker graph has unreachable nodes");
}
std::string MarkerRule::sha256() const {ImageDataProperties p({{"rule",detail::encode_marker_rule(*this)}});const auto bytes=detail::encode_image_data_properties(p);Sha256 h;h.update(bytes.data(),bytes.size());return h.finish();}
std::string marker_measurement_key(const MarkerGate& g){gate_valid(g);static constexpr const char* compartments[]={"nucleus","cell","cytoplasm"};static constexpr const char* statistics[]={"mean","sum","min","max","sample_sd"};
    return std::string("fluorescence.")+compartments[int(g.compartment)]+".C"+std::to_string(g.channel)+"."+statistics[int(g.statistic)];}
MarkerTruth evaluate_marker_gate(const MarkerGate& g,MarkerObservation value){gate_valid(g);return gate_truth(g,value);}
namespace detail {
void evaluate_marker_observations(const MarkerRule& rule,const MarkerObservation* values,MarkerTruth* gates,std::vector<MarkerTruth>& work){
    for(std::size_t i=0;i<rule.gates.size();++i)gates[i]=gate_truth(rule.gates[i],values[i]);work.resize(rule.nodes.size());
    for(std::size_t i=0;i<rule.nodes.size();++i){const auto& n=rule.nodes[i];if(n.operation==MarkerOperation::Gate){work[i]=gates[n.gate];continue;}
        bool unknown=false;auto result=n.operation==MarkerOperation::And?MarkerTruth::Positive:MarkerTruth::Negative;
        for(const auto child:n.inputs){const auto value=work[child];if((n.operation==MarkerOperation::And && value==MarkerTruth::Negative) || (n.operation==MarkerOperation::Or && value==MarkerTruth::Positive)){result=value;unknown=false;break;}if(value==MarkerTruth::Unknown)unknown=true;}
        work[i]=unknown?MarkerTruth::Unknown:result;
    }
}
V encode_marker_rule(const MarkerRule& rule){rule.validate();V::List gates,nodes;
    for(const auto& g:rule.gates)gates.push_back(V{V::Map{{"name",V{g.name}},{"channel",V{std::int64_t(g.channel)}},{"compartment",V{std::int64_t(g.compartment)}},{"statistic",V{std::int64_t(g.statistic)}},{"comparison",V{std::int64_t(g.comparison)}},{"units",V{std::int64_t(g.units)}},{"threshold",V{g.threshold}},{"minimum_finite_samples",V{std::int64_t(g.minimum_finite_samples)}}}});
    for(const auto& n:rule.nodes){V::List inputs;for(const auto i:n.inputs)inputs.push_back(V{std::int64_t(i)});nodes.push_back(V{V::Map{{"operation",V{std::int64_t(n.operation)}},{"gate",V{std::int64_t(n.gate)}},{"inputs",V{std::move(inputs)}}}});}
    return V{V::Map{{"schema",V{std::string("qpx.marker-rule/1")}},{"name",V{rule.name}},{"domain",V{rule.domain}},{"positive_class",V{rule.positive_class}},{"negative_class",V{rule.negative_class}},{"unknown_class",V{rule.unknown_class}},{"gates",V{std::move(gates)}},{"nodes",V{std::move(nodes)}}}};
}
MarkerRule decode_marker_rule(const V& value){const auto& m=typed<V::Map>(value);if(m.size()!=8 || typed<std::string>(field(m,"schema"))!="qpx.marker-rule/1")throw std::invalid_argument("Unknown stored marker rule schema");
    MarkerRule p;p.name=typed<std::string>(field(m,"name"));p.domain=typed<std::string>(field(m,"domain"));p.positive_class=typed<std::string>(field(m,"positive_class"));p.negative_class=typed<std::string>(field(m,"negative_class"));p.unknown_class=typed<std::string>(field(m,"unknown_class"));
    const auto& gates=typed<V::List>(field(m,"gates"));const auto& nodes=typed<V::List>(field(m,"nodes"));if(gates.size()>64 || nodes.size()>256)throw std::invalid_argument("Stored marker graph limit");
    for(const auto& v:gates){const auto& g=typed<V::Map>(v);if(g.size()!=8)throw std::invalid_argument("Stored marker gate shape");MarkerGate gate;gate.name=typed<std::string>(field(g,"name"));gate.channel=std::uint32_t(integer(g,"channel",65535));gate.compartment=MarkerCompartment(integer(g,"compartment",2));gate.statistic=MarkerStatistic(integer(g,"statistic",4));gate.comparison=MarkerComparison(integer(g,"comparison",3));gate.units=MarkerUnits(integer(g,"units",1));gate.threshold=typed<double>(field(g,"threshold"));gate.minimum_finite_samples=std::uint64_t(integer(g,"minimum_finite_samples",9007199254740992LL));p.gates.push_back(std::move(gate));}
    for(const auto& v:nodes){const auto& n=typed<V::Map>(v);if(n.size()!=3)throw std::invalid_argument("Stored marker node shape");MarkerNode node;node.operation=MarkerOperation(integer(n,"operation",2));node.gate=std::uint32_t(integer(n,"gate",63));const auto& inputs=typed<V::List>(field(n,"inputs"));if(inputs.size()>64)throw std::invalid_argument("Stored marker node input limit");for(const auto& input:inputs){const auto i=typed<std::int64_t>(input);if(i<0 || i>255)throw std::invalid_argument("Stored marker node index");node.inputs.push_back(std::uint32_t(i));}p.nodes.push_back(std::move(node));}p.validate();return p;
}
}
MarkerDecision evaluate_marker_rule(const MarkerRule& rule,const std::vector<MarkerObservation>& values){rule.validate();if(values.size()!=rule.gates.size())throw std::invalid_argument("Marker observation count differs from gate count");MarkerDecision result;result.gates.resize(values.size());std::vector<MarkerTruth> work;detail::evaluate_marker_observations(rule,values.data(),result.gates.data(),work);result.phenotype=work.back();return result;}
std::string keep_marker_rule(Document& doc,const MarkerRule& rule){const auto id=rule.sha256();auto all=rules(doc);if(all.count(id))throw WorkflowOutputExists("Marker rule already retained");all.emplace(id,detail::encode_marker_rule(rule));auto values=doc.image_data_properties()?doc.image_data_properties()->values():V::Map{};values[rules_key]=V{std::move(all)};doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));return id;}
std::vector<std::string> marker_rule_ids(const Document& doc){std::vector<std::string> ids;for(const auto& [id,value]:rules(doc)){stored_rule(id,value);ids.push_back(id);}return ids;}
MarkerRule load_marker_rule(const Document& doc,const std::string& id){const auto& all=rules(doc);const auto at=all.find(id);if(at==all.end())throw std::out_of_range("Marker rule not found");return stored_rule(id,at->second);}
void remove_marker_rule(Document& doc,const std::string& id){auto all=rules(doc);const auto at=all.find(id);if(at==all.end())throw std::out_of_range("Marker rule not found");stored_rule(id,at->second);all.erase(at);auto values=doc.image_data_properties()->values();if(all.empty())values.erase(rules_key);else values[rules_key]=V{std::move(all)};doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));}
}
