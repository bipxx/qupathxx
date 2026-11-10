#pragma once
#include "sampling_summary.hpp"
#include "gui_layout.hpp"
namespace qpx::detail {
inline void draw_sampling_summary(const SamplingSummaryCache& cache) {
    if(!cache.summary) {ui::TextWrapped("%s: %s",ui::i18n::text("Sampling unavailable"),cache.error.c_str());return;}
    const auto& s=*cache.summary;
    if(s.mode==SamplingSummary::Mode::Level)ui::TextWrapped("%s %d / %.5g x %.5g %s",ui::i18n::text("Level"),s.level,s.step_x_px,s.step_y_px,ui::i18n::text("px per sample"));
    else ui::TextWrapped("%s %lld x %lld / %.5g x %.5g %s",ui::i18n::text(s.mode==SamplingSummary::Mode::NominalGrid?"Nominal grid":"Grid"),static_cast<long long>(s.samples_x),static_cast<long long>(s.samples_y),s.step_x_px,s.step_y_px,ui::i18n::text("px per sample"));
    if(s.step_x_um && s.step_y_um)ui::TextWrapped("%.5g x %.5g %s",*s.step_x_um,*s.step_y_um,ui::i18n::text("um per sample"));
    else ui::TextDisabled("Calibration unknown");
}
}
