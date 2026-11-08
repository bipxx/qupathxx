#pragma once
#include "qupathxx/image_analysis_settings.hpp"
#include <cstdint>
#include <vector>
namespace qpx::detail {
std::vector<std::uint8_t> encode_image_analysis_settings(const ImageAnalysisSettings&);
ImageAnalysisSettings decode_image_analysis_settings(const void*,std::size_t);
}
