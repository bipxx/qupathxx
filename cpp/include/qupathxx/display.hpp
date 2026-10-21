#pragma once
#include "qupathxx/image_server.hpp"
#include <array>

namespace qpx {
enum class DisplayMode { RGB, Gray, Red, Green, Blue };
struct DisplaySettings {
    DisplayMode mode = DisplayMode::RGB;
    double black = 0, white = 255, gamma = 1;
    void validate() const;
    bool operator==(const DisplaySettings& other) const;
};
// Presentation-only transform. The source and its measurements remain unchanged.
std::array<std::uint8_t,256> display_lut(const DisplaySettings&);
std::vector<std::uint8_t> display_pixels(const PixelBuffer&, const DisplaySettings&);
}
