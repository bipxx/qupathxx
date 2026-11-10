#pragma once
#include "qupathxx/core.hpp"
#include "gui_dialogs.hpp"

namespace qpx::detail {
class CalibrationEditor {
    Document* owner_=nullptr;
    std::uint64_t image_=0,epoch_=0,revision_=0;
    double width_=0,height_=0;
    bool requested_=false,active_=false;
    std::string error_;
public:
    bool active() const {return active_;}
    void open(Document& document,std::uint64_t image,std::uint64_t epoch) {
        owner_=&document;image_=image;epoch_=epoch;revision_=document.calibration_revision();
        width_=document.pixel_width_um().value_or(0);height_=document.pixel_height_um().value_or(0);
        error_.clear();requested_=active_=true;
    }
    void draw(Document* document,std::uint64_t image,std::uint64_t epoch) {
        if(requested_) {ui::OpenDialog(ui::DialogKind::Calibration);requested_=false;}
        const bool valid=document && document==owner_ && image==image_ && epoch==epoch_ && document->calibration_revision()==revision_;
        ui::DialogState state(valid,false);state.dismiss_requested=!active_;
        ui::RenderDialog(ui::DialogKind::Calibration,state,{}, {&error_},[&](ui::DialogControls&) {
            ui::TextWrapped("Physical pixel size applies to measurements. Image pixels remain unchanged.");
            ui::SetNextItemWidth(-1);ui::InputDouble("Pixel width (um)",&width_,0,0,"%.17g");
            ui::SetNextItemWidth(-1);ui::InputDouble("Pixel height (um)",&height_,0,0,"%.17g");
            ui::TextWrapped("Clear removes physical calibration. This edit can be undone.");
        },[&](ui::DialogAction action) {
            if(action==ui::DialogAction::Cancel) {active_=false;return ui::DialogOutcome::Success;}
            if(!valid)return ui::DialogOutcome::Refused;
            try {
                if(action==ui::DialogAction::Discard)document->clear_calibration();
                else document->set_calibration(width_,height_);
                active_=false;error_.clear();return ui::DialogOutcome::Success;
            } catch(const std::exception& e) {error_=e.what();return ui::DialogOutcome::Refused;}
        });
    }
};
}
