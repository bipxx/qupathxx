#pragma once
#include "qupathxx/interchange.hpp"
#include <optional>
#include "qupathxx/image_axis_calibration.hpp"
#include <atomic>
namespace qpx::detail {
struct QuPathSavedData {
    std::string geojson,server_metadata;
    std::optional<double> pixel_width_um,pixel_height_um;
    std::vector<InterchangeIssue> issues;
    std::shared_ptr<const ObjectProperties> image_properties;
    std::optional<ImageAnalysisSettings> image_settings;
    std::optional<ImageAxisCalibration> axis_calibration;
    std::shared_ptr<const ImageDataProperties> data_properties;
    std::map<std::string,std::shared_ptr<const TmaCoreInfo>> tma_cores;
};
QuPathSavedData decode_qupath_saved_data(const std::string&,const std::atomic<bool>* cancelled=nullptr);
}
