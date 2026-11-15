#pragma once
#include <array>
namespace qpx::ui {
// UI navigation only; recipes, jobs and results retain their native owners.
enum class WorkflowTask { Measurements, Region, Tissue, Cells, Tables, Tools, Files };
inline constexpr std::array workflow_task_keys={"measurements","region","tissue","cells","tables","tools","files"};
inline constexpr std::array workflow_task_labels={"Measurements","Region analysis","Threshold tissue mask","Watershed cells","Tables","Tools","Source and files"};
}
