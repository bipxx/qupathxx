#pragma once
#include "../gui_i18n.hpp"
#if __has_include(<imgui.h>)
#include "gui_layout_recorder.hpp"
#endif
#include <fstream>
#include <iomanip>

namespace translation_checks {
// Each test process appends observations; the runner owns paths and ratcheting.
struct Recorder {
    qpx::ui::i18n::SurfaceTraces surfaces;
    Recorder(){if(std::getenv("QUPATHXX_I18N_TRACE"))qpx::ui::i18n::surface_traces=&surfaces;}
    ~Recorder(){
        qpx::ui::i18n::surface_traces=nullptr;
        if(const char* path=std::getenv("QUPATHXX_I18N_TRACE")) {
            std::ofstream file(path,std::ios::app);
            for(const auto& [name,trace]:surfaces) {
                file<<std::quoted(name)<<' '<<std::quoted("")<<'\n';
                for(const auto& key:trace.keys)file<<std::quoted(name)<<' '<<std::quoted(key)<<'\n';
            }
        }
    }
};
inline Recorder recorder;
}
