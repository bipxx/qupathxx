#pragma once
#include "qupathxx/raw_analysis.hpp"
#include "gui_layout.hpp"
#include <algorithm>
namespace qpx::detail {
inline void raw_grid_controls(RawGridSampling& sampling,std::size_t& work) {
    auto count=[](const char* label,std::uint32_t& value,int minimum,int maximum) {
        int n=int(value);if(ui::InputInt(label,&n)) value=std::uint32_t(std::clamp(n,minimum,maximum));
    };
    count("Core samples X",sampling.samples_x,1,1000000);count("Core samples Y",sampling.samples_y,1,1000000);
    count("Context samples X",sampling.context_x,0,4096);count("Context samples Y",sampling.context_y,0,4096);
    int mib=int(work/(1024*1024));
    if(ui::InputInt("Sampling memory (MiB)",&mib)) work=std::size_t(std::clamp(mib,1,1024))*1024*1024;
    ImGui::TextWrapped("Sample counts span each ROI's level-zero bounds. Context supplies input support; only eligible core centers enter the measurement.");
}
}
