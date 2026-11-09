#pragma once
#include "qupathxx/linear_unmixing_workflow.hpp"
#include "qupathxx/core.hpp"
namespace qpx::detail {
struct GuiUnmixPreview {
    RawPixelBuffer samples;RawRegionRequest region;
    std::vector<std::uint64_t> finite,nonfinite;std::uint64_t valid=0;
    GuiUnmixPreview(RawPixelBuffer,RawRegionRequest,const std::atomic<bool>*);
};
class GuiLinearUnmixing {
public:
    enum class Action {LoadFile,Apply,Preview,Cancel,Keep,LoadSaved,Remove,Use,Editor,Image,Coefficient,Parent,NewDraft};
    GuiLinearUnmixing();~GuiLinearUnmixing();
    void open();void close();void cancel();
    void restore(Document&,std::string record);
    std::optional<LinearUnmixRequest> request() const;
    void poll(Document&,bool blocked=false);
    void draw(Document&,bool blocked=false);
    void set_matrix_file(std::string,std::string correction_record={});
    void set_matrix(LinearMixingMatrix,std::string correction_record={});
    void set_saved_record(std::string);
    void set_preview(RawRegionRequest);
    void set_display_range(double lower,double upper);
    std::optional<LinearUnmixRequest> take_selection();
    std::shared_ptr<const GuiUnmixPreview> result() const;
    bool busy() const;Rect bounds(Action) const;
    const std::string& kept_record() const;
    const std::string& error() const;
    const std::string& state() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
