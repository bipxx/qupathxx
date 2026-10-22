#pragma once
#include "atomic_export.hpp"
#include "qupathxx/provenance.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>
#include <vector>
namespace qpx::detail {
struct GuiFrameSample {
    int frame=0;bool startup=false,cache_hit=false;std::size_t visible=0;
    double model_ms=0,widgets_ms=0,tiles_ms=0,cache_ms=0,query_ms=0,geometry_ms=0,other_ms=0,render_ms=0,total_ms=0;
};
// Developer-only bounded profiling. It does no file I/O inside the frame loop.
class GuiProfiler {
    std::string path_;std::vector<GuiFrameSample> frames_;
public:
    GuiProfiler() {
#ifdef _WIN32
        char* value=nullptr;std::size_t size=0;if(_dupenv_s(&value,&size,"QUPATHXX_GUI_PROFILE")==0 && value) {path_=value;std::free(value);}
#else
        if(const auto* value=std::getenv("QUPATHXX_GUI_PROFILE")) path_=value;
#endif
        if(!path_.empty()) {path_=export_path(path_);frames_.reserve(4096);}
    }
    bool enabled() const {return !path_.empty();}
    void add(const GuiFrameSample& sample) {if(enabled() && frames_.size()<4096) frames_.push_back(sample);}
    void save() const {
        if(!enabled()) return;std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
        out<<"{\"schema\":\"qpx.gui-profile/1\",\"source_tree_sha256\":\""<<build_environment()->source_tree_sha256<<"\",\"frames\":[";
        bool first=true;for(const auto& f:frames_) {if(!first) out<<',';first=false;
            out<<"{\"frame\":"<<f.frame<<",\"startup\":"<<(f.startup?"true":"false")<<",\"cache_hit\":"<<(f.cache_hit?"true":"false")<<",\"visible\":"<<f.visible;
            out<<",\"model_ms\":"<<f.model_ms<<",\"widgets_ms\":"<<f.widgets_ms<<",\"tiles_ms\":"<<f.tiles_ms<<",\"cache_ms\":"<<f.cache_ms<<",\"query_ms\":"<<f.query_ms<<",\"geometry_ms\":"<<f.geometry_ms<<",\"other_ms\":"<<f.other_ms<<",\"render_ms\":"<<f.render_ms<<",\"total_ms\":"<<f.total_ms<<'}';
        }out<<"]}\n";const auto text=out.str();AtomicExportFile file(path_,8*1024*1024);file.write(text.data(),text.size());file.commit();
    }
};
}
