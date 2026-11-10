#pragma once
#include "qupathxx/marker_rules.hpp"
#include "qupathxx/image_data_properties.hpp"
namespace qpx::detail {
ImageDataValue encode_marker_rule(const MarkerRule&);
MarkerRule decode_marker_rule(const ImageDataValue&);
void evaluate_marker_observations(const MarkerRule&,const MarkerObservation*,MarkerTruth*,std::vector<MarkerTruth>&);
}
