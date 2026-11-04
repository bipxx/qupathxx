#pragma once
#include "qupathxx/object_properties.hpp"
#include <cstdint>
#include <vector>
namespace qpx::detail {
std::vector<std::uint8_t> encode_object_properties(const ObjectProperties&);
std::shared_ptr<const ObjectProperties> decode_object_properties(const void*,std::size_t,bool allow_edit_state=true);
}
