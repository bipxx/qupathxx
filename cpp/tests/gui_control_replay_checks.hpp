#pragma once
#include "../gui_workflow_ui.hpp"
#include "../gui_workspace.hpp"
namespace control_replay_checks {
struct TraceScope {
    qpx::ui::WorkflowTrace trace;
    qpx::ui::WorkflowTrace* previous=qpx::ui::workflow_trace;
    TraceScope(){qpx::ui::workflow_trace=&trace;}
    ~TraceScope(){qpx::ui::workflow_trace=previous;}
};
inline bool activate(ImGuiWindow* window,const char* id) {
    if(!window || window->LastFrameActive!=ImGui::GetFrameCount() || window->SkipItems)return false;
    ImGui::ActivateItemByID(ImHashStr(id,0,window->ID));return true;
}
inline bool eye(const qpx::ui::WorkflowTrace& trace,int& stage) {
    if(stage>=3)return true;
    const auto target=trace.actions.find("Show objects (O)");if(target==trace.actions.end() || !target->second.enabled)return false;
    const auto& hit=target->second.bounds;auto& io=ImGui::GetIO();io.AddMousePosEvent(float(hit.x+hit.width/2),float(hit.y+hit.height/2));
    if(stage>0)io.AddMouseButtonEvent(0,stage==1);
    return ++stage==3;
}
inline bool metadata() {
    return activate(ImGui::FindWindowByName(qpx::GuiWorkspace::project_pane),"###Edit image metadata");
}
}
