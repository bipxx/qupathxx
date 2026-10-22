#pragma once
#include "qupathxx/core.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
namespace qpx {
// The caller supplies real widget/startup readiness and injects accepted events
// through ImGui. Physical frames keep rendering while the logical clock waits.
class GuiReplayDeadline {
public:
    using Clock=std::chrono::steady_clock;
    explicit GuiReplayDeadline(double seconds):seconds_(seconds) {
        if(!std::isfinite(seconds) || seconds<=0 || seconds>60) throw std::invalid_argument("GUI replay timeout must be >0 and <=60 seconds");
    }
    void reset() {began_.reset();}
    void check(Clock::time_point now,const std::string& description) {
        if(!began_) began_=now;
        if(std::chrono::duration<double>(now-*began_).count()>=seconds_) throw std::runtime_error("GUI replay timed out waiting for "+description);
    }
private:
    double seconds_;
    std::optional<Clock::time_point> began_;
};
class GuiReplay {
public:
    GuiReplay(std::vector<GuiInput> events,double timeout):events_(std::move(events)),deadline_(timeout) {
        for(const auto& event:events_) {
            if(event.frame<0) throw std::invalid_argument("negative replay frame");
            if(event.kind==GuiInputKind::ResizeWindow && (!std::isfinite(event.x) || !std::isfinite(event.y) ||
               event.x<640 || event.y<480 || event.x>8192 || event.y>8192 || std::floor(event.x)!=event.x || std::floor(event.y)!=event.y))
                throw std::invalid_argument("resize replay requires integer window dimensions between 640x480 and 8192x8192");
            if(event.wait_for==GuiInputWait::WindowReady && (event.kind==GuiInputKind::ResizeWindow || event.kind==GuiInputKind::MinimizeWindow || event.kind==GuiInputKind::RestoreWindow))
                throw std::invalid_argument("window controls cannot wait for window readiness");
            if(event.wait_for!=GuiInputWait::Immediate && event.wait_for!=GuiInputWait::TargetReady && event.wait_for!=GuiInputWait::StartupFinished && event.wait_for!=GuiInputWait::TissueReady && event.wait_for!=GuiInputWait::TissueContoursReady && event.wait_for!=GuiInputWait::TissueContoursPublished && event.wait_for!=GuiInputWait::TissueArchived && event.wait_for!=GuiInputWait::TissueVerified && event.wait_for!=GuiInputWait::CellsReady && event.wait_for!=GuiInputWait::CellsPublished && event.wait_for!=GuiInputWait::CellsArchived && event.wait_for!=GuiInputWait::CellsVerified && event.wait_for!=GuiInputWait::CellTableSaved && event.wait_for!=GuiInputWait::FileFingerprintReady && event.wait_for!=GuiInputWait::MeasurementTableSaved && event.wait_for!=GuiInputWait::StainsEstimated && event.wait_for!=GuiInputWait::StainsApplied && event.wait_for!=GuiInputWait::BatchFinished && event.wait_for!=GuiInputWait::WindowReady)
                throw std::invalid_argument("invalid GUI replay readiness condition");
            if(event.wait_for==GuiInputWait::TargetReady && (event.kind!=GuiInputKind::LeftDown || event.target<GuiInputTarget::DisplayReset || event.target>GuiInputTarget::CancelDiscardCheckpoint))
                throw std::invalid_argument("target readiness requires LeftDown on a named button target");
        }
        std::stable_sort(events_.begin(),events_.end(),[](const auto& a,const auto& b){return a.frame<b.frame;});
        bool minimized=false;
        for(const auto& event:events_) {
            if(event.kind==GuiInputKind::MinimizeWindow) minimized=true;
            else if(event.kind==GuiInputKind::RestoreWindow || event.kind==GuiInputKind::CloseWindow) minimized=false;
            else if(minimized && event.kind!=GuiInputKind::ResizeWindow)
                throw std::invalid_argument("restore the window before replaying UI input");
        }
    }
    template<class Ready,class Emit> void dispatch(Ready&& ready,Emit&& emit,GuiReplayDeadline::Clock::time_point now) {
        blocked_=false;
        while(cursor_<events_.size() && static_cast<std::uint64_t>(events_[cursor_].frame)<=frame_) {
            const auto& event=events_[cursor_];
            if(event.wait_for!=GuiInputWait::Immediate && !ready(event)) {
                deadline_.check(now,"event at logical frame "+std::to_string(event.frame));blocked_=true;return;
            }
            deadline_.reset();emit(event);++cursor_;
        }
    }
    void finish_frame(bool advance=true) {
        if(blocked_) {if(waits_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("replay wait counter exhausted");++waits_;}
        else if(advance) {if(frame_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("replay clock exhausted");++frame_;}
    }
    std::uint64_t frame() const {return frame_;}
    std::uint64_t waits() const {return waits_;}
    bool complete() const {return cursor_==events_.size();}
private:
    std::vector<GuiInput> events_;
    GuiReplayDeadline deadline_;
    std::size_t cursor_=0;
    std::uint64_t frame_=0,waits_=0;
    bool blocked_=false;
};
}
