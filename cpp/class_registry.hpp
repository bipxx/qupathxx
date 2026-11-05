#pragma once
#include "qupathxx/core.hpp"
#include "qupath_class_color.hpp"
#include <algorithm>

namespace qpx::detail {
inline void validate_class_definition(const ObjectClass& value) {
    bounded_utf8(value.name,4096);bounded_utf8(value.parent,4096);
    if(value.name.empty() || value.color>0xffffff)throw std::invalid_argument("invalid class name or RGB view");
    if(!value.parent.empty()) {
        const auto prefix=value.parent+": ";
        if(value.name.size()<=prefix.size() || value.name.compare(0,prefix.size(),prefix)!=0)
            throw std::invalid_argument("class key must qualify its explicit parent and nonempty leaf");
    }
}
// Lookup returns an existing definition or nullptr. Only the supplied parent
// edges define topology; arbitrary flat names are never split into ancestors.
template<class Lookup> std::vector<const ObjectClass*> class_chain(const ObjectClass& value,Lookup lookup,bool validate=true) {
    std::vector<const ObjectClass*> chain;chain.reserve(8);const auto* current=&value;
    for(;;) {
        if(validate)validate_class_definition(*current);
        if(chain.size()>=32)throw std::invalid_argument("class ancestry exceeds32 levels");
        chain.push_back(current);if(current->parent.empty())break;
        const auto* parent=lookup(current->parent);
        if(!parent || parent->name!=current->parent)throw std::invalid_argument("class parent is not registered");
        current=parent;
    }
    std::reverse(chain.begin(),chain.end());return chain;
}
template<class Lookup> ObjectClass resolve_class_definition(ObjectClass value,Lookup lookup,bool validated=false) {
    const auto chain=class_chain(value,lookup,!validated);
    if(value.color_present)return value;
    std::vector<std::string> path;path.reserve(chain.size());std::optional<std::uint32_t> resolved;
    for(const auto* node:chain) {
        path.push_back(node->leaf_name());
        resolved=node->color_present?node->color_argb():std::optional<std::uint32_t>(qupath_class_default_argb(path,resolved));
    }
    value.color=*resolved&0xffffff;value.alpha=0;return value;
}
inline std::map<std::string,ObjectClass> normalized_class_registry(const std::vector<ObjectClass>& definitions) {
    std::map<std::string,ObjectClass> result;
    for(const auto& definition:definitions)if(!result.emplace(definition.name,definition).second)throw std::invalid_argument("duplicate class registry definition");
    auto lookup=[&](const std::string& name)->const ObjectClass* {const auto found=result.find(name);return found==result.end()?nullptr:&found->second;};
    for(const auto& [name,value]:result)validate_class_definition(value);
    for(auto& [name,value]:result)value=resolve_class_definition(value,lookup,true);
    return result;
}
inline std::vector<std::string> class_path(const std::map<std::string,ObjectClass>& registry,const std::string& name) {
    if(name.empty())return {};
    const auto found=registry.find(name);if(found==registry.end())throw std::out_of_range("class does not exist");
    const auto chain=class_chain(found->second,[&](const std::string& key)->const ObjectClass* {const auto at=registry.find(key);return at==registry.end()?nullptr:&at->second;});
    std::vector<std::string> result;result.reserve(chain.size());for(const auto* value:chain)result.push_back(value->leaf_name());return result;
}
}
