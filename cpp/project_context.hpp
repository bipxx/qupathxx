#pragma once
#include "qupathxx/core.hpp"

namespace qpx::detail {
inline std::size_t project_class_bytes(const std::map<std::string,ObjectClass>& classes,
                                       const std::vector<std::string>& defaults) {
    std::size_t bytes=0;
    for(const auto& [name,value]:classes)
        bytes+=sizeof(ObjectClass)+128+name.size()+value.name.size()+value.parent.size();
    for(const auto& name:defaults) bytes+=sizeof(std::string)+name.size();
    return bytes;
}

inline std::size_t string_metadata_bytes(const std::map<std::string,std::string>& metadata) {
    std::size_t bytes=sizeof(metadata);
    for(const auto& [key,value]:metadata) bytes+=128+key.size()+value.size();
    return bytes;
}
}
