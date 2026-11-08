#pragma once
#include "qupathxx/density_workflow.hpp"
#include "workflow_input.hpp"
namespace qpx::detail {
ProjectDensityReport preview_density_input(const WorkflowInput&,const ProjectDensityRecipe&,
    const std::atomic<bool>*,ProjectDensityProgress*);
}
