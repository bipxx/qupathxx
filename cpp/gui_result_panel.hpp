#pragma once
#include "gui_result_state.hpp"
#include "gui_workflow_ui.hpp"
#include <functional>

namespace qpx::ui {
struct ResultIds {
    const char* keep;const char* view;const char* recheck;const char* add;const char* remove;
    const char* details="Details";
};
struct KeptView {
    std::uint64_t id=0;std::string label;RecordState state=RecordState::Unknown;bool can_add=false;
    std::string raw;
    const char* state_format="%s"; // Catalogue key only; label is always literal display text.
};
struct ResultModel {
    RunState run=RunState::Idle;std::string phase,error;std::uint64_t done=0,total=0;
    bool has_preview=false,preview_is_kept=false,preview_is_recheck=false,project=false;
    std::optional<KeptView> kept;std::size_t kept_count=0;
    std::string raw_run,cancellation;
    bool busy=false,can_keep=false,show_keep=false,can_recheck=false;
    bool keep_requested=false,recheck_requested=false,cancel_requested=false;
    const char* cancel_original=nullptr;
    const char* error_format="%s";
    std::function<bool()> is_busy;
    std::function<Availability(const char*)> availability;
    std::function<void()> provenance,leading_content,trailing_content,after_actions;
};
struct ResultActions {std::function<void()> keep,view,recheck,add,remove,cancel;};
enum class ResultPrimary {None,Run,Keep,View};
inline ResultPrimary primary_action(const ResultModel& model) {
    // While no job is active, Run is primary before a ready result, Keep is primary for an eligible fresh result, and View is primary for a kept or Recheck result when that action exists; busy work has no primary action.
    if(model.busy)return ResultPrimary::None;
    if(model.run==RunState::Ready && model.can_keep)return ResultPrimary::Keep;
    if(model.run==RunState::Ready && (model.preview_is_kept || model.preview_is_recheck))return ResultPrimary::View;
    return model.run==RunState::Ready?ResultPrimary::None:ResultPrimary::Run;
}
inline design::Action result_role(const ResultModel& model,ResultPrimary action) {
    return primary_action(model)==action?design::Action::Primary:design::Action::Secondary;
}
inline std::string run_caption(const ResultModel& model) {
    if(!model.cancellation.empty())return model.cancellation;
    const auto state=parse_run_state(model.raw_run);
    if(model.busy && state!=RunState::Unknown && state!=RunState::Running && state!=RunState::Cancelling)
        return caption(RunState::Running,"running");
    return caption(parse_run_state(model.raw_run),model.raw_run);
}
inline std::string tissue_step_sentence(std::string_view phase) {
    const char* key=phase=="smoothing"?"Smoothing tissue feature...":phase=="contours"?"Tracing tissue contours...":
        phase=="cleanup"?"Cleaning tissue mask...":phase=="components"?"Finding components...":"Reading and thresholding...";
    return i18n::text(key);
}
inline std::string cell_step_sentence(std::string_view phase) {
    if(phase=="reading")return i18n::text("Reading pixels...");
    if(phase=="smoothing")return i18n::text("Smoothing cell feature...");
    if(phase=="distance")return i18n::text("Measuring distances...");
    if(phase=="watershed")return i18n::text("Separating nuclei...");
    if(phase=="expansion")return i18n::text("Expanding cell bodies...");
    if(phase=="contours")return i18n::text("Tracing cell contours...");
    return {};
}
inline Availability result_availability(const ResultModel& model,const char* original,bool enabled);
inline void draw_run_status(const ResultModel& model,const ResultActions& actions) {
    const auto status=run_caption(model);
    {ActionRow row(model.busy && model.cancel_original?1:0);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row.field_width());TextWrapped("%s",status.c_str());ImGui::PopTextWrapPos();
    if(model.busy) {
        const auto available=model.cancel_original?result_availability(model,model.cancel_original,true):Availability{};
        if(model.cancel_original)row.next();
        const bool clicked=model.cancel_original && WorkflowIcon(model.cancel_original,Icon::Stop,available);
        if(available.enabled && (clicked || model.cancel_requested) && actions.cancel)actions.cancel();
    }}
    if(model.busy && !model.phase.empty())TextWrapped("%s",model.phase.c_str());
    if(model.busy && model.total>0)Text("Tiles: %llu / %llu",static_cast<unsigned long long>(model.done),static_cast<unsigned long long>(model.total));
    if(!model.error.empty())TextWrapped(model.error_format,model.error.c_str());
}
inline Availability result_availability(const ResultModel& model,const char* original,bool enabled) {
    if(model.availability)return model.availability(original);
    if(enabled)return {};
    return {false,model.busy?"Wait for the current work to finish.":"Run a preview first."};
}
inline void draw_keep_row(const ResultModel& model,const ResultIds& ids,const ResultActions& actions) {
    if(model.show_keep) {
        const auto available=result_availability(model,ids.keep,model.can_keep);
        const bool clicked=WorkflowAction(ids.keep,result_role(model,ResultPrimary::Keep),available);
        if(result_role(model,ResultPrimary::Keep)==design::Action::Primary)WorkflowReason(ids.keep,available);
        if(available.enabled && (clicked || model.keep_requested) && actions.keep)actions.keep();
    }
    if(model.kept_count && WorkflowAction(ids.view,result_role(model,ResultPrimary::View),result_availability(model,ids.view,true)) && actions.view)actions.view();
}
inline void draw_kept_block(const ResultModel& model,const ResultIds& ids,const ResultActions& actions,const std::function<void()>& details) {
    if(!model.kept)return;
    SeparatorText("Kept result");
    if(model.provenance)model.provenance();
    if(model.leading_content)model.leading_content();
    const auto status=caption(parse_record_state(model.kept->raw),model.kept->raw);
    if(!model.kept->label.empty())TextWrapped("%s",model.kept->label.c_str());
    TextWrapped(model.kept->state_format,status.c_str());
    if(model.trailing_content)model.trailing_content();
    const auto available=result_availability(model,ids.recheck,model.can_recheck);
    const bool recheck=WorkflowAction(ids.recheck,design::Action::Secondary,available);
    if(available.enabled && (recheck || model.recheck_requested) && actions.recheck)actions.recheck();
    if(AvailableButton(ids.remove,result_availability(model,ids.remove,!(model.is_busy?model.is_busy():model.busy)),design::Action::Secondary,"Remove kept result") && actions.remove)actions.remove();
    if(ids.add) {
        if(AvailableButton(ids.add,result_availability(model,ids.add,model.kept->can_add)) && actions.add)actions.add();
    }
    if(model.after_actions)model.after_actions();
    if(details && CollapsingHeader(ids.details))details();
}
}
