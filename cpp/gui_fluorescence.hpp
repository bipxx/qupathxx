#pragma once
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/core.hpp"
#include <imgui.h>
namespace qpx {class DarkFlatRequest;class LinearUnmixRequest;}
namespace qpx::detail {
class GuiDarkFlat;class GuiLinearUnmixing;
class GuiFluorescence {
public:
    enum class Action {Run,Cancel,KeepCells,KeepRecord,Replay,Remove,Correction,Domain,Unmixing,UnmixDomain};
    GuiFluorescence();~GuiFluorescence();
    void open();
    void poll(Document&,bool blocked=false);
    void draw(Document&,std::uint64_t selected,bool blocked=false);
    void draw_preview(ImDrawList*,const Viewport&,ImVec2 offset,ImVec2 size);
    void set_request(RawRegionRequest,FluorescenceCellOptions={},std::array<std::uint32_t,2> context={32,32},bool use_selected=true);
    void set_correction(const DarkFlatRequest&);
    void set_unmixing(const LinearUnmixRequest&);
    bool uses_unmixing() const;
    GuiLinearUnmixing& unmixing_tool();
    void use_original();
    bool uses_correction() const;
    GuiDarkFlat& correction_tool();
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
