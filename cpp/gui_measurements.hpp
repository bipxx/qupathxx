#pragma once
#include "qupathxx/core.hpp"
#include "gui_layout.hpp"
namespace qpx::detail {
// Native values remain the authority. The calibrated and pixel values share a
// wrapped row rather than repeating the quantity in separate sections.
inline void shape_measurement_row(const char* quantity,double pixels,
    const std::optional<double>& physical,const char* pixel_unit,const char* physical_unit) {
    const auto* label=ui::i18n::text(quantity);
    if(physical)ui::TextWrapped("%s: %.6g %s (%.6g %s)",label,*physical,physical_unit,pixels,pixel_unit);
    else ui::TextWrapped("%s: %.6g %s",label,pixels,pixel_unit);
}
inline void draw_shape_measurements(const Document& document,std::uint64_t primary,std::size_t count) {
    if(count>1)ui::TextDisabled("Primary object measurements");
    try {
        const auto features=document.shape_measurements(primary);
        shape_measurement_row("Area",features.roi.area_px2,features.roi.area_um2,"px²","µm²");
        shape_measurement_row("Length",features.roi.length_px,features.roi.length_um,"px","µm");
        if(!features.roi.area_um2)ui::TextDisabled("Set image calibration for physical units.");
        if(features.nucleus) {
            shape_measurement_row("Nucleus area",features.nucleus->area_px2,features.nucleus->area_um2,"px²","µm²");
            shape_measurement_row("Nucleus length",features.nucleus->length_px,features.nucleus->length_um,"px","µm");
        }
    } catch(const std::exception& e) {ui::TextWrapped("Measurements: %s",e.what());}
}
inline void retained_result_hint(bool project) {
    ui::TextWrapped(project?"Keep adds this result to the project. Save project writes it to disk.":"No project is open. Kept results are held in memory.");
}
}
