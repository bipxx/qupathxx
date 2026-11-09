#pragma once
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/core.hpp"
#include <imgui.h>
namespace qpx::detail {
class GuiFluorescence {
public:
    enum class Action {Run,Cancel,KeepCells,KeepRecord,Replay,Remove};
    GuiFluorescence();~GuiFluorescence();
    void open();
    void poll(Document&,bool blocked=false);
    void draw(Document&,std::uint64_t selected,bool blocked=false);
    void draw_preview(ImDrawList*,const Viewport&,ImVec2 offset,ImVec2 size);
    void set_request(RawRegionRequest,FluorescenceCellOptions={},std::array<std::uint32_t,2> context={32,32},bool use_selected=true);
    Rect bounds(Action) const;
    std::shared_ptr<const FluorescenceCellRun> result() const;
    const std::string& error() const;
    const std::string& state() const;
    const std::string& kept_record() const;
    std::size_t published_cells() const;
    bool busy() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
