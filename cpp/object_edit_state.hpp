#pragma once
#include "qupathxx/object_properties.hpp"
namespace qpx::detail {
inline std::shared_ptr<const ObjectProperties> reclassified_properties(const std::shared_ptr<const ObjectProperties>& value,bool class_assigned) {
    if(!value || (!value->class_probability() && (!class_assigned || !value->color_rgb())))return value;
    return value->with_edit_state(value->locked(),class_assigned?std::nullopt:value->color_rgb(),std::nullopt);
}
}
