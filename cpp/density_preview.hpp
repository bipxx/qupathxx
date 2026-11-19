#pragma once
#include "qupathxx/density_workflow.hpp"
#include "workflow_input.hpp"
namespace qpx {class Project;}
namespace qpx::detail {
ProjectDensityReport preview_density_input(const WorkflowInput&,const Project&,const ProjectDensityRecipe&,
    const std::atomic<bool>*,ProjectDensityProgress*);
}
