#pragma once
#include "../gui_layout.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace layout_checks {
inline qpx::ui::IconVocabulary vocabulary() {
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/gui-icon-vocabulary.tsv");
    qpx::ui::IconVocabulary values;std::string line;
    while(std::getline(file,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);int icon;std::string command;
        if(row>>icon>>std::quoted(command))values.emplace(icon,command);
    }
    return values;
}
struct Recorder {
    qpx::ui::LayoutTrace trace;
    qpx::ui::IconVocabulary allowed;
    std::set<std::string> observations;
    Recorder() {
        if(!std::getenv("QUPATHXX_LAYOUT_TRACE"))return;
        qpx::ui::layout_trace=&trace;
        trace.rendered_only=true;
        allowed=vocabulary();
        qpx::ui::i18n::observe_surface_names=true;
        if(const char* language=std::getenv("QUPATHXX_LAYOUT_LANGUAGE")) {
            const int chosen=std::string(language)=="en"?0:2;
            trace.before_frame=[chosen]{qpx::ui::i18n::choose(chosen);};
        }
        trace.frame=[&](const auto& items) {
            for(const auto& item:items) {
                std::ostringstream row;
                row<<"item "<<std::quoted(item.surface)<<' '<<std::quoted(item.window)<<' '<<int(item.kind)<<' '<<int(item.icon)<<' '<<std::quoted(item.command)<<' '<<std::quoted(item.caption)<<' '<<item.region<<' '<<item.rect.Min.x<<' '<<item.rect.Min.y<<' '<<item.rect.Max.x<<' '<<item.rect.Max.y<<' '<<item.language<<' '<<item.scale;
                observations.insert(row.str());
            }
            for(const auto& error:qpx::ui::lint_layout(items,allowed)) {
                std::ostringstream row;row<<"error "<<error.principle<<' '<<error.message<<' '<<std::quoted(error.item.surface)<<' '<<std::quoted(error.item.window)<<' '<<std::quoted(error.item.command)<<' '<<std::quoted(error.item.caption);
                observations.insert(row.str());
            }
        };
    }
    ~Recorder() {
        if(qpx::ui::layout_trace!=&trace)return;
        qpx::ui::finish_layout_frame();qpx::ui::layout_trace=nullptr;qpx::ui::i18n::observe_surface_names=false;
        std::ofstream file(std::getenv("QUPATHXX_LAYOUT_TRACE"),std::ios::app);
        for(const auto& row:observations)file<<row<<'\n';
    }
};
inline Recorder layout_recorder;
}
