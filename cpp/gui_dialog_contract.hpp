#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace qpx::ui {
// Single inventory of application modals. New surfaces must declare ownership.
#define QPX_UI_DIALOGS(X) \
    X(ImageMetadata,"Edit project entry",Editor,"gui_entry_editor.hpp",ImageEntry) \
    X(ProjectMetadata,"Edit project metadata",Editor,"gui_entry_editor.hpp",Project) \
    X(CloseProject,"Unsaved project",Confirmation,"gui.cpp",Project) \
    X(ReplaceWorkspace,"Replace workspace?",Confirmation,"gui.cpp",Workspace) \
    X(RemoveImage,"Remove image?",Confirmation,"gui.cpp",ImageEntry) \
    X(Language,"Choose language",Selector,"gui.cpp",Session) \
    X(SourceVerification,"Source verification",Confirmation,"gui.cpp",Source) \
    X(DeleteCheckpoint,"Delete recovery checkpoint?",Confirmation,"gui.cpp",Recovery) \
    X(ImportFile,"Choose import file",Selector,"gui_import_picker.hpp",Filesystem) \
    X(ReplaceExport,"Replace existing file?",Confirmation,"gui.cpp",Filesystem) \
    X(ImageInformation,"Image information",Selector,"gui.cpp",ImageEntry) \
    X(Calibration,"Edit calibration",Confirmation,"gui_calibration_editor.hpp",ImageEntry) \
    X(ObjectInformation,"Object information",Selector,"gui.cpp",Object)

