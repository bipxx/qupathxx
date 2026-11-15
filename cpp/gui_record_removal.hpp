#pragma once
#include "gui_dialogs.hpp"
#include "qupathxx/core.hpp"
#include <functional>
#include <optional>
namespace qpx::detail {
struct RecordRemoval {
    std::string label;const Document* document=nullptr;std::uint64_t epoch=0;
    std::function<bool()> exists;std::function<void()> remove;
};
class RecordRemovalDialog {
    std::optional<RecordRemoval> removal_;bool requested_=false;std::string error_;
public:
    void request(RecordRemoval removal) {if(removal_)return;removal_=std::move(removal);requested_=true;error_.clear();}
    bool pending() const {return bool(removal_);}
    template<class Lookup,class Remove>
    void request_record(Document& doc,std::uint64_t epoch,const char* family,std::uint64_t id,Lookup lookup,Remove remove) {
        request({std::string(ui::i18n::text(family))+" #"+std::to_string(id),&doc,epoch,
            [&doc,id,lookup] {try {(doc.*lookup)(id);return true;}catch(const std::out_of_range&) {return false;}},
            [&doc,id,remove] {(doc.*remove)(id);}});
    }
    void draw(const Document& active,std::uint64_t epoch,std::string& error) {
        if(!removal_)return;
        if(requested_) {ui::OpenDialog(ui::DialogKind::RemoveKeptResult);requested_=false;}
        const bool valid=removal_->document==&active && removal_->epoch==epoch && removal_->exists();
        ui::DialogState state(valid,false);bool dismissed=false;
        ui::RenderDialog(ui::DialogKind::RemoveKeptResult,state,removal_->label,{&error_},[](ui::DialogControls&) {
            ui::TextWrapped("This cannot be undone. Save project then writes the removal to disk.");
        },[&](ui::DialogAction action) {
            if(action==ui::DialogAction::Confirm) {
                // Revalidate at dispatch, before dereferencing any captured owner.
                if(removal_->document!=&active || removal_->epoch!=epoch || !removal_->exists())return ui::DialogOutcome::Refused;
                try {removal_->remove();}catch(const std::exception& e) {error_=e.what();error=error_;return ui::DialogOutcome::Refused;}
            }
            dismissed=true;return ui::DialogOutcome::Success;
        });
        if(dismissed || !ui::IsDialogOpen(ui::DialogKind::RemoveKeptResult)){removal_.reset();error_.clear();}
    }
};
}
