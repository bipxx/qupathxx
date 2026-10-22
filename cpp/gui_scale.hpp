#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace qpx {
// GLFW content scale is DPI relative to the platform baseline; framebuffer
// density converts window coordinates to pixels (notably on macOS Retina).
// Keeping these separate avoids scaling the logical UI twice on Retina.
struct GuiScale {
    float monitor=1,framebuffer=1,logical=1;
    static void validate(double multiplier) {
        if(!std::isfinite(multiplier) || multiplier<.5 || multiplier>3)
            throw std::invalid_argument("UI scale multiplier must be between 0.5 and 3");
    }
    static GuiScale from(float monitor,float framebuffer,double multiplier) {
        validate(multiplier);
        auto bounded=[](float value) {return std::isfinite(value) && value>0?std::clamp(value,.5f,4.f):1.f;};
        monitor=bounded(monitor);framebuffer=bounded(framebuffer);
        return {monitor,framebuffer,std::clamp(float(monitor/framebuffer*multiplier),.5f,6.f)};
    }
    bool same_fonts(const GuiScale& other) const {
        return std::abs(logical-other.logical)<.001f && std::abs(framebuffer-other.framebuffer)<.001f;
    }
};
}
