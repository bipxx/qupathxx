#pragma once
#include "qupathxx/density_workflow.hpp"
#include "strict_json.hpp"
namespace qpx::detail {
enum class DensityFormat {GuiV1,ProjectV1};
nlohmann::json density_engine(const BuildEnvironment&);
const char* density_checkpoint_schema(DensityFormat);
nlohmann::json density_receipt(const ProjectDensityReport&,const ProjectDensityImage&,
    const SpatialDensityResult&,DensityFormat);
std::string density_image_json(const ProjectDensityImage&,DensityFormat);
std::string density_report_json(const ProjectDensityReport&,DensityFormat);
}
