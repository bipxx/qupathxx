#pragma once
#include "qupathxx/landmark_registration.hpp"
#include "qupathxx/display.hpp"
namespace qpx::detail {
class GuiLandmarkRegistration {
public:
    enum class Action { Preview, Accept, Load };
    GuiLandmarkRegistration();
    ~GuiLandmarkRegistration();
    void open();
    void draw(Document&,const std::string& image_id,const DisplaySettings&,bool blocked=false);
    void set_landmarks(std::vector<LandmarkPair>,LandmarkFitOptions={});
    const std::vector<LandmarkPair>& landmarks() const;
    std::shared_ptr<const LandmarkFit> fit() const;
    const std::string& error() const;
    Rect bounds(Action) const;
    Point handle_position(std::size_t,bool target) const;
    bool settled() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
