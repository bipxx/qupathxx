#include "qupathxx/linear_unmixing_io.hpp"
#include "qupathxx/workflow.hpp"
#ifdef QUPATHXX_HAS_PROJECT
#include "workflow_input.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace qpx {
namespace {
using J=nlohmann::json;using V=ImageDataValue;
constexpr std::size_t byte_limit=1024u*1024;
V native(const J& j,bool floating=false){
    if(j.is_object()){V::Map m;for(auto it=j.begin();it!=j.end();++it)m.emplace(it.key(),native(it.value(),it.key()=="coefficients" || it.key()=="maximum_condition"));return V{std::move(m)};}
    if(j.is_array()){if(j.size()>4096)throw std::invalid_argument("Mixing JSON array limit");V::List list;list.reserve(j.size());for(const auto& child:j)list.push_back(native(child,floating));return V{std::move(list)};}
    if(j.is_string())return V{j.get<std::string>()};
    if(j.is_number()){if(floating){if((j.is_number_unsigned() && j.get<std::uint64_t>()>9007199254740992ULL) || (j.is_number_integer() && !j.is_number_unsigned() && (j.get<std::int64_t>()>9007199254740992LL || j.get<std::int64_t>()< -9007199254740992LL)))throw std::invalid_argument("Mixing JSON integer coefficient exceeds exact binary64 range; supply an explicit floating value");return V{j.get<double>()};}
        if(!j.is_number_integer() || (j.is_number_unsigned() && j.get<std::uint64_t>()>std::uint64_t(INT64_MAX)))throw std::invalid_argument("Mixing JSON channel index must be an integer");return V{j.get<std::int64_t>()};}
    if(j.is_boolean())return V{j.get<bool>()};return V{};
}
J json(const V& v){if(const auto* x=std::get_if<std::string>(&v.value))return *x;if(const auto* x=std::get_if<std::int64_t>(&v.value))return *x;if(const auto* x=std::get_if<double>(&v.value))return *x;if(const auto* x=std::get_if<V::List>(&v.value)){auto j=J::array();for(const auto& child:*x)j.push_back(json(child));return j;}if(const auto* x=std::get_if<V::Map>(&v.value)){auto j=J::object();for(const auto& [key,value]:*x)j[key]=json(value);return j;}throw std::invalid_argument("Unsupported typed mixing JSON value");}
}
bool has_linear_mixing_io(){return true;}
LinearMixingMatrix linear_mixing_matrix_from_json(const std::string& text){if(text.empty() || text.size()>byte_limit)throw std::length_error("Mixing JSON must be1..1048576 bytes");try{
    std::vector<std::set<std::string>> keys;std::size_t events=0;const auto callback=[&](int depth,J::parse_event_t event,J& value){if(depth>8 || ++events>16384)throw std::invalid_argument("Mixing JSON depth/node limit");if(event==J::parse_event_t::object_start)keys.emplace_back();else if(event==J::parse_event_t::object_end)keys.pop_back();else if(event==J::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)throw std::invalid_argument("Duplicate mixing JSON key");return true;};
    return LinearMixingMatrix::from_value(native(J::parse(text,callback)));
    }catch(const J::exception& e){throw std::invalid_argument(std::string("Invalid mixing JSON: ")+e.what());}}
std::string linear_mixing_matrix_to_json(const LinearMixingMatrix& p){auto text=json(p.to_value()).dump(2);if(text.size()>byte_limit)throw std::length_error("Mixing JSON output limit");return text;}
LinearMixingMatrix load_linear_mixing_matrix_file(const std::string& path,const std::atomic<bool>* cancelled){detail::WorkflowInput input(path,byte_limit,false,cancelled);auto p=linear_mixing_matrix_from_json(detail::read_workflow_input(input,cancelled));input.verify(false,cancelled);return p;}
void save_linear_mixing_matrix_file(const LinearMixingMatrix& p,const std::string& path,const std::atomic<bool>* cancelled,const std::function<void()>& guard){workflow_checkpoint(cancelled);const auto text=linear_mixing_matrix_to_json(p);if(guard)guard();detail::AtomicExportFile output(path,byte_limit,cancelled);output.write(text.data(),text.size());if(guard)guard();output.commit(false);}
}
#else
namespace qpx {
namespace {void unsupported(){throw std::runtime_error("Mixing matrix JSON/file IO requires project/JSON support");}}
bool has_linear_mixing_io(){return false;}
LinearMixingMatrix linear_mixing_matrix_from_json(const std::string&){unsupported();return {};}
std::string linear_mixing_matrix_to_json(const LinearMixingMatrix&){unsupported();return {};}
LinearMixingMatrix load_linear_mixing_matrix_file(const std::string&,const std::atomic<bool>*){unsupported();return {};}
void save_linear_mixing_matrix_file(const LinearMixingMatrix&,const std::string&,const std::atomic<bool>*,const std::function<void()>&){unsupported();}
}
#endif