enum class DialogKind {
#define QPX_DIALOG_ENUM(kind,...) kind,
    QPX_UI_DIALOGS(QPX_DIALOG_ENUM)
#undef QPX_DIALOG_ENUM
    Count
};
enum class DialogLayout {Editor,Confirmation,Selector};
enum class DialogOwnerScope {ImageEntry,Project,Workspace,Session,Source,Recovery,Filesystem,Object};
struct DialogDefinition {DialogKind kind;const char* name;DialogLayout layout;const char* owner;DialogOwnerScope scope;};
inline constexpr DialogDefinition dialog_definitions[]={
#define QPX_DIALOG_DEFINITION(kind,name,layout,owner,scope) {DialogKind::kind,name,DialogLayout::layout,owner,DialogOwnerScope::scope},
    QPX_UI_DIALOGS(QPX_DIALOG_DEFINITION)
#undef QPX_DIALOG_DEFINITION
};
#undef QPX_UI_DIALOGS
inline const DialogDefinition& dialog_definition(DialogKind kind) {
    const auto index=static_cast<std::size_t>(kind);
    if(index>=std::size(dialog_definitions))throw std::logic_error("Unregistered dialog kind");
    return dialog_definitions[index];
}
enum class DialogVariant {Default,Metadata,Details,Recovery,Output};
enum class DialogAction {None,Cancel,Reload,ApplyMetadata,ApplyDetails,Save,Discard,KeepRecovery,Confirm};
enum class DialogActionRole {Cancel,Reload,Primary,Destructive,Secondary};
enum class DialogPlacement {Header,Footer,Body};
enum class DialogCompletion {KeepOpen,DismissOnSuccess};
enum class DialogOutcome {Success,Refused,Pending};
struct DialogActionDefinition {
    DialogAction id;const char* label;DialogActionRole role;DialogPlacement placement;DialogCompletion completion;
    const char* scope=nullptr;
};
inline bool dialog_variant_allowed(DialogKind kind,DialogVariant variant) {
    switch(kind) {
    case DialogKind::ImageMetadata:return variant==DialogVariant::Metadata || variant==DialogVariant::Details;
    case DialogKind::ProjectMetadata:return variant==DialogVariant::Metadata;
    case DialogKind::ReplaceWorkspace:return variant==DialogVariant::Default || variant==DialogVariant::Recovery;
    case DialogKind::ImportFile:return variant==DialogVariant::Default || variant==DialogVariant::Output;
    default:dialog_definition(kind);return variant==DialogVariant::Default;
    }
}
inline std::vector<DialogVariant> dialog_variants(DialogKind kind) {
    dialog_definition(kind);std::vector<DialogVariant> values;
    for(auto value:{DialogVariant::Default,DialogVariant::Metadata,DialogVariant::Details,DialogVariant::Recovery,DialogVariant::Output})if(dialog_variant_allowed(kind,value))values.push_back(value);
    return values;
}
inline void validate_dialog_actions(const std::vector<DialogActionDefinition>& actions) {
    std::array<bool,static_cast<std::size_t>(DialogAction::Confirm)+1> seen{};std::vector<const DialogActionDefinition*> footer;
    int cancels=0,primary=0,destructive=0,reloads=0;
    std::vector<std::string> stable_ids;
    for(const auto& item:actions) {
        const auto index=static_cast<std::size_t>(item.id);
        if(item.id==DialogAction::None || index>=seen.size() || seen[index] || !item.label || !*item.label)throw std::logic_error("Dialog actions need unique nonempty identities");
        seen[index]=true;
        if((item.id==DialogAction::Cancel && item.role!=DialogActionRole::Cancel) || (item.id==DialogAction::Reload && item.role!=DialogActionRole::Reload) || ((item.id==DialogAction::ApplyMetadata || item.id==DialogAction::ApplyDetails || item.id==DialogAction::Save) && item.role!=DialogActionRole::Primary) || (item.id==DialogAction::Discard && item.role!=DialogActionRole::Destructive) || (item.id==DialogAction::KeepRecovery && item.role!=DialogActionRole::Secondary))throw std::logic_error("Dialog action role does not match its intent");
        const std::string label=item.label;const auto hidden=label.find("###");const auto stable=hidden==std::string::npos?label:label.substr(hidden+3);
        const auto identity=std::string(item.scope?item.scope:"")+"/"+stable;
        if(stable.empty() || std::find(stable_ids.begin(),stable_ids.end(),identity)!=stable_ids.end())throw std::logic_error("Duplicate dialog widget identity");
        stable_ids.push_back(identity);
        if(item.role==DialogActionRole::Cancel) {++cancels;if(item.id!=DialogAction::Cancel || item.placement!=DialogPlacement::Footer || item.completion!=DialogCompletion::DismissOnSuccess)throw std::logic_error("Invalid dialog Cancel contract");}
        if(item.role==DialogActionRole::Primary)++primary;
        if(item.role==DialogActionRole::Destructive)++destructive;
        if(item.role==DialogActionRole::Reload) {++reloads;if(item.id!=DialogAction::Reload || item.placement!=DialogPlacement::Header || item.completion!=DialogCompletion::KeepOpen)throw std::logic_error("Invalid dialog Reload contract");}
        if(item.placement==DialogPlacement::Footer)footer.push_back(&item);
        else if(item.placement==DialogPlacement::Header && item.role!=DialogActionRole::Reload)throw std::logic_error("Only reload belongs in the dialog header");
        else if(item.placement==DialogPlacement::Body && item.role!=DialogActionRole::Secondary)throw std::logic_error("Only secondary commands belong in the dialog body");
    }
    if(cancels!=1 || primary>1 || destructive>1 || reloads>1 || footer.empty() || footer.front()->role!=DialogActionRole::Cancel)throw std::logic_error("Dialog requires one leading Cancel and at most one primary/destructive action");
    if(primary && footer.back()->role!=DialogActionRole::Primary)throw std::logic_error("Primary dialog action must be last");
    if(!primary && footer.back()->role!=DialogActionRole::Cancel && footer.back()->role!=DialogActionRole::Destructive)throw std::logic_error("Invalid terminal dialog action");
}
inline std::vector<DialogActionDefinition> dialog_actions(DialogKind kind,DialogVariant variant) {
    if(!dialog_variant_allowed(kind,variant))throw std::logic_error("Invalid dialog variant");
    using A=DialogAction;using R=DialogActionRole;using P=DialogPlacement;using C=DialogCompletion;
    std::vector<DialogActionDefinition> actions;
    const bool metadata=kind==DialogKind::ImageMetadata || kind==DialogKind::ProjectMetadata;
    actions.push_back({A::Cancel,metadata?"Cancel###Cancel / close":kind==DialogKind::ImageInformation || kind==DialogKind::ObjectInformation?"Close":kind==DialogKind::ImportFile?"Cancel###Cancel file choice":kind==DialogKind::Language?"Close":"Cancel",R::Cancel,P::Footer,C::DismissOnSuccess});
    if(metadata) {
        actions.push_back({A::Reload,kind==DialogKind::ProjectMetadata?"Reload current project":"Reload current entry",R::Reload,P::Header,C::KeepOpen});
        actions.push_back(variant==DialogVariant::Details?DialogActionDefinition{A::ApplyDetails,"Apply details###Apply description and tags",R::Primary,P::Footer,C::KeepOpen,"Description and tags"}:DialogActionDefinition{A::ApplyMetadata,"Apply metadata",R::Primary,P::Footer,C::KeepOpen,"Metadata"});
    } else switch(kind) {
    case DialogKind::CloseProject:
        actions.push_back({A::KeepRecovery,"Keep recovery and close",R::Secondary,P::Body,C::KeepOpen});
        actions.push_back({A::Discard,"Discard and close",R::Destructive,P::Footer,C::KeepOpen});
        actions.push_back({A::Save,"Save and close",R::Primary,P::Footer,C::KeepOpen});break;
    case DialogKind::ReplaceWorkspace:
        actions.push_back({A::Discard,variant==DialogVariant::Recovery?"Discard current and recover":"Discard current and open",R::Destructive,P::Footer,C::KeepOpen});
        actions.push_back({A::Save,variant==DialogVariant::Recovery?"Save current and recover":"Save current and open",R::Primary,P::Footer,C::KeepOpen});break;
    case DialogKind::ReplaceExport:actions.push_back({A::Confirm,"Replace file",R::Destructive,P::Footer,C::DismissOnSuccess});break;
    case DialogKind::RemoveImage:actions.push_back({A::Confirm,"Remove image",R::Destructive,P::Footer,C::DismissOnSuccess});break;
    case DialogKind::DeleteCheckpoint:actions.push_back({A::Confirm,"Permanently delete checkpoint",R::Destructive,P::Footer,C::DismissOnSuccess});break;
    case DialogKind::SourceVerification:actions.push_back({A::Confirm,"Use reviewed file",R::Primary,P::Footer,C::DismissOnSuccess});break;
    case DialogKind::ImportFile:actions.push_back({A::Confirm,variant==DialogVariant::Output?"Choose new output":"Choose file",R::Primary,P::Footer,C::DismissOnSuccess});break;
    case DialogKind::Calibration:
        actions.push_back({A::Discard,"Clear calibration",R::Destructive,P::Footer,C::DismissOnSuccess});
        actions.push_back({A::Confirm,"Apply calibration",R::Primary,P::Footer,C::DismissOnSuccess});break;
    case DialogKind::ObjectInformation:case DialogKind::ImageInformation:case DialogKind::Language:break;
    default:throw std::logic_error("Unregistered dialog actions");
    }
    validate_dialog_actions(actions);return actions;
}
struct DialogState {
    const bool owner_valid,busy;
    bool submit_enabled=true,dismiss_requested=false,foreground=true;
    DialogVariant variant;
    DialogState(bool owner,bool pending,DialogVariant current=DialogVariant::Default):owner_valid(owner),busy(pending),variant(current){}
};
inline bool dialog_action_enabled(const DialogState& state,const DialogActionDefinition& action) {
    if(state.busy || !state.foreground)return false;
    if(action.role==DialogActionRole::Cancel || action.role==DialogActionRole::Reload)return true;
    return state.owner_valid && state.submit_enabled;
}
}
