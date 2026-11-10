#pragma once
#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/core.hpp"
namespace qpx::detail {
class GuiDarkFlat {
public:
    enum class Action {Load,Preview,Cancel,Keep,LoadSaved,Remove,Use,Fields,Image};
    GuiDarkFlat();~GuiDarkFlat();
    void open();void close();
    void poll(Document&,bool blocked=false);
    void draw(Document&,bool blocked=false);
    void set_fields(std::string dark,std::string flat,DarkFlatOptions,
        std::uint32_t dark_series=0,std::uint32_t flat_series=0);
    void set_preview(RawRegionRequest);
    std::optional<DarkFlatRequest> take_selection();
    std::shared_ptr<const DarkFlatBlock> result() const;
    bool busy() const;
    Rect bounds(Action) const;
    const std::string& kept_record() const;
    const std::string& error() const;
    const std::string& state() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
