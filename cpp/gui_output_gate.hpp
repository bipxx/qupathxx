#pragma once
#include "gui_export.hpp"
#include "gui_dialogs.hpp"
#include "gui_workflow_ui.hpp"
#include "gui_shell_open.hpp"
#include "gui_notifications.hpp"
#include <optional>
namespace qpx::detail {
struct PendingGuiExport {
    GuiExportOperation operation=GuiExportOperation::GeoJson;std::string path,scope;
    std::vector<std::uint64_t> ids;std::vector<std::string> protected_paths;
    CellTableOptions cell_options;MeasurementTableOptions measurement_options;
    MeasurementTableKind measurement_kind=MeasurementTableKind::Roi;
    bool selected_only=false,allow_losses=false,nested=false,intensity=false;
    ExportOwnerStamp owner;std::shared_ptr<const FileVersion> destination;
    const char* source="Tables";
    bool project_scope() const {return operation==GuiExportOperation::ProjectCells || operation==GuiExportOperation::ProjectMeasurements;}
};
struct OutputIntent {
    std::string path,scope;
    std::vector<std::string> protected_paths;
    ExportOwnerStamp owner;
    bool preauthorized=false;
    std::function<void(ExportWritePolicy)> write;
    std::function<void(const std::string&)> fail;
    // The frozen native authority may expose only create-only publication.
    std::string existing_refusal;
    std::string kind;
    const char* source="Tables";
};
inline void guard_output_intent(const std::string& path,const std::vector<std::string>& paths) {
    // The native file guard checks aliases and protected directory contents.
    // Check lexical names too, including not-yet-created recovery sidecars.
    for(const auto& source:paths)if(!source.empty()) {
        const auto input=export_path(source);
        bool same=std::filesystem::u8path(input)==std::filesystem::u8path(path);
#ifdef _WIN32
        const auto a=std::filesystem::u8path(input).native(),b=std::filesystem::u8path(path).native();
        same=CompareStringOrdinal(a.c_str(),int(a.size()),b.c_str(),int(b.size()),TRUE)==CSTR_EQUAL;
#endif
        if(same)throw std::invalid_argument("Export destination is protected: "+source);
        guard_file_output(input,"",path);
    }
}
struct OutputScope {
    std::string text;const char* source="Tables";
    OutputScope(std::string value,const char* key="Tables"):text(std::move(value)),source(key){}
    OutputScope(const char* value):text(value){}
};
inline OutputScope output_scope(const char* kind,bool project_scope=false,const char* source="Tables") {
    return {std::string(ui::i18n::text(project_scope?"Project":"Current image"))+" / "+ui::i18n::text(kind),source};
}
class OutputGate {
    friend struct OutputGateTestAccess;
    std::optional<OutputIntent> pending_;
    std::shared_ptr<const FileVersion> destination_;
    bool requested_=false;
    ui::Notifications* notifications_=nullptr;
    const char* active_source_="Tables";
    std::string error_;
    std::function<ExportOwnerStamp(bool,bool)> capture_;
    std::function<std::vector<std::string>()> protection_;
    std::function<void(const std::string&)> active_fail_;
    struct Watch {std::string path;std::function<std::optional<bool>()> complete;std::function<std::string()> error;std::function<void(const std::string&)> fail;const char* source;};
    std::vector<Watch> watches_;
    void failed(const OutputIntent& intent,const std::exception& exception) {
        if(intent.fail)intent.fail(exception.what());
        if(notifications_)notifications_->post(ui::exception_notice(intent.source,exception,intent.path));
    }
    void write(OutputIntent& intent,ExportWritePolicy policy) {
        guard_output_intent(intent.path,intent.protected_paths);
        if(protection_)guard_output_intent(intent.path,protection_());
        auto previous=std::move(active_fail_);active_fail_=intent.fail;const auto source=active_source_;active_source_=intent.source;
        try {intent.write(policy);}catch(...) {active_fail_=std::move(previous);active_source_=source;throw;}
        active_fail_=std::move(previous);active_source_=source;
    }
    bool confirm(const std::function<bool(const ExportOwnerStamp&)>& owner_valid) {
        if(!pending_)return false;
        try {
            if(!owner_valid(pending_->owner))throw ui::NoticeException<std::runtime_error>("Export context changed. Cancel and export again.");
            if(capture_ && !(capture_(pending_->owner.selected_only,pending_->owner.project_scope)==pending_->owner))
                throw ui::NoticeException<std::runtime_error>("Export context changed. Cancel and export again.");
            if(!destination_ || !same_export_destination(*destination_,*workflow_file_version(pending_->path)))
                throw ui::NoticeException<std::runtime_error>("The destination changed. Cancel and export again to review it.");
            write(*pending_,ExportWritePolicy::ReplaceExisting);return true;
        }catch(const std::exception& e) {error_=e.what();failed(*pending_,e);return false;}
    }
public:
    void notifications(ui::Notifications* value){notifications_=value;}
    ui::Notifications* notifications() const{return notifications_;}
    // Full capture is used at submit and once at Confirm, never while drawing.
    void bind(std::function<ExportOwnerStamp(bool,bool)> capture,std::function<std::vector<std::string>()> protection) {capture_=std::move(capture);protection_=std::move(protection);}
    ExportOwnerStamp owner(bool selected=true,bool project_scope=false) const {return capture_?capture_(selected,project_scope):ExportOwnerStamp{};}
    std::vector<std::string> protected_paths() const {return protection_?protection_():std::vector<std::string>{};}
    void submit(OutputIntent intent) {
        try {
            if(pending_)throw ui::NoticeException<std::runtime_error>("Finish or cancel the current export first.");
            intent.path=export_path(intent.path);guard_output_intent(intent.path,intent.protected_paths);
            if(protection_)guard_output_intent(intent.path,protection_());
            if(std::filesystem::exists(std::filesystem::u8path(intent.path))) {
                if(!intent.existing_refusal.empty())throw std::runtime_error(intent.existing_refusal);
                if(intent.preauthorized) {write(intent,ExportWritePolicy::ReplaceExisting);return;}
                destination_=workflow_file_version(intent.path);pending_=std::move(intent);requested_=true;error_.clear();
            }else write(intent,ExportWritePolicy::CreateNew);
        }catch(const std::exception& e) {failed(intent,e);}
    }
    // Callbacks own their options. The resolved path and protected set are
    // delivered by value; no frame-local lambda/reference survives a dialog.
    void submit(std::string path,OutputScope scope,
        std::function<void(const std::string&,ExportWritePolicy,const std::vector<std::string>&)> write,
        std::function<void(const std::string&)> fail,bool preauthorized=false,std::string existing_refusal={},
        std::vector<std::string> sources={},bool project_scope=false) {
        try {
            path=export_path(path);auto paths=protected_paths();paths.insert(paths.end(),sources.begin(),sources.end());
            OutputIntent intent{path,std::move(scope.text),paths,owner(true,project_scope),preauthorized,
                [path,paths,write=std::move(write)](ExportWritePolicy policy){write(path,policy,paths);},std::move(fail),std::move(existing_refusal)};
            intent.source=scope.source;submit(std::move(intent));
        }catch(const std::exception& e) {if(fail)fail(e.what());if(notifications_)notifications_->post(ui::exception_notice(scope.source,e,path));}
    }
    bool pending() const {return bool(pending_);}
    void succeeded(const std::string& path) {if(notifications_)notifications_->post({ui::Severity::Success,active_source_,"Exported.",ui::notice_path(path),true});}
    void watch(std::string path,std::function<std::optional<bool>()> complete,std::function<std::string()> error={},const char* source=nullptr) {watches_.push_back({std::move(path),std::move(complete),std::move(error),active_fail_,source?source:active_source_});}
    bool watching(const char* source) const {return std::any_of(watches_.begin(),watches_.end(),[&](const auto& watch){return std::string_view(watch.source)==source;});}
    void draw(const std::function<bool(const ExportOwnerStamp&)>& owner_valid,
        const std::function<void(ui::DialogAction)>& mark={}) {
        for(auto i=watches_.begin();i!=watches_.end();) {
            const auto done=i->complete();if(!done){++i;continue;}
            if(*done){const auto source=active_source_;active_source_=i->source;succeeded(i->path);active_source_=source;}
            else if(i->error) {const auto message=i->error();if(!message.empty()){if(i->fail)i->fail(message);if(notifications_)notifications_->post({ui::Severity::Error,i->source,message,i->path});}}
            i=watches_.erase(i);
        }
        if(requested_) {ui::OpenDialog(ui::DialogKind::ReplaceExport);requested_=false;}
        bool dismissed=false;ui::DialogState state(pending_ && owner_valid(pending_->owner),false);state.dismiss_requested=!pending_;
        ui::RenderDialog(ui::DialogKind::ReplaceExport,state,{}, {&error_},[&](ui::DialogControls&) {
            if(!pending_)return;
            if(!pending_->kind.empty())ui::TextUnformatted(pending_->kind.c_str());
            ui::TextUnformatted("Export destination");ui::DataTextWrapped("%s",pending_->path.c_str());
            ui::DataTextWrapped("%s",pending_->scope.c_str());
            ui::TextWrapped("The existing file will be replaced and cannot be restored.");
        },[&](ui::DialogAction action) {
            if(action==ui::DialogAction::Confirm && !confirm(owner_valid))return ui::DialogOutcome::Refused;
            dismissed=true;return ui::DialogOutcome::Success;
        },[&](ui::DialogAction action){if(mark)mark(action);});
        if(dismissed || (pending_ && !requested_ && !ui::IsDialogOpen(ui::DialogKind::ReplaceExport))) {pending_.reset();destination_.reset();error_.clear();}

    }
};
inline bool output_gate_available(OutputGate* gate,std::string& error) {
    if(gate)return true;error="Output gate is unavailable";return false;
}
}
