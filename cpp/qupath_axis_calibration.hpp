#pragma once
#include "qupathxx/image_axis_calibration.hpp"
#include <nlohmann/json_fwd.hpp>

namespace qpx::detail {
ImageAxisCalibration decode_qupath_axis_calibration(const nlohmann::json& z_calibration,
                                                     const nlohmann::json& time_calibration);
}
