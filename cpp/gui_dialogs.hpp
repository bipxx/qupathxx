#pragma once
#include "gui_layout.hpp"
#include "gui_dialog_contract.hpp"
#include <functional>
#include <initializer_list>
#include <vector>

namespace qpx::ui {
namespace dialog_detail {
// Dialogs share a surface and layout, while callers retain command/state ownership.
inline bool BeginDialog(const char* name,ImVec2 preferred={design::metrics.dialog_width,design::metrics.dialog_confirm_height}) {
    const auto* vp=ImGui::GetMainViewport();const float s=design::scale(),margin=design::spacing.xl*s;
    const ImVec2 extent{std::max(1.f,vp->WorkSize.x-2*margin),std::max(1.f,vp->WorkSize.y-2*margin)};
    ImGui::SetNextWindowPos({vp->WorkPos.x+vp->WorkSize.x*.5f,vp->WorkPos.y+vp->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    ImGui::SetNextWindowSize({std::min(preferred.x*s,extent.x),std::min(preferred.y*s,extent.y)},ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({std::min(280*s,extent.x),std::min(design::metrics.dialog_min_height*s,extent.y)},extent);
    if(const auto* window=ImGui::FindWindowByName(i18n::label(name).c_str());window && window->WasActive) {
        const ImVec2 size{std::min(window->SizeFull.x,extent.x),std::min(window->SizeFull.y,extent.y)};
        const ImVec2 pos{std::clamp(window->Pos.x,vp->WorkPos.x+margin,vp->WorkPos.x+margin+extent.x-size.x),std::clamp(window->Pos.y,vp->WorkPos.y+margin,vp->WorkPos.y+margin+extent.y-size.y)};
        if(pos.x!=window->Pos.x || pos.y!=window->Pos.y)ImGui::SetNextWindowPos(pos);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{design::metrics.dialog_padding*s,design::metrics.dialog_padding*s});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,design::metrics.panel_radius*s);
    const bool open=ImGui::BeginPopupModal(i18n::label(name).c_str(),nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar(2);return open;
}
inline void DialogContext(const std::string& text) {
    if(text.empty())return;
    design::Caption caption;const auto at=ImGui::GetCursorScreenPos();const float width=available();
    ImGui::Dummy({width,ImGui::GetFontSize()});
    ImGui::PushStyleColor(ImGuiCol_Text,design::color(design::colors().text_secondary));
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),at,{at.x+width,at.y+ImGui::GetFontSize()},at.x+width,text.c_str(),nullptr,nullptr);
    ImGui::PopStyleColor();
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",text.c_str());
}
enum class DialogCommand {None,Close,Reload};
inline DialogCommand DialogHeader(const char* title,const std::string& context={},const char* reload=nullptr) {
    DialogCommand command=DialogCommand::None;
    {
        ActionRow row(reload?2:1);
        {design::Heading heading(design::metrics.title_size);ImGui::AlignTextToFramePadding();ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row.field_width());TextUnformatted(title);ImGui::PopTextWrapPos();}
        if(reload) {row.next();if(IconButton(reload,Icon::Refresh))command=DialogCommand::Reload;}
        row.next();if(IconButton("Cancel###Dialog close",Icon::Close))command=DialogCommand::Close;
    }
    DialogContext(context);ImGui::Spacing();ImGui::Separator();ImGui::Spacing();return command;
}
struct DialogActionLayout {
    std::vector<float> widths,heights;
    float width=available(),height=0,total_width=0;
    bool stacked=false;
    DialogActionLayout(const std::vector<const char*>& labels) {
        const float s=design::scale(),gap=design::spacing.md*s;
        for(const auto* label:labels) {
            const auto local=i18n::label(label);const float natural=std::max(design::metrics.dialog_button_min_width*s,ImGui::CalcTextSize(local.c_str(),label_end(local.c_str())).x+2*design::metrics.button_padding_x*s);
            widths.push_back(natural);total_width+=natural;
        }
        total_width+=gap*std::max(0,int(widths.size())-1);stacked=total_width>width;
        std::size_t i=0;for(const auto* label:labels) {
            if(stacked)widths[i]=width;
            const auto local=i18n::label(label);const float text_width=std::max(1.f,widths[i]-2*design::metrics.button_padding_x*s);
            const float button=std::max(design::metrics.dialog_action_height*s,ImGui::CalcTextSize(local.c_str(),label_end(local.c_str()),false,text_width).y+2*ImGui::GetStyle().FramePadding.y);
            heights.push_back(button);height=stacked?height+button:std::max(height,button);++i;
        }
        if(stacked)height+=gap*std::max(0,int(widths.size())-1);
        height+=(design::spacing.md+design::spacing.lg)*s+ImGui::GetStyle().ItemSpacing.y;
    }
};
// A scroll wrapper keeps the dialog's semantic IDs stable when presentation moves.
class DialogBody {
public:
    explicit DialogBody(float footer_height) {
        const auto seed=ImGui::GetCurrentWindow()->ID;
        const float height=std::max(1.f,ImGui::GetContentRegionAvail().y-footer_height-ImGui::GetStyle().ItemSpacing.y);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        ImGui::BeginChild("##Dialog content",{0,height},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleVar();ImGui::PushOverrideID(seed);
    }
    ~DialogBody(){ImGui::PopID();ImGui::EndChild();}
    DialogBody(const DialogBody&)=delete;DialogBody& operator=(const DialogBody&)=delete;
};
class DialogFooter {
    DialogActionLayout layout_;std::size_t index_=0;float start_;
public:
    explicit DialogFooter(const DialogActionLayout& layout):layout_(layout),start_(ImGui::GetCursorPosX()) {
        const float s=design::scale();ImGui::SetCursorPosY(ImGui::GetCursorPosY()+design::spacing.md*s);
        const auto at=ImGui::GetCursorScreenPos();ImGui::GetWindowDrawList()->AddLine(at,{at.x+layout_.width,at.y},ImGui::GetColorU32(ImGuiCol_Separator),s);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+design::spacing.lg*s);
    }
    ImVec2 next() {
        const float gap=design::spacing.md*design::scale();
        if(index_==0)ImGui::SetCursorPosX(start_+(layout_.stacked?0:layout_.width-layout_.total_width));
        else if(layout_.stacked) {ImGui::SetCursorPosX(start_);ImGui::SetCursorPosY(ImGui::GetCursorPosY()+std::max(0.f,gap-ImGui::GetStyle().ItemSpacing.y));}
        else ImGui::SameLine(0,gap);
        const auto i=index_++;return {layout_.widths.at(i),layout_.heights.at(i)};
    }
};
struct PopupScope {~PopupScope(){ImGui::EndPopup();}};
struct DisabledScope {explicit DisabledScope(bool disabled){ImGui::BeginDisabled(disabled);}~DisabledScope(){ImGui::EndDisabled();}};
struct ModalInputScope {
    bool inherited=(ImGui::GetCurrentContext()->CurrentItemFlags&ImGuiItemFlags_Disabled)!=0;
    ModalInputScope(){if(inherited)ImGui::BeginDisabledOverrideReenable();}
    ~ModalInputScope(){if(inherited)ImGui::EndDisabledOverrideReenable();}
};
struct KeyboardHistory {int frame=-1;ImGuiID top=0,active=0;bool text=false;};
inline KeyboardHistory& keyboard_history() {
    auto* context=ImGui::GetCurrentContext();const auto owner=ImHashStr("qupathxx.dialog.contract");
    for(const auto& hook:context->Hooks)if(hook.Owner==owner && hook.Type==ImGuiContextHookType_NewFramePre)return *static_cast<KeyboardHistory*>(hook.UserData);
    auto* history=new KeyboardHistory;
    ImGuiContextHook hook;hook.Owner=owner;hook.UserData=history;hook.Type=ImGuiContextHookType_NewFramePre;
    hook.Callback=[](ImGuiContext* ctx,ImGuiContextHook* item) {auto& snapshot=*static_cast<KeyboardHistory*>(item->UserData);snapshot.frame=ctx->FrameCount+1;snapshot.active=ctx->ActiveId;snapshot.text=ctx->IO.WantTextInput;snapshot.top=ctx->OpenPopupStack.empty() || !ctx->OpenPopupStack.back().Window?0:ctx->OpenPopupStack.back().Window->ID;};
    ImGui::AddContextHook(context,&hook);hook.Type=ImGuiContextHookType_Shutdown;hook.Callback=[](ImGuiContext*,ImGuiContextHook* item){delete static_cast<KeyboardHistory*>(item->UserData);};ImGui::AddContextHook(context,&hook);return *history;
}
inline bool cancel_key(const DialogState& state) {
    if(state.busy || !state.foreground || !ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::GetIO().WantTextInput)return false;
    const auto* window=ImGui::GetCurrentWindow();const auto* modal=ImGui::GetTopMostPopupModal();if(modal!=window)return false;
    const auto& snapshot=keyboard_history();
    if(snapshot.frame==ImGui::GetFrameCount() && (snapshot.active || snapshot.text || (snapshot.top && snapshot.top!=window->ID)))return false;
    const auto& stack=ImGui::GetCurrentContext()->OpenPopupStack;if(!stack.empty() && stack.back().Window && stack.back().Window!=window)return false;
    ImGui::SetKeyOwner(ImGuiKey_Escape,window->ID,ImGuiInputFlags_LockThisFrame);return true;
}
inline std::vector<const char*> footer_labels(const std::vector<DialogActionDefinition>& actions) {std::vector<const char*> labels;for(const auto& action:actions)if(action.placement==DialogPlacement::Footer)labels.push_back(action.label);return labels;}
inline bool draw_action(const DialogActionDefinition& action,const DialogState& state,ImVec2 size,bool auxiliary=false) {
    DisabledScope disabled(!dialog_action_enabled(state,action));
    if(action.scope) {ImGui::PushID("Entry fields");ImGui::PushID(action.scope);}
    const auto role=action.role==DialogActionRole::Primary?design::Action::Primary:action.role==DialogActionRole::Destructive?design::Action::Danger:auxiliary?design::Action::Ghost:design::Action::Secondary;
    const bool pressed=Button(action.label,size,role);
    if(action.role==DialogActionRole::Cancel)ImGui::SetItemDefaultFocus();
    if(action.scope) {ImGui::PopID();ImGui::PopID();}return pressed;
}
} // namespace dialog_detail
struct DialogResult {bool visible=false,dismissed=false;DialogAction action=DialogAction::None;DialogOutcome outcome=DialogOutcome::Refused;};
inline bool DialogOwnsInput() {return ImGui::GetCurrentContext() && ImGui::GetTopMostPopupModal()!=nullptr;}
namespace dialog_detail {struct ControlAccess;}
class DialogControls {
    DialogKind kind_;DialogState& state_;std::function<void(DialogAction)> mark_;DialogAction intent_=DialogAction::None;
    friend struct dialog_detail::ControlAccess;
    void mark(DialogAction action) const {if(mark_)mark_(action);}
    bool request(DialogAction id) {
        if(intent_!=DialogAction::None)return false;
        for(const auto& action:dialog_actions(kind_,state_.variant))if(action.id==id) {if(!dialog_action_enabled(state_,action))return false;intent_=id;return true;}
        throw std::logic_error("Dialog requested an undeclared action");
    }
public:
    DialogControls(DialogKind kind,DialogState& state,std::function<void(DialogAction)> mark):kind_(kind),state_(state),mark_(std::move(mark)){}
    void variant(DialogVariant value) {if(!dialog_variant_allowed(kind_,value))throw std::logic_error("Invalid dialog body variant");state_.variant=value;}
    DialogAction intent() const {return intent_;}
    bool auxiliary(DialogAction id) {
        for(const auto& action:dialog_actions(kind_,state_.variant))if(action.id==id) {
            if(action.placement!=DialogPlacement::Body)throw std::logic_error("Only declared secondary commands belong in the dialog body");
            const bool pressed=dialog_detail::draw_action(action,state_,{},true);mark(id);return pressed && request(id);
        }
        throw std::logic_error("Undeclared dialog body command");
    }
};
namespace dialog_detail {
struct ControlAccess {
    static bool request(DialogControls& controls,DialogAction action){return controls.request(action);}
    static void mark(const DialogControls& controls,DialogAction action){controls.mark(action);}
};
}
inline void OpenDialog(DialogKind kind) {ImGui::OpenPopup(dialog_definition(kind).name);}
inline bool IsDialogOpen(DialogKind kind) {return ImGui::IsPopupOpen(dialog_definition(kind).name);}
// The renderer alone owns popup lifetime, layout, role styling and dispatch.
// Body callbacks edit local draft/presentation state; only typed handlers publish.
template<class Body,class Handler>
DialogResult RenderDialog(DialogKind kind,DialogState& state,const std::string& context,std::initializer_list<const std::string*> errors,Body&& body,Handler&& handler,std::function<void(DialogAction)> mark={}) {
    const auto& definition=dialog_definition(kind);if(!dialog_variant_allowed(kind,state.variant))throw std::logic_error("Invalid dialog state variant");
    dialog_detail::keyboard_history();DialogResult result;
    if(!ImGui::IsPopupOpen(definition.name))return result;
    const auto preferred=definition.layout==DialogLayout::Editor?ImVec2{design::metrics.dialog_editor_width,design::metrics.dialog_editor_height}:definition.layout==DialogLayout::Selector?ImVec2{700,520}:ImVec2{design::metrics.dialog_width,kind==DialogKind::ReplaceWorkspace?380:design::metrics.dialog_confirm_height};
    // Re-enable before BeginPopupModal draws its background as well as controls.
    // Restore the originating window only after the popup scope has ended.
    dialog_detail::ModalInputScope input;
    if(!dialog_detail::BeginDialog(definition.name,preferred))return result;
    dialog_detail::PopupScope popup;result.visible=true;
    state.foreground=ImGui::GetTopMostPopupModal()==ImGui::GetCurrentWindow();
    if(state.dismiss_requested) {ImGui::CloseCurrentPopup();result.dismissed=true;return result;}
    DialogControls controls(kind,state,std::move(mark));auto actions=dialog_actions(kind,state.variant);
    const char* reload=nullptr;for(const auto& action:actions)if(action.placement==DialogPlacement::Header)reload=action.label;
    {
        dialog_detail::DisabledScope locked(state.busy || !state.foreground);
        const auto header=dialog_detail::DialogHeader(kind==DialogKind::ImageMetadata?"Edit image metadata":definition.name,context,reload);
        if(header==dialog_detail::DialogCommand::Close)dialog_detail::ControlAccess::request(controls,DialogAction::Cancel);
        if(header==dialog_detail::DialogCommand::Reload)dialog_detail::ControlAccess::request(controls,DialogAction::Reload);
        if(dialog_detail::cancel_key(state))dialog_detail::ControlAccess::request(controls,DialogAction::Cancel);
    }
    float footer_height=0;
    for(auto variant:dialog_variants(kind))footer_height=std::max(footer_height,dialog_detail::DialogActionLayout(dialog_detail::footer_labels(dialog_actions(kind,variant))).height);
    std::string notice;
    if(!state.owner_valid)notice=i18n::text(reload?"The active project or image changed. Reload the current entry or cancel.":"The selected item changed. Cancel and choose it again.");
    for(const auto* error:errors)if(error && !error->empty()) {if(!notice.empty())notice+="\n\n";notice+=*error;}
    const float spacing=ImGui::GetStyle().ItemSpacing.y,room=ImGui::GetContentRegionAvail().y;
    const float notice_height=notice.empty()?0:std::min({ImGui::CalcTextSize(notice.c_str(),nullptr,false,available()).y,design::metrics.dialog_error_max_height*design::scale(),std::max(1.f,room-footer_height-spacing-1)});
    {
        dialog_detail::DialogBody content(footer_height+(notice.empty()?0:notice_height+spacing));
        dialog_detail::DisabledScope locked(state.busy || !state.foreground || !state.owner_valid || controls.intent()!=DialogAction::None);body(controls);
    }
    if(!notice.empty()) {
        const bool scroll=ImGui::CalcTextSize(notice.c_str(),nullptr,false,available()).y>notice_height;
        if(scroll)ImGui::BeginChild("##Dialog notices",{0,notice_height},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
        ImGui::PushStyleColor(ImGuiCol_Text,design::color(design::colors().error));TextWrapped("%s",notice.c_str());ImGui::PopStyleColor();if(scroll)ImGui::EndChild();
    }
    const auto& open_stack=ImGui::GetCurrentContext()->OpenPopupStack;
    state.foreground=ImGui::GetTopMostPopupModal()==ImGui::GetCurrentWindow() && (open_stack.empty() || !open_stack.back().Window || open_stack.back().Window==ImGui::GetCurrentWindow());
    actions=dialog_actions(kind,state.variant);const dialog_detail::DialogActionLayout layout(dialog_detail::footer_labels(actions));dialog_detail::DialogFooter footer(layout);
    for(const auto& action:actions)if(action.placement==DialogPlacement::Footer) {
        dialog_detail::DisabledScope selected(controls.intent()!=DialogAction::None);
        const bool pressed=dialog_detail::draw_action(action,state,footer.next());dialog_detail::ControlAccess::mark(controls,action.id);if(pressed)dialog_detail::ControlAccess::request(controls,action.id);
    }
    if(controls.intent()!=DialogAction::None)for(const auto& action:actions)if(action.id==controls.intent()) {
        result.action=action.id;result.outcome=handler(action.id);
        if(action.role==DialogActionRole::Cancel && result.outcome!=DialogOutcome::Success)throw std::logic_error("An available Cancel must dismiss without publication");
        if(result.outcome==DialogOutcome::Success && action.completion==DialogCompletion::DismissOnSuccess) {ImGui::CloseCurrentPopup();result.dismissed=true;}break;
    }
    return result;
}
inline bool DialogInput(const char* label,char* buffer,std::size_t size,ImVec2 area={0,0}) {
    TextDisabled(label);ImGui::SetNextItemWidth(-1);
    const std::string id=std::string("###")+label;
    const bool changed=area.y>0?ImGui::InputTextMultiline(id.c_str(),buffer,size,{available(),area.y*design::scale()},ImGuiInputTextFlags_AllowTabInput):ImGui::InputText(id.c_str(),buffer,size);
    observe(label);return changed;
}
}
