// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/annotation_xml.hpp"
namespace qpx_ome {
using namespace qpx_annotation_xml;
inline constexpr std::string_view ns =
    "http://www.openmicroscopy.org/Schemas/OME/2016-06";
inline bool ome(std::string_view name, std::string_view type) {
  return name.size() == ns.size() + 1 + type.size() &&
         name.substr(0, ns.size()) == ns && name[ns.size()] == separator &&
         local(name) == type;
}
} // namespace qpx_ome
