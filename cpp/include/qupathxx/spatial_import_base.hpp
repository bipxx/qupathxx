#pragma once
#include "qupathxx/object_plane.hpp"
#include <optional>
#include <string>
namespace qpx {
// Shared image/project selection for selected-count and indexed-matrix imports.
struct SpatialImportBaseRequest {
    std::string input_project,output_project,image_path,image_reader="auto";
    std::uint64_t image_id=0;
    std::optional<std::uint32_t> image_series;
    ObjectPlane plane;
    void validate() const;
};
}
