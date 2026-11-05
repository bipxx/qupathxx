#pragma once
#include "qupathxx/image_axis_calibration.hpp"
#include <cstdint>
namespace qpx::detail {
std::vector<std::uint8_t> encode_image_axis_calibration(const ImageAxisCalibration&);
ImageAxisCalibration decode_image_axis_calibration(const void*,std::size_t);
}
