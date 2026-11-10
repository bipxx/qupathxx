#pragma once
#include "qupathxx/core.hpp"
#include "gui_layout.hpp"
namespace qpx::detail {
inline void draw_shape_measurements(const Document& document,std::uint64_t primary,std::size_t count) {
    if(count>1)ui::TextDisabled("Primary object measurements");
            try {
                const auto features=document.shape_measurements(primary);
                ui::Text("Area: %.6g px^2",features.roi.area_px2);
                ui::Text("Length: %.6g px",features.roi.length_px);
                if(features.roi.area_um2) {
                    ui::Text("Area: %.6g um^2",*features.roi.area_um2);
                    ui::Text("Length: %.6g um",*features.roi.length_um);
                } else ui::TextDisabled("Physical measurements: set calibration first");
                if(features.nucleus) {
                    ui::Text("Nucleus area: %.6g px^2",features.nucleus->area_px2);
                    ui::Text("Nucleus length: %.6g px",features.nucleus->length_px);
                    if(features.nucleus->area_um2) {
                        ui::Text("Nucleus area: %.6g um^2",*features.nucleus->area_um2);
                        ui::Text("Nucleus length: %.6g um",*features.nucleus->length_um);
                    }
                }
            } catch(const std::exception& e) {ui::TextWrapped("Measurements: %s",e.what());}
}
inline void retained_result_hint(bool project) {
    ui::TextWrapped(project?"Keep retains a result for tables. Save project persists it.":"Kept results remain in current-image memory; no project is open.");
}
}
