#pragma once
#include <array>
namespace qpx::ui {
// UI navigation only; recipes, jobs and results retain their native owners.
enum class WorkflowTask { Measurements, Region, Tissue, Cells, Tables, Tools, Files };
inline constexpr std::array workflow_task_keys={"measurements","region","tissue","cells","tables","tools","files"};
inline constexpr std::array workflow_task_labels={"Measurements","Region analysis","Threshold tissue mask","Watershed cells","Tables","Tools","Source and files"};
enum class F5Action { PixelClassifier, Recoveries, Ignore };
inline F5Action f5_action(bool ctrl,bool shift,bool alt) {
    if(alt) return F5Action::Ignore;
    if(ctrl && shift) return F5Action::Recoveries;
    return !ctrl && !shift?F5Action::PixelClassifier:F5Action::Ignore;
}
}
